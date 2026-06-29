#pragma once
// =============================================================================
// NkColGJK.h — Narrowphase GÉNÉRIQUE convexe-convexe (ZÉRO STL) : GJK (distance
// + détection) + EPA (pénétration profonde) -> manifold. 2D et 3D.
//
// Principe « type PhysX » : toute forme convexe se réduit à une FONCTION DE
// SUPPORT. GJK/EPA ne manipule que des supports -> ajouter un type convexe =
// écrire son support.
//
// Modèle CŒUR + MARGE (robuste, cf. Bullet) : les formes arrondies (sphère/
// capsule/cercle) ont un cœur (point/segment) + une marge (rayon). GJK calcule
// la DISTANCE entre les CŒURS :
//   • cœurs séparés d'une distance d : contact si d < margeA+margeB
//     (profondeur = (mA+mB) - d, normale = direction des points les + proches) ;
//   • cœurs qui se pénètrent : EPA sur les cœurs -> profondeur, + (mA+mB).
// Faire tourner l'EPA sur des formes gonflées par la marge donnerait un polytope
// quasi-dégénéré (capsule = segment) -> normales fausses. D'où la distance.
//
// Sortie : 1 point de contact (clipping multi-points = vague suivante).
// =============================================================================
#include "NKCollision/NkColShapes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace collision {

        // ── Marges (rayon des formes arrondies) ──────────────────────────────
        NK_FORCE_INLINE float32 NkShapeMargin(const NkShape& s) noexcept {
            switch (s.type) {
                case NkShapeType::NK_SPHERE:   case NkShapeType::NK_CAPSULE3D:
                case NkShapeType::NK_CIRCLE2D: case NkShapeType::NK_CAPSULE2D:
                    return s.radius;
                default: return 0.f;
            }
        }

        // ── Fonctions de support — CŒUR 3D (sans marge), monde ───────────────
        inline NkVec3f NkSupportCore3D(const NkShape& s, const NkVec3f& d) noexcept {
            switch (s.type) {
                case NkShapeType::NK_SPHERE: return s.p0;
                case NkShapeType::NK_CAPSULE3D:
                    return (d.Dot(s.p0) >= d.Dot(s.p1)) ? s.p0 : s.p1;
                case NkShapeType::NK_BOX3D: {
                    // OBB : direction -> repère local, support local, -> monde (identité = AABB).
                    NkVec3f ld = s.orientation.Conjugate() * d;
                    NkVec3f ls{ ld.x < 0.f ? -s.p1.x : s.p1.x, ld.y < 0.f ? -s.p1.y : s.p1.y, ld.z < 0.f ? -s.p1.z : s.p1.z };
                    return s.p0 + s.orientation * ls;
                }
                case NkShapeType::NK_CYLINDER3D: {
                    const NkVec3f& a = s.p1;
                    const float32 t = d.Dot(a);
                    NkVec3f cap = s.p0 + a * (t >= 0.f ? s.height : -s.height);
                    NkVec3f radial = d - a * t;
                    const float32 rl2 = radial.Dot(radial);
                    if (rl2 > 1e-12f) cap = cap + radial * (s.radius / math::NkSqrt(rl2));
                    return cap;
                }
                case NkShapeType::NK_CONE3D: {
                    const NkVec3f& a = s.p1;
                    NkVec3f apex = s.p0 + a * s.height;
                    NkVec3f radial = d - a * d.Dot(a);
                    const float32 rl2 = radial.Dot(radial);
                    NkVec3f rim = s.p0;
                    if (rl2 > 1e-12f) rim = s.p0 + radial * (s.radius / math::NkSqrt(rl2));
                    return (d.Dot(apex) > d.Dot(rim)) ? apex : rim;
                }
                case NkShapeType::NK_TRIANGLE3D:
                case NkShapeType::NK_CONVEX3D: {
                    if (!s.verts || !s.vertCount) return s.p0;
                    uint32 best = 0; float32 bd = d.Dot(s.verts[0]);
                    for (uint32 i = 1; i < s.vertCount; ++i) {
                        const float32 dot = d.Dot(s.verts[i]);
                        if (dot > bd) { bd = dot; best = i; }
                    }
                    return s.verts[best];
                }
                default: return s.p0;
            }
        }
        // Support complet 3D = cœur + marge*d̂ (pour le test plan, hors GJK).
        inline NkVec3f NkSupport3D(const NkShape& s, const NkVec3f& d) noexcept {
            NkVec3f p = NkSupportCore3D(s, d);
            const float32 m = NkShapeMargin(s);
            if (m > 0.f) { const float32 l2 = d.Dot(d); if (l2 > 1e-12f) p = p + d * (m / math::NkSqrt(l2)); }
            return p;
        }

        // ── Fonctions de support — CŒUR 2D (sans marge) ──────────────────────
        inline NkVec2f NkSupportCore2D(const NkShape& s, const NkVec2f& d) noexcept {
            const NkVec2f c{ s.p0.x, s.p0.y };
            switch (s.type) {
                case NkShapeType::NK_POINT2D:
                case NkShapeType::NK_CIRCLE2D: return c;
                case NkShapeType::NK_SEGMENT2D:
                case NkShapeType::NK_CAPSULE2D: {
                    const NkVec2f a{ s.p0.x, s.p0.y }, b{ s.p1.x, s.p1.y };
                    return (d.Dot(a) >= d.Dot(b)) ? a : b;
                }
                case NkShapeType::NK_BOX2D: {
                    const float32 co = math::NkCos(s.rotation), si = math::NkSin(s.rotation);
                    const NkVec2f ux{ co, si }, uy{ -si, co };
                    return c + ux * (d.Dot(ux) < 0.f ? -s.p1.x : s.p1.x)
                             + uy * (d.Dot(uy) < 0.f ? -s.p1.y : s.p1.y);
                }
                case NkShapeType::NK_TRIANGLE2D:
                case NkShapeType::NK_POLYGON2D: {
                    if (!s.verts || !s.vertCount) return c;
                    uint32 best = 0; NkVec2f v0{ s.verts[0].x, s.verts[0].y }; float32 bd = d.Dot(v0);
                    for (uint32 i = 1; i < s.vertCount; ++i) {
                        const NkVec2f vi{ s.verts[i].x, s.verts[i].y };
                        const float32 dot = d.Dot(vi);
                        if (dot > bd) { bd = dot; best = i; }
                    }
                    return { s.verts[best].x, s.verts[best].y };
                }
                default: return c;
            }
        }

        // ── Centre représentatif (oriente la normale A->B, comme le reste du module) ──
        inline NkVec3f NkShapeCenter3D(const NkShape& s) noexcept {
            if (s.type == NkShapeType::NK_CAPSULE3D) return (s.p0 + s.p1) * 0.5f;
            if (s.type == NkShapeType::NK_CONE3D)    return s.p0 + s.p1 * (s.height * 0.5f);
            if ((s.type == NkShapeType::NK_TRIANGLE3D || s.type == NkShapeType::NK_CONVEX3D) && s.verts && s.vertCount) {
                NkVec3f sum{}; for (uint32 i = 0; i < s.vertCount; ++i) sum = sum + s.verts[i];
                return sum * (1.f / (float32)s.vertCount);
            }
            return s.p0;
        }
        inline NkVec2f NkShapeCenter2D(const NkShape& s) noexcept {
            if (s.type == NkShapeType::NK_SEGMENT2D || s.type == NkShapeType::NK_CAPSULE2D)
                return { (s.p0.x + s.p1.x) * 0.5f, (s.p0.y + s.p1.y) * 0.5f };
            if ((s.type == NkShapeType::NK_TRIANGLE2D || s.type == NkShapeType::NK_POLYGON2D) && s.verts && s.vertCount) {
                NkVec2f sum{}; for (uint32 i = 0; i < s.vertCount; ++i) sum = sum + NkVec2f{ s.verts[i].x, s.verts[i].y };
                return sum * (1.f / (float32)s.vertCount);
            }
            return { s.p0.x, s.p0.y };
        }

        // =====================================================================
        //  GJK distance + EPA — 3D (sur les CŒURS)
        // =====================================================================
        namespace detail {

            struct NkSV3 { NkVec3f v;  NkVec3f a; }; // point Minkowski cœur (A-B) + témoin sur A

            NK_FORCE_INLINE NkSV3 NkMinkCore3D(const NkShape& A, const NkShape& B, const NkVec3f& d) noexcept {
                NkVec3f sa = NkSupportCore3D(A, d);
                NkVec3f sb = NkSupportCore3D(B, d * -1.f);
                return { sa - sb, sa };
            }

            // Point le plus proche de l'origine sur un triangle (Ericson), + barycentriques.
            inline NkVec3f NkClosestTri3D(const NkVec3f& a, const NkVec3f& b, const NkVec3f& c,
                                          float32& u, float32& v, float32& w) noexcept {
                NkVec3f ab = b - a, ac = c - a, ap = a * -1.f;
                float32 d1 = ab.Dot(ap), d2 = ac.Dot(ap);
                if (d1 <= 0.f && d2 <= 0.f) { u = 1.f; v = 0.f; w = 0.f; return a; }
                NkVec3f bp = b * -1.f; float32 d3 = ab.Dot(bp), d4 = ac.Dot(bp);
                if (d3 >= 0.f && d4 <= d3) { u = 0.f; v = 1.f; w = 0.f; return b; }
                float32 vc = d1 * d4 - d3 * d2;
                if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) { float32 t = d1 / (d1 - d3); u = 1.f - t; v = t; w = 0.f; return a + ab * t; }
                NkVec3f cp = c * -1.f; float32 d5 = ab.Dot(cp), d6 = ac.Dot(cp);
                if (d6 >= 0.f && d5 <= d6) { u = 0.f; v = 0.f; w = 1.f; return c; }
                float32 vb = d5 * d2 - d1 * d6;
                if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) { float32 t = d2 / (d2 - d6); u = 1.f - t; v = 0.f; w = t; return a + ac * t; }
                float32 va = d3 * d6 - d5 * d4;
                if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) { float32 t = (d4 - d3) / ((d4 - d3) + (d5 - d6)); u = 0.f; v = 1.f - t; w = t; return b + (c - b) * t; }
                float32 den = 1.f / (va + vb + vc); float32 vv = vb * den, ww = vc * den; u = 1.f - vv - ww; v = vv; w = ww;
                return a + ab * vv + ac * ww;
            }

            // Plus proche de l'origine sur le simplexe (1..4). Réduit s[]/n au sous-simplexe
            // support, remplit le témoin A (wA). Si origine ENCLOSE (tétra) -> intersect=true.
            inline NkVec3f NkClosestSimplex3D(NkSV3* s, int32& n, NkVec3f& wA, bool& intersect) noexcept {
                intersect = false;
                if (n == 1) { wA = s[0].a; return s[0].v; }
                if (n == 2) {
                    NkVec3f a = s[0].v, b = s[1].v, ab = b - a;
                    float32 den = ab.Dot(ab);
                    float32 t = (den > 1e-12f) ? math::NkClamp((a * -1.f).Dot(ab) / den, 0.f, 1.f) : 0.f;
                    if (t <= 0.f) { n = 1; wA = s[0].a; return a; }
                    if (t >= 1.f) { s[0] = s[1]; n = 1; wA = s[0].a; return b; }
                    wA = s[0].a * (1.f - t) + s[1].a * t; return a + ab * t;
                }
                if (n == 3) {
                    float32 u, v, w; NkVec3f c = NkClosestTri3D(s[0].v, s[1].v, s[2].v, u, v, w);
                    wA = s[0].a * u + s[1].a * v + s[2].a * w;
                    NkSV3 k[3]; int32 cnt = 0;
                    if (u > 1e-7f) k[cnt++] = s[0]; if (v > 1e-7f) k[cnt++] = s[1]; if (w > 1e-7f) k[cnt++] = s[2];
                    for (int32 i = 0; i < cnt; ++i) s[i] = k[i]; if (cnt) n = cnt;
                    return c;
                }
                // n == 4 : tétraèdre. Faces + sommet opposé.
                const int32 F[4][4] = { {0,1,2,3}, {0,1,3,2}, {0,2,3,1}, {1,2,3,0} };
                float32 bestD2 = 1e30f; NkVec3f bestC{}; int32 bi = -1; float32 bu = 0, bv = 0, bw = 0;
                bool anyOutside = false;
                for (int32 f = 0; f < 4; ++f) {
                    NkVec3f A = s[F[f][0]].v, B = s[F[f][1]].v, C = s[F[f][2]].v, D = s[F[f][3]].v;
                    NkVec3f nrm = (B - A).Cross(C - A);
                    float32 refSide = nrm.Dot(D - A);
                    float32 oriSide = nrm.Dot(A * -1.f);
                    if (refSide * oriSide < 0.f) { // origine du côté opposé à D -> dehors
                        anyOutside = true;
                        float32 u, v, w; NkVec3f c = NkClosestTri3D(A, B, C, u, v, w);
                        float32 d2 = c.Dot(c);
                        if (d2 < bestD2) { bestD2 = d2; bestC = c; bi = f; bu = u; bv = v; bw = w; }
                    }
                }
                if (!anyOutside) { intersect = true; wA = s[0].a; return NkVec3f{ 0.f, 0.f, 0.f }; }
                NkSV3 a0 = s[F[bi][0]], a1 = s[F[bi][1]], a2 = s[F[bi][2]];
                wA = a0.a * bu + a1.a * bv + a2.a * bw;
                NkSV3 k[3]; int32 cnt = 0;
                if (bu > 1e-7f) k[cnt++] = a0; if (bv > 1e-7f) k[cnt++] = a1; if (bw > 1e-7f) k[cnt++] = a2;
                for (int32 i = 0; i < cnt; ++i) s[i] = k[i]; n = cnt ? cnt : 3;
                return bestC;
            }

            // a x b x c = b(a·c) - a(b·c)
            NK_FORCE_INLINE NkVec3f NkTriple3(const NkVec3f& a, const NkVec3f& b, const NkVec3f& c) noexcept {
                return b * a.Dot(c) - a * b.Dot(c);
            }
            // doSimplex booléen : avance vers l'origine, true si tétraèdre l'englobe.
            inline bool NkDoSimplex3D(NkSV3* s, int32& n, NkVec3f& dir) noexcept {
                const NkVec3f O{ 0.f, 0.f, 0.f };
                if (n == 2) {
                    NkVec3f a = s[1].v, b = s[0].v, ab = b - a, ao = O - a;
                    if (ab.Dot(ao) > 0.f) { dir = NkTriple3(ab, ao, ab); if (dir.Dot(dir) < 1e-12f) { dir = NkVec3f{ -ab.y, ab.x, 0.f }; if (dir.Dot(dir) < 1e-12f) dir = NkVec3f{ 0.f, -ab.z, ab.y }; } }
                    else { s[0] = s[1]; n = 1; dir = ao; }
                    return false;
                }
                if (n == 3) {
                    NkVec3f a = s[2].v, b = s[1].v, c = s[0].v, ab = b - a, ac = c - a, ao = O - a;
                    NkVec3f abc = ab.Cross(ac);
                    if (abc.Cross(ac).Dot(ao) > 0.f) {
                        if (ac.Dot(ao) > 0.f) { s[1] = s[2]; n = 2; dir = NkTriple3(ac, ao, ac); }
                        else { s[0] = s[1]; s[1] = s[2]; n = 2; return NkDoSimplex3D(s, n, dir); }
                    } else if (ab.Cross(abc).Dot(ao) > 0.f) {
                        s[0] = s[1]; s[1] = s[2]; n = 2; return NkDoSimplex3D(s, n, dir);
                    } else {
                        if (abc.Dot(ao) > 0.f) dir = abc;
                        else { NkSV3 t = s[0]; s[0] = s[1]; s[1] = t; dir = abc * -1.f; }
                        return false;
                    }
                    return false;
                }
                NkVec3f a = s[3].v, b = s[2].v, c = s[1].v, d = s[0].v, ao = O - a;
                NkVec3f abc = (b - a).Cross(c - a), acd = (c - a).Cross(d - a), adb = (d - a).Cross(b - a);
                if (abc.Dot(ao) > 0.f) { s[0] = s[1]; s[1] = s[2]; s[2] = s[3]; n = 3; dir = abc; return false; }
                if (acd.Dot(ao) > 0.f) { s[2] = s[3]; n = 3; dir = acd; return false; }
                if (adb.Dot(ao) > 0.f) { s[1] = s[0]; s[0] = s[2]; s[2] = s[3]; n = 3; dir = adb; return false; }
                return true;
            }
            // GJK booléen sur les cœurs -> remplit un simplexe (tétra si possible).
            inline bool NkGJKBool3D(const NkShape& A, const NkShape& B, NkSV3* s, int32& n) noexcept {
                NkVec3f dir = NkShapeCenter3D(B) - NkShapeCenter3D(A);
                if (dir.Dot(dir) < 1e-12f) dir = NkVec3f{ 1.f, 0.f, 0.f };
                s[0] = NkMinkCore3D(A, B, dir); n = 1; dir = s[0].v * -1.f;
                for (int32 iter = 0; iter < 64; ++iter) {
                    if (dir.Dot(dir) < 1e-12f) return true;
                    NkSV3 p = NkMinkCore3D(A, B, dir);
                    if (p.v.Dot(dir) < 0.f) return false;
                    s[n++] = p;
                    if (NkDoSimplex3D(s, n, dir)) return true;
                }
                return false;
            }

            struct NkGJKRes3 { bool intersect; float32 dist; NkVec3f closest; NkVec3f witnessA; NkSV3 simplex[4]; int32 n; };

            inline NkGJKRes3 NkGJKDist3D(const NkShape& A, const NkShape& B) noexcept {
                NkGJKRes3 r; r.intersect = false; r.dist = 0.f; r.n = 0;
                NkVec3f dir = NkShapeCenter3D(B) - NkShapeCenter3D(A);
                if (dir.Dot(dir) < 1e-12f) dir = NkVec3f{ 1.f, 0.f, 0.f };
                NkSV3 s[4]; int32 n = 0;
                s[0] = NkMinkCore3D(A, B, dir); n = 1;
                NkVec3f closest = s[0].v; r.witnessA = s[0].a;
                for (int32 iter = 0; iter < 64; ++iter) {
                    if (closest.Dot(closest) < 1e-10f) { r.intersect = true; break; }
                    dir = closest * -1.f;
                    NkSV3 w = NkMinkCore3D(A, B, dir);
                    // convergence : pas de point plus proche dans la direction -closest
                    if (closest.Dot(closest) - closest.Dot(w.v) <= 1e-8f * closest.Dot(closest)) break;
                    s[n++] = w;
                    bool inter = false; NkVec3f wA;
                    closest = NkClosestSimplex3D(s, n, wA, inter);
                    r.witnessA = wA;
                    if (inter) { r.intersect = true; break; }
                }
                r.dist = math::NkSqrt(closest.Dot(closest)); r.closest = closest;
                r.n = n; for (int32 i = 0; i < n; ++i) r.simplex[i] = s[i];
                return r;
            }

            struct NkFace { int32 a, b, c; NkVec3f n; float32 dist; };

            // EPA sur les CŒURS (cœurs en pénétration). Requiert un tétraèdre.
            inline bool NkEPACore3D(const NkShape& A, const NkShape& B, NkSV3* simplex, int32 n,
                                    NkVec3f& normalOut, float32& depthOut, NkVec3f& witnessAOut) noexcept {
                if (n < 4) return false;
                NkVector<NkVec3f> verts; NkVector<NkVec3f> wits; NkVector<NkFace> faces;
                for (int32 i = 0; i < 4; ++i) { verts.PushBack(simplex[i].v); wits.PushBack(simplex[i].a); }
                auto addFace = [&](int32 ia, int32 ib, int32 ic) {
                    NkFace f; f.a = ia; f.b = ib; f.c = ic;
                    NkVec3f nn = (verts[ib] - verts[ia]).Cross(verts[ic] - verts[ia]);
                    float32 l = math::NkSqrt(nn.Dot(nn));
                    f.n = (l > 1e-12f) ? nn * (1.f / l) : NkVec3f{ 0.f, 0.f, 1.f };
                    f.dist = f.n.Dot(verts[ia]);
                    if (f.dist < 0.f) { f.n = f.n * -1.f; f.dist = -f.dist; int32 t = f.b; f.b = f.c; f.c = t; }
                    faces.PushBack(f);
                };
                addFace(0, 1, 2); addFace(0, 2, 3); addFace(0, 3, 1); addFace(1, 3, 2);
                NkFace best{}; best.dist = 0.f;
                for (int32 iter = 0; iter < 48; ++iter) {
                    int32 bi = 0; float32 bd = 1e30f;
                    for (uint32 i = 0; i < faces.Size(); ++i) if (faces[i].dist < bd) { bd = faces[i].dist; bi = (int32)i; }
                    best = faces[(uint32)bi];
                    NkSV3 p = NkMinkCore3D(A, B, best.n);
                    if (p.v.Dot(best.n) - best.dist < 1e-4f) break;
                    const int32 ip = (int32)verts.Size(); verts.PushBack(p.v); wits.PushBack(p.a);
                    NkVector<int32> edges;
                    for (uint32 i = 0; i < faces.Size();) {
                        const NkFace& f = faces[i];
                        if (f.n.Dot(p.v - verts[(uint32)f.a]) > 1e-6f) {
                            const int32 e[6] = { f.a, f.b, f.b, f.c, f.c, f.a };
                            for (int32 k = 0; k < 3; ++k) {
                                const int32 e0 = e[k * 2], e1 = e[k * 2 + 1]; bool found = false;
                                for (uint32 m = 0; m < edges.Size(); m += 2)
                                    if (edges[m] == e1 && edges[m + 1] == e0) {
                                        edges[m] = edges[edges.Size() - 2]; edges[m + 1] = edges[edges.Size() - 1];
                                        edges.PopBack(); edges.PopBack(); found = true; break;
                                    }
                                if (!found) { edges.PushBack(e0); edges.PushBack(e1); }
                            }
                            faces[i] = faces[faces.Size() - 1]; faces.PopBack();
                        } else ++i;
                    }
                    if (edges.Size() == 0) break;
                    for (uint32 m = 0; m < edges.Size(); m += 2) addFace(edges[m], edges[m + 1], ip);
                    if (faces.Size() == 0) break;
                }
                NkVec3f proj = best.n * best.dist;
                const NkVec3f& A0 = verts[(uint32)best.a]; const NkVec3f& B0 = verts[(uint32)best.b]; const NkVec3f& C0 = verts[(uint32)best.c];
                NkVec3f v0 = B0 - A0, v1 = C0 - A0, v2 = proj - A0;
                const float32 d00 = v0.Dot(v0), d01 = v0.Dot(v1), d11 = v1.Dot(v1), d20 = v2.Dot(v0), d21 = v2.Dot(v1);
                const float32 den = d00 * d11 - d01 * d01;
                float32 u = 1.f, vv = 0.f, w = 0.f;
                if (math::NkAbs(den) > 1e-12f) { vv = (d11 * d20 - d01 * d21) / den; w = (d00 * d21 - d01 * d20) / den; u = 1.f - vv - w; }
                witnessAOut = wits[(uint32)best.a] * u + wits[(uint32)best.b] * vv + wits[(uint32)best.c] * w;
                normalOut = best.n; depthOut = best.dist; return true;
            }

        } // namespace detail

        // Narrowphase générique 3D convexe-convexe (cœur+marge). normale A->B.
        inline bool NkGJKEPA3D(const NkShape& A, const NkShape& B, NkManifold3D& m) noexcept {
            using namespace detail;
            const float32 mA = NkShapeMargin(A), mB = NkShapeMargin(B), tot = mA + mB;
            NkVec3f normal, witnessA; float32 depth = 0.f;
            NkSV3 bsimplex[4]; int32 bn = 0;
            if (NkGJKBool3D(A, B, bsimplex, bn)) {
                // cœurs en pénétration -> EPA (tétraèdre englobant requis)
                if (!NkEPACore3D(A, B, bsimplex, bn, normal, depth, witnessA)) {
                    NkVec3f cc = NkShapeCenter3D(B) - NkShapeCenter3D(A); float32 l = math::NkSqrt(cc.Dot(cc));
                    normal = (l > 1e-6f) ? cc * (1.f / l) : NkVec3f{ 0,1,0 }; depth = 1e-4f; witnessA = NkShapeCenter3D(A);
                }
                depth += tot;
            } else {
                // cœurs séparés -> distance ; contact si d < mA+mB.
                NkGJKRes3 r = NkGJKDist3D(A, B);
                if (r.dist > tot + 1e-6f) return false;
                if (r.dist > 1e-6f) normal = r.closest * (-1.f / r.dist);
                else { NkVec3f cc = NkShapeCenter3D(B) - NkShapeCenter3D(A); float32 l = math::NkSqrt(cc.Dot(cc)); normal = (l > 1e-6f) ? cc * (1.f / l) : NkVec3f{ 0,1,0 }; }
                depth = tot - r.dist; witnessA = r.witnessA;
            }
            NkVec3f cc = NkShapeCenter3D(B) - NkShapeCenter3D(A);
            if (normal.Dot(cc) < 0.f) normal = normal * -1.f;        // cohérent A->B
            m.normal = normal;
            m.points[0].point = witnessA + normal * (mA - depth * 0.5f);
            m.points[0].depth = depth; m.count = 1;
            return true;
        }

        // =====================================================================
        //  GJK distance + EPA — 2D (sur les CŒURS)
        // =====================================================================
        namespace detail {

            struct NkSV2 { NkVec2f v; NkVec2f a; };

            NK_FORCE_INLINE NkSV2 NkMinkCore2D(const NkShape& A, const NkShape& B, const NkVec2f& d) noexcept {
                NkVec2f sa = NkSupportCore2D(A, d);
                NkVec2f sb = NkSupportCore2D(B, d * -1.f);
                return { sa - sb, sa };
            }
            NK_FORCE_INLINE float32 NkCross2(const NkVec2f& a, const NkVec2f& b) noexcept { return a.x * b.y - a.y * b.x; }

            inline NkVec2f NkClosestSimplex2D(NkSV2* s, int32& n, NkVec2f& wA, bool& intersect) noexcept {
                intersect = false;
                if (n == 1) { wA = s[0].a; return s[0].v; }
                if (n == 2) {
                    NkVec2f a = s[0].v, b = s[1].v, ab = b - a; float32 den = ab.Dot(ab);
                    float32 t = (den > 1e-12f) ? math::NkClamp((a * -1.f).Dot(ab) / den, 0.f, 1.f) : 0.f;
                    if (t <= 0.f) { n = 1; wA = s[0].a; return a; }
                    if (t >= 1.f) { s[0] = s[1]; n = 1; wA = s[0].a; return b; }
                    wA = s[0].a * (1.f - t) + s[1].a * t; return a + ab * t;
                }
                // n == 3 : triangle. Origine dedans ? sinon plus proche arête.
                NkVec2f a = s[0].v, b = s[1].v, c = s[2].v;
                float32 ab_ = NkCross2(b - a, a * -1.f);
                float32 bc_ = NkCross2(c - b, b * -1.f);
                float32 ca_ = NkCross2(a - c, c * -1.f);
                if ((ab_ >= 0.f && bc_ >= 0.f && ca_ >= 0.f) || (ab_ <= 0.f && bc_ <= 0.f && ca_ <= 0.f)) {
                    intersect = true; wA = s[0].a; return NkVec2f{ 0.f, 0.f };
                }
                // plus proche parmi les 3 arêtes
                float32 bestD2 = 1e30f; NkVec2f bestC{}; int32 i0 = 0, i1 = 1; float32 bt = 0.f;
                const int32 E[3][2] = { {0,1}, {1,2}, {2,0} };
                for (int32 e = 0; e < 3; ++e) {
                    NkVec2f p = s[E[e][0]].v, q = s[E[e][1]].v, pq = q - p; float32 den = pq.Dot(pq);
                    float32 t = (den > 1e-12f) ? math::NkClamp((p * -1.f).Dot(pq) / den, 0.f, 1.f) : 0.f;
                    NkVec2f cpt = p + pq * t; float32 d2 = cpt.Dot(cpt);
                    if (d2 < bestD2) { bestD2 = d2; bestC = cpt; i0 = E[e][0]; i1 = E[e][1]; bt = t; }
                }
                NkSV2 e0 = s[i0], e1 = s[i1];
                wA = e0.a * (1.f - bt) + e1.a * bt;
                s[0] = e0; s[1] = e1; n = 2;
                return bestC;
            }

            NK_FORCE_INLINE NkVec2f NkTriple2(const NkVec2f& a, const NkVec2f& b, const NkVec2f& c) noexcept {
                return b * a.Dot(c) - a * b.Dot(c);
            }
            // GJK booléen 2D sur les cœurs -> simplexe (triangle si possible).
            inline bool NkGJKBool2D(const NkShape& A, const NkShape& B, NkSV2* s, int32& n) noexcept {
                const NkVec2f O{ 0.f, 0.f };
                NkVec2f dir = NkShapeCenter2D(B) - NkShapeCenter2D(A);
                if (dir.Dot(dir) < 1e-12f) dir = NkVec2f{ 1.f, 0.f };
                s[0] = NkMinkCore2D(A, B, dir); n = 1; dir = s[0].v * -1.f;
                for (int32 iter = 0; iter < 64; ++iter) {
                    if (dir.Dot(dir) < 1e-12f) return true;
                    NkSV2 p = NkMinkCore2D(A, B, dir);
                    if (p.v.Dot(dir) < 0.f) return false;
                    s[n++] = p;
                    if (n == 2) {
                        NkVec2f a = s[1].v, b = s[0].v, ab = b - a, ao = O - a;
                        dir = NkTriple2(ab, ao, ab);
                        if (dir.Dot(dir) < 1e-12f) dir = NkVec2f{ -ab.y, ab.x };
                    } else {
                        NkVec2f a = s[2].v, b = s[1].v, c = s[0].v, ab = b - a, ac = c - a, ao = O - a;
                        NkVec2f abPerp = NkTriple2(ac, ab, ab), acPerp = NkTriple2(ab, ac, ac);
                        if (abPerp.Dot(ao) > 0.f) { s[0] = s[1]; s[1] = s[2]; n = 2; dir = abPerp; }
                        else if (acPerp.Dot(ao) > 0.f) { s[1] = s[2]; n = 2; dir = acPerp; }
                        else return true;
                    }
                }
                return false;
            }

            struct NkGJKRes2 { bool intersect; float32 dist; NkVec2f closest; NkVec2f witnessA; NkSV2 simplex[3]; int32 n; };

            inline NkGJKRes2 NkGJKDist2D(const NkShape& A, const NkShape& B) noexcept {
                NkGJKRes2 r; r.intersect = false; r.dist = 0.f; r.n = 0;
                NkVec2f dir = NkShapeCenter2D(B) - NkShapeCenter2D(A);
                if (dir.Dot(dir) < 1e-12f) dir = NkVec2f{ 1.f, 0.f };
                NkSV2 s[3]; int32 n = 0;
                s[0] = NkMinkCore2D(A, B, dir); n = 1;
                NkVec2f closest = s[0].v; r.witnessA = s[0].a;
                for (int32 iter = 0; iter < 64; ++iter) {
                    if (closest.Dot(closest) < 1e-10f) { r.intersect = true; break; }
                    dir = closest * -1.f;
                    NkSV2 w = NkMinkCore2D(A, B, dir);
                    if (closest.Dot(closest) - closest.Dot(w.v) <= 1e-8f * closest.Dot(closest)) break;
                    s[n++] = w;
                    bool inter = false; NkVec2f wA;
                    closest = NkClosestSimplex2D(s, n, wA, inter); r.witnessA = wA;
                    if (inter) { r.intersect = true; break; }
                }
                r.dist = math::NkSqrt(closest.Dot(closest)); r.closest = closest;
                r.n = n; for (int32 i = 0; i < n; ++i) r.simplex[i] = s[i];
                return r;
            }

            // EPA 2D polygonal sur les cœurs (cœurs en pénétration).
            inline bool NkEPACore2D(const NkShape& A, const NkShape& B, NkSV2* simplex, int32 n,
                                    NkVec2f& normalOut, float32& depthOut, NkVec2f& witnessAOut) noexcept {
                if (n < 3) return false;
                NkVector<NkVec2f> poly; NkVector<NkVec2f> wits;
                for (int32 i = 0; i < 3; ++i) { poly.PushBack(simplex[i].v); wits.PushBack(simplex[i].a); }
                float32 area = 0.f;
                for (uint32 i = 0; i < poly.Size(); ++i) area += NkCross2(poly[i], poly[(i + 1) % poly.Size()]);
                if (area < 0.f) {
                    NkVector<NkVec2f> rp, rw;
                    for (uint32 i = poly.Size(); i > 0; --i) { rp.PushBack(poly[i - 1]); rw.PushBack(wits[i - 1]); }
                    poly = rp; wits = rw;
                }
                for (int32 iter = 0; iter < 48; ++iter) {
                    uint32 ei = 0; float32 edist = 1e30f; NkVec2f en{};
                    for (uint32 i = 0; i < poly.Size(); ++i) {
                        const uint32 j = (i + 1) % poly.Size();
                        NkVec2f e = poly[j] - poly[i]; NkVec2f nrm{ e.y, -e.x };
                        const float32 l = math::NkSqrt(nrm.Dot(nrm)); if (l > 1e-12f) nrm = nrm * (1.f / l);
                        const float32 dd = nrm.Dot(poly[i]);
                        if (dd < edist) { edist = dd; ei = i; en = nrm; }
                    }
                    NkSV2 p = NkMinkCore2D(A, B, en);
                    if (p.v.Dot(en) - edist < 1e-4f) {
                        const uint32 j = (ei + 1) % poly.Size();
                        NkVec2f e = poly[j] - poly[ei]; const float32 el2 = e.Dot(e);
                        float32 t = (el2 > 1e-12f) ? math::NkClamp((en * edist - poly[ei]).Dot(e) / el2, 0.f, 1.f) : 0.f;
                        witnessAOut = wits[ei] * (1.f - t) + wits[j] * t;
                        normalOut = en; depthOut = edist; return true;
                    }
                    NkVector<NkVec2f> np, nw;
                    for (uint32 i = 0; i <= ei; ++i) { np.PushBack(poly[i]); nw.PushBack(wits[i]); }
                    np.PushBack(p.v); nw.PushBack(p.a);
                    for (uint32 i = ei + 1; i < poly.Size(); ++i) { np.PushBack(poly[i]); nw.PushBack(wits[i]); }
                    poly = np; wits = nw;
                }
                return false;
            }

        } // namespace detail

        inline bool NkGJKEPA2D(const NkShape& A, const NkShape& B, NkManifold2D& m) noexcept {
            using namespace detail;
            const float32 mA = NkShapeMargin(A), mB = NkShapeMargin(B), tot = mA + mB;
            NkVec2f normal, witnessA; float32 depth = 0.f;
            NkSV2 bsimplex[3]; int32 bn = 0;
            if (NkGJKBool2D(A, B, bsimplex, bn)) {
                if (!NkEPACore2D(A, B, bsimplex, bn, normal, depth, witnessA)) {
                    NkVec2f cc = NkShapeCenter2D(B) - NkShapeCenter2D(A); float32 l = math::NkSqrt(cc.Dot(cc));
                    normal = (l > 1e-6f) ? cc * (1.f / l) : NkVec2f{ 0,1 }; depth = 1e-4f; witnessA = NkShapeCenter2D(A);
                }
                depth += tot;
            } else {
                NkGJKRes2 r = NkGJKDist2D(A, B);
                if (r.dist > tot + 1e-6f) return false;
                if (r.dist > 1e-6f) normal = r.closest * (-1.f / r.dist);
                else { NkVec2f cc = NkShapeCenter2D(B) - NkShapeCenter2D(A); float32 l = math::NkSqrt(cc.Dot(cc)); normal = (l > 1e-6f) ? cc * (1.f / l) : NkVec2f{ 0,1 }; }
                depth = tot - r.dist; witnessA = r.witnessA;
            }
            NkVec2f cc = NkShapeCenter2D(B) - NkShapeCenter2D(A);
            if (normal.Dot(cc) < 0.f) normal = normal * -1.f;
            m.normal = normal;
            m.points[0].point = witnessA + normal * (mA - depth * 0.5f);
            m.points[0].depth = depth; m.count = 1;
            return true;
        }

    } // namespace collision
} // namespace nkentseu
