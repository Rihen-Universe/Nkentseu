// =============================================================================
// NkCollisionWorld.cpp — Implémentation du monde de collision (ZÉRO STL).
// =============================================================================
#include "NKCollision/NkCollisionWorld.h"

namespace nkentseu {
    namespace collision {

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
            // Ordonner pour factoriser les paires asymétriques ; flip si on a swappé.
            auto rank = [](T t) -> int32 { return (int32)t; };
            const NkShape* a = &A; const NkShape* b = &B; bool swapped = false;
            if (rank(A.type) > rank(B.type)) { a = &B; b = &A; swapped = true; }

            bool hit = false;
            const T ta = a->type, tb = b->type;
            if (ta == T::NK_SPHERE && tb == T::NK_SPHERE)
                hit = NkSphereSphere(a->p0, a->radius, b->p0, b->radius, m);
            else if (ta == T::NK_SPHERE && tb == T::NK_BOX3D)
                hit = NkSphereBox(a->p0, a->radius, b->p0, b->p1, m), m.normal = m.normal * -1.f; // box->sphere -> A(sphere)->B(box)
            else if (ta == T::NK_SPHERE && tb == T::NK_CAPSULE3D)
                hit = NkSphereCapsule(a->p0, a->radius, b->p0, b->p1, b->radius, m);
            else if (ta == T::NK_BOX3D && tb == T::NK_BOX3D)
                hit = NkBoxBox3D(a->p0, a->p1, b->p0, b->p1, m);
            else if (ta == T::NK_CAPSULE3D && tb == T::NK_CAPSULE3D)
                hit = NkCapsuleCapsule(a->p0, a->p1, a->radius, b->p0, b->p1, b->radius, m);
            else
                return false; // paire non gérée en v1 (box-capsule, convex... -> phase suivante)

            if (hit && swapped) m.normal = m.normal * -1.f;  // rétablir A->B d'origine
            return hit;
        }

        // ── Narrowphase 2D -> rempli dans un manifold 3D (z=0) ───────────────
        static bool NkNarrow2D(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept {
            using T = NkShapeType;
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
                hit = NkCircleBox2D(ap, a->radius, bp, bh, m), m.normal = m.normal * -1.f;
            else if (ta == T::NK_BOX2D && tb == T::NK_BOX2D)
                hit = NkBoxBox2D(ap, ah, bp, bh, m);
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
            mBodies.PushBack(b);
            return b.id;
        }
        void NkWorld::RemoveBody(uint32 id) {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
                if (mBodies[i].id == id) { mBodies.RemoveAt(i); return; }
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
            if (NkBody* b = GetBody(id)) b->shape = s;
        }
        void NkWorld::Clear() { mBodies.Clear(); mPairs.Clear(); mNextId = 1u; }

        // ── Step : broadphase O(n²) + narrowphase ────────────────────────────
        void NkWorld::Step() {
            mPairs.Clear();
            const uint32 n = (uint32)mBodies.Size();
            for (uint32 i = 0; i < n; ++i) {
                const NkBody& A = mBodies[i];
                if (!A.active) continue;
                NkAABB3D aabbA = NkBodyAABB(A.shape);
                for (uint32 j = i + 1; j < n; ++j) {
                    const NkBody& B = mBodies[j];
                    if (!B.active) continue;
                    // Filtre de layers (bidirectionnel).
                    if (!((A.layer & B.mask) && (B.layer & A.mask))) continue;
                    NkAABB3D aabbB = NkBodyAABB(B.shape);
                    if (!aabbA.Overlaps(aabbB)) continue;     // broadphase
                    NkManifold3D m;
                    if (Narrow(A.shape, B.shape, m)) {        // narrowphase
                        NkCollisionPair pair; pair.a = A.id; pair.b = B.id; pair.manifold = m;
                        mPairs.PushBack(pair);
                    }
                }
            }
        }

        // ── Raycast 3D (hit le plus proche) ──────────────────────────────────
        bool NkWorld::Raycast3D(const NkRay3D& ray, NkRayHit3D& hit, uint32 mask) const {
            bool found = false; NkRayHit3D best; best.t = ray.maxT;
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                const NkBody& b = mBodies[i];
                if (!b.active || NkShapeIs2D(b.shape.type) || !(b.layer & mask)) continue;
                NkRayHit3D h;
                bool ok = false;
                if (b.shape.type == NkShapeType::NK_SPHERE)
                    ok = NkRaySphere(ray, b.shape.p0, b.shape.radius, h);
                else {                                        // box/capsule -> AABB (v1)
                    NkAABB3D a = NkComputeAABB3D(b.shape);
                    ok = NkRayAABB3D(ray, a.min, a.max, h);
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

    } // namespace collision
} // namespace nkentseu
