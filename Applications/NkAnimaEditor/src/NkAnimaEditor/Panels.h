#pragma once
// =============================================================================
// Panels.h — Panneaux NkAnimaEditor : Timeline + Preview squelette 2D.
// N'inclut QUE l'Editor Kit + AnimBridge (types foundation) -> pas de conflit
// NKRenderer/NKCanvas. Toute l'anim passe par les fonctions nkanima::Anim*.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "AnimBridge.h"
#include <cmath>
#include <cstdio>

namespace nkanima {

    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;          // NkRect / NkColor / NkVec2 / draw list

    // ── Timeline : scrubbing + poses-clés + insert/delete/move + undo/redo ───────
    class TimelinePanel : public NkEditorPanel {
    public:
        TimelinePanel() : NkEditorPanel("Timeline", NkEditorDockSide::NK_BOTTOM) {}

        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            if (!AnimLoaded()) { ec.Text("(aucun clip charge)"); return; }

            AnimUpdate(ec.dt);   // avance la lecture si en cours

            // Barre d'outils (une seule ligne via SameLine)
            bool bPlay=ec.Button(AnimIsPlaying()?"Pause":"Play"); ctx.SameLine();
            bool bIns =ec.Button("Inserer cle");                 ctx.SameLine();
            bool bDel =ec.Button("Supprimer");                   ctx.SameLine();
            bool bUn  =ec.Button("Annuler");                     ctx.SameLine();
            bool bRe  =ec.Button("Refaire");
            if (bPlay) AnimSetPlaying(!AnimIsPlaying());
            if (bIns)  AnimInsertKeyAtCursor();
            if (bDel)  AnimDeleteSelected();
            if (bUn)   AnimUndo();
            if (bRe)   AnimRedo();
            char info[112];
            float32 t = AnimCursor(), dur = AnimDuration();
            std::snprintf(info, sizeof(info), "t=%.3fs / %.2fs  frame %d  cles=%u",
                          (double)t, (double)dur, (int)(t*AnimFps()+0.5f), AnimKeyCount());
            ec.Text(info);
            ec.Separator();

            // Zone timeline
            const NkRect area = ctx.NextItemRect(900.f, 64.f);
            auto& dl = ctx.DL();
            dl.AddRectFilled(area, NkColor{20, 23, 30, 255}, 4.f);
            dl.AddRect(area, NkColor{60, 66, 82, 255}, 1.f);
            if (dur <= 1e-4f) return;

            const float32 pad = 8.f;
            const float32 x0 = area.x + pad, x1 = area.x + area.w - pad;
            const float32 ymid = area.y + area.h * 0.5f;
            auto timeToX = [&](float32 tt){ return x0 + (x1 - x0) * (tt / dur); };
            auto xToTime = [&](float32 xx){ float32 a=(xx-x0)/(x1-x0); if(a<0)a=0; if(a>1)a=1; return a*dur; };

            for (int32 s=0; (float32)s<=dur+1e-3f; ++s) {
                float32 gx = timeToX((float32)s);
                dl.AddLine({gx, area.y+area.h-14.f}, {gx, area.y+area.h-4.f}, NkColor{70,76,92,200}, 1.f);
            }
            dl.AddLine({x0, ymid}, {x1, ymid}, NkColor{45, 50, 62, 255}, 1.f);

            NkVector<float32> keys; AnimGetKeyTimes(keys);
            for (uint32 i=0;i<(uint32)keys.Size();++i) {
                float32 kx = timeToX(keys[i]);
                bool sel = AnimIsSelected(keys[i]);
                dl.AddCircleFilled({kx, ymid}, 5.f, sel ? NkColor{255,170,40,255} : NkColor{90,150,230,255});
            }

            float32 px = timeToX(AnimCursor());
            dl.AddLine({px, area.y+2.f}, {px, area.y+area.h-2.f}, NkColor{240, 80, 80, 255}, 2.f);
            dl.AddCircleFilled({px, area.y+6.f}, 4.f, NkColor{240, 80, 80, 255});

            // Interaction souris
            const NkVec2 m = ctx.input.mousePos;
            if (ctx.IsHovered(area) && ctx.input.mouseClicked[0]) {
                int32 hit=-1; float32 best=9.f;
                for (uint32 i=0;i<(uint32)keys.Size();++i){ float32 d=timeToX(keys[i])-m.x; if(d<0)d=-d; if(d<best){best=d;hit=(int32)i;} }
                if (hit>=0) { AnimSelectKey(keys[(uint32)hit]); mDragKey=keys[(uint32)hit]; mDragging=true; }
                else        { AnimClearSelection(); AnimSeek(xToTime(m.x)); mScrubbing=true; }
            }
            if (ctx.input.mouseDown[0]) {
                if (mScrubbing) AnimSeek(xToTime(m.x));
            } else {
                if (mDragging && mDragKey>=0.f) { float32 nt=xToTime(m.x);
                    if (std::fabs(nt-mDragKey)>1e-4f) AnimMoveKey(mDragKey, nt); }
                mDragging=false; mScrubbing=false; mDragKey=-1.f;
            }
        }
    private:
        bool    mScrubbing=false, mDragging=false;
        float32 mDragKey=-1.f;
    };

    // ── Preview : squelette 2D à la pose courante ────────────────────────────────
    class PreviewPanel : public NkEditorPanel {
    public:
        PreviewPanel() : NkEditorPanel("Apercu", NkEditorDockSide::NK_CENTER) {}

        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            ec.Text("Apercu squelette 2D — Pose Mode : clic = os, drag = IK, Enregistrer = cle");
            // Barre : Mode Pose (toggle) + Enregistrer la pose.
            bool bMode = ec.Button(AnimInPoseEdit() ? "Mode Pose: ON" : "Mode Pose: OFF"); ctx.SameLine();
            bool bRec  = ec.Button("Enregistrer pose");
            if (bMode) { if (AnimInPoseEdit()) AnimEndPoseEdit(); else AnimBeginPoseEdit(); }
            if (bRec)  AnimCommitPoseKey();

            const NkRect area = ctx.NextItemRect(560.f, 420.f);
            auto& dl = ctx.DL();
            dl.AddRectFilled(area, NkColor{16, 18, 24, 255}, 4.f);
            dl.AddRect(area, AnimInPoseEdit() ? NkColor{255,170,40,255} : NkColor{60, 66, 82, 255}, 1.f);
            if (!AnimLoaded()) return;

            NkVector<NkVec3f> pos; NkVector<int32> par;
            AnimGetSkeleton(pos, par);
            uint32 jc = (uint32)pos.Size(); if (jc==0) return;

            float32 minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
            for (uint32 j=0;j<jc;++j){ if(pos[j].x<minx)minx=pos[j].x; if(pos[j].x>maxx)maxx=pos[j].x;
                                       if(pos[j].y<miny)miny=pos[j].y; if(pos[j].y>maxy)maxy=pos[j].y; }
            float32 w=maxx-minx, h=maxy-miny; if(w<1e-4f)w=1.f; if(h<1e-4f)h=1.f;
            float32 padp=30.f;
            float32 sc=nkentseu::math::NkMin((area.w-2*padp)/w, (area.h-2*padp)/h);
            float32 cx=(minx+maxx)*0.5f, cy=(miny+maxy)*0.5f;
            float32 ox=area.x+area.w*0.5f, oy=area.y+area.h*0.5f;
            auto proj   =[&](const NkVec3f& p)->NkVec2{ return { ox+(p.x-cx)*sc, oy-(p.y-cy)*sc }; };
            auto unprojX=[&](float32 sx){ return cx + (sx-ox)/sc; };
            auto unprojY=[&](float32 sy){ return cy - (sy-oy)/sc; };

            for (uint32 j=0;j<jc;++j){
                NkVec2 sp=proj(pos[j]); int32 p=par[j];
                if (p>=0 && p<(int32)jc) dl.AddLine(proj(pos[(uint32)p]), sp, NkColor{120,180,255,255}, 2.f);
                bool sel = ((int32)j==mSel);
                dl.AddCircleFilled(sp, sel?6.f:3.f, sel ? NkColor{255,90,90,255} : NkColor{255,200,80,255});
            }

            // ── Interaction Pose Mode ─────────────────────────────────────────────
            if (!AnimInPoseEdit()) { mDragging=false; return; }
            const NkVec2 m = ctx.input.mousePos;
            if (ctx.IsHovered(area) && ctx.input.mouseClicked[0]) {
                int32 hit=-1; float32 best=14.f;
                for (uint32 j=0;j<jc;++j){ NkVec2 sp=proj(pos[j]); float32 dx=sp.x-m.x, dy=sp.y-m.y; float32 d=sqrtf(dx*dx+dy*dy); if(d<best){best=d;hit=(int32)j;} }
                if (hit>=0) { mSel=hit; mDragging=true; mDragZ = pos[(uint32)hit].z; }
            }
            if (ctx.input.mouseDown[0]) {
                if (mDragging && mSel>=0) AnimDragJoint(mSel, unprojX(m.x), unprojY(m.y), mDragZ);
            } else mDragging=false;
        }
    private:
        int32   mSel = -1;
        bool    mDragging = false;
        float32 mDragZ = 0.f;
    };

} // namespace nkanima
