// =============================================================================
// NkRHIDemoFull.cpp
// Démo RHI complète utilisant la couche NkIDevice (NKRenderer)
//
//   • Multi-backend : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//     sélectionnable via argument ligne de commande : --backend=opengl|vulkan|dx11|dx12|sw|metal
//   • Géométrie 3D : cube rotatif + sphère + plan (sol)
//   • Éclairage Phong directionnel (ambient + diffuse + specular)
//   • NKMath : NkVec3f, NkMat4f (column-major, OpenGL convention)
//   • GLSL embarqué pour OpenGL; HLSL embarqué pour DirectX; MSL pour Metal
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
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKTime/NkTime.h"
// ── Software backend (direct access to programmable shader callbacks) ─────────
#include "NKRenderer/RHI/Backend/Software/NkSoftwareDevice.h"

// ── RHI ──────────────────────────────────────────────────────────────────────
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Core/NkDeviceFactory.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"

// ── NKMath ───────────────────────────────────────────────────────────────────
#include "NKMath/NKMath.h"

// ── NKLogger ───────────────────────────────────────────────────────────────────
#include "NKLogger/NkLog.h"

// ── std ───────────────────────────────────────────────────────────────────────
#include <cstdio>
#include <cstring>
// ── Containers (custom, STL-free) ─────────────────────────────────────────────
#include "NKContainers/Sequential/NkVector.h"

// ── SPIR-V précompilé pour Vulkan ─────────────────────────────────────────────
#include "NkRHIDemoFullVkSpv.inl"

// ── NkAppData (point d'entrée) ────────────────────────────────────────────────
namespace nkentseu {
    struct NkEntryState;
}

// =============================================================================
// Shaders GLSL (OpenGL 4.6 / ES 3.0)
// =============================================================================

// UBO commun à tous les shaders GLSL (std140 — 288 bytes)
#define GLSL_UBO_BLOCK \
"layout(std140, binding = 0) uniform UBO {\n" \
"    mat4 model;\n" \
"    mat4 view;\n" \
"    mat4 proj;\n" \
"    mat4 lightVP;\n" \
"    vec4 lightDirW;\n" \
"    vec4 eyePosW;\n" \
"} ubo;\n"

// ── Shadow depth pass (binding=0 UBO, position seule) ────────────────────────
static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(std140, binding = 0) uniform UBO {
    mat4 model; mat4 view; mat4 proj; mat4 lightVP; vec4 lightDirW; vec4 eyePosW;
} ubo;
void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
void main() {}
)GLSL";

// ── Passe principale Phong + shadow mapping ───────────────────────────────────
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;     // matrice vue-projection lumière
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec4 vShadowCoord;

void main() {
    vec4 wp       = ubo.model * vec4(aPos, 1.0);
    vWorldPos     = wp.xyz;
    vNormal       = normalize(mat3(transpose(inverse(ubo.model))) * aNormal);
    vColor        = aColor;
    vShadowCoord  = ubo.lightVP * wp;
    gl_Position   = ubo.proj * ubo.view * wp;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec4 vShadowCoord;
out vec4 fragColor;

layout(std140, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

layout(binding = 1) uniform sampler2DShadow uShadowMap;

float ShadowFactor() {
    vec3 proj = vShadowCoord.xyz / vShadowCoord.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float bias = max(0.005 * (1.0 - dot(normalize(vNormal), normalize(-ubo.lightDirW.xyz))), 0.001);
    float shadow = 0.0;
    vec2 sz = vec2(textureSize(uShadowMap, 0));
    vec2 texel = 1.0 / sz;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            shadow += texture(uShadowMap, vec3(proj.xy + vec2(x,y)*texel, proj.z - bias));
    return shadow / 9.0;
}

void main() {
    vec3 N      = normalize(vNormal);
    vec3 L      = normalize(-ubo.lightDirW.xyz);
    vec3 V      = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H      = normalize(L + V);
    float diff  = max(dot(N, L), 0.0);
    float spec  = pow(max(dot(N, H), 0.0), 32.0);
    float shadow= ShadowFactor();

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
    float4x4 lightVP;   // doit correspondre à UboData C++ (offset 192)
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
    float4x4 lightVP;   // doit correspondre à UboData C++ (offset 192)
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
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];  // matrice vue-projection lumière (shadow map) — std140 offset 192
    float lightDirW[4]; // xyz + pad
    float eyePosW[4];   // xyz + pad
};

// =============================================================================
// Géométrie utilitaires
// =============================================================================
using namespace nkentseu;
using namespace nkentseu::math;

struct Vtx3D { NkVec3f pos; NkVec3f normal; NkVec3f color; };

// Remplit mat[16] depuis NkMat4f (column-major, layout identique)
static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

// Cube (36 vertices, pas d'IBO pour simplifier)
static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    // 6 faces × 4 coins → 2 triangles chacune
    static const float P = 0.5f, N = -0.5f;
    // pos (3), normal (3)
    struct Face { float vx[4][3]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, 0,0, 1},  // front
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, 0,0,-1},  // back
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, 0,-1,0},  // bottom
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, 0, 1,0},  // top
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},-1,0,0},   // left
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, 1,0,0},   // right
    };
    static const int idx[6] = {0,1,2,0,2,3};
    NkVector<Vtx3D> v;
    v.Reserve(36);
    for (auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]), NkVec3f(f.nx,f.ny,f.nz), NkVec3f(r,g,b)});
    return v;
}

// Sphère UV (stacks × slices)
static NkVector<Vtx3D> MakeSphere(int stacks=16, int slices=24, float r=0.2f, float g=0.55f, float b=0.9f) {
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
                return {NkVec3f(x*.5f, y*.5f, z*.5f), NkVec3f(x, y, z), NkVec3f(r, g, b)};
            };
            Vtx3D a=mk(phi0,th0), b2=mk(phi0,th1), c=mk(phi1,th0), d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

// Plan sol (2 triangles)
static NkVector<Vtx3D> MakePlane(float sz=3.f, float r=0.35f, float g=0.65f, float b=0.35f) {
    float h = sz * 0.5f;
    NkVector<Vtx3D> v;
    v.Reserve(6);
    v.PushBack({NkVec3f(-h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f( h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f( h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(-h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f( h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f(-h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)});
    return v;
}

// =============================================================================
// Sélection du backend depuis les arguments NkEntryState
// =============================================================================
static NkGraphicsApi ParseBackend(const nkentseu::NkVector<nkentseu::NkString>& args) {
    for (size_t i = 1; i < args.Size(); i++) {
        const nkentseu::NkString& arg = args[i];
        if (arg == "--backend=vulkan"  || arg == "-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (arg == "--backend=dx11"    || arg == "-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (arg == "--backend=dx12"    || arg == "-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (arg == "--backend=metal"   || arg == "-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (arg == "--backend=sw"      || arg == "-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (arg == "--backend=opengl"  || arg == "-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL; // défaut
}

// =============================================================================
// Construit le NkContextDesc selon l'API demandée
// =============================================================================
static NkContextDesc MakeContextDesc(NkGraphicsApi api) {
    NkContextDesc d;
    d.api = api;
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
            d = NkContextDesc::MakeOpenGLES(3, 0);
#else
            d.opengl = NkOpenGLDesc::Desktop46(false);
            d.opengl.runtime.autoLoadEntryPoints = true; // charge GLAD avant NkIDevice
            d.opengl.swapInterval = NkGLSwapInterval::VSync;
#endif
            break;
        default:
            break;
    }
    return d;
}

// =============================================================================
// Construit le NkShaderDesc selon l'API
// =============================================================================
static NkShaderDesc MakeShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "Phong3D";
    switch (api) {
        case NkGraphicsApi::NK_API_VULKAN:
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
        default: // OpenGL + Software fallback GLSL
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Frag);
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
    logger.Info("[RHIFullDemo] Device {0} initialisé. VRAM: {1} Mo\n", apiName, (unsigned long long)(device->GetCaps().vramBytes >> 20));

    // ── Dimensions swapchain ──────────────────────────────────────────────────
    uint32 W = device->GetSwapchainWidth();
    
    uint32 H = device->GetSwapchainHeight();
    

    // ── Shader ───────────────────────────────────────────────────────────────
    NkShaderDesc shaderDesc = MakeShaderDesc(targetApi);
    
    NkShaderHandle hShader = device->CreateShader(shaderDesc);
    
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Échec compilation shader\n");
        NkDeviceFactory::Destroy(device);
        ctx->Shutdown();
        window.Close();
        return 1;
    }
    

    // ── Vertex Layout (pos:float3 + normal:float3 + color:float3) ─────────────
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGpuFormat::NK_RGB32_FLOAT, 0,           "POSITION", 0)
        .AddAttribute(1, 0, NkGpuFormat::NK_RGB32_FLOAT, 3*sizeof(float), "NORMAL", 0)
        .AddAttribute(2, 0, NkGpuFormat::NK_RGB32_FLOAT, 6*sizeof(float), "COLOR",  0)
        .AddBinding(0, sizeof(Vtx3D));

    // ── Render Pass ───────────────────────────────────────────────────────────
    
    NkRenderPassHandle  hRP  = device->GetSwapchainRenderPass();
    
    NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();
    

    // ── Shadow map (OpenGL uniquement — shaders GLSL avec sampler2DShadow) ────
    static constexpr uint32 kShadowSize = 1024;
    const bool hasShadowMap = (targetApi == NkGraphicsApi::NK_API_OPENGL);

    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;

    // ── Descriptor Set Layout (créé AVANT le pipeline pour Vulkan) ───────────
    // binding 0 = UBO, binding 1 = shadow sampler si dispo
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER,          NkShaderStage::NK_ALL_GRAPHICS);
    if (hasShadowMap)
        layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);
    

    // ── Pipeline ──────────────────────────────────────────────────────────────
    NkGraphicsPipelineDesc pipeDesc;
    pipeDesc.shader       = hShader;
    pipeDesc.vertexLayout = vtxLayout;
    pipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pipeDesc.rasterizer   = NkRasterizerDesc::Default();
    pipeDesc.depthStencil = NkDepthStencilDesc::Default();
    pipeDesc.blend        = NkBlendDesc::Opaque();
    pipeDesc.renderPass   = hRP;
    pipeDesc.debugName    = "PipelinePhong3D";
    // Vulkan : le pipeline layout doit connaître le descriptor set layout
    
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
    

    auto uploadVBO = [&](const NkVector<Vtx3D>& v) -> NkBufferHandle {
    logger.Info("00");
        uint64 sz = v.Size() * sizeof(Vtx3D);
    logger.Info("01");
        NkBufferHandle h = device->CreateBuffer(NkBufferDesc::Vertex(sz, v.Begin()));
    logger.Info("02");
        return h;
    };

    
    NkBufferHandle hCube   = uploadVBO(cubeVerts);
    
    NkBufferHandle hSphere = uploadVBO(sphereVerts);
    
    NkBufferHandle hPlane  = uploadVBO(planeVerts);
    

    // ── Uniform Buffers (3 — un par objet pour éviter les conflits d'écriture immédiate) ──
    // WriteBuffer est immédiat mais Draw est différé : 3 UBO séparés évite l'écrasement
    NkBufferHandle hUBO[3];
    for (int i = 0; i < 3; i++) {
    
        hUBO[i] = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
    
        if (!hUBO[i].IsValid())
            printf("[RHIFullDemo] Échec création UBO[%d]\n", i);
    }
    

    // ── Descriptor Sets (3 — un par objet, chacun lié à son propre UBO) ────────

    NkDescSetHandle hDescSet[3];
    
    for (int i = 0; i < 3; i++) {
    
        hDescSet[i] = device->AllocateDescriptorSet(hLayout);
    
        if (hLayout.IsValid() && hDescSet[i].IsValid() && hUBO[i].IsValid()) {
    
            NkDescriptorWrite w;
            w.set          = hDescSet[i];
            w.binding      = 0;
            w.type         = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer       = hUBO[i];
            w.bufferOffset = 0;
            w.bufferRange  = sizeof(UboData);
    
            device->UpdateDescriptorSets(&w, 1);
    
        }
    
    }
    

    // ── Ressources shadow (si backend supporte les shadow shaders GLSL) ───────
    if (hasShadowMap) {
        // Texture profondeur 1024×1024
        hShadowTex     = device->CreateTexture(NkTextureDesc::DepthStencil(kShadowSize, kShadowSize));
        hShadowSampler = device->CreateSampler(NkSamplerDesc::Shadow());

        // Render pass depth-only
        hShadowRP = device->CreateRenderPass(NkRenderPassDesc::ShadowMap());

        // Framebuffer
        NkFramebufferDesc fboD;
        fboD.renderPass      = hShadowRP;
        fboD.depthAttachment = hShadowTex;
        fboD.width           = kShadowSize;
        fboD.height          = kShadowSize;
        fboD.debugName       = "ShadowFBO";
        hShadowFBO = device->CreateFramebuffer(fboD);

        // Shader profondeur (position seule)
        NkShaderDesc shadowSd;
        shadowSd.debugName = "ShadowDepth";
        shadowSd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_ShadowVert);
        shadowSd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_ShadowFrag);
        hShadowShader = device->CreateShader(shadowSd);

        // Pipeline shadow (position seule, front cull + depth bias)
        NkVertexLayout shadowLayout;
        shadowLayout
            .AddAttribute(0, 0, NkGpuFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
            .AddBinding(0, sizeof(Vtx3D));
        NkGraphicsPipelineDesc shadowPD;
        shadowPD.shader       = hShadowShader;
        shadowPD.vertexLayout = shadowLayout;
        shadowPD.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        shadowPD.rasterizer   = NkRasterizerDesc::ShadowMap();
        shadowPD.depthStencil = NkDepthStencilDesc::Default();
        shadowPD.blend        = NkBlendDesc::Opaque();
        shadowPD.renderPass   = hShadowRP;
        shadowPD.debugName    = "ShadowPipeline";
        hShadowPipe = device->CreateGraphicsPipeline(shadowPD);

        // Lier la shadow texture aux 3 descriptor sets (binding 1)
        for (int i = 0; i < 3; i++) {
            if (hDescSet[i].IsValid() && hShadowTex.IsValid() && hShadowSampler.IsValid()) {
                NkDescriptorWrite sw;
                sw.set     = hDescSet[i];
                sw.binding = 1;
                sw.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture = hShadowTex;
                sw.sampler = hShadowSampler;
                device->UpdateDescriptorSets(&sw, 1);
            }
        }
    }

    // ── Callbacks CPU pour le backend Software ────────────────────────────────
    if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);
        NkSWShader* sw = swDev->GetShader(hShader.id);
        if (sw) {
            // Vertex shader : transforme position→clip et world pos dans attrs[0..2]
            sw->vertFn = [](const void* vdata, uint32 /*idx*/, const void* udata) -> NkSWVertex {
                const Vtx3D*   v   = static_cast<const Vtx3D*>(vdata); // vdata déjà positionné sur ce vertex
                const UboData* ubo = static_cast<const UboData*>(udata);
                if (!ubo) { NkSWVertex z{}; z.position={0,0,0,1}; return z; }
                auto mv4 = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                    return NkVec4f(m[0]*x+m[4]*y+m[ 8]*z+m[12]*w,
                                   m[1]*x+m[5]*y+m[ 9]*z+m[13]*w,
                                   m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                   m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
                NkVec4f wp = mv4(ubo->model, v->pos.x, v->pos.y, v->pos.z, 1.f);
                NkVec4f vp = mv4(ubo->view,  wp.x,  wp.y,  wp.z,  wp.w);
                NkVec4f cp = mv4(ubo->proj,  vp.x,  vp.y,  vp.z,  vp.w);
                NkSWVertex out;
                out.position = cp;
                out.normal   = v->normal;
                out.color    = { v->color.r, v->color.g, v->color.b, 1.f };
                out.attrs[0] = wp.x; out.attrs[1] = wp.y; out.attrs[2] = wp.z;
                return out;
            };
            // Fragment shader : Phong directionnel
            sw->fragFn = [](const NkSWVertex& frag, const void* udata, const void*) -> NkSWColor {
                const UboData* ubo = static_cast<const UboData*>(udata);
                // Normale normalisée
                float nx=frag.normal.x, ny=frag.normal.y, nz=frag.normal.z;
                float nl = NkSqrt(nx*nx+ny*ny+nz*nz);
                if (nl > 0.f) { nx/=nl; ny/=nl; nz/=nl; }
                // Direction lumière (normalisée, vers la lumière)
                float lx=-ubo->lightDirW[0], ly=-ubo->lightDirW[1], lz=-ubo->lightDirW[2];
                float ll = NkSqrt(lx*lx+ly*ly+lz*lz);
                if (ll > 0.f) { lx/=ll; ly/=ll; lz/=ll; }
                // Vecteur vue
                float wx=frag.attrs[0], wy=frag.attrs[1], wz=frag.attrs[2];
                float vx=ubo->eyePosW[0]-wx, vy=ubo->eyePosW[1]-wy, vz=ubo->eyePosW[2]-wz;
                float vl2 = NkSqrt(vx*vx+vy*vy+vz*vz);
                if (vl2 > 0.f) { vx/=vl2; vy/=vl2; vz/=vl2; }
                // Half-vector
                float hx=(lx+vx)*0.5f, hy=(ly+vy)*0.5f, hz=(lz+vz)*0.5f;
                float hl = NkSqrt(hx*hx+hy*hy+hz*hz);
                if (hl > 0.f) { hx/=hl; hy/=hl; hz/=hl; }
                float diff = nx*lx+ny*ly+nz*lz; if (diff<0.f) diff=0.f;
                float sp   = nx*hx+ny*hy+nz*hz; if (sp  <0.f) sp  =0.f;
                float spec = NkPow(sp, 32.f);
                auto sat = [](float v){ return v<0.f?0.f:(v>1.f?1.f:v); };
                return { sat(0.15f*frag.color.r + diff*frag.color.r + spec*0.4f),
                         sat(0.15f*frag.color.g + diff*frag.color.g + spec*0.4f),
                         sat(0.15f*frag.color.b + diff*frag.color.b + spec*0.4f), 1.f };
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

    // ── Variables d'état ──────────────────────────────────────────────────────
    bool  running     = true;
    float rotAngle    = 0.f;        // degrés, cube
    float camYaw      = 0.f;        // caméra orbitale
    float camPitch    = 20.f;
    float camDist     = 4.f;
    float lightYaw    = -45.f;      // direction lumière
    float lightPitch  = -30.f;
    bool  keys[512]   = {};

    NkClock  clock;

    NkEventSystem& events = NkEvents();

    // ── Enregistrement des événements ─────────────────────────────────────────
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
        rotAngle += 45.f * dt; // cube tourne à 45°/s

        // ── Calcul UBO ────────────────────────────────────────────────────────
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

        // Direction de la lumière (normalisée)
        float lx = NkCos(NkToRadians(lightPitch)) * NkSin(NkToRadians(lightYaw));
        float ly = NkSin(NkToRadians(lightPitch));
        float lz = NkCos(NkToRadians(lightPitch)) * NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx, ly, lz);

        // Matrice vue-projection lumière (shadow map)
        NkVec3f lightPos(-lightDir.x * 8.f, -lightDir.y * 8.f, -lightDir.z * 8.f);
        NkVec3f lightUp = (NkFabs(lightDir.y) > 0.9f) ? NkVec3f(1.f,0.f,0.f) : NkVec3f(0.f,1.f,0.f);
        NkMat4f matLightView = NkMat4f::LookAt(lightPos, center, lightUp);
        NkMat4f matLightProj = NkMat4f::Orthogonal(NkVec2f(-4.f,-4.f), NkVec2f(4.f,4.f), 0.5f, 20.f);
        NkMat4f matLightVP   = matLightProj * matLightView;

        // ── Frame ─────────────────────────────────────────────────────────────
        NkFrameContext frame;
        device->BeginFrame(frame);

        if (W == 0 || H == 0) {
            device->EndFrame(frame);
            continue;
        }

        // ── Enregistrement du command buffer ──────────────────────────────────
        cmd->Reset();
        cmd->Begin();

        // ── Passe shadow (depth-only, vue lumière) ────────────────────────────
        if (hasShadowMap && hShadowFBO.IsValid() && hShadowPipe.IsValid()) {
            NkRect2D shadowArea{0, 0, kShadowSize, kShadowSize};
            cmd->BeginRenderPass(hShadowRP, hShadowFBO, shadowArea);
            NkViewport svp{0.f, 0.f, (float)kShadowSize, (float)kShadowSize, 0.f, 1.f};
            cmd->SetViewport(svp);
            cmd->SetScissor(shadowArea);
            cmd->BindGraphicsPipeline(hShadowPipe);

            // Cube (index 0)
            {
                NkMat4f mm = NkMat4f::RotationY(NkAngle(rotAngle))
                           * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));
                UboData su{}; Mat4ToArray(mm, su.model); Mat4ToArray(matLightVP, su.lightVP);
                device->WriteBuffer(hUBO[0], &su, sizeof(su));
                if (hDescSet[0].IsValid()) cmd->BindDescriptorSet(hDescSet[0], 0);
                cmd->BindVertexBuffer(0, hCube);
                cmd->Draw((uint32)cubeVerts.Size());
            }
            // Sphère (index 1)
            {
                NkMat4f mm = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
                UboData su{}; Mat4ToArray(mm, su.model); Mat4ToArray(matLightVP, su.lightVP);
                device->WriteBuffer(hUBO[1], &su, sizeof(su));
                if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
                cmd->BindVertexBuffer(0, hSphere);
                cmd->Draw((uint32)sphereVerts.Size());
            }
            // Plan (index 2)
            {
                NkMat4f mm = NkMat4f::Translation(NkVec3f(0.f, -0.65f, 0.f));
                UboData su{}; Mat4ToArray(mm, su.model); Mat4ToArray(matLightVP, su.lightVP);
                device->WriteBuffer(hUBO[2], &su, sizeof(su));
                if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
                cmd->BindVertexBuffer(0, hPlane);
                cmd->Draw((uint32)planeVerts.Size());
            }
            cmd->EndRenderPass();
        }

        NkRect2D area{0, 0, (int32)W, (int32)H};
        cmd->BeginRenderPass(hRP, hFBO, area);

        NkViewport vp{0.f, 0.f, (float)W, (float)H, 0.f, 1.f};
        cmd->SetViewport(vp);
        cmd->SetScissor(area);

        cmd->BindGraphicsPipeline(hPipe);

        auto fillUbo = [&](UboData& ubo, const NkMat4f& model) {
            Mat4ToArray(model,      ubo.model);
            Mat4ToArray(matView,    ubo.view);
            Mat4ToArray(matProj,    ubo.proj);
            Mat4ToArray(matLightVP, ubo.lightVP);
            ubo.lightDirW[0]=lightDir.x; ubo.lightDirW[1]=lightDir.y;
            ubo.lightDirW[2]=lightDir.z; ubo.lightDirW[3]=0.f;
            ubo.eyePosW[0]=eye.x; ubo.eyePosW[1]=eye.y;
            ubo.eyePosW[2]=eye.z; ubo.eyePosW[3]=0.f;
        };

        // --- Cube rotatif (centré en (0, 0, 0)) ---
        {
            NkMat4f matModel = NkMat4f::RotationY(NkAngle(rotAngle))
                             * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));
            UboData ubo{}; fillUbo(ubo, matModel);
            device->WriteBuffer(hUBO[0], &ubo, sizeof(ubo));
            if (hDescSet[0].IsValid()) cmd->BindDescriptorSet(hDescSet[0], 0);
            cmd->BindVertexBuffer(0, hCube);
            cmd->Draw((uint32)cubeVerts.Size());
        }

        // --- Sphère (décalée à droite) ---
        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
            UboData ubo{}; fillUbo(ubo, matModel);
            device->WriteBuffer(hUBO[1], &ubo, sizeof(ubo));
            if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
            cmd->BindVertexBuffer(0, hSphere);
            cmd->Draw((uint32)sphereVerts.Size());
        }

        // --- Plan sol (décalé vers le bas) ---
        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(0.f, -0.65f, 0.f));
            UboData ubo{}; fillUbo(ubo, matModel);
            device->WriteBuffer(hUBO[2], &ubo, sizeof(ubo));
            if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
            cmd->BindVertexBuffer(0, hPlane);
            cmd->Draw((uint32)planeVerts.Size());
        }

        cmd->EndRenderPass();
        cmd->End();

        // ── Soumission & présentation ─────────────────────────────────────────
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // =========================================================================
    // Nettoyage
    // =========================================================================
    device->WaitIdle();

    device->DestroyCommandBuffer(cmd);
    for (int i = 0; i < 3; i++) {
        if (hDescSet[i].IsValid()) device->FreeDescriptorSet(hDescSet[i]);
    }
    if (hLayout.IsValid())      device->DestroyDescriptorSetLayout(hLayout);
    for (int i = 0; i < 3; i++) {
        if (hUBO[i].IsValid())   device->DestroyBuffer(hUBO[i]);
    }
    if (hPlane.IsValid())       device->DestroyBuffer(hPlane);
    if (hSphere.IsValid())      device->DestroyBuffer(hSphere);
    if (hCube.IsValid())        device->DestroyBuffer(hCube);
    device->DestroyPipeline(hPipe);
    device->DestroyShader(hShader);
    // Ressources shadow
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
