#pragma once
// =============================================================================
// NkColTests.h — Narrowphase analytique (ZÉRO STL). Génère des manifolds
// (normale A->B + profondeur) et résout les raycasts. 2D + 3D.
//   3D : sphère/sphère, sphère/boîte, boîte/boîte, sphère/capsule, capsule/capsule
//   2D : cercle/cercle, cercle/boîte, boîte/boîte
//   Ray : sphère/cercle, AABB (slab)
// Boîtes traitées AABB en v1 (rotation -> SAT/OBB en phase suivante).
// =============================================================================
#include "NKCollision/NkColShapes.h"

namespace nkentseu {
    namespace collision {

        // ── Helpers ──────────────────────────────────────────────────────────
        NK_FORCE_INLINE float32 NkLenSq(const NkVec3f& v) noexcept { return v.Dot(v); }
        NK_FORCE_INLINE float32 NkLen(const NkVec3f& v) noexcept { return math::NkSqrt(v.Dot(v)); }
        NK_FORCE_INLINE float32 NkLenSq(const NkVec2f& v) noexcept { return v.Dot(v); }
        NK_FORCE_INLINE float32 NkLen(const NkVec2f& v) noexcept { return math::NkSqrt(v.Dot(v)); }

        // Point du segment [a,b] le plus proche de p (paramètre t clampé [0,1]).
        NK_FORCE_INLINE NkVec3f NkClosestOnSegment(const NkVec3f& p, const NkVec3f& a, const NkVec3f& b) noexcept {
            NkVec3f ab = b - a; float32 denom = ab.Dot(ab);
            if (denom < 1e-12f) return a;
            float32 t = math::NkClamp((p - a).Dot(ab) / denom, 0.f, 1.f);
            return a + ab * t;
        }

        // Plus courte distance entre deux segments -> points c1 (sur seg1) et c2 (sur seg2).
        // (Ericson, Real-Time Collision Detection.)
        inline void NkClosestSegmentSegment(const NkVec3f& p1, const NkVec3f& q1,
                                            const NkVec3f& p2, const NkVec3f& q2,
                                            NkVec3f& c1, NkVec3f& c2) noexcept {
            NkVec3f d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
            float32 a = d1.Dot(d1), e = d2.Dot(d2), f = d2.Dot(r);
            float32 s, t;
            if (a < 1e-12f && e < 1e-12f) { c1 = p1; c2 = p2; return; }
            if (a < 1e-12f) { s = 0.f; t = math::NkClamp(f / e, 0.f, 1.f); }
            else {
                float32 c = d1.Dot(r);
                if (e < 1e-12f) { t = 0.f; s = math::NkClamp(-c / a, 0.f, 1.f); }
                else {
                    float32 b = d1.Dot(d2), denom = a * e - b * b;
                    s = (denom > 1e-12f) ? math::NkClamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;
                    t = (b * s + f) / e;
                    if (t < 0.f)      { t = 0.f; s = math::NkClamp(-c / a, 0.f, 1.f); }
                    else if (t > 1.f) { t = 1.f; s = math::NkClamp((b - c) / a, 0.f, 1.f); }
                }
            }
            c1 = p1 + d1 * s; c2 = p2 + d2 * t;
        }

        // ── 3D ───────────────────────────────────────────────────────────────
        inline bool NkSphereSphere(const NkVec3f& c0, float32 r0, const NkVec3f& c1, float32 r1, NkManifold3D& m) noexcept {
            NkVec3f d = c1 - c0; float32 dist2 = d.Dot(d), rs = r0 + r1;
            if (dist2 > rs * rs) return false;
            float32 dist = math::NkSqrt(dist2);
            m.normal = (dist > 1e-6f) ? d * (1.f / dist) : NkVec3f{ 0.f, 1.f, 0.f };
            m.points[0].point = c0 + m.normal * r0;
            m.points[0].depth = rs - dist; m.count = 1; return true;
        }
        // Sphère vs boîte AABB (centre boxC, demi-extents boxH).
        inline bool NkSphereBox(const NkVec3f& sc, float32 sr, const NkVec3f& bc, const NkVec3f& bh, NkManifold3D& m) noexcept {
            NkVec3f d = sc - bc;
            NkVec3f closest{ math::NkClamp(d.x, -bh.x, bh.x), math::NkClamp(d.y, -bh.y, bh.y), math::NkClamp(d.z, -bh.z, bh.z) };
            NkVec3f cp = bc + closest; NkVec3f diff = sc - cp; float32 dist2 = diff.Dot(diff);
            if (dist2 > sr * sr) return false;
            float32 dist = math::NkSqrt(dist2);
            m.normal = (dist > 1e-6f) ? diff * (1.f / dist) : NkVec3f{ 0.f, 1.f, 0.f };
            m.points[0].point = cp; m.points[0].depth = sr - dist; m.count = 1; return true;
        }
        // Boîte AABB vs boîte AABB -> axe de pénétration minimale.
        inline bool NkBoxBox3D(const NkVec3f& c0, const NkVec3f& h0, const NkVec3f& c1, const NkVec3f& h1, NkManifold3D& m) noexcept {
            NkVec3f d = c1 - c0;
            float32 ox = (h0.x + h1.x) - math::NkAbs(d.x); if (ox <= 0.f) return false;
            float32 oy = (h0.y + h1.y) - math::NkAbs(d.y); if (oy <= 0.f) return false;
            float32 oz = (h0.z + h1.z) - math::NkAbs(d.z); if (oz <= 0.f) return false;
            if (ox < oy && ox < oz)      m.normal = { d.x < 0.f ? -1.f : 1.f, 0.f, 0.f }, m.points[0].depth = ox;
            else if (oy < oz)            m.normal = { 0.f, d.y < 0.f ? -1.f : 1.f, 0.f }, m.points[0].depth = oy;
            else                         m.normal = { 0.f, 0.f, d.z < 0.f ? -1.f : 1.f }, m.points[0].depth = oz;
            m.points[0].point = c0 + d * 0.5f; m.count = 1; return true;
        }
        inline bool NkSphereCapsule(const NkVec3f& sc, float32 sr, const NkVec3f& ca, const NkVec3f& cb, float32 cr, NkManifold3D& m) noexcept {
            NkVec3f cp = NkClosestOnSegment(sc, ca, cb);
            return NkSphereSphere(cp, cr, sc, sr, m) ? (m.normal = m.normal * -1.f, true) : false;
        }
        inline bool NkCapsuleCapsule(const NkVec3f& a0, const NkVec3f& b0, float32 r0, const NkVec3f& a1, const NkVec3f& b1, float32 r1, NkManifold3D& m) noexcept {
            NkVec3f c0, c1; NkClosestSegmentSegment(a0, b0, a1, b1, c0, c1);
            return NkSphereSphere(c0, r0, c1, r1, m);
        }

        // ── 2D ───────────────────────────────────────────────────────────────
        inline bool NkCircleCircle(const NkVec2f& c0, float32 r0, const NkVec2f& c1, float32 r1, NkManifold2D& m) noexcept {
            NkVec2f d = c1 - c0; float32 dist2 = d.Dot(d), rs = r0 + r1;
            if (dist2 > rs * rs) return false;
            float32 dist = math::NkSqrt(dist2);
            m.normal = (dist > 1e-6f) ? d * (1.f / dist) : NkVec2f{ 0.f, 1.f };
            m.points[0].point = c0 + m.normal * r0; m.points[0].depth = rs - dist; m.count = 1; return true;
        }
        inline bool NkCircleBox2D(const NkVec2f& sc, float32 sr, const NkVec2f& bc, const NkVec2f& bh, NkManifold2D& m) noexcept {
            NkVec2f d = sc - bc;
            NkVec2f closest{ math::NkClamp(d.x, -bh.x, bh.x), math::NkClamp(d.y, -bh.y, bh.y) };
            NkVec2f cp = bc + closest; NkVec2f diff = sc - cp; float32 dist2 = diff.Dot(diff);
            if (dist2 > sr * sr) return false;
            float32 dist = math::NkSqrt(dist2);
            m.normal = (dist > 1e-6f) ? diff * (1.f / dist) : NkVec2f{ 0.f, 1.f };
            m.points[0].point = cp; m.points[0].depth = sr - dist; m.count = 1; return true;
        }
        inline bool NkBoxBox2D(const NkVec2f& c0, const NkVec2f& h0, const NkVec2f& c1, const NkVec2f& h1, NkManifold2D& m) noexcept {
            NkVec2f d = c1 - c0;
            float32 ox = (h0.x + h1.x) - math::NkAbs(d.x); if (ox <= 0.f) return false;
            float32 oy = (h0.y + h1.y) - math::NkAbs(d.y); if (oy <= 0.f) return false;
            if (ox < oy) m.normal = { d.x < 0.f ? -1.f : 1.f, 0.f }, m.points[0].depth = ox;
            else         m.normal = { 0.f, d.y < 0.f ? -1.f : 1.f }, m.points[0].depth = oy;
            m.points[0].point = c0 + d * 0.5f; m.count = 1; return true;
        }

        // ── Raycast ──────────────────────────────────────────────────────────
        inline bool NkRaySphere(const NkRay3D& ray, const NkVec3f& c, float32 r, NkRayHit3D& hit) noexcept {
            NkVec3f oc = ray.origin - c; float32 b = oc.Dot(ray.dir), cc = oc.Dot(oc) - r * r;
            float32 disc = b * b - cc; if (disc < 0.f) return false;
            float32 t = -b - math::NkSqrt(disc); if (t < 0.f) t = -b + math::NkSqrt(disc);
            if (t < 0.f || t > ray.maxT) return false;
            hit.hit = true; hit.t = t; hit.point = ray.origin + ray.dir * t;
            hit.normal = (hit.point - c).Normalized(); return true;
        }
        inline bool NkRayAABB3D(const NkRay3D& ray, const NkVec3f& mn, const NkVec3f& mx, NkRayHit3D& hit) noexcept {
            float32 tmin = 0.f, tmax = ray.maxT; NkVec3f n{};
            for (int32 i = 0; i < 3; ++i) {
                float32 o = ray.origin[(size_t)i], dv = ray.dir[(size_t)i];
                float32 lo = mn[(size_t)i], hi = mx[(size_t)i];
                if (math::NkAbs(dv) < 1e-8f) { if (o < lo || o > hi) return false; continue; }
                float32 inv = 1.f / dv, t1 = (lo - o) * inv, t2 = (hi - o) * inv;
                float32 sign = -1.f; if (t1 > t2) { float32 tmp = t1; t1 = t2; t2 = tmp; sign = 1.f; }
                if (t1 > tmin) { tmin = t1; n = {}; n[(size_t)i] = sign; }
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return false;
            }
            hit.hit = true; hit.t = tmin; hit.point = ray.origin + ray.dir * tmin; hit.normal = n; return true;
        }
        inline bool NkRayCircle2D(const NkRay2D& ray, const NkVec2f& c, float32 r, NkRayHit2D& hit) noexcept {
            NkVec2f oc = ray.origin - c; float32 b = oc.Dot(ray.dir), cc = oc.Dot(oc) - r * r;
            float32 disc = b * b - cc; if (disc < 0.f) return false;
            float32 t = -b - math::NkSqrt(disc); if (t < 0.f) t = -b + math::NkSqrt(disc);
            if (t < 0.f || t > ray.maxT) return false;
            hit.hit = true; hit.t = t; hit.point = ray.origin + ray.dir * t;
            hit.normal = (hit.point - c).Normalized(); return true;
        }

    } // namespace collision
} // namespace nkentseu
