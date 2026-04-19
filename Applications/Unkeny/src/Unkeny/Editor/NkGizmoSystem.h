#pragma once
// =============================================================================
// Unkeny/Editor/NkGizmoSystem.h
// =============================================================================
// Gizmos de transformation 3D dans le viewport éditeur.
// Dessine les handles Translate/Rotate/Scale sur l'entité sélectionnée
// et interagit avec la souris via raycasting.
//
// Utilise NkICommandBuffer pour émettre des draw calls de debug (lignes/tris).
// Le rendu effectif dépend d'un NkDebugRenderer (à connecter Phase 3+).
// =============================================================================

#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NkEditorCamera.h"
#include "NkSelectionManager.h"
#include "Editor/CommandHistory.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace Unkeny {

        enum class NkGizmoMode : nk_uint8 { Translate = 0, Rotate, Scale, None };
        enum class NkGizmoSpace: nk_uint8 { World = 0, Local };
        enum class NkGizmoAxis : nk_uint8 { None = 0, X, Y, Z, XY, XZ, YZ, XYZ };

        class NkGizmoSystem {
        public:
            NkGizmoMode  mode  = NkGizmoMode::Translate;
            NkGizmoSpace space = NkGizmoSpace::World;
            float32      size  = 80.f;   // pixels (screen-space constant size)

            // ── Update ────────────────────────────────────────────────────────
            // Appelé chaque frame depuis EditorLayer::OnUpdate()
            // vpMouseX/Y : position souris dans l'espace viewport (pixels)
            // vpW/H      : dimensions du viewport
            void Update(ecs::NkWorld& world,
                        const NkSelectionManager& sel,
                        const NkEditorCamera& cam,
                        float32 vpMouseX, float32 vpMouseY,
                        float32 vpW, float32 vpH,
                        bool   leftDown,
                        CommandHistory* history = nullptr) noexcept;

            // ── Rendu ─────────────────────────────────────────────────────────
            // Appelé dans ViewportLayer::OnRender() après la scène.
            // Remplit drawLines avec des segments de debug (à soumettre via NkDebugRenderer).
            struct DebugLine { NkVec3f a, b; NkVec4f color; float32 thickness; };
            NkVector<DebugLine> drawLines;
            NkVector<DebugLine> drawArrows; // têtes de flèche (cones simplifiés)

            bool IsUsing() const noexcept { return mActiveAxis != NkGizmoAxis::None; }
            bool IsHovered() const noexcept { return mHoveredAxis != NkGizmoAxis::None; }

        private:
            NkGizmoAxis mHoveredAxis = NkGizmoAxis::None;
            NkGizmoAxis mActiveAxis  = NkGizmoAxis::None;

            NkVec3f  mEntityPos;
            NkMat4f  mEntityRot;
            NkVec3f  mDragStart;
            NkVec3f  mDragEntityOrigin;

            void BuildTranslateGizmo(const NkVec3f& origin,
                                     const NkMat4f& vpMatrix,
                                     float32 screenScale) noexcept;
            void BuildRotateGizmo  (const NkVec3f& origin,
                                    const NkMat4f& vpMatrix,
                                    float32 screenScale) noexcept;
            void BuildScaleGizmo   (const NkVec3f& origin,
                                    const NkMat4f& vpMatrix,
                                    float32 screenScale) noexcept;

            NkGizmoAxis HitTest(const NkEditorCamera::Ray& ray,
                                const NkVec3f& origin,
                                float32 screenScale) const noexcept;

            NkVec4f AxisColor(NkGizmoAxis axis, bool hovered) const noexcept;
        };

    } // namespace Unkeny
} // namespace nkentseu
