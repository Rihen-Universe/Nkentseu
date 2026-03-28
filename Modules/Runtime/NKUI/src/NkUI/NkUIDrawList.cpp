/**
 * @File    NkUIDrawList.cpp
 * @Brief   Implémentation NkUIDrawList — tessellation, primitives, path.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Primitive tessellation and draw command generation.
 * Main data: Rect/line/circle/path/image emission and clip handling.
 * Change this file when: Rendering artifacts or primitive generation bugs appear.
 */
#include "NkUI/NkUIDrawList.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Init / Destroy / Reset
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDrawList::Init(uint32 maxVtx,uint32 maxIdx,uint32 maxCmds) noexcept {
            vtx=static_cast<NkUIVertex*>(memory::NkAlloc(maxVtx*sizeof(NkUIVertex)));
            idx=static_cast<uint32*>(memory::NkAlloc(maxIdx*sizeof(uint32)));
            cmds=static_cast<NkUIDrawCmd*>(memory::NkAlloc(maxCmds*sizeof(NkUIDrawCmd)));
            if(!vtx||!idx||!cmds){Destroy();return false;}
            vtxCap=maxVtx; idxCap=maxIdx; cmdCap=maxCmds;
            Reset();
            return true;
        }

        void NkUIDrawList::Destroy() noexcept {
            memory::NkFree(vtx);memory::NkFree(idx);memory::NkFree(cmds);
            vtx=nullptr;idx=nullptr;cmds=nullptr;
            vtxCount=idxCount=cmdCount=0;vtxCap=idxCap=cmdCap=0;
        }

        void NkUIDrawList::Reset() noexcept {
            vtxCount=0;idxCount=0;cmdCount=0;
            clipDepth=0;pathCount=0;
            opaqueCount=0;
            fillColor=NkColor::White();
            strokeColor=NkColor::Black();
            strokeWidth=1.f;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Helpers internes
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::ReserveVtx(uint32 n) noexcept {
            if(vtxCount+n>vtxCap){
                // Grow ×2
                const uint32 newCap=(vtxCap*2>vtxCount+n)?vtxCap*2:vtxCount+n+256;
                vtx=static_cast<NkUIVertex*>(memory::NkRealloc(vtx,vtxCap*sizeof(NkUIVertex),newCap*sizeof(NkUIVertex)));
                vtxCap=newCap;
            }
        }
        void NkUIDrawList::ReserveIdx(uint32 n) noexcept {
            if(idxCount+n>idxCap){
                const uint32 newCap=(idxCap*2>idxCount+n)?idxCap*2:idxCount+n+256;
                idx=static_cast<uint32*>(memory::NkRealloc(idx,idxCap*sizeof(uint32),newCap*sizeof(uint32)));
                idxCap=newCap;
            }
        }
        // ─────────────────────────────────────────────────────────────────────────────
        //  Culling helpers
        // ─────────────────────────────────────────────────────────────────────────────

        // NOTE:
        // L'occlusion "par historique" (primitives déjà dessinées) n'est pas valide
        // en immediate-mode UI: les widgets émis plus tard sont souvent au-dessus.
        // Ce culling supprimait des panneaux/contrôles légitimes.
        bool NkUIDrawList::IsOccluded(NkRect r) const noexcept {
            (void)r;
            return false;
        }

        void NkUIDrawList::TrackOpaque(NkRect r, NkColor col) noexcept {
            (void)r;
            (void)col;
        }

        void NkUIDrawList::EnsureCmd() noexcept {
            const NkRect clip = GetClipRect();
            const bool needNew =
                (cmdCount == 0)
                || (cmds[cmdCount - 1].type != NkUIDrawCmdType::NK_TRIANGLES)
                || (cmds[cmdCount - 1].clipRect.x != clip.x)
                || (cmds[cmdCount - 1].clipRect.y != clip.y)
                || (cmds[cmdCount - 1].clipRect.w != clip.w)
                || (cmds[cmdCount - 1].clipRect.h != clip.h);

            if(needNew){
                if(cmdCount>=cmdCap) return;
                NkUIDrawCmd& c=cmds[cmdCount++];
                c.type=NkUIDrawCmdType::NK_TRIANGLES;
                c.idxOffset=idxCount;
                c.idxCount=0;
                c.clipRect=clip;
            }
        }

        uint32 NkUIDrawList::VtxWrite(NkVec2 pos,NkVec2 uv,NkColor col) noexcept {
            if(vtxCount>=vtxCap) ReserveVtx(4);
            const uint32 i=vtxCount++;
            vtx[i].pos=pos; vtx[i].uv=uv; vtx[i].col=col.ToU32();
            return i;
        }
        void NkUIDrawList::IdxWrite(uint32 a,uint32 b,uint32 c) noexcept {
            if(idxCount+3>idxCap) ReserveIdx(3);
            idx[idxCount++]=a; idx[idxCount++]=b; idx[idxCount++]=c;
            if(cmdCount>0) cmds[cmdCount-1].idxCount+=3;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Clip rect
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::PushClipRect(NkRect r,bool intersect) noexcept {
            if(clipDepth>0&&intersect) r=NkRectIntersect(clipStack[clipDepth-1],r);
            if(clipDepth<32) clipStack[clipDepth++]=r;
            // Nouvelle commande pour ce clip
            if(cmdCount<cmdCap){
                cmds[cmdCount].type=NkUIDrawCmdType::NK_CLIP_RECT;
                cmds[cmdCount].clipRect=r;
                cmds[cmdCount].idxOffset=idxCount;
                cmds[cmdCount].idxCount=0;
                ++cmdCount;
            }
        }
        void NkUIDrawList::PopClipRect() noexcept {
            if(clipDepth>0) --clipDepth;
            // Nouvelle commande avec le clip précédent
            if(cmdCount<cmdCap){
                cmds[cmdCount].type=NkUIDrawCmdType::NK_CLIP_RECT;
                cmds[cmdCount].clipRect=GetClipRect();
                cmds[cmdCount].idxOffset=idxCount;
                cmds[cmdCount].idxCount=0;
                ++cmdCount;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Primitives de base
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddTriangleFilled(NkVec2 a,NkVec2 b,NkVec2 c,NkColor col) noexcept {
            EnsureCmd(); ReserveVtx(3); ReserveIdx(3);
            const NkVec2 uv={-1.f,-1.f}; // sentinel: solid primitive (no texture sample)
            const uint32 va=VtxWrite(a,uv,col),vb=VtxWrite(b,uv,col),vc=VtxWrite(c,uv,col);
            IdxWrite(va,vb,vc);
        }
        void NkUIDrawList::AddTriangle(NkVec2 a,NkVec2 b,NkVec2 c,NkColor col) noexcept {
            AddLine(a,b,col,strokeWidth);
            AddLine(b,c,col,strokeWidth);
            AddLine(c,a,col,strokeWidth);
        }

        void NkUIDrawList::AddLine(NkVec2 a,NkVec2 b,NkColor col,float32 thickness) noexcept {
            const float32 dx=b.x-a.x,dy=b.y-a.y;
            const float32 len=::sqrtf(dx*dx+dy*dy);
            if(len<0.001f) return;
            const float32 nx=(-dy/len)*thickness*0.5f, ny=(dx/len)*thickness*0.5f;
            EnsureCmd(); ReserveVtx(4); ReserveIdx(6);
            const NkVec2 uv={-1.f,-1.f}; // sentinel: solid primitive (no texture sample)
            const uint32 v0=VtxWrite({a.x+nx,a.y+ny},uv,col);
            const uint32 v1=VtxWrite({a.x-nx,a.y-ny},uv,col);
            const uint32 v2=VtxWrite({b.x-nx,b.y-ny},uv,col);
            const uint32 v3=VtxWrite({b.x+nx,b.y+ny},uv,col);
            IdxWrite(v0,v1,v2); IdxWrite(v0,v2,v3);
        }

        void NkUIDrawList::AddPolyline(const NkVec2* pts,int32 n,NkColor col,
                                        float32 thickness,bool closed) noexcept
        {
            if(n<2) return;
            for(int32 i=0;i<n-1;++i) AddLine(pts[i],pts[i+1],col,thickness);
            if(closed) AddLine(pts[n-1],pts[0],col,thickness);
        }

        void NkUIDrawList::AddConvexPolyFilled(const NkVec2* pts,int32 n,NkColor col) noexcept {
            if(n<3) return;
            EnsureCmd();
            const NkVec2 uv={-1.f,-1.f}; // sentinel: solid primitive (no texture sample)
            ReserveVtx(n); ReserveIdx((n-2)*3);
            const uint32 base=vtxCount;
            for(int32 i=0;i<n;++i) VtxWrite(pts[i],uv,col);
            for(int32 i=2;i<n;++i) IdxWrite(base,base+i-1,base+i);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Rectangles
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddRectFilled(NkRect r,NkColor col,float32 rx,float32 ry) noexcept {
            if(r.w<=0||r.h<=0) return;
            if(col.a<=1) return;  // alpha threshold
            // Clip rect cull
            NkRect clipped=NkRectIntersect(r,GetClipRect());
            if(clipped.w<=0||clipped.h<=0) return;
            if(rx<=0&&ry<=0){
                if(IsOccluded(clipped)) return;  // occlusion cull
                EnsureCmd(); ReserveVtx(4); ReserveIdx(6);
                const NkVec2 uv={-1.f,-1.f}; // sentinel: solid primitive (no texture sample)
                const uint32 v0=VtxWrite({r.x,r.y},uv,col);
                const uint32 v1=VtxWrite({r.x+r.w,r.y},uv,col);
                const uint32 v2=VtxWrite({r.x+r.w,r.y+r.h},uv,col);
                const uint32 v3=VtxWrite({r.x,r.y+r.h},uv,col);
                IdxWrite(v0,v1,v2); IdxWrite(v0,v2,v3);
                TrackOpaque(clipped,col);
            } else {
                // Rectangle arrondi (coins transparents — pas d'occlusion tracking)
                const float32 rr=rx>0?rx:ry;
                PathRect(r,rr); PathFill(col);
            }
        }

        void NkUIDrawList::AddRectFilledMultiColor(NkRect r,
                                                    NkColor cTL,NkColor cTR,
                                                    NkColor cBR,NkColor cBL) noexcept
        {
            if(r.w<=0||r.h<=0) return;
            if(cTL.a<=1&&cTR.a<=1&&cBR.a<=1&&cBL.a<=1) return;  // alpha threshold
            // Clip rect cull
            NkRect clipped=NkRectIntersect(r,GetClipRect());
            if(clipped.w<=0||clipped.h<=0) return;
            if(IsOccluded(clipped)) return;  // occlusion cull
            EnsureCmd(); ReserveVtx(4); ReserveIdx(6);
            const NkVec2 uv={-1.f,-1.f}; // sentinel: solid primitive (no texture sample)
            const uint32 v0=VtxWrite({r.x,r.y},uv,cTL);
            const uint32 v1=VtxWrite({r.x+r.w,r.y},uv,cTR);
            const uint32 v2=VtxWrite({r.x+r.w,r.y+r.h},uv,cBR);
            const uint32 v3=VtxWrite({r.x,r.y+r.h},uv,cBL);
            IdxWrite(v0,v1,v2); IdxWrite(v0,v2,v3);
        }

        void NkUIDrawList::AddRectFilledCorners(NkRect r,NkColor col,
                                                float32 radius,uint8 cornerMask) noexcept
        {
            if(r.w<=0||r.h<=0) return;
            if(col.a<=1) return;  // alpha threshold
            // Clip rect cull
            NkRect clipped=NkRectIntersect(r,GetClipRect());
            if(clipped.w<=0||clipped.h<=0) return;
            if(radius<=0||cornerMask==0){AddRectFilled(r,col);return;}
            // Construit un path avec les 4 coins selon le masque
            const float32 rr=radius<r.w*0.5f?radius:r.w*0.5f;
            const float32 rrh=rr<r.h*0.5f?rr:r.h*0.5f;
            PathClear();
            // TL
            if(cornerMask&1) PathArcTo({r.x+rr,r.y+rrh},rr,NKUI_PI,NKUI_PI*1.5f);
            else PathMoveTo({r.x,r.y});
            // TR
            if(cornerMask&2) PathArcTo({r.x+r.w-rr,r.y+rrh},rr,-NKUI_PI*0.5f,0);
            else PathLineTo({r.x+r.w,r.y});
            // BR
            if(cornerMask&4) PathArcTo({r.x+r.w-rr,r.y+r.h-rrh},rr,0,NKUI_PI*0.5f);
            else PathLineTo({r.x+r.w,r.y+r.h});
            // BL
            if(cornerMask&8) PathArcTo({r.x+rr,r.y+r.h-rrh},rr,NKUI_PI*0.5f,NKUI_PI);
            else PathLineTo({r.x,r.y+r.h});
            PathFill(col);
        }

        void NkUIDrawList::AddRect(NkRect r,NkColor col,float32 thickness,
                                    float32 rx,float32 ry) noexcept
        {
            if(r.w<=0||r.h<=0) return;
            if(col.a<=1) return;  // alpha threshold
            { NkRect clipped=NkRectIntersect(r,GetClipRect()); if(clipped.w<=0||clipped.h<=0) return; }
            if(rx<=0&&ry<=0){
                const NkVec2 pts[4]={{r.x,r.y},{r.x+r.w,r.y},{r.x+r.w,r.y+r.h},{r.x,r.y+r.h}};
                AddPolyline(pts,4,col,thickness,true);
            } else {
                PathRect(r,rx>0?rx:ry);
                PathStroke(col,thickness,true);
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Cercles / Arcs
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddCircleFilled(NkVec2 c,float32 r,NkColor col,int32 segs) noexcept {
            if(r<=0) return;
            if(col.a<=1) return;  // alpha threshold
            { NkRect bb={c.x-r,c.y-r,r*2,r*2}; NkRect clipped=NkRectIntersect(bb,GetClipRect()); if(clipped.w<=0||clipped.h<=0) return; }
            if(segs<=0) segs=CalcCircleSegs(r);
            PathClear();
            for(int32 i=0;i<segs;++i){
                const float32 a=2.f*NKUI_PI*i/segs;
                PathLineTo({c.x+::cosf(a)*r,c.y+::sinf(a)*r});
            }
            PathFill(col);
        }

        void NkUIDrawList::AddCircle(NkVec2 c,float32 r,NkColor col,
                                    float32 thickness,int32 segs) noexcept {
            if(r<=0) return;
            if(col.a<=1) return;  // alpha threshold
            { NkRect bb={c.x-r,c.y-r,r*2,r*2}; NkRect clipped=NkRectIntersect(bb,GetClipRect()); if(clipped.w<=0||clipped.h<=0) return; }
            if(segs<=0) segs=CalcCircleSegs(r);
            PathClear();
            for(int32 i=0;i<=segs;++i){
                const float32 a=2.f*NKUI_PI*i/segs;
                PathLineTo({c.x+::cosf(a)*r,c.y+::sinf(a)*r});
            }
            PathStroke(col,thickness,true);
        }

        void NkUIDrawList::AddEllipseFilled(NkVec2 c,float32 rx,float32 ry,
                                            NkColor col,int32 segs) noexcept {
            if(rx<=0||ry<=0) return;
            if(col.a<=1) return;  // alpha threshold
            { NkRect bb={c.x-rx,c.y-ry,rx*2,ry*2}; NkRect clipped=NkRectIntersect(bb,GetClipRect()); if(clipped.w<=0||clipped.h<=0) return; }
            if(segs<=0) segs=CalcCircleSegs((rx+ry)*0.5f);
            PathClear();
            for(int32 i=0;i<segs;++i){
                const float32 a=2.f*NKUI_PI*i/segs;
                PathLineTo({c.x+::cosf(a)*rx,c.y+::sinf(a)*ry});
            }
            PathFill(col);
        }

        void NkUIDrawList::AddArc(NkVec2 c,float32 r,float32 a0,float32 a1,
                                    NkColor col,float32 thickness,int32 segs) noexcept {
            PathArcTo(c,r,a0,a1,segs);
            PathStroke(col,thickness,false);
        }

        void NkUIDrawList::AddArcFilled(NkVec2 c,float32 r,float32 a0,float32 a1,
                                        NkColor col,int32 segs) noexcept {
            pathCount=0;
            PathLineTo(c);
            PathArcTo(c,r,a0,a1,segs);
            PathFill(col);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Bézier
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddBezierCubic(NkVec2 p0,NkVec2 p1,NkVec2 p2,NkVec2 p3,
                                            NkColor col,float32 thickness,int32 segs) noexcept {
            PathClear(); PathMoveTo(p0);
            PathBezierCubicTo(p1,p2,p3,segs);
            PathStroke(col,thickness,false);
        }

        void NkUIDrawList::AddBezierQuadratic(NkVec2 p0,NkVec2 p1,NkVec2 p2,
                                                NkColor col,float32 thickness,int32 segs) noexcept {
            PathClear(); PathMoveTo(p0);
            PathBezierQuadTo(p1,p2,segs);
            PathStroke(col,thickness,false);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Image
        // ─────────────────────────────────────────────────────────────────────────────

        // void NkUIDrawList::AddImage(uint32 texId,NkRect dst,
        //                             NkVec2 uvMin,NkVec2 uvMax,NkColor tint) noexcept {
        //     if(dst.w<=0||dst.h<=0) return;
        //     if(tint.a<=1) return;  // alpha threshold
        //     // Clip rect cull
        //     { NkRect clipped=NkRectIntersect(dst,GetClipRect()); if(clipped.w<=0||clipped.h<=0) return; }
        //     // Nouvelle commande avec texture
        //     if(cmdCount>=cmdCap) return;
        //     cmds[cmdCount].type=NkUIDrawCmdType::NK_TEXTURED_TRIS;
        //     cmds[cmdCount].texId=texId;
        //     cmds[cmdCount].idxOffset=idxCount;
        //     cmds[cmdCount].idxCount=0;
        //     cmds[cmdCount].clipRect=GetClipRect();
        //     ++cmdCount;
        //     ReserveVtx(4); ReserveIdx(6);
        //     const uint32 v0=VtxWrite({dst.x,dst.y},uvMin,tint);
        //     const uint32 v1=VtxWrite({dst.x+dst.w,dst.y},{uvMax.x,uvMin.y},tint);
        //     const uint32 v2=VtxWrite({dst.x+dst.w,dst.y+dst.h},uvMax,tint);
        //     const uint32 v3=VtxWrite({dst.x,dst.y+dst.h},{uvMin.x,uvMax.y},tint);
        //     // IdxWrite(v0,v1,v2); IdxWrite(v0,v2,v3);

        //     IdxWrite(v0, v2, v1);
        //     IdxWrite(v0, v3, v2);
        // }

        void NkUIDrawList::AddImage(uint32 texId,NkRect dst,
                            NkVec2 uvMin,NkVec2 uvMax,NkColor tint) noexcept {
            if(dst.w<=0||dst.h<=0) return;
            if(tint.a<=1) return;  // alpha threshold

            const NkRect clip = GetClipRect();
            const NkRect clipped = NkRectIntersect(dst, clip);
            if(clipped.w<=0||clipped.h<=0) return;
            if(IsOccluded(clipped)) return;

            bool merged = false;
            if (cmdCount > 0) {
                NkUIDrawCmd& prev = cmds[cmdCount - 1];
                if (prev.type == NkUIDrawCmdType::NK_TEXTURED_TRIS
                    && prev.texId == texId
                    && prev.clipRect.x == clip.x
                    && prev.clipRect.y == clip.y
                    && prev.clipRect.w == clip.w
                    && prev.clipRect.h == clip.h)
                {
                    merged = true;
                }
            }

            if (!merged) {
                if(cmdCount>=cmdCap) return;
                cmds[cmdCount].type=NkUIDrawCmdType::NK_TEXTURED_TRIS;
                cmds[cmdCount].texId=texId;
                cmds[cmdCount].idxOffset=idxCount;
                cmds[cmdCount].idxCount=0;
                cmds[cmdCount].clipRect=clip;
                ++cmdCount;
            }

            ReserveVtx(4); ReserveIdx(6);
            const uint32 v0=VtxWrite({dst.x,dst.y},uvMin,tint);
            const uint32 v1=VtxWrite({dst.x+dst.w,dst.y},{uvMax.x,uvMin.y},tint);
            const uint32 v2=VtxWrite({dst.x+dst.w,dst.y+dst.h},uvMax,tint);
            const uint32 v3=VtxWrite({dst.x,dst.y+dst.h},{uvMin.x,uvMax.y},tint);
            IdxWrite(v0, v1, v2);
            IdxWrite(v0, v2, v3);
        }

        void NkUIDrawList::AddImageRounded(uint32 texId,NkRect dst,
                                            float32 radius,NkColor tint) noexcept {
            // Clip avec un masque arrondi — implémentation simplifiée
            (void)radius;
            AddImage(texId,dst,{0,0},{1,1},tint);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Texte (stub — le vrai rendu est dans NkUIFont)
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddText(NkVec2 pos,const char* text,NkColor col,
                                    float32 size,uint32 fId) noexcept {
            // Stub : les vrais quads sont ajoutés par NkUIFont::RenderText()
            // qui appelle directement VtxWrite/IdxWrite
            (void)pos;(void)text;(void)col;(void)size;(void)fId;
        }
        void NkUIDrawList::AddTextWrapped(NkRect bounds,const char* text,NkColor col,
                                            float32 size,uint32 fId) noexcept {
            (void)bounds;(void)text;(void)col;(void)size;(void)fId;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Path builder
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::PathClear() noexcept { pathCount=0; }

        void NkUIDrawList::PathMoveTo(NkVec2 p) noexcept {
            pathCount=0;
            if(pathCount<4096){ pathPts[pathCount++]=p; }
        }

        void NkUIDrawList::PathLineTo(NkVec2 p) noexcept {
            if(pathCount<4096) pathPts[pathCount++]=p;
        }

        void NkUIDrawList::PathArcTo(NkVec2 c,float32 r,float32 a0,float32 a1,int32 segs) noexcept {
            if(segs<=0) segs=CalcCircleSegs(r);
            const int32 steps=segs+1;
            for(int32 i=0;i<steps&&pathCount<4096;++i){
                const float32 a=a0+(a1-a0)*i/(steps-1);
                pathPts[pathCount++]={c.x+::cosf(a)*r,c.y+::sinf(a)*r};
            }
        }

        void NkUIDrawList::PathBezierCubicTo(NkVec2 p1,NkVec2 p2,NkVec2 p3,int32 segs) noexcept {
            if(pathCount==0) return;
            if(segs<=0) segs=32;
            const NkVec2 p0=pathPts[pathCount-1];
            for(int32 i=1;i<=segs&&pathCount<4096;++i){
                const float32 t=static_cast<float32>(i)/segs;
                const float32 u=1-t;
                pathPts[pathCount++]={
                    u*u*u*p0.x+3*u*u*t*p1.x+3*u*t*t*p2.x+t*t*t*p3.x,
                    u*u*u*p0.y+3*u*u*t*p1.y+3*u*t*t*p2.y+t*t*t*p3.y
                };
            }
        }

        void NkUIDrawList::PathBezierQuadTo(NkVec2 p1,NkVec2 p2,int32 segs) noexcept {
            if(pathCount==0) return;
            if(segs<=0) segs=24;
            const NkVec2 p0=pathPts[pathCount-1];
            for(int32 i=1;i<=segs&&pathCount<4096;++i){
                const float32 t=static_cast<float32>(i)/segs;
                const float32 u=1-t;
                pathPts[pathCount++]={u*u*p0.x+2*u*t*p1.x+t*t*p2.x,
                                    u*u*p0.y+2*u*t*p1.y+t*t*p2.y};
            }
        }

        void NkUIDrawList::PathRect(NkRect r,float32 rx) noexcept {
            if(rx<=0){
                PathMoveTo({r.x,r.y});
                PathLineTo({r.x+r.w,r.y});
                PathLineTo({r.x+r.w,r.y+r.h});
                PathLineTo({r.x,r.y+r.h});
                return;
            }
            const float32 rr=rx<r.w*0.5f?rx:r.w*0.5f;
            const float32 rrh=rr<r.h*0.5f?rr:r.h*0.5f;
            PathMoveTo({r.x+rr,r.y});
            PathArcTo({r.x+r.w-rr,r.y+rrh},rr,-NKUI_PI*0.5f,0.f);
            PathArcTo({r.x+r.w-rr,r.y+r.h-rrh},rr,0.f,NKUI_PI*0.5f);
            PathArcTo({r.x+rr,r.y+r.h-rrh},rr,NKUI_PI*0.5f,NKUI_PI);
            PathArcTo({r.x+rr,r.y+rrh},rr,NKUI_PI,NKUI_PI*1.5f);
        }

        void NkUIDrawList::PathFill(NkColor col) noexcept {
            AddConvexPolyFilled(pathPts,pathCount,col);
            pathCount=0;
        }

        void NkUIDrawList::PathStroke(NkColor col,float32 thickness,bool closed) noexcept {
            AddPolyline(pathPts,pathCount,col,thickness,closed);
            pathCount=0;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Widget helpers composites
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDrawList::AddCheckMark(NkVec2 pos,float32 sz,NkColor col) noexcept {
            // Coche : 3 segments formant un "v" penché
            const float32 h=sz*0.7f;
            const NkVec2 pts[3]={
                {pos.x,           pos.y+h*0.5f},
                {pos.x+sz*0.35f,  pos.y+h},
                {pos.x+sz,        pos.y}
            };
            AddPolyline(pts,3,col,sz*0.15f,false);
        }

        void NkUIDrawList::AddArrow(NkVec2 center,float32 sz,int32 dir,NkColor col) noexcept {
            // dir : 0=droite, 1=bas, 2=gauche, 3=haut
            NkVec2 pts[3];
            const float32 h=sz*0.5f;
            switch(dir){
                case 0:pts[0]={center.x-h,center.y-h};pts[1]={center.x+h,center.y};pts[2]={center.x-h,center.y+h};break;
                case 1:pts[0]={center.x-h,center.y-h};pts[1]={center.x,center.y+h};pts[2]={center.x+h,center.y-h};break;
                case 2:pts[0]={center.x+h,center.y-h};pts[1]={center.x-h,center.y};pts[2]={center.x+h,center.y+h};break;
                default:pts[0]={center.x-h,center.y+h};pts[1]={center.x,center.y-h};pts[2]={center.x+h,center.y+h};break;
            }
            AddConvexPolyFilled(pts,3,col);
        }

        void NkUIDrawList::AddShadow(NkRect r,float32 radius,NkColor col,NkVec2 offset) noexcept {
            // Approximation d'ombre : rectangle décalé avec alpha réduit
            const int32 steps=4;
            for(int32 i=0;i<steps;++i){
                const float32 t=static_cast<float32>(i)/steps;
                const NkColor sc=col.WithAlpha(static_cast<uint8>(col.a*(1-t)*0.3f));
                const float32 expand=radius*(1-t)*0.5f;
                AddRectFilled({r.x+offset.x-expand,r.y+offset.y-expand,
                            r.w+expand*2,r.h+expand*2},sc,radius);
            }
        }

        void NkUIDrawList::AddScrollbarThumb(NkRect r,NkColor col,bool vertical) noexcept {
            (void)vertical;
            AddRectFilled(r,col,r.w<r.h?r.w*0.5f:r.h*0.5f);
        }

        void NkUIDrawList::AddResizeGrip(NkVec2 pos,float32 sz,NkColor col) noexcept {
            // 3 lignes diagonales dans le coin
            for(int32 i=1;i<=3;++i){
                const float32 s=sz*i/3.f;
                AddLine({pos.x+s,pos.y},{pos.x,pos.y+s},col,1.f);
            }
        }

        void NkUIDrawList::AddSpinner(NkVec2 center,float32 r,float32 angle,NkColor col) noexcept {
            // Arc tournant pour indiquer le chargement
            const float32 span=NKUI_PI*1.4f;
            AddArc(center,r,angle,angle+span,col,2.f);
        }

        void NkUIDrawList::AddColorWheel(NkRect r,float32 hue,float32 sat,float32 val) noexcept {
            // Roue de couleur simplifiée : cercle de secteurs HSV
            const NkVec2 c={r.x+r.w*0.5f,r.y+r.h*0.5f};
            const float32 radius=(r.w<r.h?r.w:r.h)*0.5f;
            const int32 segs=36;
            for(int32 i=0;i<segs;++i){
                const float32 h0=static_cast<float32>(i)/segs;
                const float32 h1=static_cast<float32>(i+1)/segs;
                const float32 a0=h0*NKUI_PI*2;
                const float32 a1=h1*NKUI_PI*2;
                // Conversion HSV → RGB
                auto hsv=[](float32 H,float32 S,float32 V)->NkColor{
                    if(S<=0) return {uint8(V*255),uint8(V*255),uint8(V*255),255};
                    H*=6; int32 i2=static_cast<int32>(H); float32 f=H-i2;
                    float32 p=V*(1-S),q=V*(1-S*f),t2=V*(1-S*(1-f));
                    float32 r2,g2,b2;
                    switch(i2%6){case 0:r2=V;g2=t2;b2=p;break;case 1:r2=q;g2=V;b2=p;break;
                    case 2:r2=p;g2=V;b2=t2;break;case 3:r2=p;g2=q;b2=V;break;
                    case 4:r2=t2;g2=p;b2=V;break;default:r2=V;g2=p;b2=q;break;}
                    return{uint8(r2*255),uint8(g2*255),uint8(b2*255),255};};
                const NkColor col2=hsv(h0,sat,val);
                AddArcFilled(c,radius,a0,a1,col2);
            }
            // Point de sélection
            const float32 ax=c.x+::cosf(hue*NKUI_PI*2)*radius*0.8f;
            const float32 ay=c.y+::sinf(hue*NKUI_PI*2)*radius*0.8f;
            AddCircle({ax,ay},6,NkColor::White(),2.f);
        }
    }
} // namespace nkentseu
