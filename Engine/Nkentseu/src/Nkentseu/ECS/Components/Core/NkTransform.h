#pragma once
// =============================================================================
// Nkentseu/ECS/Components/Core/NkTransform.h  — Source unique du Transform
// =============================================================================
// Ce fichier est LA définition canonique du composant Transform dans Nkentseu.
// NkLocalTransform/NkWorldTransform (ex-NkSceneGraph) et NkTransformComponent
// (ex-NkCoreComponents) sont tous REMPLACÉS par ce seul composant.
//
// Philosophie :
//   • Un seul composant → zéro duplication, zéro ambiguïté
//   • Données locales séparées des données mondiales (flags dirty)
//   • NkTransformSystem calcule le world à partir du local + hiérarchie
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKMath/NkMath.h"

namespace nkentseu {
    namespace ecs {

        using namespace math;

        // =====================================================================
        // NkTransform — Composant transform canonique
        // =====================================================================
        struct NkTransform {
            // ── Locale (écrite par l'utilisateur) ─────────────────────────
            NkVec3f  localPosition = {0.f, 0.f, 0.f};
            NkQuatf  localRotation = NkQuatf::Identity();
            NkVec3f  localScale    = {1.f, 1.f, 1.f};

            // ── Mondiale (READ-ONLY, calculée par NkTransformSystem) ───────
            mutable NkMat4f worldMatrix   = NkMat4f::Identity();
            mutable NkVec3f worldPosition = {0.f, 0.f, 0.f};
            mutable bool    worldDirty    = true;  ///< recalcul monde requis

            // ── API modification locale ────────────────────────────────────
            void SetLocalPosition(const NkVec3f& p) noexcept {
                localPosition = p; worldDirty = true;
            }
            void SetLocalPosition(float32 x, float32 y, float32 z) noexcept {
                localPosition = {x, y, z}; worldDirty = true;
            }
            void Translate(const NkVec3f& d) noexcept {
                localPosition = localPosition + d; worldDirty = true;
            }
            void SetLocalRotation(const NkQuatf& q) noexcept {
                localRotation = q; worldDirty = true;
            }
            void SetLocalRotationEuler(float32 pitchDeg, float32 yawDeg, float32 rollDeg) noexcept {
                localRotation = NkQuatf(NkEulerAngle(pitchDeg, yawDeg, rollDeg));
                worldDirty = true;
            }
            void Rotate(const NkQuatf& delta) noexcept {
                localRotation = delta * localRotation; worldDirty = true;
            }
            void SetLocalScale(const NkVec3f& s) noexcept {
                localScale = s; worldDirty = true;
            }
            void SetLocalScale(float32 u) noexcept {
                localScale = {u, u, u}; worldDirty = true;
            }

            // ── Accès monde (valide après NkTransformSystem::Execute) ──────
            [[nodiscard]] const NkVec3f& GetWorldPosition() const noexcept {
                return worldPosition;
            }
            [[nodiscard]] NkVec3f GetWorldForward() const noexcept {
                return { -worldMatrix[2][0], -worldMatrix[2][1], -worldMatrix[2][2] };
            }
            [[nodiscard]] NkVec3f GetWorldRight() const noexcept {
                return { worldMatrix[0][0], worldMatrix[0][1], worldMatrix[0][2] };
            }
            [[nodiscard]] NkVec3f GetWorldUp() const noexcept {
                return { worldMatrix[1][0], worldMatrix[1][1], worldMatrix[1][2] };
            }

            [[nodiscard]] NkMat4f ComputeLocalMatrix() const noexcept {
                return NkMat4f::TRS(localPosition, localRotation, localScale);
            }

            void MarkDirty() const noexcept { worldDirty = true; }
        };
        NK_COMPONENT(NkTransform)

        // =====================================================================
        // NkParent — hiérarchie : référence vers le parent ECS
        // =====================================================================
        struct NkParent {
            NkEntityId entity = NkEntityId::Invalid();  ///< Invalid() = racine
        };
        NK_COMPONENT(NkParent)

        // =====================================================================
        // NkChildren — hiérarchie : liste inline des enfants directs (max 64)
        // =====================================================================
        struct NkChildren {
            static constexpr uint32 kMax = 64u;
            NkEntityId children[kMax] = {};
            uint32     count = 0;

            bool Add(NkEntityId c) noexcept {
                if (count >= kMax) return false;
                children[count++] = c;
                return true;
            }
            void Remove(NkEntityId c) noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (children[i] == c) { children[i] = children[--count]; return; }
                }
            }
            bool Has(NkEntityId c) const noexcept {
                for (uint32 i = 0; i < count; ++i)
                    if (children[i] == c) return true;
                return false;
            }
        };
        NK_COMPONENT(NkChildren)

    } // namespace ecs
} // namespace nkentseu