#pragma once
// =============================================================================
// NkColCast.h — Casts génériques par CONSERVATIVE ADVANCEMENT (ZÉRO STL).
// Une forme convexe A translatée le long de `dir` ; on cherche le premier instant
// t (TOI) où elle touche une forme convexe B. Repose UNIQUEMENT sur les fonctions
// de support (modèle cœur+marge) -> couvre toutes les formes convexes :
//   • Raycast convexe   = caster un point (sphère r=0) le long du rayon.
//   • Shape cast (sphère/boîte/capsule…) = caster la forme elle-même.
// Algo : à chaque pas, GJK-distance entre A(+dir*t) et B donne la séparation et
// l'axe ; on avance t juste assez pour combler la séparation projetée sur dir.
// =============================================================================
#include "NKCollision/NkColGJK.h"

namespace nkentseu {
    namespace collision {

        namespace detail {
            // Support Minkowski (cœurs) avec A décalée d'un offset constant.
            NK_FORCE_INLINE NkSV3 NkMinkOff3D(const NkShape& A, const NkVec3f& offA, const NkShape& B, const NkVec3f& d) noexcept {
                NkVec3f sa = NkSupportCore3D(A, d) + offA;
                NkVec3f sb = NkSupportCore3D(B, d * -1.f);
                return { sa - sb, sa };
            }
            // GJK-distance entre (A + offA) et B (cœurs). Renseigne dist/closest/witnessA/intersect.
            inline void NkGJKDistOff3D(const NkShape& A, const NkVec3f& offA, const NkShape& B,
                                       float32& dist, NkVec3f& closest, NkVec3f& witnessA, bool& intersect) noexcept {
                intersect = false;
                NkVec3f dir = (NkShapeCenter3D(B) - (NkShapeCenter3D(A) + offA)) * -1.f; // vers l'origine
                if (dir.Dot(dir) < 1e-12f) dir = NkVec3f{ 1.f, 0.f, 0.f };
                NkSV3 s[4]; int32 n = 0;
                s[0] = NkMinkOff3D(A, offA, B, dir); n = 1;
                NkVec3f c = s[0].v; witnessA = s[0].a;
                for (int32 iter = 0; iter < 64; ++iter) {
                    if (c.Dot(c) < 1e-10f) { intersect = true; break; }
                    dir = c * -1.f;
                    NkSV3 w = NkMinkOff3D(A, offA, B, dir);
                    if (c.Dot(c) - c.Dot(w.v) <= 1e-8f * c.Dot(c)) break;
                    s[n++] = w;
                    bool inter = false; NkVec3f wA;
                    c = NkClosestSimplex3D(s, n, wA, inter); witnessA = wA;
                    if (inter) { intersect = true; break; }
                }
                dist = math::NkSqrt(c.Dot(c)); closest = c;
            }
        } // namespace detail

        // Cast de A (convexe) le long de dir (unité conseillée) jusqu'à toucher B (convexe).
        // tOut in [0,maxDist], normalOut = normale de surface sur B (de B vers A), pointOut ~ contact.
        inline bool NkConvexCast3D(const NkShape& A, const NkVec3f& dir, float32 maxDist, const NkShape& B,
                                   float32& tOut, NkVec3f& normalOut, NkVec3f& pointOut) noexcept {
            using namespace detail;
            const float32 tot = NkShapeMargin(A) + NkShapeMargin(B);
            float32 t = 0.f;
            for (int32 iter = 0; iter < 32; ++iter) {
                float32 dist; NkVec3f closest, witnessA; bool intersect;
                NkGJKDistOff3D(A, dir * t, B, dist, closest, witnessA, intersect);
                if (intersect) { tOut = t; normalOut = (dir.Dot(dir) > 0.f) ? dir.Normalized() * -1.f : NkVec3f{ 0,1,0 }; pointOut = witnessA; return true; }
                const float32 sep = dist - tot;
                const NkVec3f n = (dist > 1e-6f) ? closest * (1.f / dist) : NkVec3f{ 0,1,0 }; // de B vers A
                if (sep <= 1e-4f) { tOut = t; normalOut = n; pointOut = witnessA - n * NkShapeMargin(A); return true; }
                const float32 denom = -dir.Dot(n);       // progression de A vers B le long de dir
                if (denom <= 1e-6f) return false;         // ne s'approche pas -> rate
                t += sep / denom;
                if (t > maxDist) return false;
            }
            return false;
        }

        // Raycast générique contre une forme convexe (point casté le long du rayon).
        inline bool NkRayConvex3D(const NkRay3D& ray, const NkShape& shape, NkRayHit3D& hit) noexcept {
            NkShape pt = NkShape::Sphere(ray.origin, 0.f);   // caster un point
            float32 t; NkVec3f nrm, p;
            if (!NkConvexCast3D(pt, ray.dir, ray.maxT, shape, t, nrm, p)) return false;
            hit.hit = true; hit.t = t; hit.point = ray.origin + ray.dir * t; hit.normal = nrm;
            return true;
        }

    } // namespace collision
} // namespace nkentseu
