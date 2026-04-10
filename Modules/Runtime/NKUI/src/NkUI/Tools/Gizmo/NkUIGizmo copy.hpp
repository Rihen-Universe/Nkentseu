/*
 * NkUIGizmo.cpp — v3 final
 *
 * Corrections :
 *  1. axisLength vient de cfg (NkUIGizmoConfig), PAS de desc (NkUIGizmo3DDesc)
 *  2. Comparaisons d'axes via == / != uniquement (jamais >= sur uint8 pour
 *     éviter le conflit avec operator>= de NkStringView)
 *  3. DrawGrid3D avec grille infinie (fade-out) et plan de sol plein/wireframe
 */
#include "NKUI/Tools/Gizmo/NkUIGizmo.h"
#include <cmath>
#include <cstdio>

namespace nkentseu {
    namespace nkui {

        // =====================================================================
        //  Helpers mathématiques (namespace anonyme)
        // =====================================================================
        namespace {

        static NKUI_INLINE float32 Dot2(NkVec2 a, NkVec2 b) noexcept {
            return a.x*b.x + a.y*b.y;
        }
        static NKUI_INLINE float32 Len2(NkVec2 v) noexcept {
            return ::sqrtf(v.x*v.x + v.y*v.y);
        }
        static NKUI_INLINE NkVec2 Norm2(NkVec2 v, NkVec2 fb = {1.f,0.f}) noexcept {
            const float32 l = Len2(v);
            return l > 1e-6f ? NkVec2{v.x/l, v.y/l} : fb;
        }
        static NKUI_INLINE float32 Clamp01(float32 v) noexcept {
            return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
        }
        static NKUI_INLINE bool InVP(NkRect r, NkVec2 p) noexcept {
            return p.x >= r.x && p.x <= r.x+r.w && p.y >= r.y && p.y <= r.y+r.h;
        }
        static NKUI_INLINE float32 DistSeg(NkVec2 p, NkVec2 a, NkVec2 b) noexcept {
            const NkVec2 ab{b.x-a.x, b.y-a.y};
            const float32 d = Dot2(ab, ab);
            if (d <= 1e-6f) return Len2({p.x-a.x, p.y-a.y});
            const float32 t = Clamp01(Dot2({p.x-a.x, p.y-a.y}, ab) / d);
            return Len2({p.x-(a.x+ab.x*t), p.y-(a.y+ab.y*t)});
        }
        static NKUI_INLINE float32 SnapV(float32 v, float32 step) noexcept {
            if (step <= 1e-6f) return v;
            return ::floorf(v/step + (v >= 0.f ? .5f : -.5f)) * step;
        }

        // Teste si un point est dans un quad convexe (4 sommets dans l'ordre)
        static bool PointInQuad(NkVec2 p,
                                 NkVec2 a, NkVec2 b,
                                 NkVec2 c, NkVec2 d) noexcept {
            auto cross = [](NkVec2 o, NkVec2 e, NkVec2 pt) {
                return (e.x-o.x)*(pt.y-o.y) - (e.y-o.y)*(pt.x-o.x);
            };
            const bool s0 = cross(a,b,p)>=0.f, s1 = cross(b,c,p)>=0.f;
            const bool s2 = cross(c,d,p)>=0.f, s3 = cross(d,a,p)>=0.f;
            return (s0&&s1&&s2&&s3) || (!s0&&!s1&&!s2&&!s3);
        }

        static void DrawAxisLabel(NkUIDrawList& dl, NkVec2 pos,
                                  const char* txt, NkColor col) noexcept {
            dl.AddText(pos, txt, col, 12.f);
        }

        // Interpolation couleur avec alpha pour le fade-out de la grille
        static NKUI_INLINE NkColor FadeColor(NkColor c, float32 alpha01) noexcept {
            return c.WithAlpha(static_cast<uint8>(c.a * alpha01));
        }

        } // anon namespace

        // =====================================================================
        //  DrawGrid2D
        // =====================================================================

        void NkUIGizmo::DrawGrid2D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   NkRect viewport,
                                   NkVec2 originPx) noexcept {
            if (!cfg.enabled || !cfg.showPlane2D) return;

            const float32 step = cfg.cellSizePx > 2.f ? cfg.cellSizePx : 2.f;
            const int32   n    = cfg.lineCount   > 2   ? cfg.lineCount   : 2;
            const float32 half = static_cast<float32>(n) * step * 0.5f;
            const float32 x0 = originPx.x - half, x1 = originPx.x + half;
            const float32 y0 = originPx.y - half, y1 = originPx.y + half;

            dl.PushClipRect(viewport, true);

            // Plan plein
            if (cfg.solid)
                dl.AddRectFilled({x0, y0, x1-x0, y1-y0}, cfg.solidColor, 0.f);

            // Lignes
            for (int32 i = 0; i <= n; ++i) {
                const float32 t = static_cast<float32>(i) / static_cast<float32>(n);
                const float32 x = x0 + (x1-x0)*t;
                const float32 y = y0 + (y1-y0)*t;

                float32 fade = 1.f;
                if (cfg.infinite) {
                    const float32 dist = ::fabsf(t - 0.5f) * 2.f; // 0 au centre, 1 aux bords
                    fade = 1.f - Clamp01((dist - cfg.fadeStart) / (1.f - cfg.fadeStart));
                }
                dl.AddLine({x,y0}, {x,y1}, FadeColor(cfg.lineColor, fade), 1.f);
                dl.AddLine({x0,y}, {x1,y}, FadeColor(cfg.lineColor, fade), 1.f);
            }

            // Axes
            if (cfg.showAxisX)
                dl.AddLine({viewport.x, originPx.y},
                           {viewport.x+viewport.w, originPx.y}, cfg.axisXColor, 1.5f);
            if (cfg.showAxisY)
                dl.AddLine({originPx.x, viewport.y},
                           {originPx.x, viewport.y+viewport.h}, cfg.axisYColor, 1.5f);

            dl.PopClipRect();
        }

        // =====================================================================
        //  DrawGrid3D — grille infinie avec fade-out + plan de sol solide
        // =====================================================================

        void NkUIGizmo::DrawGrid3D(const NkUIGridConfig& cfg,
                                   NkUIDrawList& dl,
                                   const NkUIGizmo3DDesc& desc,
                                   const NkUIGizmoConfig& gizmoCfg) noexcept {
            if (!cfg.enabled) return;

            const NkVec2 dx = Norm2(desc.axisXDirPx, {1.f, 0.f});
            const NkVec2 dy = Norm2(desc.axisYDirPx, {0.f,-1.f});
            const NkVec2 dz = Norm2(desc.axisZDirPx, {.7f,.5f});

            // Taille de la grille projetée en pixels
            // On utilise un multiple de axisLength pour que la grille soit
            // proportionnelle au zoom — effet "infini" par le nombre de lignes
            const float32 axLen  = gizmoCfg.axisLength;
            const float32 step   = cfg.cellSizePx > 1.f ? cfg.cellSizePx : 1.f;
            const int32   nLines = cfg.lineCount > 2 ? cfg.lineCount : 2;
            const float32 half   = static_cast<float32>(nLines) * step * 0.5f;

            dl.PushClipRect(desc.viewport, true);

            // ── Plan de sol XZ ──────────────────────────────────────────────
            if (cfg.showPlaneXZ) {

                // Si plan plein (solid), on le dessine d'abord (derrière les lignes)
                if (cfg.solid) {
                    // Quatre coins du plan projeté
                    const NkVec2 c0 {desc.originPx.x - dx.x*half - dz.x*half,
                                     desc.originPx.y - dx.y*half - dz.y*half};
                    const NkVec2 c1 {desc.originPx.x + dx.x*half - dz.x*half,
                                     desc.originPx.y + dx.y*half - dz.y*half};
                    const NkVec2 c2 {desc.originPx.x + dx.x*half + dz.x*half,
                                     desc.originPx.y + dx.y*half + dz.y*half};
                    const NkVec2 c3 {desc.originPx.x - dx.x*half + dz.x*half,
                                     desc.originPx.y - dx.y*half + dz.y*half};
                    // Deux triangles pour le quad
                    dl.AddTriangleFilled(c0, c1, c2, cfg.solidColor);
                    dl.AddTriangleFilled(c0, c2, c3, cfg.solidColor);
                }

                // Lignes de grille avec fade-out vers le bord
                for (int32 i = -nLines/2; i <= nLines/2; ++i) {
                    const float32 fi   = static_cast<float32>(i);
                    const float32 fh   = static_cast<float32>(nLines/2);
                    const float32 t    = ::fabsf(fi) / (fh > 0.f ? fh : 1.f); // 0=centre, 1=bord

                    float32 fade = 1.f;
                    if (cfg.infinite) {
                        fade = 1.f - Clamp01((t - cfg.fadeStart) / (1.f - cfg.fadeStart + 1e-4f));
                        if (fade < 0.02f) continue; // skip invisible lines
                    }
                    const NkColor lc = FadeColor(cfg.lineColor, fade);

                    // Ligne parallèle à X (à distance i * step le long de Z)
                    const float32 offZx = dz.x * fi * step;
                    const float32 offZy = dz.y * fi * step;
                    dl.AddLine(
                        {desc.originPx.x - dx.x*half + offZx,
                         desc.originPx.y - dx.y*half + offZy},
                        {desc.originPx.x + dx.x*half + offZx,
                         desc.originPx.y + dx.y*half + offZy},
                        lc, 1.f);

                    // Ligne parallèle à Z (à distance i * step le long de X)
                    const float32 offXx = dx.x * fi * step;
                    const float32 offXy = dx.y * fi * step;
                    dl.AddLine(
                        {desc.originPx.x - dz.x*half + offXx,
                         desc.originPx.y - dz.y*half + offXy},
                        {desc.originPx.x + dz.x*half + offXx,
                         desc.originPx.y + dz.y*half + offXy},
                        lc, 1.f);
                }
            }

            // ── Plan XY ──────────────────────────────────────────────────────
            if (cfg.showPlaneXY) {
                if (cfg.solid) {
                    const NkVec2 c0{desc.originPx.x - dx.x*half - dy.x*half,
                                    desc.originPx.y - dx.y*half - dy.y*half};
                    const NkVec2 c1{desc.originPx.x + dx.x*half - dy.x*half,
                                    desc.originPx.y + dx.y*half - dy.y*half};
                    const NkVec2 c2{desc.originPx.x + dx.x*half + dy.x*half,
                                    desc.originPx.y + dx.y*half + dy.y*half};
                    const NkVec2 c3{desc.originPx.x - dx.x*half + dy.x*half,
                                    desc.originPx.y - dx.y*half + dy.y*half};
                    dl.AddTriangleFilled(c0,c1,c2,cfg.solidColor);
                    dl.AddTriangleFilled(c0,c2,c3,cfg.solidColor);
                }
                for (int32 i = -nLines/2; i <= nLines/2; ++i) {
                    const float32 fi=static_cast<float32>(i), fh=static_cast<float32>(nLines/2);
                    const float32 t=::fabsf(fi)/(fh>0.f?fh:1.f);
                    float32 fade=1.f;
                    if (cfg.infinite){ fade=1.f-Clamp01((t-cfg.fadeStart)/(1.f-cfg.fadeStart+1e-4f)); if(fade<0.02f)continue; }
                    const NkColor lc=FadeColor(cfg.lineColor,fade);
                    const float32 odx=dy.x*fi*step, ody=dy.y*fi*step;
                    dl.AddLine({desc.originPx.x-dx.x*half+odx,desc.originPx.y-dx.y*half+ody},
                               {desc.originPx.x+dx.x*half+odx,desc.originPx.y+dx.y*half+ody},lc,1.f);
                    const float32 oex=dx.x*fi*step, oey=dx.y*fi*step;
                    dl.AddLine({desc.originPx.x-dy.x*half+oex,desc.originPx.y-dy.y*half+oey},
                               {desc.originPx.x+dy.x*half+oex,desc.originPx.y+dy.y*half+oey},lc,1.f);
                }
            }

            // ── Plan YZ ──────────────────────────────────────────────────────
            if (cfg.showPlaneYZ) {
                if (cfg.solid) {
                    const NkVec2 c0{desc.originPx.x-dy.x*half-dz.x*half,desc.originPx.y-dy.y*half-dz.y*half};
                    const NkVec2 c1{desc.originPx.x+dy.x*half-dz.x*half,desc.originPx.y+dy.y*half-dz.y*half};
                    const NkVec2 c2{desc.originPx.x+dy.x*half+dz.x*half,desc.originPx.y+dy.y*half+dz.y*half};
                    const NkVec2 c3{desc.originPx.x-dy.x*half+dz.x*half,desc.originPx.y-dy.y*half+dz.y*half};
                    dl.AddTriangleFilled(c0,c1,c2,cfg.solidColor);
                    dl.AddTriangleFilled(c0,c2,c3,cfg.solidColor);
                }
                for (int32 i=-nLines/2;i<=nLines/2;++i){
                    const float32 fi=static_cast<float32>(i),fh=static_cast<float32>(nLines/2);
                    const float32 t=::fabsf(fi)/(fh>0.f?fh:1.f);
                    float32 fade=1.f;
                    if(cfg.infinite){fade=1.f-Clamp01((t-cfg.fadeStart)/(1.f-cfg.fadeStart+1e-4f));if(fade<0.02f)continue;}
                    const NkColor lc=FadeColor(cfg.lineColor,fade);
                    const float32 odx=dz.x*fi*step,ody=dz.y*fi*step;
                    dl.AddLine({desc.originPx.x-dy.x*half+odx,desc.originPx.y-dy.y*half+ody},{desc.originPx.x+dy.x*half+odx,desc.originPx.y+dy.y*half+ody},lc,1.f);
                    const float32 oex=dy.x*fi*step,oey=dy.y*fi*step;
                    dl.AddLine({desc.originPx.x-dz.x*half+oex,desc.originPx.y-dz.y*half+oey},{desc.originPx.x+dz.x*half+oex,desc.originPx.y+dz.y*half+oey},lc,1.f);
                }
            }

            // ── Axes principaux (très longs — "infinis" visuellement) ────────
            const float32 axExtent = half * 1.8f; // Depasse la grille

            if (cfg.showAxisX)
                dl.AddLine({desc.originPx.x - dx.x*axExtent, desc.originPx.y - dx.y*axExtent},
                           {desc.originPx.x + dx.x*axExtent, desc.originPx.y + dx.y*axExtent},
                           cfg.axisXColor, 1.8f);
            if (cfg.showAxisY)
                dl.AddLine({desc.originPx.x - dy.x*axExtent, desc.originPx.y - dy.y*axExtent},
                           {desc.originPx.x + dy.x*axExtent, desc.originPx.y + dy.y*axExtent},
                           cfg.axisYColor, 1.8f);
            if (cfg.showAxisZ)
                dl.AddLine({desc.originPx.x - dz.x*axExtent, desc.originPx.y - dz.y*axExtent},
                           {desc.originPx.x + dz.x*axExtent, desc.originPx.y + dz.y*axExtent},
                           cfg.axisZColor, 1.8f);

            dl.PopClipRect();
            (void)axLen;
        }

        // =====================================================================
        //  Manipulate2D
        // =====================================================================

        NkUIGizmoResult NkUIGizmo::Manipulate2D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo2DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept {
            NkUIGizmoResult out{};

            if (cfg.showGrid) {
                NkUIGridConfig g{};
                DrawGrid2D(g, dl, desc.viewport, desc.originPx);
            }

            const NkVec2 origin {desc.originPx.x + transform.position.x,
                                  desc.originPx.y + transform.position.y};
            const float32 axLen = cfg.axisLength > 8.f ? cfg.axisLength : 8.f;
            const NkVec2 endX   {origin.x + axLen, origin.y};
            const NkVec2 endY   {origin.x,          origin.y - axLen};
            const bool axX = (cfg.axisMask & NK_GIZMO_AXIS_X) != 0;
            const bool axY = (cfg.axisMask & NK_GIZMO_AXIS_Y) != 0;

            uint8 hovAxis = NK_GIZMO_AXIS_NONE;
            if (!state.active && InVP(desc.viewport, ctx.input.mousePos)) {
                if (axX && DistSeg(ctx.input.mousePos, origin, endX) <= 8.f)
                    hovAxis = NKGIZMO_AX_X;
                if (axY && DistSeg(ctx.input.mousePos, origin, endY) <= 8.f)
                    hovAxis = NKGIZMO_AX_Y;
            }
            state.hovered = (hovAxis != NK_GIZMO_AXIS_NONE) || state.active;
            out.hovered   = state.hovered;

            if (!state.active && hovAxis != NK_GIZMO_AXIS_NONE && ctx.ConsumeMouseClick(0)) {
                state.active = true; state.activeAxis = hovAxis;
                state.dragStartPx = ctx.input.mousePos; state.dragAccum = 0.f;
                state.accumX = state.accumY = state.accumZ = 0.f;
                state.snapAccumX = state.snapAccumY = state.snapAccumZ = 0.f;
            }

            if (state.active && ctx.input.mouseDown[0]) {
                const NkVec2 dir = (state.activeAxis == NKGIZMO_AX_X)
                    ? NkVec2{1.f, 0.f} : NkVec2{0.f, -1.f};
                float32 d = Dot2(ctx.input.mouseDelta, dir);

                if (cfg.mode == NkUIGizmoMode::NK_TRANSLATE) {
                    if (cfg.snap.enabled) {
                        float32& acc = (state.activeAxis==NKGIZMO_AX_X)
                                     ? state.snapAccumX : state.snapAccumY;
                        acc += d; const float32 s = SnapV(acc, cfg.snap.translateStep);
                        d = s-(acc-d); acc = s;
                    }
                    if (state.activeAxis==NKGIZMO_AX_X) {
                        transform.position.x += d; out.deltaPosition.x = d;
                    } else {
                        transform.position.y -= d; out.deltaPosition.y = -d;
                    }
                } else if (cfg.mode == NkUIGizmoMode::NK_ROTATE) {
                    float32 deg = d * 0.8f;
                    if (cfg.snap.enabled) {
                        state.dragAccum += deg;
                        const float32 s=SnapV(state.dragAccum,cfg.snap.rotateStepDeg);
                        deg=s-(state.dragAccum-deg); state.dragAccum=s;
                    }
                    out.deltaRotationDeg.z = deg;
                } else {
                    float32 sc = d * 0.01f;
                    if (cfg.snap.enabled) {
                        state.dragAccum += sc;
                        const float32 s=SnapV(state.dragAccum,cfg.snap.scaleStep);
                        sc=s-(state.dragAccum-sc); state.dragAccum=s;
                    }
                    if (state.activeAxis==NKGIZMO_AX_X) out.deltaScale.x=sc;
                    else out.deltaScale.y=sc;
                }
                out.changed=true; out.active=true; out.activeAxis=state.activeAxis;
            }

            if (state.active && !ctx.input.mouseDown[0]) {
                state.active=false; state.activeAxis=NK_GIZMO_AXIS_NONE;
                state.dragAccum=0.f;
            }

            const NkColor cX=(state.activeAxis==NKGIZMO_AX_X||hovAxis==NKGIZMO_AX_X)
                ?cfg.colorHL:cfg.colorX;
            const NkColor cY=(state.activeAxis==NKGIZMO_AX_Y||hovAxis==NKGIZMO_AX_Y)
                ?cfg.colorHL:cfg.colorY;

            dl.PushClipRect(desc.viewport, true);
            if (axX) {
                dl.AddLine(origin, endX, cX, cfg.axisThickness);
                dl.AddCircleFilled(endX, cfg.handleRadius, cX);
                if (cfg.showAxisLabels) DrawAxisLabel(dl,{endX.x+7.f,endX.y-7.f},"X",cX);
            }
            if (axY) {
                dl.AddLine(origin, endY, cY, cfg.axisThickness);
                dl.AddCircleFilled(endY, cfg.handleRadius, cY);
                if (cfg.showAxisLabels) DrawAxisLabel(dl,{endY.x+7.f,endY.y-7.f},"Y",cY);
            }
            if (cfg.showCenter)
                dl.AddCircleFilled(origin, 4.5f, {240,240,240,230});
            dl.PopClipRect();
            return out;
        }

        // =====================================================================
        //  Manipulate3D — axes gelés, longueur fixe depuis cfg.axisLength
        // =====================================================================

        NkUIGizmoResult NkUIGizmo::Manipulate3D(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                const NkUIGizmo3DDesc& desc,
                                                const NkUIGizmoConfig& cfg,
                                                NkUIGizmoState& state,
                                                NkUIGizmoTransform& transform) noexcept
        {
            NkUIGizmoResult out{};

            // ── Grille ────────────────────────────────────────────────────
            if (cfg.showGrid) {
                NkUIGridConfig g{};
                g.showPlaneXZ = true;
                g.showAxisX = g.showAxisY = g.showAxisZ = true;
                g.infinite  = true;
                g.lineCount  = 40;
                g.cellSizePx = 24.f;
                DrawGrid3D(g, dl, desc, cfg);
            }

            // ── Longueur d'axe — depuis cfg, FIXE en pixels ───────────────
            // (desc n'a PAS de axisLength — correction de la v2)
            const float32 axLen = cfg.axisLength > 8.f ? cfg.axisLength : 8.f;

            // ── Directions courantes (normalisées depuis la caméra) ────────
            const NkVec2 rawDX = Norm2(desc.axisXDirPx, {1.f, 0.f});
            const NkVec2 rawDY = Norm2(desc.axisYDirPx, {0.f,-1.f});
            const NkVec2 rawDZ = Norm2(desc.axisZDirPx, {.7f,.5f});

            // ── Axes utilisés : gelés si drag actif, courants sinon ────────
            //
            // RÈGLE FONDAMENTALE : les axes sont snapshotés au premier clic
            // et ne bougent plus jusqu'à la fin du drag (même si la caméra tourne).
            //
            const NkVec2 dx = state.active ? state.frozenAxisX : rawDX;
            const NkVec2 dy = state.active ? state.frozenAxisY : rawDY;
            const NkVec2 dz = state.active ? state.frozenAxisZ : rawDZ;

            // Origine gelée (évite le saut quand l'app met à jour la position)
            const NkVec2 O = state.active ? state.frozenOrigin : desc.originPx;

            // Extrémités des axes (longueur FIXE = cfg.axisLength px)
            const NkVec2 endX {O.x + dx.x*axLen, O.y + dx.y*axLen};
            const NkVec2 endY {O.x + dy.x*axLen, O.y + dy.y*axLen};
            const NkVec2 endZ {O.x + dz.x*axLen, O.y + dz.y*axLen};

            // Plans de translation (taille proportionnelle à axLen)
            const float32 pSz = axLen * cfg.planeSize;

            const NkVec2 pXY0=O, pXY1{O.x+dx.x*pSz,O.y+dx.y*pSz},
                         pXY2{O.x+dx.x*pSz+dy.x*pSz,O.y+dx.y*pSz+dy.y*pSz},
                         pXY3{O.x+dy.x*pSz,O.y+dy.y*pSz};

            const NkVec2 pXZ0=O, pXZ1{O.x+dx.x*pSz,O.y+dx.y*pSz},
                         pXZ2{O.x+dx.x*pSz+dz.x*pSz,O.y+dx.y*pSz+dz.y*pSz},
                         pXZ3{O.x+dz.x*pSz,O.y+dz.y*pSz};

            const NkVec2 pYZ0=O, pYZ1{O.x+dy.x*pSz,O.y+dy.y*pSz},
                         pYZ2{O.x+dy.x*pSz+dz.x*pSz,O.y+dy.y*pSz+dz.y*pSz},
                         pYZ3{O.x+dz.x*pSz,O.y+dz.y*pSz};

            const float32 cR = cfg.handleRadius;

            // ── Hover ─────────────────────────────────────────────────────
            uint8 hovAxis = NK_GIZMO_AXIS_NONE;
            const NkVec2 mp = ctx.input.mousePos;

            if (!state.active && InVP(desc.viewport, mp)) {
                const float32 pr = cR * 2.2f;

                // Plans (priorité haute si translate)
                if (cfg.showPlanes && cfg.mode == NkUIGizmoMode::NK_TRANSLATE) {
                    if      (PointInQuad(mp,pXY0,pXY1,pXY2,pXY3)) hovAxis = NKGIZMO_AX_XY;
                    else if (PointInQuad(mp,pXZ0,pXZ1,pXZ2,pXZ3)) hovAxis = NKGIZMO_AX_XZ;
                    else if (PointInQuad(mp,pYZ0,pYZ1,pYZ2,pYZ3)) hovAxis = NKGIZMO_AX_YZ;
                }

                // Centre translate libre
                if (hovAxis == NK_GIZMO_AXIS_NONE &&
                    cfg.mode == NkUIGizmoMode::NK_TRANSLATE && cfg.showCenterMove &&
                    Len2({mp.x-O.x,mp.y-O.y}) < cR*2.f)
                    hovAxis = NKGIZMO_AX_UNI;

                // Centre scale uniforme
                if (hovAxis == NK_GIZMO_AXIS_NONE &&
                    cfg.mode == NkUIGizmoMode::NK_SCALE && cfg.showCenterScale) {
                    const float32 hs = cR*1.8f;
                    if (mp.x>=O.x-hs && mp.x<=O.x+hs && mp.y>=O.y-hs && mp.y<=O.y+hs)
                        hovAxis = NKGIZMO_AX_UNI;
                }

                // Axes simples
                if (hovAxis == NK_GIZMO_AXIS_NONE) {
                    if ((cfg.axisMask & NK_GIZMO_AXIS_X) && DistSeg(mp,O,endX)<=pr)
                        hovAxis = NKGIZMO_AX_X;
                    else if ((cfg.axisMask & NK_GIZMO_AXIS_Y) && DistSeg(mp,O,endY)<=pr)
                        hovAxis = NKGIZMO_AX_Y;
                    else if ((cfg.axisMask & NK_GIZMO_AXIS_Z) && DistSeg(mp,O,endZ)<=pr)
                        hovAxis = NKGIZMO_AX_Z;
                }

                // Arcs de rotation
                if (cfg.mode == NkUIGizmoMode::NK_ROTATE && hovAxis == NK_GIZMO_AXIS_NONE) {
                    const float32 arcR = axLen * 0.82f;
                    auto hArc = [&](NkVec2 ad, uint8 ax2) {
                        if (!(cfg.axisMask & ax2)) return;
                        const NkVec2 perp{-ad.y,ad.x};
                        const NkVec2 ac{O.x+perp.x*arcR*0.18f, O.y+perp.y*arcR*0.18f};
                        if (::fabsf(Len2({mp.x-ac.x,mp.y-ac.y}) - arcR*0.82f) < pr*1.5f)
                            hovAxis = ax2;
                    };
                    hArc(dx, NKGIZMO_AX_X);
                    if (hovAxis == NK_GIZMO_AXIS_NONE) hArc(dy, NKGIZMO_AX_Y);
                    if (hovAxis == NK_GIZMO_AXIS_NONE) hArc(dz, NKGIZMO_AX_Z);
                }
            }

            state.hovered = (hovAxis != NK_GIZMO_AXIS_NONE) || state.active;
            out.hovered   = state.hovered;

            // ── Début du drag — GEL des axes ──────────────────────────────
            if (!state.active && hovAxis != NK_GIZMO_AXIS_NONE && ctx.ConsumeMouseClick(0)) {
                state.active      = true;
                state.activeAxis  = hovAxis;
                state.dragStartPx = mp;
                state.dragAccum   = 0.f;

                // Snapshot des directions courantes
                state.frozenAxisX  = rawDX;
                state.frozenAxisY  = rawDY;
                state.frozenAxisZ  = rawDZ;
                state.frozenOrigin = desc.originPx;

                state.accumX = state.accumY = state.accumZ = 0.f;
                state.snapAccumX = state.snapAccumY = state.snapAccumZ = 0.f;
            }

            // ── Drag en cours ─────────────────────────────────────────────
            if (state.active && ctx.input.mouseDown[0]) {
                const uint8   ax  = state.activeAxis;
                const float32 upp = desc.unitsPerPixel > 1e-9f ? desc.unitsPerPixel : 0.014f;
                const NkVec2  dm  = ctx.input.mouseDelta;

                if (cfg.mode == NkUIGizmoMode::NK_TRANSLATE) {

                    float32 wx=0.f, wy=0.f, wz=0.f;

                    if (ax == NKGIZMO_AX_UNI) {
                        wx = Dot2(dm, state.frozenAxisX) * upp;
                        wz = Dot2(dm, state.frozenAxisZ) * upp;
                    } else if (ax == NKGIZMO_AX_XY) {
                        wx = Dot2(dm, state.frozenAxisX) * upp;
                        wy = Dot2(dm, state.frozenAxisY) * upp;
                    } else if (ax == NKGIZMO_AX_XZ) {
                        wx = Dot2(dm, state.frozenAxisX) * upp;
                        wz = Dot2(dm, state.frozenAxisZ) * upp;
                    } else if (ax == NKGIZMO_AX_YZ) {
                        wy = Dot2(dm, state.frozenAxisY) * upp;
                        wz = Dot2(dm, state.frozenAxisZ) * upp;
                    } else {
                        const NkVec2 axDir = (ax==NKGIZMO_AX_X) ? state.frozenAxisX
                                           : (ax==NKGIZMO_AX_Y) ? state.frozenAxisY
                                                                  : state.frozenAxisZ;
                        const float32 px = Dot2(dm, axDir) * upp;
                        if      (ax==NKGIZMO_AX_X) wx=px;
                        else if (ax==NKGIZMO_AX_Y) wy=px;
                        else                        wz=px;
                    }

                    if (cfg.snap.enabled) {
                        auto snap1=[&](float32& v, float32& acc){
                            acc+=v;
                            const float32 s=SnapV(acc,cfg.snap.translateStep);
                            v=s-(acc-v); acc=s;
                        };
                        snap1(wx,state.snapAccumX);
                        snap1(wy,state.snapAccumY);
                        snap1(wz,state.snapAccumZ);
                    }

                    out.deltaPosition = {wx,wy,wz};
                    state.accumX+=wx; state.accumY+=wy; state.accumZ+=wz;
                    out.totalPosition = {state.accumX,state.accumY,state.accumZ};
                }
                else if (cfg.mode == NkUIGizmoMode::NK_ROTATE) {
                    // Direction de l'axe concerné (gelé)
                    const NkVec2 axDir = (ax==NKGIZMO_AX_X) ? state.frozenAxisX
                                       : (ax==NKGIZMO_AX_Y) ? state.frozenAxisY
                                                              : state.frozenAxisZ;
                    // Tangente à l'arc (perpendiculaire à l'axe)
                    const NkVec2 tang{-axDir.y, axDir.x};
                    float32 deg = Dot2(dm, tang) * 0.35f;

                    if (cfg.snap.enabled) {
                        state.dragAccum+=deg;
                        const float32 s=SnapV(state.dragAccum,cfg.snap.rotateStepDeg);
                        deg=s-(state.dragAccum-deg); state.dragAccum=s;
                    }

                    if      (ax==NKGIZMO_AX_X) { out.deltaRotationDeg.x=deg; state.accumX+=deg; transform.rotationDeg.x+=deg; out.totalRotationDeg.x=state.accumX; }
                    else if (ax==NKGIZMO_AX_Y) { out.deltaRotationDeg.y=deg; state.accumY+=deg; transform.rotationDeg.y+=deg; out.totalRotationDeg.y=state.accumY; }
                    else                        { out.deltaRotationDeg.z=deg; state.accumZ+=deg; transform.rotationDeg.z+=deg; out.totalRotationDeg.z=state.accumZ; }
                }
                else { // Scale
                    const bool unif = (ax == NKGIZMO_AX_UNI);
                    float32 sx=0.f, sy=0.f, sz=0.f;
                    const float32 kSc = 0.005f;

                    if (unif) {
                        const NkVec2 fromStart{mp.x-state.frozenOrigin.x, mp.y-state.frozenOrigin.y};
                        const NkVec2 prev{mp.x-dm.x-state.frozenOrigin.x, mp.y-dm.y-state.frozenOrigin.y};
                        const float32 delta=(Len2(fromStart)-Len2(prev))*kSc;
                        sx=sy=sz=delta;
                    } else {
                        const NkVec2 axDir=(ax==NKGIZMO_AX_X)?state.frozenAxisX
                                          :(ax==NKGIZMO_AX_Y)?state.frozenAxisY
                                          :state.frozenAxisZ;
                        const float32 px=Dot2(dm,axDir)*kSc;
                        if      (ax==NKGIZMO_AX_X) sx=px;
                        else if (ax==NKGIZMO_AX_Y) sy=px;
                        else                        sz=px;
                    }

                    if (cfg.snap.enabled && unif) {
                        state.dragAccum+=sx;
                        const float32 s=SnapV(state.dragAccum,cfg.snap.scaleStep);
                        sx=sy=sz=s-(state.dragAccum-sx); state.dragAccum=s;
                    } else if (cfg.snap.enabled) {
                        auto sn=[&](float32& v,float32& acc){acc+=v;const float32 s=SnapV(acc,cfg.snap.scaleStep);v=s-(acc-v);acc=s;};
                        sn(sx,state.snapAccumX); sn(sy,state.snapAccumY); sn(sz,state.snapAccumZ);
                    }

                    out.deltaScale={sx,sy,sz};
                    state.accumX+=sx; state.accumY+=sy; state.accumZ+=sz;
                    out.totalScale={state.accumX,state.accumY,state.accumZ};
                }

                out.changed=true; out.active=true; out.activeAxis=ax;
            }

            // ── Fin du drag ───────────────────────────────────────────────
            if (state.active && !ctx.input.mouseDown[0]) {
                state.active=false; state.activeAxis=NK_GIZMO_AXIS_NONE;
                state.dragAccum=0.f;
                state.snapAccumX=state.snapAccumY=state.snapAccumZ=0.f;
            }

            // ── Couleurs ──────────────────────────────────────────────────
            auto hlC=[&](uint8 ax2, NkColor base)->NkColor{
                return (state.activeAxis==ax2||hovAxis==ax2)?cfg.colorHL:base;
            };
            const NkColor cX   = hlC(NKGIZMO_AX_X,  cfg.colorX);
            const NkColor cY   = hlC(NKGIZMO_AX_Y,  cfg.colorY);
            const NkColor cZ   = hlC(NKGIZMO_AX_Z,  cfg.colorZ);
            const NkColor cXY  = hlC(NKGIZMO_AX_XY, {220,220,80,200});
            const NkColor cXZ  = hlC(NKGIZMO_AX_XZ, {80,220,220,200});
            const NkColor cYZ  = hlC(NKGIZMO_AX_YZ, {220,80,220,200});
            const NkColor cU   = hlC(NKGIZMO_AX_UNI,{240,240,240,220});

            // ── Helpers dessin ────────────────────────────────────────────

            auto arrowHead=[&](NkVec2 tip, NkVec2 dir, NkColor col){
                const float32 hs=cR*2.4f, hw=cR*1.0f;
                const NkVec2 back{tip.x-dir.x*hs,tip.y-dir.y*hs};
                const NkVec2 perp{-dir.y,dir.x};
                dl.AddTriangleFilled(tip,{back.x+perp.x*hw,back.y+perp.y*hw},
                                         {back.x-perp.x*hw,back.y-perp.y*hw},col);
            };

            auto scaleBox=[&](NkVec2 tip, NkColor col){
                const float32 hs=cR*1.4f;
                dl.AddRectFilled({tip.x-hs,tip.y-hs,hs*2.f,hs*2.f},col,2.f);
                dl.AddRect({tip.x-hs,tip.y-hs,hs*2.f,hs*2.f},{255,255,255,40},1.f,2.f);
            };

            auto drawPlane=[&](NkVec2 a,NkVec2 b,NkVec2 c,NkVec2 d,
                               NkColor col, uint8 pid){
                const bool act=(hovAxis==pid||state.activeAxis==pid);
                dl.AddTriangleFilled(a,b,c,col.WithAlpha(act?90u:45u));
                dl.AddTriangleFilled(a,c,d,col.WithAlpha(act?90u:45u));
                const uint8 la=act?210u:130u;
                dl.AddLine(a,b,col.WithAlpha(la),1.2f);
                dl.AddLine(b,c,col.WithAlpha(la),1.2f);
                dl.AddLine(c,d,col.WithAlpha(la),1.2f);
            };

            // Arc de rotation stable
            auto rotArc=[&](NkVec2 axDir, NkColor col, uint8 ax2){
                if (!(cfg.axisMask & ax2)) return;
                const float32 arcR=axLen*0.82f;
                const NkVec2 perp{-axDir.y,axDir.x};
                const NkVec2 ac{O.x+perp.x*arcR*0.18f,O.y+perp.y*arcR*0.18f};
                const float32 baseA=::atan2f(axDir.y,axDir.x)+NKUI_PI*0.5f;
                const float32 sweep=NKUI_PI*1.15f;
                dl.AddArc(ac,arcR*0.82f,baseA-sweep*0.5f,baseA+sweep*0.5f,col,cfg.axisThickness+1.f,24);
                const float32 tipA=baseA+sweep*0.5f;
                const NkVec2 arcTip{ac.x+::cosf(tipA)*arcR*0.82f,ac.y+::sinf(tipA)*arcR*0.82f};
                arrowHead(arcTip,{-::sinf(tipA),::cosf(tipA)},col);

                // Angle affiché pendant le drag
                if (cfg.showRotationAngle && state.active && state.activeAxis==ax2) {
                    const float32 totalDeg=(ax2==NKGIZMO_AX_X)?state.accumX
                                          :(ax2==NKGIZMO_AX_Y)?state.accumY:state.accumZ;
                    char buf[32]; ::snprintf(buf,sizeof(buf),"%.1f\xc2\xb0",totalDeg);
                    const float32 tw=static_cast<float32>(::strlen(buf))*7.f;
                    dl.AddRectFilled({arcTip.x+8.f,arcTip.y-10.f,tw+6.f,16.f},{0,0,0,160},3.f);
                    dl.AddText({arcTip.x+11.f,arcTip.y-2.f},buf,col,11.f);
                    // Arc de progression
                    if (::fabsf(totalDeg) > 0.5f) {
                        const float32 ps=totalDeg*(NKUI_PI/180.f);
                        const float32 p0=baseA-ps*0.5f, p1=baseA+ps*0.5f;
                        dl.AddArc(ac,arcR*0.82f,(p0<p1?p0:p1),(p0<p1?p1:p0),
                                  col.WithAlpha(160),cfg.axisThickness+3.f,24);
                    }
                }
            };

            // Snap grid visuel
            auto drawSnapGrid=[&](){
                if (!cfg.showSnapGrid||!cfg.snap.enabled||!state.active) return;
                if (cfg.mode!=NkUIGizmoMode::NK_TRANSLATE) return;
                const uint8 ax2=state.activeAxis;
                // Seulement pour les axes simples — comparaisons explicites
                if (ax2!=NKGIZMO_AX_X && ax2!=NKGIZMO_AX_Y && ax2!=NKGIZMO_AX_Z) return;
                const NkVec2 axDir=(ax2==NKGIZMO_AX_X)?state.frozenAxisX
                                  :(ax2==NKGIZMO_AX_Y)?state.frozenAxisY
                                  :state.frozenAxisZ;
                const NkVec2 perp{-axDir.y,axDir.x};
                const float32 snapPx=cfg.snap.translateStep/desc.unitsPerPixel;
                const float32 range=axLen*2.2f;
                const int32 nL=static_cast<int32>(range/snapPx)+1;
                for (int32 i=-nL;i<=nL;++i){
                    const float32 t=static_cast<float32>(i)*snapPx;
                    const NkVec2 lp{O.x+axDir.x*t,O.y+axDir.y*t};
                    const float32 fade=1.f-::fabsf(static_cast<float32>(i))/static_cast<float32>(nL+1);
                    const float32 arm=18.f*fade;
                    dl.AddLine({lp.x-perp.x*arm,lp.y-perp.y*arm},
                               {lp.x+perp.x*arm,lp.y+perp.y*arm},
                               {200,200,200,static_cast<uint8>(fade*80.f)},0.8f);
                }
            };

            // ── Dessin ────────────────────────────────────────────────────
            dl.PushClipRect(desc.viewport, true);

            if (cfg.mode == NkUIGizmoMode::NK_ROTATE) {
                // Cercle de référence externe (style Blender)
                dl.AddCircle(O, axLen*0.90f, {120,120,120,45}, 1.f, 40);
                // Arcs — dessinés en ordre de profondeur approximatif (Z, Y, X)
                rotArc(dz, cZ, NKGIZMO_AX_Z);
                rotArc(dy, cY, NKGIZMO_AX_Y);
                rotArc(dx, cX, NKGIZMO_AX_X);
            }
            else if (cfg.mode == NkUIGizmoMode::NK_SCALE) {
                if (cfg.axisMask & NK_GIZMO_AXIS_X) {
                    dl.AddLine(O,endX,cX,cfg.axisThickness);
                    scaleBox(endX,cX);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endX.x+8.f,endX.y-8.f},"X",cX);
                }
                if (cfg.axisMask & NK_GIZMO_AXIS_Y) {
                    dl.AddLine(O,endY,cY,cfg.axisThickness);
                    scaleBox(endY,cY);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endY.x+8.f,endY.y-8.f},"Y",cY);
                }
                if (cfg.axisMask & NK_GIZMO_AXIS_Z) {
                    dl.AddLine(O,endZ,cZ,cfg.axisThickness);
                    scaleBox(endZ,cZ);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endZ.x+8.f,endZ.y-8.f},"Z",cZ);
                }
                // Carré central scale uniforme
                if (cfg.showCenterScale) {
                    const float32 hs=cR*1.8f;
                    dl.AddRectFilled({O.x-hs,O.y-hs,hs*2.f,hs*2.f},cU,3.f);
                    dl.AddRect({O.x-hs,O.y-hs,hs*2.f,hs*2.f},{255,255,255,60},1.f,3.f);
                }
            }
            else { // TRANSLATE
                // Plans (dessinés avant les axes pour rester derrière)
                if (cfg.showPlanes) {
                    drawPlane(pXZ0,pXZ1,pXZ2,pXZ3, cXZ, NKGIZMO_AX_XZ);
                    drawPlane(pXY0,pXY1,pXY2,pXY3, cXY, NKGIZMO_AX_XY);
                    drawPlane(pYZ0,pYZ1,pYZ2,pYZ3, cYZ, NKGIZMO_AX_YZ);
                }

                drawSnapGrid();

                if (cfg.axisMask & NK_GIZMO_AXIS_X) {
                    dl.AddLine(O,endX,cX,cfg.axisThickness);
                    arrowHead(endX,dx,cX);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endX.x+8.f,endX.y-8.f},"X",cX);
                }
                if (cfg.axisMask & NK_GIZMO_AXIS_Y) {
                    dl.AddLine(O,endY,cY,cfg.axisThickness);
                    arrowHead(endY,dy,cY);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endY.x+8.f,endY.y-8.f},"Y",cY);
                }
                if (cfg.axisMask & NK_GIZMO_AXIS_Z) {
                    dl.AddLine(O,endZ,cZ,cfg.axisThickness);
                    arrowHead(endZ,dz,cZ);
                    if (cfg.showAxisLabels) DrawAxisLabel(dl,{endZ.x+8.f,endZ.y-8.f},"Z",cZ);
                }

                // Cercle central translate libre
                if (cfg.showCenterMove) {
                    dl.AddCircleFilled(O,cR*1.6f,cU,14);
                    dl.AddCircle(O,cR*1.6f,{120,120,120,140},1.f,14);
                }
            }

            // Point d'origine (toujours au-dessus)
            if (cfg.showCenter) {
                dl.AddCircleFilled(O,5.5f,{240,240,240,240},14);
                dl.AddCircle(O,5.5f,{90,90,90,160},1.f,14);
            }

            dl.PopClipRect();
            return out;
        }

    } // namespace nkui
} // namespace nkentseu