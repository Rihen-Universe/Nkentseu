#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Interaction-first 2D/3D gizmo + 2D/3D grid API.
 * Main data: Configs, runtime state and manipulate/draw entry points.
 * Change this file when: Public gizmo/grid behavior or contracts evolve.
 */

#include "NkUI/NkUIContext.h"
#include "NkUI/NkUIDrawList.h"

namespace nkentseu {
    namespace nkui {

        struct NkUIGizmoVec3 {
            float32 x = 0.f;
            float32 y = 0.f;
            float32 z = 0.f;
        };

        struct NkUIGizmoTransform {
            NkUIGizmoVec3 position {0.f, 0.f, 0.f};
            NkUIGizmoVec3 rotationDeg {0.f, 0.f, 0.f};
            NkUIGizmoVec3 scale {1.f, 1.f, 1.f};
        };

        enum class NkUIGizmoMode : uint8 {
            NK_TRANSLATE = 0,
            NK_ROTATE,
            NK_SCALE
        };

        enum class NkUIGizmoSpace : uint8 {
            NK_LOCAL = 0,
            NK_WORLD
        };

        enum NkUIGizmoAxisMask : uint8 {
            NK_GIZMO_AXIS_NONE = 0,
            NK_GIZMO_AXIS_X = 1 << 0,
            NK_GIZMO_AXIS_Y = 1 << 1,
            NK_GIZMO_AXIS_Z = 1 << 2,
            NK_GIZMO_AXIS_ALL = NK_GIZMO_AXIS_X | NK_GIZMO_AXIS_Y | NK_GIZMO_AXIS_Z
        };

        struct NkUIGizmoSnap {
            bool enabled = false;
            float32 translateStep = 0.25f;
            float32 rotateStepDeg = 15.f;
            float32 scaleStep = 0.1f;
        };

        struct NkUIGizmoConfig {
            NkUIGizmoMode mode = NkUIGizmoMode::NK_TRANSLATE;
            NkUIGizmoSpace space = NkUIGizmoSpace::NK_LOCAL;
            uint8 axisMask = NK_GIZMO_AXIS_ALL;
            NkUIGizmoSnap snap {};
            float32 axisLength = 84.f;
            float32 axisThickness = 2.f;
            float32 handleRadius = 5.f;
            bool showCenter = true;
            bool showAxisLabels = true;
        };

        struct NkUIGridConfig {
            bool enabled = true;
            bool showAxisX = true;
            bool showAxisY = true;
            bool showAxisZ = true;
            bool showPlane2D = true;
            bool showPlaneXZ = true;
            bool showPlaneXY = false;
            bool showPlaneYZ = false;
            float32 cellSizePx = 24.f;
            int32 lineCount = 24;
            NkColor lineColor = {62, 70, 85, 140};
            NkColor axisXColor = {220, 84, 84, 220};
            NkColor axisYColor = {84, 220, 120, 220};
            NkColor axisZColor = {84, 140, 220, 220};
        };

        struct NkUIGizmo2DDesc {
            NkRect viewport {};
            NkVec2 originPx {};
            float32 axisLength = 84.f;
        };

        struct NkUIGizmo3DDesc {
            NkRect viewport {};
            NkVec2 originPx {};
            NkVec2 axisXDirPx {1.f, 0.f};   // screen-space direction
            NkVec2 axisYDirPx {0.f, -1.f};  // screen-space direction
            NkVec2 axisZDirPx {0.7f, 0.5f}; // screen-space direction
            float32 axisLength = 96.f;
            float32 unitsPerPixel = 0.01f;
        };

        struct NkUIGizmoState {
            bool hovered = false;
            bool active = false;
            uint8 activeAxis = NK_GIZMO_AXIS_NONE;
            NkVec2 dragStartPx {};
            float32 dragAccum = 0.f;
        };

        struct NkUIGizmoResult {
            bool changed = false;
            bool hovered = false;
            bool active = false;
            uint8 activeAxis = NK_GIZMO_AXIS_NONE;
            NkUIGizmoVec3 deltaPosition {};
            NkUIGizmoVec3 deltaRotationDeg {};
            NkUIGizmoVec3 deltaScale {};
        };

        struct NKUI_API NkUIGizmo {
            static void DrawGrid2D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   NkRect viewport,
                                   NkVec2 originPx) noexcept;

            static void DrawGrid3D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   const NkUIGizmo3DDesc& desc) noexcept;

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

