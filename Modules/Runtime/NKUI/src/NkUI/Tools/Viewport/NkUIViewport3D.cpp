/*
 * NkUIViewport3D.cpp — Viewport 3D production-ready style Unreal Engine.
 * Partie 1/3 : projection, primitives 3D, helpers.
 */
#include "NkUIViewport3D.h"
#include <cmath>
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace nkui {

        // =====================================================================
        //  Constantes visuelles (style Unreal Engine)
        // =====================================================================

        // Couleurs grille
        static constexpr NkColor kGridLine     = { 55,  58,  68, 120};
        static constexpr NkColor kGridLineSub  = { 40,  42,  50,  70};
        static constexpr NkColor kAxisX        = {200,  60,  60, 220};
        static constexpr NkColor kAxisY        = { 60, 200, 100, 220};
        static constexpr NkColor kAxisZ        = { 60, 120, 200, 220};

        // Couleurs UI (Unreal dark theme)
        static constexpr NkColor kToolbarBg    = { 32,  32,  36, 255};
        static constexpr NkColor kPanelBg      = { 24,  24,  28, 255};
        static constexpr NkColor kPanelBorder  = { 48,  48,  55, 255};
        static constexpr NkColor kSectionBg    = { 35,  35,  40, 255};
        static constexpr NkColor kHoverBg      = { 55,  55,  65, 180};
        static constexpr NkColor kSelectBg     = { 18,  86, 153, 200};  // bleu UE
        static constexpr NkColor kSelectionHL  = {255, 140,  0, 255};   // orange UE
        static constexpr NkColor kTextPrimary  = {220, 220, 220, 255};
        static constexpr NkColor kTextSecond   = {140, 140, 150, 255};
        static constexpr NkColor kTextDisabled = { 80,  80,  90, 255};
        static constexpr NkColor kAccent       = { 0,  122, 204, 255};  // bleu UE
        static constexpr NkColor kAccentHover  = { 30, 140, 220, 255};
        static constexpr NkColor kDanger       = {200,  50,  50, 255};
        static constexpr NkColor kSuccess      = { 60, 180,  80, 255};

        // =====================================================================
        //  Projection — orbite caméra avec pivot
        // =====================================================================

        NkUIViewport3D::ProjectCtx NkUIViewport3D::MakeProjectCtx(
            const NkVP3DCamera& cam, NkRect viewport) noexcept
        {
            ProjectCtx pc;
            pc.center.x = viewport.x + viewport.w * 0.5f;
            pc.center.y = viewport.y + viewport.h * 0.5f;
            pc.isPerspective = (cam.mode == NkVP3DCameraMode::NK_VP3D_PERSPECTIVE);
            pc.cameraMode = cam.mode;
            pc.orthoZoom = cam.orthoZoom > 0.01f ? cam.orthoZoom : 0.01f;

            // Scale : pixels par unité monde
            // En perspective on simule un zoom basé sur la distance
            if (pc.isPerspective) {
                const float32 fovRad = cam.fovDeg * NKUI_PI / 180.f;
                const float32 halfH  = viewport.h * 0.5f;
                pc.scale = halfH / ::tanf(fovRad * 0.5f) / cam.distance;
            } else {
                pc.scale = 48.f * pc.orthoZoom;
            }
            pc.scale = pc.scale > 0.001f ? pc.scale : 0.001f;

            pc.camZ  = pc.isPerspective ? cam.distance : 1000.f;
            pc.yawR  = cam.yaw   * NKUI_PI / 180.f;
            pc.pitchR= cam.pitch * NKUI_PI / 180.f;
            return pc;
        }

        NkVec2 NkUIViewport3D::Project(const ProjectCtx& pc,
                                        float32 wx, float32 wy, float32 wz) noexcept {
            // Rotation yaw (autour de Y)
            const float32 cy = ::cosf(pc.yawR), sy = ::sinf(pc.yawR);
            const float32 x1 = wx*cy - wz*sy;
            const float32 z1 = wx*sy + wz*cy;

            // Rotation pitch (autour de X)
            const float32 cp = ::cosf(pc.pitchR), sp = ::sinf(pc.pitchR);
            const float32 y2 = wy*cp - z1*sp;
            const float32 z2 = wy*sp + z1*cp;

            if (pc.isPerspective) {
                const float32 dz = z2 + pc.camZ;
                const float32 d  = (dz > 0.01f) ? (pc.camZ / dz) : 0.001f;
                return {pc.center.x + x1*d*pc.scale,
                        pc.center.y - y2*d*pc.scale};
            } else {
                // Orthographique
                return {pc.center.x + x1*pc.scale,
                        pc.center.y - y2*pc.scale};
            }
        }

        NkVec2 NkUIViewport3D::ProjectDir(const ProjectCtx& pc,
                                           float32 dx, float32 dy, float32 dz) noexcept {
            const NkVec2 o  = Project(pc, 0.f, 0.f, 0.f);
            const NkVec2 p  = Project(pc, dx, dy, dz);
            const float32 l = ::sqrtf((p.x-o.x)*(p.x-o.x)+(p.y-o.y)*(p.y-o.y));
            return l > 1e-6f ? NkVec2{(p.x-o.x)/l,(p.y-o.y)/l} : NkVec2{1.f,0.f};
        }

        // =====================================================================
        //  Layout
        // =====================================================================

        NkUIViewport3D::Layout NkUIViewport3D::ComputeLayout(
            NkRect r, const NkVP3DConfig& cfg) noexcept
        {
            Layout ly;
            const float32 tbH = cfg.toolbarH;
            const float32 sbH = cfg.statusBarH;
            const float32 ow  = cfg.outlinerW;

            ly.toolbar  = {r.x, r.y, r.w, tbH};
            ly.statusBar= {r.x, r.y + r.h - sbH, r.w, sbH};

            const float32 contentY = r.y + tbH;
            const float32 contentH = r.h - tbH - sbH;

            ly.outliner = {r.x + r.w - ow, contentY, ow, contentH * 0.45f};
            ly.details  = {r.x + r.w - ow, contentY + contentH*0.45f,
                           ow, contentH * 0.55f};
            ly.viewport3D = {r.x, contentY, r.w - ow, contentH};
            return ly;
        }

        // =====================================================================
        //  Helpers géométriques 3D
        // =====================================================================

        static NKUI_INLINE float32 Len2(NkVec2 v) noexcept {
            return ::sqrtf(v.x*v.x + v.y*v.y);
        }
        static NKUI_INLINE float32 Clamp01(float32 v) noexcept {
            return v<0.f?0.f:v>1.f?1.f:v;
        }
        static NKUI_INLINE bool InRect(NkRect r, NkVec2 p) noexcept {
            return p.x>=r.x&&p.x<=r.x+r.w&&p.y>=r.y&&p.y<=r.y+r.h;
        }
        static NKUI_INLINE float32 Lerp(float32 a, float32 b, float32 t) noexcept {
            return a + (b-a)*t;
        }

        // Primitives 3D projetées

        void NkUIViewport3D::DrawCube(NkUIDrawList& dl, NkRect clip,
                                       const ProjectCtx& pc,
                                       float32 px,float32 py,float32 pz,
                                       float32 sx,float32 sy,float32 sz,
                                       NkColor col,float32 lw) noexcept {
            const float32 hx=sx*0.5f,hy=sy*0.5f,hz=sz*0.5f;
            NkVec2 v[8];
            v[0]=Project(pc,px-hx,py-hy,pz-hz); v[1]=Project(pc,px+hx,py-hy,pz-hz);
            v[2]=Project(pc,px+hx,py+hy,pz-hz); v[3]=Project(pc,px-hx,py+hy,pz-hz);
            v[4]=Project(pc,px-hx,py-hy,pz+hz); v[5]=Project(pc,px+hx,py-hy,pz+hz);
            v[6]=Project(pc,px+hx,py+hy,pz+hz); v[7]=Project(pc,px-hx,py+hy,pz+hz);
            const int32 edges[12][2]={
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}};
            dl.PushClipRect(clip,true);
            for (const auto& e:edges) dl.AddLine(v[e[0]],v[e[1]],col,lw);
            dl.PopClipRect();
        }

        void NkUIViewport3D::DrawSphere(NkUIDrawList& dl, NkRect clip,
                                         const ProjectCtx& pc,
                                         float32 px,float32 py,float32 pz,
                                         float32 r,NkColor col,float32 lw) noexcept {
            const int32 seg=24;
            dl.PushClipRect(clip,true);
            for (int32 plane=0;plane<3;++plane){
                NkVec2 prev={};
                for (int32 k=0;k<=seg;++k){
                    const float32 a=k*NKUI_PI*2.f/(float32)seg;
                    const float32 ca=::cosf(a)*r,sa=::sinf(a)*r;
                    NkVec2 pt;
                    if      (plane==0) pt=Project(pc,px+ca,py+sa,pz);
                    else if (plane==1) pt=Project(pc,px+ca,py,pz+sa);
                    else               pt=Project(pc,px,py+ca,pz+sa);
                    if (k>0) dl.AddLine(prev,pt,col,lw);
                    prev=pt;
                }
            }
            dl.PopClipRect();
        }

        void NkUIViewport3D::DrawCylinder(NkUIDrawList& dl, NkRect clip,
                                           const ProjectCtx& pc,
                                           float32 px,float32 py,float32 pz,
                                           float32 rx,float32 ry,
                                           NkColor col,float32 lw) noexcept {
            const int32 seg=16;
            const float32 hh=ry*0.5f;
            dl.PushClipRect(clip,true);
            NkVec2 top[seg+1], bot[seg+1];
            for (int32 k=0;k<=seg;++k){
                const float32 a=k*NKUI_PI*2.f/(float32)seg;
                const float32 ca=::cosf(a)*rx,sa=::sinf(a)*rx;
                top[k]=Project(pc,px+ca,py+hh,pz+sa);
                bot[k]=Project(pc,px+ca,py-hh,pz+sa);
                if (k>0){ dl.AddLine(top[k-1],top[k],col,lw); dl.AddLine(bot[k-1],bot[k],col,lw); }
            }
            // Colonnes verticales sur 8 points
            for (int32 k=0;k<seg;k+=2) dl.AddLine(top[k],bot[k],col,lw);
            dl.PopClipRect();
        }

        void NkUIViewport3D::DrawCone(NkUIDrawList& dl, NkRect clip,
                                       const ProjectCtx& pc,
                                       float32 px,float32 py,float32 pz,
                                       float32 r,float32 h,NkColor col,float32 lw) noexcept {
            const int32 seg=16;
            const NkVec2 tip=Project(pc,px,py+h,pz);
            dl.PushClipRect(clip,true);
            NkVec2 base[seg+1];
            for (int32 k=0;k<=seg;++k){
                const float32 a=k*NKUI_PI*2.f/(float32)seg;
                base[k]=Project(pc,px+::cosf(a)*r,py,pz+::sinf(a)*r);
                if (k>0){ dl.AddLine(base[k-1],base[k],col,lw); }
                if (k%4==0) dl.AddLine(tip,base[k],col,lw);
            }
            dl.PopClipRect();
        }

        void NkUIViewport3D::DrawPlane(NkUIDrawList& dl, NkRect clip,
                                        const ProjectCtx& pc,
                                        float32 px,float32 py,float32 pz,
                                        float32 sx,float32 sz,
                                        NkColor col,float32 lw) noexcept {
            const float32 hx=sx*0.5f,hz=sz*0.5f;
            const NkVec2 v0=Project(pc,px-hx,py,pz-hz);
            const NkVec2 v1=Project(pc,px+hx,py,pz-hz);
            const NkVec2 v2=Project(pc,px+hx,py,pz+hz);
            const NkVec2 v3=Project(pc,px-hx,py,pz+hz);
            dl.PushClipRect(clip,true);
            dl.AddLine(v0,v1,col,lw); dl.AddLine(v1,v2,col,lw);
            dl.AddLine(v2,v3,col,lw); dl.AddLine(v3,v0,col,lw);
            dl.AddLine(v0,v2,col.WithAlpha(100),lw*0.6f);
            dl.AddLine(v1,v3,col.WithAlpha(100),lw*0.6f);
            dl.PopClipRect();
        }

        void NkUIViewport3D::DrawLightIcon(NkUIDrawList& dl,
                                            const ProjectCtx& pc,
                                            float32 px,float32 py,float32 pz,
                                            NkVP3DObjectShape type,NkColor col) noexcept {
            const NkVec2 c=Project(pc,px,py,pz);
            const float32 r=10.f;
            dl.AddCircle(c,r,col,1.5f,12);
            dl.AddCircleFilled(c,r*0.45f,col.WithAlpha(200),8);
            if (type==NkVP3DObjectShape::NK_VP3D_LIGHT_POINT){
                // Rayons
                for (int32 i=0;i<8;++i){
                    const float32 a=i*NKUI_PI*0.25f;
                    dl.AddLine({c.x+::cosf(a)*(r+2.f),c.y+::sinf(a)*(r+2.f)},
                               {c.x+::cosf(a)*(r+6.f),c.y+::sinf(a)*(r+6.f)},
                               col,1.2f);
                }
            } else {
                // Directionnel : flèche vers le bas
                dl.AddLine({c.x,c.y+r},{c.x,c.y+r+12.f},col,1.5f);
                dl.AddTriangleFilled({c.x,c.y+r+16.f},{c.x-4.f,c.y+r+10.f},{c.x+4.f,c.y+r+10.f},col);
            }
        }

        void NkUIViewport3D::DrawCameraIcon(NkUIDrawList& dl,
                                             const ProjectCtx& pc,
                                             float32 px,float32 py,float32 pz,
                                             NkColor col) noexcept {
            const NkVec2 c=Project(pc,px,py,pz);
            // Corps caméra
            dl.AddRectFilled({c.x-12.f,c.y-8.f,24.f,16.f},col.WithAlpha(140),2.f);
            dl.AddRect({c.x-12.f,c.y-8.f,24.f,16.f},col,1.5f,2.f);
            // Objectif
            dl.AddCircle({c.x,c.y},5.f,col,1.5f,8);
            // Viseur
            dl.AddTriangleFilled({c.x+12.f,c.y-5.f},{c.x+20.f,c.y-8.f},{c.x+20.f,c.y+8.f},col.WithAlpha(160));
            dl.AddTriangleFilled({c.x+12.f,c.y+5.f},{c.x+20.f,c.y-8.f},{c.x+20.f,c.y+8.f},col.WithAlpha(160));
        }

        void NkUIViewport3D::DrawEmptyIcon(NkUIDrawList& dl,
                                            const ProjectCtx& pc,
                                            float32 px,float32 py,float32 pz,
                                            NkColor col) noexcept {
            const NkVec2 c=Project(pc,px,py,pz);
            const float32 s=8.f;
            dl.AddLine({c.x-s,c.y},{c.x+s,c.y},col,1.2f);
            dl.AddLine({c.x,c.y-s},{c.x,c.y+s},col,1.2f);
            dl.AddCircle(c,s*0.6f,col.WithAlpha(160),1.f,8);
        }

        // =====================================================================
        //  DrawObjectShape — dispatch selon le type
        // =====================================================================

        void NkUIViewport3D::DrawObjectShape(NkUIDrawList& dl, NkRect viewport,
                                              const NkVP3DObject& obj,
                                              const ProjectCtx& pc,
                                              bool selected, bool hovered,
                                              const NkVP3DConfig& cfg,
                                              NkVP3DShadingMode shading) noexcept {
            (void)shading; (void)cfg;
            const NkColor baseCol = obj.color;
            const NkColor wireCol = selected ? kSelectionHL
                                  : hovered  ? NkColor{255,220,150,220}
                                             : obj.wireColor;
            const float32 lw = selected ? 2.0f : hovered ? 1.6f : 1.2f;

            const float32 px=obj.transform.position.x;
            const float32 py=obj.transform.position.y;
            const float32 pz=obj.transform.position.z;
            const float32 sx=::fmaxf(0.01f,obj.transform.scale.x);
            const float32 sy=::fmaxf(0.01f,obj.transform.scale.y);
            const float32 sz=::fmaxf(0.01f,obj.transform.scale.z);

            switch (obj.shape) {
                case NkVP3DObjectShape::NK_VP3D_CUBE:
                    DrawCube(dl,viewport,pc,px,py,pz,sx,sy,sz,wireCol,lw);
                    break;
                case NkVP3DObjectShape::NK_VP3D_SPHERE:
                    DrawSphere(dl,viewport,pc,px,py,pz,sx*0.5f,wireCol,lw);
                    break;
                case NkVP3DObjectShape::NK_VP3D_CYLINDER:
                    DrawCylinder(dl,viewport,pc,px,py,pz,sx*0.5f,sy,wireCol,lw);
                    break;
                case NkVP3DObjectShape::NK_VP3D_CONE:
                    DrawCone(dl,viewport,pc,px,py,pz,sx*0.5f,sy,wireCol,lw);
                    break;
                case NkVP3DObjectShape::NK_VP3D_PLANE:
                    DrawPlane(dl,viewport,pc,px,py,pz,sx,sz,wireCol,lw);
                    break;
                case NkVP3DObjectShape::NK_VP3D_LIGHT_POINT:
                case NkVP3DObjectShape::NK_VP3D_LIGHT_DIR:
                    DrawLightIcon(dl,pc,px,py,pz,obj.shape,wireCol);
                    break;
                case NkVP3DObjectShape::NK_VP3D_CAMERA:
                    DrawCameraIcon(dl,pc,px,py,pz,wireCol);
                    break;
                case NkVP3DObjectShape::NK_VP3D_EMPTY:
                    DrawEmptyIcon(dl,pc,px,py,pz,wireCol);
                    break;
                default:
                    DrawCube(dl,viewport,pc,px,py,pz,sx,sy,sz,wireCol,lw);
                    break;
            }
            (void)baseCol;
        }

        // =====================================================================
        //  Picking
        // =====================================================================

        float32 NkUIViewport3D::PickObject(const NkVP3DObject& obj,
                                            const ProjectCtx& pc,
                                            NkVec2 mp) noexcept {
            const NkVec2 c=Project(pc,
                obj.transform.position.x,
                obj.transform.position.y,
                obj.transform.position.z);
            const float32 dist=Len2({mp.x-c.x,mp.y-c.y});
            const float32 pickR=::fmaxf(16.f,
                ::fmaxf(obj.transform.scale.x,
                ::fmaxf(obj.transform.scale.y,obj.transform.scale.z))*pc.scale*0.6f);
            return (dist<pickR)?dist:-1.f;
        }

        // =====================================================================
        //  ShapeName / DrawShapeIcon
        // =====================================================================

        const char* NkUIViewport3D::ShapeName(NkVP3DObjectShape s) noexcept {
            switch(s){
                case NkVP3DObjectShape::NK_VP3D_CUBE:       return "Cube";
                case NkVP3DObjectShape::NK_VP3D_SPHERE:     return "Sphere";
                case NkVP3DObjectShape::NK_VP3D_CYLINDER:   return "Cylinder";
                case NkVP3DObjectShape::NK_VP3D_PLANE:      return "Plane";
                case NkVP3DObjectShape::NK_VP3D_CONE:       return "Cone";
                case NkVP3DObjectShape::NK_VP3D_TORUS:      return "Torus";
                case NkVP3DObjectShape::NK_VP3D_LIGHT_POINT:return "Point Light";
                case NkVP3DObjectShape::NK_VP3D_LIGHT_DIR:  return "Dir Light";
                case NkVP3DObjectShape::NK_VP3D_CAMERA:     return "Camera";
                case NkVP3DObjectShape::NK_VP3D_EMPTY:      return "Empty";
                default: return "Object";
            }
        }

        void NkUIViewport3D::DrawShapeIcon(NkUIDrawList& dl, NkRect r,
                                            NkVP3DObjectShape s, NkColor col) noexcept {
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f, sz=r.w*0.38f;
            switch(s){
                case NkVP3DObjectShape::NK_VP3D_CUBE:
                    dl.AddRect({cx-sz,cy-sz,sz*2.f,sz*2.f},col,1.2f);
                    dl.AddRect({cx-sz*0.6f,cy-sz*1.4f,sz*1.2f,sz*2.f},col.WithAlpha(120),0.8f);
                    break;
                case NkVP3DObjectShape::NK_VP3D_SPHERE:
                    dl.AddCircle({cx,cy},sz,col,1.2f,12);
                    dl.AddEllipse({cx,cy},sz,sz*0.4f,col.WithAlpha(120),1.f,12);
                    break;
                case NkVP3DObjectShape::NK_VP3D_LIGHT_POINT:
                    dl.AddCircle({cx,cy},sz*0.6f,col,1.2f,8);
                    dl.AddCircleFilled({cx,cy},sz*0.28f,col.WithAlpha(200),6);
                    for(int32 i=0;i<6;++i){
                        const float32 a=i*NKUI_PI/3.f;
                        dl.AddLine({cx+::cosf(a)*sz*0.7f,cy+::sinf(a)*sz*0.7f},
                                   {cx+::cosf(a)*sz,cy+::sinf(a)*sz},col,1.f);
                    }
                    break;
                case NkVP3DObjectShape::NK_VP3D_CAMERA:
                    dl.AddRectFilled({cx-sz,cy-sz*0.5f,sz*1.6f,sz},{col.r,col.g,col.b,100},2.f);
                    dl.AddRect({cx-sz,cy-sz*0.5f,sz*1.6f,sz},col,1.f,2.f);
                    dl.AddTriangleFilled({cx+sz*0.7f,cy-sz*0.6f},{cx+sz*1.3f,cy-sz*0.4f},{cx+sz*0.7f,cy+sz*0.6f},col.WithAlpha(160));
                    break;
                case NkVP3DObjectShape::NK_VP3D_EMPTY:
                    dl.AddLine({cx-sz,cy},{cx+sz,cy},col,1.f);
                    dl.AddLine({cx,cy-sz},{cx,cy+sz},col,1.f);
                    dl.AddCircle({cx,cy},sz*0.5f,col.WithAlpha(120),1.f,8);
                    break;
                default:
                    dl.AddCircle({cx,cy},sz,col,1.f,8);
                    break;
            }
        }

        // =====================================================================
        //  AddObject / SetupDemoScene / FocusSelected
        // =====================================================================

        int32 NkUIViewport3D::AddObject(NkVP3DState& state,
                                         const char* name,
                                         NkVP3DObjectShape shape,
                                         NkColor color) noexcept {
            if (state.numObjects >= NkVP3DState::MAX_OBJECTS) return -1;
            NkVP3DObject& obj = state.objects[state.numObjects];
            ::memset(&obj, 0, sizeof(obj));
            ::strncpy(obj.name, name, 63);
            obj.shape   = shape;
            obj.color   = color;
            obj.wireColor = color;
            obj.visible = true;
            obj.transform.scale = {1.f,1.f,1.f};
            obj.prevTransform   = obj.transform;
            return state.numObjects++;
        }

        void NkUIViewport3D::SetupDemoScene(NkVP3DState& state) noexcept {
            ::memset(&state, 0, sizeof(NkVP3DState));

            // Caméra par défaut
            state.camera.yaw      = 35.f;
            state.camera.pitch    = 28.f;
            state.camera.distance = 12.f;
            state.camera.fovDeg   = 60.f;
            state.camera.mode     = NkVP3DCameraMode::NK_VP3D_PERSPECTIVE;

            // Gizmo config
            state.gizmoCfg.mode         = NkUIGizmoMode::NK_TRANSLATE;
            state.gizmoCfg.axisMask     = NK_GIZMO_AXIS_ALL;
            state.gizmoCfg.axisLength   = 60.f;
            state.gizmoCfg.showCenter   = true;
            state.gizmoCfg.showAxisLabels = true;
            state.gizmoCfg.showGrid     = false;
            state.gizmoCfg.showPlanes   = true;
            state.gizmoCfg.showCenterMove  = true;
            state.gizmoCfg.showCenterScale = true;
            state.gizmoCfg.snap.enabled = false;

            // Objets
            int32 i;
            i = AddObject(state, "Cube",      NkVP3DObjectShape::NK_VP3D_CUBE,   {100,180,255,255});
            state.objects[i].transform.position = {-2.5f, 0.f, 0.f};

            i = AddObject(state, "Sphere",    NkVP3DObjectShape::NK_VP3D_SPHERE, {255,160,60,255});
            state.objects[i].transform.position = { 0.f,  1.f, 0.f};

            i = AddObject(state, "Cylinder",  NkVP3DObjectShape::NK_VP3D_CYLINDER,{80,220,140,255});
            state.objects[i].transform.position = { 2.5f, 0.f, 0.f};

            i = AddObject(state, "Ground",    NkVP3DObjectShape::NK_VP3D_PLANE,  {120,120,130,255});
            state.objects[i].transform.position = { 0.f, -0.5f, 0.f};
            state.objects[i].transform.scale    = {8.f, 1.f, 8.f};

            i = AddObject(state, "Point Light",NkVP3DObjectShape::NK_VP3D_LIGHT_POINT,{255,240,180,255});
            state.objects[i].transform.position = { 0.f, 4.f, 0.f};

            i = AddObject(state, "Camera",    NkVP3DObjectShape::NK_VP3D_CAMERA, {160,200,255,255});
            state.objects[i].transform.position = { 5.f, 3.f, 7.f};
        }

        void NkUIViewport3D::FocusSelected(NkVP3DState& state,
                                            const NkVP3DConfig& cfg) noexcept {
            if (state.numSelected == 0) {
                // Focus sur l'origine
                state.camera.pivotX = state.camera.pivotY = state.camera.pivotZ = 0.f;
                state.camera.distance = 10.f;
                return;
            }
            // Centroïde des sélections
            float32 cx=0.f,cy=0.f,cz=0.f;
            float32 maxR=0.f;
            for (int32 k=0;k<state.numSelected;++k){
                const NkVP3DObject& o=state.objects[state.selectedIdx[k]];
                cx+=o.transform.position.x;
                cy+=o.transform.position.y;
                cz+=o.transform.position.z;
                const float32 r=::fmaxf(o.transform.scale.x,
                                ::fmaxf(o.transform.scale.y,o.transform.scale.z));
                if (r>maxR) maxR=r;
            }
            cx/=static_cast<float32>(state.numSelected);
            cy/=static_cast<float32>(state.numSelected);
            cz/=static_cast<float32>(state.numSelected);

            state.camera.pivotX=cx; state.camera.pivotY=cy; state.camera.pivotZ=cz;
            state.camera.distance=::fmaxf(2.f,maxR*cfg.focusPaddingFactor);
        }
        // =====================================================================
        //  DrawToolbar — style Unreal Engine
        // =====================================================================
 
        void NkUIViewport3D::DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                          NkUIFont& font, NkRect r,
                                          NkVP3DConfig& cfg,
                                          NkVP3DState& state) noexcept {
            // Fond toolbar
            dl.AddRectFilled(r, kToolbarBg);
            dl.AddLine({r.x,r.y+r.h},{r.x+r.w,r.y+r.h},kPanelBorder.WithAlpha(200),1.f);
 
            const float32 bh=r.h-6.f, by=r.y+3.f;
            const float32 ph=6.f;
            float32 cx=r.x+ph;
 
            // ── Helper bouton ──────────────────────────────────────────────
            auto btn=[&](const char* label, float32 bw, bool active, bool enabled=true)->bool{
                const NkRect br={cx,by,bw,bh};
                const bool hov=enabled&&InRect(br,ctx.input.mousePos);
                NkColor bg=kToolbarBg;
                if      (active)  bg=kAccent;
                else if (hov)     bg=kHoverBg;
                dl.AddRectFilled(br,bg,3.f);
                if (active) dl.AddRect(br,kAccentHover.WithAlpha(160),1.f,3.f);
                else dl.AddRect(br,kPanelBorder.WithAlpha(active?200:100),1.f,3.f);
                const float32 tw=font.MeasureWidth(label);
                const float32 tx=br.x+(bw-tw)*0.5f;
                const float32 ty=br.y+(bh-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
                font.RenderText(dl,{tx,ty},label,
                    enabled?kTextPrimary:kTextDisabled,bw,false);
                cx+=bw+3.f;
                return hov&&ctx.input.IsMouseClicked(0)&&enabled;
            };
 
            // ── Helper icône bouton ────────────────────────────────────────
            auto icoBtn=[&](float32 bw, bool active, auto drawFn)->bool{
                const NkRect br={cx,by,bw,bh};
                const bool hov=InRect(br,ctx.input.mousePos);
                NkColor bg=active?kAccent:hov?kHoverBg:kToolbarBg;
                dl.AddRectFilled(br,bg,3.f);
                dl.AddRect(br,kPanelBorder.WithAlpha(active?200:80),1.f,3.f);
                const NkRect ir={br.x+3.f,br.y+3.f,bw-6.f,bh-6.f};
                drawFn(ir,active||hov);
                cx+=bw+3.f;
                return hov&&ctx.input.IsMouseClicked(0);
            };
 
            // ── Séparateur ─────────────────────────────────────────────────
            auto sep=[&](){
                dl.AddLine({cx+2.f,by+4.f},{cx+2.f,by+bh-4.f},
                           kPanelBorder.WithAlpha(160),1.f);
                cx+=8.f;
            };
 
            // ── SECTION 1 : Modes de transform ────────────────────────────
            // Icône curseur (select)
            if (icoBtn(bh,false,[&](NkRect ir,bool hov){
                    const NkColor c=hov?kTextPrimary:kTextSecond;
                    // Icône flèche sélection
                    dl.AddTriangleFilled({ir.x+ir.w*0.2f,ir.y+ir.h*0.1f},
                                        {ir.x+ir.w*0.2f,ir.y+ir.h*0.85f},
                                        {ir.x+ir.w*0.65f,ir.y+ir.h*0.65f},c);
                    dl.AddRectFilled({ir.x+ir.w*0.55f,ir.y+ir.h*0.60f,
                                      ir.w*0.15f,ir.h*0.35f},c,1.f);})) {}
 
            sep();
 
            // Translate
            if (btn("Translate",72.f,state.gizmoCfg.mode==NkUIGizmoMode::NK_TRANSLATE))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_TRANSLATE;
            // Rotate
            if (btn("Rotate",62.f,state.gizmoCfg.mode==NkUIGizmoMode::NK_ROTATE))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_ROTATE;
            // Scale
            if (btn("Scale",55.f,state.gizmoCfg.mode==NkUIGizmoMode::NK_SCALE))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_SCALE;
 
            sep();
 
            // ── SECTION 2 : Espace de coordonnées ─────────────────────────
            if (btn("World",52.f,state.gizmoCfg.space==NkUIGizmoSpace::NK_WORLD))
                state.gizmoCfg.space=NkUIGizmoSpace::NK_WORLD;
            if (btn("Local",48.f,state.gizmoCfg.space==NkUIGizmoSpace::NK_LOCAL))
                state.gizmoCfg.space=NkUIGizmoSpace::NK_LOCAL;
 
            sep();
 
            // ── SECTION 3 : Snap ──────────────────────────────────────────
            if (icoBtn(bh,state.snapEnabled,[&](NkRect ir,bool hov){
                    // Icône aimant
                    const NkColor c=hov||state.snapEnabled?kTextPrimary:kTextSecond;
                    const float32 cx2=ir.x+ir.w*0.5f,cy2=ir.y+ir.h*0.5f;
                    dl.AddArc({cx2,cy2+ir.h*0.05f},ir.h*0.32f,
                              NKUI_PI*0.05f,NKUI_PI*0.95f,c,2.f,10);
                    dl.AddLine({cx2-ir.h*0.32f,cy2+ir.h*0.05f},
                               {cx2-ir.h*0.32f,cy2+ir.h*0.35f},c,2.f);
                    dl.AddLine({cx2+ir.h*0.32f,cy2+ir.h*0.05f},
                               {cx2+ir.h*0.32f,cy2+ir.h*0.35f},c,2.f);
                    dl.AddLine({cx2-ir.h*0.32f,cy2+ir.h*0.35f},
                               {cx2-ir.h*0.18f,cy2+ir.h*0.35f},c,2.f);
                    dl.AddLine({cx2+ir.h*0.32f,cy2+ir.h*0.35f},
                               {cx2+ir.h*0.18f,cy2+ir.h*0.35f},c,2.f);
                })) {
                state.snapEnabled=!state.snapEnabled;
                state.gizmoCfg.snap.enabled=state.snapEnabled;
                state.gizmoCfg.snap.translateStep=cfg.snapTranslate;
                state.gizmoCfg.snap.rotateStepDeg=cfg.snapRotate;
                state.gizmoCfg.snap.scaleStep=cfg.snapScale;
            }
 
            sep();
 
            // ── SECTION 4 : Mode de vue caméra ────────────────────────────
            const char* viewLabels[]={
                "Persp","Front","Back","Left","Right","Top","Bot"};
            const NkVP3DCameraMode viewModes[]={
                NkVP3DCameraMode::NK_VP3D_PERSPECTIVE,
                NkVP3DCameraMode::NK_VP3D_ORTHO_FRONT,
                NkVP3DCameraMode::NK_VP3D_ORTHO_BACK,
                NkVP3DCameraMode::NK_VP3D_ORTHO_LEFT,
                NkVP3DCameraMode::NK_VP3D_ORTHO_RIGHT,
                NkVP3DCameraMode::NK_VP3D_ORTHO_TOP,
                NkVP3DCameraMode::NK_VP3D_ORTHO_BOTTOM,
            };
            const float32 viewBW[]={46.f,46.f,44.f,42.f,48.f,40.f,38.f};
            for (int32 i=0;i<7;++i){
                if (btn(viewLabels[i],viewBW[i],state.camera.mode==viewModes[i]))
                    state.camera.mode=viewModes[i];
            }
 
            sep();
 
            // ── SECTION 5 : Shading ───────────────────────────────────────
            if (btn("Wire",42.f,state.shadingMode==NkVP3DShadingMode::NK_VP3D_WIREFRAME))
                state.shadingMode=NkVP3DShadingMode::NK_VP3D_WIREFRAME;
            if (btn("Solid",46.f,state.shadingMode==NkVP3DShadingMode::NK_VP3D_SOLID))
                state.shadingMode=NkVP3DShadingMode::NK_VP3D_SOLID;
            if (btn("Unlit",44.f,state.shadingMode==NkVP3DShadingMode::NK_VP3D_UNLIT))
                state.shadingMode=NkVP3DShadingMode::NK_VP3D_UNLIT;
 
            sep();
 
            // ── SECTION 6 : Grille + overlays ─────────────────────────────
            if (btn("Grid",40.f,cfg.showGrid))     cfg.showGrid=!cfg.showGrid;
            if (btn("Stats",44.f,cfg.showStats))   cfg.showStats=!cfg.showStats;
 
            sep();
 
            // ── DROITE : Add Object ────────────────────────────────────────
            {
                // Bouton "Add" aligné à droite
                const float32 addW=58.f;
                const float32 addX=r.x+r.w-cfg.outlinerW-addW-ph;
                const NkRect addR={addX,by,addW,bh};
                const bool hov=InRect(addR,ctx.input.mousePos);
                dl.AddRectFilled(addR,hov?kAccentHover:kAccent,3.f);
                dl.AddRect(addR,kAccentHover,1.f,3.f);
                const char* addLbl="+ Add";
                const float32 tw=font.MeasureWidth(addLbl);
                font.RenderText(dl,{addX+(addW-tw)*0.5f,
                                    by+(bh-font.metrics.lineHeight)*0.5f+font.metrics.ascender},
                               addLbl,{255,255,255,255},addW,false);
                // Popup "Add" (simplifié — on ajoute un cube pour la démo)
                if (hov&&ctx.input.IsMouseClicked(0)&&
                    state.numObjects<NkVP3DState::MAX_OBJECTS) {
                    char nm[32]; ::snprintf(nm,sizeof(nm),"Cube%d",state.numObjects);
                    uint8 r = (140 + state.numObjects * 20) % 256;
                    AddObject(state,nm,NkVP3DObjectShape::NK_VP3D_CUBE, {r, 180, 255, 255});
                }
            }
        }
 
        // =====================================================================
        //  DrawGrid — grille infinie style Blender/Unreal
        // =====================================================================
 
        void NkUIViewport3D::DrawGrid(NkUIDrawList& dl, NkRect r,
                                       const NkVP3DConfig& cfg,
                                       const NkVP3DState& state,
                                       const ProjectCtx& pc) noexcept {
            if (!cfg.showGrid) return;
 
            // Plan de sol : grille XZ
            // On dessine 2 niveaux : sous-grille fine (1u) et grille principale (5u)
            const float32 coarseU = 1.f;    // unités monde entre lignes principales
            const float32 fineU   = 0.25f;  // sous-divisions
            const int32   nCoarse = 24;     // ±24 unités
            const int32   nFine   = 4;      // 4 sous-divisions par case
 
            dl.PushClipRect(r, true);
 
            // Plan solide (si mode solid activé dans la config grille)
            if (cfg.grid.solid) {
                const float32 ext = nCoarse * coarseU;
                const NkVec2 c0=Project(pc,-ext,0.f,-ext);
                const NkVec2 c1=Project(pc, ext,0.f,-ext);
                const NkVec2 c2=Project(pc, ext,0.f, ext);
                const NkVec2 c3=Project(pc,-ext,0.f, ext);
                dl.AddTriangleFilled(c0,c1,c2,cfg.grid.solidColor);
                dl.AddTriangleFilled(c0,c2,c3,cfg.grid.solidColor);
            }
 
            // Sous-grille fine
            for (int32 i=-nCoarse*nFine; i<=nCoarse*nFine; ++i) {
                if (i%nFine==0) continue; // ligne principale → traitée après
                const float32 t=fineU*static_cast<float32>(i);
                const float32 ext=nCoarse*coarseU;
                const float32 fade=1.f-Clamp01(::fabsf(t)/ext*1.2f);
                if (fade<0.05f) continue;
                const NkColor fc=kGridLineSub.WithAlpha(
                    static_cast<uint8>(kGridLineSub.a*fade));
                dl.AddLine(Project(pc,t,0.f,-ext),Project(pc,t,0.f,ext),fc,0.6f);
                dl.AddLine(Project(pc,-ext,0.f,t),Project(pc,ext,0.f,t),fc,0.6f);
            }
 
            // Grille principale
            for (int32 i=-nCoarse; i<=nCoarse; ++i) {
                if (i==0) continue; // axes traités séparément
                const float32 t=coarseU*static_cast<float32>(i);
                const float32 ext=nCoarse*coarseU;
                const float32 fade=1.f-Clamp01(::fabsf(t)/ext*1.1f);
                if (fade<0.04f) continue;
                const NkColor lc=kGridLine.WithAlpha(static_cast<uint8>(kGridLine.a*fade));
                dl.AddLine(Project(pc,t,0.f,-ext),Project(pc,t,0.f,ext),lc,0.8f);
                dl.AddLine(Project(pc,-ext,0.f,t),Project(pc,ext,0.f,t),lc,0.8f);
            }
 
            // Axes principaux (très visibles, longues)
            const float32 axExt=nCoarse*coarseU;
            dl.AddLine(Project(pc,-axExt,0.f,0.f),Project(pc,axExt,0.f,0.f),
                       kAxisX.WithAlpha(180),1.8f);
            dl.AddLine(Project(pc,0.f,-axExt,0.f),Project(pc,0.f,axExt,0.f),
                       kAxisY.WithAlpha(180),1.8f);
            dl.AddLine(Project(pc,0.f,0.f,-axExt),Project(pc,0.f,0.f,axExt),
                       kAxisZ.WithAlpha(180),1.8f);
 
            dl.PopClipRect();
            (void)state;
        }
 
        // =====================================================================
        //  DrawMiniAxes — coin bas-gauche
        // =====================================================================
 
        void NkUIViewport3D::DrawMiniAxes(NkUIDrawList& dl, NkRect viewport,
                                           const ProjectCtx& pc) noexcept {
            const float32 cx=viewport.x+38.f, cy=viewport.y+viewport.h-38.f;
            const float32 axLen=28.f;
 
            auto axis=[&](float32 wx,float32 wy,float32 wz,NkColor col,const char* lbl){
                const NkVec2 dir = pc.ProjectDir ? NkVec2{0.f, 0.f} : NkVec2{0.f, 0.f}; // unused
                // On projette juste la direction
                const NkVec2 o = pc.isPerspective ? NkVec2{cx,cy} : NkVec2{cx,cy};
 
                // Projection manuelle de la direction d'axe
                const float32 yawR=pc.yawR, pitchR=pc.pitchR;
                const float32 cy2=::cosf(yawR),sy2=::sinf(yawR);
                const float32 x1=wx*cy2-wz*sy2, z1=wx*sy2+wz*cy2;
                const float32 cp=::cosf(pitchR),sp=::sinf(pitchR);
                const float32 y2=wy*cp-z1*sp;
                // Ignorer Z (projection simple)
                const NkVec2 d={x1,y2};
                const float32 l=::sqrtf(d.x*d.x+d.y*d.y);
                const NkVec2 nd=l>1e-4f?NkVec2{d.x/l,d.y/l}:NkVec2{1.f,0.f};
                const NkVec2 tip={cx+nd.x*axLen,cy-nd.y*axLen};
                dl.AddLine({cx,cy},tip,col,2.f);
                dl.AddCircleFilled(tip,4.f,col,8);
                dl.AddText({tip.x+4.f,tip.y-6.f},lbl,col,10.f);
                (void)dir; (void)o;
            };
 
            // Fond semi-transparent
            dl.AddCircleFilled({cx,cy},axLen+8.f,{0,0,0,80},20);
            axis(1.f,0.f,0.f,kAxisX,"X");
            axis(0.f,1.f,0.f,kAxisY,"Y");
            axis(0.f,0.f,1.f,kAxisZ,"Z");
        }
 
        // =====================================================================
        //  DrawOrientationCube — coin haut-droit style Unreal/Blender
        // =====================================================================
 
        void NkUIViewport3D::DrawOrientationCube(NkUIDrawList& dl, NkUIFont& font,
                                                   NkRect viewport,
                                                   const NkVP3DCamera& cam) noexcept {
            const float32 sz=52.f;
            const float32 ox=viewport.x+viewport.w-sz-10.f;
            const float32 oy=viewport.y+10.f;
            const NkRect cubeR={ox,oy,sz,sz};
 
            // Fond
            dl.AddRectFilled(cubeR,{20,20,25,180},6.f);
            dl.AddRect(cubeR,kPanelBorder.WithAlpha(160),1.f,6.f);
 
            // Cube miniature
            const float32 cx=ox+sz*0.5f,cy=oy+sz*0.5f;
            const float32 cr=sz*0.30f;
            const float32 yr=cam.yaw*NKUI_PI/180.f;
            const float32 pr=cam.pitch*NKUI_PI/180.f;
 
            auto proj=[&](float32 wx,float32 wy,float32 wz)->NkVec2{
                const float32 c=::cosf(yr),s=::sinf(yr);
                const float32 x1=wx*c-wz*s,z1=wx*s+wz*c;
                const float32 cp=::cosf(pr),sp=::sinf(pr);
                const float32 y2=wy*cp-z1*sp;
                return {cx+x1*cr,cy-y2*cr};
            };
 
            NkVec2 v[8];
            v[0]=proj(-1.f,-1.f,-1.f); v[1]=proj(1.f,-1.f,-1.f);
            v[2]=proj(1.f, 1.f,-1.f); v[3]=proj(-1.f, 1.f,-1.f);
            v[4]=proj(-1.f,-1.f, 1.f); v[5]=proj(1.f,-1.f, 1.f);
            v[6]=proj(1.f, 1.f, 1.f); v[7]=proj(-1.f, 1.f, 1.f);
 
            const int32 edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
            for (const auto& e:edges)
                dl.AddLine(v[e[0]],v[e[1]],{180,180,200,200},1.2f);
 
            // Labels des faces
            auto faceCenter=[&](float32 wx,float32 wy,float32 wz,const char* lbl,NkColor col){
                const NkVec2 fc=proj(wx,wy,wz);
                const float32 tw=font.MeasureWidth(lbl);
                dl.AddText({fc.x-tw*0.5f,fc.y-4.f},lbl,col,9.f);
            };
            faceCenter( 0.f, 0.f,-1.4f,"F",kAxisZ);
            faceCenter( 0.f, 0.f, 1.4f,"B",kAxisZ.WithAlpha(140));
            faceCenter( 1.4f, 0.f, 0.f,"R",kAxisX);
            faceCenter(-1.4f, 0.f, 0.f,"L",kAxisX.WithAlpha(140));
            faceCenter( 0.f, 1.4f, 0.f,"T",kAxisY);
            faceCenter( 0.f,-1.4f, 0.f,"B",kAxisY.WithAlpha(140));
        }
 
        // =====================================================================
        //  DrawStats — overlay performances
        // =====================================================================
 
        void NkUIViewport3D::DrawStats(NkUIDrawList& dl, NkUIFont& font,
                                        NkRect viewport,
                                        const NkVP3DState& state) noexcept {
            const float32 x=viewport.x+8.f, y=viewport.y+8.f;
            const float32 lh=font.metrics.lineHeight+2.f;
            const float32 panW=160.f, panH=lh*4.f+10.f;
            dl.AddRectFilled({x,y,panW,panH},{0,0,0,140},4.f);
 
            char buf[64];
            float32 ly=y+6.f+font.metrics.ascender;
 
            ::snprintf(buf,sizeof(buf),"FPS: %.1f",state.fps);
            font.RenderText(dl,{x+6.f,ly},buf,kTextPrimary,panW-8.f,false);
            ly+=lh;
 
            ::snprintf(buf,sizeof(buf),"Objects: %d",state.numObjects);
            font.RenderText(dl,{x+6.f,ly},buf,kTextSecond,panW-8.f,false);
            ly+=lh;
 
            ::snprintf(buf,sizeof(buf),"Selected: %d",state.numSelected);
            font.RenderText(dl,{x+6.f,ly},buf,kTextSecond,panW-8.f,false);
            ly+=lh;
 
            const char* modeName=
                state.gizmoCfg.mode==NkUIGizmoMode::NK_TRANSLATE?"Translate":
                state.gizmoCfg.mode==NkUIGizmoMode::NK_ROTATE?"Rotate":"Scale";
            ::snprintf(buf,sizeof(buf),"Mode: %s",modeName);
            font.RenderText(dl,{x+6.f,ly},buf,kAccent,panW-8.f,false);
        }
 
        // =====================================================================
        //  DrawNavigationHelp — aide en bas
        // =====================================================================
 
        void NkUIViewport3D::DrawNavigationHelp(NkUIDrawList& dl, NkUIFont& font,
                                                 NkRect viewport) noexcept {
            const char* help =
                "LMB: Orbit  RMB: Pan  Wheel: Zoom  "
                "F: Focus  G: Move  R: Rotate  S: Scale  Del: Delete";
            const float32 tw=font.MeasureWidth(help);
            const float32 x=viewport.x+(viewport.w-tw)*0.5f;
            const float32 y=viewport.y+viewport.h-font.metrics.lineHeight-8.f;
            const float32 padH=4.f;
            dl.AddRectFilled({x-padH,y-padH-font.metrics.ascender,tw+padH*2.f,
                              font.metrics.lineHeight+padH*2.f},{0,0,0,130},3.f);
            font.RenderText(dl,{x,y},help,{150,155,170,210},tw+4.f,false);
        }
 
        // =====================================================================
        //  DrawSelectionOverlay — info transform sélection en overlay
        // =====================================================================
 
        void NkUIViewport3D::DrawSelectionOverlay(NkUIDrawList& dl, NkUIFont& font,
                                                   NkRect viewport,
                                                   const NkVP3DState& state,
                                                   const ProjectCtx& pc) noexcept {
            if (state.numSelected == 0) return;
            const NkVP3DObject& obj=state.objects[state.selectedIdx[0]];
 
            // Anneau de sélection au-dessus de l'objet
            const NkVec2 c=Project(pc,
                obj.transform.position.x,
                obj.transform.position.y,
                obj.transform.position.z);
            dl.AddCircle(c,18.f,kSelectionHL.WithAlpha(160),1.5f,20);
 
            // Info transform en petit texte sous l'objet
            if (!pc.isPerspective && state.gizmoCfg.active) {
                char buf[80];
                const auto& t=obj.transform;
                const auto& g=state.gizmoCfg;
                if (g.mode==NkUIGizmoMode::NK_TRANSLATE)
                    ::snprintf(buf,sizeof(buf),"%.2f, %.2f, %.2f",
                               t.position.x,t.position.y,t.position.z);
                else if (g.mode==NkUIGizmoMode::NK_ROTATE)
                    ::snprintf(buf,sizeof(buf),"%.1f, %.1f, %.1f deg",
                               t.rotationDeg.x,t.rotationDeg.y,t.rotationDeg.z);
                else
                    ::snprintf(buf,sizeof(buf),"%.2f, %.2f, %.2f", t.scale.x,t.scale.y,t.scale.z);
                const float32 tw=font.MeasureWidth(buf);
                const float32 bx=c.x-tw*0.5f, by2=c.y+24.f;
                dl.AddRectFilled({bx-4.f,by2-2.f,tw+8.f,font.metrics.lineHeight+4.f},{0,0,0,160},3.f);
                font.RenderText(dl,{bx,by2+font.metrics.ascender},buf,kSelectionHL,tw+4.f,false);
            }
            (void)pc;
        }
 
        // =====================================================================
        //  HandleNavigation
        // =====================================================================
 
        void NkUIViewport3D::HandleNavigation(NkUIContext& ctx, NkRect r,
                                               NkVP3DConfig& cfg,
                                               NkVP3DState& state,
                                               bool gizmoActive,
                                               float32 dt) noexcept {
            NkVP3DCamera& cam=state.camera;
            const NkVec2 mp=ctx.input.mousePos;
            const bool inView=InRect(r,mp);
 
            // ── Animation focus ────────────────────────────────────────────
            if (state.focusAnimating) {
                state.focusT += dt * 5.f; // vitesse de l'animation
                if (state.focusT >= 1.f) { state.focusT=1.f; state.focusAnimating=false; }
                const float32 t=state.focusT;
                // Ease in-out quadratique
                const float32 ease=t<0.5f?2.f*t*t:1.f-2.f*(1.f-t)*(1.f-t);
                cam.yaw     =Lerp(state.focusFrom.yaw,state.focusTo.yaw,ease);
                cam.pitch   =Lerp(state.focusFrom.pitch,state.focusTo.pitch,ease);
                cam.distance=Lerp(state.focusFrom.distance,state.focusTo.distance,ease);
                cam.pivotX  =Lerp(state.focusFrom.pivotX,state.focusTo.pivotX,ease);
                cam.pivotY  =Lerp(state.focusFrom.pivotY,state.focusTo.pivotY,ease);
                cam.pivotZ  =Lerp(state.focusFrom.pivotZ,state.focusTo.pivotZ,ease);
            }
 
            if (!inView) return;
 
            // ── Orbite (LMB) ──────────────────────────────────────────────
            if (!gizmoActive && ctx.input.IsMouseDown(0)) {
                if (!state.orbiting) {
                    if (!ctx.input.IsMouseClicked(0)) state.orbiting=true;
                }
            }
            if (state.orbiting && ctx.input.IsMouseDown(0)) {
                cam.yaw   += ctx.input.mouseDelta.x * cfg.orbitSensitivity;
                cam.pitch += ctx.input.mouseDelta.y * cfg.orbitSensitivity * 0.6f;
                cam.pitch  = cam.pitch> 89.f? 89.f:cam.pitch<-89.f?-89.f:cam.pitch;
            }
            if (!ctx.input.IsMouseDown(0)) state.orbiting=false;
 
            // ── Pan (RMB) ─────────────────────────────────────────────────
            if (ctx.input.IsMouseDown(1)) {
                if (!state.panning && !ctx.input.IsMouseClicked(1)) state.panning=true;
            }
            if (state.panning && ctx.input.IsMouseDown(1)) {
                // Pan dans le plan de la vue
                const float32 sen=cfg.panSensitivity*cam.distance;
                const float32 yr=cam.yaw*NKUI_PI/180.f;
                // Right = (cos(yaw), 0, sin(yaw))
                // Up    = (-sin(yaw)*sin(pitch), cos(pitch), cos(yaw)*sin(pitch))
                const float32 cy2=::cosf(yr),sy2=::sinf(yr);
                const float32 pr=cam.pitch*NKUI_PI/180.f;
                const float32 cp2=::cosf(pr),sp2=::sinf(pr);
                const float32 dx=ctx.input.mouseDelta.x*sen;
                const float32 dy=ctx.input.mouseDelta.y*sen;
                // right * -dx
                cam.pivotX -= cy2*dx;
                cam.pivotZ -= sy2*dx;
                // up * dy
                cam.pivotX += sy2*sp2*dy;
                cam.pivotY += cp2*dy;
                cam.pivotZ -= cy2*sp2*dy;
            }
            if (!ctx.input.IsMouseDown(1)) state.panning=false;
 
            // ── Zoom (molette) ────────────────────────────────────────────
            if (!ctx.wheelConsumed && ctx.input.mouseWheel!=0.f) {
                const float32 factor=1.f+ctx.input.mouseWheel*-0.12f*cfg.zoomSensitivity;
                cam.distance *= factor;
                cam.distance  = cam.distance>0.1f?cam.distance:0.1f;
                cam.distance  = cam.distance<500.f?cam.distance:500.f;
                ctx.wheelConsumed=true;
            }
        }
 
        // =====================================================================
        //  HandleKeyboardShortcuts
        // =====================================================================
 
        void NkUIViewport3D::HandleKeyboardShortcuts(NkUIContext& ctx,
                                                       NkVP3DConfig& cfg,
                                                       NkVP3DState& state,
                                                       NkRect viewport) noexcept {
            if (!InRect(viewport,ctx.input.mousePos)) return;
            (void)cfg;
 
            // G → Translate, R → Rotate, S → Scale
            if (ctx.input.IsKeyPressed(NkKey::NK_G))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_TRANSLATE;
            if (ctx.input.IsKeyPressed(NkKey::NK_R))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_ROTATE;
            if (ctx.input.IsKeyPressed(NkKey::NK_S))
                state.gizmoCfg.mode=NkUIGizmoMode::NK_SCALE;
 
            // F → Focus
            if (ctx.input.IsKeyPressed(NkKey::NK_F)) {
                state.focusFrom=state.camera;
                NkVP3DConfig tmpCfg=cfg;
                FocusSelected(state,tmpCfg);
                state.focusTo=state.camera;
                state.camera=state.focusFrom; // remettre, l'animation le fait
                state.focusAnimating=true;
                state.focusT=0.f;
            }
 
            // Del → supprimer sélection
            if (ctx.input.IsKeyPressed(NkKey::NK_DELETE) && state.numSelected>0) {
                // Undo snapshot
                state.hasUndo=false;
                // Compacter le tableau
                for (int32 k=state.numSelected-1;k>=0;--k){
                    const int32 idx=state.selectedIdx[k];
                    if (idx<0||idx>=state.numObjects) continue;
                    // Désélectionner si dans state
                    for (int32 j=idx;j<state.numObjects-1;++j)
                        state.objects[j]=state.objects[j+1];
                    --state.numObjects;
                }
                state.numSelected=0;
            }
 
            // Ctrl+Z → undo (1 niveau)
            if (ctx.input.ctrl && ctx.input.IsKeyPressed(NkKey::NK_Z) && state.hasUndo) {
                if (state.undoIdx>=0&&state.undoIdx<state.numObjects){
                    state.objects[state.undoIdx].transform=state.undoTransform;
                    state.hasUndo=false;
                }
            }
 
            // Vues numpad (simplifiées via touches numériques)
            if (ctx.input.IsKeyPressed(NkKey::NK_NUMPAD_0))
                { state.camera.mode=NkVP3DCameraMode::NK_VP3D_ORTHO_FRONT; state.camera.yaw=0; state.camera.pitch=0; }
            if (ctx.input.IsKeyPressed(NkKey::NK_NUMPAD_3))
                { state.camera.mode=NkVP3DCameraMode::NK_VP3D_ORTHO_RIGHT; state.camera.yaw=90; state.camera.pitch=0; }
            if (ctx.input.IsKeyPressed(NkKey::NK_NUMPAD_7))
                { state.camera.mode=NkVP3DCameraMode::NK_VP3D_ORTHO_TOP; state.camera.yaw=0; state.camera.pitch=90; }
            if (ctx.input.IsKeyPressed(NkKey::NK_NUMPAD_5))
                state.camera.mode=(state.camera.mode==NkVP3DCameraMode::NK_VP3D_PERSPECTIVE)
                    ?NkVP3DCameraMode::NK_VP3D_ORTHO_TOP
                    :NkVP3DCameraMode::NK_VP3D_PERSPECTIVE;
        }

                // =====================================================================
        //  DrawObjects — rendu + picking des objets 3D
        // =====================================================================
 
        void NkUIViewport3D::DrawObjects(NkUIContext& ctx, NkUIDrawList& dl,
                                          NkUIFont& font, NkRect r,
                                          NkVP3DConfig& cfg,
                                          NkVP3DState& state,
                                          const ProjectCtx& pc,
                                          bool leftClick,
                                          NkVP3DResult& result) noexcept {
            // Trier par profondeur (Z) approximative pour l'ordre de rendu
            // (simple — pas de tri de face, juste ordre des objets)
 
            // Meilleur candidat picking
            int32 bestPickIdx = -1;
            float32 bestPickDist = 1e9f;
 
            for (int32 oi=0;oi<state.numObjects;++oi) {
                NkVP3DObject& obj=state.objects[oi];
                if (!obj.visible) continue;
 
                bool isSel=false;
                for (int32 k=0;k<state.numSelected;++k)
                    if (state.selectedIdx[k]==oi){isSel=true;break;}
 
                const bool isHov=(state.outlinerHovered==oi);
 
                // Rendu de la forme
                DrawObjectShape(dl,r,obj,pc,isSel,isHov,cfg,state.shadingMode);
 
                // Picking
                if (leftClick) {
                    const float32 d=PickObject(obj,pc,ctx.input.mousePos);
                    if (d>=0.f && d<bestPickDist) {
                        bestPickDist=d; bestPickIdx=oi;
                    }
                }
 
                // Label de l'objet (si sélectionné ou survolé)
                if (isSel||isHov) {
                    const NkVec2 c=Project(pc,
                        obj.transform.position.x,
                        obj.transform.position.y,
                        obj.transform.position.z);
                    if (InRect(r,c)) {
                        const float32 tw=font.MeasureWidth(obj.name);
                        font.RenderText(dl,{c.x-tw*0.5f,
                                            c.y-22.f+font.metrics.ascender},
                                       obj.name,
                                       isSel?kSelectionHL:NkColor{200,200,200,160},
                                       tw+4.f,false);
                    }
                }
            }
 
            // Appliquer le picking
            if (leftClick) {
                const bool ctrl=ctx.input.ctrl;
                if (bestPickIdx>=0) {
                    // Toggle ou sélection unique
                    bool alreadySel=false;
                    int32 selPos=-1;
                    for (int32 k=0;k<state.numSelected;++k)
                        if (state.selectedIdx[k]==bestPickIdx){alreadySel=true;selPos=k;break;}
 
                    if (ctrl) {
                        if (alreadySel) {
                            // Déselectionner
                            for (int32 j=selPos;j<state.numSelected-1;++j)
                                state.selectedIdx[j]=state.selectedIdx[j+1];
                            --state.numSelected;
                        } else {
                            if (state.numSelected<NkVP3DState::MAX_OBJECTS)
                                state.selectedIdx[state.numSelected++]=bestPickIdx;
                        }
                    } else {
                        state.numSelected=1;
                        state.selectedIdx[0]=bestPickIdx;
                    }
                    result.event=NkVP3DEvent::NK_VP3D_OBJECT_SELECTED;
                    result.objIdx=bestPickIdx;
                } else if (!ctrl) {
                    // Clic dans le vide → déselectionner
                    state.numSelected=0;
                    result.event=NkVP3DEvent::NK_VP3D_OBJECT_DESELECTED;
                }
            }
            (void)font;
        }
 
        // =====================================================================
        //  DrawGizmo — gizmo sur l'objet sélectionné
        // =====================================================================
 
        void NkUIViewport3D::DrawGizmo(NkUIContext& ctx, NkUIDrawList& dl,
                                        NkRect r,
                                        NkVP3DConfig& cfg,
                                        NkVP3DState& state,
                                        const ProjectCtx& pc,
                                        NkVP3DResult& result) noexcept {
            if (state.numSelected==0) return;
            const int32 idx=state.selectedIdx[0];
            if (idx<0||idx>=state.numObjects) return;
 
            NkVP3DObject& obj=state.objects[idx];
            if (obj.locked) return;
 
            const float32 px=obj.transform.position.x;
            const float32 py=obj.transform.position.y;
            const float32 pz=obj.transform.position.z;
 
            const NkVec2 orig=Project(pc,px,py,pz);
            const NkVec2 axX =Project(pc,px+1.f,py,pz);
            const NkVec2 axY =Project(pc,px,py+1.f,pz);
            const NkVec2 axZ =Project(pc,px,py,pz+1.f);
 
            auto norm2=[](NkVec2 v)->NkVec2{
                const float32 l=::sqrtf(v.x*v.x+v.y*v.y);
                return l>0.001f?NkVec2{v.x/l,v.y/l}:NkVec2{1.f,0.f};};
 
            NkUIGizmo3DDesc gdesc;
            gdesc.viewport      = r;
            gdesc.originPx      = orig;
            gdesc.axisXDirPx    = norm2({axX.x-orig.x,axX.y-orig.y});
            gdesc.axisYDirPx    = norm2({axY.x-orig.x,axY.y-orig.y});
            gdesc.axisZDirPx    = norm2({axZ.x-orig.x,axZ.y-orig.y});
            gdesc.unitsPerPixel = 1.f/pc.scale;
 
            // Undo snapshot avant le drag
            if (!obj.gizmoState.active && ctx.input.IsMouseClicked(0))
            {
                // Vérifie qu'on va commencer un drag
                // (ConsumeMouseClick consommera le clic si le gizmo le prend)
            }
 
            const NkUIGizmoTransform prevT=obj.transform;
            NkUIGizmoResult gr=NkUIGizmo::Manipulate3D(
                ctx,dl,gdesc,state.gizmoCfg,obj.gizmoState,obj.transform);
 
            if (gr.changed) {
                // Undo snapshot
                if (!state.hasUndo) {
                    state.hasUndo=true;
                    state.undoIdx=idx;
                    state.undoTransform=prevT;
                }
                obj.transform.position.x += gr.deltaPosition.x;
                obj.transform.position.y += gr.deltaPosition.y;
                obj.transform.position.z += gr.deltaPosition.z;
                obj.transform.scale.x=::fmaxf(0.001f,obj.transform.scale.x+gr.deltaScale.x);
                obj.transform.scale.y=::fmaxf(0.001f,obj.transform.scale.y+gr.deltaScale.y);
                obj.transform.scale.z=::fmaxf(0.001f,obj.transform.scale.z+gr.deltaScale.z);
                // Rotation déjà appliquée dans Manipulate3D
                result.event=NkVP3DEvent::NK_VP3D_OBJECT_TRANSFORMED;
                result.objIdx=idx;
                result.newTransform=obj.transform;
            }
            (void)cfg;
        }
 
        // =====================================================================
        //  DrawViewport — la zone 3D principale
        // =====================================================================
 
        NkVP3DResult NkUIViewport3D::DrawViewport(NkUIContext& ctx, NkUIDrawList& dl,
                                                    NkUIFont& font, NkUIID id,
                                                    NkRect r,
                                                    NkVP3DConfig& cfg,
                                                    NkVP3DState& state,
                                                    float32 dt) noexcept {
            NkVP3DResult result{};
 
            // Fond dégradé subtil
            dl.AddRectFilled(r,cfg.bgGradBot);
            // Dégradé du haut vers le bas (simplifié — deux triangles)
            dl.AddTriangleFilled({r.x,r.y},{r.x+r.w,r.y},{r.x,r.y+r.h*0.4f},
                                 cfg.bgGradTop.WithAlpha(60));
            dl.AddTriangleFilled({r.x+r.w,r.y},{r.x+r.w,r.y+r.h*0.4f},{r.x,r.y+r.h*0.4f},
                                 cfg.bgGradTop.WithAlpha(60));
 
            dl.PushClipRect(r,true);
 
            // FPS
            state.fpsAccum+=dt; ++state.fpsFrames;
            if (state.fpsAccum>=0.5f){
                state.fps=static_cast<float32>(state.fpsFrames)/state.fpsAccum;
                state.fpsAccum=0.f; state.fpsFrames=0;
            }
 
            // Contexte de projection
            NkVP3DCamera viewCam=state.camera;
            // Appliquer le pivot
            // (on translate le centre de projection selon le pivot)
            ProjectCtx pc=MakeProjectCtx(viewCam,r);
            // Ajuster le centre avec le pivot
            const NkVec2 pivotScreen=Project(pc,viewCam.pivotX,viewCam.pivotY,viewCam.pivotZ);
            const NkVec2 viewCenter={r.x+r.w*0.5f,r.y+r.h*0.5f};
            pc.center.x+=viewCenter.x-pivotScreen.x;
            pc.center.y+=viewCenter.y-pivotScreen.y;
 
            // Grille
            DrawGrid(dl,r,cfg,state,pc);
 
            // Gizmo actif ?
            bool gizmoActive=false;
            if (state.numSelected>0){
                const int32 idx=state.selectedIdx[0];
                if (idx>=0&&idx<state.numObjects)
                    gizmoActive=state.objects[idx].gizmoState.active||
                                state.objects[idx].gizmoState.hovered;
            }
 
            // Navigation
            HandleNavigation(ctx,r,cfg,state,gizmoActive,dt);
            HandleKeyboardShortcuts(ctx,cfg,state,r);
 
            // Pick = clic gauche sans drag en cours et sans gizmo
            const bool leftClick=InRect(r,ctx.input.mousePos)&&
                ctx.input.IsMouseClicked(0)&&!gizmoActive&&!state.orbiting;
 
            // Objets
            DrawObjects(ctx,dl,font,r,cfg,state,pc,leftClick,result);
 
            // Gizmo
            DrawGizmo(ctx,dl,r,cfg,state,pc,result);
 
            // Overlays
            if (cfg.showMiniAxes) DrawMiniAxes(dl,r,pc);
            if (cfg.showOrientationCube) DrawOrientationCube(dl,font,r,viewCam);
            if (cfg.showStats) DrawStats(dl,font,r,state);
            if (cfg.showSelectionInfo&&state.numSelected>0)
                DrawSelectionOverlay(dl,font,r,state,pc);
            DrawNavigationHelp(dl,font,r);
 
            // Bordure viewport
            dl.AddRect(r,kPanelBorder.WithAlpha(120),1.f);
 
            dl.PopClipRect();
            (void)id;
            return result;
        }
 
        // =====================================================================
        //  DrawOutliner — panneau liste objets (style Unreal)
        // =====================================================================
 
        void NkUIViewport3D::DrawOutliner(NkUIContext& ctx, NkUIDrawList& dl,
                                           NkUIFont& font, NkRect r,
                                           NkVP3DConfig& cfg,
                                           NkVP3DState& state) noexcept {
            (void)cfg;
            // Fond panneau
            dl.AddRectFilled(r,kPanelBg);
            dl.AddRect(r,kPanelBorder.WithAlpha(160),1.f);
 
            // Titre
            const float32 titleH=22.f;
            const NkRect titleR={r.x,r.y,r.w,titleH};
            dl.AddRectFilled(titleR,kSectionBg);
            dl.AddLine({r.x,r.y+titleH},{r.x+r.w,r.y+titleH},kPanelBorder.WithAlpha(150),1.f);
            const float32 asc=font.metrics.ascender;
            font.RenderText(dl,{r.x+8.f,r.y+(titleH-font.metrics.lineHeight)*0.5f+asc},
                           "Outliner",kTextPrimary,r.w-10.f,false);
 
            // Icône filtre en haut-droit
            const NkRect filterR={r.x+r.w-20.f,r.y+2.f,18.f,18.f};
            const bool fHov=InRect(filterR,ctx.input.mousePos);
            if (fHov) dl.AddRectFilled(filterR,kHoverBg,3.f);
            dl.AddLine({filterR.x+3.f,filterR.y+5.f},{filterR.x+filterR.w-3.f,filterR.y+5.f},kTextSecond,1.2f);
            dl.AddLine({filterR.x+5.f,filterR.y+9.f},{filterR.x+filterR.w-5.f,filterR.y+9.f},kTextSecond,1.2f);
            dl.AddLine({filterR.x+7.f,filterR.y+13.f},{filterR.x+filterR.w-7.f,filterR.y+13.f},kTextSecond,1.2f);
 
            dl.PushClipRect({r.x,r.y+titleH,r.w,r.h-titleH},true);
 
            const float32 rowH=22.f;
            const float32 indentX=r.x+6.f;
            float32 y=r.y+titleH+2.f;
            state.outlinerHovered=-1;
 
            for (int32 oi=0;oi<state.numObjects;++oi) {
                NkVP3DObject& obj=state.objects[oi];
                if (y+rowH>r.y+r.h) break;
 
                bool isSel=false;
                for (int32 k=0;k<state.numSelected;++k)
                    if (state.selectedIdx[k]==oi){isSel=true;break;}
 
                const NkRect row={r.x,y,r.w,rowH};
                const bool hov=InRect(row,ctx.input.mousePos);
                if (hov) state.outlinerHovered=oi;
 
                // Fond ligne
                if      (isSel) dl.AddRectFilled(row,kSelectBg.WithAlpha(160),2.f);
                else if (hov)   dl.AddRectFilled(row,kHoverBg,2.f);
 
                // Icône visibilité (œil)
                const NkRect eyeR={r.x+r.w-18.f,y+(rowH-14.f)*0.5f,14.f,14.f};
                const NkColor eyeCol=obj.visible?kTextSecond:kTextDisabled;
                dl.AddCircle({eyeR.x+eyeR.w*0.5f,eyeR.y+eyeR.h*0.5f},5.f,eyeCol,1.f,8);
                dl.AddCircleFilled({eyeR.x+eyeR.w*0.5f,eyeR.y+eyeR.h*0.5f},2.f,eyeCol,6);
                if (InRect(eyeR,ctx.input.mousePos)&&ctx.input.IsMouseClicked(0))
                    obj.visible=!obj.visible;
 
                // Icône verrou
                const NkRect lockR={r.x+r.w-34.f,y+(rowH-14.f)*0.5f,14.f,14.f};
                const NkColor lockCol=obj.locked?kSelectionHL:kTextDisabled;
                dl.AddRect({lockR.x+2.f,lockR.y+6.f,10.f,8.f},lockCol,1.f,1.f);
                dl.AddArc({lockR.x+7.f,lockR.y+7.f},4.f,NKUI_PI,NKUI_PI*2.f,lockCol,1.f,6);
                if (InRect(lockR,ctx.input.mousePos)&&ctx.input.IsMouseClicked(0))
                    obj.locked=!obj.locked;
 
                // Icône type de forme
                const NkRect icoR={indentX,y+(rowH-16.f)*0.5f,16.f,16.f};
                DrawShapeIcon(dl,icoR,obj.shape,isSel?kSelectionHL:kTextSecond);
 
                // Nom
                const float32 nameX=indentX+20.f;
                const float32 nameW=r.x+r.w-nameX-40.f;
                const float32 tyy=y+(rowH-font.metrics.lineHeight)*0.5f+asc;
                font.RenderText(dl,{nameX,tyy},obj.name,
                    isSel?kTextPrimary:hov?NkColor{200,200,205,255}:kTextSecond,
                    nameW,true);
 
                // Clic sélection
                if (hov&&!InRect(eyeR,ctx.input.mousePos)&&!InRect(lockR,ctx.input.mousePos)
                    &&ctx.input.IsMouseClicked(0)) {
                    const bool ctrl=ctx.input.ctrl;
                    if (ctrl) {
                        bool already=false;
                        int32 pos=-1;
                        for (int32 k=0;k<state.numSelected;++k)
                            if (state.selectedIdx[k]==oi){already=true;pos=k;break;}
                        if (already){
                            for (int32 j=pos;j<state.numSelected-1;++j)
                                state.selectedIdx[j]=state.selectedIdx[j+1];
                            --state.numSelected;
                        } else {
                            if (state.numSelected<NkVP3DState::MAX_OBJECTS)
                                state.selectedIdx[state.numSelected++]=oi;
                        }
                    } else {
                        state.numSelected=1;
                        state.selectedIdx[0]=oi;
                    }
                }
 
                y+=rowH;
            }
 
            dl.PopClipRect();
        }
 
        // =====================================================================
        //  DrawDetails — panneau transform/propriétés (style Unreal)
        // =====================================================================
 
        void NkUIViewport3D::DrawDetails(NkUIContext& ctx, NkUIDrawList& dl,
                                          NkUIFont& font, NkRect r,
                                          NkVP3DConfig& cfg,
                                          NkVP3DState& state) noexcept {
            (void)cfg;
            dl.AddRectFilled(r,kPanelBg);
            dl.AddRect(r,kPanelBorder.WithAlpha(160),1.f);
 
            const float32 asc=font.metrics.ascender;
            const float32 lh=font.metrics.lineHeight;
            const float32 titleH=22.f;
 
            // ── Titre ──────────────────────────────────────────────────────
            const NkRect titleR={r.x,r.y,r.w,titleH};
            dl.AddRectFilled(titleR,kSectionBg);
            dl.AddLine({r.x,r.y+titleH},{r.x+r.w,r.y+titleH},kPanelBorder.WithAlpha(150),1.f);
            font.RenderText(dl,{r.x+8.f,r.y+(titleH-lh)*0.5f+asc},
                           "Details",kTextPrimary,r.w-10.f,false);
 
            if (state.numSelected==0) {
                font.RenderText(dl,{r.x+8.f,r.y+titleH+12.f+asc},
                               "No selection",kTextDisabled,r.w-10.f,false);
                return;
            }
 
            const int32 idx=state.selectedIdx[0];
            if (idx<0||idx>=state.numObjects) return;
            NkVP3DObject& obj=state.objects[idx];
 
            dl.PushClipRect({r.x,r.y+titleH,r.w,r.h-titleH},true);
 
            float32 y=r.y+titleH+6.f;
            const float32 ph=8.f, fieldW=r.w-ph*2.f;
 
            // ── Nom de l'objet ─────────────────────────────────────────────
            {
                const NkRect sec={r.x,y,r.w,20.f};
                dl.AddRectFilled(sec,kSectionBg.WithAlpha(80));
                font.RenderText(dl,{r.x+ph,y+(20.f-lh)*0.5f+asc},obj.name,
                               kSelectionHL,fieldW,true);
                font.RenderText(dl,{r.x+r.w-font.MeasureWidth(ShapeName(obj.shape))-ph,
                                    y+(20.f-lh)*0.5f+asc},ShapeName(obj.shape),
                               kTextSecond,fieldW,false);
                y+=22.f;
                dl.AddLine({r.x,y},{r.x+r.w,y},kPanelBorder.WithAlpha(100),1.f);
                y+=4.f;
            }
 
            // Helper pour une ligne label + valeur
            auto row3=[&](const char* label, float32& x, float32& yy, float32& z, NkColor col){
                font.RenderText(dl,{r.x+ph,yy+asc},label,kTextSecond,40.f,false);
                yy+=lh+2.f;
                const float32 fw=(fieldW-6.f)/3.f;
                char buf[32];
                struct { const char* lbl; float32* val; NkColor col2; } comps[]={
                    {"X",&x,kAxisX},{"Y",&yy,kAxisY},{"Z",&z,kAxisZ}};
                float32 fx=r.x+ph;
                for (auto& c2:comps){
                    const NkRect fr={fx,yy,fw-2.f,lh+4.f};
                    dl.AddRectFilled(fr,{30,30,35,200},2.f);
                    dl.AddRect(fr,c2.col2.WithAlpha(100),1.f,2.f);
                    ::snprintf(buf,sizeof(buf),"%.3f",*c2.val);
                    font.RenderText(dl,{fr.x+3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   buf,col,fw-6.f,false);
                    const float32 lbW=font.MeasureWidth(c2.lbl);
                    font.RenderText(dl,{fr.x+fw-lbW-3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   c2.lbl,c2.col2,lbW+2.f,false);
                    fx+=fw+1.f;
                }
                yy+=lh+8.f;
                (void)col;
            };
 
            // ── Transform ─────────────────────────────────────────────────
            {
                // Header section
                dl.AddRectFilled({r.x,y,r.w,18.f},kSectionBg);
                font.RenderText(dl,{r.x+ph,y+(18.f-lh)*0.5f+asc},"Transform",kAccent,fieldW,false);
                y+=20.f;
            }
 
            // Position
            {
                float32 dummy=y; // on ne veut pas que row3 modifie y via yy
                float32 px2=obj.transform.position.x;
                float32 py2=obj.transform.position.y;
                float32 pz2=obj.transform.position.z;
                font.RenderText(dl,{r.x+ph,y+asc},"Location",kTextSecond,fieldW,false);
                y+=lh+2.f;
                const float32 fw=(fieldW-6.f)/3.f;
                char buf[32];
                struct { const char* lbl; float32 val; NkColor col2; } comps[]={
                    {"X",px2,kAxisX},{"Y",py2,kAxisY},{"Z",pz2,kAxisZ}};
                float32 fx=r.x+ph;
                for (auto& c2:comps){
                    const NkRect fr={fx,y,fw-2.f,lh+4.f};
                    dl.AddRectFilled(fr,{30,30,35,200},2.f);
                    dl.AddRect(fr,c2.col2.WithAlpha(100),1.f,2.f);
                    ::snprintf(buf,sizeof(buf),"%.3f",c2.val);
                    font.RenderText(dl,{fr.x+3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   buf,kTextPrimary,fw-6.f,false);
                    const float32 lbW=font.MeasureWidth(c2.lbl);
                    font.RenderText(dl,{fr.x+fw-lbW-3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   c2.lbl,c2.col2,lbW+2.f,false);
                    fx+=fw+1.f;
                }
                y+=lh+8.f;
                (void)dummy;
            }
 
            // Rotation
            {
                font.RenderText(dl,{r.x+ph,y+asc},"Rotation",kTextSecond,fieldW,false);
                y+=lh+2.f;
                const float32 fw=(fieldW-6.f)/3.f;
                char buf[32];
                struct { const char* lbl; float32 val; NkColor col2; } comps[]={
                    {"X",obj.transform.rotationDeg.x,kAxisX},
                    {"Y",obj.transform.rotationDeg.y,kAxisY},
                    {"Z",obj.transform.rotationDeg.z,kAxisZ}};
                float32 fx=r.x+ph;
                for (auto& c2:comps){
                    const NkRect fr={fx,y,fw-2.f,lh+4.f};
                    dl.AddRectFilled(fr,{30,30,35,200},2.f);
                    dl.AddRect(fr,c2.col2.WithAlpha(100),1.f,2.f);
                    ::snprintf(buf,sizeof(buf),"%.1f",c2.val);
                    font.RenderText(dl,{fr.x+3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   buf,kTextPrimary,fw-6.f,false);
                    const float32 lbW=font.MeasureWidth(c2.lbl);
                    font.RenderText(dl,{fr.x+fw-lbW-3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   c2.lbl,c2.col2,lbW+2.f,false);
                    fx+=fw+1.f;
                }
                y+=lh+8.f;
            }
 
            // Scale
            {
                font.RenderText(dl,{r.x+ph,y+asc},"Scale",kTextSecond,fieldW,false);
                y+=lh+2.f;
                const float32 fw=(fieldW-6.f)/3.f;
                char buf[32];
                struct { const char* lbl; float32 val; NkColor col2; } comps[]={
                    {"X",obj.transform.scale.x,kAxisX},
                    {"Y",obj.transform.scale.y,kAxisY},
                    {"Z",obj.transform.scale.z,kAxisZ}};
                float32 fx=r.x+ph;
                for (auto& c2:comps){
                    const NkRect fr={fx,y,fw-2.f,lh+4.f};
                    dl.AddRectFilled(fr,{30,30,35,200},2.f);
                    dl.AddRect(fr,c2.col2.WithAlpha(100),1.f,2.f);
                    ::snprintf(buf,sizeof(buf),"%.3f",c2.val);
                    font.RenderText(dl,{fr.x+3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   buf,kTextPrimary,fw-6.f,false);
                    const float32 lbW=font.MeasureWidth(c2.lbl);
                    font.RenderText(dl,{fr.x+fw-lbW-3.f,fr.y+(fr.h-lh)*0.5f+asc},
                                   c2.lbl,c2.col2,lbW+2.f,false);
                    fx+=fw+1.f;
                }
                y+=lh+8.f;
            }
 
            // Séparateur
            dl.AddLine({r.x,y},{r.x+r.w,y},kPanelBorder.WithAlpha(100),1.f);
            y+=6.f;
 
            // Couleur de l'objet
            {
                dl.AddRectFilled({r.x,y,r.w,18.f},kSectionBg);
                font.RenderText(dl,{r.x+ph,y+(18.f-lh)*0.5f+asc},"Appearance",kAccent,fieldW,false);
                y+=22.f;
                const NkRect cr={r.x+ph,y,fieldW,16.f};
                dl.AddRectFilled(cr,obj.color,3.f);
                dl.AddRect(cr,{255,255,255,40},1.f,3.f);
                y+=20.f;
            }
 
            // Bouton Reset Transform
            y+=4.f;
            {
                const NkRect resetR={r.x+ph,y,fieldW,22.f};
                const bool hov=InRect(resetR,ctx.input.mousePos);
                dl.AddRectFilled(resetR, hov ? kHoverBg : NkColor(35, 35, 40, 255), 3.f);
                dl.AddRect(resetR,kPanelBorder.WithAlpha(160),1.f,3.f);
                const char* lbl="Reset Transform";
                const float32 tw=font.MeasureWidth(lbl);
                font.RenderText(dl,{resetR.x+(fieldW-tw)*0.5f,
                                    resetR.y+(22.f-lh)*0.5f+asc},
                               lbl,kTextPrimary,fieldW,false);
                if (hov&&ctx.input.IsMouseClicked(0)){
                    obj.transform.position={};
                    obj.transform.rotationDeg={};
                    obj.transform.scale={1.f,1.f,1.f};
                }
            }
 
            dl.PopClipRect();
        }
 
        // =====================================================================
        //  DrawStatusBar — barre de statut style Unreal
        // =====================================================================
 
        void NkUIViewport3D::DrawStatusBar(NkUIContext& ctx, NkUIDrawList& dl,
                                            NkUIFont& font, NkRect r,
                                            const NkVP3DConfig& cfg,
                                            const NkVP3DState& state) noexcept {
            (void)cfg; (void)ctx;
            dl.AddRectFilled(r,kToolbarBg);
            dl.AddLine({r.x,r.y},{r.x+r.w,r.y},kPanelBorder.WithAlpha(180),1.f);
 
            const float32 ph=8.f;
            const float32 bly=r.y+(r.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
            float32 cx=r.x+ph;
 
            auto stat=[&](const char* label, NkColor col=kTextSecond){
                font.RenderText(dl,{cx,bly},label,col,500.f,false);
                cx+=font.MeasureWidth(label)+16.f;
                dl.AddLine({cx-8.f,r.y+4.f},{cx-8.f,r.y+r.h-4.f},
                           kPanelBorder.WithAlpha(100),1.f);
            };
 
            char buf[80];
            // Position caméra
            ::snprintf(buf,sizeof(buf),"Cam  Yaw:%.0f  Pitch:%.0f  Dist:%.1f",
                state.camera.yaw,state.camera.pitch,state.camera.distance);
            stat(buf);
 
            // Mode
            const char* modeNames[]={
                "Perspective","Front (Ortho)","Back (Ortho)",
                "Left (Ortho)","Right (Ortho)","Top (Ortho)","Bottom (Ortho)"};
            stat(modeNames[static_cast<int32>(state.camera.mode)],kAccent);
 
            // Sélection
            if (state.numSelected>0){
                const NkVP3DObject& obj=state.objects[state.selectedIdx[0]];
                ::snprintf(buf,sizeof(buf),"Sel: %s  Pos(%.2f,%.2f,%.2f)",
                    obj.name,
                    obj.transform.position.x,
                    obj.transform.position.y,
                    obj.transform.position.z);
                stat(buf,kSelectionHL);
            }
 
            // Raccourcis à droite
            const char* help="G:Move  R:Rotate  S:Scale  F:Focus  Del:Delete  Ctrl+Z:Undo";
            const float32 hw=font.MeasureWidth(help);
            font.RenderText(dl,{r.x+r.w-hw-ph,bly},help,kTextDisabled,hw+4.f,false);
        }
 
        // =====================================================================
        //  NkUIViewport3D::Draw — point d'entrée principal
        // =====================================================================
 
        NkVP3DResult NkUIViewport3D::Draw(NkUIContext& ctx,
                                           NkUIDrawList& dl,
                                           NkUIFont& font,
                                           NkUIID id,
                                           NkRect rect,
                                           NkVP3DConfig& cfg,
                                           NkVP3DState& state,
                                           float32 dt) noexcept {
            NkVP3DResult result{};
 
            // Fond global
            dl.AddRectFilled(rect,kPanelBg,4.f);
            dl.AddRect(rect,kPanelBorder,1.f,4.f);
            dl.PushClipRect(rect,true);
 
            const Layout ly=ComputeLayout(rect,cfg);
 
            // Toolbar
            DrawToolbar(ctx,dl,font,ly.toolbar,cfg,state);
 
            // Viewport 3D
            {
                NkVP3DResult vr=DrawViewport(ctx,dl,font,id,ly.viewport3D,cfg,state,dt);
                if (vr.event!=NkVP3DEvent::NK_VP3D_NONE) result=vr;
            }
 
            // Outliner
            DrawOutliner(ctx,dl,font,ly.outliner,cfg,state);
 
            // Details
            DrawDetails(ctx,dl,font,ly.details,cfg,state);
 
            // Status bar
            DrawStatusBar(ctx,dl,font,ly.statusBar,cfg,state);
 
            // Ligne séparation outliner/viewport
            dl.AddLine({ly.outliner.x,ly.outliner.y},
                       {ly.outliner.x,ly.outliner.y+ly.outliner.h+ly.details.h},
                       kPanelBorder,1.f);
 
            dl.PopClipRect();
            return result;
        }
    } // namespace nkui
} // namespace nkentseu