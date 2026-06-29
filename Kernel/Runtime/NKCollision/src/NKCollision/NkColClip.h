#pragma once
// =============================================================================
// NkColClip.h — Manifolds MULTI-POINTS par clipping (ZÉRO STL). 2D : collision
// polygone/polygone (boîte/polygone/triangle) par SAT + clipping d'arête
// incidente (algorithme façon b2CollidePolygons). Produit jusqu'à 2 points de
// contact (vs 1 seul pour GJK/EPA) — indispensable à la STABILITÉ de la
// résolution physique (empilement de caisses sans basculement parasite).
// Les formes courbes (cercle/capsule) gardent 1 point (correct par nature).
// =============================================================================
#include "NKCollision/NkColShapes.h"

namespace nkentseu {
    namespace collision {

        // Polygone convexe 2D temporaire (CCW) : sommets + normales d'arêtes sortantes.
        struct NkPoly2 { NkVec2f v[8]; NkVec2f n[8]; int32 count = 0; };

        NK_FORCE_INLINE bool NkShapeIsPoly2(NkShapeType t) noexcept {
            return t == NkShapeType::NK_BOX2D || t == NkShapeType::NK_POLYGON2D || t == NkShapeType::NK_TRIANGLE2D;
        }

        // Construit un NkPoly2 (CCW) depuis une forme polygonale 2D.
        inline NkPoly2 NkBuildPoly2(const NkShape& s) noexcept {
            NkPoly2 p;
            if (s.type == NkShapeType::NK_BOX2D) {
                const float32 co = math::NkCos(s.rotation), si = math::NkSin(s.rotation);
                const NkVec2f c{ s.p0.x, s.p0.y }, hx{ co * s.p1.x, si * s.p1.x }, hy{ -si * s.p1.y, co * s.p1.y };
                p.v[0] = c - hx - hy; p.v[1] = c + hx - hy; p.v[2] = c + hx + hy; p.v[3] = c - hx + hy; // CCW
                p.count = 4;
            } else {
                const int32 n = (int32)(s.vertCount < 8 ? s.vertCount : 8);
                for (int32 i = 0; i < n; ++i) p.v[i] = { s.verts[i].x, s.verts[i].y };
                p.count = n;
                // assurer CCW (aire signée > 0)
                float32 area = 0.f;
                for (int32 i = 0; i < n; ++i) { const NkVec2f& a = p.v[i]; const NkVec2f& b = p.v[(i + 1) % n]; area += a.x * b.y - a.y * b.x; }
                if (area < 0.f) for (int32 i = 0; i < n / 2; ++i) { NkVec2f t = p.v[i]; p.v[i] = p.v[n - 1 - i]; p.v[n - 1 - i] = t; }
            }
            for (int32 i = 0; i < p.count; ++i) {              // normales sortantes (CCW)
                const NkVec2f e = p.v[(i + 1) % p.count] - p.v[i];
                NkVec2f nrm{ e.y, -e.x }; const float32 l = math::NkSqrt(nrm.Dot(nrm));
                p.n[i] = (l > 1e-12f) ? nrm * (1.f / l) : NkVec2f{ 0.f, 1.f };
            }
            return p;
        }

        // Séparation max de B par rapport aux arêtes de A (SAT). edgeOut = arête de A.
        inline float32 NkMaxSeparation2(const NkPoly2& A, const NkPoly2& B, int32& edgeOut) noexcept {
            float32 best = -1e30f; edgeOut = 0;
            for (int32 i = 0; i < A.count; ++i) {
                const NkVec2f n = A.n[i], v = A.v[i];
                float32 minDot = 1e30f;
                for (int32 j = 0; j < B.count; ++j) minDot = math::NkMin(minDot, n.Dot(B.v[j] - v));
                if (minDot > best) { best = minDot; edgeOut = i; }
            }
            return best;
        }

        // Clip d'un segment [a,b] par la demi-droite n·x <= offset. Garde <=2 points.
        inline int32 NkClipSeg2(NkVec2f* io, const NkVec2f& n, float32 offset) noexcept {
            NkVec2f out[2]; int32 c = 0;
            const float32 d0 = n.Dot(io[0]) - offset, d1 = n.Dot(io[1]) - offset;
            if (d0 <= 0.f) out[c++] = io[0];
            if (d1 <= 0.f) out[c++] = io[1];
            if (d0 * d1 < 0.f && c < 2) { const float32 t = d0 / (d0 - d1); out[c++] = io[0] + (io[1] - io[0]) * t; }
            io[0] = out[0]; if (c > 1) io[1] = out[1];
            return c;
        }

        // Collision polygone-polygone 2D -> manifold (1 à 2 points), normale A->B.
        inline bool NkCollidePolygons2D(const NkShape& sa, const NkShape& sb, NkManifold2D& m) noexcept {
            const NkPoly2 A = NkBuildPoly2(sa), B = NkBuildPoly2(sb);
            int32 ea, eb;
            const float32 sepA = NkMaxSeparation2(A, B, ea);
            if (sepA > 0.f) return false;
            const float32 sepB = NkMaxSeparation2(B, A, eb);
            if (sepB > 0.f) return false;

            // Référence = polygone à la séparation la plus grande (la moins négative).
            const NkPoly2* ref; const NkPoly2* inc; int32 refEdge; bool flip;
            if (sepB > sepA + 1e-4f) { ref = &B; inc = &A; refEdge = eb; flip = true; }
            else { ref = &A; inc = &B; refEdge = ea; flip = false; }

            const NkVec2f refN = ref->n[refEdge];
            // Arête incidente = la plus anti-parallèle à refN.
            int32 incEdge = 0; float32 minDot = 1e30f;
            for (int32 i = 0; i < inc->count; ++i) { const float32 d = refN.Dot(inc->n[i]); if (d < minDot) { minDot = d; incEdge = i; } }

            NkVec2f seg[2] = { inc->v[incEdge], inc->v[(incEdge + 1) % inc->count] };
            const NkVec2f rv1 = ref->v[refEdge], rv2 = ref->v[(refEdge + 1) % ref->count];
            const NkVec2f tangent = (rv2 - rv1).Normalized();
            // clip par les deux plans latéraux de l'arête de référence
            if (NkClipSeg2(seg, tangent * -1.f, (tangent * -1.f).Dot(rv1)) < 2) return false;
            if (NkClipSeg2(seg, tangent, tangent.Dot(rv2)) < 2) return false;

            const float32 refOffset = refN.Dot(rv1);
            NkVec2f outN = flip ? refN * -1.f : refN;          // normale orientée A->B
            m.count = 0;
            for (int32 i = 0; i < 2; ++i) {
                const float32 depth = refOffset - refN.Dot(seg[i]); // >0 si sous la face de référence
                if (depth >= -1e-4f) {
                    m.points[m.count].point = seg[i];
                    m.points[m.count].depth = depth > 0.f ? depth : 0.f;
                    ++m.count;
                }
            }
            if (m.count == 0) return false;
            m.normal = outN;
            return true;
        }

    } // namespace collision
} // namespace nkentseu
