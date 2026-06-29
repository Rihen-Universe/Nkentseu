// =============================================================================
// NkCollisionWorld.cpp — Implémentation du monde de collision (ZÉRO STL).
// =============================================================================
#include "NKCollision/NkCollisionWorld.h"
#include "NKCollision/NkColSAT.h"      // OBB (boîtes orientées) via SAT
#include "NKCollision/NkColGJK.h"      // narrowphase générique convexe (GJK/EPA)
#include "NKCollision/NkColConcave.h"  // décomposition des concaves (trimesh/heightfield/chain)
#include "NKCollision/NkColCast.h"     // casts génériques (ray-convexe, shape cast) par CA
#include "NKCollision/NkColClip.h"     // manifolds multi-points 2D (clipping polygones)

namespace nkentseu {
    namespace collision {

        static bool NkNarrow3D(const NkShape& A, const NkShape& B, NkManifold3D& m) noexcept;   // fwd
        static bool NkNarrow2D(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept; // fwd

        // Translate une sous-forme PRIMITIVE par un offset (compound). Les formes à
        // sommets (verts) doivent être pré-positionnées (offset ignoré).
        static NkShape NkTranslateShape(const NkShape& s, const NkVec3f& off) noexcept {
            NkShape r = s; r.p0 = s.p0 + off;
            if (s.type == NkShapeType::NK_CAPSULE3D || s.type == NkShapeType::NK_SEGMENT2D
                || s.type == NkShapeType::NK_CAPSULE2D) r.p1 = s.p1 + off; // p1 = 2e extrémité
            return r;
        }

        // Compound : décompose en sous-formes, garde le contact le plus profond.
        static bool NkCompoundNarrow(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept {
            bool any = false; float32 best = -1.f;
            const bool aCompound = (A.type == NkShapeType::NK_COMPOUND);
            const NkShape& comp = aCompound ? A : B;
            for (uint32 i = 0; i < comp.childCount; ++i) {
                NkShape c = NkTranslateShape(comp.children[i], comp.p0);
                const NkShape& X = aCompound ? c : A;
                const NkShape& Y = aCompound ? B : c;
                if (NkShapeIs2D(X.type) != NkShapeIs2D(Y.type)) continue;
                NkManifold3D m;
                const bool hit = NkShapeIs2D(X.type) ? NkNarrow2D(X, Y, m) : NkNarrow3D(X, Y, m);
                if (hit && m.points[0].depth > best) { best = m.points[0].depth; out = m; any = true; }
            }
            return any;
        }

        // ── Plan / half-space infini vs convexe (le plan n'est pas GJK-able) ──
        // Normale renvoyée = du PLAN vers l'autre forme (+n). depth = pénétration.
        static bool NkPlaneVsConvex3D(const NkShape& plane, const NkShape& other, NkManifold3D& m) noexcept {
            const NkVec3f n = plane.p1;                       // normale unité du plan
            NkVec3f deepest = NkSupport3D(other, n * -1.f);   // point le + enfoncé (vers le solide)
            const float32 sep = n.Dot(deepest - plane.p0);    // <=0 => pénètre
            if (sep > 0.f) return false;
            m.normal = n;                                     // plan -> other
            m.points[0].point = deepest - n * (sep * 0.5f);
            m.points[0].depth = -sep;
            m.count = 1;
            return true;
        }

        // ── AABB 3D unifiée d'un corps (2D promu en tranche fine z≈0) ─────────
        static NkAABB3D NkBodyAABB(const NkShape& s) noexcept {
            if (NkShapeIs2D(s.type)) {
                NkAABB2D a = NkComputeAABB2D(s);
                NkAABB3D r;
                r.min = { a.min.x, a.min.y, -0.01f };
                r.max = { a.max.x, a.max.y,  0.01f };
                return r;
            }
            return NkComputeAABB3D(s);
        }

        // ── Narrowphase 3D : dispatch type×type avec swap (normale A->B) ──────
        static bool NkNarrow3D(const NkShape& A, const NkShape& B, NkManifold3D& m) noexcept {
            using T = NkShapeType;

            // Compound : décomposition en sous-formes.
            if (A.type == T::NK_COMPOUND || B.type == T::NK_COMPOUND)
                return NkCompoundNarrow(A, B, m);

            // Concaves statiques (trimesh / heightfield) vs convexe -> décomposition.
            if (A.type == T::NK_TRIMESH3D && NkShapeIsConvex(B.type)) return NkConvexVsTrimesh3D(A, B, m);
            if (B.type == T::NK_TRIMESH3D && NkShapeIsConvex(A.type)) { if (!NkConvexVsTrimesh3D(B, A, m)) return false; m.normal = m.normal * -1.f; return true; }
            if (A.type == T::NK_HEIGHTFIELD3D && NkShapeIsConvex(B.type)) return NkConvexVsHeightfield3D(A, B, m);
            if (B.type == T::NK_HEIGHTFIELD3D && NkShapeIsConvex(A.type)) { if (!NkConvexVsHeightfield3D(B, A, m)) return false; m.normal = m.normal * -1.f; return true; }

            // Plan / half-space infini : test dédié (non convexe borné -> hors GJK).
            if (A.type == T::NK_PLANE3D && NkShapeIsConvex(B.type))
                return NkPlaneVsConvex3D(A, B, m);            // normale plan(A)->B = +n : déjà A->B
            if (B.type == T::NK_PLANE3D && NkShapeIsConvex(A.type)) {
                if (!NkPlaneVsConvex3D(B, A, m)) return false;
                m.normal = m.normal * -1.f;                   // plan(B)->A -> on veut A->B
                return true;
            }

            // Box-box (OBB ou AABB) -> manifold MULTI-POINTS (EPA + clipping de face).
            if (A.type == T::NK_BOX3D && B.type == T::NK_BOX3D)
                return NkCollideBoxBox3D(A, B, m);

            // Ordonner pour factoriser les paires asymétriques ; flip si on a swappé.
            auto rank = [](T t) -> int32 { return (int32)t; };
            const NkShape* a = &A; const NkShape* b = &B; bool swapped = false;
            if (rank(A.type) > rank(B.type)) { a = &B; b = &A; swapped = true; }

            bool hit = false;
            const T ta = a->type, tb = b->type;
            if (ta == T::NK_SPHERE && tb == T::NK_SPHERE)
                hit = NkSphereSphere(a->p0, a->radius, b->p0, b->radius, m);
            else if (ta == T::NK_SPHERE && tb == T::NK_BOX3D && NkBoxAligned(*b))
                hit = NkSphereBox(a->p0, a->radius, b->p0, b->p1, m), m.normal = m.normal * -1.f; // box->sphere -> A(sphere)->B(box)
            else if (ta == T::NK_SPHERE && tb == T::NK_CAPSULE3D)
                hit = NkSphereCapsule(a->p0, a->radius, b->p0, b->p1, b->radius, m);
            else if (ta == T::NK_BOX3D && tb == T::NK_BOX3D && NkBoxAligned(*a) && NkBoxAligned(*b))
                hit = NkBoxBox3D(a->p0, a->p1, b->p0, b->p1, m);
            else if (ta == T::NK_CAPSULE3D && tb == T::NK_CAPSULE3D)
                hit = NkCapsuleCapsule(a->p0, a->p1, a->radius, b->p0, b->p1, b->radius, m);
            else if (NkShapeIsConvex(ta) && NkShapeIsConvex(tb))
                return NkGJKEPA3D(A, B, m);                   // fallback générique (box-capsule, cône, convexe…)
            else
                return false;                                 // concave (trimesh/heightfield/compound) -> vague suivante

            if (hit && swapped) m.normal = m.normal * -1.f;  // rétablir A->B d'origine
            return hit;
        }

        // ── Narrowphase 2D -> rempli dans un manifold 3D (z=0) ───────────────
        // Remplit un manifold 3D (z=0) depuis un manifold 2D.
        static void NkFill3DFrom2D(const NkManifold2D& m, NkManifold3D& out) noexcept {
            out.normal = { m.normal.x, m.normal.y, 0.f };
            out.count = m.count;
            for (int32 i = 0; i < m.count; ++i) {
                out.points[i].point = { m.points[i].point.x, m.points[i].point.y, 0.f };
                out.points[i].depth = m.points[i].depth;
            }
        }

        static bool NkNarrow2D(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept {
            using T = NkShapeType;

            // Chain 2D (concave) vs convexe -> décomposition en segments.
            if (A.type == T::NK_CHAIN2D && NkShapeIsConvex(B.type)) {
                NkManifold2D m; if (!NkConvexVsChain2D(A, B, m)) return false; NkFill3DFrom2D(m, out); return true;
            }
            if (B.type == T::NK_CHAIN2D && NkShapeIsConvex(A.type)) {
                NkManifold2D m; if (!NkConvexVsChain2D(B, A, m)) return false; m.normal = m.normal * -1.f; NkFill3DFrom2D(m, out); return true;
            }

            // Polygone/boîte/triangle vs idem -> manifold MULTI-POINTS (clipping).
            if (NkShapeIsPoly2(A.type) && NkShapeIsPoly2(B.type)) {
                NkManifold2D pm; if (!NkCollidePolygons2D(A, B, pm)) return false;
                NkFill3DFrom2D(pm, out); return true;
            }

            auto rank = [](T t) -> int32 { return (int32)t; };
            const NkShape* a = &A; const NkShape* b = &B; bool swapped = false;
            if (rank(A.type) > rank(B.type)) { a = &B; b = &A; swapped = true; }

            NkManifold2D m; bool hit = false;
            const NkVec2f ap{ a->p0.x, a->p0.y }, bp{ b->p0.x, b->p0.y };
            const NkVec2f ah{ a->p1.x, a->p1.y }, bh{ b->p1.x, b->p1.y };
            const T ta = a->type, tb = b->type;
            if (ta == T::NK_CIRCLE2D && tb == T::NK_CIRCLE2D)
                hit = NkCircleCircle(ap, a->radius, bp, b->radius, m);
            else if (ta == T::NK_CIRCLE2D && tb == T::NK_BOX2D)
                // OBB (gère la rotation ; normale boîte->cercle -> on inverse pour A->B).
                hit = NkOBB2DvsCircle(bp, bh, b->rotation, ap, a->radius, m), m.normal = m.normal * -1.f;
            else if (ta == T::NK_BOX2D && tb == T::NK_BOX2D)
                hit = NkOBB2DvsOBB2D(ap, ah, a->rotation, bp, bh, b->rotation, m);  // OBB-OBB (rotation)
            else if (NkShapeIsConvex(ta) && NkShapeIsConvex(tb)) {
                // fallback générique 2D (segment/triangle/polygone/capsule-box…)
                NkManifold2D gm;
                if (!NkGJKEPA2D(A, B, gm)) return false;       // A,B d'origine (pas de swap)
                out.normal = { gm.normal.x, gm.normal.y, 0.f };
                out.count = gm.count;
                for (int32 i = 0; i < gm.count; ++i) {
                    out.points[i].point = { gm.points[i].point.x, gm.points[i].point.y, 0.f };
                    out.points[i].depth = gm.points[i].depth;
                }
                return true;
            }
            else
                return false;

            if (!hit) return false;
            if (swapped) m.normal = m.normal * -1.f;
            out.normal = { m.normal.x, m.normal.y, 0.f };
            out.count = m.count;
            for (int32 i = 0; i < m.count; ++i) {
                out.points[i].point = { m.points[i].point.x, m.points[i].point.y, 0.f };
                out.points[i].depth = m.points[i].depth;
            }
            return true;
        }

        bool NkWorld::Narrow(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept {
            const bool a2 = NkShapeIs2D(A.type), b2 = NkShapeIs2D(B.type);
            if (a2 != b2) return false;             // pas de mixte 2D/3D
            return a2 ? NkNarrow2D(A, B, out) : NkNarrow3D(A, B, out);
        }

        // ── Gestion des corps ────────────────────────────────────────────────
        uint32 NkWorld::AddBody(const NkShape& shape, uint32 layer, uint32 mask, void* user) {
            NkBody b; b.shape = shape; b.id = mNextId++; b.layer = layer; b.mask = mask; b.user = user;
            b.proxy = mTree.Insert(NkBodyAABB(shape), b.id);   // broadphase persistante
            mBodies.PushBack(b);
            return b.id;
        }
        void NkWorld::RemoveBody(uint32 id) {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
                if (mBodies[i].id == id) { mTree.Remove(mBodies[i].proxy); mBodies.RemoveAt(i); return; }
        }
        NkBody* NkWorld::GetBody(uint32 id) {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
                if (mBodies[i].id == id) return &mBodies[i];
            return nullptr;
        }
        const NkBody* NkWorld::GetBody(uint32 id) const {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
                if (mBodies[i].id == id) return &mBodies[i];
            return nullptr;
        }
        void NkWorld::SetShape(uint32 id, const NkShape& s) {
            if (NkBody* b = GetBody(id)) { b->shape = s; mTree.Update(b->proxy, NkBodyAABB(s)); } // garder le DBVH à jour
        }
        void NkWorld::Clear() { mBodies.Clear(); mPairs.Clear(); mEnter.Clear(); mStay.Clear(); mExit.Clear(); mPrevKeys.Clear(); mTree.Clear(); mNextId = 1u; }

        // Clé ordonnée d'une paire (min,max) -> uint64, pour comparer les frames.
        static nkentseu::uint64 NkPairKey(uint32 a, uint32 b) noexcept {
            const uint32 lo = a < b ? a : b, hi = a < b ? b : a;
            return ((nkentseu::uint64)lo << 32) | (nkentseu::uint64)hi;
        }

        // ── Step : broadphase DBVH + narrowphase + événements enter/stay/exit ─
        void NkWorld::Step() {
            mPairs.Clear(); mEnter.Clear(); mStay.Clear(); mExit.Clear();
            NkVector<nkentseu::uint64> curKeys;
            const uint32 n = (uint32)mBodies.Size();

            // Pour chaque corps actif : requête DBVH de son AABB -> candidats voisins.
            // Dédup : on ne traite la paire que depuis le corps d'id le plus petit.
            NkVector<uint32> cand;
            for (uint32 i = 0; i < n; ++i) {
                const NkBody& A = mBodies[i];
                if (!A.active) continue;
                const NkAABB3D aabbA = NkBodyAABB(A.shape);
                mTree.Query(aabbA, cand);
                for (uint32 c = 0; c < (uint32)cand.Size(); ++c) {
                    const uint32 candId = cand[c];
                    if (candId <= A.id) continue;             // self + dédup (traité côté plus petit id)
                    NkBody* Bp = GetBody(candId);
                    if (!Bp || !Bp->active) continue;
                    const NkBody& B = *Bp;
                    if (!((A.layer & B.mask) && (B.layer & A.mask))) continue;   // filtre layers
                    if (!aabbA.Overlaps(NkBodyAABB(B.shape))) continue;          // AABB serrée
                    NkManifold3D m;
                    if (Narrow(A.shape, B.shape, m)) {                          // narrowphase
                        NkCollisionPair pair; pair.a = A.id; pair.b = B.id; pair.manifold = m;
                        mPairs.PushBack(pair);
                        const nkentseu::uint64 key = NkPairKey(A.id, B.id);
                        curKeys.PushBack(key);
                        bool wasThere = false;
                        for (uint32 k = 0; k < (uint32)mPrevKeys.Size(); ++k) if (mPrevKeys[k] == key) { wasThere = true; break; }
                        NkCollisionEvent ev{ A.id, B.id };
                        if (wasThere) mStay.PushBack(ev); else mEnter.PushBack(ev);
                    }
                }
            }
            // 3) Exit : paires présentes la frame d'avant, absentes maintenant.
            for (uint32 k = 0; k < (uint32)mPrevKeys.Size(); ++k) {
                const nkentseu::uint64 pk = mPrevKeys[k];
                bool still = false;
                for (uint32 c = 0; c < (uint32)curKeys.Size(); ++c) if (curKeys[c] == pk) { still = true; break; }
                if (!still) { NkCollisionEvent ev{ (uint32)(pk >> 32), (uint32)(pk & 0xFFFFFFFFu) }; mExit.PushBack(ev); }
            }
            mPrevKeys = curKeys;
        }

        // ── Raycast 3D (hit le plus proche) ──────────────────────────────────
        bool NkWorld::Raycast3D(const NkRay3D& ray, NkRayHit3D& hit, uint32 mask) const {
            bool found = false; NkRayHit3D best; best.t = ray.maxT;
            NkVector<uint32> cand; mTree.RayCast(ray, cand);          // préfiltre DBVH (AABB traversées)
            for (uint32 i = 0; i < (uint32)cand.Size(); ++i) {
                const NkBody* bp = GetBody(cand[i]);
                if (!bp) continue;
                const NkBody& b = *bp;
                if (!b.active || NkShapeIs2D(b.shape.type) || !(b.layer & mask)) continue;
                NkRayHit3D h;
                bool ok = false;
                const NkShape& s = b.shape;
                switch (s.type) {
                    case NkShapeType::NK_SPHERE:  ok = NkRaySphere(ray, s.p0, s.radius, h); break;
                    case NkShapeType::NK_BOX3D:   ok = NkRayOBB3D(ray, s.p0, s.p1, s.orientation, h); break; // OBB exact (identité = AABB)
                    case NkShapeType::NK_PLANE3D: ok = NkRayPlane3D(ray, s.p0, s.p1, h); break;
                    case NkShapeType::NK_TRIANGLE3D:
                        if (s.verts && s.vertCount >= 3) ok = NkRayTriangle3D(ray, s.verts[0], s.verts[1], s.verts[2], h);
                        break;
                    case NkShapeType::NK_TRIMESH3D: {           // triangle le plus proche
                        if (s.verts && s.indices) {
                            NkRayHit3D th; float32 bt = ray.maxT;
                            for (uint32 t = 0; t + 2 < s.indexCount; t += 3)
                                if (NkRayTriangle3D(ray, s.verts[s.indices[t]], s.verts[s.indices[t + 1]], s.verts[s.indices[t + 2]], th) && th.t < bt) { bt = th.t; h = th; ok = true; }
                        }
                        break;
                    }
                    default: {                                  // capsule/cylindre/cône/convexe -> ray-cast GJK exact
                        if (NkShapeIsConvex(s.type)) ok = NkRayConvex3D(ray, s, h);
                        else { NkAABB3D a = NkComputeAABB3D(s); ok = NkRayAABB3D(ray, a.min, a.max, h); } // concave non géré -> AABB
                        break;
                    }
                }
                if (ok && h.t < best.t) { best = h; found = true; }
            }
            if (found) hit = best;
            return found;
        }

        // ── Raycast 2D ───────────────────────────────────────────────────────
        bool NkWorld::Raycast2D(const NkRay2D& ray, NkRayHit2D& hit, uint32 mask) const {
            bool found = false; NkRayHit2D best; best.t = ray.maxT;
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                const NkBody& b = mBodies[i];
                if (!b.active || !NkShapeIs2D(b.shape.type) || !(b.layer & mask)) continue;
                NkRayHit2D h;
                if (b.shape.type == NkShapeType::NK_CIRCLE2D) {
                    if (NkRayCircle2D(ray, { b.shape.p0.x, b.shape.p0.y }, b.shape.radius, h)
                        && h.t < best.t) { best = h; found = true; }
                } else {
                    NkAABB2D a = NkComputeAABB2D(b.shape);
                    NkRay3D r3{ { ray.origin.x, ray.origin.y, 0.f }, { ray.dir.x, ray.dir.y, 0.f }, ray.maxT };
                    NkRayHit3D h3;
                    if (NkRayAABB3D(r3, { a.min.x, a.min.y, -0.01f }, { a.max.x, a.max.y, 0.01f }, h3)
                        && h3.t < best.t) {
                        best.hit = true; best.t = h3.t; best.point = { h3.point.x, h3.point.y };
                        best.normal = { h3.normal.x, h3.normal.y }; found = true;
                    }
                }
            }
            if (found) hit = best;
            return found;
        }

        // ── Overlap : corps chevauchant une forme arbitraire (broad + narrow) ──
        uint32 NkWorld::Overlap(const NkShape& s, NkVector<uint32>& out, uint32 mask) const {
            out.Clear();
            const NkAABB3D sab = NkBodyAABB(s);
            const bool s2 = NkShapeIs2D(s.type);
            NkVector<uint32> cand; mTree.Query(sab, cand);            // broadphase DBVH O(log n)
            for (uint32 c = 0; c < (uint32)cand.Size(); ++c) {
                const NkBody* bp = GetBody(cand[c]);
                if (!bp || !bp->active || !(bp->layer & mask)) continue;
                if (NkShapeIs2D(bp->shape.type) != s2) continue;      // pas de mixte 2D/3D
                NkManifold3D m;
                if (Narrow(s, bp->shape, m)) out.PushBack(bp->id);    // narrowphase
            }
            return (uint32)out.Size();
        }

        // ── Shape cast (TOI) : forme convexe translatée le long de dir ────────
        bool NkWorld::ShapeCast(const NkShape& shape, const NkVec3f& dir, float32 maxDist,
                                NkRayHit3D& hit, uint32 mask) const {
            const float32 dl = math::NkSqrt(dir.Dot(dir));
            if (dl < 1e-8f || !NkShapeIsConvex(shape.type)) return false;
            const NkVec3f d = dir * (1.f / dl);                       // direction unité
            // AABB balayée (départ U arrivée) pour le cull broadphase.
            NkAABB3D a0 = NkBodyAABB(shape);
            NkAABB3D swept = a0; swept.Expand(a0.min + d * maxDist); swept.Expand(a0.max + d * maxDist);
            bool found = false; NkRayHit3D best; best.t = maxDist;
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                const NkBody& b = mBodies[i];
                if (!b.active || !(b.layer & mask) || NkShapeIs2D(b.shape.type) || !NkShapeIsConvex(b.shape.type)) continue;
                if (!swept.Overlaps(NkBodyAABB(b.shape))) continue;   // broadphase
                float32 t; NkVec3f nrm, p;
                if (NkConvexCast3D(shape, d, maxDist, b.shape, t, nrm, p) && t < best.t) {
                    best.hit = true; best.t = t; best.normal = nrm; best.point = p; found = true;
                }
            }
            if (found) hit = best;
            return found;
        }

        // ── CCD : balayage d'un corps existant (anti-tunneling) ──────────────
        bool NkWorld::SweepBody(uint32 id, const NkVec3f& translation, NkRayHit3D& hit, uint32 mask) const {
            const NkBody* self = GetBody(id);
            if (!self || !NkShapeIsConvex(self->shape.type)) return false;
            const float32 dl = math::NkSqrt(translation.Dot(translation));
            if (dl < 1e-8f) return false;
            const NkVec3f d = translation * (1.f / dl);
            NkAABB3D a0 = NkBodyAABB(self->shape);
            NkAABB3D swept = a0; swept.Expand(a0.min + translation); swept.Expand(a0.max + translation);
            NkVector<uint32> cand; mTree.Query(swept, cand);     // broadphase DBVH (volume balayé)
            bool found = false; NkRayHit3D best; best.t = dl;
            for (uint32 i = 0; i < (uint32)cand.Size(); ++i) {
                if (cand[i] == id) continue;                     // exclure soi-même
                const NkBody* bp = GetBody(cand[i]);
                if (!bp || !bp->active || !(bp->layer & mask) || NkShapeIs2D(bp->shape.type) || !NkShapeIsConvex(bp->shape.type)) continue;
                float32 t; NkVec3f nrm, p;
                if (NkConvexCast3D(self->shape, d, dl, bp->shape, t, nrm, p) && t < best.t) {
                    best.hit = true; best.t = t; best.normal = nrm; best.point = p; found = true;
                }
            }
            if (found) hit = best;
            return found;
        }

    } // namespace collision
} // namespace nkentseu
