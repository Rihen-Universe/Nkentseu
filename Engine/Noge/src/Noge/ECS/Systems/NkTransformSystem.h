#pragma once
// =============================================================================
// Nkentseu/ECS/Systems/NkTransformSystem.h — v2
// =============================================================================
// Propagation hiérarchique des matrices monde (DFS itératif, zéro récursion).
//
// ALGORITHME :
//   PreUpdate prio 1000 (premier à s'exécuter) :
//   1. Collecte les entités racines : NkTransform + sans NkParent valide
//   2. DFS itératif via mStack (NkVector — pas de récursion = stack overflow proof)
//   3. Pour chaque nœud :
//      worldMatrix = parent.worldMatrix * ComputeLocalMatrix()
//      worldPosition = worldMatrix.column(3)
//      worldDirty = false
//   4. Propage aux enfants (NkChildren) si worldDirty

//
// DIRTY PROPAGATION :
//   Si un ancêtre a worldDirty = true, tous ses descendants sont recalculés
//   même s'ils n'ont pas changé localement.
//
// PERFORMANCE :
//   • Zéro allocation frame : mStack est réutilisé chaque frame (Clear() only)
//   • SoA-friendly : accès contigus aux composants NkTransform
//   • Compatible SIMD : NkMat4f est aligné 16 bytes
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Nkentseu/ECS/Components/Core/NkTransform.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkMath.h"

namespace nkentseu {

    using namespace math;

    class NkTransformSystem final : public ecs::NkSystem {
        public:
            NkTransformSystem()  noexcept = default;
            ~NkTransformSystem() noexcept override = default;

            // ── Descriptor ───────────────────────────────────────────────
            [[nodiscard]] ecs::NkSystemDesc Describe() const override {
                return ecs::NkSystemDesc{}
                    .Reads<ecs::NkParent>()
                    .Reads<ecs::NkChildren>()
                    .Writes<ecs::NkTransform>()
                    .InGroup(ecs::NkSystemGroup::PreUpdate)
                    .WithPriority(1000.f)    // En premier absolu
                    .Named("NkTransformSystem");
            }

            // ── Exécution ─────────────────────────────────────────────────
            void Execute(ecs::NkWorld& world, float32 dt) noexcept override;

        private:
            struct StackEntry {
                ecs::NkEntityId entity;
                NkMat4f         parentWorld  = NkMat4f::Identity();
                bool            parentDirty  = false;
            };

            NkVector<StackEntry> mStack;  ///< Réutilisé chaque frame (0 allocation)

            void ProcessEntity(ecs::NkWorld& world,
                               ecs::NkEntityId id,
                               const NkMat4f& parentWorld,
                               bool parentDirty) noexcept;
    };

} // namespace nkentseu