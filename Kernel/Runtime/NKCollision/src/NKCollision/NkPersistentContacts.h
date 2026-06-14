#pragma once
// persistent_contacts.h
// Contact cache with frame-to-frame persistence.
// Allows warm-starting (reusing last frame's impulses) → faster convergence,
// reduced jitter, stable stacks.

#include "narrowphase.h"
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  PersistedContact — one contact point tracked over multiple frames
// ─────────────────────────────────────────────────────────────────────────────
struct PersistedContact {
    Vec3  localA;          // contact point in body A local space
    Vec3  localB;          // contact point in body B local space
    Vec3  normal;          // contact normal (world space, A→B)
    float depth     = 0.f;
    float normalImpulse  = 0.f;  // accumulated normal impulse (warm-start)
    float tangentImpulse[2] = {0.f, 0.f};  // friction impulses
    uint32_t frameStamp = 0;     // last frame this contact was active
    bool  fresh     = true;      // newly created this frame
};

// ─────────────────────────────────────────────────────────────────────────────
//  PairKey — compact 64-bit key for (bodyA, bodyB) pair
// ─────────────────────────────────────────────────────────────────────────────
inline uint64_t makePairKey(uint64_t a, uint64_t b) {
    if(a > b) std::swap(a, b);
    return (a << 32) | (b & 0xFFFFFFFF);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PersistentManifold — up to 4 contacts between one pair, tracked over time
// ─────────────────────────────────────────────────────────────────────────────
struct PersistentManifold {
    static constexpr int kMaxContacts = 4;
    static constexpr float kBreakThresholdNormal  = 0.02f;  // 2cm
    static constexpr float kBreakThresholdTangent = 0.04f;  // 4cm

    uint64_t          bodyA = 0, bodyB = 0;
    PersistedContact  contacts[kMaxContacts];
    int               count      = 0;
    uint32_t          lastActive = 0;

    // ── Update with new narrowphase result ───────────────────────────────────
    void update(const ContactManifold& fresh,
                const Transform& tA, const Transform& tB,
                uint32_t frameStamp)
    {
        lastActive = frameStamp;

        for(int i = 0; i < fresh.count; i++) {
            const ContactPoint& cp = fresh.points[i];
            addOrMerge(cp, tA, tB, frameStamp);
        }
        // Remove stale contacts
        removeBreaking(tA, tB);
    }

    // ── Remove contacts that have drifted apart ───────────────────────────────
    void removeBreaking(const Transform& tA, const Transform& tB) {
        for(int i = count - 1; i >= 0; i--) {
            PersistedContact& pc = contacts[i];
            // Re-project local points to world space
            Vec3 wA = tA.transformPoint(pc.localA);
            Vec3 wB = tB.transformPoint(pc.localB);
            Vec3 diff = wB - wA;

            // Normal separation (positive = pulling apart)
            float normalSep = diff.dot(pc.normal);
            if(normalSep > kBreakThresholdNormal) {
                removeAt(i); continue;
            }
            // Tangential drift
            Vec3 tangentialDrift = diff - pc.normal * normalSep;
            if(tangentialDrift.lengthSq() > kBreakThresholdTangent * kBreakThresholdTangent) {
                removeAt(i); continue;
            }
            // Update depth
            pc.depth = -normalSep;
        }
    }

    bool isActive(uint32_t currentFrame, uint32_t maxAge = 2) const {
        return (currentFrame - lastActive) <= maxAge;
    }

    void clear() { count = 0; }

private:
    void addOrMerge(const ContactPoint& cp,
                    const Transform& tA, const Transform& tB,
                    uint32_t frameStamp)
    {
        // Convert world contact to local space for persistence
        Vec3 localA = tA.inverseTransformPoint(cp.positionA);
        Vec3 localB = tB.inverseTransformPoint(cp.positionB);

        // Find closest existing contact to merge with
        int closest = -1;
        float bestDist = 0.02f * 0.02f;  // 2cm merge threshold
        for(int i = 0; i < count; i++) {
            float d = (contacts[i].localA - localA).lengthSq();
            if(d < bestDist) { bestDist = d; closest = i; }
        }

        if(closest >= 0) {
            // Update existing contact — preserve accumulated impulses
            PersistedContact& pc = contacts[closest];
            pc.localA      = localA;
            pc.localB      = localB;
            pc.normal      = cp.normal;
            pc.depth       = cp.penetrationDepth;
            pc.frameStamp  = frameStamp;
            pc.fresh       = false;
        } else {
            // Add new contact
            if(count < kMaxContacts) {
                PersistedContact& pc = contacts[count++];
                pc.localA      = localA;
                pc.localB      = localB;
                pc.normal      = cp.normal;
                pc.depth       = cp.penetrationDepth;
                pc.frameStamp  = frameStamp;
                pc.normalImpulse    = 0.f;
                pc.tangentImpulse[0]= 0.f;
                pc.tangentImpulse[1]= 0.f;
                pc.fresh       = true;
            } else {
                // Replace the contact with least depth (keep most penetrating)
                int minIdx = 0;
                float minDepth = contacts[0].depth;
                for(int i = 1; i < count; i++) {
                    if(contacts[i].depth < minDepth) {
                        minDepth = contacts[i].depth; minIdx = i;
                    }
                }
                if(cp.penetrationDepth > minDepth) {
                    PersistedContact& pc = contacts[minIdx];
                    pc.localA      = localA;
                    pc.localB      = localB;
                    pc.normal      = cp.normal;
                    pc.depth       = cp.penetrationDepth;
                    pc.frameStamp  = frameStamp;
                    pc.normalImpulse    = 0.f;
                    pc.tangentImpulse[0]= 0.f;
                    pc.tangentImpulse[1]= 0.f;
                    pc.fresh       = true;
                }
            }
        }
    }

    void removeAt(int i) {
        if(i < count - 1) contacts[i] = contacts[count - 1];
        count--;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ContactCache — global store for all persistent manifolds
// ─────────────────────────────────────────────────────────────────────────────
class ContactCache {
public:
    static constexpr uint32_t kMaxAge = 3;  // frames before eviction

    // Get or create manifold for a pair
    PersistentManifold& getOrCreate(uint64_t bodyA, uint64_t bodyB) {
        uint64_t key = makePairKey(bodyA, bodyB);
        auto it = cache_.find(key);
        if(it != cache_.end()) return it->second;
        PersistentManifold& m = cache_[key];
        m.bodyA = bodyA; m.bodyB = bodyB;
        return m;
    }

    // Update manifold from fresh narrowphase result
    void update(const ContactManifold& fresh,
                const Transform& tA, const Transform& tB,
                uint32_t frameStamp)
    {
        if(!fresh.hit) return;
        auto& pm = getOrCreate(fresh.bodyA, fresh.bodyB);
        pm.update(fresh, tA, tB, frameStamp);
    }

    // Evict stale manifolds (no contact for kMaxAge frames)
    void evict(uint32_t currentFrame) {
        for(auto it = cache_.begin(); it != cache_.end(); ) {
            if(!it->second.isActive(currentFrame, kMaxAge))
                it = cache_.erase(it);
            else
                ++it;
        }
    }

    // Get warm-start impulses for a pair (0 if new pair)
    bool getWarmStart(uint64_t bodyA, uint64_t bodyB,
                      int contactIdx, float& normalImpulse,
                      float tangentImpulse[2]) const
    {
        uint64_t key = makePairKey(bodyA, bodyB);
        auto it = cache_.find(key);
        if(it == cache_.end()) return false;
        const PersistentManifold& pm = it->second;
        if(contactIdx >= pm.count) return false;
        const PersistedContact& pc = pm.contacts[contactIdx];
        if(pc.fresh) return false;
        normalImpulse    = pc.normalImpulse;
        tangentImpulse[0]= pc.tangentImpulse[0];
        tangentImpulse[1]= pc.tangentImpulse[1];
        return true;
    }

    // Store impulses after solver step (for next frame warm-start)
    void storeImpulse(uint64_t bodyA, uint64_t bodyB,
                      int contactIdx, float normalImpulse,
                      float tangentImpulse[2])
    {
        uint64_t key = makePairKey(bodyA, bodyB);
        auto it = cache_.find(key);
        if(it == cache_.end()) return;
        PersistentManifold& pm = it->second;
        if(contactIdx >= pm.count) return;
        PersistedContact& pc = pm.contacts[contactIdx];
        pc.normalImpulse    = normalImpulse;
        pc.tangentImpulse[0]= tangentImpulse[0];
        pc.tangentImpulse[1]= tangentImpulse[1];
    }

    // Iteration
    void forEach(const std::function<void(PersistentManifold&)>& fn) {
        for(auto& [key, pm] : cache_) fn(pm);
    }
    void forEachConst(const std::function<void(const PersistentManifold&)>& fn) const {
        for(auto& [key, pm] : cache_) fn(pm);
    }

    size_t size()  const { return cache_.size(); }
    void   clear()       { cache_.clear(); }

    // Build ContactManifold view from persistent data (for callbacks)
    ContactManifold toContactManifold(const PersistentManifold& pm,
                                      const Transform& tA,
                                      const Transform& tB) const
    {
        ContactManifold m;
        m.bodyA = pm.bodyA; m.bodyB = pm.bodyB;
        m.hit   = pm.count > 0;
        for(int i = 0; i < pm.count && i < ContactManifold::kMaxContacts; i++) {
            const PersistedContact& pc = pm.contacts[i];
            ContactPoint cp;
            cp.positionA       = tA.transformPoint(pc.localA);
            cp.positionB       = tB.transformPoint(pc.localB);
            cp.normal          = pc.normal;
            cp.penetrationDepth= pc.depth;
            m.addPoint(cp);
        }
        return m;
    }

private:
    std::unordered_map<uint64_t, PersistentManifold> cache_;
};

} // namespace col
