#pragma once
// =============================================================================
// Unkeny/Editor/NkEditorCamera.h
// =============================================================================
// Caméra éditeur avec deux modes :
//   Orbit  : tourne autour d'un point cible (Alt+Drag, Scroll=zoom)
//   Fly    : déplacement libre WASD + souris droite maintenue
//
// Produit view + proj matrices consommées par ViewportLayer::OnRender().
// Alimentée par EventBus (NkKeyPressEvent, NkMouseMoveEvent, etc.)
// ou par mise à jour manuelle depuis UILayer.
// =============================================================================

#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "Nkentseu/Core/EventBus.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace Unkeny {

        enum class NkEditorCameraMode : nk_uint8 { Orbit = 0, Fly };

        class NkEditorCamera {
        public:
            NkEditorCamera() noexcept { RecalcMatrices(); }

            // ── Config ────────────────────────────────────────────────────────
            float32 fovDeg   = 60.f;
            float32 nearClip = 0.05f;
            float32 farClip  = 1000.f;
            float32 aspect   = 16.f / 9.f;

            // ── Orbit ────────────────────────────────────────────────────────
            NkVec3f target    = {0.f, 0.f, 0.f};
            float32 orbitYaw  = 0.f;      // degrés
            float32 orbitPitch= 25.f;     // degrés (clampé -89..89)
            float32 orbitDist = 5.f;      // distance à la cible

            // ── Fly ──────────────────────────────────────────────────────────
            NkVec3f flyPos   = {0.f, 1.f, 5.f};
            float32 flyYaw   = 0.f;
            float32 flyPitch = 0.f;
            float32 flySpeed = 5.f;

            NkEditorCameraMode mode = NkEditorCameraMode::Orbit;

            // ── Matrices (recalculées par Update) ─────────────────────────────
            NkMat4f viewMatrix;
            NkMat4f projMatrix;
            NkMat4f viewProjMatrix;
            NkVec3f position;   // position monde de la caméra

            // ── Update — appelé depuis ViewportLayer::OnUpdate(dt) ────────────
            // isHovered : true si la souris est dans le viewport NKUI
            void Update(float32 dt, bool isViewportHovered) noexcept;

            // ── Input direct (depuis EventBus ou UILayer) ─────────────────────
            void OnMouseMove   (float32 dx, float32 dy,
                                bool altDown, bool rightDown) noexcept;
            void OnMouseScroll (float32 delta) noexcept;
            void OnKeyDown     (NkKey key)  noexcept;
            void OnKeyUp       (NkKey key)  noexcept;

            // Recentrer sur un point
            void FocusOn(const NkVec3f& point, float32 dist = -1.f) noexcept;

            // ── Raycasting ────────────────────────────────────────────────────
            // Retourne un rayon depuis le pixel viewport (x,y) dans l'espace monde
            struct Ray { NkVec3f origin, dir; };
            Ray ScreenRay(float32 vpX, float32 vpY,
                          float32 vpW, float32 vpH) const noexcept;

        private:
            void RecalcMatrices() noexcept;
            void UpdateOrbit(float32 dt, bool hovered) noexcept;
            void UpdateFly  (float32 dt, bool hovered) noexcept;

            // State touches
            bool mKeys[512] = {};
            float32 mPrevMouseX = -1.f, mPrevMouseY = -1.f;
            bool    mRightDown  = false;
            bool    mAltDown    = false;
        };

    } // namespace Unkeny
} // namespace nkentseu
