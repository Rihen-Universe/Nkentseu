#pragma once
// =============================================================================
// NkColConcave.h — Narrowphase des formes CONCAVES par DÉCOMPOSITION (ZÉRO STL).
// Une forme concave (trimesh / heightfield / chain) n'est pas GJK-able d'un bloc :
// on la décompose en morceaux CONVEXES (triangles, segments) que l'on teste un à
// un contre la forme convexe adverse via le narrowphase générique (GJK/EPA),
// avec un cull AABB par morceau. v1 : on renvoie le contact le PLUS PROFOND
// (manifold multi-points par clipping = vague suivante).
//
// Convention : forme concave = A, forme convexe = B -> normale A->B (mesh->convexe).
// Le dispatch du world rétablit l'ordre réel des corps.
// =============================================================================
#include "NKCollision/NkColGJK.h"

namespace nkentseu {
    namespace collision {

        // ── Convexe vs Triangle mesh (statique) ──────────────────────────────
        inline bool NkConvexVsTrimesh3D(const NkShape& mesh, const NkShape& convex, NkManifold3D& out) noexcept {
            if (!mesh.verts || !mesh.indices || mesh.indexCount < 3) return false;
            const NkAABB3D cb = NkComputeAABB3D(convex);
            bool any = false; float32 bestDepth = -1.f;
            for (uint32 i = 0; i + 2 < mesh.indexCount; i += 3) {
                NkVec3f tv[3] = { mesh.verts[mesh.indices[i]], mesh.verts[mesh.indices[i + 1]], mesh.verts[mesh.indices[i + 2]] };
                NkAABB3D tb; tb.min = tb.max = tv[0]; tb.Expand(tv[1]); tb.Expand(tv[2]);
                if (!cb.Overlaps(tb)) continue;
                NkShape tri = NkShape::Triangle3D(tv);          // tv vivant le temps de l'appel
                NkManifold3D m;
                if (NkGJKEPA3D(tri, convex, m) && m.points[0].depth > bestDepth) {
                    bestDepth = m.points[0].depth; out = m; any = true;
                }
            }
            return any;
        }

        // ── Convexe vs Heightfield (grille rows*cols, 2 triangles par cellule) ─
        inline bool NkConvexVsHeightfield3D(const NkShape& hf, const NkShape& convex, NkManifold3D& out) noexcept {
            if (!hf.heights || hf.rows < 2 || hf.cols < 2) return false;
            const float32 dx = hf.p1.x, dz = hf.p1.z;
            if (dx <= 0.f || dz <= 0.f) return false;
            const NkAABB3D cb = NkComputeAABB3D(convex);
            // plage de cellules couvrant l'AABB du convexe (x=colonnes, z=lignes)
            auto clampi = [](int32 v, int32 lo, int32 hi) { return v < lo ? lo : (v > hi ? hi : v); };
            int32 c0 = clampi((int32)math::NkFloor((cb.min.x - hf.p0.x) / dx), 0, (int32)hf.cols - 2);
            int32 c1 = clampi((int32)math::NkFloor((cb.max.x - hf.p0.x) / dx), 0, (int32)hf.cols - 2);
            int32 r0 = clampi((int32)math::NkFloor((cb.min.z - hf.p0.z) / dz), 0, (int32)hf.rows - 2);
            int32 r1 = clampi((int32)math::NkFloor((cb.max.z - hf.p0.z) / dz), 0, (int32)hf.rows - 2);
            bool any = false; float32 bestDepth = -1.f;
            for (int32 r = r0; r <= r1; ++r) {
                for (int32 c = c0; c <= c1; ++c) {
                    const float32 x0 = hf.p0.x + (float32)c * dx, x1 = x0 + dx;
                    const float32 z0 = hf.p0.z + (float32)r * dz, z1 = z0 + dz;
                    const float32 h00 = hf.p0.y + hf.heights[(uint32)r * hf.cols + (uint32)c];
                    const float32 h10 = hf.p0.y + hf.heights[(uint32)r * hf.cols + (uint32)c + 1];
                    const float32 h01 = hf.p0.y + hf.heights[(uint32)(r + 1) * hf.cols + (uint32)c];
                    const float32 h11 = hf.p0.y + hf.heights[(uint32)(r + 1) * hf.cols + (uint32)c + 1];
                    const NkVec3f P00{ x0, h00, z0 }, P10{ x1, h10, z0 }, P01{ x0, h01, z1 }, P11{ x1, h11, z1 };
                    NkVec3f tris[2][3] = { { P00, P10, P11 }, { P00, P11, P01 } };
                    for (int32 t = 0; t < 2; ++t) {
                        NkAABB3D tb; tb.min = tb.max = tris[t][0]; tb.Expand(tris[t][1]); tb.Expand(tris[t][2]);
                        if (!cb.Overlaps(tb)) continue;
                        NkShape tri = NkShape::Triangle3D(tris[t]);
                        NkManifold3D m;
                        if (NkGJKEPA3D(tri, convex, m) && m.points[0].depth > bestDepth) {
                            bestDepth = m.points[0].depth; out = m; any = true;
                        }
                    }
                }
            }
            return any;
        }

        // ── Convexe vs Chain 2D (polyligne, segments consécutifs) ─────────────
        inline bool NkConvexVsChain2D(const NkShape& chain, const NkShape& convex, NkManifold2D& out) noexcept {
            if (!chain.verts || chain.vertCount < 2) return false;
            const NkAABB2D cb = NkComputeAABB2D(convex);
            bool any = false; float32 bestDepth = -1.f;
            for (uint32 i = 0; i + 1 < chain.vertCount; ++i) {
                const NkVec2f a{ chain.verts[i].x, chain.verts[i].y }, b{ chain.verts[i + 1].x, chain.verts[i + 1].y };
                NkAABB2D sb; sb.min = { math::NkMin(a.x, b.x), math::NkMin(a.y, b.y) }; sb.max = { math::NkMax(a.x, b.x), math::NkMax(a.y, b.y) };
                if (!cb.Overlaps(sb)) continue;
                NkShape seg = NkShape::Segment2D(a, b);
                NkManifold2D m;
                if (NkGJKEPA2D(seg, convex, m) && m.points[0].depth > bestDepth) {
                    bestDepth = m.points[0].depth; out = m; any = true;
                }
            }
            return any;
        }

    } // namespace collision
} // namespace nkentseu
