#pragma once
// =============================================================================
// NkSceneNode.h  — NKRenderer v5.0  (Core/)
//
// Implementation par defaut de NkITransformable. Sert de base pour les
// classes du scene graph : NkStaticMesh, NkSkeletalMesh, NkLight, NkCamera,
// NkSprite, NkSceneGroup. La hierarchie parent/enfant est geree ici.
//
// Pas de runtime polymorphism inutile : les noeuds n'embarquent pas de
// vtable supplementaire, juste les hooks de NkITransformable.
//
// L'API est inspiree de UE Component / Unity Transform avec les conventions
// suivantes :
//  - World matrix recalculee paresseusement (lazy) au premier
//    GetWorldMatrix() apres un setter dirty.
//  - SetParent transmet le dirty flag aux enfants.
//  - Pas d'Update() per-frame : tant qu'on ne touche pas aux setters,
//    le cache reste valide indefiniment.
// =============================================================================
#include "NkITransformable.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkSceneNode : public NkITransformable {
            public:
                NkSceneNode() noexcept = default;
                ~NkSceneNode() noexcept override;

                NkSceneNode(const NkSceneNode&)            = delete;
                NkSceneNode& operator=(const NkSceneNode&) = delete;

                // ── NkITransformable ───────────────────────────────────────────
                const NkTransform& GetLocalTransform() const noexcept override { return mLocal; }
                void                SetLocalTransform(const NkTransform& t) noexcept override;

                void SetLocalPosition(NkVec3f pos)  noexcept override;
                void SetLocalRotation(NkQuatf rot)  noexcept override;
                void SetLocalScale   (NkVec3f s)    noexcept override;
                NkVec3f GetLocalPosition() const noexcept override { return mLocal.translation; }
                NkQuatf GetLocalRotation() const noexcept override { return mLocal.rotation; }
                NkVec3f GetLocalScale()    const noexcept override { return mLocal.scale; }

                const NkMat4f& GetWorldMatrix()  const noexcept override;
                NkVec3f         GetWorldPosition() const noexcept override;

                NkITransformable* GetParent() const noexcept override { return mParent; }
                void               SetParent(NkITransformable* parent) noexcept override;

                void MarkTransformDirty() noexcept override;

                // ── API additionnelle scene-graph ──────────────────────────────
                const NkVector<NkSceneNode*>& GetChildren() const noexcept { return mChildren; }
                void AddChild(NkSceneNode* child) noexcept;
                void RemoveChild(NkSceneNode* child) noexcept;

                // Nom optionnel (pour debug / editor outliner)
                const char* GetName() const noexcept { return mName; }
                void        SetName(const char* n) noexcept { mName = n; }

            protected:
                NkTransform mLocal;
                mutable NkMat4f mWorldCached    = NkMat4f::Identity();
                mutable bool    mWorldDirty     = true;

                NkITransformable*      mParent = nullptr;   // raw pointer non-owning
                NkVector<NkSceneNode*> mChildren;            // non-owning
                const char*            mName   = nullptr;
        };

    } // namespace renderer
} // namespace nkentseu
