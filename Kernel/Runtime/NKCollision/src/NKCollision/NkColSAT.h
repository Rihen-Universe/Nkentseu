#pragma once
// =============================================================================
// NkColSAT.h — Boîtes ORIENTÉES (OBB) via Séparation d'Axes (SAT), ZÉRO STL.
// Complète NkColTests.h (qui traite les boîtes en AABB). Ici la rotation est
// prise en compte -> OBB-OBB et OBB-cercle (2D). 3D OBB-OBB ajouté ensuite.
// Manifold : normale A->B (axe de pénétration minimale) + profondeur.
// =============================================================================
#include "NKCollision/NkColTests.h"

namespace nkentseu {
    namespace collision {

        // Repère local d'une OBB 2D : axes u (selon rotation) et v (perpendiculaire).
        NK_FORCE_INLINE void NkOBB2DAxes(float32 rot, NkVec2f& u, NkVec2f& v) noexcept {
            const float32 c = math::NkCos(rot), s = math::NkSin(rot);
            u = { c, s }; v = { -s, c };
        }

        // Rayon de projection d'une OBB 2D sur un axe normalisé `axis`.
        NK_FORCE_INLINE float32 NkOBB2DProjRadius(const NkVec2f& h, const NkVec2f& u,
                                                  const NkVec2f& v, const NkVec2f& axis) noexcept {
            return h.x * math::NkAbs(u.Dot(axis)) + h.y * math::NkAbs(v.Dot(axis));
        }

        // OBB 2D vs OBB 2D (SAT, 4 axes). Manifold normale A->B + profondeur.
        inline bool NkOBB2DvsOBB2D(const NkVec2f& c0, const NkVec2f& h0, float32 r0,
                                   const NkVec2f& c1, const NkVec2f& h1, float32 r1,
                                   NkManifold2D& m) noexcept {
            NkVec2f u0, v0, u1, v1; NkOBB2DAxes(r0, u0, v0); NkOBB2DAxes(r1, u1, v1);
            const NkVec2f d = c1 - c0;
            const NkVec2f axes[4] = { u0, v0, u1, v1 };
            float32 minOverlap = 1e30f; NkVec2f minAxis{ 1.f, 0.f };
            for (int32 i = 0; i < 4; ++i) {
                const NkVec2f ax = axes[i];
                float32 ra = NkOBB2DProjRadius(h0, u0, v0, ax);
                float32 rb = NkOBB2DProjRadius(h1, u1, v1, ax);
                float32 dist = math::NkAbs(d.Dot(ax));
                float32 overlap = (ra + rb) - dist;
                if (overlap <= 0.f) return false;                 // axe séparateur trouvé
                if (overlap < minOverlap) { minOverlap = overlap; minAxis = ax; }
            }
            // Oriente la normale de A vers B.
            if (d.Dot(minAxis) < 0.f) minAxis = minAxis * -1.f;
            m.normal = minAxis; m.points[0].depth = minOverlap;
            m.points[0].point = c0 + minAxis * (NkOBB2DProjRadius(h0, u0, v0, minAxis));
            m.count = 1; return true;
        }

        // OBB 2D vs cercle : on passe le cercle dans le repère local de l'OBB, on
        // applique le test cercle-AABB local, puis on re-transforme la normale.
        inline bool NkOBB2DvsCircle(const NkVec2f& bc, const NkVec2f& bh, float32 brot,
                                    const NkVec2f& sc, float32 sr, NkManifold2D& m) noexcept {
            NkVec2f u, v; NkOBB2DAxes(brot, u, v);
            NkVec2f rel = sc - bc;
            NkVec2f local{ rel.Dot(u), rel.Dot(v) };              // cercle en repère boîte
            NkVec2f closest{ math::NkClamp(local.x, -bh.x, bh.x), math::NkClamp(local.y, -bh.y, bh.y) };
            NkVec2f diff = local - closest; float32 dist2 = diff.Dot(diff);
            if (dist2 > sr * sr) return false;
            float32 dist = math::NkSqrt(dist2);
            NkVec2f nLocal = (dist > 1e-6f) ? diff * (1.f / dist) : NkVec2f{ 0.f, 1.f };
            // Normale boîte->cercle en monde : nLocal.x*u + nLocal.y*v.
            m.normal = u * nLocal.x + v * nLocal.y;
            m.points[0].depth = sr - dist;
            NkVec2f cpLocal = closest;
            m.points[0].point = bc + u * cpLocal.x + v * cpLocal.y;
            m.count = 1; return true;
        }

    } // namespace collision
} // namespace nkentseu
