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
#include "NKCollision/NkColGJK.h"   // EPA fournit la normale robuste, le clipping les points

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
                    m.points[m.count].id = (uint32)((refEdge << 8) | (incEdge << 4) | i) | (flip ? 0x10000u : 0u);
                    ++m.count;
                }
            }
            if (m.count == 0) return false;
            m.normal = outN;
            return true;
        }

        // =====================================================================
        //  Manifold MULTI-POINTS 3D — box-box (OBB) : EPA (normale) + clipping de face
        // =====================================================================
        struct NkBoxFrame { NkVec3f c; NkVec3f u[3]; float32 e[3]; };
        inline NkBoxFrame NkBoxFrameOf(const NkShape& s) noexcept {
            NkBoxFrame f; f.c = s.p0;
            f.u[0] = s.orientation * NkVec3f{1,0,0}; f.u[1] = s.orientation * NkVec3f{0,1,0}; f.u[2] = s.orientation * NkVec3f{0,0,1};
            f.e[0] = s.p1.x; f.e[1] = s.p1.y; f.e[2] = s.p1.z; return f;
        }
        // Clip un polygone (<=8 pts) par le demi-espace n·x <= offset (Sutherland-Hodgman 3D).
        inline int32 NkClipPolyPlane3(NkVec3f* poly, int32 count, const NkVec3f& n, float32 offset) noexcept {
            NkVec3f out[8]; int32 c = 0;
            for (int32 i = 0; i < count && c < 8; ++i) {
                const NkVec3f& A = poly[i]; const NkVec3f& B = poly[(i + 1) % count];
                const float32 da = n.Dot(A) - offset, db = n.Dot(B) - offset;
                if (da <= 0.f) out[c++] = A;
                if (da * db < 0.f && c < 8) { const float32 t = da / (da - db); out[c++] = A + (B - A) * t; }
            }
            for (int32 i = 0; i < c; ++i) poly[i] = out[i];
            return c;
        }

        // box-box (OBB) -> manifold (1 à 4 points). normale A->B (issue d'EPA).
        inline bool NkCollideBoxBox3D(const NkShape& sa, const NkShape& sb, NkManifold3D& m) noexcept {
            NkManifold3D epa;
            if (!NkGJKEPA3D(sa, sb, epa)) return false;          // normale + 1 point (repli)
            const NkVec3f n = epa.normal;                        // A->B
            const NkBoxFrame A = NkBoxFrameOf(sa), B = NkBoxFrameOf(sb);

            // Référence = boîte dont une face est la plus parallèle à la normale.
            int32 ai = 0; float32 bestA = -1.f; for (int32 i = 0; i < 3; ++i) { float32 d = math::NkAbs(A.u[i].Dot(n)); if (d > bestA) { bestA = d; ai = i; } }
            int32 bi = 0; float32 bestB = -1.f; for (int32 i = 0; i < 3; ++i) { float32 d = math::NkAbs(B.u[i].Dot(n)); if (d > bestB) { bestB = d; bi = i; } }

            const NkBoxFrame* ref; const NkBoxFrame* inc; int32 refAxis; NkVec3f rn;
            if (bestA >= bestB) { ref = &A; inc = &B; refAxis = ai; rn = A.u[ai] * (A.u[ai].Dot(n) < 0.f ? -1.f : 1.f); } // rn ~ +n (A->B)
            else                { ref = &B; inc = &A; refAxis = bi; rn = B.u[bi] * (B.u[bi].Dot(n) < 0.f ? 1.f : -1.f); } // rn ~ -n (B->A)

            // Face de référence : centre + rn*extent ; 2 axes tangents.
            const int32 t1 = (refAxis + 1) % 3, t2 = (refAxis + 2) % 3;
            const NkVec3f refC = ref->c + rn * ref->e[refAxis];
            const NkVec3f T1 = ref->u[t1], T2 = ref->u[t2];
            const float32 e1 = ref->e[t1], e2 = ref->e[t2];

            // Face incidente : face de `inc` la plus anti-parallèle à rn.
            int32 ii = 0; float32 worst = 1e30f; float32 incSign = 1.f;
            for (int32 i = 0; i < 3; ++i) {
                const float32 dp = inc->u[i].Dot(rn);
                if (dp < worst)  { worst = dp;  ii = i; incSign =  1.f; }
                if (-dp < worst) { worst = -dp; ii = i; incSign = -1.f; }
            }
            const int32 it1 = (ii + 1) % 3, it2 = (ii + 2) % 3;
            const NkVec3f incC = inc->c + inc->u[ii] * (incSign * inc->e[ii]);
            NkVec3f poly[8]; int32 pc = 4;
            poly[0] = incC + inc->u[it1] * inc->e[it1] + inc->u[it2] * inc->e[it2];
            poly[1] = incC - inc->u[it1] * inc->e[it1] + inc->u[it2] * inc->e[it2];
            poly[2] = incC - inc->u[it1] * inc->e[it1] - inc->u[it2] * inc->e[it2];
            poly[3] = incC + inc->u[it1] * inc->e[it1] - inc->u[it2] * inc->e[it2];

            // Clip par les 4 plans latéraux de la face de référence.
            pc = NkClipPolyPlane3(poly, pc,  T1,  T1.Dot(refC) + e1);
            pc = NkClipPolyPlane3(poly, pc,  T1 * -1.f, (T1 * -1.f).Dot(refC) + e1);
            pc = NkClipPolyPlane3(poly, pc,  T2,  T2.Dot(refC) + e2);
            pc = NkClipPolyPlane3(poly, pc,  T2 * -1.f, (T2 * -1.f).Dot(refC) + e2);
            if (pc == 0) { m = epa; return true; }               // repli : point EPA

            // Garder les points pénétrants (sous la face de réf), <= 4, les plus profonds.
            const float32 refOffset = rn.Dot(refC);
            m.normal = n; m.count = 0;
            for (int32 i = 0; i < pc && m.count < 4; ++i) {
                const float32 depth = refOffset - rn.Dot(poly[i]);
                if (depth >= -1e-3f) {
                    m.points[m.count].point = poly[i];
                    m.points[m.count].depth = depth > 0.f ? depth : 0.f;
                    m.points[m.count].id = (uint32)((refAxis << 8) | (ii << 4) | i);
                    ++m.count;
                }
            }
            if (m.count == 0) { m = epa; }
            return true;
        }

    } // namespace collision
} // namespace nkentseu
