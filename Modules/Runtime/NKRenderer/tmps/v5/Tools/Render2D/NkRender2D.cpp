// =============================================================================
// NkRender2D.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRender2D.h"
#include <cmath>
#ifndef NK_PI
#define NK_PI 3.14159265358979f
#endif

namespace nkentseu {
namespace renderer {

    NkRender2D::~NkRender2D() { Shutdown(); }

    bool NkRender2D::Init(NkIDevice* device, NkTextureLibrary* texLib, uint32 maxVerts) {
        mDevice = device; mTexLib = texLib;
        mVerts.Reserve(maxVerts);
        // VBO dynamique
        NkBufferDesc vbd; vbd.size=maxVerts*sizeof(Vert2D);
        vbd.usage=NkBufferUsage::NK_DYNAMIC; vbd.type=NkBufferType::NK_VERTEX;
        mVBO = mDevice->CreateBuffer(vbd);
        // IBO (quads → triangles)
        uint32 maxIdx = maxVerts / 4 * 6;
        NkVector<uint32> idata(maxIdx);
        for(uint32 i=0,v=0;i<maxIdx;i+=6,v+=4){
            idata[i+0]=v;idata[i+1]=v+1;idata[i+2]=v+2;
            idata[i+3]=v;idata[i+4]=v+2;idata[i+5]=v+3;
        }
        NkBufferDesc ibd; ibd.size=maxIdx*sizeof(uint32);
        ibd.usage=NkBufferUsage::NK_STATIC; ibd.type=NkBufferType::NK_INDEX;
        ibd.data=idata.Data();
        mIBO = mDevice->CreateBuffer(ibd);

        // Pipelines 2D
        NkPipelineDesc pd2D;
        pd2D.name="2D_Alpha"; pd2D.type=NkPipelineType::NK_2D;
        pd2D.blendMode=(uint8)NkBlendMode::NK_ALPHA;
        mPipeAlpha = mDevice->CreatePipeline(pd2D);
        pd2D.name="2D_Add"; pd2D.blendMode=(uint8)NkBlendMode::NK_ADDITIVE;
        mPipeAdd   = mDevice->CreatePipeline(pd2D);
        pd2D.name="2D_Opaque"; pd2D.blendMode=(uint8)NkBlendMode::NK_OPAQUE;
        mPipeOpaque= mDevice->CreatePipeline(pd2D);
        return mVBO.IsValid();
    }

    void NkRender2D::Shutdown() {
        if(mVBO.IsValid()){mDevice->DestroyBuffer(mVBO);mVBO={};}
        if(mIBO.IsValid()){mDevice->DestroyBuffer(mIBO);mIBO={};}
    }

    // ── Frame ──────────────────────────────────────────────────────────────────
    void NkRender2D::Begin(NkICommandBuffer* cmd, uint32 w, uint32 h,
                             float32 cX, float32 cY, float32 zoom, float32 rotDeg) {
        mCmd=cmd; mW=w; mH=h; mInFrame=true;
        mVerts.Clear(); mBatches.Clear();
        // Build ortho
        float32 l=cX-w*0.5f/zoom, r=cX+w*0.5f/zoom;
        float32 t=cY-h*0.5f/zoom, b=cY+h*0.5f/zoom;
        mOrtho=NkMat4f::Zero();
        mOrtho[0][0]=2.f/(r-l); mOrtho[1][1]=2.f/(t-b);
        mOrtho[2][2]=-1.f;
        mOrtho[3][0]=-(r+l)/(r-l); mOrtho[3][1]=-(t+b)/(t-b); mOrtho[3][3]=1.f;
        (void)rotDeg; // TODO: rotation ortho
    }

    void NkRender2D::End() {
        Flush();
        mInFrame=false; mCmd=nullptr;
    }

    void NkRender2D::FlushPending(NkICommandBuffer* cmd) {
        if (!mVerts.Empty()) { mCmd=cmd; Flush(); mCmd=nullptr; }
    }

    void NkRender2D::Flush() {
        if (mVerts.Empty() || !mCmd) return;
        mDevice->UpdateBuffer(mVBO, mVerts.Data(), (uint32)mVerts.Size()*sizeof(Vert2D));
        mCmd->BindVertexBuffer(mVBO, sizeof(Vert2D));
        mCmd->BindIndexBuffer(mIBO, NkIndexType::NK_UINT32);
        mCmd->UpdateUniformBuffer(0, &mOrtho, sizeof(NkMat4f));

        for (auto& b : mBatches) {
            NkPipelineHandle pipe = mPipeAlpha;
            if (b.blend==NkBlendMode::NK_ADDITIVE) pipe=mPipeAdd;
            else if (b.blend==NkBlendMode::NK_OPAQUE) pipe=mPipeOpaque;
            mCmd->BindPipeline(pipe);
            auto rhi = mTexLib->GetRHIHandle(b.tex.IsValid() ? b.tex : mTexLib->GetWhite1x1());
            auto samp= mTexLib->GetRHISampler(b.tex.IsValid() ? b.tex : mTexLib->GetWhite1x1());
            mCmd->BindTexture(0, rhi); mCmd->BindSampler(0, samp);
            uint32 triIdx = b.vStart/4*6;
            uint32 triCount = b.vCount/4*6;
            mCmd->DrawIndexed(triCount, 1, triIdx, 0, 0);
        }
        mBatchCount += (uint32)mBatches.Size();
        mVertCount  += (uint32)mVerts.Size();
        mVerts.Clear(); mBatches.Clear();
    }

    void NkRender2D::PushQuad(NkVec2f tl,NkVec2f tr,NkVec2f br,NkVec2f bl,
                                NkVec2f uvTL,NkVec2f uvTR,NkVec2f uvBR,NkVec2f uvBL,
                                NkVec4f color, NkTexHandle tex) {
        uint32 c = PackColor(color);
        uint32 vStart = (uint32)mVerts.Size();
        mVerts.PushBack({tl,uvTL,c,0}); mVerts.PushBack({tr,uvTR,c,0});
        mVerts.PushBack({br,uvBR,c,0}); mVerts.PushBack({bl,uvBL,c,0});

        // Fusionner batch si même texture/blend/layer
        if (!mBatches.Empty()) {
            auto& last = mBatches[mBatches.Size()-1];
            if (last.tex==tex && last.blend==mBlend && last.layer==mLayer) {
                last.vCount+=4; return;
            }
        }
        Batch b; b.tex=tex; b.blend=mBlend; b.layer=mLayer;
        b.vStart=vStart; b.vCount=4;
        mBatches.PushBack(b);
    }

    // ── Sprites ────────────────────────────────────────────────────────────────
    void NkRender2D::DrawSprite(NkRectF dst, NkTexHandle tex,
                                  NkVec4f tint, NkRectF uv) {
        float32 x0=dst.x,y0=dst.y,x1=dst.x+dst.w,y1=dst.y+dst.h;
        float32 u0=uv.x,v0=uv.y,u1=uv.x+uv.w,v1=uv.y+uv.h;
        PushQuad({x0,y0},{x1,y0},{x1,y1},{x0,y1},
                  {u0,v0},{u1,v0},{u1,v1},{u0,v1}, tint, tex);
    }

    void NkRender2D::DrawSpriteRotated(NkRectF dst, NkTexHandle tex,
                                         float32 angleDeg, NkVec2f pivot,
                                         NkVec4f tint, NkRectF uv) {
        float32 cx=dst.x+dst.w*pivot.x, cy=dst.y+dst.h*pivot.y;
        float32 rad=angleDeg*NK_PI/180.f;
        float32 co=cosf(rad), si=sinf(rad);
        auto rot=[&](float32 x,float32 y)->NkVec2f{
            float32 dx=x-cx,dy=y-cy;
            return {cx+dx*co-dy*si, cy+dx*si+dy*co};
        };
        float32 x0=dst.x,y0=dst.y,x1=dst.x+dst.w,y1=dst.y+dst.h;
        float32 u0=uv.x,v0=uv.y,u1=uv.x+uv.w,v1=uv.y+uv.h;
        PushQuad(rot(x0,y0),rot(x1,y0),rot(x1,y1),rot(x0,y1),
                  {u0,v0},{u1,v0},{u1,v1},{u0,v1}, tint, tex);
    }

    void NkRender2D::DrawNineSlice(NkRectF dst, NkTexHandle tex,
                                     float32 l, float32 t, float32 r, float32 b,
                                     NkVec4f tint) {
        // 9 quads : corners + edges + center
        float32 xs[4]={dst.x, dst.x+l, dst.x+dst.w-r, dst.x+dst.w};
        float32 ys[4]={dst.y, dst.y+t, dst.y+dst.h-b, dst.y+dst.h};
        float32 us[4]={0.f, l/dst.w, 1.f-r/dst.w, 1.f};
        float32 vs[4]={0.f, t/dst.h, 1.f-b/dst.h, 1.f};
        for(int i=0;i<3;i++) for(int j=0;j<3;j++){
            NkRectF d={xs[i],ys[j],xs[i+1]-xs[i],ys[j+1]-ys[j]};
            NkRectF u={us[i],vs[j],us[i+1]-us[i],vs[j+1]-vs[j]};
            DrawSprite(d,tex,tint,u);
        }
    }

    // ── Formes ────────────────────────────────────────────────────────────────
    void NkRender2D::DrawImage(NkTexHandle tex, NkRectF dst, NkVec4f tint) {
        DrawSprite(dst,tex,tint);
    }

    void NkRender2D::FillRect(NkRectF r, NkVec4f color) {
        DrawSprite(r, mTexLib->GetWhite1x1(), color);
    }

    void NkRender2D::FillRectGradH(NkRectF r, NkVec4f left, NkVec4f right) {
        float32 x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h;
        uint32 tl=PackColor(left), tr=PackColor(right);
        uint32 vStart=(uint32)mVerts.Size();
        mVerts.PushBack({{x0,y0},{0,0},tl,0}); mVerts.PushBack({{x1,y0},{1,0},tr,0});
        mVerts.PushBack({{x1,y1},{1,1},tr,0}); mVerts.PushBack({{x0,y1},{0,1},tl,0});
        Batch b;b.tex=mTexLib->GetWhite1x1();b.blend=mBlend;b.layer=mLayer;
        b.vStart=vStart;b.vCount=4; mBatches.PushBack(b);
    }

    void NkRender2D::FillRectGradV(NkRectF r, NkVec4f top, NkVec4f bot) {
        float32 x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h;
        uint32 tc=PackColor(top), bc=PackColor(bot);
        uint32 vStart=(uint32)mVerts.Size();
        mVerts.PushBack({{x0,y0},{0,0},tc,0}); mVerts.PushBack({{x1,y0},{1,0},tc,0});
        mVerts.PushBack({{x1,y1},{1,1},bc,0}); mVerts.PushBack({{x0,y1},{0,1},bc,0});
        Batch b;b.tex=mTexLib->GetWhite1x1();b.blend=mBlend;b.layer=mLayer;
        b.vStart=vStart;b.vCount=4; mBatches.PushBack(b);
    }

    void NkRender2D::FillCircle(NkVec2f c, float32 radius, NkVec4f color, uint32 segs) {
        for(uint32 i=0;i<segs;i++){
            float32 a0=2*NK_PI*i/segs, a1=2*NK_PI*(i+1)/segs;
            NkVec2f p0={c.x+cosf(a0)*radius,c.y+sinf(a0)*radius};
            NkVec2f p1={c.x+cosf(a1)*radius,c.y+sinf(a1)*radius};
            FillTriangle(c,p0,p1,color);
        }
    }

    void NkRender2D::FillTriangle(NkVec2f a, NkVec2f b, NkVec2f c, NkVec4f color) {
        uint32 col=PackColor(color);
        NkTexHandle white=mTexLib->GetWhite1x1();
        uint32 vStart=(uint32)mVerts.Size();
        // Use degenerate quad trick (repeat last vert)
        mVerts.PushBack({a,{0,0},col,0}); mVerts.PushBack({b,{1,0},col,0});
        mVerts.PushBack({c,{1,1},col,0}); mVerts.PushBack({c,{1,1},col,0});
        Batch bt;bt.tex=white;bt.blend=mBlend;bt.layer=mLayer;
        bt.vStart=vStart;bt.vCount=4; mBatches.PushBack(bt);
    }

    void NkRender2D::FillRoundRect(NkRectF r, NkVec4f color, float32 radius) {
        // Centre + 4 rectangles + 4 coins
        FillRect({r.x+radius,r.y,r.w-2*radius,r.h},color);
        FillRect({r.x,r.y+radius,radius,r.h-2*radius},color);
        FillRect({r.x+r.w-radius,r.y+radius,radius,r.h-2*radius},color);
        uint32 segs=8;
        NkVec2f corners[4]={
            {r.x+radius,r.y+radius},{r.x+r.w-radius,r.y+radius},
            {r.x+r.w-radius,r.y+r.h-radius},{r.x+radius,r.y+r.h-radius}
        };
        float32 startAngs[4]={NK_PI,1.5f*NK_PI,0,0.5f*NK_PI};
        for(int ci=0;ci<4;ci++){
            for(uint32 s=0;s<segs;s++){
                float32 a0=startAngs[ci]+0.5f*NK_PI*s/segs;
                float32 a1=startAngs[ci]+0.5f*NK_PI*(s+1)/segs;
                NkVec2f p0={corners[ci].x+cosf(a0)*radius,corners[ci].y+sinf(a0)*radius};
                NkVec2f p1={corners[ci].x+cosf(a1)*radius,corners[ci].y+sinf(a1)*radius};
                FillTriangle(corners[ci],p0,p1,color);
            }
        }
    }

    void NkRender2D::DrawLine(NkVec2f a, NkVec2f b, NkVec4f color, float32 thick) {
        float32 dx=b.x-a.x, dy=b.y-a.y;
        float32 len=sqrtf(dx*dx+dy*dy);
        if(len<1e-4f)return;
        float32 nx=-dy/len*thick*0.5f, ny=dx/len*thick*0.5f;
        PushQuad({a.x-nx,a.y-ny},{b.x-nx,b.y-ny},{b.x+nx,b.y+ny},{a.x+nx,a.y+ny},
                  {0,0},{1,0},{1,1},{0,1}, color, mTexLib->GetWhite1x1());
    }

    void NkRender2D::DrawRect(NkRectF r, NkVec4f color, float32 thick) {
        DrawLine({r.x,r.y},{r.x+r.w,r.y},color,thick);
        DrawLine({r.x+r.w,r.y},{r.x+r.w,r.y+r.h},color,thick);
        DrawLine({r.x+r.w,r.y+r.h},{r.x,r.y+r.h},color,thick);
        DrawLine({r.x,r.y+r.h},{r.x,r.y},color,thick);
    }

    void NkRender2D::DrawCircle(NkVec2f c, float32 radius, NkVec4f color,
                                  float32 thick, uint32 segs) {
        for(uint32 i=0;i<segs;i++){
            float32 a0=2*NK_PI*i/segs, a1=2*NK_PI*(i+1)/segs;
            NkVec2f p0={c.x+cosf(a0)*radius,c.y+sinf(a0)*radius};
            NkVec2f p1={c.x+cosf(a1)*radius,c.y+sinf(a1)*radius};
            DrawLine(p0,p1,color,thick);
        }
    }

    void NkRender2D::DrawArc(NkVec2f c, float32 r, float32 a0, float32 a1,
                               NkVec4f color, float32 thick) {
        uint32 segs=32;
        float32 step=(a1-a0)/segs;
        for(uint32 i=0;i<segs;i++){
            float32 ang0=a0+step*i, ang1=a0+step*(i+1);
            NkVec2f p0={c.x+cosf(ang0*NK_PI/180.f)*r,c.y+sinf(ang0*NK_PI/180.f)*r};
            NkVec2f p1={c.x+cosf(ang1*NK_PI/180.f)*r,c.y+sinf(ang1*NK_PI/180.f)*r};
            DrawLine(p0,p1,color,thick);
        }
    }

    void NkRender2D::DrawPolyline(const NkVec2f* pts, uint32 n, NkVec4f color,
                                    float32 thick, bool closed) {
        for(uint32 i=0;i<n-1;i++) DrawLine(pts[i],pts[i+1],color,thick);
        if(closed && n>1) DrawLine(pts[n-1],pts[0],color,thick);
    }

    void NkRender2D::DrawBezier(NkVec2f p0,NkVec2f p1,NkVec2f p2,NkVec2f p3,
                                  NkVec4f color, float32 thick, uint32 segs) {
        NkVec2f prev=p0;
        for(uint32 i=1;i<=segs;i++){
            float32 t=(float32)i/segs, t2=t*t, t3=t2*t;
            float32 mt=1-t,mt2=mt*mt,mt3=mt2*mt;
            NkVec2f cur={
                mt3*p0.x+3*mt2*t*p1.x+3*mt*t2*p2.x+t3*p3.x,
                mt3*p0.y+3*mt2*t*p1.y+3*mt*t2*p2.y+t3*p3.y
            };
            DrawLine(prev,cur,color,thick);
            prev=cur;
        }
    }

    void NkRender2D::DrawRoundRect(NkRectF r, NkVec4f color, float32 radius, float32 thick) {
        DrawRect({r.x+radius,r.y,r.w-2*radius,r.h},color,thick);
        DrawArc({r.x+radius,r.y+radius},radius,180,270,color,thick);
        DrawArc({r.x+r.w-radius,r.y+radius},radius,270,360,color,thick);
        DrawArc({r.x+r.w-radius,r.y+r.h-radius},radius,0,90,color,thick);
        DrawArc({r.x+radius,r.y+r.h-radius},radius,90,180,color,thick);
    }

    // ── Clip / Blend ──────────────────────────────────────────────────────────
    void NkRender2D::PushClip(NkRectF rect) {
        Flush(); // flush avant scissor
        if(mCmd) mCmd->SetScissor((int32)rect.x,(int32)rect.y,(uint32)rect.w,(uint32)rect.h);
        mClipStack.PushBack(rect);
    }

    void NkRender2D::PopClip() {
        Flush();
        if (!mClipStack.Empty()) mClipStack.PopBack();
        if(mCmd){
            if(!mClipStack.Empty()){
                auto& r=mClipStack[mClipStack.Size()-1];
                mCmd->SetScissor((int32)r.x,(int32)r.y,(uint32)r.w,(uint32)r.h);
            } else {
                mCmd->SetScissor(0,0,mW,mH);
            }
        }
    }

    void NkRender2D::SetBlendMode(NkBlendMode mode) { mBlend=mode; }
    void NkRender2D::SetLayer(uint8 layer) { mLayer=layer; }

} // namespace renderer
} // namespace nkentseu
