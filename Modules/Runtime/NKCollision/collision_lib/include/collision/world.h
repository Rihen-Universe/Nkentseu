#pragma once
#include "broadphase.h"
#include "narrowphase.h"
#include "gpu_backend.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Body flags
// ─────────────────────────────────────────────────────────────────────────────
enum BodyFlags : uint32_t {
    BodyFlag_None      = 0,
    BodyFlag_Static    = 1<<0,   // never moves
    BodyFlag_Kinematic = 1<<1,   // moved by user, not physics
    BodyFlag_Trigger   = 1<<2,   // no contact response, just events
    BodyFlag_Sleeping  = 1<<3,   // broadphase skips sleeping pairs
    BodyFlag_Is2D      = 1<<4,   // 2D collision in XY plane
    BodyFlag_CCD       = 1<<5,   // continuous collision detection
};

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionBody — one collidable entity
// ─────────────────────────────────────────────────────────────────────────────
struct CollisionBody {
    uint64_t      id        = 0;
    CollisionShape shape;
    Transform     transform;
    Vec3          velocity;         // for predictive AABB
    uint32_t      flags     = 0;
    uint32_t      layer     = 0xFFFFFFFF;  // bitmask
    uint32_t      mask      = 0xFFFFFFFF;  // which layers this body collides with
    void*         userData  = nullptr;
    AABB3D        cachedAABB;

    bool isStatic()    const { return flags & BodyFlag_Static; }
    bool isKinematic() const { return flags & BodyFlag_Kinematic; }
    bool isTrigger()   const { return flags & BodyFlag_Trigger; }
    bool isSleeping()  const { return flags & BodyFlag_Sleeping; }
    bool is2D()        const { return flags & BodyFlag_Is2D; }
    bool collidesWith(const CollisionBody& o) const {
        return (layer & o.mask) && (o.layer & mask);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Raycast result
// ─────────────────────────────────────────────────────────────────────────────
struct RaycastHit {
    uint64_t bodyId   = 0;
    Vec3     point    = {0,0,0};
    Vec3     normal   = {0,1,0};
    float    distance = kInfinity;
    void*    userData = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Collision event callbacks
// ─────────────────────────────────────────────────────────────────────────────
using ContactCallback = std::function<void(const ContactManifold&,
                                           const CollisionBody&,
                                           const CollisionBody&)>;
using TriggerCallback = std::function<void(uint64_t bodyA, uint64_t bodyB, bool entered)>;

// ─────────────────────────────────────────────────────────────────────────────
//  World configuration
// ─────────────────────────────────────────────────────────────────────────────
struct WorldConfig {
    GPUBackend       gpuBackend      = GPUBackend::Auto;
    BroadphaseType   broadphase      = BroadphaseType::DynamicBVH;
    int              threadCount     = -1;   // -1 = hardware_concurrency
    size_t           maxBodies       = 65536;
    size_t           maxPairs        = 1<<20;  // 1M pairs
    size_t           maxContacts     = 1<<18;
    bool             enableGPU       = true;
    uint32_t         gpuThreshold    = 512;   // switch to GPU above this body count
    bool             persistContacts = true;
    float            contactBreakDist= 0.02f;
    bool             multithreaded   = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ObjectPool — zero-allocation pool for fixed-size objects
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity) {
        storage_.resize(capacity);
        free_.reserve(capacity);
        for(int i=(int)capacity-1;i>=0;i--) free_.push_back(i);
    }
    T* alloc() {
        if(free_.empty()) return nullptr;
        int idx = free_.back(); free_.pop_back();
        return &storage_[idx];
    }
    void free(T* ptr) {
        int idx = (int)(ptr - storage_.data());
        free_.push_back(idx);
    }
    void reset() {
        free_.clear();
        for(int i=(int)storage_.size()-1;i>=0;i--) free_.push_back(i);
    }
    size_t capacity() const { return storage_.size(); }
    size_t used()     const { return storage_.size() - free_.size(); }

private:
    std::vector<T>   storage_;
    std::vector<int> free_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionWorld — main API
// ─────────────────────────────────────────────────────────────────────────────
class CollisionWorld {
public:
    explicit CollisionWorld(const WorldConfig& cfg = WorldConfig{})
        : cfg_(cfg),
          broadphase_(cfg.broadphase),
          manifoldPool_(cfg.maxContacts),
          pairBuffer_(cfg.maxPairs),
          contactBuffer_(cfg.maxContacts)
    {
        bodies_.reserve(cfg.maxBodies);
        backend_ = BackendFactory::create(cfg.enableGPU ? cfg.gpuBackend : GPUBackend::None);

        if(cfg.multithreaded) {
            int nc = cfg.threadCount>0 ? cfg.threadCount
                                       : (int)std::thread::hardware_concurrency();
            threadCount_ = std::max(1, nc);
        }

        // Pre-allocate GPU buffers
        if(backend_->isGPU()) {
            size_t aabbBytes = cfg.maxBodies * sizeof(GPUBodyAABB);
            size_t pairBytes = cfg.maxPairs  * sizeof(GPUPair);
            size_t ctBytes   = cfg.maxContacts * sizeof(GPUContact);
            gpuAABBs_    = backend_->createBuffer(aabbBytes, false);
            gpuPairs_    = backend_->createBuffer(pairBytes, false);
            gpuContacts_ = backend_->createBuffer(ctBytes, true);
        }
    }

    ~CollisionWorld() {
        if(backend_->isGPU()) {
            backend_->destroyBuffer(gpuAABBs_);
            backend_->destroyBuffer(gpuPairs_);
            backend_->destroyBuffer(gpuContacts_);
        }
    }

    // ── Body management ──────────────────────────────────────────────────────

    uint64_t addBody(const CollisionBody& body) {
        uint64_t id = nextId_++;
        CollisionBody b = body;
        b.id = id;
        b.cachedAABB = computeAABB(b);
        bodies_[id] = std::move(b);
        broadphase_.insert(id, bodies_[id].cachedAABB);
        return id;
    }

    void removeBody(uint64_t id) {
        broadphase_.remove(id);
        bodies_.erase(id);
        persistentManifolds_.erase(id);
    }

    void setTransform(uint64_t id, const Transform& t) {
        auto it = bodies_.find(id);
        if(it==bodies_.end()) return;
        it->second.transform = t;
        it->second.cachedAABB = computeAABB(it->second);
        broadphase_.update(id, it->second.cachedAABB, it->second.velocity);
    }

    void setVelocity(uint64_t id, const Vec3& vel) {
        auto it = bodies_.find(id);
        if(it!=bodies_.end()) it->second.velocity = vel;
    }

    CollisionBody* getBody(uint64_t id) {
        auto it = bodies_.find(id);
        return it!=bodies_.end() ? &it->second : nullptr;
    }

    // ── Callbacks ────────────────────────────────────────────────────────────
    void setContactCallback(ContactCallback cb)  { contactCb_ = std::move(cb); }
    void setTriggerCallback(TriggerCallback cb)  { triggerCb_ = std::move(cb); }

    // ── Main update ──────────────────────────────────────────────────────────
    void step(float dt = 0.016f) {
        (void)dt;
        frameStamp_++;

        // Choose CPU vs GPU path
        bool useGPU = backend_->isGPU() && bodies_.size() >= cfg_.gpuThreshold;

        if(useGPU) {
            stepGPU();
        } else {
            stepCPU();
        }

        // Fire callbacks
        fireCallbacks();
    }

    // ── Queries ──────────────────────────────────────────────────────────────
    bool raycast(const Ray& ray, RaycastHit& hit) const {
        hit = {};
        float best = ray.tMax;
        bool found = false;

        broadphase_.rayQuery(ray, [&](uint64_t bodyId, float) -> bool {
            auto it = bodies_.find(bodyId);
            if(it==bodies_.end()) return true;
            // TODO: detailed ray vs shape test
            float t;
            if(it->second.cachedAABB.raycast(ray, t) && t < best) {
                best = t;
                hit.bodyId   = bodyId;
                hit.point    = ray.at(t);
                hit.distance = t;
                hit.userData = it->second.userData;
                found = true;
            }
            return true;
        });
        return found;
    }

    void overlapAABB(const AABB3D& aabb,
                     std::vector<uint64_t>& results) const {
        results.clear();
        broadphase_.query(aabb, [&](uint64_t id) -> bool {
            results.push_back(id); return true;
        });
    }

    bool testBodyBody(uint64_t idA, uint64_t idB, ContactManifold& out) {
        auto itA=bodies_.find(idA), itB=bodies_.find(idB);
        if(itA==bodies_.end()||itB==bodies_.end()) return false;
        out = Narrowphase::generateContacts(
            itA->second.shape, itA->second.transform,
            itB->second.shape, itB->second.transform);
        return out.hit;
    }

    // ── State ────────────────────────────────────────────────────────────────
    size_t   bodyCount()    const { return bodies_.size(); }
    size_t   contactCount() const { return currentContacts_.size(); }
    uint32_t frameStamp()   const { return frameStamp_; }
    ICollisionBackend* backend() { return backend_.get(); }

    const std::vector<ContactManifold>& contacts() const { return currentContacts_; }

private:
    // ── CPU step ──────────────────────────────────────────────────────────────
    void stepCPU() {
        currentContacts_.clear();

        // Broadphase
        broadphase_.collectPairs(pairBuffer_);

        // Narrowphase (optionally multi-threaded)
        if(threadCount_>1 && pairBuffer_.size()>128) {
            stepCPUParallel();
        } else {
            stepCPUSingle();
        }
    }

    void stepCPUSingle() {
        for(auto& pair : pairBuffer_) {
            auto itA = bodies_.find(pair.bodyA);
            auto itB = bodies_.find(pair.bodyB);
            if(itA==bodies_.end()||itB==bodies_.end()) continue;

            const CollisionBody& a = itA->second;
            const CollisionBody& b = itB->second;

            if(!a.collidesWith(b)) continue;
            if(a.isSleeping() && b.isSleeping()) continue;

            // Static-static: skip
            if(a.isStatic() && b.isStatic()) continue;

            ContactManifold m = Narrowphase::generateContacts(
                a.shape, a.transform, b.shape, b.transform);

            if(m.hit) {
                m.bodyA      = pair.bodyA;
                m.bodyB      = pair.bodyB;
                m.frameStamp = frameStamp_;
                currentContacts_.push_back(m);
            }
        }
    }

    void stepCPUParallel() {
        // Split pairs across threads
        size_t N = pairBuffer_.size();
        size_t chunk = (N + threadCount_ - 1) / threadCount_;

        std::vector<std::vector<ContactManifold>> threadResults(threadCount_);
        std::vector<std::thread> threads;

        for(int t=0;t<threadCount_;t++) {
            size_t start = t*chunk, end = std::min(start+chunk, N);
            threads.emplace_back([&,t,start,end](){
                for(size_t k=start;k<end;k++) {
                    auto& pair = pairBuffer_[k];
                    auto itA=bodies_.find(pair.bodyA), itB=bodies_.find(pair.bodyB);
                    if(itA==bodies_.end()||itB==bodies_.end()) continue;
                    const CollisionBody& a=itA->second, &b=itB->second;
                    if(!a.collidesWith(b)) continue;
                    if(a.isStatic()&&b.isStatic()) continue;
                    ContactManifold m = Narrowphase::generateContacts(
                        a.shape,a.transform,b.shape,b.transform);
                    if(m.hit) {
                        m.bodyA=pair.bodyA; m.bodyB=pair.bodyB;
                        m.frameStamp=frameStamp_;
                        threadResults[t].push_back(m);
                    }
                }
            });
        }
        for(auto& th:threads) th.join();
        for(auto& res:threadResults)
            currentContacts_.insert(currentContacts_.end(),res.begin(),res.end());
    }

    // ── GPU step ──────────────────────────────────────────────────────────────
    void stepGPU() {
        currentContacts_.clear();
        if(bodies_.empty()) return;

        // Pack AABBs into flat buffer
        gpuAABBFlat_.clear();
        gpuBodyIndex_.clear();
        gpuAABBFlat_.reserve(bodies_.size());

        for(auto& [id,body] : bodies_) {
            GPUBodyAABB g;
            g.minX=body.cachedAABB.min.x; g.minY=body.cachedAABB.min.y; g.minZ=body.cachedAABB.min.z;
            g.maxX=body.cachedAABB.max.x; g.maxY=body.cachedAABB.max.y; g.maxZ=body.cachedAABB.max.z;
            g.bodyId = id;
            gpuBodyIndex_.push_back(id);
            gpuAABBFlat_.push_back(g);
        }

        uint32_t count = (uint32_t)gpuAABBFlat_.size();
        backend_->uploadBuffer(gpuAABBs_, gpuAABBFlat_.data(),
                               count * sizeof(GPUBodyAABB));

        // GPU broadphase
        uint32_t pairCount = backend_->gpuBroadphase(
            gpuAABBs_, count, gpuPairs_, (uint32_t)cfg_.maxPairs);

        if(pairCount==0) return;

        // GPU narrowphase (sphere approx) + CPU refine
        uint32_t contactCount = backend_->gpuNarrowphase(
            gpuAABBs_, gpuPairs_, pairCount, gpuContacts_, (uint32_t)cfg_.maxContacts);

        // Download contacts
        if(contactCount>0) {
            gpuContactFlat_.resize(contactCount);
            backend_->downloadBuffer(gpuContactFlat_.data(), gpuContacts_,
                                     contactCount * sizeof(GPUContact));
            for(auto& gc : gpuContactFlat_) {
                ContactManifold m;
                m.hit=true; m.bodyA=gc.bodyA; m.bodyB=gc.bodyB;
                m.frameStamp=frameStamp_;
                ContactPoint cp;
                cp.positionA={gc.posAx,gc.posAy,gc.posAz};
                cp.positionB={gc.posBx,gc.posBy,gc.posBz};
                cp.normal   ={gc.normX,gc.normY,gc.normZ};
                cp.penetrationDepth=gc.depth;
                m.addPoint(cp);
                currentContacts_.push_back(m);
            }
        }
    }

    // ── Callbacks ────────────────────────────────────────────────────────────
    void fireCallbacks() {
        if(!contactCb_ && !triggerCb_) return;
        for(auto& m : currentContacts_) {
            auto itA=bodies_.find(m.bodyA), itB=bodies_.find(m.bodyB);
            if(itA==bodies_.end()||itB==bodies_.end()) continue;
            bool aTrig=itA->second.isTrigger(), bTrig=itB->second.isTrigger();
            if((aTrig||bTrig) && triggerCb_) {
                triggerCb_(m.bodyA, m.bodyB, true);
            } else if(contactCb_) {
                contactCb_(m, itA->second, itB->second);
            }
        }
    }

    // ── AABB computation ─────────────────────────────────────────────────────
    AABB3D computeAABB(const CollisionBody& body) const {
        return body.shape.worldAABB(body.transform);
    }

    // ── Members ───────────────────────────────────────────────────────────────
    WorldConfig   cfg_;
    Broadphase    broadphase_;
    std::unique_ptr<ICollisionBackend> backend_;

    std::unordered_map<uint64_t, CollisionBody> bodies_;
    std::atomic<uint64_t>                       nextId_ {1};
    uint32_t                                    frameStamp_ = 0;
    int                                         threadCount_ = 1;

    // Zero-alloc pools
    ObjectPool<ContactManifold>           manifoldPool_;
    std::vector<CollidingPair>            pairBuffer_;
    std::vector<ContactManifold>          contactBuffer_;
    std::vector<ContactManifold>          currentContacts_;

    // Persistent contacts (for warm-starting)
    std::unordered_map<uint64_t, std::vector<ContactManifold>> persistentManifolds_;

    // GPU resources
    GPUBuffer gpuAABBs_, gpuPairs_, gpuContacts_;
    std::vector<GPUBodyAABB> gpuAABBFlat_;
    std::vector<GPUContact>  gpuContactFlat_;
    std::vector<uint64_t>    gpuBodyIndex_;

    // Callbacks
    ContactCallback contactCb_;
    TriggerCallback triggerCb_;
};

} // namespace col
