// =============================================================================
// NkRHIInterpTest.cpp
// Test minimal INCREMENTAL de l'interpolation des varyings via NKRHI, sur les 4
// backends (gl/vk/dx11/dx12). Reproduit le bug DX12 "triangle plein écran =
// varyings plats" en isolation, sans le bruit de renderdemo (327 draws + PP).
//
//   -t0  CLEAR : clear bleu, readback → baseline present.
//   -t1  GÉOMÉTRIE INTERPOLÉE : triangle VBO (3 sommets R/G/B), couleur interpolée.
//   -t2  PLEIN ÉCRAN INTERPOLÉ (clé) : triangle gl_VertexID SANS VBO, vUV → fragColor=(vUV,0,1).
//   -t3  OFFSCREEN→FULLSCREEN SAMPLE : T1 dans un RT offscreen, puis triangle plein
//        écran qui échantillonne ce RT (sampler2D).
//   Backend : -bgl / -bvk / -bdx11 / -bdx12.
//
// Chaque test rend dans un RT OFFSCREEN RGBA8 (256x256), readback CPU (CopyTextureToBuffer
// + ReadBuffer), et logge 5 pixels (TL/TR/BL/BR/centre) → DÉGRADÉ ou APLAT.
// Le résultat est aussi blitté/présenté à l'écran (clear swapchain) pour vie.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKTime/NkTime.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKSL/NKSL.h"

#include <cstring>
#include <cstdint>

using namespace nkentseu;

namespace nkentseu { struct NkEntryState; }

// =============================================================================
// Shaders — GLSL (GL + VK via runtime glslang) et HLSL (DX11/DX12).
// =============================================================================

// ── T1 : triangle géométrique avec couleur par sommet, interpolée ────────────
static const char* kGLSL_GeomVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=0) out vec3 vColor;
void main(){ vColor=aColor; gl_Position=vec4(aPos,1.0); }
)";
static const char* kGLSL_GeomFS = R"(#version 460 core
layout(location=0) in vec3 vColor;
layout(location=0) out vec4 oColor;
void main(){ oColor=vec4(vColor,1.0); }
)";
static const char* kHLSL_GeomVS = R"(
struct VSIn { float3 aPos:POSITION; float3 aColor:COLOR; };
struct VSOut{ float4 pos:SV_Position; float3 vColor:TEXCOORD0; };
VSOut VSMain(VSIn i){ VSOut o; o.pos=float4(i.aPos,1.0); o.vColor=i.aColor; return o; }
)";
static const char* kHLSL_GeomPS = R"(
struct VSOut{ float4 pos:SV_Position; float3 vColor:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target { return float4(i.vColor,1.0); }
)";

// ── T2 : triangle plein écran via gl_VertexID, SANS VBO. fragColor=(vUV,0,1) ──
static const char* kGLSL_FsVS = R"(#version 460 core
layout(location=0) out vec2 vUV;
void main(){
    vec2 p = vec2((gl_VertexIndex==1)?3.0:-1.0, (gl_VertexIndex==2)?3.0:-1.0);
    vUV = p*0.5+0.5;
    gl_Position = vec4(p,0.0,1.0);
}
)";
static const char* kGLSL_FsFS = R"(#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
void main(){ oColor=vec4(vUV,0.0,1.0); }
)";
static const char* kHLSL_FsVS = R"(
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
VSOut VSMain(uint vid:SV_VertexID){
    VSOut o;
    float2 p = float2((vid==1)?3.0:-1.0, (vid==2)?3.0:-1.0);
    o.vUV = p*0.5+0.5;
    o.pos = float4(p,0.0,1.0);
    return o;
}
)";
static const char* kHLSL_FsPS = R"(
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target { return float4(i.vUV,0.0,1.0); }
)";

// ── Morph T2v* : on rapproche le HLSL plein écran de la STRUCTURE EXACTE générée
//    par NkSL (cf. logs NkSL-DUMP demo2/dx12). Entry = "main". On change UNIQUEMENT
//    la forme HLSL, le corps reste fragColor=(vuv,0,1) pour isoler l'interpolation.

// v1 : VSOut/PSIn anonymes + SV_IsFrontFace ajouté APRÈS la varying (suspect #1).
static const char* kHLSL_v1VS = kHLSL_FsVS; // VS inchangé
static const char* kHLSL_v1PS = R"(
struct PSIn{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; bool isFront:SV_IsFrontFace; };
float4 PSMain(PSIn i):SV_Target { return float4(i.vUV,0.0,1.0); }
)";

// v2 : structs NOMMÉS NkInput/NkOutput + SV_InstanceID au VS, comme le générateur.
static const char* kHLSL_v2VS = R"(
struct NkInput { uint _VertexID:SV_VertexID; uint _InstanceID:SV_InstanceID; };
struct NkOutput{ float4 _Position:SV_Position; float2 vuv:TEXCOORD0; };
NkOutput VSMain(NkInput input){
    NkOutput output;
    float2 pos = float2((input._VertexID==1)?3.0:-1.0,(input._VertexID==2)?3.0:-1.0);
    output._Position = float4(pos,0.0,1.0);
    output.vuv = pos*0.5+0.5;
    return output;
}
)";
static const char* kHLSL_v2PS = R"(
struct NkInput{ float4 _Position:SV_Position; float2 vuv:TEXCOORD0; bool IsFrontFace:SV_IsFrontFace; };
struct NkOutput{ float4 ocolor:SV_Target0; };
NkOutput PSMain(NkInput input){
    NkOutput output;
    output.ocolor = float4(input.vuv,0.0,1.0);
    return output;
}
)";

// v3 : v2 + #pragma pack_matrix + helpers + ConstantBuffer<PC> b0/space1 (yflip),
//      EXACTEMENT le préambule du générateur. Entry = "main".
static const char* kHLSL_v3VS = R"(
#pragma pack_matrix(column_major)
float2 nksl_texsize2d(Texture2D t) { uint w,h; t.GetDimensions(w,h); return float2(w,h); }
struct PC { float4 p0; float4 p1; float4 p2; };
ConstantBuffer<PC> pc : register(b0, space1);
struct NkInput { uint _VertexID:SV_VertexID; uint _InstanceID:SV_InstanceID; };
struct NkOutput{ float4 _Position:SV_Position; float2 vuv:TEXCOORD0; };
NkOutput main(NkInput input){
    NkOutput output;
    float2 pos = float2(((input._VertexID==1)?3.0f:-1.0f),((input._VertexID==2)?3.0f:-1.0f));
    output._Position = float4(pos,0.0f,1.0f);
    float2 uvbase = ((pos*0.5f)+0.5f);
    float yflipuv = pc.p1.z;
    output.vuv = ((yflipuv<0.0f) ? float2(uvbase.x,(1.0f-uvbase.y)) : uvbase);
    return output;
}
)";
static const char* kHLSL_v3PS = R"(
#pragma pack_matrix(column_major)
float2 nksl_texsize2d(Texture2D t) { uint w,h; t.GetDimensions(w,h); return float2(w,h); }
struct PC { float4 p0; float4 p1; float4 p2; };
ConstantBuffer<PC> pc : register(b0, space1);
struct NkInput{ float4 _Position:SV_Position; float2 vuv:TEXCOORD0; bool IsFrontFace:SV_IsFrontFace; };
struct NkOutput{ float4 ocolor:SV_Target0; };
NkOutput main(NkInput input){
    NkOutput output;
    output.ocolor = float4(input.vuv,0.0f,1.0f);
    return output;
}
)";

// ── T3 : plein écran qui ÉCHANTILLONNE une texture (sampler2D) ───────────────
static const char* kGLSL_SampVS = kGLSL_FsVS; // même VS plein écran
static const char* kGLSL_SampFS = R"(#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uTex;
void main(){ oColor=vec4(texture(uTex,vUV).rgb,1.0); }
)";
static const char* kHLSL_SampVS = kHLSL_FsVS;
static const char* kHLSL_SampPS = R"(
Texture2D uTex : register(t0);
SamplerState uSmp : register(s0);
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target { return float4(uTex.Sample(uSmp,i.vUV).rgb,1.0); }
)";

// ── T4 : 4 SRV dans UN set (pattern mToneSet). Échantillonne SLOT 0 uniquement. ──
static const char* kGLSL_Multi4VS = kGLSL_FsVS;
static const char* kGLSL_Multi4FS_slot0 = R"(#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uTex0;
layout(binding=1) uniform sampler2D uTex1;
layout(binding=2) uniform sampler2D uTex2;
layout(binding=3) uniform sampler2D uTex3;
void main(){ oColor=vec4(texture(uTex0,vUV).rgb,1.0); }
)";
static const char* kHLSL_Multi4VS = kHLSL_FsVS;
static const char* kHLSL_Multi4PS_slot0 = R"(
Texture2D uTex0:register(t0); SamplerState uSmp0:register(s0);
Texture2D uTex1:register(t1); SamplerState uSmp1:register(s1);
Texture2D uTex2:register(t2); SamplerState uSmp2:register(s2);
Texture2D uTex3:register(t3); SamplerState uSmp3:register(s3);
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target { return float4(uTex0.Sample(uSmp0,i.vUV).rgb,1.0); }
)";

// ── T4b : échantillonne les 4 et combine (vérifie que CHAQUE slot livre sa couleur) ──
static const char* kGLSL_Multi4FS_comb = R"(#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uTex0;
layout(binding=1) uniform sampler2D uTex1;
layout(binding=2) uniform sampler2D uTex2;
layout(binding=3) uniform sampler2D uTex3;
void main(){
    vec3 c = texture(uTex0,vUV).rgb*0.25 + texture(uTex1,vUV).rgb*0.25
           + texture(uTex2,vUV).rgb*0.25 + texture(uTex3,vUV).rgb*0.25;
    oColor=vec4(c,1.0);
}
)";
static const char* kHLSL_Multi4PS_comb = R"(
Texture2D uTex0:register(t0); SamplerState uSmp0:register(s0);
Texture2D uTex1:register(t1); SamplerState uSmp1:register(s1);
Texture2D uTex2:register(t2); SamplerState uSmp2:register(s2);
Texture2D uTex3:register(t3); SamplerState uSmp3:register(s3);
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target {
    float3 c = uTex0.Sample(uSmp0,i.vUV).rgb*0.25 + uTex1.Sample(uSmp1,i.vUV).rgb*0.25
             + uTex2.Sample(uSmp2,i.vUV).rgb*0.25 + uTex3.Sample(uSmp3,i.vUV).rgb*0.25;
    return float4(c,1.0);
}
)";

// ── T7 : SIGNATURE EXACTE DU TONEMAP : 4 SRV (set) + push constant 48o (b0,space1)
//    + le VS lit pc.p1.z (yFlip) ET le FS lit pc.p0.x (exposure). Tous deux dans le
//    MÊME draw. C'est la combinaison push-constant root + table-SRV que le vrai
//    tonemap fait et que T4/T4b seuls ne couvrent pas.
static const char* kGLSL_T7VS = R"(#version 460 core
layout(location=0) out vec2 vUV;
layout(push_constant) uniform PC { vec4 p0; vec4 p1; vec4 p2; } pc;
void main(){
    vec2 p = vec2((gl_VertexIndex==1)?3.0:-1.0,(gl_VertexIndex==2)?3.0:-1.0);
    vec2 uvbase = p*0.5+0.5;
    float yflip = pc.p1.z;
    vUV = (yflip<0.0) ? vec2(uvbase.x, 1.0-uvbase.y) : uvbase;
    gl_Position = vec4(p,0.0,1.0);
}
)";
static const char* kGLSL_T7FS = R"(#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uTex0;
layout(binding=1) uniform sampler2D uTex1;
layout(binding=2) uniform sampler2D uTex2;
layout(binding=3) uniform sampler2D uTex3;
layout(push_constant) uniform PC { vec4 p0; vec4 p1; vec4 p2; } pc;
void main(){
    float expo = pc.p0.x;
    vec3 c = texture(uTex0,vUV).rgb*0.25 + texture(uTex1,vUV).rgb*0.25
           + texture(uTex2,vUV).rgb*0.25 + texture(uTex3,vUV).rgb*0.25;
    oColor = vec4(c*expo, 1.0);
}
)";
static const char* kHLSL_T7VS = R"(
struct PC { float4 p0; float4 p1; float4 p2; };
ConstantBuffer<PC> pc : register(b0, space1);
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
VSOut VSMain(uint vid:SV_VertexID){
    VSOut o;
    float2 p = float2((vid==1)?3.0:-1.0,(vid==2)?3.0:-1.0);
    float2 uvbase = p*0.5+0.5;
    float yflip = pc.p1.z;
    o.vUV = (yflip<0.0) ? float2(uvbase.x, 1.0-uvbase.y) : uvbase;
    o.pos = float4(p,0.0,1.0);
    return o;
}
)";
static const char* kHLSL_T7PS = R"(
Texture2D uTex0:register(t0); SamplerState uSmp0:register(s0);
Texture2D uTex1:register(t1); SamplerState uSmp1:register(s1);
Texture2D uTex2:register(t2); SamplerState uSmp2:register(s2);
Texture2D uTex3:register(t3); SamplerState uSmp3:register(s3);
struct PC { float4 p0; float4 p1; float4 p2; };
ConstantBuffer<PC> pc : register(b0, space1);
struct VSOut{ float4 pos:SV_Position; float2 vUV:TEXCOORD0; };
float4 PSMain(VSOut i):SV_Target {
    float expo = pc.p0.x;
    float3 c = uTex0.Sample(uSmp0,i.vUV).rgb*0.25 + uTex1.Sample(uSmp1,i.vUV).rgb*0.25
             + uTex2.Sample(uSmp2,i.vUV).rgb*0.25 + uTex3.Sample(uSmp3,i.vUV).rgb*0.25;
    return float4(c*expo,1.0);
}
)";

// =============================================================================
struct Vtx { float x,y,z; float r,g,b; };

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args){
    for (const auto& a: args){
        if (a=="-bvk"||a=="--backend=vulkan") return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (a=="-bdx11"||a=="--backend=dx11") return NkGraphicsApi::NK_GFX_API_DX11;
        if (a=="-bdx12"||a=="--backend=dx12") return NkGraphicsApi::NK_GFX_API_DX12;
        if (a=="-bgl"||a=="--backend=opengl") return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}
static int ParseTest(const NkVector<NkString>& args){
    for (const auto& a: args){
        if (a=="-t0") return 0; if (a=="-t1") return 1;
        if (a=="-t2") return 2; if (a=="-t3") return 3;
        if (a=="-t4b") return 5;             // 4 SRV : combine les 4
        if (a=="-t4") return 4;              // 4 SRV : sample slot 0
        if (a=="-t5") return 6;              // chaîne 3 passes (RT transients enchaînés)
        if (a=="-t6") return 7;              // chaîne + HDR transient RGBA16F
        if (a=="-t7") return 8;              // signature tonemap : 4 SRV set + push const b0/space1
    }
    return 2; // défaut = le test clé
}
// Variante de morphing du shader plein écran T2 (0=baseline qui marche). -v1/-v2/-v3.
static int ParseVariant(const NkVector<NkString>& args){
    for (const auto& a: args){
        if (a=="-v0") return 0; if (a=="-v1") return 1;
        if (a=="-v2") return 2; if (a=="-v3") return 3;
    }
    return 0;
}
static bool IsDX(NkGraphicsApi a){ return a==NkGraphicsApi::NK_GFX_API_DX11||a==NkGraphicsApi::NK_GFX_API_DX12; }
static bool IsVK(NkGraphicsApi a){ return a==NkGraphicsApi::NK_GFX_API_VULKAN; }

// Construit un NkShaderDesc cross-backend depuis GLSL (GL+VK) ou HLSL (DX).
static NkShaderHandle MakeShader(NkIDevice* dev, NkGraphicsApi api, const char* dbg,
                                 const char* glslVS, const char* glslFS,
                                 const char* hlslVS, const char* hlslFS,
                                 const char* hlslVSEntry="VSMain",
                                 const char* hlslPSEntry="PSMain"){
    NkShaderDesc sd; sd.debugName = dbg;
    if (IsDX(api)) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   hlslVS, hlslVSEntry);
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, hlslFS, hlslPSEntry);
    } else if (IsVK(api)) {
        // GLSL→SPIRV runtime via glslang
        auto vr = NkShaderConverter::GlslToSpirv(NkString(glslVS), NkSLStage::NK_VERTEX,   dbg);
        auto fr = NkShaderConverter::GlslToSpirv(NkString(glslFS), NkSLStage::NK_FRAGMENT, dbg);
        if (!vr.success || !fr.success){ logger.Info("[InterpTest] SPIRV compile FAIL ({0})\n", dbg); return {}; }
        sd.AddSPIRV(NkShaderStage::NK_VERTEX,   vr.SpirvWords(), vr.SpirvWordCount()*sizeof(uint32));
        sd.AddSPIRV(NkShaderStage::NK_FRAGMENT, fr.SpirvWords(), fr.SpirvWordCount()*sizeof(uint32));
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   glslVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, glslFS);
    }
    return dev->CreateShader(sd);
}

// Crée une texture RGBA8 256x256 remplie d'une couleur unie (SHADER_RESOURCE).
static NkTextureHandle MakeSolidTex(NkIDevice* dev, uint32 w, uint32 h,
                                    uint8 r, uint8 g, uint8 b, const char* dbg){
    NkTextureDesc td = NkTextureDesc::Tex2D(w, h, NkGPUFormat::NK_RGBA8_UNORM, 1);
    td.bindFlags=NkBindFlags::NK_SHADER_RESOURCE;
    td.debugName=dbg;
    NkTextureHandle t = dev->CreateTexture(td);
    if (!t.IsValid()) return t;
    NkVector<uint8> px; px.Resize(w*h*4);
    for (uint32 i=0;i<w*h;i++){ px[i*4+0]=r; px[i*4+1]=g; px[i*4+2]=b; px[i*4+3]=255; }
    dev->WriteTextureRegion(t, px.Data(), 0,0,0, w,h,1, 0,0, w*4);
    return t;
}

// Readback d'un RT offscreen RGBA8 → log de 5 pixels.
static void ReadbackAndLog(NkIDevice* dev, NkICommandBuffer* cmd, NkTextureHandle rt,
                           uint32 w, uint32 h, const char* tag, const char* apiName){
    // buffer staging readback (RGBA8 sans padding)
    uint64 sz = (uint64)w*h*4;
    NkBufferDesc bd = NkBufferDesc::Staging(sz);
    bd.usage = NkResourceUsage::NK_READBACK;
    NkBufferHandle rb = dev->CreateBuffer(bd);
    if (!rb.IsValid()){ logger.Info("[InterpTest] readback buffer FAIL\n"); return; }

    // copie RT -> buffer via une command list dédiée
    cmd->Reset(); cmd->Begin();
    NkBufferTextureCopyRegion r{};
    r.bufferOffset=0; r.bufferRowPitch=w*4; r.width=w; r.height=h; r.depth=1;
    r.x=0; r.y=0; r.z=0; r.mipLevel=0; r.arrayLayer=0;
    cmd->CopyTextureToBuffer(rt, rb, r);
    cmd->End();
    dev->Submit(&cmd, 1, {});
    dev->WaitIdle();

    NkVector<uint8> px; px.Resize((uint32)sz);
    if (!dev->ReadBuffer(rb, px.Data(), sz, 0)){ logger.Info("[InterpTest] ReadBuffer FAIL\n"); dev->DestroyBuffer(rb); return; }

    auto P=[&](uint32 x,uint32 y){ const uint8* p=&px[(y*w+x)*4]; return NkFormat("({0},{1},{2})",(int)p[0],(int)p[1],(int)p[2]); };
    uint32 cx=w/2, cy=h/2;
    logger.Info("[InterpTest] {0} {1}: TL={2} TR={3} BL={4} BR={5} C={6}\n",
                tag, apiName,
                P(2,2).CStr(), P(w-3,2).CStr(), P(2,h-3).CStr(), P(w-3,h-3).CStr(), P(cx,cy).CStr());
    dev->DestroyBuffer(rb);
}

// =============================================================================
int nkmain(const NkEntryState& state) {
    NkShaderCache::Global().SetCacheDir("Build/ShaderCache");
    NkGraphicsApi api = ParseBackend(state.GetArgs());
    int test = ParseTest(state.GetArgs());
    int variant = ParseVariant(state.GetArgs());
    const char* apiName = NkGraphicsApiName(api);
    logger.Info("[InterpTest] === T{0}v{1} backend={2} ===\n", test, variant, apiName);

    NkWindowConfig wc; wc.title=NkFormat("InterpTest T{0} {1}", test, apiName);
    wc.width=512; wc.height=512; wc.centered=true; wc.resizable=false;
    NkWindow window;
    if (!window.Create(wc)){ logger.Info("[InterpTest] window FAIL\n"); return 1; }

    NkDeviceInitInfo ii; ii.api=api; ii.surface=window.GetSurfaceDesc();
    ii.width=window.GetSize().width; ii.height=window.GetSize().height;
    ii.context.vulkan.validationLayers=false; ii.context.vulkan.debugMessenger=false;
    ii.context.vulkan.srgbSwapchain=false;
    NkIDevice* dev = NkDeviceFactory::Create(ii);
    if (!dev||!dev->IsValid()){ logger.Info("[InterpTest] device FAIL\n"); window.Close(); return 1; }

    const uint32 RTW=256, RTH=256;

    // ── RT offscreen RGBA8 (cible de tous les tests, pour readback) ──────────
    NkTextureDesc rtd = NkTextureDesc::RenderTarget(RTW, RTH, NkGPUFormat::NK_RGBA8_UNORM);
    NkTextureHandle hRT = dev->CreateTexture(rtd);
    // depth pour le RT (les pipelines DepthStencil::Default exigent un depth attach)
    NkTextureDesc dtd = NkTextureDesc::DepthStencil(RTW, RTH, NkGPUFormat::NK_D32_FLOAT);
    NkTextureHandle hRTDepth = dev->CreateTexture(dtd);
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_UNORM)).SetDepth(NkAttachmentDesc::Depth());
    NkRenderPassHandle hRTRP = dev->CreateRenderPass(rpd);
    NkFramebufferDesc fbd; fbd.renderPass=hRTRP; fbd.colorAttachments.PushBack(hRT);
    fbd.depthAttachment=hRTDepth; fbd.width=RTW; fbd.height=RTH;
    NkFramebufferHandle hRTFBO = dev->CreateFramebuffer(fbd);
    NkRenderPassHandle hRTRPeff = dev->GetFramebufferRenderPass(hRTFBO);
    if (hRTRPeff.IsValid()) hRTRP = hRTRPeff;

    NkICommandBuffer* cmd = dev->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);

    // ── T1 geometry pipeline (VBO triangle RGB) ──────────────────────────────
    NkShaderHandle hGeomSh; NkPipelineHandle hGeomPipe; NkBufferHandle hVBO;
    if (test==1 || test==3) {
        hGeomSh = MakeShader(dev, api, "geom", kGLSL_GeomVS, kGLSL_GeomFS, kHLSL_GeomVS, kHLSL_GeomPS);
        NkVertexLayout vl;
        vl.AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,0,"POSITION",0)
          .AddAttribute(1,0,NkGPUFormat::NK_RGB32_FLOAT,3*sizeof(float),"COLOR",0)
          .AddBinding(0,sizeof(Vtx));
        Vtx tri[3] = {
            { 0.0f, 0.8f,0.f, 1,0,0},   // haut = rouge
            {-0.8f,-0.8f,0.f, 0,1,0},   // bas-gauche = vert
            { 0.8f,-0.8f,0.f, 0,0,1},   // bas-droit = bleu
        };
        hVBO = dev->CreateBuffer(NkBufferDesc::Vertex(sizeof(tri), tri));
        NkGraphicsPipelineDesc pd; pd.shader=hGeomSh; pd.vertexLayout=vl;
        pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
        pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hRTRP; pd.debugName="geomPipe";
        hGeomPipe = dev->CreateGraphicsPipeline(pd);
    }

    // ── T2 fullscreen pipeline (VertexID, no VBO) ────────────────────────────
    NkShaderHandle hFsSh; NkPipelineHandle hFsPipe;
    if (test==2) {
        // Sélection de la variante de morphing HLSL (GL/VK gardent le GLSL de base).
        const char* hvs = kHLSL_FsVS;  const char* hps = kHLSL_FsPS;
        const char* eVS = "VSMain";    const char* ePS = "PSMain";
        switch (variant) {
            case 1: hvs=kHLSL_v1VS; hps=kHLSL_v1PS; break;            // +SV_IsFrontFace
            case 2: hvs=kHLSL_v2VS; hps=kHLSL_v2PS; break;            // structs nommés + SV_InstanceID
            case 3: hvs=kHLSL_v3VS; hps=kHLSL_v3PS; eVS="main"; ePS="main"; break; // exact générateur
            default: break;
        }
        logger.Info("[InterpTest] T2 variant={0} (entry VS={1} PS={2})\n", variant, eVS, ePS);
        hFsSh = MakeShader(dev, api, "fs", kGLSL_FsVS, kGLSL_FsFS, hvs, hps, eVS, ePS);
        NkGraphicsPipelineDesc pd; pd.shader=hFsSh;  // pas de vertexLayout
        pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
        pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hRTRP; pd.debugName="fsPipe";
        hFsPipe = dev->CreateGraphicsPipeline(pd);
    }

    // ── T3 sample pipeline (fullscreen sampling le RT de T1) ──────────────────
    // Pour T3 on a besoin de 2 RT : srcRT (geom) + dstRT (sample). On réutilise hRT comme
    // dst et un 2e RT comme src.
    NkTextureHandle hSrcRT; NkFramebufferHandle hSrcFBO; NkRenderPassHandle hSrcRP=hRTRP;
    NkShaderHandle hSampSh; NkPipelineHandle hSampPipe; NkSamplerHandle hSamp;
    NkDescSetHandle hSampLayout; NkDescSetHandle hSampSet;
    if (test==3) {
        NkTextureDesc srtd = NkTextureDesc::RenderTarget(RTW, RTH, NkGPUFormat::NK_RGBA8_UNORM);
        srtd.bindFlags = srtd.bindFlags | NkBindFlags::NK_SHADER_RESOURCE;
        hSrcRT = dev->CreateTexture(srtd);
        NkFramebufferDesc sfbd; sfbd.renderPass=hRTRP; sfbd.colorAttachments.PushBack(hSrcRT);
        sfbd.depthAttachment=hRTDepth; sfbd.width=RTW; sfbd.height=RTH;
        hSrcFBO = dev->CreateFramebuffer(sfbd);
        auto eff = dev->GetFramebufferRenderPass(hSrcFBO); if (eff.IsValid()) hSrcRP=eff;

        hSamp = dev->CreateSampler(NkSamplerDesc::Clamp());
        NkDescriptorSetLayoutDesc ld; ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
        hSampLayout = dev->CreateDescriptorSetLayout(ld);
        hSampSet = dev->AllocateDescriptorSet(hSampLayout);
        dev->BindTextureSampler(hSampSet, 0, hSrcRT, hSamp);

        hSampSh = MakeShader(dev, api, "samp", kGLSL_SampVS, kGLSL_SampFS, kHLSL_SampVS, kHLSL_SampPS);
        NkGraphicsPipelineDesc pd; pd.shader=hSampSh;
        pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
        pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hRTRP; pd.debugName="sampPipe";
        if (hSampLayout.IsValid()) pd.descriptorSetLayouts.PushBack(hSampLayout);
        hSampPipe = dev->CreateGraphicsPipeline(pd);
    }

    // ── T4 / T4b : 4 textures couleur distinctes dans UN descriptor set ──────
    // tex0=ROUGE, tex1=VERT, tex2=BLEU, tex3=BLANC. On les bind aux slots 0..3
    // EXACTEMENT comme NkPostProcessStack::ExecuteRHI (4× BindTextureSampler +
    // BindDescriptorSet(set,0)). T4 sample slot 0 (attendu ROUGE). T4b combine.
    NkTextureHandle hT4[4]; for (auto& h:hT4) h={};
    NkSamplerHandle hT4Samp; NkDescSetHandle hT4Layout, hT4Set;
    NkShaderHandle hT4Sh; NkPipelineHandle hT4Pipe;
    const bool t7 = (test==8);
    if (test==4 || test==5 || test==8) {
        hT4[0]=MakeSolidTex(dev,RTW,RTH,255,0,0,"t4_red");
        hT4[1]=MakeSolidTex(dev,RTW,RTH,0,255,0,"t4_green");
        hT4[2]=MakeSolidTex(dev,RTW,RTH,0,0,255,"t4_blue");
        hT4[3]=MakeSolidTex(dev,RTW,RTH,255,255,255,"t4_white");
        hT4Samp=dev->CreateSampler(NkSamplerDesc::Clamp());
        NkDescriptorSetLayoutDesc ld;
        ld.Add(0,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        ld.Add(1,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        ld.Add(2,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        ld.Add(3,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        hT4Layout=dev->CreateDescriptorSetLayout(ld);
        hT4Set=dev->AllocateDescriptorSet(hT4Layout);
        // Pattern mToneSet : bind les 4 textures aux slots 0..3.
        dev->BindTextureSampler(hT4Set,0,hT4[0],hT4Samp);
        dev->BindTextureSampler(hT4Set,1,hT4[1],hT4Samp);
        dev->BindTextureSampler(hT4Set,2,hT4[2],hT4Samp);
        dev->BindTextureSampler(hT4Set,3,hT4[3],hT4Samp);
        const char* gvs = t7?kGLSL_T7VS:kGLSL_Multi4VS;
        const char* hvs = t7?kHLSL_T7VS:kHLSL_Multi4VS;
        const char* gfs = t7?kGLSL_T7FS:((test==4)?kGLSL_Multi4FS_slot0:kGLSL_Multi4FS_comb);
        const char* hfs = t7?kHLSL_T7PS:((test==4)?kHLSL_Multi4PS_slot0:kHLSL_Multi4PS_comb);
        hT4Sh=MakeShader(dev,api,"multi4",gvs,gfs,hvs,hfs);
        NkGraphicsPipelineDesc pd; pd.shader=hT4Sh;
        pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
        pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hRTRP; pd.debugName="multi4Pipe";
        if (hT4Layout.IsValid()) pd.descriptorSetLayouts.PushBack(hT4Layout);
        if (t7) pd.AddPushConstant(NkShaderStage::NK_ALL_GRAPHICS, 0, 48); // p0,p1,p2
        hT4Pipe=dev->CreateGraphicsPipeline(pd);
    }

    // ── T5 / T6 : CHAÎNE 3 PASSES (transients enchaînés, état RT->SRV entre passes) ──
    // passe1 : geom triangle RGB -> RT_A (T6 : RT_A = RGBA16F comme le vrai HDR)
    // passe2 : plein écran sample RT_A -> RT_B (RGBA8)
    // passe3 : plein écran sample RT_B -> hRT (readback)
    // Reproduit SSAO->bloom->tonemap->composite (RT intermédiaires + barriers).
    NkTextureHandle hChA, hChB; NkFramebufferHandle hChAFBO, hChBFBO;
    NkRenderPassHandle hChARP=hRTRP, hChBRP=hRTRP;
    NkSamplerHandle hChSamp; NkDescSetHandle hChLayout, hChSetA, hChSetB;
    NkShaderHandle hChGeomSh, hChSampSh; NkPipelineHandle hChGeomPipe, hChSampPipeA, hChSampPipeB;
    NkBufferHandle hChVBO;
    if (test==6 || test==7) {
        const bool hdr16 = (test==7);
        const NkGPUFormat fmtA = hdr16 ? NkGPUFormat::NK_RGBA16_FLOAT : NkGPUFormat::NK_RGBA8_UNORM;
        // RT_A (+ depth réutilisé hRTDepth) — son propre render pass (format A)
        NkTextureDesc tad = NkTextureDesc::RenderTarget(RTW, RTH, fmtA);
        tad.bindFlags = tad.bindFlags | NkBindFlags::NK_SHADER_RESOURCE; tad.debugName="chainA";
        hChA = dev->CreateTexture(tad);
        NkRenderPassDesc rpA; rpA.AddColor(NkAttachmentDesc::Color(fmtA)).SetDepth(NkAttachmentDesc::Depth());
        hChARP = dev->CreateRenderPass(rpA);
        NkFramebufferDesc fbA; fbA.renderPass=hChARP; fbA.colorAttachments.PushBack(hChA);
        fbA.depthAttachment=hRTDepth; fbA.width=RTW; fbA.height=RTH;
        hChAFBO = dev->CreateFramebuffer(fbA);
        { auto e=dev->GetFramebufferRenderPass(hChAFBO); if (e.IsValid()) hChARP=e; }
        // RT_B (RGBA8) — render pass = hRTRP (même format que hRT)
        NkTextureDesc tbd = NkTextureDesc::RenderTarget(RTW, RTH, NkGPUFormat::NK_RGBA8_UNORM);
        tbd.bindFlags = tbd.bindFlags | NkBindFlags::NK_SHADER_RESOURCE; tbd.debugName="chainB";
        hChB = dev->CreateTexture(tbd);
        NkFramebufferDesc fbB; fbB.renderPass=hRTRP; fbB.colorAttachments.PushBack(hChB);
        fbB.depthAttachment=hRTDepth; fbB.width=RTW; fbB.height=RTH;
        hChBFBO = dev->CreateFramebuffer(fbB);
        { auto e=dev->GetFramebufferRenderPass(hChBFBO); if (e.IsValid()) hChBRP=e; }

        hChSamp = dev->CreateSampler(NkSamplerDesc::Clamp());
        NkDescriptorSetLayoutDesc ld; ld.Add(0,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        hChLayout = dev->CreateDescriptorSetLayout(ld);
        hChSetA = dev->AllocateDescriptorSet(hChLayout); // sample RT_A
        hChSetB = dev->AllocateDescriptorSet(hChLayout); // sample RT_B
        dev->BindTextureSampler(hChSetA, 0, hChA, hChSamp);
        dev->BindTextureSampler(hChSetB, 0, hChB, hChSamp);

        // geom pipeline (RT_A)
        hChGeomSh = MakeShader(dev, api, "chgeom", kGLSL_GeomVS, kGLSL_GeomFS, kHLSL_GeomVS, kHLSL_GeomPS);
        NkVertexLayout vl;
        vl.AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,0,"POSITION",0)
          .AddAttribute(1,0,NkGPUFormat::NK_RGB32_FLOAT,3*sizeof(float),"COLOR",0)
          .AddBinding(0,sizeof(Vtx));
        Vtx tri[3] = { {0.0f,0.8f,0.f,1,0,0}, {-0.8f,-0.8f,0.f,0,1,0}, {0.8f,-0.8f,0.f,0,0,1} };
        hChVBO = dev->CreateBuffer(NkBufferDesc::Vertex(sizeof(tri), tri));
        { NkGraphicsPipelineDesc pd; pd.shader=hChGeomSh; pd.vertexLayout=vl;
          pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
          pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
          pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hChARP; pd.debugName="chGeomPipe";
          hChGeomPipe = dev->CreateGraphicsPipeline(pd); }
        // sample pipeline (réutilisé pour passe2 RT_A->RT_B et passe3 RT_B->hRT)
        hChSampSh = MakeShader(dev, api, "chsamp", kGLSL_SampVS, kGLSL_SampFS, kHLSL_SampVS, kHLSL_SampPS);
        { NkGraphicsPipelineDesc pd; pd.shader=hChSampSh;
          pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
          pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
          pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hChBRP; pd.debugName="chSampPipeB";
          if (hChLayout.IsValid()) pd.descriptorSetLayouts.PushBack(hChLayout);
          hChSampPipeB = dev->CreateGraphicsPipeline(pd); }
        { NkGraphicsPipelineDesc pd; pd.shader=hChSampSh;
          pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
          pd.rasterizer=NkRasterizerDesc::NoCull(); pd.depthStencil=NkDepthStencilDesc::NoDepth();
          pd.blend=NkBlendDesc::Opaque(); pd.renderPass=hRTRP; pd.debugName="chSampPipeRT";
          if (hChLayout.IsValid()) pd.descriptorSetLayouts.PushBack(hChLayout);
          hChSampPipeA = dev->CreateGraphicsPipeline(pd); }
    }

    NkEventSystem& events = NkEvents();
    bool running=true;
    bool readbackDone=false;
    int frameCount=0;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*){ running=false; });

    while (running) {
        events.PollEvents();
        if (!running) break;

        NkFrameContext frame;
        if (!dev->BeginFrame(frame)) continue;

        cmd->Reset(); cmd->Begin();

        NkRect2D rtArea{0,0,(int32)RTW,(int32)RTH};

        // T3 : passe 1 = geom dans hSrcRT
        if (test==3 && hGeomPipe.IsValid()) {
            cmd->SetClearColor(0,0,0,1); cmd->SetClearDepth(1.f);
            cmd->BeginRenderPass(hSrcRP, hSrcFBO, rtArea);
            cmd->SetViewport({0,0,(float)RTW,(float)RTH,0,1});
            cmd->SetScissor(rtArea);
            cmd->BindGraphicsPipeline(hGeomPipe);
            cmd->BindVertexBuffer(0, hVBO, 0);
            cmd->Draw(3,1,0,0);
            cmd->EndRenderPass();
        }

        // T5/T6 : passe1 geom->RT_A, passe2 sample RT_A->RT_B (avant la passe principale).
        if ((test==6||test==7) && hChGeomPipe.IsValid()) {
            // passe 1 : geom RGB -> RT_A
            cmd->SetClearColor(0,0,0,1); cmd->SetClearDepth(1.f);
            cmd->BeginRenderPass(hChARP, hChAFBO, rtArea);
            cmd->SetViewport({0,0,(float)RTW,(float)RTH,0,1}); cmd->SetScissor(rtArea);
            cmd->BindGraphicsPipeline(hChGeomPipe);
            cmd->BindVertexBuffer(0, hChVBO, 0);
            cmd->Draw(3,1,0,0);
            cmd->EndRenderPass();
            // passe 2 : sample RT_A -> RT_B
            cmd->SetClearColor(0,0,0,1); cmd->SetClearDepth(1.f);
            cmd->BeginRenderPass(hChBRP, hChBFBO, rtArea);
            cmd->SetViewport({0,0,(float)RTW,(float)RTH,0,1}); cmd->SetScissor(rtArea);
            cmd->BindGraphicsPipeline(hChSampPipeB);
            if (hChSetA.IsValid()) cmd->BindDescriptorSet(hChSetA, 0);
            cmd->Draw(3,1,0,0);
            cmd->EndRenderPass();
        }

        // Passe principale dans hRT (cible de readback)
        cmd->SetClearColor(0.1f,0.1f,0.5f,1.f); cmd->SetClearDepth(1.f); // bleu = baseline T0
        cmd->BeginRenderPass(hRTRP, hRTFBO, rtArea);
        cmd->SetViewport({0,0,(float)RTW,(float)RTH,0,1});
        cmd->SetScissor(rtArea);
        if (test==1 && hGeomPipe.IsValid()) {
            cmd->BindGraphicsPipeline(hGeomPipe);
            cmd->BindVertexBuffer(0, hVBO, 0);
            cmd->Draw(3,1,0,0);
        } else if (test==2 && hFsPipe.IsValid()) {
            cmd->BindGraphicsPipeline(hFsPipe);
            cmd->Draw(3,1,0,0);
        } else if (test==3 && hSampPipe.IsValid()) {
            cmd->BindGraphicsPipeline(hSampPipe);
            if (hSampSet.IsValid()) cmd->BindDescriptorSet(hSampSet, 0);
            cmd->Draw(3,1,0,0);
        } else if ((test==4||test==5||test==8) && hT4Pipe.IsValid()) {
            cmd->BindGraphicsPipeline(hT4Pipe);
            if (hT4Set.IsValid()) cmd->BindDescriptorSet(hT4Set, 0);
            if (test==8) {
                // push constant 48o : p0=(expo,0,0,0) p1=(0,0,yflip=-1,0) p2=0
                float32 pc[12] = { 1.f,0,0,0,  0,0,-1.f,0,  0,0,0,0 };
                cmd->PushConstants(NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), pc);
            }
            cmd->Draw(3,1,0,0);
        } else if ((test==6||test==7) && hChSampPipeA.IsValid()) {
            // passe 3 : sample RT_B -> hRT
            cmd->BindGraphicsPipeline(hChSampPipeA);
            if (hChSetB.IsValid()) cmd->BindDescriptorSet(hChSetB, 0);
            cmd->Draw(3,1,0,0);
        }
        cmd->EndRenderPass();
        cmd->End();
        dev->Submit(&cmd, 1, {});
        dev->WaitIdle();

        // Readback une seule fois (après quelques frames de stabilisation)
        if (!readbackDone && frameCount>=2) {
            readbackDone=true;
            char tag[8]; snprintf(tag,sizeof(tag),"T%d", test);
            ReadbackAndLog(dev, cmd, hRT, RTW, RTH, tag, apiName);
        }

        // Présent : clear le swapchain (la fenêtre montre juste du noir, le test = readback)
        NkFramebufferHandle hSwap = dev->GetSwapchainFramebuffer();
        cmd->Reset(); cmd->Begin();
        cmd->SetClearColor(0.f,0.f,0.f,1.f); cmd->SetClearDepth(1.f);
        NkRect2D swArea{0,0,(int32)dev->GetSwapchainWidth(),(int32)dev->GetSwapchainHeight()};
        cmd->BeginRenderPass(dev->GetSwapchainRenderPass(), hSwap, swArea);
        cmd->EndRenderPass();
        cmd->End();
        dev->SubmitAndPresent(cmd);
        dev->EndFrame(frame);

        frameCount++;
        if (frameCount>=6) running=false; // auto-quit après readback + qq frames
    }

    dev->WaitIdle();
    // ── Nettoyage (Create→Destroy) ───────────────────────────────────────────
    if (hChSampPipeA.IsValid()) dev->DestroyPipeline(hChSampPipeA);
    if (hChSampPipeB.IsValid()) dev->DestroyPipeline(hChSampPipeB);
    if (hChGeomPipe.IsValid())  dev->DestroyPipeline(hChGeomPipe);
    if (hChSampSh.IsValid())    dev->DestroyShader(hChSampSh);
    if (hChGeomSh.IsValid())    dev->DestroyShader(hChGeomSh);
    if (hChVBO.IsValid())       dev->DestroyBuffer(hChVBO);
    if (hChSetA.IsValid())      dev->FreeDescriptorSet(hChSetA);
    if (hChSetB.IsValid())      dev->FreeDescriptorSet(hChSetB);
    if (hChLayout.IsValid())    dev->DestroyDescriptorSetLayout(hChLayout);
    if (hChSamp.IsValid())      dev->DestroySampler(hChSamp);
    if (hChAFBO.IsValid())      dev->DestroyFramebuffer(hChAFBO);
    if (hChBFBO.IsValid())      dev->DestroyFramebuffer(hChBFBO);
    if (hChARP.IsValid() && hChARP.id!=hRTRP.id) dev->DestroyRenderPass(hChARP);
    if (hChA.IsValid())         dev->DestroyTexture(hChA);
    if (hChB.IsValid())         dev->DestroyTexture(hChB);
    if (hT4Pipe.IsValid())     dev->DestroyPipeline(hT4Pipe);
    if (hT4Sh.IsValid())       dev->DestroyShader(hT4Sh);
    if (hT4Set.IsValid())      dev->FreeDescriptorSet(hT4Set);
    if (hT4Layout.IsValid())   dev->DestroyDescriptorSetLayout(hT4Layout);
    if (hT4Samp.IsValid())     dev->DestroySampler(hT4Samp);
    for (auto& h:hT4) if (h.IsValid()) dev->DestroyTexture(h);
    if (hSampSet.IsValid())    dev->FreeDescriptorSet(hSampSet);
    if (hSampLayout.IsValid()) dev->DestroyDescriptorSetLayout(hSampLayout);
    if (hSamp.IsValid())       dev->DestroySampler(hSamp);
    if (hSampPipe.IsValid())   dev->DestroyPipeline(hSampPipe);
    if (hSampSh.IsValid())     dev->DestroyShader(hSampSh);
    if (hSrcFBO.IsValid())     dev->DestroyFramebuffer(hSrcFBO);
    if (hSrcRT.IsValid())      dev->DestroyTexture(hSrcRT);
    if (hFsPipe.IsValid())     dev->DestroyPipeline(hFsPipe);
    if (hFsSh.IsValid())       dev->DestroyShader(hFsSh);
    if (hGeomPipe.IsValid())   dev->DestroyPipeline(hGeomPipe);
    if (hGeomSh.IsValid())     dev->DestroyShader(hGeomSh);
    if (hVBO.IsValid())        dev->DestroyBuffer(hVBO);
    dev->DestroyCommandBuffer(cmd);
    if (hRTFBO.IsValid())      dev->DestroyFramebuffer(hRTFBO);
    if (hRTRP.IsValid())       dev->DestroyRenderPass(hRTRP);
    if (hRTDepth.IsValid())    dev->DestroyTexture(hRTDepth);
    if (hRT.IsValid())         dev->DestroyTexture(hRT);
    NkDeviceFactory::Destroy(dev);
    window.Close();
    logger.Info("[InterpTest] === done T{0} {1} ===\n", test, apiName);
    return 0;
}
