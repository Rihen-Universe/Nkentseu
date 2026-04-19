#pragma once
// =============================================================================
// Nkentseu/Viewport/NkViewportCamera.h
// =============================================================================
// Caméra interactive pour les viewports d'éditeur (modélisation, animation...).
//
// DIFFÉRENCE avec NkCameraComponent :
//   • NkCameraComponent = caméra ECS pour le rendu final (jeu/film)
//   • NkViewportCamera  = caméra éditeur standalone, contrôlée par input
//
// La NkViewportCamera NE crée PAS d'entité ECS. Elle produit des matrices
// view/proj directement utilisables par NkRenderSystem via NkOffscreenTarget.
//
// MODES :
//   Orbit   — rotation autour d'un pivot (mode par défaut 3D)
//   Pan     — déplacement perpendiculaire au plan de vue (middle-click)
//   Fly     — navigation libre FPS (shift+F dans Blender)
//   Ortho   — vues orthographiques fixes (Front/Back/Left/Right/Top/Bottom)
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    using namespace math;

    // =========================================================================
    // NkRay — rayon 3D (pour picking/sélection)
    // =========================================================================
    struct NkRay {
        NkVec3f origin    = {};
        NkVec3f direction = {0, 0, -1};  ///< Normalisé

        [[nodiscard]] NkVec3f At(float32 t) const noexcept {
            return origin + direction * t;
        }

        /**
         * @brief Intersection avec un plan (axe normal + distance depuis origine).
         * @return t >= 0 si intersection devant, -1 sinon.
         */
        [[nodiscard]] float32 IntersectPlane(const NkVec3f& planeNormal,
                                              float32 planeD) const noexcept {
            const float32 denom = NkVec3f::Dot(planeNormal, direction);
            if (NkAbs(denom) < 1e-6f) return -1.f;
            return -(NkVec3f::Dot(planeNormal, origin) + planeD) / denom;
        }

        /**
         * @brief Intersection avec une AABB.
         */
        [[nodiscard]] bool IntersectAABB(const NkAABB& aabb, float32& tNear) const noexcept;
    };

    // =========================================================================
    // NkViewportCamera
    // =========================================================================
    class NkViewportCamera {
        public:
            // ── Modes de navigation ───────────────────────────────────────
            enum class Mode : uint8 { Orbit, Pan, Fly, Walk };

            enum class OrthoView : uint8 {
                None = 0,
                Front, Back,
                Left, Right,
                Top, Bottom,
                Isometric  ///< Vue isométrique (45°/35.26°)
            };

            // ── Configuration ─────────────────────────────────────────────
            float32 fovDeg      = 60.f;
            float32 nearClip    = 0.01f;
            float32 farClip     = 10000.f;
            float32 orthoSize   = 5.f;     ///< Demi-hauteur en unités monde (ortho)
            float32 orbitSpeed  = 0.5f;    ///< Sensibilité rotation orbit
            float32 panSpeed    = 0.01f;   ///< Sensibilité pan
            float32 zoomSpeed   = 0.1f;    ///< Sensibilité zoom
            float32 flySpeed    = 5.f;     ///< Vitesse mode fly (unités/s)
            bool    invertY     = false;   ///< Inversion axe Y

            // ── Construction / Réinitialisation ───────────────────────────
            NkViewportCamera() noexcept;

            void Reset() noexcept;

            // ── Navigation interactive ────────────────────────────────────

            /**
             * @brief Rotation orbitale autour du pivot (mode Orbit).
             * @param dx Delta horizontal en pixels (converti en degrés).
             * @param dy Delta vertical en pixels.
             */
            void Orbit(float32 dx, float32 dy) noexcept;

            /**
             * @brief Déplacement perpendiculaire à la direction de vue.
             * @param dx Delta horizontal en pixels.
             * @param dy Delta vertical en pixels.
             */
            void Pan(float32 dx, float32 dy) noexcept;

            /**
             * @brief Zoom avant/arrière (rapproche ou éloigne du pivot).
             * @param delta Positif = zoom avant, négatif = zoom arrière.
             */
            void Zoom(float32 delta) noexcept;

            /**
             * @brief Navigation libre FPS (mode Fly).
             * @param forward  Axe avant/arrière [-1..1].
             * @param right    Axe gauche/droite [-1..1].
             * @param up       Axe haut/bas [-1..1].
             * @param dt       Delta-time.
             */
            void Fly(float32 forward, float32 right, float32 up,
                     float32 dt) noexcept;

            /**
             * @brief Rotation en mode Fly (regarder autour).
             */
            void FlyLook(float32 dx, float32 dy) noexcept;

            // ── Vues prédéfinies ──────────────────────────────────────────

            /**
             * @brief Bascule en vue orthographique fixe.
             */
            void SetOrthoView(OrthoView view) noexcept;

            /**
             * @brief Bascule entre perspective et orthographique.
             */
            void TogglePerspective() noexcept;

            [[nodiscard]] bool IsOrtho() const noexcept { return mOrthoView != OrthoView::None; }
            [[nodiscard]] bool IsPerspective() const noexcept { return mOrthoView == OrthoView::None; }
            [[nodiscard]] OrthoView GetOrthoView() const noexcept { return mOrthoView; }

            // ── Focus ─────────────────────────────────────────────────────

            /**
             * @brief Cadre la caméra sur une AABB (Frame Selection, touche F).
             * Ajuste la distance du pivot pour contenir l'AABB dans le frustum.
             */
            void FrameAABB(const NkAABB& bbox, float32 padding = 1.2f) noexcept;

            /**
             * @brief Cadre la caméra sur un point.
             */
            void FramePoint(const NkVec3f& point, float32 distance = 5.f) noexcept;

            // ── Matrices ──────────────────────────────────────────────────

            /**
             * @brief Retourne la matrice de vue (monde → caméra).
             */
            [[nodiscard]] NkMat4f GetViewMatrix() const noexcept;

            /**
             * @brief Retourne la matrice de projection.
             * @param aspect Rapport largeur/hauteur du viewport.
             */
            [[nodiscard]] NkMat4f GetProjectionMatrix(float32 aspect) const noexcept;

            /**
             * @brief Retourne view × projection.
             */
            [[nodiscard]] NkMat4f GetViewProjMatrix(float32 aspect) const noexcept;

            // ── Picking ───────────────────────────────────────────────────

            /**
             * @brief Convertit un pixel écran en rayon 3D dans l'espace monde.
             * @param pixelX  Coordonnée X du pixel [0..vpWidth].
             * @param pixelY  Coordonnée Y du pixel [0..vpHeight].
             * @param vpWidth Largeur du viewport en pixels.
             * @param vpHeight Hauteur du viewport en pixels.
             */
            [[nodiscard]] NkRay ScreenToRay(float32 pixelX, float32 pixelY,
                                             float32 vpWidth, float32 vpHeight) const noexcept;

            /**
             * @brief Projette un point 3D monde vers coordonnées écran [0..1].
             */
            [[nodiscard]] NkVec3f WorldToScreen(const NkVec3f& worldPos,
                                                 float32 vpWidth,
                                                 float32 vpHeight,
                                                 float32 aspect) const noexcept;

            // ── Accès à l'état ────────────────────────────────────────────
            [[nodiscard]] const NkVec3f& GetPosition()    const noexcept { return mPosition; }
            [[nodiscard]] const NkVec3f& GetTarget()      const noexcept { return mTarget; }
            [[nodiscard]] const NkVec3f& GetUp()          const noexcept { return mUp; }
            [[nodiscard]] float32        GetDistance()    const noexcept { return mDistance; }
            [[nodiscard]] float32        GetAzimuth()     const noexcept { return mAzimuth; }
            [[nodiscard]] float32        GetElevation()   const noexcept { return mElevation; }

            void SetTarget(const NkVec3f& t)              noexcept { mTarget = t; UpdatePosition(); }
            void SetDistance(float32 d)                   noexcept { mDistance = NkMax(0.01f, d); UpdatePosition(); }

        private:
            // ── Orbit state ───────────────────────────────────────────────
            NkVec3f mTarget     = {0, 0, 0};  ///< Pivot d'orbite
            float32 mDistance   = 8.f;         ///< Distance au pivot
            float32 mAzimuth    = 45.f;         ///< Angle horizontal (degrés)
            float32 mElevation  = 25.f;         ///< Angle vertical (degrés)

            // ── Position calculée ─────────────────────────────────────────
            NkVec3f mPosition   = {};
            NkVec3f mUp         = {0, 1, 0};

            // ── Mode Fly ──────────────────────────────────────────────────
            float32 mFlyYaw     = 0.f;
            float32 mFlyPitch   = 0.f;

            // ── Vue orthographique ────────────────────────────────────────
            OrthoView mOrthoView = OrthoView::None;
            float32   mSavedDistance  = 8.f;    ///< Sauvegardé avant switch ortho
            float32   mSavedAzimuth   = 45.f;
            float32   mSavedElevation = 25.f;

            void UpdatePosition() noexcept;
    };

    // =========================================================================
    // NkMultiViewport — gestion de 1-4 viewports simultanés (style Blender)
    // =========================================================================
    struct NkViewportLayout {
        enum class Type : uint8 { Single, Split2H, Split2V, Split4 };
        Type type = Type::Single;
    };

    class NkMultiViewport {
        public:
            static constexpr uint32 kMaxViewports = 4u;

            NkMultiViewport() noexcept;

            /**
             * @brief Configure le layout des viewports.
             */
            void SetLayout(NkViewportLayout::Type layout) noexcept;

            /**
             * @brief Retourne la caméra d'un viewport (0-3).
             */
            [[nodiscard]] NkViewportCamera& GetCamera(uint32 vpIdx) noexcept {
                NKECS_ASSERT(vpIdx < kMaxViewports);
                return mCameras[vpIdx];
            }

            /**
             * @brief Configure le viewport maximisé (focus sur un seul).
             */
            void Maximize(uint32 vpIdx) noexcept;
            void Unmaximize() noexcept;

            [[nodiscard]] bool IsMaximized() const noexcept { return mMaximizedVp >= 0; }
            [[nodiscard]] uint32 ActiveViewport() const noexcept { return mActiveVp; }
            void SetActive(uint32 idx) noexcept { mActiveVp = idx; }

        private:
            NkViewportCamera      mCameras[kMaxViewports];
            NkViewportLayout::Type mLayout     = NkViewportLayout::Type::Single;
            int32                  mMaximizedVp = -1;
            uint32                 mActiveVp    = 0;
    };

} // namespace nkentseu
