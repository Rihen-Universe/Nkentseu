#pragma once
// ccd.h — Continuous Collision Detection
// Prevents tunneling for fast-moving objects.
// Implements:
//   - Swept sphere (most common, cheapest)
//   - Linear cast / shape cast (GJK-based)
//   - TOI (Time of Impact) bisection solver

#include "shapes.h"
#include "narrowphase.h"

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  TOI result
// ─────────────────────────────────────────────────────────────────────────────
struct TOIResult {
    float toi     = 1.f;   // time of impact in [0,1] (1 = no impact this frame)
    bool  hit     = false;
    Vec3  normal  = {0,1,0};
    Vec3  pointA  = {0,0,0};
    Vec3  pointB  = {0,0,0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  SweptSphere — linear sweep of a sphere against a static shape
//  Fastest CCD: O(1) per pair, handles most gameplay cases
// ─────────────────────────────────────────────────────────────────────────────
class SweptSphere {
public:
    // Sweep sphere A (moving) against sphere B (static or also moving)
    static TOIResult sphereVsSphere(
        const Vec3& centerA0, const Vec3& centerA1, float radiusA,
        const Vec3& centerB0, const Vec3& centerB1, float radiusB)
    {
        TOIResult result;
        // Relative motion
        Vec3 d0 = centerA0 - centerB0;
        Vec3 d1 = centerA1 - centerB1;
        Vec3 delta = d1 - d0;   // relative displacement

        float rSum = radiusA + radiusB;
        float a = delta.dot(delta);
        float b = 2.f * d0.dot(delta);
        float c = d0.dot(d0) - rSum * rSum;

        if(c < 0.f) {
            // Already overlapping at t=0
            result.hit  = true;
            result.toi  = 0.f;
            result.normal = d0.normalized();
            return result;
        }
        if(std::abs(a) < kEpsilon) return result; // parallel, no impact

        float disc = b*b - 4.f*a*c;
        if(disc < 0.f) return result;  // miss

        float t = (-b - std::sqrt(disc)) / (2.f*a);
        if(t < 0.f || t > 1.f) return result;

        result.hit    = true;
        result.toi    = t;
        Vec3 posA     = Vec3::Lerp(centerA0, centerA1, t);
        Vec3 posB     = Vec3::Lerp(centerB0, centerB1, t);
        result.normal = (posA - posB).normalized();
        result.pointA = posA - result.normal * radiusA;
        result.pointB = posB + result.normal * radiusB;
        return result;
    }

    // Sweep sphere against AABB (Minkowski sum → larger AABB + point test)
    static TOIResult sphereVsAABB(
        const Vec3& c0, const Vec3& c1, float r,
        const AABB3D& box)
    {
        TOIResult result;
        // Expand AABB by sphere radius (Minkowski sum)
        AABB3D expanded = { box.min - Vec3(r), box.max + Vec3(r) };

        // Ray-AABB test
        Ray ray(c0, c1 - c0);
        float len = (c1 - c0).length();
        if(len < kEpsilon) return result;
        ray.tMax = len;

        float tHit;
        if(!expanded.raycast(ray, tHit)) return result;

        result.hit    = true;
        result.toi    = tHit / len;
        Vec3 hitPt    = ray.at(tHit);

        // Find contact normal: which face did we hit?
        Vec3 boxC     = box.center();
        Vec3 d        = hitPt - boxC;
        Vec3 e        = box.extents() + Vec3(r);
        Vec3 overlap  = e - d.abs();

        result.normal = {0,1,0};
        if(overlap.x < overlap.y && overlap.x < overlap.z)
            result.normal = {d.x>0?1.f:-1.f, 0, 0};
        else if(overlap.y < overlap.z)
            result.normal = {0, d.y>0?1.f:-1.f, 0};
        else
            result.normal = {0, 0, d.z>0?1.f:-1.f};

        result.pointA = hitPt - result.normal * r;
        result.pointB = Vec3::Min(Vec3::Max(hitPt, box.min), box.max);
        return result;
    }

    // Sweep capsule against sphere
    static TOIResult capsuleVsSphere(
        const Vec3& capA0, const Vec3& capA1,  // capsule seg start (t=0 and t=1)
        const Vec3& capB0, const Vec3& capB1,  // capsule seg end
        float capRadius,
        const Vec3& sphereC0, const Vec3& sphereC1, float sphereR)
    {
        TOIResult best; best.toi = 1.f;

        // Conservative: sweep the two endpoint spheres and take min TOI
        TOIResult r1 = sphereVsSphere(capA0, capA1, capRadius, sphereC0, sphereC1, sphereR);
        TOIResult r2 = sphereVsSphere(capB0, capB1, capRadius, sphereC0, sphereC1, sphereR);

        if(r1.hit && r1.toi < best.toi) best = r1;
        if(r2.hit && r2.toi < best.toi) best = r2;
        return best;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  GJK-based Shape Cast (linear cast)
//  Casts shape A moving from posA0 to posA1 against static shape B.
//  Returns first time of impact.
// ─────────────────────────────────────────────────────────────────────────────
class LinearCast {
public:
    static constexpr int   kMaxIter    = 32;
    static constexpr float kTolerance  = 1e-4f;

    // Shape A translates from (pos tA0) to (pos tA1), B is static at tB.
    static TOIResult cast(
        const CollisionShape& shapeA, const Transform& tA0, const Transform& tA1,
        const CollisionShape& shapeB, const Transform& tB)
    {
        TOIResult result;
        Vec3 displacement = tA1.position - tA0.position;
        float dist = displacement.length();
        if(dist < kEpsilon) return result;

        // Check if already overlapping at t=0
        auto m0 = Narrowphase::generateContacts(shapeA, tA0, shapeB, tB);
        if(m0.hit) {
            result.hit = true;
            result.toi = 0.f;
            if(m0.count > 0) result.normal = m0.points[0].normal;
            return result;
        }

        // Binary search on t ∈ [0, 1]
        float lo = 0.f, hi = 1.f;
        Transform tAMid = tA0;

        for(int iter = 0; iter < kMaxIter; iter++) {
            float mid = (lo + hi) * 0.5f;
            tAMid.position = Vec3::Lerp(tA0.position, tA1.position, mid);
            tAMid.rotation = Quat::slerp(tA0.rotation, tA1.rotation, mid);

            auto m = Narrowphase::generateContacts(shapeA, tAMid, shapeB, tB);
            if(m.hit) {
                hi = mid;
                if(m.count > 0) {
                    result.normal = m.points[0].normal;
                    result.pointA = m.points[0].positionA;
                    result.pointB = m.points[0].positionB;
                }
            } else {
                lo = mid;
            }

            if((hi - lo) < kTolerance) break;
        }

        if(hi < 1.f) {
            result.hit = true;
            result.toi = hi;
        }
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  TOI Solver — resolves TOI for a list of moving bodies
// ─────────────────────────────────────────────────────────────────────────────
struct CCDPair {
    uint64_t bodyA, bodyB;
    TOIResult result;
};

class TOISolver {
public:
    static constexpr float kMinTOI = 0.001f;  // don't resolve sub-millisecond TOIs

    // Find earliest TOI among all pairs where at least one body has CCD flag
    static CCDPair findEarliestImpact(
        const std::vector<CollidingPair>& pairs,
        const std::unordered_map<uint64_t, CollisionBody>& bodies)
    {
        CCDPair earliest; earliest.result.toi = 1.f;

        for(auto& pair : pairs) {
            auto itA = bodies.find(pair.bodyA);
            auto itB = bodies.find(pair.bodyB);
            if(itA == bodies.end() || itB == bodies.end()) continue;

            const CollisionBody& a = itA->second;
            const CollisionBody& b = itB->second;

            bool aCCD = a.flags & BodyFlag_CCD;
            bool bCCD = b.flags & BodyFlag_CCD;
            if(!aCCD && !bCCD) continue;

            // Build end-of-frame transforms
            float dt = 1.f/60.f;  // assume 60Hz
            Transform tA1 = a.transform;
            Transform tB1 = b.transform;
            if(aCCD) tA1.position = tA1.position + a.velocity * dt;
            if(bCCD) tB1.position = tB1.position + b.velocity * dt;

            TOIResult toi = LinearCast::cast(
                a.shape, a.transform, tA1,
                b.shape, b.transform);  // B static for now

            if(toi.hit && toi.toi < earliest.result.toi && toi.toi >= kMinTOI) {
                earliest.bodyA  = pair.bodyA;
                earliest.bodyB  = pair.bodyB;
                earliest.result = toi;
            }
        }
        return earliest;
    }

    // Sub-step: advance all bodies to TOI, resolve, then advance remainder
    static void resolveStep(
        CollisionBody& a, CollisionBody& b,
        const TOIResult& toi, float dt)
    {
        // Advance to TOI
        float t = toi.toi;
        a.transform.position = a.transform.position + a.velocity * (t * dt);
        b.transform.position = b.transform.position + b.velocity * (t * dt);

        // Simple velocity resolution (elastic impulse)
        float e = 0.3f;  // restitution
        Vec3 n  = toi.normal;
        Vec3 rv = a.velocity - b.velocity;
        float vn = rv.dot(n);

        if(vn < 0.f) {
            float j = -(1.f + e) * vn;
            // Approximate equal masses
            a.velocity = a.velocity + n * (j * 0.5f);
            b.velocity = b.velocity - n * (j * 0.5f);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CCD Manager — integrates into CollisionWorld
// ─────────────────────────────────────────────────────────────────────────────
class CCDManager {
public:
    // Per-frame CCD pass: finds and resolves earliest TOI among CCD bodies
    // Call AFTER broadphase, BEFORE standard narrowphase
    void process(
        std::vector<CollidingPair>& pairs,
        std::unordered_map<uint64_t, CollisionBody>& bodies,
        float dt,
        std::vector<ContactManifold>& contacts)
    {
        if(pairs.empty()) return;

        CCDPair earliest = TOISolver::findEarliestImpact(pairs, bodies);
        if(!earliest.result.hit) return;

        auto itA = bodies.find(earliest.bodyA);
        auto itB = bodies.find(earliest.bodyB);
        if(itA == bodies.end() || itB == bodies.end()) return;

        CollisionBody& a = itA->second;
        CollisionBody& b = itB->second;

        TOISolver::resolveStep(a, b, earliest.result, dt);

        // Add contact manifold
        ContactManifold m;
        m.hit    = true;
        m.bodyA  = earliest.bodyA;
        m.bodyB  = earliest.bodyB;
        ContactPoint cp;
        cp.positionA        = earliest.result.pointA;
        cp.positionB        = earliest.result.pointB;
        cp.normal           = earliest.result.normal;
        cp.penetrationDepth = 0.f;
        m.addPoint(cp);
        contacts.push_back(m);
    }

    // Quick sphere-based CCD pre-filter: skip pairs where max swept AABBs don't overlap
    static bool sweptAABBsOverlap(
        const CollisionBody& a, const CollisionBody& b, float dt)
    {
        // Expand AABB by velocity * dt
        AABB3D aabbA = a.cachedAABB;
        AABB3D aabbB = b.cachedAABB;

        if(a.flags & BodyFlag_CCD) {
            Vec3 vel = a.velocity * dt;
            if(vel.x > 0) aabbA.max.x += vel.x; else aabbA.min.x += vel.x;
            if(vel.y > 0) aabbA.max.y += vel.y; else aabbA.min.y += vel.y;
            if(vel.z > 0) aabbA.max.z += vel.z; else aabbA.min.z += vel.z;
        }
        if(b.flags & BodyFlag_CCD) {
            Vec3 vel = b.velocity * dt;
            if(vel.x > 0) aabbB.max.x += vel.x; else aabbB.min.x += vel.x;
            if(vel.y > 0) aabbB.max.y += vel.y; else aabbB.min.y += vel.y;
            if(vel.z > 0) aabbB.max.z += vel.z; else aabbB.min.z += vel.z;
        }
        return aabbA.overlaps(aabbB);
    }
};

} // namespace col
