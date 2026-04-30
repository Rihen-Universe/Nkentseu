// =============================================================================
// NkRHIDemoFull.cpp — v4 final
//
// CORRECTIONS v4 :
//   BUG 1 (ROOT CAUSE des "points") :
//     out.position = {v->pos.x, v->pos.y, v->pos.z, 1.f}; → out.position = clipPos;
//     Les vertices SW n'étaient PAS transformés → positions [-0.5..0.5] en NDC
//     → triangles de 1 pixel = semblent être des points.
//
//   BUG 7 (Vulkan crash intermittent) :
//     - hRP re-fetché à chaque frame (pas sauvegardé avant la boucle)
//     - UBO double-buffering (2 copies, indexées par frameIndex)
//     - Fence avant WriteBuffer pour attendre GPU
//
//   INTÉGRATION NkSL :
//     - NkSLShaderProvider : sélectionne GLSL/HLSL/MSL/SPIRV selon l'API
//       en compilant via NkSLCompiler si la source est .nksl
//     - NkSWShaderBuilder reste l'interface pour les shaders CPU SW
//     - Pour chaque backend : la source NkSL est compilée vers le bon langage
//
// ARCHITECTURE NkSL → backends :
//   .nksl → NkSLCompiler → GLSL  → OpenGL
//                        → SPIRV → Vulkan
//                        → HLSL  → DX11/DX12
//                        → MSL   → Metal
//                        → CPU   → Software (NkSWShader)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKTime/NkTime.h"
#include "NKRenderer/RHI/Backend/Software/NkSoftwareDevice.h"
#include "NKRenderer/RHI/Backend/Software/NkSWShaderHelper.h"
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Core/NkDeviceFactory.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/SL/NkSLCompiler.h"
#include "NKRenderer/SL/NkSLIntegration.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstdio>
#include <cstring>

// SPIR-V précompilé Vulkan (si disponible)
#include "NkRHIDemoFullVkSpv.inl"

namespace nkentseu { struct NkEntryState; }

// =============================================================================
// Source NkSL partagée — compilée vers le bon backend selon l'API
// =============================================================================
static constexpr const char* kNkSL_Phong3D = R"NKSL(
// NkSL Phong3D — compilé vers GLSL/HLSL/MSL/SPIRV/CPU selon le backend

struct UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} binding(0, uniform);

// ── Vertex ────────────────────────────────────────────────────────────────────
vertex main(
    vec3 aPos    : location(0),
    vec3 aNormal : location(1),
    vec3 aColor  : location(2)
) -> (
    vec4 sv_position,
    vec3 vWorldPos  : location(0),
    vec3 vNormal    : location(1),
    vec3 vColor     : location(2)
) {
    vec4 worldPos = UBO.model * vec4(aPos, 1.0);
    vWorldPos     = worldPos.xyz;
    vNormal       = normalize(mat3(UBO.model) * aNormal);
    vColor        = aColor;
    sv_position   = UBO.proj * UBO.view * worldPos;
}

// ── Fragment ──────────────────────────────────────────────────────────────────
fragment main(
    vec3 vWorldPos  : location(0),
    vec3 vNormal    : location(1),
    vec3 vColor     : location(2)
) -> vec4 sv_target {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-UBO.lightDirW.xyz);
    vec3 V = normalize(UBO.eyePosW.xyz - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = diff * vColor;
    vec3 specular = spec * vec3(0.4, 0.4, 0.4);

    sv_target = vec4(ambient + diffuse + specular, 1.0);
}
)NKSL";

// Fallback GLSL direct (si NkSL non disponible ou pour tests)
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4 model; mat4 view; mat4 proj; mat4 lightVP;
    vec4 lightDirW; vec4 eyePosW;
} ubo;

out vec3 vWorldPos; out vec3 vNormal; out vec3 vColor;

void main() {
    vec4 worldPos = ubo.model * vec4(aPos, 1.0);
    vWorldPos     = worldPos.xyz;
    vNormal       = normalize(mat3(ubo.model) * aNormal);
    vColor        = aColor;
    gl_Position   = ubo.proj * ubo.view * worldPos;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
in vec3 vWorldPos; in vec3 vNormal; in vec3 vColor;
out vec4 fragColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4 model; mat4 view; mat4 proj; mat4 lightVP;
    vec4 lightDirW; vec4 eyePosW;
} ubo;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    vec3 V = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H = normalize(L + V);
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 col = 0.15*vColor + diff*vColor + spec*vec3(0.4);
    fragColor = vec4(col, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(std140, binding = 0) uniform ShadowUBO {
    mat4 model; mat4 view; mat4 proj; mat4 lightVP; vec4 lightDirW; vec4 eyePosW;
} ubo;
void main() { gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0); }
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
void main() {}
)GLSL";

static constexpr const char* kHLSL_VS = R"HLSL(
cbuffer UBO : register(b0) {
    float4x4 model, view, proj, lightVP; float4 lightDirW, eyePosW;
};
struct VSIn { float3 pos:POSITION; float3 norm:NORMAL; float3 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float3 wp:TEXCOORD0; float3 n:TEXCOORD1; float3 c:TEXCOORD2; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos, 1));
    o.wp = wp.xyz; o.n = normalize(mul((float3x3)model, v.norm));
    o.c = v.col; o.pos = mul(proj, mul(view, wp));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_PS = R"HLSL(
cbuffer UBO : register(b0) {
    float4x4 model, view, proj, lightVP; float4 lightDirW, eyePosW;
};
struct PSIn { float4 pos:SV_Position; float3 wp:TEXCOORD0; float3 n:TEXCOORD1; float3 c:TEXCOORD2; };
float4 PSMain(PSIn i) : SV_Target {
    float3 N=normalize(i.n), L=normalize(-lightDirW.xyz);
    float3 V=normalize(eyePosW.xyz-i.wp), H=normalize(L+V);
    float diff=max(dot(N,L),0.0f), spec=pow(max(dot(N,H),0.0f),32.0f);
    return float4(0.15f*i.c + diff*i.c + spec*0.4f, 1.0f);
}
)HLSL";

static constexpr const char* kHLSL_ShadowVS = R"HLSL(
cbuffer UBO : register(b0) { float4x4 model,view,proj,lightVP; float4 lightDirW,eyePosW; };
struct VSIn { float3 pos:POSITION; }; struct VSOut { float4 pos:SV_POSITION; };
VSOut main(VSIn v) { VSOut o; o.pos=mul(lightVP,mul(model,float4(v.pos,1))); return o; }
)HLSL";

static constexpr const char* kHLSL_ShadowPS = R"HLSL(
struct PSIn { float4 pos:SV_POSITION; }; void main(PSIn i) {}
)HLSL";

static constexpr const char* kMSL_Shaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UBO { float4x4 model,view,proj; float4 lightDirW,eyePosW; };
struct VSIn  { float3 pos [[attribute(0)]]; float3 norm [[attribute(1)]]; float3 col [[attribute(2)]]; };
struct VSOut { float4 pos [[position]]; float3 wp,n,c; };
vertex VSOut vmain(VSIn v [[stage_in]], constant UBO& u [[buffer(0)]]) {
    VSOut o; float4 wp=u.model*float4(v.pos,1);
    o.wp=wp.xyz; o.n=normalize(float3x3(u.model[0].xyz,u.model[1].xyz,u.model[2].xyz)*v.norm);
    o.c=v.col; o.pos=u.proj*u.view*wp; return o;
}
fragment float4 fmain(VSOut i [[stage_in]], constant UBO& u [[buffer(0)]]) {
    float3 N=normalize(i.n), L=normalize(-u.lightDirW.xyz);
    float3 V=normalize(u.eyePosW.xyz-i.wp), H=normalize(L+V);
    float diff=max(dot(N,L),0.f), spec=pow(max(dot(N,H),0.f),32.f);
    return float4(0.15f*i.c+diff*i.c+spec*0.4f,1.f);
}
)MSL";

// =============================================================================
// UBO
// =============================================================================
struct alignas(16) UboData {
    float model[16], view[16], proj[16], lightVP[16];
    float lightDirW[4], eyePosW[4];
};
static constexpr uint32 kUboStride = sizeof(UboData);

// =============================================================================
// Géométrie
// =============================================================================
using namespace nkentseu;
using namespace nkentseu::math;
struct Vtx3D { NkVec3f pos; NkVec3f normal; NkVec3f color; };
static_assert(sizeof(Vtx3D) == 36, "Vtx3D doit faire 36 bytes (3×vec3)");

static void Mat4ToArr(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P=.5f, N=-.5f;
    struct Face { float vx[4][3]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}},0,0,1},
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}},0,0,-1},
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}},0,-1,0},
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}},0,1,0},
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},-1,0,0},
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}},1,0,0}
    };
    static const int idx[6]={0,1,2,0,2,3};
    NkVector<Vtx3D> v; v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]),
                        NkVec3f(f.nx,f.ny,f.nz), NkVec3f(r,g,b)});
    return v;
}

static NkVector<Vtx3D> MakeSphere(int stacks=16, int slices=24,
                                    float r=0.2f, float g=0.55f, float b=0.9f) {
    NkVector<Vtx3D> v;
    const float pi = (float)NkPi;
    for (int i=0;i<stacks;i++) {
        float phi0=(float)i/stacks*pi, phi1=(float)(i+1)/stacks*pi;
        for (int j=0;j<slices;j++) {
            float th0=(float)j/slices*2.f*pi, th1=(float)(j+1)/slices*2.f*pi;
            auto mk=[&](float phi,float th)->Vtx3D{
                float x=NkSin(phi)*NkCos(th),y=NkCos(phi),z=NkSin(phi)*NkSin(th);
                return {NkVec3f(x*.5f,y*.5f,z*.5f),NkVec3f(x,y,z),NkVec3f(r,g,b)};};
            Vtx3D a=mk(phi0,th0),b2=mk(phi0,th1),c=mk(phi1,th0),d=mk(phi1,th1);
            v.PushBack(a);v.PushBack(b2);v.PushBack(d);
            v.PushBack(a);v.PushBack(d);v.PushBack(c);
        }
    }
    return v;
}

static NkVector<Vtx3D> MakePlane(float sz=3.f,float r=0.35f,float g=0.65f,float b=0.35f) {
    float h=sz*.5f; NkVector<Vtx3D> v; v.Reserve(6);
    v.PushBack({NkVec3f(-h,0,h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(h,0,h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(h,0,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(-h,0,h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(h,0,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(-h,0,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    return v;
}

// =============================================================================
// NkSLShaderProvider — intégration NkSL
//
// Stratégie : tente de compiler la source NkSL vers le bon backend.
// En cas d'échec (NkSL non disponible ou erreur de compilation), utilise
// le fallback GLSL/HLSL/MSL hardcodé.
//
// Pour le backend Software, génère un NkSWShader CPU via NkSLCodeGenCPP.
// =============================================================================
class NkSLShaderProvider {
public:
    // Compile une source NkSL et retourne un NkShaderDesc pour l'API donnée.
    // Retourne un desc vide si la compilation échoue → l'appelant utilise son fallback.
    static NkShaderDesc CompileNkSL(const char* nkslSource, NkGraphicsApi api) {
        NkShaderDesc result;
        if (!nkslSource) return result;

        // Récupérer le compilateur NkSL singleton
        NkSLCompiler* compiler = NkSLIntegration::GetCompiler();
        if (!compiler) {
            logger.Info("[NkSL] Compilateur non disponible → fallback GLSL/HLSL\n");
            return result;
        }

        NkSLCompileOptions opts;
        opts.sourceLanguage = NkSLSourceLanguage::NkSL;
        opts.debugInfo      = false;

        switch (api) {
            case NkGraphicsApi::NK_GFX_API_OPENGL:
                opts.targetLanguage = NkSLTargetLanguage::GLSL;
                opts.glslVersion    = 460;
                break;
            case NkGraphicsApi::NK_GFX_API_VULKAN:
                opts.targetLanguage = NkSLTargetLanguage::SPIRV;
                opts.spirvVersion   = 0x00010000;
                break;
            case NkGraphicsApi::NK_GFX_API_D3D11:
                opts.targetLanguage = NkSLTargetLanguage::HLSL;
                opts.hlslShaderModel = 50;
                break;
            case NkGraphicsApi::NK_GFX_API_D3D12:
                opts.targetLanguage = NkSLTargetLanguage::HLSL;
                opts.hlslShaderModel = 60;
                break;
            case NkGraphicsApi::NK_GFX_API_METAL:
                opts.targetLanguage = NkSLTargetLanguage::MSL;
                break;
            default:
                // Software : ne passe pas par NkSL (CPU functions)
                return result;
        }

        NkSLCompileResult cres = compiler->Compile(nkslSource, opts);
        if (!cres.success) {
            logger.Info("[NkSL] Erreur de compilation → fallback\n");
            for (auto& e : cres.errors)
                logger.Info("[NkSL] {:}: {:}\n", e.line, e.message.CStr());
            return result;
        }

        // Construire le NkShaderDesc selon le langage cible
        switch (opts.targetLanguage) {
            case NkSLTargetLanguage::GLSL:
                result.AddGLSL(NkShaderStage::NK_VERTEX,   cres.vertexSource.CStr());
                result.AddGLSL(NkShaderStage::NK_FRAGMENT,  cres.fragmentSource.CStr());
                logger.Info("[NkSL] Compilé → GLSL OK\n");
                break;
            case NkSLTargetLanguage::HLSL:
                result.AddHLSL(NkShaderStage::NK_VERTEX,   cres.vertexSource.CStr(),   "VSMain");
                result.AddHLSL(NkShaderStage::NK_FRAGMENT,  cres.fragmentSource.CStr(), "PSMain");
                logger.Info("[NkSL] Compilé → HLSL OK\n");
                break;
            case NkSLTargetLanguage::SPIRV:
                if (!cres.spirvVertex.IsEmpty())
                    result.AddSPIRV(NkShaderStage::NK_VERTEX,
                                    cres.spirvVertex.Data(),
                                    (uint64)cres.spirvVertex.Size() * sizeof(uint32));
                if (!cres.spirvFragment.IsEmpty())
                    result.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                                    cres.spirvFragment.Data(),
                                    (uint64)cres.spirvFragment.Size() * sizeof(uint32));
                logger.Info("[NkSL] Compilé → SPIR-V OK ({0} mots vert, {1} mots frag)\n",
                            cres.spirvVertex.Size(), cres.spirvFragment.Size());
                break;
            case NkSLTargetLanguage::MSL:
                result.AddMSL(NkShaderStage::NK_VERTEX,   cres.vertexSource.CStr(),   "vmain");
                result.AddMSL(NkShaderStage::NK_FRAGMENT,  cres.fragmentSource.CStr(), "fmain");
                logger.Info("[NkSL] Compilé → MSL OK\n");
                break;
            default: break;
        }
        return result;
    }
};

// =============================================================================
// Sélection backend
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i=1; i<args.Size(); i++) {
        const NkString& a=args[i];
        if (a=="--backend=vulkan"||a=="-bvk")    return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (a=="--backend=dx11"  ||a=="-bdx11")  return NkGraphicsApi::NK_GFX_API_D3D11;
        if (a=="--backend=dx12"  ||a=="-bdx12")  return NkGraphicsApi::NK_GFX_API_D3D12;
        if (a=="--backend=metal" ||a=="-bmtl")   return NkGraphicsApi::NK_GFX_API_METAL;
        if (a=="--backend=sw"    ||a=="-bsw")    return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (a=="--backend=opengl"||a=="-bgl")    return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

static NkContextDesc MakeCtxDesc(NkGraphicsApi api) {
    NkContextDesc d; d.api = api;
    if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
        d = NkContextDesc::MakeOpenGLES(3, 0);
#else
        d.opengl = NkOpenGLDesc::Desktop46(false);
        d.opengl.runtime.autoLoadEntryPoints = true;
        d.opengl.swapInterval = NkGLSwapInterval::VSync;
#endif
    }
    return d;
}

// Construit un NkShaderDesc en essayant d'abord NkSL, puis le fallback hardcodé.
static NkShaderDesc MakeMainShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "Phong3D";

    // 1. Tenter la compilation NkSL
    sd = NkSLShaderProvider::CompileNkSL(kNkSL_Phong3D, api);
    if (!sd.stages.IsEmpty()) return sd;

    // 2. Fallback hardcodé par API
    logger.Info("[RHIFullDemo] NkSL non disponible → fallback hardcodé pour {0}\n",
                NkGraphicsApiName(api));
    switch (api) {
        case NkGraphicsApi::NK_GFX_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkRHIFullDemoVertSpv,
                        (uint64)kVkRHIFullDemoVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkRHIFullDemoFragSpv,
                        (uint64)kVkRHIFullDemoFragSpvWordCount * sizeof(uint32));
            break;
        case NkGraphicsApi::NK_GFX_API_D3D11:
        case NkGraphicsApi::NK_GFX_API_D3D12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_VS, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_PS, "PSMain");
            break;
        case NkGraphicsApi::NK_GFX_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_Shaders, "vmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_Shaders, "fmain");
            break;
        default: // OpenGL + Software
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Frag);
            break;
    }
    return sd;
}

static NkShaderDesc MakeShadowShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "ShadowDepth";
    switch (api) {
        case NkGraphicsApi::NK_GFX_API_OPENGL:
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_ShadowVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_ShadowFrag);
            break;
        case NkGraphicsApi::NK_GFX_API_D3D11:
        case NkGraphicsApi::NK_GFX_API_D3D12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_ShadowVS);
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_ShadowPS);
            break;
        default: break; // Vulkan/Metal : shadow non implémentée
    }
    return sd;
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {

    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    const char* apiName = NkGraphicsApiName(targetApi);
    logger.Info("[RHIFullDemo] Backend : {0}\n", apiName);

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title    = NkFormat("NkRHI Full Demo v4 — {0}", apiName);
    winCfg.width    = 1280; winCfg.height = 720;
    winCfg.centered = true; winCfg.resizable = true;
    NkWindow window;
    if (!window.Create(winCfg)) { logger.Info("[RHIFullDemo] Fenêtre échouée\n"); return 1; }

    // ── Contexte + Device ─────────────────────────────────────────────────────
    NkContextDesc ctxDesc = MakeCtxDesc(targetApi);
    auto ctx = NkContextFactory::Create(window, ctxDesc);
    if (!ctx || !ctx->IsValid()) {
        logger.Info("[RHIFullDemo] Contexte échoué\n"); window.Close(); return 1;
    }
    NkIDevice* device = NkDeviceFactory::Create(ctx);
    if (!device || !device->IsValid()) {
        logger.Info("[RHIFullDemo] Device échoué\n"); ctx->Shutdown(); window.Close(); return 1;
    }
    logger.Info("[RHIFullDemo] Device OK. VRAM: {0} Mo\n",
                (unsigned long long)(device->GetCaps().vramBytes >> 20));

    // ── Dimensions ────────────────────────────────────────────────────────────
    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── Shader principal ──────────────────────────────────────────────────────
    NkShaderDesc shaderDesc = MakeMainShaderDesc(targetApi);
    NkShaderHandle hShader = device->CreateShader(shaderDesc);
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Shader échoué\n");
        NkDeviceFactory::Destroy(device); ctx->Shutdown(); window.Close(); return 1;
    }

    // ── Vertex Layout — STRIDE EXPLICITE : sizeof(Vtx3D) = 36 ────────────────
    // CORRECTION BUG 10 : stride DOIT être défini ici, sinon TransformVertex
    // lit les vertices avec un offset incorrect.
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGpuFormat::NK_RGB32_FLOAT, 0,              "POSITION", 0)
        .AddAttribute(1, 0, NkGpuFormat::NK_RGB32_FLOAT, 3*sizeof(float), "NORMAL",  0)
        .AddAttribute(2, 0, NkGpuFormat::NK_RGB32_FLOAT, 6*sizeof(float), "COLOR",   0)
        .AddBinding(0, (uint32)sizeof(Vtx3D));  // ← 36 bytes, pas 32 !

    // ── Descriptor Set Layout ─────────────────────────────────────────────────
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);

    // ── Pipeline principal ────────────────────────────────────────────────────
    NkGraphicsPipelineDesc pipeDesc;
    pipeDesc.shader       = hShader;
    pipeDesc.vertexLayout = vtxLayout;
    pipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pipeDesc.rasterizer   = NkRasterizerDesc::Default();
    pipeDesc.depthStencil = NkDepthStencilDesc::Default();
    pipeDesc.blend        = NkBlendDesc::Opaque();
    pipeDesc.debugName    = "PipelinePhong3D";
    if (hLayout.IsValid()) pipeDesc.descriptorSetLayouts.PushBack(hLayout);
    NkPipelineHandle hPipe = device->CreateGraphicsPipeline(pipeDesc);
    if (!hPipe.IsValid()) {
        logger.Info("[RHIFullDemo] Pipeline échoué\n");
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device); ctx->Shutdown(); window.Close(); return 1;
    }

    // ── Shaders CPU pour le backend Software ──────────────────────────────────
    // CORRECTION BUG 1 (ROOT CAUSE) : out.position = clipPos (pas pos brute!)
    if (targetApi == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);
        NkSWShader* sw = swDev->GetShader(hShader.id);
        if (sw) {
            sw->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkSWVertex {
                const Vtx3D* v = static_cast<const Vtx3D*>(vdata) + idx;
                NkSWVertex out;

                if (!udata) {
                    // Pas d'UBO : position NDC directe pour tests
                    out.position = {v->pos.x, v->pos.y, v->pos.z, 1.f};
                    out.normal   = v->normal;
                    out.color    = {v->color.r, v->color.g, v->color.b, 1.f};
                    return out;
                }

                const UboData* ubo = static_cast<const UboData*>(udata);

                // Multiplication matrice column-major 4×4 × vecteur
                auto mulM = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                    return {
                        m[0]*x + m[4]*y + m[8]*z  + m[12]*w,
                        m[1]*x + m[5]*y + m[9]*z  + m[13]*w,
                        m[2]*x + m[6]*y + m[10]*z + m[14]*w,
                        m[3]*x + m[7]*y + m[11]*z + m[15]*w
                    };
                };

                // World → View → Clip
                NkVec4f worldPos = mulM(ubo->model, v->pos.x, v->pos.y, v->pos.z, 1.f);
                NkVec4f viewPos  = mulM(ubo->view,  worldPos.x, worldPos.y, worldPos.z, worldPos.w);
                NkVec4f clipPos  = mulM(ubo->proj,  viewPos.x,  viewPos.y,  viewPos.z,  viewPos.w);

                // ═══════════════════════════════════════════════════════════════
                // CORRECTION BUG 1 — ROOT CAUSE des "points" :
                // out.position DOIT être clipPos, PAS la position locale.
                // Avant cette correction :
                //   out.position = {v->pos.x, v->pos.y, v->pos.z, 1.f}; ← FAUX
                // Avec la correction :
                out.position = clipPos;   // ← CORRECT
                // ═══════════════════════════════════════════════════════════════

                // Transformation de la normale (matrice normale = mat3(model))
                float nx = ubo->model[0]*v->normal.x + ubo->model[4]*v->normal.y + ubo->model[8]*v->normal.z;
                float ny = ubo->model[1]*v->normal.x + ubo->model[5]*v->normal.y + ubo->model[9]*v->normal.z;
                float nz = ubo->model[2]*v->normal.x + ubo->model[6]*v->normal.y + ubo->model[10]*v->normal.z;
                float nl = NkSqrt(nx*nx + ny*ny + nz*nz);
                if (nl > 0.001f) { nx/=nl; ny/=nl; nz/=nl; }
                out.normal = {nx, ny, nz};

                out.color = {v->color.r, v->color.g, v->color.b, 1.f};

                // World pos dans attrs[0..2] pour le fragment shader
                out.attrs[0]=worldPos.x; out.attrs[1]=worldPos.y; out.attrs[2]=worldPos.z;
                out.attrCount = 3;

                return out;
            };

            sw->fragFn = [](const NkSWVertex& frag, const void* udata, const void*) -> NkVec4f {
                if (!udata) return frag.color;
                const UboData* ubo = static_cast<const UboData*>(udata);

                // Normaliser la normale
                float nx=frag.normal.x, ny=frag.normal.y, nz=frag.normal.z;
                float nl=NkSqrt(nx*nx+ny*ny+nz*nz);
                if (nl>0.001f){nx/=nl;ny/=nl;nz/=nl;}

                // Direction lumière (normalisée, inversée pour dot(N,L) > 0 si éclairé)
                float lx=-ubo->lightDirW[0], ly=-ubo->lightDirW[1], lz=-ubo->lightDirW[2];
                float ll=NkSqrt(lx*lx+ly*ly+lz*lz);
                if (ll>0.001f){lx/=ll;ly/=ll;lz/=ll;}

                // Direction vue (depuis la world pos stockée dans attrs)
                float vx=ubo->eyePosW[0]-frag.attrs[0];
                float vy=ubo->eyePosW[1]-frag.attrs[1];
                float vz=ubo->eyePosW[2]-frag.attrs[2];
                float vl=NkSqrt(vx*vx+vy*vy+vz*vz);
                if (vl>0.001f){vx/=vl;vy/=vl;vz/=vl;}

                // Half-vector
                float hx=(lx+vx)*.5f, hy=(ly+vy)*.5f, hz=(lz+vz)*.5f;
                float hl=NkSqrt(hx*hx+hy*hy+hz*hz);
                if (hl>0.001f){hx/=hl;hy/=hl;hz/=hl;}

                float diff  = NkMax(0.f, nx*lx+ny*ly+nz*lz);
                float sdot  = NkMax(0.f, nx*hx+ny*hy+nz*hz);
                float spec  = NkPow(sdot, 32.f);

                float r = NkClamp(0.15f*frag.color.x + diff*frag.color.x + spec*0.4f, 0.f, 1.f);
                float g = NkClamp(0.15f*frag.color.y + diff*frag.color.y + spec*0.4f, 0.f, 1.f);
                float b = NkClamp(0.15f*frag.color.z + diff*frag.color.z + spec*0.4f, 0.f, 1.f);
                return {r, g, b, 1.f};
            };
        }
    }

    // ── Géométrie ─────────────────────────────────────────────────────────────
    auto cubeV   = MakeCube();
    auto sphereV = MakeSphere();
    auto planeV  = MakePlane();

    auto uploadVBO = [&](const NkVector<Vtx3D>& v) -> NkBufferHandle {
        return device->CreateBuffer(NkBufferDesc::Vertex((uint64)v.Size()*sizeof(Vtx3D), v.Data()));
    };
    NkBufferHandle hCube   = uploadVBO(cubeV);
    NkBufferHandle hSphere = uploadVBO(sphereV);
    NkBufferHandle hPlane  = uploadVBO(planeV);

    // ── Shadow map ────────────────────────────────────────────────────────────
    static constexpr uint32 kShadowSize = 2048;
    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;

    {
        NkShaderDesc sd = MakeShadowShaderDesc(targetApi);
        if (!sd.stages.IsEmpty()) hShadowShader = device->CreateShader(sd);
    }
    const bool hasShadow = hShadowShader.IsValid();
    logger.Info("[RHIFullDemo] Shadow map : {0}\n", hasShadow ? "activée" : "désactivée");

    if (hasShadow) {
        NkTextureDesc td = NkTextureDesc::DepthStencil(kShadowSize, kShadowSize);
        td.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
        hShadowTex = device->CreateTexture(td);

        NkSamplerDesc sd = NkSamplerDesc::Shadow();
        sd.magFilter = NkFilter::NK_LINEAR; sd.minFilter = NkFilter::NK_LINEAR;
        sd.mipFilter = NkMipFilter::NK_NONE; sd.minLod = sd.maxLod = 0.f;
        hShadowSampler = device->CreateSampler(sd);

        hShadowRP  = device->CreateRenderPass(NkRenderPassDesc::ShadowMap());

        NkFramebufferDesc fd{}; fd.renderPass=hShadowRP;
        fd.depthAttachment=hShadowTex; fd.width=kShadowSize; fd.height=kShadowSize;
        hShadowFBO = device->CreateFramebuffer(fd);

        NkVertexLayout svl;
        svl.AddAttribute(0,0,NkGpuFormat::NK_RGB32_FLOAT,0,"POSITION",0)
           .AddBinding(0,(uint32)sizeof(Vtx3D));

        NkGraphicsPipelineDesc spd{};
        spd.shader=hShadowShader; spd.vertexLayout=svl;
        spd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        spd.rasterizer=NkRasterizerDesc::ShadowMap();
        spd.depthStencil=NkDepthStencilDesc::Default();
        spd.blend=NkBlendDesc::Opaque(); spd.renderPass=hShadowRP;
        if (hLayout.IsValid()) spd.descriptorSetLayouts.PushBack(hLayout);
        hShadowPipe = device->CreateGraphicsPipeline(spd);
    }

    // ── Descriptor Set Layout étendu (avec shadow sampler si actif) ───────────
    // Recréer le layout en ajoutant binding 1 si shadow activée
    if (hasShadow && hLayout.IsValid()) {
        device->DestroyDescriptorSetLayout(hLayout);
        NkDescriptorSetLayoutDesc ld2;
        ld2.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER,          NkShaderStage::NK_ALL_GRAPHICS);
        ld2.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  NkShaderStage::NK_FRAGMENT);
        hLayout = device->CreateDescriptorSetLayout(ld2);
    }

    // ── UBO double-buffering (BUG 7 fix) ─────────────────────────────────────
    // 2 copies par objet pour éviter que le GPU lise pendant que le CPU écrit
    static constexpr uint32 kFramesInFlight = 2;
    static constexpr uint32 kObjCount       = 3;
    NkBufferHandle   hUBO    [kObjCount][kFramesInFlight] = {};
    NkDescSetHandle  hDescSet[kObjCount][kFramesInFlight] = {};
    NkFenceHandle    hFences [kFramesInFlight] = {};

    for (uint32 f = 0; f < kFramesInFlight; f++) {
        hFences[f] = device->CreateFence(true); // signaled=true = prêt dès le départ
        for (uint32 i = 0; i < kObjCount; i++) {
            hUBO[i][f]     = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
            hDescSet[i][f] = device->AllocateDescriptorSet(hLayout);
            if (hLayout.IsValid() && hDescSet[i][f].IsValid() && hUBO[i][f].IsValid()) {
                NkDescriptorWrite w{};
                w.set=hDescSet[i][f]; w.binding=0;
                w.type=NkDescriptorType::NK_UNIFORM_BUFFER;
                w.buffer=hUBO[i][f]; w.bufferRange=sizeof(UboData);
                device->UpdateDescriptorSets(&w, 1);
            }
            if (hasShadow && hShadowTex.IsValid() && hShadowSampler.IsValid()) {
                NkDescriptorWrite sw{};
                sw.set=hDescSet[i][f]; sw.binding=1;
                sw.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture=hShadowTex; sw.sampler=hShadowSampler;
                sw.textureLayout=NkResourceState::NK_DEPTH_READ;
                device->UpdateDescriptorSets(&sw, 1);
            }
        }
    }

    // ── Command Buffers (un par frame in flight) ──────────────────────────────
    NkICommandBuffer* cmds[kFramesInFlight];
    for (uint32 f = 0; f < kFramesInFlight; f++)
        cmds[f] = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);

    // ── Boucle ────────────────────────────────────────────────────────────────
    bool  running=true; float rotAngle=0.f;
    float camYaw=0.f, camPitch=20.f, camDist=4.f;
    float lightYaw=-45.f, lightPitch=-30.f;
    bool  keys[512] = {};
    uint32 frameIdx = 0;

    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if (e->GetKey()==NkKey::NK_ESCAPE) running=false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
        device->OnResize(W, H);
        // BUG 7 fix : re-fetch le swapchain render pass APRÈS resize
        // (hRP est fetché à chaque frame maintenant, donc OK)
    });

    logger.Info("[RHIFullDemo] Boucle. ESC=quitter WASD=caméra Flèches=lumière\n");

    while (running) {
        events.PollEvents();
        if (!running) break;

        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;

        const float camSpd=60.f, lightSpd=90.f;
        if (keys[(uint32)NkKey::NK_A]) camYaw    -= camSpd*dt;
        if (keys[(uint32)NkKey::NK_D]) camYaw    += camSpd*dt;
        if (keys[(uint32)NkKey::NK_W]) camPitch  += camSpd*dt;
        if (keys[(uint32)NkKey::NK_S]) camPitch  -= camSpd*dt;
        if (keys[(uint32)NkKey::NK_LEFT])  lightYaw  -= lightSpd*dt;
        if (keys[(uint32)NkKey::NK_RIGHT]) lightYaw  += lightSpd*dt;
        if (keys[(uint32)NkKey::NK_UP])    lightPitch+= lightSpd*dt;
        if (keys[(uint32)NkKey::NK_DOWN])  lightPitch-= lightSpd*dt;
        camPitch = NkClamp(camPitch, -80.f, 80.f);
        rotAngle += 45.f*dt;

        float aspect = (H>0) ? (float)W/(float)H : 1.f;
        float eyeX=camDist*NkCos(NkToRadians(camPitch))*NkSin(NkToRadians(camYaw));
        float eyeY=camDist*NkSin(NkToRadians(camPitch));
        float eyeZ=camDist*NkCos(NkToRadians(camPitch))*NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX,eyeY,eyeZ);

        NkMat4f matView = NkMat4f::LookAt(eye, NkVec3f(0,0,0), NkVec3f(0,1,0));
        NkMat4f matProj = NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 100.f);

        float lx=NkCos(NkToRadians(lightPitch))*NkSin(NkToRadians(lightYaw));
        float ly=NkSin(NkToRadians(lightPitch));
        float lz=NkCos(NkToRadians(lightPitch))*NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx,ly,lz);
        NkVec3f lightPos(-lx*10.f,-ly*10.f,-lz*10.f);
        NkVec3f lightUp = (NkFabs(ly)>0.9f) ? NkVec3f(1,0,0) : NkVec3f(0,1,0);
        NkMat4f matLightView = NkMat4f::LookAt(lightPos, NkVec3f(0,0,0), lightUp);
        NkMat4f matLightProj = NkMat4f::Orthogonal(NkVec2f(-5,-5),NkVec2f(5,5),1.f,20.f);
        NkMat4f matLightVP   = matLightProj * matLightView;

        // ── Frame ─────────────────────────────────────────────────────────────
        NkFrameContext frame;
        device->BeginFrame(frame);
        if (W==0||H==0) { device->EndFrame(frame); continue; }

        // BUG 7 fix : attendre la fence du frame courant avant d'écrire l'UBO
        uint32 fi = frameIdx % kFramesInFlight;
        if (hFences[fi].IsValid()) {
            device->WaitFence(hFences[fi], UINT64_MAX);
            device->ResetFence(hFences[fi]);
        }

        // BUG 7 fix : re-fetcher le render pass APRÈS BeginFrame (stable pour 1 frame)
        NkRenderPassHandle  hRP  = device->GetSwapchainRenderPass();
        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();

        auto fillUbo = [&](UboData& ubo, const NkMat4f& model) {
            Mat4ToArr(model,     ubo.model);
            Mat4ToArr(matView,   ubo.view);
            Mat4ToArr(matProj,   ubo.proj);
            Mat4ToArr(matLightVP,ubo.lightVP);
            ubo.lightDirW[0]=lightDir.x; ubo.lightDirW[1]=lightDir.y;
            ubo.lightDirW[2]=lightDir.z; ubo.lightDirW[3]=0.f;
            ubo.eyePosW[0]=eye.x; ubo.eyePosW[1]=eye.y;
            ubo.eyePosW[2]=eye.z; ubo.eyePosW[3]=0.f;
        };

        // Pré-calculer les modèles
        NkMat4f matModels[3] = {
            NkMat4f::RotationY(NkAngle(rotAngle)) * NkMat4f::RotationX(NkAngle(rotAngle*.5f)),
            NkMat4f::Translation(NkVec3f(2.f,0.f,0.f)),
            NkMat4f::Translation(NkVec3f(0.f,-0.65f,0.f))
        };

        // Écrire les UBOs du frame courant
        for (uint32 i = 0; i < kObjCount; i++) {
            if (hUBO[i][fi].IsValid()) {
                UboData ubo{};
                fillUbo(ubo, matModels[i]);
                device->WriteBuffer(hUBO[i][fi], &ubo, sizeof(ubo));
            }
        }

        NkICommandBuffer* cmd = cmds[fi];
        cmd->Reset(); cmd->Begin();

        // ── Passe shadow ──────────────────────────────────────────────────────
        if (hasShadow && hShadowFBO.IsValid() && hShadowPipe.IsValid()) {
            NkRect2D sa{0,0,(int32)kShadowSize,(int32)kShadowSize};
            cmd->BeginRenderPass(hShadowRP, hShadowFBO, sa);
            NkViewport svp{0.f,0.f,(float)kShadowSize,(float)kShadowSize,0.f,1.f};
            cmd->SetViewport(svp); cmd->SetScissor(sa);
            cmd->BindGraphicsPipeline(hShadowPipe);
            // Shadow pass : UBO = lightVP*model seulement
            NkBufferHandle vbos[3] = {hCube, hSphere, hPlane};
            uint32 cnts[3] = {(uint32)cubeV.Size(),(uint32)sphereV.Size(),(uint32)planeV.Size()};
            for (uint32 i = 0; i < kObjCount; i++) {
                if (hDescSet[i][fi].IsValid()) {
                    // Pour la passe shadow, l'UBO est déjà écrit avec le bon model + lightVP
                    cmd->BindDescriptorSet(hDescSet[i][fi], 0);
                    cmd->BindVertexBuffer(0, vbos[i]);
                    cmd->Draw(cnts[i]);
                }
            }
            cmd->EndRenderPass();
            if (hShadowTex.IsValid()) {
                NkTextureBarrier tb{};
                tb.texture=hShadowTex;
                tb.stateBefore=NkResourceState::NK_DEPTH_WRITE;
                tb.stateAfter=NkResourceState::NK_DEPTH_READ;
                tb.srcStage=NkPipelineStage::NK_LATE_FRAGMENT;
                tb.dstStage=NkPipelineStage::NK_FRAGMENT_SHADER;
                cmd->Barrier(nullptr,0,&tb,1);
            }
        }

        // ── Passe principale ──────────────────────────────────────────────────
        NkRect2D area{0,0,(int32)W,(int32)H};
        cmd->BeginRenderPass(hRP, hFBO, area);
        NkViewport vp{0.f,0.f,(float)W,(float)H,0.f,1.f};
        cmd->SetViewport(vp); cmd->SetScissor(area);
        cmd->BindGraphicsPipeline(hPipe);

        NkBufferHandle vbos[3] = {hCube, hSphere, hPlane};
        uint32         cnts[3] = {(uint32)cubeV.Size(),(uint32)sphereV.Size(),(uint32)planeV.Size()};
        for (uint32 i = 0; i < kObjCount; i++) {
            if (hDescSet[i][fi].IsValid()) cmd->BindDescriptorSet(hDescSet[i][fi], 0);
            cmd->BindVertexBuffer(0, vbos[i]);
            cmd->Draw(cnts[i]);
        }
        cmd->EndRenderPass();
        cmd->End();

        // Submit avec fence pour le double-buffering
        device->Submit(&cmd, 1, hFences[fi]);
        device->Present();
        device->EndFrame(frame);

        frameIdx++;
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();
    for (uint32 f=0;f<kFramesInFlight;f++) {
        if (cmds[f]) device->DestroyCommandBuffer(cmds[f]);
        if (hFences[f].IsValid()) device->DestroyFence(hFences[f]);
        for (uint32 i=0;i<kObjCount;i++) {
            if (hDescSet[i][f].IsValid()) device->FreeDescriptorSet(hDescSet[i][f]);
            if (hUBO[i][f].IsValid())     device->DestroyBuffer(hUBO[i][f]);
        }
    }
    if (hLayout.IsValid())        device->DestroyDescriptorSetLayout(hLayout);
    if (hPlane.IsValid())         device->DestroyBuffer(hPlane);
    if (hSphere.IsValid())        device->DestroyBuffer(hSphere);
    if (hCube.IsValid())          device->DestroyBuffer(hCube);
    device->DestroyPipeline(hPipe);
    device->DestroyShader(hShader);
    if (hShadowPipe.IsValid())    device->DestroyPipeline(hShadowPipe);
    if (hShadowShader.IsValid())  device->DestroyShader(hShadowShader);
    if (hShadowFBO.IsValid())     device->DestroyFramebuffer(hShadowFBO);
    if (hShadowRP.IsValid())      device->DestroyRenderPass(hShadowRP);
    if (hShadowSampler.IsValid()) device->DestroySampler(hShadowSampler);
    if (hShadowTex.IsValid())     device->DestroyTexture(hShadowTex);
    NkDeviceFactory::Destroy(device);
    ctx->Shutdown();
    window.Close();
    printf("[RHIFullDemo] Terminé.\n");
    return 0;
}