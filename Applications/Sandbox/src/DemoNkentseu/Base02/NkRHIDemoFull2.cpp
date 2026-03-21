// =============================================================================
// NkRHIDemoFull.cpp
// Démo RHI complète utilisant la couche NkIDevice (NKRenderer)
//
//   • Multi-backend : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//     sélectionnable via argument ligne de commande : --backend=opengl|vulkan|dx11|dx12|sw|metal
//   • Géométrie 3D : cube rotatif + sphère + plan (sol)
//   • Éclairage Phong directionnel (ambient + diffuse + specular)
//   • Shadow mapping (OpenGL uniquement — sampler2DShadow PCF 3x3)
//   • NKMath : NkVec3f, NkMat4f (column-major, OpenGL convention)
//
// Pipeline d'initialisation :
//   NkWindow → NkContextFactory → NkDeviceFactory → ressources RHI → boucle
// =============================================================================

// ── Détection plateforme ──────────────────────────────────────────────────────
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"

// ── Headers NKEngine ──────────────────────────────────────────────────────────
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkKeyboardEvent.h"
#include "NKWindow/Events/NkMouseEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKTime/NkTime.h"
#include "NKRenderer/RHI/Backend/Software/NkSoftwareDevice.h"

// ── RHI ──────────────────────────────────────────────────────────────────────
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Core/NkDeviceFactory.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"

// ── NKMath ───────────────────────────────────────────────────────────────────
#include "NKMath/NKMath.h"

// ── NKLogger ──────────────────────────────────────────────────────────────────
#include "NKLogger/NkLog.h"

// ── std ───────────────────────────────────────────────────────────────────────
#include <cstdio>
#include <cstring>
#include "NKContainers/Sequential/NkVector.h"

// ── SPIR-V précompilé pour Vulkan ─────────────────────────────────────────────
#include "NkRHIDemoFullVkSpv.inl"

namespace nkentseu {
    struct NkEntryState;
}

// =============================================================================
// Shaders GLSL (OpenGL 4.6)
// =============================================================================

// ── Shadow depth pass : position seule, matrix = lightVP * model ─────────────
static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;

// On utilise un UBO séparé pour la passe shadow (binding 0)
layout(std140, binding = 0) uniform ShadowUBO {
    mat4 model;
    mat4 view;       // non utilisé dans cette passe
    mat4 proj;       // non utilisé dans cette passe
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

void main() {
    // Transforme directement dans l'espace lumière
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
// Pas de sortie couleur — on écrit uniquement le depth buffer
void main() {}
)GLSL";

// ── Passe principale Phong + shadow mapping PCF ───────────────────────────────
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec4 vShadowCoord;  // coordonnées dans l'espace lumière (non-divisées)

void main() {
    vec4 worldPos     = ubo.model * vec4(aPos, 1.0);
    vWorldPos         = worldPos.xyz;

    // Matrice normale = transposée inverse de la partie 3x3 du model
    // (correct même pour les objets non-uniformément mis à l'échelle)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    vNormal           = normalize(normalMatrix * aNormal);

    vColor            = aColor;

    // Coordonnées shadow : espace lumière homogène, pas encore divisées par w
    vShadowCoord      = ubo.lightVP * worldPos;

    gl_Position       = ubo.proj * ubo.view * worldPos;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec4 vShadowCoord;   // coordonnées lumière homogènes

out vec4 fragColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

// sampler2DShadow avec compare mode LESS_OR_EQUAL
// (le sampler doit être configuré avec compareEnable=true, compareOp=LESS_EQUAL)
layout(binding = 1) uniform sampler2DShadow uShadowMap;

// ── Filtrage PCF 3x3 ─────────────────────────────────────────────────────────
float ShadowFactor(vec4 shadowCoord) {
    // Division perspective (projection orthogonale → w=1, mais on normalise quand même)
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;

    // En OpenGL, NDC z est dans [-1, 1]. On remet dans [0, 1] pour le lookup.
    // xy  : NDC [-1,1] → UV [0,1]
    // z   : NDC [-1,1] → profondeur [0,1] pour comparer avec la shadow map
    projCoords = projCoords * 0.5 + 0.5;

    // Rejeter les fragments hors du frustum lumière
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;  // hors frustum = pas d'ombre
    }

    // Bias adaptatif pour éviter le shadow acne
    // Plus la surface est perpendiculaire à la lumière, plus le bias est grand
    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);   // [0.0005 .. 0.005]

    projCoords.z -= bias;

    // Taille d'un texel en UV
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    // PCF 3×3 — texture() avec sampler2DShadow fait la comparaison hardware
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            // texture(sampler2DShadow, vec3(uv, compareDepth))
            // retourne 1.0 si le fragment est ÉCLAIRÉ, 0.0 si dans l'ombre
            shadow += texture(uShadowMap, vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    return shadow / 9.0;  // moyenne du PCF
}

void main() {
    vec3 N      = normalize(vNormal);
    vec3 L      = normalize(-ubo.lightDirW.xyz);
    vec3 V      = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H      = normalize(L + V);

    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 32.0);

    // shadow = 1.0 → éclairé, 0.0 → dans l'ombre
    float shadow = ShadowFactor(vShadowCoord);

    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = shadow * diff * vColor;
    vec3 specular = shadow * spec * vec3(0.4);

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";

// =============================================================================
// Shaders HLSL (DX11 / DX12)
// =============================================================================
static constexpr const char* kHLSL_VS = R"HLSL(
cbuffer UBO : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
};
struct VSIn  { float3 pos:POSITION; float3 norm:NORMAL; float3 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float3 wp:TEXCOORD0; float3 n:TEXCOORD1; float3 c:TEXCOORD2; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos,1));
    o.wp  = wp.xyz;
    o.n   = normalize(mul((float3x3)model, v.norm));
    o.c   = v.col;
    o.pos = mul(proj, mul(view, wp));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_PS = R"HLSL(
cbuffer UBO : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
};
struct PSIn { float4 pos:SV_Position; float3 wp:TEXCOORD0; float3 n:TEXCOORD1; float3 c:TEXCOORD2; };
float4 PSMain(PSIn i) : SV_Target {
    float3 N = normalize(i.n);
    float3 L = normalize(-lightDirW.xyz);
    float3 V = normalize(eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(N,H), 0.0), 32.0);
    float3 col = 0.15*i.c + diff*i.c + spec*0.4;
    return float4(col, 1.0);
}
)HLSL";

// =============================================================================
// Shaders MSL (Metal)
// =============================================================================
static constexpr const char* kMSL_Shaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UBO { float4x4 model,view,proj; float4 lightDirW,eyePosW; };
struct VSIn  { float3 pos [[attribute(0)]]; float3 norm [[attribute(1)]]; float3 col [[attribute(2)]]; };
struct VSOut { float4 pos [[position]]; float3 wp,n,c; };
vertex VSOut vmain(VSIn v [[stage_in]], constant UBO& u [[buffer(0)]]) {
    VSOut o;
    float4 wp = u.model * float4(v.pos,1);
    o.wp = wp.xyz;
    o.n  = normalize(float3x3(u.model[0].xyz,u.model[1].xyz,u.model[2].xyz) * v.norm);
    o.c  = v.col;
    o.pos = u.proj * u.view * wp;
    return o;
}
fragment float4 fmain(VSOut i [[stage_in]], constant UBO& u [[buffer(0)]]) {
    float3 N = normalize(i.n);
    float3 L = normalize(-u.lightDirW.xyz);
    float3 V = normalize(u.eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float diff = max(dot(N,L), 0.0f);
    float spec = pow(max(dot(N,H), 0.0f), 32.0f);
    float3 col = 0.15f*i.c + diff*i.c + spec*0.4f;
    return float4(col, 1.0f);
}
)MSL";

// =============================================================================
// UBO (std140-compatible, 16-byte aligned)
// Ordre : model(64) + view(64) + proj(64) + lightVP(64) + lightDirW(16) + eyePosW(16) = 288 bytes
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];   // matrice VP de la lumière pour le shadow mapping
    float lightDirW[4];  // direction lumière world space (xyz) + pad
    float eyePosW[4];    // position caméra world space (xyz) + pad
};

// =============================================================================
// Géométrie
// =============================================================================
using namespace nkentseu;
using namespace nkentseu::math;

struct Vtx3D { NkVec3f pos; NkVec3f normal; NkVec3f color; };

static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

// Cube (36 vertices, 6 faces × 2 triangles)
static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P = 0.5f, N = -0.5f;
    struct Face { float vx[4][3]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, 0, 0, 1},   // front  +Z
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, 0, 0,-1},   // back   -Z
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, 0,-1, 0},   // bottom -Y
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, 0, 1, 0},   // top    +Y
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},-1, 0, 0},   // left   -X
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, 1, 0, 0},   // right  +X
    };
    static const int idx[6] = {0,1,2, 0,2,3};
    NkVector<Vtx3D> v;
    v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]),
                        NkVec3f(f.nx,f.ny,f.nz),
                        NkVec3f(r,g,b)});
    return v;
}

// Sphère UV
static NkVector<Vtx3D> MakeSphere(int stacks=16, int slices=24,
                                    float r=0.2f, float g=0.55f, float b=0.9f) {
    NkVector<Vtx3D> v;
    const float pi = (float)NkPi;
    for (int i = 0; i < stacks; i++) {
        float phi0 = (float)i     / stacks * pi;
        float phi1 = (float)(i+1) / stacks * pi;
        for (int j = 0; j < slices; j++) {
            float th0 = (float)j     / slices * 2.f * pi;
            float th1 = (float)(j+1) / slices * 2.f * pi;
            auto mk = [&](float phi, float th) -> Vtx3D {
                float x = NkSin(phi)*NkCos(th);
                float y = NkCos(phi);
                float z = NkSin(phi)*NkSin(th);
                return {NkVec3f(x*.5f,y*.5f,z*.5f), NkVec3f(x,y,z), NkVec3f(r,g,b)};
            };
            Vtx3D a=mk(phi0,th0), b2=mk(phi0,th1), c=mk(phi1,th0), d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

// Plan sol
static NkVector<Vtx3D> MakePlane(float sz=3.f,
                                   float r=0.35f, float g=0.65f, float b=0.35f) {
    float h = sz * 0.5f;
    NkVector<Vtx3D> v;
    v.Reserve(6);
    // Triangle 1
    v.PushBack({NkVec3f(-h,0.f, h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f( h,0.f, h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f( h,0.f,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    // Triangle 2
    v.PushBack({NkVec3f(-h,0.f, h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f( h,0.f,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(-h,0.f,-h),NkVec3f(0,1,0),NkVec3f(r,g,b)});
    return v;
}

// =============================================================================
// Sélection du backend
// =============================================================================
static NkGraphicsApi ParseBackend(const nkentseu::NkVector<nkentseu::NkString>& args) {
    for (size_t i = 1; i < args.Size(); i++) {
        const nkentseu::NkString& arg = args[i];
        if (arg == "--backend=vulkan"  || arg == "-bvk")   return NkGraphicsApi::NK_API_VULKAN;
        if (arg == "--backend=dx11"    || arg == "-bdx11")  return NkGraphicsApi::NK_API_DIRECTX11;
        if (arg == "--backend=dx12"    || arg == "-bdx12")  return NkGraphicsApi::NK_API_DIRECTX12;
        if (arg == "--backend=metal"   || arg == "-bmtl")   return NkGraphicsApi::NK_API_METAL;
        if (arg == "--backend=sw"      || arg == "-bsw")    return NkGraphicsApi::NK_API_SOFTWARE;
        if (arg == "--backend=opengl"  || arg == "-bgl")    return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static NkContextDesc MakeContextDesc(NkGraphicsApi api) {
    NkContextDesc d;
    d.api = api;
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
            d = NkContextDesc::MakeOpenGLES(3, 0);
#else
            d.opengl = NkOpenGLDesc::Desktop46(false);
            d.opengl.runtime.autoLoadEntryPoints = true;
            d.opengl.swapInterval = NkGLSwapInterval::VSync;
#endif
            break;
        default:
            break;
    }
    return d;
}

static NkShaderDesc MakeShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "Phong3D";
    switch (api) {
        case NkGraphicsApi::NK_API_VULKAN:
            // SPIR-V précompilé depuis NkRHIDemoFullVkSpv.inl
            // Ce shader Phong n'échantillonne PAS de shadow map (pas de binding 1).
            // La shadow map Vulkan nécessiterait un SPIR-V séparé avec sampler shadow.
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkRHIFullDemoVertSpv,
                        (uint64)kVkRHIFullDemoVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkRHIFullDemoFragSpv,
                        (uint64)kVkRHIFullDemoFragSpvWordCount * sizeof(uint32));
            break;
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_VS, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_PS, "PSMain");
            break;
        case NkGraphicsApi::NK_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_Shaders, "vmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_Shaders, "fmain");
            break;
        default:
            // OpenGL et Software : GLSL avec shadow map intégrée (binding 1 = sampler2DShadow)
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Frag);
            break;
    }
    return sd;
}

// Construit le NkShaderDesc du shader shadow selon l'API.
// Retourne un NkShaderDesc valide uniquement pour les APIs qui supportent
// la shadow pass (OpenGL avec GLSL). Pour Vulkan, retourne un desc vide
// (hShadowShader sera invalide → hasShadowMap sera faux en Vulkan).
static NkShaderDesc MakeShadowShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "ShadowDepth";
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
            // Shadow pass GLSL : vertex transforme en espace lumière, frag vide
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_ShadowVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_ShadowFrag);
            break;
        case NkGraphicsApi::NK_API_VULKAN:
        // Vulkan : pas de SPIR-V shadow disponible ici — on retourne un desc vide.
        // Pour activer les ombres en Vulkan, il faudrait ajouter :
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,   kVkShadowVertSpv, kVkShadowVertSpvWordCount*4);
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT, kVkShadowFragSpv, kVkShadowFragSpvWordCount*4);
        // et les inclure dans NkRHIDemoFullVkSpv.inl
        default:
            break;
    }
    return sd;
}

// =============================================================================
// nkmain — point d'entrée
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {

    // ── Sélection backend ─────────────────────────────────────────────────────
    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    const char* apiName = NkGraphicsApiName(targetApi);
    logger.Info("[RHIFullDemo] Backend cible : {0}\n", apiName);

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NkRHI Full Demo — {0}", apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Info("[RHIFullDemo] Échec création fenêtre\n");
        return 1;
    }

    // ── Contexte graphique ────────────────────────────────────────────────────
    NkContextDesc ctxDesc = MakeContextDesc(targetApi);
    auto ctx = NkContextFactory::Create(window, ctxDesc);
    if (!ctx || !ctx->IsValid()) {
        logger.Info("[RHIFullDemo] Échec création contexte ({0})\n", apiName);
        window.Close();
        return 1;
    }

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkIDevice* device = NkDeviceFactory::Create(ctx);
    if (!device || !device->IsValid()) {
        logger.Info("[RHIFullDemo] Échec création NkIDevice ({0})\n", apiName);
        ctx->Shutdown();
        window.Close();
        return 1;
    }
    logger.Info("[RHIFullDemo] Device {0} initialisé. VRAM: {1} Mo\n",
                apiName, (unsigned long long)(device->GetCaps().vramBytes >> 20));

    // ── Dimensions swapchain ──────────────────────────────────────────────────
    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── Shader principal ──────────────────────────────────────────────────────
    NkShaderDesc shaderDesc = MakeShaderDesc(targetApi);
    NkShaderHandle hShader = device->CreateShader(shaderDesc);
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Échec compilation shader\n");
        NkDeviceFactory::Destroy(device);
        ctx->Shutdown();
        window.Close();
        return 1;
    }

    // ── Vertex Layout ─────────────────────────────────────────────────────────
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGpuFormat::NK_RGB32_FLOAT, 0,               "POSITION", 0)
        .AddAttribute(1, 0, NkGpuFormat::NK_RGB32_FLOAT, 3*sizeof(float),  "NORMAL",  0)
        .AddAttribute(2, 0, NkGpuFormat::NK_RGB32_FLOAT, 6*sizeof(float),  "COLOR",   0)
        .AddBinding(0, sizeof(Vtx3D));

    // ── Render Pass swapchain (récupéré une fois, stable entre les frames) ────
    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    // ── Shadow map ────────────────────────────────────────────────────────────
    // La shadow map est activée uniquement si l'API dispose d'un shader shadow
    // valide. Pour OpenGL : shader GLSL disponible. Pour Vulkan : nécessite
    // un SPIR-V shadow précompilé (non fourni ici → shadow désactivée en Vulkan).
    // hasShadowMap est fixé plus bas, APRÈS avoir tenté de compiler hShadowShader.
    static constexpr uint32 kShadowSize = 2048;

    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;

    // Compiler le shader shadow selon l'API. Retourne un handle invalide si
    // l'API ne dispose pas du bon bytecode (ex: Vulkan sans SPIR-V shadow).
    {
        NkShaderDesc shadowSd = MakeShadowShaderDesc(targetApi);
        if (!shadowSd.stages.IsEmpty()) {
            hShadowShader = device->CreateShader(shadowSd);
        }
        // Si hShadowShader est invalide (pas de bytecode shadow), la shadow map
        // sera désactivée automatiquement via hasShadowMap = false ci-dessous.
    }

    // La shadow map est active uniquement si le shader shadow a pu être compilé
    const bool hasShadowMap = hShadowShader.IsValid();

    if (hasShadowMap) {
        logger.Info("[RHIFullDemo] Shadow map activée ({0}x{0})\n", kShadowSize);
    } else {
        logger.Info("[RHIFullDemo] Shadow map désactivée (pas de shader shadow pour {0})\n", apiName);
    }

    // ── Descriptor Set Layout (binding 0 = UBO, binding 1 = shadow sampler) ──
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER,          NkShaderStage::NK_ALL_GRAPHICS);
    if (hasShadowMap)
        layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);

    // ── Pipeline principal ────────────────────────────────────────────────────
    NkGraphicsPipelineDesc pipeDesc;
    pipeDesc.shader       = hShader;
    pipeDesc.vertexLayout = vtxLayout;
    pipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pipeDesc.rasterizer   = NkRasterizerDesc::Default();
    pipeDesc.depthStencil = NkDepthStencilDesc::Default();
    pipeDesc.blend        = NkBlendDesc::Opaque();
    pipeDesc.renderPass   = hRP;
    pipeDesc.debugName    = "PipelinePhong3D";
    if (hLayout.IsValid())
        pipeDesc.descriptorSetLayouts.PushBack(hLayout);

    NkPipelineHandle hPipe = device->CreateGraphicsPipeline(pipeDesc);
    if (!hPipe.IsValid()) {
        logger.Info("[RHIFullDemo] Échec création pipeline\n");
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        ctx->Shutdown();
        window.Close();
        return 1;
    }

    // ── Géométrie ─────────────────────────────────────────────────────────────
    auto cubeVerts   = MakeCube();
    auto sphereVerts = MakeSphere();
    auto planeVerts  = MakePlane();

    auto uploadVBO = [&](const NkVector<Vtx3D>& verts) -> NkBufferHandle {
        uint64 sz = verts.Size() * sizeof(Vtx3D);
        return device->CreateBuffer(NkBufferDesc::Vertex(sz, verts.Begin()));
    };

    NkBufferHandle hCube   = uploadVBO(cubeVerts);
    NkBufferHandle hSphere = uploadVBO(sphereVerts);
    NkBufferHandle hPlane  = uploadVBO(planeVerts);

    // ── Uniform Buffers ───────────────────────────────────────────────────────
    // 3 UBO : un par objet (cube, sphère, plan)
    // Séparés pour éviter les conflits d'écriture entre passe shadow et passe principale
    NkBufferHandle hUBO[3];
    for (int i = 0; i < 3; i++) {
        hUBO[i] = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
        if (!hUBO[i].IsValid())
            logger.Info("[RHIFullDemo] Échec création UBO[{0}]\n", i);
    }

    // ── Descriptor Sets ───────────────────────────────────────────────────────
    // 3 descriptor sets : un par objet
    // Chaque set lie son propre UBO (binding 0) + la shadow texture (binding 1)
    NkDescSetHandle hDescSet[3];
    for (int i = 0; i < 3; i++) {
        hDescSet[i] = device->AllocateDescriptorSet(hLayout);
        if (hLayout.IsValid() && hDescSet[i].IsValid() && hUBO[i].IsValid()) {
            NkDescriptorWrite w{};
            w.set          = hDescSet[i];
            w.binding      = 0;
            w.type         = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer       = hUBO[i];
            w.bufferOffset = 0;
            w.bufferRange  = sizeof(UboData);
            device->UpdateDescriptorSets(&w, 1);
        }
    }

    // ── Ressources shadow ─────────────────────────────────────────────────────
    if (hasShadowMap) {
        // Texture depth dédiée pour la shadow map
        // IMPORTANT : bindFlags doit inclure NK_SHADER_RESOURCE pour pouvoir la
        // lire en shader dans la passe principale
        NkTextureDesc shadowTexDesc = NkTextureDesc::DepthStencil(
            kShadowSize, kShadowSize,
            NkGpuFormat::NK_D32_FLOAT,
            NkSampleCount::NK_S1);
        // S'assurer que la texture peut aussi être lue comme texture de shader
        shadowTexDesc.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
        hShadowTex = device->CreateTexture(shadowTexDesc);

        // Sampler shadow :
        // - compare mode LESS_EQUAL (hardware PCF via sampler2DShadow)
        // - clamp to border avec couleur blanche = hors frustum = éclairé
        // - pas de mipmaps
        NkSamplerDesc shadowSamplerDesc = NkSamplerDesc::Shadow();
        shadowSamplerDesc.magFilter     = NkFilter::NK_LINEAR;  // hardware bilinear PCF
        shadowSamplerDesc.minFilter     = NkFilter::NK_LINEAR;
        shadowSamplerDesc.mipFilter     = NkMipFilter::NK_NONE;
        shadowSamplerDesc.minLod        = 0.f;
        shadowSamplerDesc.maxLod        = 0.f;  // pas de mipmaps
        hShadowSampler = device->CreateSampler(shadowSamplerDesc);

        // Render pass depth-only (shadow)
        // Pas de color attachment, juste depth
        hShadowRP = device->CreateRenderPass(NkRenderPassDesc::ShadowMap());

        // Framebuffer shadow : depth uniquement
        NkFramebufferDesc fboD{};
        fboD.renderPass      = hShadowRP;
        fboD.depthAttachment = hShadowTex;
        fboD.width           = kShadowSize;
        fboD.height          = kShadowSize;
        fboD.debugName       = "ShadowFBO";
        hShadowFBO = device->CreateFramebuffer(fboD);

        // Shader shadow — déjà compilé avant ce bloc (voir MakeShadowShaderDesc)
        // hShadowShader est déjà valide ici (sinon hasShadowMap serait false)

        // Vertex layout shadow : position seule (stride = sizeof(Vtx3D) pour
        // réutiliser les mêmes VBOs, mais on ne lit que les 12 premiers bytes)
        NkVertexLayout shadowVtxLayout;
        shadowVtxLayout
            .AddAttribute(0, 0, NkGpuFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
            .AddBinding(0, sizeof(Vtx3D));  // stride complet pour compatibilité

        // Pipeline shadow :
        // - front face culling pour éviter le shadow acne (Peter Panning tradeoff)
        // - depth bias pour décaler légèrement les surfaces
        // - pas de color output
        NkGraphicsPipelineDesc shadowPipeDesc{};
        shadowPipeDesc.shader       = hShadowShader;
        shadowPipeDesc.vertexLayout = shadowVtxLayout;
        shadowPipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        shadowPipeDesc.rasterizer   = NkRasterizerDesc::ShadowMap();
        shadowPipeDesc.depthStencil = NkDepthStencilDesc::Default();
        shadowPipeDesc.blend        = NkBlendDesc::Opaque();
        shadowPipeDesc.renderPass   = hShadowRP;
        shadowPipeDesc.debugName    = "ShadowPipeline";
        // Le pipeline shadow utilise le même layout de descripteur (binding 0 = UBO)
        if (hLayout.IsValid())
            shadowPipeDesc.descriptorSetLayouts.PushBack(hLayout);
        hShadowPipe = device->CreateGraphicsPipeline(shadowPipeDesc);

        // Lier la shadow texture (binding 1) dans les 3 descriptor sets
        // IMPORTANT : textureLayout = NK_DEPTH_READ pour indiquer que la
        // texture sera lue (layout SHADER_READ_ONLY / DEPTH_READ)
        for (int i = 0; i < 3; i++) {
            if (hDescSet[i].IsValid() && hShadowTex.IsValid() && hShadowSampler.IsValid()) {
                NkDescriptorWrite sw{};
                sw.set           = hDescSet[i];
                sw.binding       = 1;
                sw.type          = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture       = hShadowTex;
                sw.sampler       = hShadowSampler;
                // NK_DEPTH_READ indique que la texture est utilisée en lecture
                // depuis un shader (layout DEPTH_STENCIL_READ_ONLY_OPTIMAL sur Vulkan,
                // SHADER_READ_ONLY_OPTIMAL en pratique pour OpenGL)
                sw.textureLayout = NkResourceState::NK_DEPTH_READ;
                device->UpdateDescriptorSets(&sw, 1);
            }
        }
    }

    // ── Callbacks CPU pour le backend Software ────────────────────────────────
    if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);
        NkSWShader* sw = swDev->GetShader(hShader.id);
        if (sw) {
            sw->vertFn = [](const void* vdata, uint32, const void* udata) -> NkSWVertex {
                const Vtx3D*   v   = static_cast<const Vtx3D*>(vdata);
                const UboData* ubo = static_cast<const UboData*>(udata);
                if (!ubo) { NkSWVertex z{}; z.position={0,0,0,1}; return z; }
                auto mv4 = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                    return NkVec4f(m[0]*x+m[4]*y+m[ 8]*z+m[12]*w,
                                   m[1]*x+m[5]*y+m[ 9]*z+m[13]*w,
                                   m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                   m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
                NkVec4f wp = mv4(ubo->model, v->pos.x, v->pos.y, v->pos.z, 1.f);
                NkVec4f vp = mv4(ubo->view,  wp.x, wp.y, wp.z, wp.w);
                NkVec4f cp = mv4(ubo->proj,  vp.x, vp.y, vp.z, vp.w);
                NkSWVertex out;
                out.position = cp;
                out.normal   = v->normal;
                out.color    = { v->color.r, v->color.g, v->color.b, 1.f };
                out.attrs[0] = wp.x; out.attrs[1] = wp.y; out.attrs[2] = wp.z;
                return out;
            };
            sw->fragFn = [](const NkSWVertex& frag, const void* udata, const void*) -> NkSWColor {
                const UboData* ubo = static_cast<const UboData*>(udata);
                float nx=frag.normal.x, ny=frag.normal.y, nz=frag.normal.z;
                float nl = NkSqrt(nx*nx+ny*ny+nz*nz);
                if (nl>0.f){nx/=nl;ny/=nl;nz/=nl;}
                float lx=-ubo->lightDirW[0], ly=-ubo->lightDirW[1], lz=-ubo->lightDirW[2];
                float ll = NkSqrt(lx*lx+ly*ly+lz*lz);
                if (ll>0.f){lx/=ll;ly/=ll;lz/=ll;}
                float wx=frag.attrs[0], wy=frag.attrs[1], wz=frag.attrs[2];
                float vx=ubo->eyePosW[0]-wx, vy=ubo->eyePosW[1]-wy, vz=ubo->eyePosW[2]-wz;
                float vl2=NkSqrt(vx*vx+vy*vy+vz*vz);
                if (vl2>0.f){vx/=vl2;vy/=vl2;vz/=vl2;}
                float hx=(lx+vx)*.5f, hy=(ly+vy)*.5f, hz=(lz+vz)*.5f;
                float hl=NkSqrt(hx*hx+hy*hy+hz*hz);
                if (hl>0.f){hx/=hl;hy/=hl;hz/=hl;}
                float diff=nx*lx+ny*ly+nz*lz; if(diff<0.f)diff=0.f;
                float sp=nx*hx+ny*hy+nz*hz;   if(sp  <0.f)sp  =0.f;
                float spec=NkPow(sp,32.f);
                auto sat=[](float v){return v<0.f?0.f:(v>1.f?1.f:v);};
                return {sat(.15f*frag.color.r+diff*frag.color.r+spec*.4f),
                        sat(.15f*frag.color.g+diff*frag.color.g+spec*.4f),
                        sat(.15f*frag.color.b+diff*frag.color.b+spec*.4f), 1.f};
            };
        }
    }

    // ── Command Buffer ────────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd) {
        logger.Info("[RHIFullDemo] Échec création command buffer\n");
        NkDeviceFactory::Destroy(device);
        ctx->Shutdown();
        window.Close();
        return 1;
    }

    // ── État de la simulation ─────────────────────────────────────────────────
    bool  running    = true;
    float rotAngle   = 0.f;
    float camYaw     = 0.f;
    float camPitch   = 20.f;
    float camDist    = 4.f;
    float lightYaw   = -45.f;
    float lightPitch = -30.f;
    bool  keys[512]  = {};

    NkClock clock;
    NkEventSystem& events = NkEvents();

    // ── Callbacks événements ──────────────────────────────────────────────────
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = true;
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (uint32)e->GetWidth();
        H = (uint32)e->GetHeight();
        device->OnResize(W, H);
    });

    logger.Info("[RHIFullDemo] Boucle principale. ESC=quitter, WASD=caméra, Flèches=lumière\n");

    // =========================================================================
    // Boucle principale
    // =========================================================================
    while (running) {
        events.PollEvents();
        if (!running) break;

        // ── Delta time ────────────────────────────────────────────────────────
        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        // ── Contrôles clavier ─────────────────────────────────────────────────
        const float camSpd = 60.f, lightSpd = 90.f;
        if (keys[(uint32)NkKey::NK_A]) camYaw   -= camSpd * dt;
        if (keys[(uint32)NkKey::NK_D]) camYaw   += camSpd * dt;
        if (keys[(uint32)NkKey::NK_W]) camPitch += camSpd * dt;
        if (keys[(uint32)NkKey::NK_S]) camPitch -= camSpd * dt;
        if (keys[(uint32)NkKey::NK_LEFT])  lightYaw   -= lightSpd * dt;
        if (keys[(uint32)NkKey::NK_RIGHT]) lightYaw   += lightSpd * dt;
        if (keys[(uint32)NkKey::NK_UP])    lightPitch += lightSpd * dt;
        if (keys[(uint32)NkKey::NK_DOWN])  lightPitch -= lightSpd * dt;
        camPitch = NkClamp(camPitch, -80.f, 80.f);

        rotAngle += 45.f * dt;

        // ── Matrices de transformation ────────────────────────────────────────
        float aspect = (H > 0) ? (float)W / (float)H : 1.f;

        // Caméra orbitale
        float eyeX = camDist * NkCos(NkToRadians(camPitch)) * NkSin(NkToRadians(camYaw));
        float eyeY = camDist * NkSin(NkToRadians(camPitch));
        float eyeZ = camDist * NkCos(NkToRadians(camPitch)) * NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX, eyeY, eyeZ);
        NkVec3f center(0.f, 0.f, 0.f);
        NkVec3f up(0.f, 1.f, 0.f);

        NkMat4f matView = NkMat4f::LookAt(eye, center, up);
        NkMat4f matProj = NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 100.f);

        // Direction lumière
        float lx = NkCos(NkToRadians(lightPitch)) * NkSin(NkToRadians(lightYaw));
        float ly = NkSin(NkToRadians(lightPitch));
        float lz = NkCos(NkToRadians(lightPitch)) * NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx, ly, lz);

        // Matrice de la lumière (orthogonale, couvre les 3 objets)
        // La position de la lumière est opposée à sa direction, à distance suffisante
        NkVec3f lightPos = NkVec3f(-lightDir.x * 10.f,
                                    -lightDir.y * 10.f,
                                    -lightDir.z * 10.f);

        // Éviter un up vector colinéaire avec la direction lumière
        NkVec3f lightUp = (NkFabs(lightDir.y) > 0.9f)
                          ? NkVec3f(1.f, 0.f, 0.f)
                          : NkVec3f(0.f, 1.f, 0.f);

        NkMat4f matLightView = NkMat4f::LookAt(lightPos, center, lightUp);

        // Frustum orthogonal assez large pour couvrir toute la scène visible
        // (-5..5 en X/Y, 1..20 en profondeur)
        NkMat4f matLightProj = NkMat4f::Orthogonal(
            NkVec2f(-5.f, -5.f),
            NkVec2f( 5.f,  5.f),
            1.f, 20.f);

        NkMat4f matLightVP = matLightProj * matLightView;

        // ── Frame ─────────────────────────────────────────────────────────────
        NkFrameContext frame;
        device->BeginFrame(frame);

        if (W == 0 || H == 0) {
            device->EndFrame(frame);
            continue;
        }

        // Récupérer le framebuffer courant APRÈS BeginFrame (index mis à jour)
        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();

        cmd->Reset();
        cmd->Begin();

        // =====================================================================
        // Passe 1 : Shadow map (depth-only, POV de la lumière)
        // =====================================================================
        if (hasShadowMap && hShadowFBO.IsValid() && hShadowPipe.IsValid()) {

            NkRect2D shadowArea{0, 0, (int32)kShadowSize, (int32)kShadowSize};
            cmd->BeginRenderPass(hShadowRP, hShadowFBO, shadowArea);

            NkViewport svp{0.f, 0.f, (float)kShadowSize, (float)kShadowSize, 0.f, 1.f};
            cmd->SetViewport(svp);
            cmd->SetScissor(shadowArea);
            cmd->BindGraphicsPipeline(hShadowPipe);

            // Pour la passe shadow, on remplit l'UBO avec :
            //   model  = matrice de chaque objet
            //   lightVP = matrice VP de la lumière
            // Les autres champs (view, proj, lightDir, eyePos) ne sont pas
            // utilisés par le vertex shader shadow, mais on les remplit quand même
            // pour éviter les valeurs aléatoires.

            // Objet 0 : Cube rotatif
            {
                NkMat4f mm = NkMat4f::RotationY(NkAngle(rotAngle))
                           * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));
                UboData su{};
                Mat4ToArray(mm,         su.model);
                Mat4ToArray(matLightVP, su.lightVP);
                // view/proj = identité (non utilisés dans ce shader)
                NkMat4f identity = NkMat4f::Identity();
                Mat4ToArray(identity, su.view);
                Mat4ToArray(identity, su.proj);
                device->WriteBuffer(hUBO[0], &su, sizeof(su));
                if (hDescSet[0].IsValid()) cmd->BindDescriptorSet(hDescSet[0], 0);
                cmd->BindVertexBuffer(0, hCube);
                cmd->Draw((uint32)cubeVerts.Size());
            }

            // Objet 1 : Sphère
            {
                NkMat4f mm = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
                UboData su{};
                Mat4ToArray(mm,         su.model);
                Mat4ToArray(matLightVP, su.lightVP);
                NkMat4f identity = NkMat4f::Identity();
                Mat4ToArray(identity, su.view);
                Mat4ToArray(identity, su.proj);
                device->WriteBuffer(hUBO[1], &su, sizeof(su));
                if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
                cmd->BindVertexBuffer(0, hSphere);
                cmd->Draw((uint32)sphereVerts.Size());
            }

            // Objet 2 : Plan sol
            // IMPORTANT : le plan reçoit des ombres mais ne doit PAS être exclu
            // du rendu shadow — ses propres surfaces pourraient projeter sur
            // d'autres géométries. On le passe quand même dans la shadow pass.
            {
                NkMat4f mm = NkMat4f::Translation(NkVec3f(0.f, -0.65f, 0.f));
                UboData su{};
                Mat4ToArray(mm,         su.model);
                Mat4ToArray(matLightVP, su.lightVP);
                NkMat4f identity = NkMat4f::Identity();
                Mat4ToArray(identity, su.view);
                Mat4ToArray(identity, su.proj);
                device->WriteBuffer(hUBO[2], &su, sizeof(su));
                if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
                cmd->BindVertexBuffer(0, hPlane);
                cmd->Draw((uint32)planeVerts.Size());
            }

            cmd->EndRenderPass();

            // ── Barrière explicite shadow → shader read ───────────────────────
            // Nécessaire pour Vulkan : après EndRenderPass, la shadow map est
            // en SHADER_READ_ONLY_OPTIMAL (finalLayout du render pass), MAIS
            // il faut quand même une barrière de pipeline pour que le fragment
            // shader de la passe suivante voie bien les données écrites.
            //
            // Pour OpenGL, cmd->Barrier() émet glMemoryBarrier — nécessaire aussi
            // pour que la texture depth soit visible comme sampler2DShadow.
            if (hShadowTex.IsValid()) {
                NkTextureBarrier shadowBarrier{};
                shadowBarrier.texture     = hShadowTex;
                shadowBarrier.stateBefore = NkResourceState::NK_DEPTH_WRITE;
                shadowBarrier.stateAfter  = NkResourceState::NK_DEPTH_READ;
                shadowBarrier.srcStage    = NkPipelineStage::NK_LATE_FRAGMENT;
                shadowBarrier.dstStage    = NkPipelineStage::NK_FRAGMENT_SHADER;
                cmd->Barrier(nullptr, 0, &shadowBarrier, 1);
            }
        }

        // =====================================================================
        // Passe 2 : Rendu principal (Phong + shadow)
        // =====================================================================
        NkRect2D area{0, 0, (int32)W, (int32)H};
        cmd->BeginRenderPass(hRP, hFBO, area);

        NkViewport vp{0.f, 0.f, (float)W, (float)H, 0.f, 1.f};
        cmd->SetViewport(vp);
        cmd->SetScissor(area);
        cmd->BindGraphicsPipeline(hPipe);

        // Remplit un UboData complet pour la passe principale
        auto fillUboMain = [&](UboData& ubo, const NkMat4f& model) {
            Mat4ToArray(model,      ubo.model);
            Mat4ToArray(matView,    ubo.view);
            Mat4ToArray(matProj,    ubo.proj);
            Mat4ToArray(matLightVP, ubo.lightVP);
            ubo.lightDirW[0] = lightDir.x;  ubo.lightDirW[1] = lightDir.y;
            ubo.lightDirW[2] = lightDir.z;  ubo.lightDirW[3] = 0.f;
            ubo.eyePosW[0]   = eye.x;        ubo.eyePosW[1]   = eye.y;
            ubo.eyePosW[2]   = eye.z;        ubo.eyePosW[3]   = 0.f;
        };

        // Cube rotatif
        {
            NkMat4f matModel = NkMat4f::RotationY(NkAngle(rotAngle))
                             * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));
            UboData ubo{};
            fillUboMain(ubo, matModel);
            device->WriteBuffer(hUBO[0], &ubo, sizeof(ubo));
            if (hDescSet[0].IsValid()) cmd->BindDescriptorSet(hDescSet[0], 0);
            cmd->BindVertexBuffer(0, hCube);
            cmd->Draw((uint32)cubeVerts.Size());
        }

        // Sphère
        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
            UboData ubo{};
            fillUboMain(ubo, matModel);
            device->WriteBuffer(hUBO[1], &ubo, sizeof(ubo));
            if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
            cmd->BindVertexBuffer(0, hSphere);
            cmd->Draw((uint32)sphereVerts.Size());
        }

        // Plan sol — reçoit les ombres du cube et de la sphère
        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(0.f, -0.65f, 0.f));
            UboData ubo{};
            fillUboMain(ubo, matModel);
            device->WriteBuffer(hUBO[2], &ubo, sizeof(ubo));
            if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
            cmd->BindVertexBuffer(0, hPlane);
            cmd->Draw((uint32)planeVerts.Size());
        }

        cmd->EndRenderPass();
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // =========================================================================
    // Nettoyage
    // =========================================================================
    device->WaitIdle();

    device->DestroyCommandBuffer(cmd);
    for (int i = 0; i < 3; i++)
        if (hDescSet[i].IsValid()) device->FreeDescriptorSet(hDescSet[i]);
    if (hLayout.IsValid())       device->DestroyDescriptorSetLayout(hLayout);
    for (int i = 0; i < 3; i++)
        if (hUBO[i].IsValid())   device->DestroyBuffer(hUBO[i]);
    if (hPlane.IsValid())        device->DestroyBuffer(hPlane);
    if (hSphere.IsValid())       device->DestroyBuffer(hSphere);
    if (hCube.IsValid())         device->DestroyBuffer(hCube);
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

    printf("[RHIFullDemo] Terminé proprement.\n");
    return 0;
}