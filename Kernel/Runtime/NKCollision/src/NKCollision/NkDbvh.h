#pragma once
// =============================================================================
// NkDbvh.h — Dynamic AABB Tree (DBVH / DBVT, ZÉRO STL). Arbre binaire d'AABB
// auto-équilibré, index-based (aucun pointeur brut), inspiré de b2DynamicTree
// (Box2D) — amélioré à notre sauce (NkVector, NkAABB3D, free-list).
//
// Usage : broadphase PERSISTANTE (insert/update/remove incrémental) + requêtes
// accélérées O(log n) : Query(AABB) et RayCast. Les feuilles portent un `userId`
// (ex. id de corps). Les AABB des feuilles sont « grossies » d'une marge pour
// éviter de ré-insérer à chaque petit déplacement.
// =============================================================================
#include "NKCollision/NkColTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace collision {

        NK_FORCE_INLINE float32 NkAABBArea(const NkAABB3D& b) noexcept {
            const NkVec3f d = b.max - b.min;            // surface (heuristique SAH)
            return 2.f * (d.x * d.y + d.y * d.z + d.z * d.x);
        }
        NK_FORCE_INLINE NkAABB3D NkAABBUnion(const NkAABB3D& a, const NkAABB3D& b) noexcept {
            NkAABB3D r;
            r.min = { math::NkMin(a.min.x, b.min.x), math::NkMin(a.min.y, b.min.y), math::NkMin(a.min.z, b.min.z) };
            r.max = { math::NkMax(a.max.x, b.max.x), math::NkMax(a.max.y, b.max.y), math::NkMax(a.max.z, b.max.z) };
            return r;
        }
        NK_FORCE_INLINE bool NkAABBContains(const NkAABB3D& a, const NkAABB3D& b) noexcept { // a contient b
            return a.min.x <= b.min.x && a.min.y <= b.min.y && a.min.z <= b.min.z
                && a.max.x >= b.max.x && a.max.y >= b.max.y && a.max.z >= b.max.z;
        }

        class NkDbvh {
            public:
                static constexpr int32 kNull = -1;

                explicit NkDbvh(float32 margin = 0.1f) noexcept : mMargin(margin) {}

                // Insère une feuille (AABB serrée + userId) -> renvoie un proxy (index stable).
                int32 Insert(const NkAABB3D& tight, uint32 userId) {
                    const int32 leaf = AllocNode();
                    mNodes[(uint32)leaf].aabb = Fatten(tight);
                    mNodes[(uint32)leaf].userId = userId;
                    mNodes[(uint32)leaf].height = 0;
                    mNodes[(uint32)leaf].child1 = kNull; mNodes[(uint32)leaf].child2 = kNull;
                    InsertLeaf(leaf);
                    return leaf;
                }

                void Remove(int32 proxy) {
                    if (proxy < 0) return;
                    RemoveLeaf(proxy);
                    FreeNode(proxy);
                }

                // Met à jour la feuille ; ne ré-insère que si la box serrée sort de la box grossie.
                // Renvoie true si une ré-insertion a eu lieu.
                bool Update(int32 proxy, const NkAABB3D& tight) {
                    if (proxy < 0) return false;
                    if (NkAABBContains(mNodes[(uint32)proxy].aabb, tight)) return false;
                    RemoveLeaf(proxy);
                    mNodes[(uint32)proxy].aabb = Fatten(tight);
                    InsertLeaf(proxy);
                    return true;
                }

                uint32 UserId(int32 proxy) const noexcept { return mNodes[(uint32)proxy].userId; }

                // Tous les userId dont l'AABB (grossie) chevauche `box`.
                void Query(const NkAABB3D& box, NkVector<uint32>& out) const {
                    out.Clear();
                    if (mRoot == kNull) return;
                    NkVector<int32> stack; stack.PushBack(mRoot);
                    while (stack.Size() > 0) {
                        const int32 i = stack[stack.Size() - 1]; stack.PopBack();
                        if (i == kNull) continue;
                        const NkDbvhNode& nd = mNodes[(uint32)i];
                        if (!nd.aabb.Overlaps(box)) continue;
                        if (nd.IsLeaf()) out.PushBack(nd.userId);
                        else { stack.PushBack(nd.child1); stack.PushBack(nd.child2); }
                    }
                }

                // Toutes les feuilles dont l'AABB est traversée par le rayon (filtrage large).
                void RayCast(const NkRay3D& ray, NkVector<uint32>& out) const {
                    out.Clear();
                    if (mRoot == kNull) return;
                    NkVector<int32> stack; stack.PushBack(mRoot);
                    while (stack.Size() > 0) {
                        const int32 i = stack[stack.Size() - 1]; stack.PopBack();
                        if (i == kNull) continue;
                        const NkDbvhNode& nd = mNodes[(uint32)i];
                        NkRayHit3D h;
                        if (!NkRayAABBSlab(ray, nd.aabb, h)) continue;
                        if (nd.IsLeaf()) out.PushBack(nd.userId);
                        else { stack.PushBack(nd.child1); stack.PushBack(nd.child2); }
                    }
                }

                void Clear() { mNodes.Clear(); mRoot = kNull; mFree = kNull; }
                int32 Height() const noexcept { return mRoot == kNull ? 0 : mNodes[(uint32)mRoot].height; }

            private:
                struct NkDbvhNode {
                    NkAABB3D aabb;
                    int32 parent = kNull;
                    int32 child1 = kNull, child2 = kNull; // feuille : child1 == kNull
                    int32 height = 0;                         // feuille = 0 ; libre = -1
                    uint32 userId = 0;
                    NK_FORCE_INLINE bool IsLeaf() const noexcept { return child1 == kNull; }
                };

                NkAABB3D Fatten(const NkAABB3D& b) const noexcept {
                    NkAABB3D r = b; r.Grow(mMargin); return r;
                }

                // Ray-AABB (slab) local au DBVH (sans dépendre de NkColTests).
                static bool NkRayAABBSlab(const NkRay3D& ray, const NkAABB3D& bx, NkRayHit3D& hit) noexcept {
                    float32 tmin = 0.f, tmax = ray.maxT;
                    for (int32 a = 0; a < 3; ++a) {
                        const float32 o = ray.origin[(size_t)a], dv = ray.dir[(size_t)a];
                        const float32 lo = bx.min[(size_t)a], hi = bx.max[(size_t)a];
                        if (math::NkAbs(dv) < 1e-8f) { if (o < lo || o > hi) return false; continue; }
                        const float32 inv = 1.f / dv; float32 t1 = (lo - o) * inv, t2 = (hi - o) * inv;
                        if (t1 > t2) { float32 t = t1; t1 = t2; t2 = t; }
                        if (t1 > tmin) tmin = t1; if (t2 < tmax) tmax = t2;
                        if (tmin > tmax) return false;
                    }
                    hit.hit = true; hit.t = tmin; return true;
                }

                int32 AllocNode() {
                    if (mFree != kNull) {
                        const int32 i = mFree; mFree = mNodes[(uint32)i].parent;
                        mNodes[(uint32)i] = NkDbvhNode{};
                        return i;
                    }
                    mNodes.PushBack(NkDbvhNode{});
                    return (int32)mNodes.Size() - 1;
                }
                void FreeNode(int32 i) {
                    mNodes[(uint32)i].parent = mFree; mNodes[(uint32)i].height = -1; mFree = i;
                }

                void InsertLeaf(int32 leaf) {
                    if (mRoot == kNull) { mRoot = leaf; mNodes[(uint32)leaf].parent = kNull; return; }
                    // 1) descendre vers le meilleur frère (coût SAH).
                    const NkAABB3D leafBox = mNodes[(uint32)leaf].aabb;
                    int32 idx = mRoot;
                    while (!mNodes[(uint32)idx].IsLeaf()) {
                        const int32 c1 = mNodes[(uint32)idx].child1, c2 = mNodes[(uint32)idx].child2;
                        const float32 area = NkAABBArea(mNodes[(uint32)idx].aabb);
                        const NkAABB3D combined = NkAABBUnion(mNodes[(uint32)idx].aabb, leafBox);
                        const float32 combinedArea = NkAABBArea(combined);
                        const float32 cost = 2.f * combinedArea;
                        const float32 inherit = 2.f * (combinedArea - area);
                        auto descend = [&](int32 c) {
                            float32 cc;
                            if (mNodes[(uint32)c].IsLeaf()) cc = NkAABBArea(NkAABBUnion(leafBox, mNodes[(uint32)c].aabb)) + inherit;
                            else { const float32 old = NkAABBArea(mNodes[(uint32)c].aabb);
                                   cc = (NkAABBArea(NkAABBUnion(leafBox, mNodes[(uint32)c].aabb)) - old) + inherit; }
                            return cc;
                        };
                        const float32 cost1 = descend(c1), cost2 = descend(c2);
                        if (cost < cost1 && cost < cost2) break;
                        idx = (cost1 < cost2) ? c1 : c2;
                    }
                    const int32 sibling = idx;
                    // 2) nouveau parent.
                    const int32 oldParent = mNodes[(uint32)sibling].parent;
                    const int32 newParent = AllocNode();
                    mNodes[(uint32)newParent].parent = oldParent;
                    mNodes[(uint32)newParent].aabb = NkAABBUnion(leafBox, mNodes[(uint32)sibling].aabb);
                    mNodes[(uint32)newParent].height = mNodes[(uint32)sibling].height + 1;
                    mNodes[(uint32)newParent].child1 = sibling;
                    mNodes[(uint32)newParent].child2 = leaf;
                    mNodes[(uint32)sibling].parent = newParent;
                    mNodes[(uint32)leaf].parent = newParent;
                    if (oldParent != kNull) {
                        if (mNodes[(uint32)oldParent].child1 == sibling) mNodes[(uint32)oldParent].child1 = newParent;
                        else mNodes[(uint32)oldParent].child2 = newParent;
                    } else mRoot = newParent;
                    // 3) remonter : refit + équilibrage.
                    RefitFrom(mNodes[(uint32)leaf].parent);
                }

                void RemoveLeaf(int32 leaf) {
                    if (leaf == mRoot) { mRoot = kNull; return; }
                    const int32 parent = mNodes[(uint32)leaf].parent;
                    const int32 grand = mNodes[(uint32)parent].parent;
                    const int32 sibling = (mNodes[(uint32)parent].child1 == leaf) ? mNodes[(uint32)parent].child2 : mNodes[(uint32)parent].child1;
                    if (grand != kNull) {
                        if (mNodes[(uint32)grand].child1 == parent) mNodes[(uint32)grand].child1 = sibling;
                        else mNodes[(uint32)grand].child2 = sibling;
                        mNodes[(uint32)sibling].parent = grand;
                        FreeNode(parent);
                        RefitFrom(grand);
                    } else {
                        mRoot = sibling; mNodes[(uint32)sibling].parent = kNull; FreeNode(parent);
                    }
                }

                void RefitFrom(int32 i) {
                    while (i != kNull) {
                        i = Balance(i);
                        const int32 c1 = mNodes[(uint32)i].child1, c2 = mNodes[(uint32)i].child2;
                        mNodes[(uint32)i].height = 1 + math::NkMax(mNodes[(uint32)c1].height, mNodes[(uint32)c2].height);
                        mNodes[(uint32)i].aabb = NkAABBUnion(mNodes[(uint32)c1].aabb, mNodes[(uint32)c2].aabb);
                        i = mNodes[(uint32)i].parent;
                    }
                }

                // Équilibrage par rotation (algorithme b2DynamicTree, connu-correct) :
                // si un sous-arbre est déséquilibré de +/-2 en hauteur, on remonte
                // l'enfant le plus haut. Renvoie la nouvelle racine du sous-arbre.
                int32 Balance(int32 iA) noexcept {
                    NkDbvhNode& A = mNodes[(uint32)iA];
                    if (A.IsLeaf() || A.height < 2) return iA;
                    const int32 iB = A.child1, iC = A.child2;
                    const int32 bal = mNodes[(uint32)iC].height - mNodes[(uint32)iB].height;
                    if (bal > 1) {                       // remonter C
                        const int32 iF = mNodes[(uint32)iC].child1, iG = mNodes[(uint32)iC].child2;
                        mNodes[(uint32)iC].child1 = iA; mNodes[(uint32)iC].parent = A.parent; A.parent = iC;
                        if (mNodes[(uint32)iC].parent != kNull) {
                            if (mNodes[(uint32)mNodes[(uint32)iC].parent].child1 == iA) mNodes[(uint32)mNodes[(uint32)iC].parent].child1 = iC;
                            else mNodes[(uint32)mNodes[(uint32)iC].parent].child2 = iC;
                        } else mRoot = iC;
                        if (mNodes[(uint32)iF].height > mNodes[(uint32)iG].height) {
                            mNodes[(uint32)iC].child2 = iF; A.child2 = iG; mNodes[(uint32)iG].parent = iA;
                            A.aabb = NkAABBUnion(mNodes[(uint32)iB].aabb, mNodes[(uint32)iG].aabb);
                            mNodes[(uint32)iC].aabb = NkAABBUnion(A.aabb, mNodes[(uint32)iF].aabb);
                            A.height = 1 + math::NkMax(mNodes[(uint32)iB].height, mNodes[(uint32)iG].height);
                            mNodes[(uint32)iC].height = 1 + math::NkMax(A.height, mNodes[(uint32)iF].height);
                        } else {
                            mNodes[(uint32)iC].child2 = iG; A.child2 = iF; mNodes[(uint32)iF].parent = iA;
                            A.aabb = NkAABBUnion(mNodes[(uint32)iB].aabb, mNodes[(uint32)iF].aabb);
                            mNodes[(uint32)iC].aabb = NkAABBUnion(A.aabb, mNodes[(uint32)iG].aabb);
                            A.height = 1 + math::NkMax(mNodes[(uint32)iB].height, mNodes[(uint32)iF].height);
                            mNodes[(uint32)iC].height = 1 + math::NkMax(A.height, mNodes[(uint32)iG].height);
                        }
                        return iC;
                    }
                    if (bal < -1) {                      // remonter B
                        const int32 iD = mNodes[(uint32)iB].child1, iE = mNodes[(uint32)iB].child2;
                        mNodes[(uint32)iB].child1 = iA; mNodes[(uint32)iB].parent = A.parent; A.parent = iB;
                        if (mNodes[(uint32)iB].parent != kNull) {
                            if (mNodes[(uint32)mNodes[(uint32)iB].parent].child1 == iA) mNodes[(uint32)mNodes[(uint32)iB].parent].child1 = iB;
                            else mNodes[(uint32)mNodes[(uint32)iB].parent].child2 = iB;
                        } else mRoot = iB;
                        if (mNodes[(uint32)iD].height > mNodes[(uint32)iE].height) {
                            mNodes[(uint32)iB].child2 = iD; A.child1 = iE; mNodes[(uint32)iE].parent = iA;
                            A.aabb = NkAABBUnion(mNodes[(uint32)iC].aabb, mNodes[(uint32)iE].aabb);
                            mNodes[(uint32)iB].aabb = NkAABBUnion(A.aabb, mNodes[(uint32)iD].aabb);
                            A.height = 1 + math::NkMax(mNodes[(uint32)iC].height, mNodes[(uint32)iE].height);
                            mNodes[(uint32)iB].height = 1 + math::NkMax(A.height, mNodes[(uint32)iD].height);
                        } else {
                            mNodes[(uint32)iB].child2 = iE; A.child1 = iD; mNodes[(uint32)iD].parent = iA;
                            A.aabb = NkAABBUnion(mNodes[(uint32)iC].aabb, mNodes[(uint32)iD].aabb);
                            mNodes[(uint32)iB].aabb = NkAABBUnion(A.aabb, mNodes[(uint32)iE].aabb);
                            A.height = 1 + math::NkMax(mNodes[(uint32)iC].height, mNodes[(uint32)iD].height);
                            mNodes[(uint32)iB].height = 1 + math::NkMax(A.height, mNodes[(uint32)iE].height);
                        }
                        return iB;
                    }
                    return iA;
                }

                NkVector<NkDbvhNode> mNodes;
                int32   mRoot = kNull;
                int32   mFree = kNull;
                float32 mMargin = 0.1f;
        };

    } // namespace collision
} // namespace nkentseu
