#pragma once

/*
 * NkUIGizmo.h — v3 final
 *
 * Corrections v3 :
 *  - axisLength est dans NkUIGizmoConfig UNIQUEMENT (pas dans NkUIGizmo3DDesc)
 *  - Comparaisons d'axes via constantes uint8 explicites (évite conflit NkStringView)
 *  - Grille infinie (fade-out vers l'horizon comme Blender)
 *  - Plan de sol plein ou wireframe (cfg.gridSolid)
 */

#include "NKUI/NkUIContext.h"
#include "NKUI/NkUIDrawList.h"

namespace nkentseu {
    namespace nkui {

        // =====================================================================
        //  Types math
        // =====================================================================

        struct NkUIGizmoVec3 {
            float32 x = 0.f, y = 0.f, z = 0.f;
        };

        struct NkUIGizmoTransform {
            NkUIGizmoVec3 position    {0.f, 0.f, 0.f};
            NkUIGizmoVec3 rotationDeg {0.f, 0.f, 0.f};
            NkUIGizmoVec3 scale       {1.f, 1.f, 1.f};
        };

        // =====================================================================
        //  Enums
        // =====================================================================

        enum class NkUIGizmoMode : uint8 {
            NK_TRANSLATE = 0,
            NK_ROTATE,
            NK_SCALE
        };

        enum class NkUIGizmoSpace : uint8 {
            NK_LOCAL = 0,
            NK_WORLD,
            NK_NORMAL,
        };

        // Masques d'axes — tous uint8, pas de comparaison >= avec NkStringView possible
        enum NkUIGizmoAxisMask : uint8 {
            NK_GIZMO_AXIS_NONE  = 0,
            NK_GIZMO_AXIS_X     = 1,
            NK_GIZMO_AXIS_Y     = 2,
            NK_GIZMO_AXIS_Z     = 4,
            NK_GIZMO_AXIS_ALL   = 7,
        };

        // Codes d'axe actif — séparés des masques pour éviter les ambiguïtés
        // (jamais comparés avec >= dans le code de production)
        static constexpr uint8 NKGIZMO_AX_X    = 1u;
        static constexpr uint8 NKGIZMO_AX_Y    = 2u;
        static constexpr uint8 NKGIZMO_AX_Z    = 4u;
        static constexpr uint8 NKGIZMO_AX_XY   = 3u;   // plan XY
        static constexpr uint8 NKGIZMO_AX_XZ   = 5u;   // plan XZ
        static constexpr uint8 NKGIZMO_AX_YZ   = 6u;   // plan YZ
        static constexpr uint8 NKGIZMO_AX_UNI  = 255u; // scale uniforme / translate libre

        // =====================================================================
        //  Snap
        // =====================================================================

        struct NkUIGizmoSnap {
            bool    enabled       = false;
            float32 translateStep = 0.25f;
            float32 rotateStepDeg = 15.f;
            float32 scaleStep     = 0.1f;
        };

        // =====================================================================
        //  Configuration du gizmo
        //
        //  axisLength : longueur FIXE en pixels — indépendante de la caméra.
        //               Mis ici (dans cfg), PAS dans NkUIGizmo3DDesc.
        // =====================================================================

        struct NkUIGizmoConfig {
            NkUIGizmoMode  mode          = NkUIGizmoMode::NK_TRANSLATE;
            NkUIGizmoSpace space         = NkUIGizmoSpace::NK_WORLD;
            uint8          axisMask      = NK_GIZMO_AXIS_ALL;
            NkUIGizmoSnap  snap          {};

            // Dimensions (toutes en pixels, fixes)
            float32 axisLength           = 90.f;   ///< longueur des axes en px (FIXE)
            float32 axisThickness        = 2.f;
            float32 handleRadius         = 5.5f;
            float32 planeSize            = 0.38f;  ///< taille plan = axisLength * planeSize

            // Visibilité
            bool showCenter              = true;
            bool showAxisLabels          = true;
            bool showGrid                = true;
            bool showPlanes              = true;
            bool showCenterMove          = true;
            bool showCenterScale         = true;
            bool showSnapGrid            = true;
            bool showRotationAngle       = true;
            bool active                  = true;

            // Couleurs
            NkColor colorX  = {220,  80,  80, 235};
            NkColor colorY  = { 80, 220, 120, 235};
            NkColor colorZ  = { 80, 140, 220, 235};
            NkColor colorHL = {245, 245, 245, 255};

            // Options modeMask (bits: 0=translate,1=rotate,2=scale)
            // 0 = utilise mode seul
            uint8 modeMask = 0;
        };

        // =====================================================================
        //  Grille — infinie avec fade-out
        // =====================================================================

        struct NkUIGridConfig {
            bool    enabled      = true;

            // Axes
            bool    showAxisX    = true;
            bool    showAxisY    = true;
            bool    showAxisZ    = true;

            // Plans
            bool    showPlane2D  = true;
            bool    showPlaneXZ  = true;   ///< plan principal (sol)
            bool    showPlaneXY  = false;
            bool    showPlaneYZ  = false;

            // Apparence grille
            float32 cellSizePx   = 24.f;   ///< taille d'une cellule en pixels
            int32   lineCount    = 40;      ///< nombre de lignes (0 = infini simulé)
            bool    infinite     = true;    ///< fade-out vers l'horizon (style Blender)
            float32 fadeStart    = 0.55f;   ///< début du fade-out (0..1 de la demi-grille)
            bool    solid        = false;   ///< plan de sol plein (rempli) ou wireframe

            // Couleurs
            NkColor lineColor    = { 62,  70,  85, 130};
            NkColor solidColor   = { 30,  35,  45, 110}; ///< couleur si solid=true
            NkColor axisXColor   = {220,  84,  84, 220};
            NkColor axisYColor   = { 84, 220, 120, 220};
            NkColor axisZColor   = { 84, 140, 220, 220};
        };

        // =====================================================================
        //  Description de la vue 3D
        //
        //  N'a PAS de axisLength — c'est dans NkUIGizmoConfig.
        //  L'app fournit uniquement les directions d'axes projetées et l'origine.
        // =====================================================================

        struct NkUIGizmo3DDesc {
            NkRect  viewport      {};
            NkVec2  originPx      {};            ///< position écran de l'objet
            NkVec2  axisXDirPx    {1.f,  0.f};  ///< direction X normalisée (screen-space)
            NkVec2  axisYDirPx    {0.f, -1.f};  ///< direction Y normalisée (screen-space)
            NkVec2  axisZDirPx    {0.7f, 0.5f}; ///< direction Z normalisée (screen-space)
            float32 unitsPerPixel = 0.014f;      ///< 1px = N unités monde (1/scale)
        };

        struct NkUIGizmo2DDesc {
            NkRect  viewport   {};
            NkVec2  originPx   {};
        };

        // =====================================================================
        //  État runtime — axes gelés au début du drag
        // =====================================================================

        struct NkUIGizmoState {
            bool    hovered    = false;
            bool    active     = false;
            uint8   activeAxis = NK_GIZMO_AXIS_NONE;

            NkVec2  dragStartPx {};
            float32 dragAccum   = 0.f;

            // Snapshot au début du drag (gelés pendant toute l'interaction)
            NkVec2  frozenAxisX  {1.f,  0.f};
            NkVec2  frozenAxisY  {0.f, -1.f};
            NkVec2  frozenAxisZ  {0.7f, 0.5f};
            NkVec2  frozenOrigin {};

            // Accumulateurs (delta total depuis le début du drag)
            float32 accumX = 0.f, accumY = 0.f, accumZ = 0.f;

            // Accumulateurs snap par axe
            float32 snapAccumX = 0.f, snapAccumY = 0.f, snapAccumZ = 0.f;
        };

        // =====================================================================
        //  Résultat
        // =====================================================================

        struct NkUIGizmoResult {
            bool    changed    = false;
            bool    hovered    = false;
            bool    active     = false;
            uint8   activeAxis = NK_GIZMO_AXIS_NONE;

            NkUIGizmoVec3 deltaPosition    {};
            NkUIGizmoVec3 deltaRotationDeg {};
            NkUIGizmoVec3 deltaScale       {};

            NkUIGizmoVec3 totalPosition    {};
            NkUIGizmoVec3 totalRotationDeg {};
            NkUIGizmoVec3 totalScale       {};
        };

        // =====================================================================
        //  NkUIGizmo
        // =====================================================================

        struct NKUI_API NkUIGizmo {

            /// Grille 2D (plan XY)
            static void DrawGrid2D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   NkRect viewport,
                                   NkVec2 originPx) noexcept;

            /// Grille 3D projetée — infinie si cfg.infinite == true
            static void DrawGrid3D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   const NkUIGizmo3DDesc& desc,
                                   const NkUIGizmoConfig& gizmoCfg) noexcept;

            static NkUIGizmoResult Manipulate2D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo2DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept;

            static NkUIGizmoResult Manipulate3D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo3DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu