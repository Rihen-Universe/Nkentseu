// =============================================================================
// NkSceneNode.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkSceneNode.h"

namespace nkentseu {
    namespace renderer {

        NkSceneNode::~NkSceneNode() noexcept {
            // Detacher du parent + des enfants pour eviter les pointeurs pendants.
            if (mParent) {
                if (auto* p = static_cast<NkSceneNode*>(mParent)) {
                    p->RemoveChild(this);
                }
                mParent = nullptr;
            }
            for (auto* c : mChildren) {
                if (c) c->mParent = nullptr;
            }
            mChildren.Clear();
        }

        void NkSceneNode::SetLocalTransform(const NkTransform& t) noexcept {
            mLocal = t;
            MarkTransformDirty();
        }

        void NkSceneNode::SetLocalPosition(NkVec3f pos) noexcept {
            mLocal.translation = pos;
            MarkTransformDirty();
        }

        void NkSceneNode::SetLocalRotation(NkQuatf rot) noexcept {
            mLocal.rotation = rot;
            MarkTransformDirty();
        }

        void NkSceneNode::SetLocalScale(NkVec3f s) noexcept {
            mLocal.scale = s;
            MarkTransformDirty();
        }

        const NkMat4f& NkSceneNode::GetWorldMatrix() const noexcept {
            if (mWorldDirty) {
                NkMat4f local = mLocal.ToMatrix();
                if (mParent) {
                    mWorldCached = mParent->GetWorldMatrix() * local;
                } else {
                    mWorldCached = local;
                }
                mWorldDirty = false;
            }
            return mWorldCached;
        }

        NkVec3f NkSceneNode::GetWorldPosition() const noexcept {
            const NkMat4f& m = GetWorldMatrix();
            // Translation column en column-major : data[12..14]
            return {m[3][0], m[3][1], m[3][2]};
        }

        void NkSceneNode::SetParent(NkITransformable* parent) noexcept {
            if (mParent == parent) return;
            // Detach de l'ancien parent
            if (mParent) {
                if (auto* old = static_cast<NkSceneNode*>(mParent)) {
                    old->RemoveChild(this);
                }
            }
            mParent = parent;
            // Attach au nouveau (si c'est un NkSceneNode)
            if (auto* np = static_cast<NkSceneNode*>(parent)) {
                np->AddChild(this);
            }
            MarkTransformDirty();
        }

        void NkSceneNode::MarkTransformDirty() noexcept {
            if (mWorldDirty) return;   // deja propage
            mWorldDirty = true;
            for (auto* c : mChildren) {
                if (c) c->MarkTransformDirty();
            }
        }

        void NkSceneNode::AddChild(NkSceneNode* child) noexcept {
            if (!child || child == this) return;
            // Verifier qu'il n'est pas deja dans la liste
            for (auto* c : mChildren) if (c == child) return;
            mChildren.PushBack(child);
            // Pas d'appel a child->SetParent(this) ici : SetParent appelle AddChild,
            // donc on aurait une recursion. Le caller doit utiliser SetParent.
        }

        void NkSceneNode::RemoveChild(NkSceneNode* child) noexcept {
            for (uint32 i = 0; i < (uint32)mChildren.Size(); i++) {
                if (mChildren[i] == child) {
                    mChildren.RemoveAt(i);
                    return;
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
