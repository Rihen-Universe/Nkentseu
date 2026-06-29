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
            dl.AddRectFilled(area, NkColor{22, 22, 25, 255}, 4.f);
            dl.AddRect(area, NkColor{40, 42, 48, 255}, 1.f);
            if (dur <= 1e-4f) return;

            const float32 pad = 8.f;
            const float32 x0 = area.x + pad, x1 = area.x + area.w - pad;
            const float32 ymid = area.y + area.h * 0.5f;
            auto timeToX = [&](float32 tt){ return x0 + (x1 - x0) * (tt / dur); };
            auto xToTime = [&](float32 xx){ float32 a=(xx-x0)/(x1-x0); if(a<0)a=0; if(a>1)a=1; return a*dur; };

            for (int32 s=0; (float32)s<=dur+1e-3f; ++s) {
                float32 gx = timeToX((float32)s);
                dl.AddLine({gx, area.y+area.h-14.f}, {gx, area.y+area.h-4.f}, NkColor{50, 52, 60, 200}, 1.f);
            }
            dl.AddLine({x0, ymid}, {x1, ymid}, NkColor{32, 34, 40, 255}, 1.f);

            NkVector<float32> keys; AnimGetKeyTimes(keys);
            for (uint32 i=0;i<(uint32)keys.Size();++i) {
                float32 kx = timeToX(keys[i]);
                bool sel = AnimIsSelected(keys[i]);
                dl.AddCircleFilled({kx, ymid}, 5.f, sel ? NkColor{0, 212, 255, 255} : NkColor{0, 150, 185, 255});
            }

            float32 px = timeToX(AnimCursor());
            dl.AddLine({px, area.y+2.f}, {px, area.y+area.h-2.f}, NkColor{0, 212, 255, 255}, 2.f);
            dl.AddCircleFilled({px, area.y+6.f}, 4.f, NkColor{0, 212, 255, 255});

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
            ec.Text("Apercu squelette 2D — Pose Mode : clic = os, drag = manipuler, Enregistrer = cle");
            // Barre : Mode Pose (toggle) + choix outil (Drag IK / Rotation FK) + Enregistrer.
            bool bMode = ec.Button(AnimInPoseEdit() ? "Mode Pose: ON" : "Mode Pose: OFF"); ctx.SameLine();
            bool bIK   = ec.Button(mTool==0 ? "[ Drag IK ]" : "Drag IK");   ctx.SameLine();
            bool bFK   = ec.Button(mTool==1 ? "[ Rotation FK ]" : "Rotation FK"); ctx.SameLine();
            bool bTR   = ec.Button(mTool==2 ? "[ Translation FK ]" : "Translation FK"); ctx.SameLine();
            bool bRec  = ec.Button("Enregistrer pose"); ctx.SameLine();
            bool bRag  = ec.Button(AnimPhysicsEnabled() ? "[ Ragdoll: ON ]" : "Ragdoll: OFF");
            if (bMode) { if (AnimInPoseEdit()) AnimEndPoseEdit(); else AnimBeginPoseEdit(); }
            if (bIK)   mTool = 0;
            if (bFK)   mTool = 1;
            if (bTR)   mTool = 2;
            if (bRec)  AnimCommitPoseKey();
            if (bRag)  AnimSetPhysics(!AnimPhysicsEnabled());   // couplage NKPhysics : le perso devient un ragdoll

            // 2e ligne : modes d'affichage du viewport (façon Blender).
            const NkAnimViewMode vm = Anim3DViewMode();
            ec.Text("Vue:"); ctx.SameLine();
            bool vSol = ec.Button(vm==NkAnimViewMode::SOLIDE  ? "[ Solide ]"  : "Solide");  ctx.SameLine();
            bool vRen = ec.Button(vm==NkAnimViewMode::RENDU   ? "[ Rendu ]"   : "Rendu");   ctx.SameLine();
            bool vWir = ec.Button(vm==NkAnimViewMode::FILAIRE ? "[ Filaire ]" : "Filaire");
            if (vSol) Anim3DSetViewMode(NkAnimViewMode::SOLIDE);
            if (vRen) Anim3DSetViewMode(NkAnimViewMode::RENDU);
            if (vWir) Anim3DSetViewMode(NkAnimViewMode::FILAIRE);

            const NkRect area = ctx.NextItemRect(560.f, 420.f);
            auto& dl = ctx.DL();
            dl.AddRectFilled(area, NkColor{18, 18, 21, 255}, 4.f);
            // Viewport 3D : texture offscreen NKRenderer (device partagé). UV Y inversé
            // (origine bas-gauche des render targets GL). Le squelette 2D reste dessiné
            // par-dessus en mode édition (repère de manipulation).
            const bool has3D = AnimLoaded() && Anim3DReady();
            if (has3D)
                dl.AddImage(ANIM_VIEWPORT_TEXID, area, NkVec2{0.f, 1.f}, NkVec2{1.f, 0.f},
                            NkColor{255, 255, 255, 255});
            dl.AddRect(area, AnimInPoseEdit() ? NkColor{0, 212, 255, 255} : NkColor{40, 42, 48, 255}, 1.f);
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
                if (p>=0 && p<(int32)jc) dl.AddLine(proj(pos[(uint32)p]), sp, NkColor{105, 135, 160, 255}, 2.f);
                bool sel = ((int32)j==mSel);
                dl.AddCircleFilled(sp, sel?6.f:3.f, sel ? NkColor{0, 212, 255, 255} : NkColor{0, 175, 215, 255});
            }

            // ── Gizmos selon l'outil, autour du joint sélectionné ────────────────
            const float32 gizR = 46.f, axLen = 52.f, axHit = 9.f;
            const bool gizmoOn = AnimInPoseEdit() && mSel>=0 && mSel<(int32)jc;
            NkVec2 gc = gizmoOn ? proj(pos[(uint32)mSel]) : NkVec2{0,0};
            if (gizmoOn && mTool==1) {              // Rotation FK : anneau cyan
                const int32 SEG=48; NkVec2 prev{};
                for (int32 s=0;s<=SEG;++s){ float32 a=(float32)s/(float32)SEG*6.2831853f;
                    NkVec2 q{ gc.x+cosf(a)*gizR, gc.y+sinf(a)*gizR };
                    if (s>0) dl.AddLine(prev, q, NkColor{0, 212, 255, 200}, 1.5f); prev=q; }
            } else if (gizmoOn && mTool==2) {       // Translation FK : axes X / Y
                NkVec2 xe{ gc.x+axLen, gc.y }, ye{ gc.x, gc.y-axLen };
                dl.AddLine(gc, xe, NkColor{230, 96, 96, 235}, 2.f);     // +X (rouge)
                dl.AddLine(gc, ye, NkColor{120, 210, 130, 235}, 2.f);   // +Y (vert)
                dl.AddCircleFilled(xe, 4.f, NkColor{230, 96, 96, 255});
                dl.AddCircleFilled(ye, 4.f, NkColor{120, 210, 130, 255});
                dl.AddRectFilled(NkRect{ gc.x-4.f, gc.y-4.f, 8.f, 8.f }, NkColor{0, 212, 255, 220}, 1.f);
            }

            // ── Interaction Pose Mode ─────────────────────────────────────────────
            if (!AnimInPoseEdit()) { mDragging=mRotating=mTranslating=false; return; }
            const NkVec2 m = ctx.input.mousePos;
            auto angAt =[&](const NkVec2& c){ return atan2f(m.y-c.y, m.x-c.x); };
            // distance point->segment (poignées d'axe)
            auto nearSeg=[&](const NkVec2& a, const NkVec2& b, float32 tol){
                float32 vx=b.x-a.x, vy=b.y-a.y, wx=m.x-a.x, wy=m.y-a.y;
                float32 L=vx*vx+vy*vy, t=(L>1e-6f)?((wx*vx+wy*vy)/L):0.f; if(t<0)t=0; if(t>1)t=1;
                float32 dx=m.x-(a.x+vx*t), dy=m.y-(a.y+vy*t); return sqrtf(dx*dx+dy*dy)<=tol; };

            if (ctx.IsHovered(area) && ctx.input.mouseClicked[0]) {
                bool grabbed=false;
                // 1) poignées de gizmo sur la sélection courante (prioritaire)
                if (mSel>=0 && mSel<(int32)jc) {
                    NkVec2 c=proj(pos[(uint32)mSel]);
                    if (mTool==1) { float32 dx=c.x-m.x,dy=c.y-m.y,d=sqrtf(dx*dx+dy*dy);
                        if (d<=gizR+8.f && d>=gizR-14.f) { mRotating=true; mPrevAng=angAt(c); grabbed=true; } }
                    else if (mTool==2) {
                        NkVec2 xe{c.x+axLen,c.y}, ye{c.x,c.y-axLen};
                        if      (nearSeg(c,xe,axHit)) { mTranslating=true; mTransAxis=1; grabbed=true; }
                        else if (nearSeg(c,ye,axHit)) { mTranslating=true; mTransAxis=2; grabbed=true; }
                        else { float32 dx=c.x-m.x,dy=c.y-m.y; if (sqrtf(dx*dx+dy*dy)<=8.f){ mTranslating=true; mTransAxis=0; grabbed=true; } }
                    }
                }
                // 2) sinon, sélectionne le joint le plus proche et démarre l'outil
                if (!grabbed) {
                    int32 hit=-1; float32 best=14.f;
                    for (uint32 j=0;j<jc;++j){ NkVec2 sp=proj(pos[j]); float32 dx=sp.x-m.x, dy=sp.y-m.y; float32 d=sqrtf(dx*dx+dy*dy); if(d<best){best=d;hit=(int32)j;} }
                    if (hit>=0) {
                        mSel=hit; mDragZ=pos[(uint32)hit].z;
                        if      (mTool==0) mDragging=true;
                        else if (mTool==1) { mRotating=true; mPrevAng=angAt(proj(pos[(uint32)hit])); }
                        else if (mTool==2) { mTranslating=true; mTransAxis=0; }
                    }
                }
                mLastMx=m.x; mLastMy=m.y;
            }
            if (ctx.input.mouseDown[0]) {
                if (mDragging && mSel>=0)               // Drag IK
                    AnimDragJoint(mSel, unprojX(m.x), unprojY(m.y), mDragZ);
                else if (mRotating && mSel>=0) {        // Rotation FK
                    float32 a = angAt(proj(pos[(uint32)mSel]));
                    float32 da = a - mPrevAng;
                    if (da> 3.14159265f) da-=6.2831853f; if (da<-3.14159265f) da+=6.2831853f;
                    AnimRotateJoint(mSel, -da);         // écran Y inversé -> sens visuel
                    mPrevAng = a;
                } else if (mTranslating && mSel>=0) {   // Translation FK
                    float32 wdx=(m.x-mLastMx)/sc, wdy=-(m.y-mLastMy)/sc;   // écran -> monde
                    if (mTransAxis==1) wdy=0.f; else if (mTransAxis==2) wdx=0.f;
                    AnimTranslateJoint(mSel, wdx, wdy, 0.f);
                    mLastMx=m.x; mLastMy=m.y;
                }
            } else { mDragging=mRotating=mTranslating=false; }
        }
    private:
        int32   mSel = -1;
        bool    mDragging = false, mRotating = false, mTranslating = false;
        int32   mTool = 0;          // 0 = Drag IK, 1 = Rotation FK, 2 = Translation FK
        int32   mTransAxis = 0;     // 0 = libre, 1 = X, 2 = Y
        float32 mDragZ = 0.f, mPrevAng = 0.f, mLastMx = 0.f, mLastMy = 0.f;
    };

} // namespace nkanima
