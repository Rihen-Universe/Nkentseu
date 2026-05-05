// =============================================================================
// Nkentseu/ECS/Systems/NkTransformSystem.cpp — v2
// =============================================================================
#include "NkTransformSystem.h"

namespace nkentseu {

    using namespace ecs;
    using namespace math;

    // =========================================================================
    // Execute
    // =========================================================================
    void NkTransformSystem::Execute(NkWorld& world, float32 /*dt*/) noexcept {
        mStack.Clear();

        // ── Étape 1 : Collecte des racines ──────────────────────────────────
        // Entités racines = NkTransform + (pas de NkParent OU NkParent invalide)
        world.Query<NkTransform>()
            .ForEach([&](NkEntityId id, const NkTransform& tf) {
                // Vérifier si c'est une racine
                bool isRoot = true;
                if (world.Has<NkParent>(id)) {
                    const auto* parent = world.Get<NkParent>(id);
                    if (parent && parent->entity.IsValid() &&
                        world.IsAlive(parent->entity)) {
                        isRoot = false;  // A un parent valide → pas une racine
                    }
                }

                if (isRoot) {
                    mStack.PushBack({
                        id,
                        NkMat4f::Identity(),
                        tf.worldDirty  // Toujours dirty pour les racines si local changé
                    });
                }
            });

        // ── Étape 2 : DFS itératif ──────────────────────────────────────────
        while (!mStack.IsEmpty()) {
            const StackEntry entry = mStack.Back();
            mStack.PopBack();

            ProcessEntity(world, entry.entity, entry.parentWorld, entry.parentDirty);
        }
    }

    // =========================================================================
    // ProcessEntity — Traite un nœud et empile ses enfants
    // =========================================================================
    void NkTransformSystem::ProcessEntity(NkWorld& world,
                                          NkEntityId id,
                                          const NkMat4f& parentWorld,
                                          bool parentDirty) noexcept
    {
        auto* tf = world.Get<NkTransform>(id);
        if (!tf) return;

        const bool needsUpdate = parentDirty || tf->worldDirty;

        if (needsUpdate) {
            // Calcul de la matrice monde :
            // worldMatrix = parentWorld * TRS(localPos, localRot, localScale)
            const NkMat4f local = tf->ComputeLocalMatrix();
            tf->worldMatrix    = parentWorld * local;

            // Extrait la position monde depuis la matrice (colonne 3)
            tf->worldPosition = {
                tf->worldMatrix[3][0],
                tf->worldMatrix[3][1],
                tf->worldMatrix[3][2]
            };

            tf->worldDirty = false;
        }

        // ── Empile les enfants ───────────────────────────────────────────────
        const auto* children = world.Get<NkChildren>(id);
        if (!children || children->count == 0) return;

        for (nk_uint32 i = 0; i < children->count; ++i) {
            const NkEntityId childId = children->children[i];
            if (!childId.IsValid() || !world.IsAlive(childId)) continue;

            mStack.PushBack({
                childId,
                tf->worldMatrix,
                needsUpdate  // Propage dirty aux enfants si ce nœud a changé
            });
        }
    }

} // namespace nkentseu