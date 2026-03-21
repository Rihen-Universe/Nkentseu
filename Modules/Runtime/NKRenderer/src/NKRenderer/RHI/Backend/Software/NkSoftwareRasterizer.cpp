// =============================================================================
// NkSoftwareRasterizer.cpp — v4 final
// Corrections :
//   - NkSWGeomContext callbacks par valeur (pas de dangling ptr)
//   - Suppression des logs debug dans les boucles internes
//   - SyncColorTargetAlias() appelé systématiquement
//   - RasterTriangle : guard correct (colorTargetCount == 0 ET colorTarget == nullptr)
// =============================================================================
#include "NkSoftwareRasterizer.h"
#include "NkSoftwareTypes.h"
#include "NKMath/NkFunctions.h"
#include <cstring>
#include <cmath>

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

namespace nkentseu {

// =============================================================================
// NkSWTexture — implémentation
// =============================================================================
uint32 NkSWTexture::Bpp() const { return NkSWBpp(desc.format); }

NkVec4f NkSWTexture::Read(uint32 x, uint32 y, uint32 mip) const {
    if (mips.IsEmpty() || mip >= (uint32)mips.Size()) return {};
    uint32 w=Width(mip), h=Height(mip), bpp=Bpp();
    x=NkMin(x,w-1); y=NkMin(y,h-1);
    return NkSWReadPixel(mips[mip].Data()+(y*w+x)*bpp, desc.format);
}

void NkSWTexture::Write(uint32 x, uint32 y, const NkVec4f& c, uint32 mip) {
    if (mips.IsEmpty() || mip >= (uint32)mips.Size()) return;
    uint32 w=Width(mip), h=Height(mip), bpp=Bpp();
    if (x>=w || y>=h) return;
    NkSWWritePixel(mips[mip].Data()+(y*w+x)*bpp, desc.format, c);
}

void NkSWTexture::Clear(const NkVec4f& c, uint32 mip) {
    if (mips.IsEmpty() || mip >= (uint32)mips.Size()) return;
    for (uint32 y=0;y<Height(mip);y++)
        for (uint32 x=0;x<Width(mip);x++) Write(x,y,c,mip);
}

void NkSWTexture::ClearDepth(float d) {
    if (mips.IsEmpty()) return;
    uint32 count=Width(0)*Height(0);
    uint8* p=mips[0].Data();
    for (uint32 i=0;i<count;i++) memcpy(p+i*4,&d,4);
}

NkVec4f NkSWTexture::Sample(float u, float v, uint32 mip) const {
    if (mips.IsEmpty()) return {0,0,0,1};
    mip=NkMin(mip,(uint32)mips.Size()-1);
    uint32 w=Width(mip),h=Height(mip);
    u=u-NkFloor(u); v=v-NkFloor(v);
    float px=u*(float)w-0.5f, py=v*(float)h-0.5f;
    int x0=(int)NkFloor(px),y0=(int)NkFloor(py);
    float fx=px-(float)x0, fy=py-(float)y0;
    auto wrap=[](int val,int sz){return ((val%sz)+sz)%sz;};
    NkVec4f c00=Read((uint32)wrap(x0,w),(uint32)wrap(y0,h),mip);
    NkVec4f c10=Read((uint32)wrap(x0+1,w),(uint32)wrap(y0,h),mip);
    NkVec4f c01=Read((uint32)wrap(x0,w),(uint32)wrap(y0+1,h),mip);
    NkVec4f c11=Read((uint32)wrap(x0+1,w),(uint32)wrap(y0+1,h),mip);
    auto l4=[](const NkVec4f&a,const NkVec4f&b,float t){
        return NkVec4f{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};};
    return l4(l4(c00,c10,fx),l4(c01,c11,fx),fy);
}

NkVec4f NkSWTexture::SampleShadow(float u, float v, float compareZ) const {
    if (mips.IsEmpty()) return {1,1,1,1};
    uint32 w=Width(0),h=Height(0); if (!w||!h) return {1,1,1,1};
    u=NkClamp(u,0.f,1.f); v=NkClamp(v,0.f,1.f);
    float px=u*(float)w-0.5f,py=v*(float)h-0.5f;
    int x0=(int)NkFloor(px),y0=(int)NkFloor(py);
    float fx=px-(float)x0, fy=py-(float)y0;
    auto cc=[](int val,int sz)->uint32{return (uint32)NkClamp(val,0,sz-1);};
    auto cmp=[&](int cx,int cy)->float{NkVec4f d=Read(cc(cx,w),cc(cy,h)); return (compareZ<=d.x)?1.f:0.f;};
    float s=cmp(x0,y0)*(1-fx)*(1-fy)+cmp(x0+1,y0)*fx*(1-fy)+cmp(x0,y0+1)*(1-fx)*fy+cmp(x0+1,y0+1)*fx*fy;
    return {s,s,s,1.f};
}

// =============================================================================
// NkSWRasterizer
// =============================================================================
static constexpr float kClipEps = 1e-6f;

void NkSWRasterizer::SetState(const NkSWDrawState& state) {
    mState = state;
    mState.SyncColorTargetAlias(); // garantir colorTarget == colorTargets[0]
}

// ── Clipping Sutherland-Hodgman ───────────────────────────────────────────────
uint32 NkSWRasterizer::ComputeClipFlags(const NkSWVertex& v) const {
    uint32 f=0; float w=v.position.w;
    if (w<kClipEps)      f|=NK_SW_CLIP_POS_W;
    if (v.position.x<-w) f|=NK_SW_CLIP_NEG_X;
    if (v.position.x> w) f|=NK_SW_CLIP_POS_X;
    if (v.position.y<-w) f|=NK_SW_CLIP_NEG_Y;
    if (v.position.y> w) f|=NK_SW_CLIP_POS_Y;
    if (v.position.z<-w) f|=NK_SW_CLIP_NEG_Z;
    if (v.position.z> w) f|=NK_SW_CLIP_POS_Z;
    return f;
}

NkSWVertex NkSWRasterizer::LerpVertex(const NkSWVertex& a, const NkSWVertex& b, float t) {
    NkSWVertex r;
    auto l=[t](float x,float y){return x+(y-x)*t;};
    r.position={l(a.position.x,b.position.x),l(a.position.y,b.position.y),
                l(a.position.z,b.position.z),l(a.position.w,b.position.w)};
    r.normal={l(a.normal.x,b.normal.x),l(a.normal.y,b.normal.y),l(a.normal.z,b.normal.z)};
    r.uv={l(a.uv.x,b.uv.x),l(a.uv.y,b.uv.y)};
    r.color={l(a.color.x,b.color.x),l(a.color.y,b.color.y),l(a.color.z,b.color.z),l(a.color.w,b.color.w)};
    r.attrCount=NkMax(a.attrCount,b.attrCount);
    for (uint32 i=0;i<r.attrCount&&i<16;i++) r.attrs[i]=l(a.attrs[i],b.attrs[i]);
    return r;
}

static float fPW(const NkSWVertex& v){return v.position.w-kClipEps;}
static float fPX(const NkSWVertex& v){return v.position.w-v.position.x;}
static float fNX(const NkSWVertex& v){return v.position.w+v.position.x;}
static float fPY(const NkSWVertex& v){return v.position.w-v.position.y;}
static float fNY(const NkSWVertex& v){return v.position.w+v.position.y;}
static float fPZ(const NkSWVertex& v){return v.position.w-v.position.z;}
static float fNZ(const NkSWVertex& v){return v.position.w+v.position.z;}
static float (*kPlanes[])(const NkSWVertex&) = {fPW,fPX,fNX,fPY,fNY,fPZ,fNZ};
static constexpr uint32 kNumPlanes = 7;

static uint32 ClipPoly(const NkSWVertex* in, uint32 n, NkSWVertex* out, uint32 maxOut,
                        float(*pf)(const NkSWVertex&)) {
    uint32 cnt=0;
    for (uint32 i=0;i<n;i++) {
        const NkSWVertex& cur=in[i], &nxt=in[(i+1)%n];
        float fc=pf(cur),fn=pf(nxt);
        bool ci=(fc>=0.f),ni=(fn>=0.f);
        if (ci) {
            if (cnt<maxOut) out[cnt++]=cur;
            if (!ni && cnt<maxOut) {
                float denom=fc-fn;
                float t=(NkFabs(denom)<kClipEps)?0.f:fc/denom;
                out[cnt++]=NkSWRasterizer::LerpVertex(cur,nxt,t);
            }
        } else if (ni) {
            float denom=fc-fn;
            float t=(NkFabs(denom)<kClipEps)?0.f:fc/denom;
            if (cnt<maxOut) out[cnt++]=NkSWRasterizer::LerpVertex(cur,nxt,t);
        }
    }
    return cnt;
}

uint32 NkSWRasterizer::ClipTriangle(const NkSWVertex v[3], NkSWVertex out[9]) const {
    uint32 f=ComputeClipFlags(v[0])|ComputeClipFlags(v[1])|ComputeClipFlags(v[2]);
    if (f==0){out[0]=v[0];out[1]=v[1];out[2]=v[2];return 3;}
    if (ComputeClipFlags(v[0])&ComputeClipFlags(v[1])&ComputeClipFlags(v[2])) return 0;
    static NkSWVertex buf1[16],buf2[16];
    buf1[0]=v[0];buf1[1]=v[1];buf1[2]=v[2];
    uint32 n=3;
    for (uint32 p=0;p<kNumPlanes&&n>0;p++){
        n=ClipPoly(buf1,n,buf2,16,kPlanes[p]);
        memcpy(buf1,buf2,n*sizeof(NkSWVertex));
    }
    if (n>9) n=9;
    for (uint32 i=0;i<n;i++) out[i]=buf1[i];
    return n;
}

NkSWVertex NkSWRasterizer::PerspectiveDivide(const NkSWVertex& v) const {
    NkSWVertex r=v;
    float iw=(v.position.w!=0.f)?1.f/v.position.w:0.f;
    r.position.x*=iw; r.position.y*=iw; r.position.z*=iw;
    r.position.w=iw;
    return r;
}

NkSWVertex NkSWRasterizer::NDCToScreen(const NkSWVertex& v) const {
    NkSWVertex r=v;
    float vpX=mState.hasViewport?mState.vpX:0.f;
    float vpY=mState.hasViewport?mState.vpY:0.f;
    float vpW=mState.hasViewport?mState.vpW:(float)mState.targetW;
    float vpH=mState.hasViewport?mState.vpH:(float)mState.targetH;
    float minZ=mState.hasViewport?mState.vpMinZ:0.f;
    float maxZ=mState.hasViewport?mState.vpMaxZ:1.f;
    r.position.x=vpX+(v.position.x+1.f)*0.5f*vpW;
    r.position.y=vpY+(1.f-v.position.y)*0.5f*vpH;
    r.position.z=minZ+v.position.z*(maxZ-minZ)*0.5f+(maxZ-minZ)*0.5f;
    return r;
}

NkSWVertex NkSWRasterizer::BaryInterp(const NkSWVertex& s0, const NkSWVertex& s1,
                                        const NkSWVertex& s2, float l0, float l1, float l2) const {
    float w0=s0.position.w,w1=s1.position.w,w2=s2.position.w;
    float ws=l0*w0+l1*w1+l2*w2;
    float iw=(ws!=0.f)?1.f/ws:0.f;
    auto lp=[&](float a,float b,float c){return(l0*a*w0+l1*b*w1+l2*c*w2)*iw;};
    NkSWVertex r;
    r.normal={lp(s0.normal.x,s1.normal.x,s2.normal.x),lp(s0.normal.y,s1.normal.y,s2.normal.y),lp(s0.normal.z,s1.normal.z,s2.normal.z)};
    r.uv={lp(s0.uv.x,s1.uv.x,s2.uv.x),lp(s0.uv.y,s1.uv.y,s2.uv.y)};
    r.color={lp(s0.color.x,s1.color.x,s2.color.x),lp(s0.color.y,s1.color.y,s2.color.y),
             lp(s0.color.z,s1.color.z,s2.color.z),lp(s0.color.w,s1.color.w,s2.color.w)};
    r.attrCount=NkMax(NkMax(s0.attrCount,s1.attrCount),s2.attrCount);
    for (uint32 i=0;i<r.attrCount&&i<16;i++) r.attrs[i]=lp(s0.attrs[i],s1.attrs[i],s2.attrs[i]);
    return r;
}

bool NkSWRasterizer::DepthTest(uint32 x, uint32 y, float z) {
    if (!mState.depthTarget) return true;
    if (!mState.pipeline||!mState.pipeline->depthTest) return true;
    NkVec4f d=mState.depthTarget->Read(x,y); float dz=d.x;
    bool pass=false;
    switch (mState.pipeline->depthOp) {
        case NkCompareOp::NK_LESS:          pass=z<dz;  break;
        case NkCompareOp::NK_LESS_EQUAL:    pass=z<=dz; break;
        case NkCompareOp::NK_GREATER:       pass=z>dz;  break;
        case NkCompareOp::NK_GREATER_EQUAL: pass=z>=dz; break;
        case NkCompareOp::NK_EQUAL:         pass=NkFabs(z-dz)<1e-6f; break;
        case NkCompareOp::NK_NOT_EQUAL:     pass=NkFabs(z-dz)>=1e-6f; break;
        case NkCompareOp::NK_ALWAYS:        pass=true;  break;
        case NkCompareOp::NK_NEVER:         pass=false; break;
        default:                         pass=z<dz;  break;
    }
    if (pass && mState.pipeline->depthWrite)
        mState.depthTarget->Write(x,y,{z,z,z,1.f});
    return pass;
}

float NkSWRasterizer::ApplyFactor(NkBlendFactor f, float src, float dst, float sA, float dA) const {
    switch (f) {
        case NkBlendFactor::NK_ZERO:                return 0.f;
        case NkBlendFactor::NK_ONE:                 return 1.f;
        case NkBlendFactor::NK_SRC_COLOR:           return src;
        case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR: return 1.f-src;
        case NkBlendFactor::NK_SRC_ALPHA:           return sA;
        case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA: return 1.f-sA;
        case NkBlendFactor::NK_DST_COLOR:           return dst;
        case NkBlendFactor::NK_ONE_MINUS_DST_COLOR: return 1.f-dst;
        case NkBlendFactor::NK_DST_ALPHA:           return dA;
        case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA: return 1.f-dA;
        default: return 1.f;
    }
}

NkVec4f NkSWRasterizer::Blend(const NkVec4f& src, const NkVec4f& dst) const {
    if (!mState.pipeline||!mState.pipeline->blendEnable) return src;
    auto& p=*mState.pipeline;
    NkVec4f r;
    r.x=src.x*ApplyFactor(p.srcColor,src.x,dst.x,src.w,dst.w)+dst.x*ApplyFactor(p.dstColor,src.x,dst.x,src.w,dst.w);
    r.y=src.y*ApplyFactor(p.srcColor,src.y,dst.y,src.w,dst.w)+dst.y*ApplyFactor(p.dstColor,src.y,dst.y,src.w,dst.w);
    r.z=src.z*ApplyFactor(p.srcColor,src.z,dst.z,src.w,dst.w)+dst.z*ApplyFactor(p.dstColor,src.z,dst.z,src.w,dst.w);
    r.w=src.w*ApplyFactor(p.srcAlpha,src.w,dst.w,src.w,dst.w)+dst.w*ApplyFactor(p.dstAlpha,src.w,dst.w,src.w,dst.w);
    return r;
}

void NkSWRasterizer::WriteFragment(uint32 x, uint32 y, float /*z*/, const NkSWVertex& frag) {
    NkVec4f color = frag.color;
    if (mState.shader && mState.shader->fragFn) {
        const void* texPtr = mState.textures[0] ? (const void*)mState.textures[0]
                                                 : (const void*)mState.textures[1];
        color = mState.shader->fragFn(frag, mState.uniformData, texPtr);
    }
    bool blendEnabled = mState.pipeline && mState.pipeline->blendEnable;
    for (uint32 rt=0; rt<mState.colorTargetCount; rt++) {
        NkSWTexture* target = mState.colorTargets[rt];
        if (!target) continue;
        NkVec4f finalColor = color;
        if (rt > 0 && frag.attrCount >= (rt+1)*4)
            finalColor = frag.GetAttrVec4(rt*4);
        if (blendEnabled) {
            NkVec4f dst = target->Read(x,y);
            finalColor = Blend(finalColor,dst);
        }
        target->Write(x,y,finalColor);
    }
}

void NkSWRasterizer::WriteSampleMSAA(uint32 x, uint32 y, uint32 sample, float z, const NkSWVertex& frag) {
    if (sample >= mState.msaaSamples) return;
    if (mState.msaaDepth) {
        uint32 W=mState.targetW, idx=(y*W+x)*mState.msaaSamples+sample;
        uint32 bpp=mState.msaaDepth->Bpp();
        uint8* dp=mState.msaaDepth->mips[0].Data();
        uint64 off=(uint64)idx*bpp;
        if (off+bpp<=mState.msaaDepth->mips[0].Size()) {
            float dz; memcpy(&dz,dp+off,4);
            bool pass=(mState.pipeline&&mState.pipeline->depthTest)?z<dz:true;
            if (!pass) return;
            if (mState.pipeline&&mState.pipeline->depthWrite) memcpy(dp+off,&z,4);
        }
    }
    NkVec4f color=frag.color;
    if (mState.shader&&mState.shader->fragFn) {
        const void* texPtr=mState.textures[0]?(const void*)mState.textures[0]:(const void*)mState.textures[1];
        color=mState.shader->fragFn(frag,mState.uniformData,texPtr);
    }
    for (uint32 rt=0;rt<mState.colorTargetCount;rt++) {
        NkSWTexture* msaaTex=(rt<kNkSWMaxColorTargets)?mState.msaaTargets[rt]:nullptr;
        if (!msaaTex) continue;
        NkVec4f c=(rt>0&&frag.attrCount>=(rt+1)*4)?frag.GetAttrVec4(rt*4):color;
        uint32 W=mState.targetW, idx=(y*W+x)*mState.msaaSamples+sample;
        uint32 bpp=msaaTex->Bpp();
        uint64 off=(uint64)idx*bpp;
        if (off+bpp<=msaaTex->mips[0].Size())
            NkSWWritePixel(msaaTex->mips[0].Data()+off,msaaTex->desc.format,c);
    }
}

void NkSWRasterizer::ResolveMSAA() {
    if (mState.msaaSamples<=1) return;
    uint32 W=mState.targetW, H=mState.targetH, N=mState.msaaSamples;
    for (uint32 rt=0;rt<mState.colorTargetCount;rt++) {
        NkSWTexture* msaaTex=(rt<kNkSWMaxColorTargets)?mState.msaaTargets[rt]:nullptr;
        NkSWTexture* dstTex=mState.colorTargets[rt];
        if (!msaaTex||!dstTex) continue;
        for (uint32 y=0;y<H;y++) for (uint32 x=0;x<W;x++) {
            NkVec4f acc={0,0,0,0};
            uint32 bpp=msaaTex->Bpp();
            for (uint32 s=0;s<N;s++) {
                uint32 idx=(y*W+x)*N+s;
                uint64 off=(uint64)idx*bpp;
                if (off+bpp>msaaTex->mips[0].Size()) continue;
                NkVec4f c=NkSWReadPixel(msaaTex->mips[0].Data()+off,msaaTex->desc.format);
                acc.x+=c.x; acc.y+=c.y; acc.z+=c.z; acc.w+=c.w;
            }
            float invN=1.f/(float)N;
            dstTex->Write(x,y,{acc.x*invN,acc.y*invN,acc.z*invN,acc.w*invN});
        }
    }
}

// =============================================================================
// RasterTriangle — SANS logs debug dans la boucle
// =============================================================================
void NkSWRasterizer::RasterTriangle(const NkSWVertex& s0, const NkSWVertex& s1, const NkSWVertex& s2) {
    // Guard : pas de target disponible
    if (mState.colorTargetCount == 0) return;

    float W=(float)mState.targetW, H=(float)mState.targetH;

    if (mState.wireframe) {
        RasterLine(s0,s1); RasterLine(s1,s2); RasterLine(s2,s0);
        return;
    }

    float area2=(s1.position.x-s0.position.x)*(s2.position.y-s0.position.y)
               -(s2.position.x-s0.position.x)*(s1.position.y-s0.position.y);
    if (mState.pipeline) {
        NkCullMode cm=mState.pipeline->cullMode;
        bool ccw=(mState.pipeline->frontFace==NkFrontFace::NK_CCW);
        if (cm!=NkCullMode::NK_NONE) {
            bool isFront=ccw?(area2>0.f):(area2<0.f);
            if ((cm==NkCullMode::NK_BACK&&isFront)||(cm==NkCullMode::NK_FRONT&&!isFront)) return;
        }
    }
    if (NkFabs(area2)<0.001f) return;
    float invArea=1.f/area2;

    float bx0=NkMin(s0.position.x,NkMin(s1.position.x,s2.position.x));
    float bx1=NkMax(s0.position.x,NkMax(s1.position.x,s2.position.x));
    float by0=NkMin(s0.position.y,NkMin(s1.position.y,s2.position.y));
    float by1=NkMax(s0.position.y,NkMax(s1.position.y,s2.position.y));

    int scX0=mState.hasScissor?mState.scissorX:0;
    int scY0=mState.hasScissor?mState.scissorY:0;
    int scX1=mState.hasScissor?mState.scissorX+mState.scissorW:(int)W;
    int scY1=mState.hasScissor?mState.scissorY+mState.scissorH:(int)H;
    scX0=NkMax(scX0,0); scY0=NkMax(scY0,0);
    scX1=NkMin(scX1,(int)W); scY1=NkMin(scY1,(int)H);

    int minX=NkMax((int)NkFloor(bx0),scX0);
    int maxX=NkMin((int)NkCeil(bx1), scX1-1);
    int minY=NkMax((int)NkFloor(by0),scY0);
    int maxY=NkMin((int)NkCeil(by1), scY1-1);
    if (minX>maxX||minY>maxY) return;

    uint32 N=mState.msaaSamples;
    const NkVec2f* offsets=NkSWGetMSAAOffsets(N);
    bool useMSAA=(N>1)&&(mState.msaaTargets[0]!=nullptr);

    for (int y=minY;y<=maxY;y++) for (int x=minX;x<=maxX;x++) {
        if (useMSAA) {
            for (uint32 s=0;s<N;s++) {
                float px=(float)x+0.5f+offsets[s].x, py=(float)y+0.5f+offsets[s].y;
                float l0=((s1.position.x-s2.position.x)*(py-s2.position.y)-(s1.position.y-s2.position.y)*(px-s2.position.x))*invArea;
                float l1=((s2.position.x-s0.position.x)*(py-s0.position.y)-(s2.position.y-s0.position.y)*(px-s0.position.x))*invArea;
                float l2=1.f-l0-l1;
                if (l0<0.f||l1<0.f||l2<0.f) continue;
                float z=l0*s0.position.z+l1*s1.position.z+l2*s2.position.z;
                NkSWVertex frag=BaryInterp(s0,s1,s2,l0,l1,l2);
                frag.position={px,py,z,0.f};
                WriteSampleMSAA((uint32)x,(uint32)y,s,z,frag);
            }
        } else {
            float px=(float)x+0.5f, py=(float)y+0.5f;
            float l0=((s1.position.x-s2.position.x)*(py-s2.position.y)-(s1.position.y-s2.position.y)*(px-s2.position.x))*invArea;
            float l1=((s2.position.x-s0.position.x)*(py-s0.position.y)-(s2.position.y-s0.position.y)*(px-s0.position.x))*invArea;
            float l2=1.f-l0-l1;
            if (l0<0.f||l1<0.f||l2<0.f) continue;
            float z=l0*s0.position.z+l1*s1.position.z+l2*s2.position.z;
            if (!DepthTest((uint32)x,(uint32)y,z)) continue;
            NkSWVertex frag=BaryInterp(s0,s1,s2,l0,l1,l2);
            frag.position={px,py,z,0.f};
            WriteFragment((uint32)x,(uint32)y,z,frag);
        }
    }
}

void NkSWRasterizer::RasterLine(const NkSWVertex& s0, const NkSWVertex& s1) {
    if (mState.colorTargetCount==0) return;
    float W=(float)mState.targetW, H=(float)mState.targetH;
    int x0=(int)(s0.position.x+0.5f), y0=(int)(s0.position.y+0.5f);
    int x1=(int)(s1.position.x+0.5f), y1=(int)(s1.position.y+0.5f);
    int dx=NkAbs(x1-x0),sx=(x0<x1)?1:-1;
    int dy=-NkAbs(y1-y0),sy=(y0<y1)?1:-1;
    int err=dx+dy, steps=NkMax(dx,-dy), step=0;
    while (true) {
        float t=(steps>0)?(float)step/(float)steps:0.f;
        NkSWVertex v=LerpVertex(s0,s1,t);
        v.position.x=(float)x0; v.position.y=(float)y0;
        if (x0>=0&&y0>=0&&x0<(int)W&&y0<(int)H)
            if (DepthTest((uint32)x0,(uint32)y0,v.position.z))
                WriteFragment((uint32)x0,(uint32)y0,v.position.z,v);
        if (x0==x1&&y0==y1) break;
        int e2=err*2;
        if (e2>=dy){err+=dy;x0+=sx;}
        if (e2<=dx){err+=dx;y0+=sy;}
        step++;
    }
}

void NkSWRasterizer::RasterPoint(const NkSWVertex& s) {
    if (mState.colorTargetCount==0) return;
    int x=(int)(s.position.x+0.5f), y=(int)(s.position.y+0.5f);
    if (x<0||y<0||x>=(int)mState.targetW||y>=(int)mState.targetH) return;
    if (!DepthTest((uint32)x,(uint32)y,s.position.z)) return;
    WriteFragment((uint32)x,(uint32)y,s.position.z,s);
}

void NkSWRasterizer::DrawOneTriangle(const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2) {
    NkSWVertex tri[3]={v0,v1,v2};
    NkSWVertex clipped[9];
    uint32 n=ClipTriangle(tri,clipped);
    if (n<3) return;
    NkSWVertex sc[9];
    for (uint32 i=0;i<n;i++) sc[i]=NDCToScreen(PerspectiveDivide(clipped[i]));
    for (uint32 i=1;i+1<n;i++) RasterTriangle(sc[0],sc[i],sc[i+1]);
}

// =============================================================================
// Geometry Shader CPU — avec callbacks par valeur (pas de dangling ptr)
// =============================================================================
void NkSWRasterizer::RunGeometryShader(const NkSWVertex* inVerts, uint32 inCount,
                                         NkPrimitiveTopology inputTopo) {
    if (!mState.shader || !mState.shader->geomFn) return;

    NkVector<NkSWVertex> pendingPrim;
    NkVector<NkSWVertex> outputVerts;

    // CORRECTION v4 : NkSWGeomContext contient NkFunction par valeur, pas ptr
    NkSWGeomContext ctx;
    ctx.uniformData    = mState.uniformData;
    ctx.outputTopology = mState.shader->geomOutputTopology;
    ctx.emitVertex     = [&](const NkSWVertex& v) { pendingPrim.PushBack(v); };
    ctx.endPrimitive   = [&]() {
        for (auto& v : pendingPrim) outputVerts.PushBack(v);
        pendingPrim.Clear();
    };

    uint32 primSize=1;
    switch (inputTopo) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:  primSize=3; break;
        case NkPrimitiveTopology::NK_TRIANGLE_STRIP: primSize=3; break;
        case NkPrimitiveTopology::NK_LINE_LIST:      primSize=2; break;
        default: primSize=3; break;
    }

    if (inputTopo==NkPrimitiveTopology::NK_TRIANGLE_STRIP||
        inputTopo==NkPrimitiveTopology::NK_TRIANGLE_FAN) {
        for (uint32 i=0;i+2<inCount;i++) {
            NkSWVertex prim[3];
            if (inputTopo==NkPrimitiveTopology::NK_TRIANGLE_STRIP) {
                if (i&1){prim[0]=inVerts[i+1];prim[1]=inVerts[i];prim[2]=inVerts[i+2];}
                else    {prim[0]=inVerts[i];prim[1]=inVerts[i+1];prim[2]=inVerts[i+2];}
            } else {
                prim[0]=inVerts[0]; prim[1]=inVerts[i+1]; prim[2]=inVerts[i+2];
            }
            mState.shader->geomFn(prim,3,ctx);
        }
    } else {
        for (uint32 i=0;i+primSize<=inCount;i+=primSize)
            mState.shader->geomFn(inVerts+i,primSize,ctx);
    }
    if (!pendingPrim.IsEmpty()) ctx.endPrimitive();

    uint32 outCount=(uint32)outputVerts.Size();
    switch (ctx.outputTopology) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:
            for (uint32 i=0;i+2<outCount;i+=3)
                DrawOneTriangle(outputVerts[i],outputVerts[i+1],outputVerts[i+2]);
            break;
        case NkPrimitiveTopology::NK_LINE_LIST:
            for (uint32 i=0;i+1<outCount;i+=2){
                NkSWVertex s0=NDCToScreen(PerspectiveDivide(outputVerts[i]));
                NkSWVertex s1=NDCToScreen(PerspectiveDivide(outputVerts[i+1]));
                RasterLine(s0,s1);
            }
            break;
        case NkPrimitiveTopology::NK_POINT_LIST:
            for (uint32 i=0;i<outCount;i++){
                NkSWVertex s=NDCToScreen(PerspectiveDivide(outputVerts[i]));
                RasterPoint(s);
            }
            break;
        default:
            for (uint32 i=0;i+2<outCount;i+=3)
                DrawOneTriangle(outputVerts[i],outputVerts[i+1],outputVerts[i+2]);
            break;
    }
}

void NkSWRasterizer::RunTessellation(const NkSWVertex* patch, uint32 patchSize) {
    if (!mState.shader||!mState.shader->tessFn) return;
    int divOuter=NkMax(1,(int)NkFloor(mState.shader->tessOuter+0.5f));

    if (patchSize==3) {
        NkVector<NkSWVertex> micro;
        for (int i=0;i<=divOuter;i++) {
            float u=(float)i/(float)divOuter;
            for (int j=0;j<=divOuter-i;j++) {
                float v=(float)j/(float)divOuter, w=1.f-u-v;
                NkSWVertex p;
                p.position={u*patch[0].position.x+v*patch[1].position.x+w*patch[2].position.x,
                             u*patch[0].position.y+v*patch[1].position.y+w*patch[2].position.y,
                             u*patch[0].position.z+v*patch[1].position.z+w*patch[2].position.z,
                             u*patch[0].position.w+v*patch[1].position.w+w*patch[2].position.w};
                p.uv={u*patch[0].uv.x+v*patch[1].uv.x+w*patch[2].uv.x,
                      u*patch[0].uv.y+v*patch[1].uv.y+w*patch[2].uv.y};
                p.color=patch[0].color;
                float bary[3]={u,v,w};
                p=mState.shader->tessFn(patch,patchSize,bary,mState.uniformData);
                micro.PushBack(p);
            }
        }
        int idx=0;
        for (int i=0;i<divOuter;i++) {
            int rowLen=divOuter-i+1, nextLen=rowLen-1;
            for (int j=0;j<nextLen;j++) {
                DrawOneTriangle(micro[idx+j],micro[idx+j+1],micro[idx+rowLen+j]);
                if (j<nextLen-1) DrawOneTriangle(micro[idx+j+1],micro[idx+rowLen+j+1],micro[idx+rowLen+j]);
            }
            idx+=rowLen;
        }
    } else if (patchSize==4) {
        for (int i=0;i<divOuter;i++) {
            float u0=(float)i/(float)divOuter, u1=(float)(i+1)/(float)divOuter;
            for (int j=0;j<divOuter;j++) {
                float v0=(float)j/(float)divOuter, v1=(float)(j+1)/(float)divOuter;
                auto bl=[&](float u,float v)->NkSWVertex{
                    NkSWVertex e0=LerpVertex(patch[0],patch[1],u);
                    NkSWVertex e1=LerpVertex(patch[3],patch[2],u);
                    NkSWVertex p=LerpVertex(e0,e1,v);
                    float bary[3]={u,v,0.f};
                    return mState.shader->tessFn(patch,patchSize,bary,mState.uniformData);
                };
                NkSWVertex p00=bl(u0,v0),p10=bl(u1,v0),p01=bl(u0,v1),p11=bl(u1,v1);
                DrawOneTriangle(p00,p10,p01); DrawOneTriangle(p10,p11,p01);
            }
        }
    }
}

// ── API publique ──────────────────────────────────────────────────────────────
void NkSWRasterizer::DrawTriangles(const NkSWVertex* v, uint32 n) {
    if (mState.shader&&mState.shader->geomFn){RunGeometryShader(v,n,NkPrimitiveTopology::NK_TRIANGLE_LIST);return;}
    if (mState.shader&&mState.shader->tessFn&&mState.shader->patchSize>0){
        uint32 ps=mState.shader->patchSize;
        for (uint32 i=0;i+ps<=n;i+=ps) RunTessellation(v+i,ps); return;
    }
    for (uint32 i=0;i+2<n;i+=3) DrawOneTriangle(v[i],v[i+1],v[i+2]);
    if (mState.msaaSamples>1) ResolveMSAA();
}

void NkSWRasterizer::DrawTrianglesIdx(const NkSWVertex* v, const uint32* idx, uint32 n) {
    for (uint32 i=0;i+2<n;i+=3) DrawOneTriangle(v[idx[i]],v[idx[i+1]],v[idx[i+2]]);
    if (mState.msaaSamples>1) ResolveMSAA();
}

void NkSWRasterizer::DrawTriangleStrip(const NkSWVertex* v, uint32 n) {
    if (mState.shader&&mState.shader->geomFn){RunGeometryShader(v,n,NkPrimitiveTopology::NK_TRIANGLE_STRIP);return;}
    for (uint32 i=0;i+2<n;i++){
        if (i&1) DrawOneTriangle(v[i+1],v[i],v[i+2]);
        else     DrawOneTriangle(v[i],v[i+1],v[i+2]);
    }
    if (mState.msaaSamples>1) ResolveMSAA();
}

void NkSWRasterizer::DrawTriangleFan(const NkSWVertex* v, uint32 n) {
    if (mState.shader&&mState.shader->geomFn){RunGeometryShader(v,n,NkPrimitiveTopology::NK_TRIANGLE_FAN);return;}
    for (uint32 i=1;i+1<n;i++) DrawOneTriangle(v[0],v[i],v[i+1]);
    if (mState.msaaSamples>1) ResolveMSAA();
}

void NkSWRasterizer::DrawLines(const NkSWVertex* v, uint32 n) {
    if (mState.shader&&mState.shader->geomFn){RunGeometryShader(v,n,NkPrimitiveTopology::NK_LINE_LIST);return;}
    for (uint32 i=0;i+1<n;i+=2){
        NkSWVertex s0=NDCToScreen(PerspectiveDivide(v[i]));
        NkSWVertex s1=NDCToScreen(PerspectiveDivide(v[i+1]));
        RasterLine(s0,s1);
    }
}

void NkSWRasterizer::DrawLineStrip(const NkSWVertex* v, uint32 n) {
    for (uint32 i=0;i+1<n;i++){
        NkSWVertex s0=NDCToScreen(PerspectiveDivide(v[i]));
        NkSWVertex s1=NDCToScreen(PerspectiveDivide(v[i+1]));
        RasterLine(s0,s1);
    }
}

void NkSWRasterizer::DrawLineLoop(const NkSWVertex* v, uint32 n) {
    DrawLineStrip(v,n);
    if (n>=2){
        NkSWVertex s0=NDCToScreen(PerspectiveDivide(v[n-1]));
        NkSWVertex s1=NDCToScreen(PerspectiveDivide(v[0]));
        RasterLine(s0,s1);
    }
}

void NkSWRasterizer::DrawPoints(const NkSWVertex* v, uint32 n) {
    for (uint32 i=0;i<n;i++){
        NkSWVertex s=NDCToScreen(PerspectiveDivide(v[i]));
        RasterPoint(s);
    }
}

} // namespace nkentseu