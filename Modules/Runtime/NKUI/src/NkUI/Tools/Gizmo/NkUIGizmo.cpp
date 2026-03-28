#include "NkUI/Tools/Gizmo/NkUIGizmo.h"

#include <cmath>

namespace nkentseu {
    namespace nkui {

        namespace {
            static NKUI_INLINE float32 AbsF(float32 v) noexcept { return v < 0.f ? -v : v; }

            static NKUI_INLINE float32 Dot2(NkVec2 a, NkVec2 b) noexcept {
                return a.x * b.x + a.y * b.y;
            }

            static NKUI_INLINE float32 Len2(NkVec2 v) noexcept {
                return std::sqrt(v.x * v.x + v.y * v.y);
            }

            static NKUI_INLINE NkVec2 Normalize2(NkVec2 v, NkVec2 fallback = {1.f, 0.f}) noexcept {
                const float32 l = Len2(v);
                if (l <= 1e-6f) return fallback;
                return {v.x / l, v.y / l};
            }

            static NKUI_INLINE float32 Clamp01(float32 v) noexcept {
                if (v < 0.f) return 0.f;
                if (v > 1.f) return 1.f;
                return v;
            }

            static NKUI_INLINE bool InViewport(NkRect r, NkVec2 p) noexcept {
                return p.x >= r.x && p.x <= (r.x + r.w) && p.y >= r.y && p.y <= (r.y + r.h);
            }

            static NKUI_INLINE float32 DistanceToSegment(NkVec2 p, NkVec2 a, NkVec2 b) noexcept {
                const NkVec2 ab {b.x - a.x, b.y - a.y};
                const float32 denom = Dot2(ab, ab);
                if (denom <= 1e-6f) return Len2({p.x - a.x, p.y - a.y});
                const float32 t = Clamp01(Dot2({p.x - a.x, p.y - a.y}, ab) / denom);
                const NkVec2 h {a.x + ab.x * t, a.y + ab.y * t};
                return Len2({p.x - h.x, p.y - h.y});
            }

            static NKUI_INLINE float32 SnapValue(float32 v, float32 step) noexcept {
                if (step <= 1e-6f) return v;
                const float32 q = std::floor((v / step) + (v >= 0.f ? 0.5f : -0.5f));
                return q * step;
            }

            static NKUI_INLINE void DrawAxisLabel(NkUIDrawList& dl,
                                                  NkVec2 pos,
                                                  char label,
                                                  NkColor col) noexcept {
                char text[2] = {label, 0};
                dl.AddText(pos, text, col, 12.f);
            }

            static NKUI_INLINE void ApplyDeltaForAxis(uint8 axis,
                                                      float32 value,
                                                      NkUIGizmoVec3& out) noexcept {
                out = {};
                if (axis == NK_GIZMO_AXIS_X) out.x = value;
                if (axis == NK_GIZMO_AXIS_Y) out.y = value;
                if (axis == NK_GIZMO_AXIS_Z) out.z = value;
            }

            static NKUI_INLINE NkColor AxisColor(uint8 axis, const NkUIGridConfig& g) noexcept {
                if (axis == NK_GIZMO_AXIS_X) return g.axisXColor;
                if (axis == NK_GIZMO_AXIS_Y) return g.axisYColor;
                return g.axisZColor;
            }
        } // namespace

        void NkUIGizmo::DrawGrid2D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   NkRect viewport,
                                   NkVec2 originPx) noexcept {
            if (!cfg.enabled || !cfg.showPlane2D) return;

            const float32 step = cfg.cellSizePx > 2.f ? cfg.cellSizePx : 2.f;
            const int32 lines = cfg.lineCount > 2 ? cfg.lineCount : 2;
            const float32 half = static_cast<float32>(lines) * step * 0.5f;

            const float32 minX = originPx.x - half;
            const float32 maxX = originPx.x + half;
            const float32 minY = originPx.y - half;
            const float32 maxY = originPx.y + half;

            dl.PushClipRect(viewport, true);
            for (int32 i = 0; i <= lines; ++i) {
                const float32 t = static_cast<float32>(i) / static_cast<float32>(lines);
                const float32 x = minX + (maxX - minX) * t;
                const float32 y = minY + (maxY - minY) * t;
                dl.AddLine({x, minY}, {x, maxY}, cfg.lineColor, 1.f);
                dl.AddLine({minX, y}, {maxX, y}, cfg.lineColor, 1.f);
            }
            if (cfg.showAxisX) dl.AddLine({viewport.x, originPx.y}, {viewport.x + viewport.w, originPx.y}, cfg.axisXColor, 1.6f);
            if (cfg.showAxisY) dl.AddLine({originPx.x, viewport.y}, {originPx.x, viewport.y + viewport.h}, cfg.axisYColor, 1.6f);
            dl.PopClipRect();
        }

        void NkUIGizmo::DrawGrid3D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   const NkUIGizmo3DDesc& desc) noexcept {
            if (!cfg.enabled || !(cfg.showPlaneXZ || cfg.showPlaneXY || cfg.showPlaneYZ)) return;

            const NkVec2 dx = Normalize2(desc.axisXDirPx, {1.f, 0.f});
            const NkVec2 dy = Normalize2(desc.axisYDirPx, {0.f, -1.f});
            const NkVec2 dz = Normalize2(desc.axisZDirPx, {0.7f, 0.5f});
            const float32 step = cfg.cellSizePx > 2.f ? cfg.cellSizePx : 2.f;
            const int32 lines = cfg.lineCount > 2 ? cfg.lineCount : 2;
            const int32 half = lines / 2;

            dl.PushClipRect(desc.viewport, true);

            // XZ plane (editor default)
            if (cfg.showPlaneXZ) {
                for (int32 i = -half; i <= half; ++i) {
                    const NkVec2 offZ {dz.x * i * step, dz.y * i * step};
                    const NkVec2 offX {dx.x * static_cast<float32>(half) * step, dx.y * static_cast<float32>(half) * step};
                    dl.AddLine({desc.originPx.x - offX.x + offZ.x, desc.originPx.y - offX.y + offZ.y},
                               {desc.originPx.x + offX.x + offZ.x, desc.originPx.y + offX.y + offZ.y},
                               cfg.lineColor, 1.f);
                }
                for (int32 i = -half; i <= half; ++i) {
                    const NkVec2 offX {dx.x * i * step, dx.y * i * step};
                    const NkVec2 offZ {dz.x * static_cast<float32>(half) * step, dz.y * static_cast<float32>(half) * step};
                    dl.AddLine({desc.originPx.x - offZ.x + offX.x, desc.originPx.y - offZ.y + offX.y},
                               {desc.originPx.x + offZ.x + offX.x, desc.originPx.y + offZ.y + offX.y},
                               cfg.lineColor, 1.f);
                }
            }

            if (cfg.showAxisX) dl.AddLine(desc.originPx, {desc.originPx.x + dx.x * desc.axisLength, desc.originPx.y + dx.y * desc.axisLength}, cfg.axisXColor, 1.8f);
            if (cfg.showAxisY) dl.AddLine(desc.originPx, {desc.originPx.x + dy.x * desc.axisLength, desc.originPx.y + dy.y * desc.axisLength}, cfg.axisYColor, 1.8f);
            if (cfg.showAxisZ) dl.AddLine(desc.originPx, {desc.originPx.x + dz.x * desc.axisLength, desc.originPx.y + dz.y * desc.axisLength}, cfg.axisZColor, 1.8f);

            dl.PopClipRect();
        }

        NkUIGizmoResult NkUIGizmo::Manipulate2D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo2DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept {
            NkUIGizmoResult out {};
            out.activeAxis = state.activeAxis;

            const NkUIGridConfig gridCfg {};
            DrawGrid2D(gridCfg, dl, desc.viewport, desc.originPx);

            NkVec2 origin {desc.originPx.x + transform.position.x, desc.originPx.y + transform.position.y};
            const float32 axisLen = cfg.axisLength > 8.f ? cfg.axisLength : 8.f;
            const NkVec2 axisX {origin.x + axisLen, origin.y};
            const NkVec2 axisY {origin.x, origin.y - axisLen};

            const bool allowX = (cfg.axisMask & NK_GIZMO_AXIS_X) != 0;
            const bool allowY = (cfg.axisMask & NK_GIZMO_AXIS_Y) != 0;
            const bool inVp = InViewport(desc.viewport, ctx.input.mousePos);

            uint8 hoveredAxis = NK_GIZMO_AXIS_NONE;
            if (inVp && allowX && DistanceToSegment(ctx.input.mousePos, origin, axisX) <= 8.f) hoveredAxis = NK_GIZMO_AXIS_X;
            if (inVp && allowY && DistanceToSegment(ctx.input.mousePos, origin, axisY) <= 8.f) hoveredAxis = NK_GIZMO_AXIS_Y;

            state.hovered = hoveredAxis != NK_GIZMO_AXIS_NONE;
            out.hovered = state.hovered;

            if (!state.active && hoveredAxis != NK_GIZMO_AXIS_NONE && ctx.ConsumeMouseClick(0)) {
                state.active = true;
                state.activeAxis = hoveredAxis;
                state.dragStartPx = ctx.input.mousePos;
                state.dragAccum = 0.f;
            }

            if (state.active && state.activeAxis != NK_GIZMO_AXIS_NONE) {
                const NkVec2 axisDir = (state.activeAxis == NK_GIZMO_AXIS_X) ? Normalize2({1.f, 0.f}) : Normalize2({0.f, -1.f});
                const float32 proj = Dot2(ctx.input.mouseDelta, axisDir);
                state.dragAccum += proj;

                if (cfg.mode == NkUIGizmoMode::NK_TRANSLATE) {
                    float32 delta = proj;
                    if (cfg.snap.enabled) delta = SnapValue(delta, cfg.snap.translateStep);
                    transform.position.x += (state.activeAxis == NK_GIZMO_AXIS_X) ? delta : 0.f;
                    transform.position.y += (state.activeAxis == NK_GIZMO_AXIS_Y) ? -delta : 0.f;
                    ApplyDeltaForAxis(state.activeAxis, delta, out.deltaPosition);
                } else if (cfg.mode == NkUIGizmoMode::NK_ROTATE) {
                    float32 deltaDeg = proj * 0.8f;
                    if (cfg.snap.enabled) deltaDeg = SnapValue(deltaDeg, cfg.snap.rotateStepDeg);
                    transform.rotationDeg.z += deltaDeg;
                    out.deltaRotationDeg.z = deltaDeg;
                } else {
                    float32 deltaScale = proj * 0.01f;
                    if (cfg.snap.enabled) deltaScale = SnapValue(deltaScale, cfg.snap.scaleStep);
                    if (state.activeAxis == NK_GIZMO_AXIS_X) transform.scale.x += deltaScale;
                    if (state.activeAxis == NK_GIZMO_AXIS_Y) transform.scale.y += deltaScale;
                    ApplyDeltaForAxis(state.activeAxis, deltaScale, out.deltaScale);
                }

                out.changed = true;
                out.active = true;
                out.activeAxis = state.activeAxis;
            }

            if (state.active && ctx.ConsumeMouseRelease(0)) {
                state.active = false;
                state.activeAxis = NK_GIZMO_AXIS_NONE;
                state.dragAccum = 0.f;
            }

            const NkColor baseX = {220, 84, 84, 230};
            const NkColor baseY = {84, 220, 120, 230};
            const NkColor hl = {245, 245, 245, 255};
            const NkColor colX = (state.activeAxis == NK_GIZMO_AXIS_X || hoveredAxis == NK_GIZMO_AXIS_X) ? hl : baseX;
            const NkColor colY = (state.activeAxis == NK_GIZMO_AXIS_Y || hoveredAxis == NK_GIZMO_AXIS_Y) ? hl : baseY;

            dl.PushClipRect(desc.viewport, true);
            if (allowX) {
                dl.AddLine(origin, axisX, colX, cfg.axisThickness);
                dl.AddCircleFilled(axisX, cfg.handleRadius, colX);
                if (cfg.showAxisLabels) DrawAxisLabel(dl, {axisX.x + 6.f, axisX.y - 6.f}, 'X', colX);
            }
            if (allowY) {
                dl.AddLine(origin, axisY, colY, cfg.axisThickness);
                dl.AddCircleFilled(axisY, cfg.handleRadius, colY);
                if (cfg.showAxisLabels) DrawAxisLabel(dl, {axisY.x + 6.f, axisY.y - 6.f}, 'Y', colY);
            }
            if (cfg.showCenter) dl.AddCircleFilled(origin, 4.f, {240, 240, 240, 230});
            dl.PopClipRect();

            return out;
        }

        NkUIGizmoResult NkUIGizmo::Manipulate3D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo3DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept {
            NkUIGizmoResult out {};
            out.activeAxis = state.activeAxis;

            NkUIGridConfig g {};
            g.showPlaneXZ = true;
            g.showAxisX = true;
            g.showAxisY = true;
            g.showAxisZ = true;
            DrawGrid3D(g, dl, desc);

            const float32 axisLen = cfg.axisLength > 8.f ? cfg.axisLength : 8.f;
            const NkVec2 dx = Normalize2(desc.axisXDirPx, {1.f, 0.f});
            const NkVec2 dy = Normalize2(desc.axisYDirPx, {0.f, -1.f});
            const NkVec2 dz = Normalize2(desc.axisZDirPx, {0.7f, 0.5f});

            const NkVec2 origin = desc.originPx;
            const NkVec2 endX {origin.x + dx.x * axisLen, origin.y + dx.y * axisLen};
            const NkVec2 endY {origin.x + dy.x * axisLen, origin.y + dy.y * axisLen};
            const NkVec2 endZ {origin.x + dz.x * axisLen, origin.y + dz.y * axisLen};

            uint8 hoveredAxis = NK_GIZMO_AXIS_NONE;
            if (InViewport(desc.viewport, ctx.input.mousePos)) {
                if ((cfg.axisMask & NK_GIZMO_AXIS_X) && DistanceToSegment(ctx.input.mousePos, origin, endX) <= 8.f) hoveredAxis = NK_GIZMO_AXIS_X;
                if ((cfg.axisMask & NK_GIZMO_AXIS_Y) && DistanceToSegment(ctx.input.mousePos, origin, endY) <= 8.f) hoveredAxis = NK_GIZMO_AXIS_Y;
                if ((cfg.axisMask & NK_GIZMO_AXIS_Z) && DistanceToSegment(ctx.input.mousePos, origin, endZ) <= 8.f) hoveredAxis = NK_GIZMO_AXIS_Z;
            }

            state.hovered = hoveredAxis != NK_GIZMO_AXIS_NONE;
            out.hovered = state.hovered;

            if (!state.active && hoveredAxis != NK_GIZMO_AXIS_NONE && ctx.ConsumeMouseClick(0)) {
                state.active = true;
                state.activeAxis = hoveredAxis;
                state.dragStartPx = ctx.input.mousePos;
                state.dragAccum = 0.f;
            }

            if (state.active && state.activeAxis != NK_GIZMO_AXIS_NONE) {
                NkVec2 axisDir = dx;
                if (state.activeAxis == NK_GIZMO_AXIS_Y) axisDir = dy;
                if (state.activeAxis == NK_GIZMO_AXIS_Z) axisDir = dz;

                const float32 proj = Dot2(ctx.input.mouseDelta, axisDir);
                state.dragAccum += proj;

                if (cfg.mode == NkUIGizmoMode::NK_TRANSLATE) {
                    float32 worldDelta = proj * (desc.unitsPerPixel > 1e-6f ? desc.unitsPerPixel : 0.01f);
                    if (cfg.snap.enabled) worldDelta = SnapValue(worldDelta, cfg.snap.translateStep);
                    if (state.activeAxis == NK_GIZMO_AXIS_X) transform.position.x += worldDelta;
                    if (state.activeAxis == NK_GIZMO_AXIS_Y) transform.position.y += worldDelta;
                    if (state.activeAxis == NK_GIZMO_AXIS_Z) transform.position.z += worldDelta;
                    ApplyDeltaForAxis(state.activeAxis, worldDelta, out.deltaPosition);
                } else if (cfg.mode == NkUIGizmoMode::NK_ROTATE) {
                    float32 deltaDeg = proj * 0.7f;
                    if (cfg.snap.enabled) deltaDeg = SnapValue(deltaDeg, cfg.snap.rotateStepDeg);
                    if (state.activeAxis == NK_GIZMO_AXIS_X) transform.rotationDeg.x += deltaDeg;
                    if (state.activeAxis == NK_GIZMO_AXIS_Y) transform.rotationDeg.y += deltaDeg;
                    if (state.activeAxis == NK_GIZMO_AXIS_Z) transform.rotationDeg.z += deltaDeg;
                    ApplyDeltaForAxis(state.activeAxis, deltaDeg, out.deltaRotationDeg);
                } else {
                    float32 deltaScale = proj * 0.01f;
                    if (cfg.snap.enabled) deltaScale = SnapValue(deltaScale, cfg.snap.scaleStep);
                    if (state.activeAxis == NK_GIZMO_AXIS_X) transform.scale.x += deltaScale;
                    if (state.activeAxis == NK_GIZMO_AXIS_Y) transform.scale.y += deltaScale;
                    if (state.activeAxis == NK_GIZMO_AXIS_Z) transform.scale.z += deltaScale;
                    ApplyDeltaForAxis(state.activeAxis, deltaScale, out.deltaScale);
                }

                out.changed = true;
                out.active = true;
                out.activeAxis = state.activeAxis;
            }

            if (state.active && ctx.ConsumeMouseRelease(0)) {
                state.active = false;
                state.activeAxis = NK_GIZMO_AXIS_NONE;
                state.dragAccum = 0.f;
            }

            const NkColor colorX = (state.activeAxis == NK_GIZMO_AXIS_X || hoveredAxis == NK_GIZMO_AXIS_X) ? NkColor{245, 245, 245, 255} : NkColor{220, 84, 84, 230};
            const NkColor colorY = (state.activeAxis == NK_GIZMO_AXIS_Y || hoveredAxis == NK_GIZMO_AXIS_Y) ? NkColor{245, 245, 245, 255} : NkColor{84, 220, 120, 230};
            const NkColor colorZ = (state.activeAxis == NK_GIZMO_AXIS_Z || hoveredAxis == NK_GIZMO_AXIS_Z) ? NkColor{245, 245, 245, 255} : NkColor{84, 140, 220, 230};

            dl.PushClipRect(desc.viewport, true);
            if (cfg.axisMask & NK_GIZMO_AXIS_X) {
                dl.AddLine(origin, endX, colorX, cfg.axisThickness);
                dl.AddCircleFilled(endX, cfg.handleRadius, colorX);
                if (cfg.showAxisLabels) DrawAxisLabel(dl, {endX.x + 6.f, endX.y - 6.f}, 'X', colorX);
            }
            if (cfg.axisMask & NK_GIZMO_AXIS_Y) {
                dl.AddLine(origin, endY, colorY, cfg.axisThickness);
                dl.AddCircleFilled(endY, cfg.handleRadius, colorY);
                if (cfg.showAxisLabels) DrawAxisLabel(dl, {endY.x + 6.f, endY.y - 6.f}, 'Y', colorY);
            }
            if (cfg.axisMask & NK_GIZMO_AXIS_Z) {
                dl.AddLine(origin, endZ, colorZ, cfg.axisThickness);
                dl.AddCircleFilled(endZ, cfg.handleRadius, colorZ);
                if (cfg.showAxisLabels) DrawAxisLabel(dl, {endZ.x + 6.f, endZ.y - 6.f}, 'Z', colorZ);
            }
            if (cfg.showCenter) dl.AddCircleFilled(origin, 4.f, {240, 240, 240, 230});
            dl.PopClipRect();

            return out;
        }

    } // namespace nkui
} // namespace nkentseu

