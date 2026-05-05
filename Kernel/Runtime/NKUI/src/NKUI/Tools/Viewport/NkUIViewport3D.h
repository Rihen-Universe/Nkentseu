#pragma once

/*
 * NkUIViewport3D.h
 * Viewport 3D production-ready style Unreal Engine.
 *
 * Fonctionnalités :
 *  ┌─────────────────────────────────────────────────────┐
 *  │  Barre d'outils supérieure (mode transform, vue, snap, stats)
 *  │  ┌──────────────────────────────┬──────────────────┐
 *  │  │                              │                  │
 *  │  │   Viewport 3D                │   Outliner       │
 *  │  │   - Grille infinie           │   - Liste objets │
 *  │  │   - Axes XYZ                 │   - Icônes type  │
 *  │  │   - Gizmo multi-mode         │                  │
 *  │  │   - Objets 3D                ├──────────────────┤
 *  │  │   - Minimap caméra           │   Détails        │
 *  │  │                              │   - Transform    │
 *  │  │   Navigation info            │   - Couleur      │
 *  │  └──────────────────────────────┴──────────────────┘
 *  │  Barre de statut (pos, rot, scale, fps, infos)
 *  └─────────────────────────────────────────────────────┘
 *
 *  Navigation :
 *   - LMB drag   : orbite caméra
 *   - RMB drag   : pan
 *   - Molette    : zoom
 *   - F          : focus sur objet sélectionné
 *   - Numpad 1/3/7 : vues front/right/top
 *   - G/R/S       : activer mode translate/rotate/scale
 *   - Ctrl+Z     : undo (1 niveau)
 *   - Del        : supprimer objet
 */

#include "NKUI/NkUIContext.h"
#include "NKUI/NkUIDrawList.h"
#include "NKUI/Tools/Gizmo/NkUIGizmo.h"
#include "NKUI/NkUIFont.h"

namespace nkentseu {
    namespace nkui {

        // =====================================================================
        //  Types de projection caméra
        // =====================================================================

        enum class NkVP3DCameraMode : uint8 {
            NK_VP3D_PERSPECTIVE = 0,
            NK_VP3D_ORTHO_FRONT,
            NK_VP3D_ORTHO_BACK,
            NK_VP3D_ORTHO_LEFT,
            NK_VP3D_ORTHO_RIGHT,
            NK_VP3D_ORTHO_TOP,
            NK_VP3D_ORTHO_BOTTOM,
        };

        // =====================================================================
        //  Type de shading/rendu wireframe
        // =====================================================================

        enum class NkVP3DShadingMode : uint8 {
            NK_VP3D_WIREFRAME = 0,
            NK_VP3D_SOLID,
            NK_VP3D_UNLIT,
        };

        // =====================================================================
        //  Type d'objet dans la scène
        // =====================================================================

        enum class NkVP3DObjectShape : uint8 {
            NK_VP3D_CUBE = 0,
            NK_VP3D_SPHERE,
            NK_VP3D_CYLINDER,
            NK_VP3D_PLANE,
            NK_VP3D_CONE,
            NK_VP3D_TORUS,
            NK_VP3D_LIGHT_POINT,
            NK_VP3D_LIGHT_DIR,
            NK_VP3D_CAMERA,
            NK_VP3D_EMPTY,
        };

        // =====================================================================
        //  Objet 3D de la scène
        // =====================================================================

        struct NkVP3DObject {
            char               name[64]      = {};
            NkVP3DObjectShape  shape         = NkVP3DObjectShape::NK_VP3D_CUBE;
            NkUIGizmoTransform transform     = {};
            NkUIGizmoState     gizmoState    = {};
            NkColor            color         = {180, 180, 180, 255};
            NkColor            wireColor     = {200, 200, 200, 255};
            bool               visible       = true;
            bool               locked        = false;
            bool               selected      = false;

            // Pour la restauration undo
            NkUIGizmoTransform prevTransform = {};
        };

        // =====================================================================
        //  Caméra de la scène
        // =====================================================================

        struct NkVP3DCamera {
            float32 yaw          = 35.f;
            float32 pitch        = 25.f;
            float32 distance     = 10.f;    ///< distance du pivot (orbite)
            float32 pivotX       = 0.f;
            float32 pivotY       = 0.f;
            float32 pivotZ       = 0.f;
            float32 fovDeg       = 60.f;
            float32 orthoZoom    = 1.f;     ///< zoom en mode orthographique
            NkVP3DCameraMode mode = NkVP3DCameraMode::NK_VP3D_PERSPECTIVE;
        };

        // =====================================================================
        //  Configuration du viewport
        // =====================================================================

        struct NkVP3DConfig {
            // Dimensions panneaux
            float32 outlinerW         = 220.f;   ///< largeur panneau droit
            float32 toolbarH          = 34.f;    ///< hauteur toolbar
            float32 statusBarH        = 22.f;    ///< hauteur barre de statut

            // Grille
            NkUIGridConfig grid       = {};

            // Overlays
            bool showStats            = true;    ///< FPS, draw calls, etc.
            bool showOrientationCube  = true;    ///< cube d'orientation (coin sup-droit)
            bool showCameraInfo       = true;    ///< info caméra en overlay
            bool showMiniAxes         = true;    ///< mini axes XYZ en bas-gauche
            bool showSelectionInfo    = true;    ///< info sélection en overlay
            bool showGrid             = true;

            // Navigation
            float32 orbitSensitivity  = 0.5f;
            float32 panSensitivity    = 0.014f;
            float32 zoomSensitivity   = 1.2f;
            float32 focusPaddingFactor= 2.5f;

            // Apparence
            NkColor bgColor           = {25, 25, 28, 255};
            NkColor bgGradTop         = {20, 22, 30, 255};
            NkColor bgGradBot         = {14, 16, 22, 255};
            NkColor selectionColor    = {255, 140, 0, 200};  ///< orange Unreal
            NkColor accentColor       = {0,   122, 204, 255}; ///< bleu UE

            // Snap
            float32 snapTranslate     = 0.25f;
            float32 snapRotate        = 15.f;
            float32 snapScale         = 0.1f;
        };

        // =====================================================================
        //  État runtime du viewport
        // =====================================================================

        struct NkVP3DState {
            static constexpr int32 MAX_OBJECTS = 64;

            // Objets de la scène
            NkVP3DObject objects[MAX_OBJECTS]  = {};
            int32        numObjects            = 0;

            // Sélection multi
            int32        selectedIdx[MAX_OBJECTS] = {};
            int32        numSelected           = 0;

            // Caméra
            NkVP3DCamera camera               = {};

            // Navigation en cours
            bool         orbiting             = false;
            bool         panning              = false;
            NkVec2       navStart             = {};

            // Mode gizmo
            NkUIGizmoConfig gizmoCfg          = {};

            // UI state
            bool         snapEnabled          = false;
            NkVP3DShadingMode shadingMode     = NkVP3DShadingMode::NK_VP3D_WIREFRAME;

            // Undo (1 niveau)
            bool         hasUndo              = false;
            int32        undoIdx              = -1;
            NkUIGizmoTransform undoTransform  = {};

            // Overlays
            float32      fps                  = 0.f;
            float32      fpsAccum             = 0.f;
            int32        fpsFrames            = 0;

            // Hover pour l'outliner
            int32        outlinerHovered      = -1;

            // Pour l'animation focus
            bool         focusAnimating       = false;
            float32      focusT               = 0.f;
            NkVP3DCamera focusFrom            = {};
            NkVP3DCamera focusTo              = {};
        };

        // =====================================================================
        //  Résultat du viewport (événements)
        // =====================================================================

        enum class NkVP3DEvent : uint8 {
            NK_VP3D_NONE = 0,
            NK_VP3D_OBJECT_SELECTED,
            NK_VP3D_OBJECT_DESELECTED,
            NK_VP3D_OBJECT_TRANSFORMED,
            NK_VP3D_OBJECT_DELETED,
            NK_VP3D_CAMERA_MOVED,
        };

        struct NkVP3DResult {
            NkVP3DEvent event   = NkVP3DEvent::NK_VP3D_NONE;
            int32       objIdx  = -1;
            NkUIGizmoTransform newTransform = {};
        };

        // =====================================================================
        //  NkUIViewport3D — le widget complet
        // =====================================================================

        struct NKUI_API NkUIViewport3D {

            /// Dessine le viewport complet dans rect.
            static NkVP3DResult Draw(NkUIContext& ctx,
                                     NkUIDrawList& dl,
                                     NkUIFont& font,
                                     NkUIID id,
                                     NkRect rect,
                                     NkVP3DConfig& cfg,
                                     NkVP3DState& state,
                                     float32 dt) noexcept;

            /// Ajoute un objet à la scène.
            static int32 AddObject(NkVP3DState& state,
                                    const char* name,
                                    NkVP3DObjectShape shape,
                                    NkColor color = {180,180,180,255}) noexcept;

            /// Focus la caméra sur l'objet sélectionné (ou tous les objets).
            static void FocusSelected(NkVP3DState& state,
                                       const NkVP3DConfig& cfg) noexcept;

            /// Initialise l'état avec des objets de démonstration.
            static void SetupDemoScene(NkVP3DState& state) noexcept;

        private:
            // Sous-zones
            struct Layout {
                NkRect toolbar;
                NkRect viewport3D;
                NkRect outliner;
                NkRect details;
                NkRect statusBar;
            };
            static Layout ComputeLayout(NkRect r, const NkVP3DConfig& cfg) noexcept;

            // Projection
            struct ProjectCtx {
                NkVec2  center;
                float32 scale;
                float32 camZ;
                float32 yawR;
                float32 pitchR;
                bool    isPerspective;
                float32 orthoZoom;
                bool    ProjectDir = true;
                NkVP3DCameraMode cameraMode;
            };
            static ProjectCtx MakeProjectCtx(const NkVP3DCamera& cam,
                                              NkRect viewport) noexcept;
            static NkVec2 Project(const ProjectCtx& pc,
                                  float32 wx, float32 wy, float32 wz) noexcept;
            static NkVec2 ProjectDir(const ProjectCtx& pc,
                                     float32 dx, float32 dy, float32 dz) noexcept;

            // Rendu des sous-zones
            static void DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                     NkUIFont& font, NkRect r,
                                     NkVP3DConfig& cfg,
                                     NkVP3DState& state) noexcept;

            static NkVP3DResult DrawViewport(NkUIContext& ctx, NkUIDrawList& dl,
                                              NkUIFont& font, NkUIID id,
                                              NkRect r,
                                              NkVP3DConfig& cfg,
                                              NkVP3DState& state,
                                              float32 dt) noexcept;

            static void DrawOutliner(NkUIContext& ctx, NkUIDrawList& dl,
                                      NkUIFont& font, NkRect r,
                                      NkVP3DConfig& cfg,
                                      NkVP3DState& state) noexcept;

            static void DrawDetails(NkUIContext& ctx, NkUIDrawList& dl,
                                     NkUIFont& font, NkRect r,
                                     NkVP3DConfig& cfg,
                                     NkVP3DState& state) noexcept;

            static void DrawStatusBar(NkUIContext& ctx, NkUIDrawList& dl,
                                       NkUIFont& font, NkRect r,
                                       const NkVP3DConfig& cfg,
                                       const NkVP3DState& state) noexcept;

            // Overlays dans le viewport
            static void DrawGrid(NkUIDrawList& dl, NkRect r,
                                  const NkVP3DConfig& cfg,
                                  const NkVP3DState& state,
                                  const ProjectCtx& pc) noexcept;

            static void DrawObjects(NkUIContext& ctx, NkUIDrawList& dl,
                                     NkUIFont& font, NkRect r,
                                     NkVP3DConfig& cfg,
                                     NkVP3DState& state,
                                     const ProjectCtx& pc,
                                     bool leftClick,
                                     NkVP3DResult& result) noexcept;

            static void DrawObjectShape(NkUIDrawList& dl, NkRect viewport,
                                         const NkVP3DObject& obj,
                                         const ProjectCtx& pc,
                                         bool selected, bool hovered,
                                         const NkVP3DConfig& cfg,
                                         NkVP3DShadingMode shading) noexcept;

            static void DrawGizmo(NkUIContext& ctx, NkUIDrawList& dl,
                                   NkRect r,
                                   NkVP3DConfig& cfg,
                                   NkVP3DState& state,
                                   const ProjectCtx& pc,
                                   NkVP3DResult& result) noexcept;

            static void DrawOrientationCube(NkUIDrawList& dl, NkUIFont& font,
                                             NkRect viewport,
                                             const NkVP3DCamera& cam) noexcept;

            static void DrawMiniAxes(NkUIDrawList& dl, NkRect viewport,
                                      const ProjectCtx& pc) noexcept;

            static void DrawStats(NkUIDrawList& dl, NkUIFont& font,
                                   NkRect viewport,
                                   const NkVP3DState& state) noexcept;

            static void DrawNavigationHelp(NkUIDrawList& dl, NkUIFont& font,
                                            NkRect viewport) noexcept;

            static void DrawSelectionOverlay(NkUIDrawList& dl, NkUIFont& font,
                                              NkRect viewport,
                                              const NkVP3DState& state,
                                              const ProjectCtx& pc) noexcept;

            // Navigation caméra
            static void HandleNavigation(NkUIContext& ctx, NkRect r,
                                          NkVP3DConfig& cfg,
                                          NkVP3DState& state,
                                          bool gizmoActive,
                                          float32 dt) noexcept;

            static void HandleKeyboardShortcuts(NkUIContext& ctx,
                                                 NkVP3DConfig& cfg,
                                                 NkVP3DState& state,
                                                 NkRect viewport) noexcept;

            // Helpers objets
            static const char* ShapeName(NkVP3DObjectShape s) noexcept;
            static void DrawShapeIcon(NkUIDrawList& dl, NkRect r,
                                       NkVP3DObjectShape s, NkColor col) noexcept;

            // Helpers dessin 3D
            static void DrawCube(NkUIDrawList& dl, NkRect clip,
                                  const ProjectCtx& pc,
                                  float32 px, float32 py, float32 pz,
                                  float32 sx, float32 sy, float32 sz,
                                  NkColor col, float32 lw) noexcept;

            static void DrawSphere(NkUIDrawList& dl, NkRect clip,
                                    const ProjectCtx& pc,
                                    float32 px, float32 py, float32 pz,
                                    float32 r, NkColor col, float32 lw) noexcept;

            static void DrawCylinder(NkUIDrawList& dl, NkRect clip,
                                      const ProjectCtx& pc,
                                      float32 px, float32 py, float32 pz,
                                      float32 rx, float32 ry, NkColor col,
                                      float32 lw) noexcept;

            static void DrawCone(NkUIDrawList& dl, NkRect clip,
                                  const ProjectCtx& pc,
                                  float32 px, float32 py, float32 pz,
                                  float32 r, float32 h, NkColor col,
                                  float32 lw) noexcept;

            static void DrawPlane(NkUIDrawList& dl, NkRect clip,
                                   const ProjectCtx& pc,
                                   float32 px, float32 py, float32 pz,
                                   float32 sx, float32 sz, NkColor col,
                                   float32 lw) noexcept;

            static void DrawLightIcon(NkUIDrawList& dl,
                                       const ProjectCtx& pc,
                                       float32 px, float32 py, float32 pz,
                                       NkVP3DObjectShape type,
                                       NkColor col) noexcept;

            static void DrawCameraIcon(NkUIDrawList& dl,
                                        const ProjectCtx& pc,
                                        float32 px, float32 py, float32 pz,
                                        NkColor col) noexcept;

            static void DrawEmptyIcon(NkUIDrawList& dl,
                                       const ProjectCtx& pc,
                                       float32 px, float32 py, float32 pz,
                                       NkColor col) noexcept;

            // Picking 2D
            static float32 PickObject(const NkVP3DObject& obj,
                                       const ProjectCtx& pc,
                                       NkVec2 mousePos) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu