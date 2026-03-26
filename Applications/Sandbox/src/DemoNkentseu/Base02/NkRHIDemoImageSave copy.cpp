// =============================================================================
// NkRHIDemoImageSave.cpp
// Démo RHI multi-backend — rendu offscreen + overlay 2D + sauvegarde PNG via NKImage
//
//  • Backends : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//    sélection via : --backend=opengl|vulkan|dx11|dx12|sw|metal
//  • Offscreen  : cube 3D rotatif → texture 512×512
//  • Affichage  : quad plein-écran qui échantillonne la texture offscreen
//  • Overlay    : 2 barres semi-transparentes (haut/bas), sans depth test
//  • Sauvegarde : touche S → screenshot.png (NKImage)
//  • Touche ESC → quitter
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"

#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

#include "NKImage/NKImage.h"

// SPIR-V précompilé pour Vulkan (MinGW : glslang ABI incompatible)
#include "NkRHIDemoImageSaveVkSpv.inl"

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Shaders GLSL (OpenGL + Software fallback)
// =============================================================================

// ── Cube 3D : Phong directionnel ─────────────────────────────────────────────
static const char* kGLSL_CubeVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(std140, binding=0) uniform CubeUBO { mat4 uModel; mat4 uView; mat4 uProj; };
out vec3 vColor;
out vec3 vNormal;
void main(){
    vec4 wPos = uModel * vec4(aPos,1.0);
    vNormal   = mat3(uModel) * aNormal;
    vColor    = aColor;
    gl_Position = uProj * uView * wPos;
})";
static const char* kGLSL_CubeFS = R"(#version 460 core
in vec3 vColor; in vec3 vNormal;
out vec4 fragColor;
void main(){
    vec3 L = normalize(vec3(0.577,0.577,0.577));
    float d = max(dot(normalize(vNormal),L),0.0);
    fragColor = vec4(vColor*(0.15+0.85*d),1.0);
})";

// ── Quad plein-écran : échantillonner texture offscreen ───────────────────────
static const char* kGLSL_QuadVS = R"(#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); })";
static const char* kGLSL_QuadFS = R"(#version 460 core
in vec2 vUV; out vec4 fragColor;
layout(binding=1) uniform sampler2D uOffscreen;
void main(){ fragColor = texture(uOffscreen, vUV); })";

// ── Overlay 2D : formes colorées semi-transparentes ───────────────────────────
static const char* kGLSL_OverlayVS = R"(#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aColor;
layout(std140, binding=0) uniform OrthoUBO { mat4 uOrtho; };
out vec4 vColor;
void main(){ vColor=vec4(aColor,0.85); gl_Position=uOrtho*vec4(aPos,0.0,1.0); })";
static const char* kGLSL_OverlayFS = R"(#version 460 core
in vec4 vColor; out vec4 fragColor;
void main(){ fragColor=vColor; })";

// =============================================================================
// Shaders HLSL (DX11 / DX12)
// =============================================================================
static const char* kHLSL_CubeVS = R"(
cbuffer UBO : register(b0){ float4x4 model,view,proj; };
struct VI { float3 pos:POSITION; float3 nor:NORMAL; float3 col:COLOR; };
struct VO { float4 pos:SV_Position; float3 col:COLOR; float3 nor:NORMAL; };
VO VSMain(VI i){
    VO o;
    float4 wp = mul(model,float4(i.pos,1)); o.nor=mul((float3x3)model,i.nor);
    o.pos = mul(proj,mul(view,wp)); o.col=i.col; return o;
})";
static const char* kHLSL_CubePS = R"(
struct VO { float4 pos:SV_Position; float3 col:COLOR; float3 nor:NORMAL; };
float4 PSMain(VO i):SV_Target{
    float3 L=normalize(float3(0.577,0.577,0.577));
    float d=max(dot(normalize(i.nor),L),0);
    return float4(i.col*(0.15+0.85*d),1);
})";

static const char* kHLSL_QuadVS = R"(
struct VI { float2 pos:POSITION; float2 uv:TEXCOORD; };
struct VO { float4 pos:SV_Position; float2 uv:TEXCOORD; };
VO VSMain(VI i){ VO o; o.pos=float4(i.pos,0,1); o.uv=i.uv; return o; })";
static const char* kHLSL_QuadPS = R"(
Texture2D uOffscreen : register(t1);
SamplerState uSampler : register(s1);
struct VO { float4 pos:SV_Position; float2 uv:TEXCOORD; };
float4 PSMain(VO i):SV_Target{ return uOffscreen.Sample(uSampler,i.uv); })";

static const char* kHLSL_OverlayVS = R"(
cbuffer OrthoUBO : register(b0){ float4x4 uOrtho; };
struct VI { float2 pos:POSITION; float3 col:COLOR; };
struct VO { float4 pos:SV_Position; float4 col:COLOR; };
VO VSMain(VI i){ VO o; o.pos=mul(uOrtho,float4(i.pos,0,1)); o.col=float4(i.col,0.85); return o; })";
static const char* kHLSL_OverlayPS = R"(
struct VO { float4 pos:SV_Position; float4 col:COLOR; };
float4 PSMain(VO i):SV_Target{ return i.col; })";

// =============================================================================
// Géométrie — cube 3D (36 vertices, 6 faces colorées)
// =============================================================================
struct Vtx3D { float x,y,z, nx,ny,nz, r,g,b; };

static const Vtx3D kCubeVerts[] = {
    {-1,-1, 1, 0,0,1, 1,.2f,.2f}, { 1,-1, 1, 0,0,1, 1,.2f,.2f}, { 1, 1, 1, 0,0,1, 1,.2f,.2f},
    {-1,-1, 1, 0,0,1, 1,.2f,.2f}, { 1, 1, 1, 0,0,1, 1,.2f,.2f}, {-1, 1, 1, 0,0,1, 1,.2f,.2f},
    { 1,-1,-1, 0,0,-1,.2f,.2f,1}, {-1,-1,-1, 0,0,-1,.2f,.2f,1}, {-1, 1,-1, 0,0,-1,.2f,.2f,1},
    { 1,-1,-1, 0,0,-1,.2f,.2f,1}, {-1, 1,-1, 0,0,-1,.2f,.2f,1}, { 1, 1,-1, 0,0,-1,.2f,.2f,1},
    { 1,-1,-1, 1,0,0, .2f,1,.2f}, { 1,-1, 1, 1,0,0, .2f,1,.2f}, { 1, 1, 1, 1,0,0, .2f,1,.2f},
    { 1,-1,-1, 1,0,0, .2f,1,.2f}, { 1, 1, 1, 1,0,0, .2f,1,.2f}, { 1, 1,-1, 1,0,0, .2f,1,.2f},
    {-1,-1, 1,-1,0,0, 1,1,.2f},  {-1,-1,-1,-1,0,0, 1,1,.2f},   {-1, 1,-1,-1,0,0, 1,1,.2f},
    {-1,-1, 1,-1,0,0, 1,1,.2f},  {-1, 1,-1,-1,0,0, 1,1,.2f},   {-1, 1, 1,-1,0,0, 1,1,.2f},
    {-1, 1, 1, 0,1,0, .2f,1,1},  { 1, 1, 1, 0,1,0, .2f,1,1},   { 1, 1,-1, 0,1,0, .2f,1,1},
    {-1, 1, 1, 0,1,0, .2f,1,1},  { 1, 1,-1, 0,1,0, .2f,1,1},   {-1, 1,-1, 0,1,0, .2f,1,1},
    {-1,-1,-1, 0,-1,0, 1,.2f,1}, { 1,-1,-1, 0,-1,0, 1,.2f,1},  { 1,-1, 1, 0,-1,0, 1,.2f,1},
    {-1,-1,-1, 0,-1,0, 1,.2f,1}, { 1,-1, 1, 0,-1,0, 1,.2f,1},  {-1,-1, 1, 0,-1,0, 1,.2f,1},
};
static constexpr uint32 kCubeCount = 36;

// ── Quad plein-écran NDC ──────────────────────────────────────────────────────
struct VtxQuad { float x,y, u,v; };
static const VtxQuad kQuadVerts[] = {
    {-1,-1,0,1},{ 1,-1,1,1},{ 1, 1,1,0},
    {-1,-1,0,1},{ 1, 1,1,0},{-1, 1,0,0},
};

// ── Overlay 2D ────────────────────────────────────────────────────────────────
struct Vtx2D { float x,y, r,g,b; };
static Vtx2D kOverlayVerts[60];
static uint32 kOverlayCount = 0;

static void BuildOverlay(float W, float H) {
    kOverlayCount = 0;
    float bH = 30.f;
    auto push = [](float x,float y,float r,float g,float b){ kOverlayVerts[kOverlayCount++]={x,y,r,g,b}; };
    auto rect = [&](float x0,float y0,float x1,float y1,float r,float g,float b){
        push(x0,y0,r,g,b); push(x1,y0,r,g,b); push(x1,y1,r,g,b);
        push(x0,y0,r,g,b); push(x1,y1,r,g,b); push(x0,y1,r,g,b);
    };
    rect(0,    0,   W, bH,       0.06f,0.06f,0.15f);
    rect(0, H-bH,   W, H,        0.06f,0.06f,0.15f);
    rect(0, bH-3,   W, bH,       0.2f, 0.6f, 1.f);
    rect(0, H-bH,   W, H-bH+3,   0.2f, 0.6f, 1.f);
    rect(8,    6,   160, bH-6,    0.1f, 0.4f, 0.7f);
    rect(W-130,6,   W-8, bH-6,    0.1f, 0.55f,0.3f);
}

// =============================================================================
// UBOs
// =============================================================================
struct alignas(16) CubeUbo  { float model[16], view[16], proj[16]; };
struct alignas(16) QuadUbo  { NkSWTexture* srcTex; uint8 _pad[8]; };  // SW seulement
struct alignas(16) OrthoUbo { float ortho[16]; };

// =============================================================================
// Math helpers
// =============================================================================
static void Mat4Identity(float m[16])
    { for(int i=0;i<16;i++) m[i]=0; m[0]=m[5]=m[10]=m[15]=1.f; }

static void Mat4Perspective(float m[16],float fovR,float asp,float zn,float zf){
    for(int i=0;i<16;i++) m[i]=0;
    float f=1.f/NkTan(fovR*.5f);
    m[0]=f/asp; m[5]=f;
    m[10]=(zf+zn)/(zn-zf); m[11]=-1.f; m[14]=(2.f*zf*zn)/(zn-zf);
}

static void Mat4LookAt(float m[16],float ex,float ey,float ez,float cx,float cy,float cz){
    float fx=cx-ex,fy=cy-ey,fz=cz-ez,l=NkSqrt(fx*fx+fy*fy+fz*fz);
    if(l>0){fx/=l;fy/=l;fz/=l;}
    float rx=fy*0-fz*1,ry=fz*0-fx*0,rz=fx*1-fy*0,rl=NkSqrt(rx*rx+ry*ry+rz*rz);
    if(rl>0){rx/=rl;ry/=rl;rz/=rl;}
    float ux=ry*fz-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy-ry*fx;
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=rx;m[1]=ux;m[2]=-fx;m[4]=ry;m[5]=uy;m[6]=-fy;m[8]=rz;m[9]=uz;m[10]=-fz;
    m[12]=-(rx*ex+ry*ey+rz*ez);m[13]=-(ux*ex+uy*ey+uz*ez);m[14]=(fx*ex+fy*ey+fz*ez);m[15]=1;
}

static void Mat4RotY(float m[16],float a)
    { Mat4Identity(m); m[0]=NkCos(a);m[2]=NkSin(a);m[8]=-NkSin(a);m[10]=NkCos(a); }
static void Mat4RotX(float m[16],float a)
    { Mat4Identity(m); m[5]=NkCos(a);m[6]=-NkSin(a);m[9]=NkSin(a);m[10]=NkCos(a); }
static void Mat4Mul(float o[16],const float a[16],const float b[16]){
    for(int c=0;c<4;c++) for(int r=0;r<4;r++){
        float s=0; for(int k=0;k<4;k++) s+=a[r+k*4]*b[k+c*4]; o[r+c*4]=s;
    }
}
static void Mat4Ortho(float m[16],float l,float r,float b,float t,float n,float f){
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=2.f/(r-l);m[5]=2.f/(t-b);m[10]=-2.f/(f-n);
    m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);m[15]=1;
}
static void MulMV(const float m[16],float x,float y,float z,float w,
                  float& ox,float& oy,float& oz,float& ow){
    ox=m[0]*x+m[4]*y+m[8]*z+m[12]*w; oy=m[1]*x+m[5]*y+m[9]*z+m[13]*w;
    oz=m[2]*x+m[6]*y+m[10]*z+m[14]*w;ow=m[3]*x+m[7]*y+m[11]*z+m[15]*w;
}

// =============================================================================
// Backend selection
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); i++) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i] == "--backend=metal"   || args[i] == "-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_SOFTWARE; // défaut
}

static const char* ApiName(NkGraphicsApi a) {
    switch(a){
        case NkGraphicsApi::NK_API_VULKAN:     return "Vulkan";
        case NkGraphicsApi::NK_API_DIRECTX11:  return "DX11";
        case NkGraphicsApi::NK_API_DIRECTX12:  return "DX12";
        case NkGraphicsApi::NK_API_METAL:      return "Metal";
        case NkGraphicsApi::NK_API_SOFTWARE:   return "Software";
        case NkGraphicsApi::NK_API_OPENGL:     return "OpenGL";
        default: return "Unknown";
    }
}

// =============================================================================
// SW CPU shaders
// =============================================================================
static NkSWVertexShader MakeCubeVertSW() {
    return [](const void* vdata, uint32 idx, const void* udata) -> NkSWVertex {
        const Vtx3D*   v  = static_cast<const Vtx3D*>(vdata) + idx;
        const CubeUbo* ub = static_cast<const CubeUbo*>(udata);
        NkSWVertex out;
        if (!ub) { out.position={v->x,v->y,v->z,1}; out.color={v->r,v->g,v->b,1}; return out; }
        float wx,wy,wz,ww; MulMV(ub->model,v->x,v->y,v->z,1,wx,wy,wz,ww);
        float vx,vy,vz,vw; MulMV(ub->view, wx,wy,wz,ww,vx,vy,vz,vw);
        float cx,cy,cz,cw; MulMV(ub->proj, vx,vy,vz,vw,cx,cy,cz,cw);
        out.position={cx,cy,cz,cw};
        float lx=.577f,ly=.577f,lz=.577f;
        float d=NkMax(0.f,v->nx*lx+v->ny*ly+v->nz*lz);
        float lit=.15f+.85f*d;
        out.color={v->r*lit,v->g*lit,v->b*lit,1};
        return out;
    };
}
static NkSWPixelShader MakeCubeFragSW() {
    return [](const NkSWVertex& v, const void*, const void*) -> NkSWColor { return v.color; };
}

static NkSWVertexShader MakeQuadVertSW() {
    return [](const void* vdata, uint32 idx, const void*) -> NkSWVertex {
        const VtxQuad* v = static_cast<const VtxQuad*>(vdata)+idx;
        NkSWVertex out; out.position={v->x,v->y,0,1}; out.uv={v->u,v->v}; return out;
    };
}
// Fragment shader quad SW : lit NkSWTexture* depuis l'UBO
static NkSWPixelShader MakeQuadFragSW() {
    return [](const NkSWVertex& v, const void* udata, const void*) -> NkSWColor {
        const QuadUbo* ub = static_cast<const QuadUbo*>(udata);
        if (!ub || !ub->srcTex) return {v.uv.x,v.uv.y,.5f,1};
        return ub->srcTex->Sample(v.uv.x,v.uv.y,0);
    };
}

static NkSWVertexShader MakeOverlayVertSW() {
    return [](const void* vdata, uint32 idx, const void* udata) -> NkSWVertex {
        const Vtx2D*    v  = static_cast<const Vtx2D*>(vdata)+idx;
        const OrthoUbo* ub = static_cast<const OrthoUbo*>(udata);
        NkSWVertex out;
        if (!ub){ out.position={v->x,v->y,0,1}; out.color={v->r,v->g,v->b,1}; return out; }
        float cx,cy,cz,cw; MulMV(ub->ortho,v->x,v->y,0,1,cx,cy,cz,cw);
        out.position={cx,cy,cz,cw}; out.color={v->r,v->g,v->b,.85f};
        return out;
    };
}
static NkSWPixelShader MakeOverlayFragSW() {
    return [](const NkSWVertex& v, const void*, const void*) -> NkSWColor { return v.color; };
}

// =============================================================================
// Création de shaders multi-backend
// =============================================================================
static NkShaderHandle MakeCubeShader(NkIDevice* dev, NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "Cube3D";
    // Déclarés à portée de fonction pour rester valides jusqu'à CreateShader.
    NkSWVertexShader vs; NkSWPixelShader fs;
    if (api == NkGraphicsApi::NK_API_VULKAN) {
        sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                    kVkImgSaveCubeVertSpv,
                    (uint64)kVkImgSaveCubeVertSpvWordCount * sizeof(uint32));
        sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                    kVkImgSaveCubeFragSpv,
                    (uint64)kVkImgSaveCubeFragSpvWordCount * sizeof(uint32));
    } else if (api == NkGraphicsApi::NK_API_DIRECTX11 || api == NkGraphicsApi::NK_API_DIRECTX12) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_CubeVS, "VSMain");
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_CubePS, "PSMain");
    } else if (api == NkGraphicsApi::NK_API_SOFTWARE) {
        vs = MakeCubeVertSW(); fs = MakeCubeFragSW();
        NkShaderStageDesc svs; svs.stage=NkShaderStage::NK_VERTEX;   svs.cpuVertFn=&vs; sd.stages.PushBack(svs);
        NkShaderStageDesc sfs; sfs.stage=NkShaderStage::NK_FRAGMENT; sfs.cpuFragFn=&fs; sd.stages.PushBack(sfs);
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_CubeVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_CubeFS);
    }
    return dev->CreateShader(sd);
}

static NkShaderHandle MakeQuadShader(NkIDevice* dev, NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "QuadOffscreen";
    // Déclarés à portée de fonction pour rester valides jusqu'à CreateShader.
    NkSWVertexShader vs; NkSWPixelShader fs;
    if (api == NkGraphicsApi::NK_API_VULKAN) {
        sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                    kVkImgSaveQuadVertSpv,
                    (uint64)kVkImgSaveQuadVertSpvWordCount * sizeof(uint32));
        sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                    kVkImgSaveQuadFragSpv,
                    (uint64)kVkImgSaveQuadFragSpvWordCount * sizeof(uint32));
    } else if (api == NkGraphicsApi::NK_API_DIRECTX11 || api == NkGraphicsApi::NK_API_DIRECTX12) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_QuadVS, "VSMain");
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_QuadPS, "PSMain");
    } else if (api == NkGraphicsApi::NK_API_SOFTWARE) {
        vs = MakeQuadVertSW(); fs = MakeQuadFragSW();
        NkShaderStageDesc svs; svs.stage=NkShaderStage::NK_VERTEX;   svs.cpuVertFn=&vs; sd.stages.PushBack(svs);
        NkShaderStageDesc sfs; sfs.stage=NkShaderStage::NK_FRAGMENT; sfs.cpuFragFn=&fs; sd.stages.PushBack(sfs);
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_QuadVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_QuadFS);
    }
    return dev->CreateShader(sd);
}

static NkShaderHandle MakeOverlayShader(NkIDevice* dev, NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "Overlay2D";
    // Déclarés à portée de fonction pour rester valides jusqu'à CreateShader.
    NkSWVertexShader vs; NkSWPixelShader fs;
    if (api == NkGraphicsApi::NK_API_VULKAN) {
        sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                    kVkImgSaveOverlayVertSpv,
                    (uint64)kVkImgSaveOverlayVertSpvWordCount * sizeof(uint32));
        sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                    kVkImgSaveOverlayFragSpv,
                    (uint64)kVkImgSaveOverlayFragSpvWordCount * sizeof(uint32));
    } else if (api == NkGraphicsApi::NK_API_DIRECTX11 || api == NkGraphicsApi::NK_API_DIRECTX12) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_OverlayVS, "VSMain");
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_OverlayPS, "PSMain");
    } else if (api == NkGraphicsApi::NK_API_SOFTWARE) {
        vs = MakeOverlayVertSW(); fs = MakeOverlayFragSW();
        NkShaderStageDesc svs; svs.stage=NkShaderStage::NK_VERTEX;   svs.cpuVertFn=&vs; sd.stages.PushBack(svs);
        NkShaderStageDesc sfs; sfs.stage=NkShaderStage::NK_FRAGMENT; sfs.cpuFragFn=&fs; sd.stages.PushBack(sfs);
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_OverlayVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_OverlayFS);
    }
    return dev->CreateShader(sd);
}

// =============================================================================
// Main
// =============================================================================
int nkmain(const NkEntryState& state) {
    NkGraphicsApi api = ParseBackend(state.args);
    logger_src.Infof("[ImageSave] Backend : %s\n", ApiName(api));

    // ── Fenêtre ──────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NkRHI — Offscreen+Overlay+ImageSave (S=save ESC=quit)";
    winCfg.width     = 900;
    winCfg.height    = 600;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) return 1;
    uint32 W = winCfg.width, H = winCfg.height;

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkDeviceInitInfo di;
    di.api    = api;
    di.surface= window.GetSurfaceDesc();
    di.width  = W;
    di.height = H;
    di.context.vulkan.appName    = "NkRHIDemoImageSave";
    di.context.vulkan.engineName = "Unkenty";
    di.context.software.threading= true;
    di.context.software.useSSE   = true;

    NkIDevice* device = NkDeviceFactory::Create(di);
    if (!device || !device->IsValid()) {
        logger_src.Errorf("[ImageSave] Echec création device\n");
        window.Close(); return 1;
    }

    const bool isSW = (api == NkGraphicsApi::NK_API_SOFTWARE);

    // ── Framebuffer offscreen 512×512 ────────────────────────────────────────
    constexpr uint32 OW=512, OH=512;

    NkTextureDesc tdOff;
    tdOff.format    = NkGPUFormat::NK_RGBA8_UNORM;
    tdOff.width     = OW; tdOff.height = OH;
    tdOff.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
    tdOff.debugName = "OffColor";
    NkTextureHandle hOffColor = device->CreateTexture(tdOff);

    NkTextureDesc tdDep;
    tdDep.format    = NkGPUFormat::NK_D32_FLOAT;
    tdDep.width     = OW; tdDep.height = OH;
    tdDep.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
    tdDep.debugName = "OffDepth";
    NkTextureHandle hOffDepth = device->CreateTexture(tdDep);

    NkRenderPassDesc rpOff;
    rpOff.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_UNORM))
         .SetDepth(NkAttachmentDesc::Depth());
    NkRenderPassHandle hRPOff = device->CreateRenderPass(rpOff);

    NkFramebufferDesc fbOff;
    fbOff.renderPass = hRPOff;
    fbOff.colorAttachments.PushBack(hOffColor);
    fbOff.depthAttachment = hOffDepth;
    fbOff.width=OW; fbOff.height=OH; fbOff.debugName="OffFBO";
    NkFramebufferHandle hFBOff = device->CreateFramebuffer(fbOff);

    // ── Swapchain ─────────────────────────────────────────────────────────────
    NkRenderPassHandle  hRPSwap = device->GetSwapchainRenderPass();
    NkFramebufferHandle hFBSwap = device->GetSwapchainFramebuffer();

    // ── Sampler pour la texture offscreen (non-SW) ────────────────────────────
    NkSamplerHandle hSampler;
    if (!isSW) {
        NkSamplerDesc sd; sd.minFilter=NkFilter::NK_LINEAR; sd.magFilter=NkFilter::NK_LINEAR;
        hSampler = device->CreateSampler(sd);
    }

    // ── Chargement d'image depuis fichier (--texture=<path> ou par défaut Resources/)
    // Candidats par défaut (relatifs au répertoire de travail)
    static const char* sDefaultTextures[] = {
        "Resources/Textures/Checkerboard.png",
        "Resources/Textures/container2.png",
        "Resources/Textures/Brick/brick_wall_001_diffuse_2k.jpg",
        "Resources/Textures/container.jpg",
        nullptr
    };
    const char* imgTexPath = nullptr;
    for (usize i = 1; i < state.args.Size(); i++) {
        const char* s = state.args[i].CStr();
        if (::strncmp(s, "--texture=", 10) == 0) { imgTexPath = s + 10; break; }
    }
    // Aucun argument → essayer les candidats par défaut
    if (!imgTexPath) {
        for (int k = 0; sDefaultTextures[k]; ++k) {
            FILE* f = ::fopen(sDefaultTextures[k], "rb");
            if (f) { ::fclose(f); imgTexPath = sDefaultTextures[k]; break; }
        }
    }

    NkTextureHandle hImgTex;
    NkSamplerHandle hSamplerImg;

    if (imgTexPath) {
        // Vérifier si le fichier est accessible avant de décoder
        {
            FILE* fcheck = ::fopen(imgTexPath, "rb");
            if (!fcheck)
                logger_src.Infof("[ImageSave] Fichier introuvable : %s\n", imgTexPath);
            else
                ::fclose(fcheck);
        }
        // Chargement via NKImage — forcer RGBA32 (4 canaux)
        NkImage* img = NkImage::Load(imgTexPath, 4);
        if (!img) {
            logger_src.Infof("[ImageSave] Decodage echoue : %s\n", imgTexPath);
        } else {
            // Convertir en RGBA32 si le format natif est différent
            NkImage* imgRGBA = (img->Format() == NkImagePixelFormat::NK_RGBA32)
                               ? img : img->Convert(NkImagePixelFormat::NK_RGBA32);
            if (imgRGBA) {
                // Créer la texture GPU avec les pixels de l'image
                NkTextureDesc td = NkTextureDesc::Tex2D(
                    (uint32)imgRGBA->Width(), (uint32)imgRGBA->Height(),
                    NkGPUFormat::NK_RGBA8_UNORM);
                td.initialData = imgRGBA->Pixels();
                td.debugName   = "LoadedImage";
                hImgTex = device->CreateTexture(td);
                if (hImgTex.IsValid()) {
                    logger_src.Infof("[ImageSave] Texture image %dx%d chargee sur GPU\n",
                                     imgRGBA->Width(), imgRGBA->Height());
                    if (!isSW) {
                        NkSamplerDesc sd;
                        sd.minFilter = NkFilter::NK_LINEAR;
                        sd.magFilter = NkFilter::NK_LINEAR;
                        hSamplerImg  = device->CreateSampler(sd);
                    }
                }
                if (imgRGBA != img) imgRGBA->Free();
            }
            img->Free();
        }
    }

    // ── Buffers ───────────────────────────────────────────────────────────────
    NkBufferHandle hVBCube    = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(kCubeVerts), kCubeVerts));
    NkBufferHandle hVBQuad    = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(kQuadVerts), kQuadVerts));
    NkBufferHandle hVBOverlay = device->CreateBuffer(NkBufferDesc::VertexDynamic(sizeof(kOverlayVerts)));
    BuildOverlay((float)W,(float)H);
    device->WriteBuffer(hVBOverlay, kOverlayVerts, sizeof(kOverlayVerts), 0);

    NkBufferHandle hUBOCube    = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(CubeUbo)));
    NkBufferHandle hUBOQuad    = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(QuadUbo)));
    NkBufferHandle hUBOOrtho   = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(OrthoUbo)));
    // UBO dédié au background image (SW : contient NkSWTexture*, non-SW : inutilisé)
    NkBufferHandle hUBOImgBg   = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(QuadUbo)));

    // ── Descriptor set layouts ────────────────────────────────────────────────
    // Layout A (cube + overlay) : binding 0 = UBO
    NkDescriptorSetLayoutDesc dslA;
    dslA.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    NkDescSetHandle hDSLA = device->CreateDescriptorSetLayout(dslA);

    // Layout B (quad) :
    //   SW       : binding 0 = UBO (contient NkSWTexture*)
    //   non-SW   : binding 0 = UBO (vide), binding 1 = combined image sampler
    NkDescriptorSetLayoutDesc dslB;
    dslB.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    if (!isSW)
        dslB.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hDSLB = device->CreateDescriptorSetLayout(dslB);

    NkDescSetHandle hDSCube    = device->AllocateDescriptorSet(hDSLA);
    NkDescSetHandle hDSOrtho   = device->AllocateDescriptorSet(hDSLA);
    NkDescSetHandle hDSQuad    = device->AllocateDescriptorSet(hDSLB);
    // Descriptor set pour le background image (même layout que quad)
    NkDescSetHandle hDSImgBg   = device->AllocateDescriptorSet(hDSLB);

    auto bindBuf = [&](NkDescSetHandle ds, uint32 b, NkBufferHandle buf) {
        NkDescriptorWrite w; w.set=ds; w.binding=b;
        w.type=NkDescriptorType::NK_UNIFORM_BUFFER; w.buffer=buf;
        device->UpdateDescriptorSets(&w,1);
    };
    bindBuf(hDSCube,  0, hUBOCube);
    bindBuf(hDSOrtho, 0, hUBOOrtho);
    bindBuf(hDSQuad,  0, hUBOQuad);
    bindBuf(hDSImgBg, 0, hUBOImgBg);

    // Lier la texture offscreen au descriptor set du quad (non-SW)
    if (!isSW && hSampler.IsValid()) {
        NkDescriptorWrite w; w.set=hDSQuad; w.binding=1;
        w.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
        w.texture=hOffColor; w.sampler=hSampler;
        w.textureLayout=NkResourceState::NK_SHADER_READ;
        device->UpdateDescriptorSets(&w,1);
    }

    // Lier la texture image au descriptor set ImgBg (non-SW)
    if (!isSW && hImgTex.IsValid() && hSamplerImg.IsValid()) {
        NkDescriptorWrite w; w.set=hDSImgBg; w.binding=1;
        w.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
        w.texture=hImgTex; w.sampler=hSamplerImg;
        w.textureLayout=NkResourceState::NK_SHADER_READ;
        device->UpdateDescriptorSets(&w,1);
    }

    // ── Shaders ───────────────────────────────────────────────────────────────
    NkShaderHandle hShCube    = MakeCubeShader   (device, api);
    NkShaderHandle hShQuad    = MakeQuadShader   (device, api);
    NkShaderHandle hShOverlay = MakeOverlayShader(device, api);

    // ── Vertex layouts ────────────────────────────────────────────────────────
    // Cube : pos(xyz) + normal(xyz) + color(rgb)
    NkVertexLayout vlCube;
    vlCube.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0,              "POSITION", 0)
          .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3*sizeof(float),"NORMAL",   0)
          .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 6*sizeof(float),"COLOR",    0)
          .AddBinding(0, sizeof(Vtx3D));

    // Quad : pos(xy) + uv(xy)
    NkVertexLayout vlQuad;
    vlQuad.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0,              "POSITION", 0)
          .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 2*sizeof(float),"TEXCOORD", 0)
          .AddBinding(0, sizeof(VtxQuad));

    // Overlay 2D : pos(xy) + color(rgb)
    NkVertexLayout vlOverlay;
    vlOverlay.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  0,              "POSITION", 0)
             .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 2*sizeof(float),"COLOR",    0)
             .AddBinding(0, sizeof(Vtx2D));

    // ── Pipelines ─────────────────────────────────────────────────────────────
    auto makePipe = [&](NkShaderHandle sh, const NkVertexLayout& vl,
                        NkRenderPassHandle rp, bool depthTest, bool blend,
                        NkDescSetHandle dsl) -> NkPipelineHandle {
        NkGraphicsPipelineDesc pd;
        pd.shader       = sh;
        pd.vertexLayout = vl;
        pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer   = NkRasterizerDesc::Default();
        if (depthTest) pd.depthStencil = NkDepthStencilDesc::Default();
        else           pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.blend        = blend ? NkBlendDesc::Alpha() : NkBlendDesc::Opaque();
        pd.renderPass   = rp;
        if (dsl.IsValid()) pd.descriptorSetLayouts.PushBack(dsl);
        return device->CreateGraphicsPipeline(pd);
    };

    NkPipelineHandle hPipeCube    = makePipe(hShCube,    vlCube,    hRPOff,  true,  false, hDSLA);
    NkPipelineHandle hPipeQuad    = makePipe(hShQuad,    vlQuad,    hRPSwap, false, false, hDSLB);
    NkPipelineHandle hPipeOverlay = makePipe(hShOverlay, vlOverlay, hRPSwap, false, true,  hDSLA);
    // Pipeline image background : quad dans le pass offscreen (avant le cube)
    NkPipelineHandle hPipeImgBg;
    if (hImgTex.IsValid())
        hPipeImgBg = makePipe(hShQuad, vlQuad, hRPOff, false, false, hDSLB);

    // ── Résoudre texture offscreen SW ────────────────────────────────────────
    NkSWTexture* swOffTex = nullptr;
    NkSWTexture* swImgTex = nullptr;
    if (isSW) {
        auto* swDev = static_cast<NkSoftwareDevice*>(device);
        auto* fbo   = swDev->GetFBO(hFBOff.id);
        if (fbo) swOffTex = swDev->GetTex(fbo->colorId);
        if (hImgTex.IsValid()) swImgTex = swDev->GetTex(hImgTex.id);
    }

    // ── Command buffer ────────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);

    bool running     = true;
    bool wantsToSave = false;
    float angleY=0, angleX=0;

    // ── Gestion événements ────────────────────────────────────────────────────
    NkEventSystem& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
        if (e->GetKey() == NkKey::NK_S) wantsToSave = true;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = e->GetWidth(); H = e->GetHeight();
    });

    NkChrono chrono;
    (void)chrono.Reset();
    float lastTime = 0;

    // ── Boucle principale ─────────────────────────────────────────────────────
    while (running) {
        events.PollEvents();
        if (!running) break;

        if (W == 0 || H == 0) continue;
        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);

            hRPSwap = device->GetSwapchainRenderPass();
            hFBSwap = device->GetSwapchainFramebuffer();
            BuildOverlay((float)W,(float)H);
            device->WriteBuffer(hVBOverlay, kOverlayVerts, sizeof(kOverlayVerts), 0);
        }

        float now = (float)chrono.Elapsed().seconds;
        float dt  = now - lastTime; lastTime = now;
        angleY += dt * 0.8f;
        angleX += dt * 0.35f;

        NkFrameContext frame;
        device->BeginFrame(frame);

        // ── Cube UBO ─────────────────────────────────────────────────────────
        {
            CubeUbo ub;
            float mY[16],mX[16]; Mat4RotY(mY,angleY); Mat4RotX(mX,angleX);
            Mat4Mul(ub.model,mY,mX);
            Mat4LookAt(ub.view, 0,1.5f,4.5f, 0,0,0);
            Mat4Perspective(ub.proj, NkToRadians(45.f), (float)OW/(float)OH, .1f, 100.f);
            device->WriteBuffer(hUBOCube, &ub, sizeof(ub), 0);
        }
        // ── Quad UBO (SW : pointer texture) ─────────────────────────────────
        if (isSW) {
            QuadUbo ub; ub.srcTex = swOffTex;
            device->WriteBuffer(hUBOQuad, &ub, sizeof(ub), 0);
            // UBO image background SW : pointer vers la texture image chargée
            if (swImgTex) {
                QuadUbo ubImg; ubImg.srcTex = swImgTex;
                device->WriteBuffer(hUBOImgBg, &ubImg, sizeof(ubImg), 0);
            }
        }
        // ── Ortho UBO ────────────────────────────────────────────────────────
        {
            OrthoUbo ub; Mat4Ortho(ub.ortho, 0,(float)W, 0,(float)H, -1,1);
            device->WriteBuffer(hUBOOrtho, &ub, sizeof(ub), 0);
        }

        cmd->Begin();

        // === PASS 1 : cube → texture offscreen ===============================
        {
            NkRect2D area{0,0,(int32)OW,(int32)OH};
            cmd->BeginRenderPass(hRPOff, hFBOff, area);
            NkViewport vp{0.f,0.f,(float)OW,(float)OH,0.f,1.f};
            cmd->SetViewport(vp); cmd->SetScissor(area);

            // Background image (si une image a été chargée via --texture=)
            if (hPipeImgBg.IsValid() && hImgTex.IsValid()) {
                cmd->BindGraphicsPipeline(hPipeImgBg);
                cmd->BindDescriptorSet(hDSImgBg, 0, nullptr, 0);
                uint64 off2=0; cmd->BindVertexBuffer(0, hVBQuad, off2);
                cmd->Draw(6, 1, 0, 0);
            }

            cmd->BindGraphicsPipeline(hPipeCube);
            cmd->BindDescriptorSet(hDSCube, 0, nullptr, 0);
            uint64 off=0; cmd->BindVertexBuffer(0, hVBCube, off);
            cmd->Draw(kCubeCount, 1, 0, 0);
            cmd->EndRenderPass();
        }

        // Barrier : offscreen → lecture par fragment shader
        if (!isSW) {
            NkTextureBarrier b{};
            b.texture=hOffColor;
            b.stateBefore=NkResourceState::NK_RENDER_TARGET;
            b.stateAfter =NkResourceState::NK_SHADER_READ;
            b.srcStage   =NkPipelineStage::NK_COLOR_ATTACHMENT;
            b.dstStage   =NkPipelineStage::NK_FRAGMENT_SHADER;
            cmd->Barrier(nullptr,0,&b,1);
        }

        // === PASS 2 : swapchain — quad + overlay =============================
        {
            NkRect2D area{0,0,(int32)W,(int32)H};
            cmd->BeginRenderPass(hRPSwap, hFBSwap, area);
            NkViewport vp{0.f,0.f,(float)W,(float)H,0.f,1.f};
            cmd->SetViewport(vp); cmd->SetScissor(area);

            // Fullscreen quad (texture offscreen)
            cmd->BindGraphicsPipeline(hPipeQuad);
            cmd->BindDescriptorSet(hDSQuad, 0, nullptr, 0);
            uint64 off=0; cmd->BindVertexBuffer(0, hVBQuad, off);
            cmd->Draw(6, 1, 0, 0);

            // Overlay 2D
            cmd->BindGraphicsPipeline(hPipeOverlay);
            cmd->BindDescriptorSet(hDSOrtho, 0, nullptr, 0);
            cmd->BindVertexBuffer(0, hVBOverlay, off);
            cmd->Draw(kOverlayCount, 1, 0, 0);

            cmd->EndRenderPass();
        }

        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        // ── Sauvegarde PNG ────────────────────────────────────────────────────
        if (wantsToSave) {
            wantsToSave = false;
            const uint8* pixels = nullptr;
            uint32 pw = W, ph = H;
            if (isSW) {
                auto* sw = static_cast<NkSoftwareDevice*>(device);
                pixels = sw->BackbufferPixels();
                pw = sw->BackbufferWidth(); ph = sw->BackbufferHeight();
            }
            if (pixels) {
                NkImage* img = NkImage::Wrap(const_cast<uint8*>(pixels), (int32)pw, (int32)ph, NkImagePixelFormat::NK_RGBA32);
                if (img) {
                    bool ok = img->SavePNG("screenshot.png");
                    logger_src.Infof("[ImageSave] SavePNG : %s\n", ok?"OK":"ECHEC");
                    img->Free();
                }
            } else {
                logger_src.Infof("[ImageSave] Readback non implemente pour ce backend.\n");
            }
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    device->WaitIdle();
    device->DestroyCommandBuffer(cmd);
    device->DestroyBuffer(hVBCube); device->DestroyBuffer(hVBQuad);
    device->DestroyBuffer(hVBOverlay);
    device->DestroyBuffer(hUBOCube); device->DestroyBuffer(hUBOQuad); device->DestroyBuffer(hUBOOrtho);
    device->DestroyBuffer(hUBOImgBg);
    device->DestroyShader(hShCube); device->DestroyShader(hShQuad); device->DestroyShader(hShOverlay);
    device->DestroyPipeline(hPipeCube); device->DestroyPipeline(hPipeQuad); device->DestroyPipeline(hPipeOverlay);
    if (hPipeImgBg.IsValid()) device->DestroyPipeline(hPipeImgBg);
    device->DestroyFramebuffer(hFBOff); device->DestroyRenderPass(hRPOff);
    device->DestroyTexture(hOffColor); device->DestroyTexture(hOffDepth);
    if (hImgTex.IsValid())    device->DestroyTexture(hImgTex);
    if (hSampler.IsValid())   device->DestroySampler(hSampler);
    if (hSamplerImg.IsValid()) device->DestroySampler(hSamplerImg);
    device->FreeDescriptorSet(hDSCube); device->FreeDescriptorSet(hDSOrtho);
    device->FreeDescriptorSet(hDSQuad); device->FreeDescriptorSet(hDSImgBg);
    device->DestroyDescriptorSetLayout(hDSLA); device->DestroyDescriptorSetLayout(hDSLB);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}
