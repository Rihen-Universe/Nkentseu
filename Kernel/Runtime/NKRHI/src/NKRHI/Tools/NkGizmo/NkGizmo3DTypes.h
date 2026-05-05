#pragma once

#include "NKMath/NKMath.h"
#include "NKMath/NkColor.h"

namespace nkentseu {

    // Espace de manipulation
    enum class GizmoSpace : uint8 {
        Local,      // axes locaux de l'objet
        World,      // axes mondiaux
        Gimbal,     // axes d'Euler (rotation uniquement)
        Normal      // perpendiculaire à la face (pour translate)
    };

    // Mode de transformation
    enum class GizmoMode : uint8 {
        Translate,
        Rotate,
        Scale
    };

    // Axe ou combinaison d'axes
    enum class GizmoAxis : uint8 {
        None = 0,
        X = 1 << 0,
        Y = 1 << 1,
        Z = 1 << 2,
        XY = X | Y,
        XZ = X | Z,
        YZ = Y | Z,
        XYZ = X | Y | Z
    };
    inline GizmoAxis operator|(GizmoAxis a, GizmoAxis b) { return (GizmoAxis)((uint8)a | (uint8)b); }
    inline bool HasFlag(GizmoAxis a, GizmoAxis f) { return ((uint8)a & (uint8)f) != 0; }

    // Résultat d'une manipulation
    struct Gizmo3DResult {
        bool changed = false;
        math::NkVec3 deltaPosition = {0,0,0};
        math::NkVec3 deltaRotation = {0,0,0}; // degrés (Euler)
        math::NkVec3 deltaScale = {0,0,0};
        GizmoAxis activeAxis = GizmoAxis::None;
    };

    // Configuration
    struct Gizmo3DConfig {
        GizmoMode mode = GizmoMode::Translate;
        GizmoSpace space = GizmoSpace::World;
        GizmoAxis visibleAxes = GizmoAxis::XYZ;      // axes affichés
        float axisLength = 1.2f;
        float arrowHeadSize = 0.1f;
        float lineWidth = 3.0f;
        float planeOpacity = 0.25f;
        float rotationCircleRadius = 1.1f;
        float scaleHandleOffset = 1.1f;
        bool showCenter = true;
        bool showAxisLabels = true;
        bool snapEnabled = false;
        float snapTranslate = 0.25f;
        float snapRotate = 15.0f;   // degrés
        float snapScale = 0.1f;

        // Couleurs
        math::NkColor colorX = {230, 80, 80, 255};
        math::NkColor colorY = {80, 230, 80, 255};
        math::NkColor colorZ = {80, 80, 230, 255};
        math::NkColor colorSelected = {255, 200, 0, 255};
        math::NkColor colorPlane = {128, 128, 128, 64};
        math::NkColor colorCenter = {200, 200, 200, 255};
    };

} // namespace nkentseu