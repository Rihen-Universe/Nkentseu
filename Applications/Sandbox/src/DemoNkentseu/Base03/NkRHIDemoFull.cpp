// =============================================================================
// NkRHIDemoFull.cpp
// Démo RHI complète utilisant la couche NkIDevice (NKRenderer)
//
//   • Multi-backend : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//     sélectionnable via argument ligne de commande : --backend=opengl|vulkan|dx11|dx12|sw|metal
//   • Géométrie 3D : cube rotatif + sphère + plan (sol)
//   • Éclairage Phong directionnel (ambient + diffuse + specular)
//   • Shadow mapping multi-backend (OpenGL / Vulkan / DX11 / DX12 / Software)
//   • NKMath : NkVec3f, NkMat4f (column-major, OpenGL convention)
//
// Pipeline d'initialisation :
//   NkWindow → NkDeviceFactory → ressources RHI → boucle
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
#include "NKTime/NkTime.h"

// ── RHI ──────────────────────────────────────────────────────────────────────
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"

// ── NKMath ───────────────────────────────────────────────────────────────────
#include "NKMath/NKMath.h"

// ── NKLogger ──────────────────────────────────────────────────────────────────
#include "NKLogger/NkLog.h"

// ── std ───────────────────────────────────────────────────────────────────────
#include <cstring>
#include <cstdint>
#include "NKContainers/Sequential/NkVector.h"

// ── SPIR-V précompilé pour Vulkan ─────────────────────────────────────────────
#include "NkRHIDemoFullVkSpv.inl"

// ── ShaderConvert : compilation GLSL→SPIRV runtime + cache .nksc ─────────────
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

namespace nkentseu {
    struct NkEntryState;
}

// =============================================================================
// Shaders GLSL (OpenGL 4.6)
// =============================================================================

// ── Shadow depth pass : position seule, matrix = lightVP * model ─────────────
// static constexpr const char* kGLSL_ShadowVert = R"GLSL(
// #version 460 core
// layout(location = 0) in vec3 aPos;

// // On utilise un UBO séparé pour la passe shadow (binding 0)
// layout(std140, binding = 0) uniform ShadowUBO {
//     mat4 model;
//     mat4 view;       // non utilisé dans cette passe
//     mat4 proj;       // non utilisé dans cette passe
//     mat4 lightVP;
//     vec4 lightDirW;
//     vec4 eyePosW;
// } ubo;

// void main() {
//     // Transforme directement dans l'espace lumière
//     gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
// }
// )GLSL";

// static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
// #version 460 core
// // Pas de sortie couleur — on écrit uniquement le depth buffer
// void main() {}
// )GLSL";

static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;

layout(std140, binding = 0) uniform ShadowUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
// Passe shadow depth-only : pas de sortie couleur
void main() {}
)GLSL";

// ── Passe principale Phong + shadow mapping PCF ───────────────────────────────
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec4 vShadowCoord;  // coordonnées dans l'espace lumière (non-divisées)

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
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec4 vShadowCoord;   // coordonnées lumière homogènes

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

// sampler2DShadow avec compare mode LESS_OR_EQUAL
// (le sampler doit être configuré avec compareEnable=true, compareOp=LESS_EQUAL)
layout(binding = 1) uniform sampler2DShadow uShadowMap;

// ── Filtrage PCF 3x3 ─────────────────────────────────────────────────────────
float ShadowFactor(vec4 shadowCoord) {
    // Division perspective (projection orthogonale → w=1, mais on normalise quand même)
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;

    // xy : NDC [-1,1] → UV [0,1]
    // z  : remap selon la convention de l'API (ndcZScale/ndcZOffset dans l'UBO)
    //      OpenGL: z ∈ [-1,1] → scale=0.5, offset=0.5
    //      Vulkan/DX: z ∈ [0,1] → scale=1.0, offset=0.0
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z  = projCoords.z * ubo.ndcZScale + ubo.ndcZOffset;

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
    float shadow = max(ShadowFactor(vShadowCoord), 0.35);

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
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
struct VSIn  { float3 pos:POSITION; float3 norm:NORMAL; float3 col:COLOR; };
struct VSOut {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
};
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos,1));
    o.wp  = wp.xyz;
    o.n   = normalize(mul((float3x3)model, v.norm));
    o.c   = v.col;
    o.shadowPos = mul(lightVP, wp);
    o.pos = mul(proj, mul(view, wp));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_PS = R"HLSL(
cbuffer UBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
Texture2D<float> uShadowMap : register(t1);
SamplerComparisonState uShadowSampler : register(s1);

struct PSIn {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
};

float ShadowFactor(float4 shadowPos, float3 N) {
    float3 projCoords = shadowPos.xyz / shadowPos.w;
    // DX: NDC z ∈ [0,1] avec depthZeroToOne — pas besoin de remap Z
    // DX UV: y=0 est en haut → flip Y (NDC +1 → UV 0 top)
    projCoords.x =  projCoords.x * 0.5 + 0.5;
    projCoords.y = -projCoords.y * 0.5 + 0.5;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float3 L = normalize(-lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = lerp(0.005, 0.0005, cosA);
    float cmpDepth = projCoords.z - bias;

    uint sw = 1, sh = 1;
    uShadowMap.GetDimensions(sw, sh);
    float2 texel = 1.0 / float2((float)sw, (float)sh);

    float sum = 0.0;
    [unroll] for (int x = -1; x <= 1; ++x) {
        [unroll] for (int y = -1; y <= 1; ++y) {
            float2 uv = projCoords.xy + float2((float)x, (float)y) * texel;
            sum += uShadowMap.SampleCmpLevelZero(uShadowSampler, uv, cmpDepth);
        }
    }
    return sum / 9.0;
}

float4 PSMain(PSIn i) : SV_Target {
    float3 N = normalize(i.n);
    float3 L = normalize(-lightDirW.xyz);
    float3 V = normalize(eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(N,H), 0.0), 32.0);
    float shadow = max(ShadowFactor(i.shadowPos, N), 0.35);
    float3 col = 0.15*i.c + shadow * diff * i.c + shadow * spec * 0.4;
    return float4(col, 1.0);
}
)HLSL";

static constexpr const char* kHLSL_ShadowVert = R"HLSL(
// NkShadowVert.hlsl
struct VSInput {
    float3 aPos : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

// Utilisation d'un constant buffer séparé pour la passe shadow (binding 0)
cbuffer ShadowUBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4 lightDirW;
    float4 eyePosW;
    float  ndcZScale;
    float  ndcZOffset;
    float2 _pad;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    // Transforme directement dans l'espace lumière
    output.position = mul(lightVP, mul(model, float4(input.aPos, 1.0)));
    
    return output;
}
)HLSL";

static constexpr const char* kHLSL_ShadowFrag = R"HLSL(
// NkShadowFrag.hlsl
struct PSInput {
    float4 position : SV_POSITION;
};

// Pas de sortie couleur — on écrit uniquement le depth buffer
// En HLSL, si on n'écrit pas de couleur, le pipeline utilise la valeur de depth
// déjà présente de l'étape de rasterization
void main(PSInput input) {
    // Rien à faire — le depth buffer est automatiquement mis à jour
    // On peut éventuellement faire early depth test optimizations
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
// Ordre : model(64) + view(64) + proj(64) + lightVP(64) + lightDirW(16) + eyePosW(16)
//       + ndcZScale(4) + ndcZOffset(4) + _pad(8) = 304 bytes
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];   // matrice VP de la lumière pour le shadow mapping
    float lightDirW[4];  // direction lumière world space (xyz) + pad
    float eyePosW[4];    // position caméra world space (xyz) + pad
    float ndcZScale;     // 0.5 pour OpenGL/SW (z NDC ∈ [-1,1]), 1.0 pour Vulkan/DX/Metal
    float ndcZOffset;    // 0.5 pour OpenGL/SW, 0.0 pour Vulkan/DX/Metal
    float _pad[2];
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

static bool HasArg(const nkentseu::NkVector<nkentseu::NkString>& args,
                   const char* longName,
                   const char* shortName = nullptr) {
    for (size_t i = 1; i < args.Size(); ++i) {
        if (args[i] == longName) return true;
        if (shortName && args[i] == shortName) return true;
    }
    return false;
}

// Essaie de compiler GLSL→SPIRV via NkShaderConverter (avec cache).
// Si NK_RHI_GLSLANG_ENABLED est absent ou la compilation échoue,
// retombe sur les mots SPIRV précompilés fournis en fallback.
static void AddSpirvOrFallback(NkShaderDesc& sd, NkShaderStage stage,
                               const char* glsl, const char* dbgName,
                               const uint32* fallbackSpv, uint64 fallbackBytes)
{
    if (NkShaderConverter::CanGlslToSpirv()) {
        NkSLStage sl = (stage == NkShaderStage::NK_VERTEX)
                     ? NkSLStage::NK_VERTEX : NkSLStage::NK_FRAGMENT;
        uint64 key = NkShaderCache::ComputeKey(NkString(glsl), sl, "spirv");
        NkShaderConvertResult res = NkShaderCache::Global().Load(key);
        if (!res.success)
            res = NkShaderConverter::GlslToSpirv(NkString(glsl), sl, dbgName);
        if (res.success) {
            NkShaderCache::Global().Save(key, res);
            sd.AddSPIRV(stage, res.SpirvWords(), res.SpirvWordCount() * sizeof(uint32));
            logger.Info("[ShaderConvert] {0} — SPIRV runtime OK ({1} mots)\n",
                        dbgName, (unsigned long long)res.SpirvWordCount());
            return;
        }
        logger.Info("[ShaderConvert] {0} — fallback sur .inl\n", dbgName);
    }
    sd.AddSPIRV(stage, fallbackSpv, fallbackBytes);
}

static NkShaderDesc MakeShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "Phong3D";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_VS, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_PS, "PSMain");
            break;
        case NkGraphicsApi::NK_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_Shaders, "vmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_Shaders, "fmain");
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            // Compilation runtime GLSL→SPIRV via NkShaderConverter (avec cache .nksc).
            // Fallback automatique sur le SPIRV précompilé si glslang non dispo.
            AddSpirvOrFallback(sd, NkShaderStage::NK_VERTEX,   kGLSL_Vert, "Phong3D.vert",
                kVkRHIFullDemoVertSpv, (uint64)kVkRHIFullDemoVertSpvWordCount * sizeof(uint32));
            AddSpirvOrFallback(sd, NkShaderStage::NK_FRAGMENT, kGLSL_Frag, "Phong3D.frag",
                kVkRHIFullDemoFragSpv, (uint64)kVkRHIFullDemoFragSpvWordCount * sizeof(uint32));
            break;
        default:
            // OpenGL, Software : GLSL
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Frag);
            break;
    }
    return sd;
}

static NkShaderDesc MakeShadowShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "ShadowDepth";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_ShadowVert);
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_ShadowFrag);
            break;
        case NkGraphicsApi::NK_API_METAL:
            // Shadow pass MSL non implémenté (Metal sans ombres)
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkShadowVertSpv,
                        (uint64)kVkShadowVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkShadowFragSpv,
                        (uint64)kVkShadowFragSpvWordCount * sizeof(uint32));
            break;
        default:
            // OpenGL, Software : GLSL
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_ShadowVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_ShadowFrag);
            break;
    }
    return sd;
}

// =============================================================================
// nkmain — point d'entrée
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {

    // ── ShaderCache ───────────────────────────────────────────────────────────
    NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

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

    // ── Surface native + init info device ────────────────────────────────────
    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo deviceInitInfo;
    deviceInitInfo.api = targetApi;
    deviceInitInfo.surface = surface;
    deviceInitInfo.height = window.GetSize().height;
    deviceInitInfo.width = window.GetSize().width;

    deviceInitInfo.context.vulkan.appName = "NkRHIDemoFull";
    deviceInitInfo.context.vulkan.engineName = "Unkeny";

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkIDevice* device = NkDeviceFactory::Create(deviceInitInfo);
    if (!device || !device->IsValid()) {
        logger.Info("[RHIFullDemo] Échec création NkIDevice ({0})\n", apiName);
        window.Close();
        return 1;
    }
    logger.Info("[RHIFullDemo] Device {0} initialisé. VRAM: {1} Mo\n",
                apiName, (unsigned long long)(device->GetCaps().vramBytes >> 20));

    // ── Dimensions swapchain ──────────────────────────────────────────────────
    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── Shader principal ──────────────────────────────────────────────────────
    logger.Info("[RHIFullDemo] Init step: build main shader desc\n");
    NkShaderDesc shaderDesc = MakeShaderDesc(targetApi);
    logger.Info("[RHIFullDemo] Init step: create main shader begin\n");
    NkShaderHandle hShader = device->CreateShader(shaderDesc);
    logger.Info("[RHIFullDemo] Init step: create main shader done valid={0}\n", hShader.IsValid() ? 1 : 0);
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Échec compilation shader\n");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Vertex Layout ─────────────────────────────────────────────────────────
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0,               "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3*sizeof(float),  "NORMAL",  0)
        .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 6*sizeof(float),  "COLOR",   0)
        .AddBinding(0, sizeof(Vtx3D));

    // ── Render Pass swapchain (récupéré une fois, stable entre les frames) ────
    logger.Info("[RHIFullDemo] Init step: get swapchain render pass\n");
    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    // ── Shadow map ────────────────────────────────────────────────────────────
    // shaderNeedsShadowSampler : le shader GPU lit la shadow map via un descriptor (binding 1).
    // wantsShadowResources : on crée les ressources RHI shadow (inclut Software).
    const bool shaderNeedsShadowSampler =
        targetApi == NkGraphicsApi::NK_API_OPENGL    ||
        targetApi == NkGraphicsApi::NK_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX12;
    const bool wantsShadowResources =
        shaderNeedsShadowSampler ||
        targetApi == NkGraphicsApi::NK_API_SOFTWARE;
    const bool forceSafeShadows = HasArg(state.GetArgs(), "--safe-shadows", "--shadow-safe-clear");
    const bool requestRealShadows = !forceSafeShadows;
    static constexpr uint32 kShadowSize = 2048;

    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;
    bool hasShadowMap = false;
    bool useRealShadowPass = false;

    // Descriptor set layout (binding 0 = UBO, binding 1 = shadow sampler)
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    if (shaderNeedsShadowSampler)
        layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    logger.Info("[RHIFullDemo] Init step: create descriptor set layout begin\n");
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);
    logger.Info("[RHIFullDemo] Init step: create descriptor set layout done valid={0}\n", hLayout.IsValid() ? 1 : 0);

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

    logger.Info("[RHIFullDemo] Init step: create main pipeline begin\n");
    NkPipelineHandle hPipe = device->CreateGraphicsPipeline(pipeDesc);
    logger.Info("[RHIFullDemo] Init step: create main pipeline done valid={0}\n", hPipe.IsValid() ? 1 : 0);
    if (!hPipe.IsValid()) {
        logger.Info("[RHIFullDemo] Échec création pipeline\n");
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Géométrie ─────────────────────────────────────────────────────────────
    auto cubeVerts   = MakeCube();
    auto sphereVerts = MakeSphere();
    auto planeVerts  = MakePlane();
    logger.Info("[RHIFullDemo] Init step: geometry generated cube={0} sphere={1} plane={2}\n",
                (unsigned long long)cubeVerts.Size(),
                (unsigned long long)sphereVerts.Size(),
                (unsigned long long)planeVerts.Size());

    auto uploadVBO = [&](const NkVector<Vtx3D>& verts) -> NkBufferHandle {
        uint64 sz = verts.Size() * sizeof(Vtx3D);
        return device->CreateBuffer(NkBufferDesc::Vertex(sz, verts.Begin()));
    };

    NkBufferHandle hCube   = uploadVBO(cubeVerts);
    NkBufferHandle hSphere = uploadVBO(sphereVerts);
    NkBufferHandle hPlane  = uploadVBO(planeVerts);
    logger.Info("[RHIFullDemo] Init step: VBO created cube={0} sphere={1} plane={2}\n",
                hCube.IsValid() ? 1 : 0, hSphere.IsValid() ? 1 : 0, hPlane.IsValid() ? 1 : 0);

    // ── Uniform Buffers ───────────────────────────────────────────────────────
    // 3 UBO : un par objet (cube, sphère, plan)
    // Séparés pour éviter les conflits d'écriture entre passe shadow et passe principale
    NkBufferHandle hUBO[3];
    for (int i = 0; i < 3; i++) {
        hUBO[i] = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
        if (!hUBO[i].IsValid())
            logger.Info("[RHIFullDemo] Échec création UBO[{0}]\n", i);
    }
    logger.Info("[RHIFullDemo] Init step: UBO created {0}/{1}/{2}\n",
                hUBO[0].IsValid() ? 1 : 0, hUBO[1].IsValid() ? 1 : 0, hUBO[2].IsValid() ? 1 : 0);

    // ── Descriptor Sets ───────────────────────────────────────────────────────
    // 3 descriptor sets : un par objet
    // Chaque set lie son propre UBO (binding 0) + la shadow texture (binding 1)
    NkDescSetHandle hDescSet[3];
    for (int i = 0; i < 3; i++) {
        logger.Info("[RHIFullDemo] Init step: alloc desc set[{0}] begin\n", i);
        hDescSet[i] = device->AllocateDescriptorSet(hLayout);
        logger.Info("[RHIFullDemo] Init step: alloc desc set[{0}] done valid={1}\n", i, hDescSet[i].IsValid() ? 1 : 0);
        if (hLayout.IsValid() && hDescSet[i].IsValid() && hUBO[i].IsValid()) {
            NkDescriptorWrite w{};
            w.set          = hDescSet[i];
            w.binding      = 0;
            w.type         = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer       = hUBO[i];
            w.bufferOffset = 0;
            w.bufferRange  = sizeof(UboData);
            logger.Info("[RHIFullDemo] Init step: update UBO desc set[{0}] begin\n", i);
            device->UpdateDescriptorSets(&w, 1);
            logger.Info("[RHIFullDemo] Init step: update UBO desc set[{0}] done\n", i);
        }
    }

    // ── Ressources shadow ─────────────────────────────────────────────────────
    if (wantsShadowResources) {
        logger.Info("[RHIFullDemo] Shadow init begin ({0})\n", apiName);
        NkTextureDesc shadowTexDesc = NkTextureDesc::DepthStencil(
            kShadowSize, kShadowSize,
            NkGPUFormat::NK_D32_FLOAT,
            NkSampleCount::NK_S1);
        shadowTexDesc.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
        logger.Info("[RHIFullDemo] Shadow create texture begin\n");
        hShadowTex = device->CreateTexture(shadowTexDesc);
        logger.Info("[RHIFullDemo] Shadow create texture done valid={0}\n", hShadowTex.IsValid() ? 1 : 0);

        NkSamplerDesc shadowSamplerDesc = NkSamplerDesc::Shadow();
        shadowSamplerDesc.magFilter = NkFilter::NK_LINEAR;
        shadowSamplerDesc.minFilter = NkFilter::NK_LINEAR;
        shadowSamplerDesc.mipFilter = NkMipFilter::NK_NONE;
        shadowSamplerDesc.minLod = 0.f;
        shadowSamplerDesc.maxLod = 0.f;
        logger.Info("[RHIFullDemo] Shadow create sampler begin\n");
        hShadowSampler = device->CreateSampler(shadowSamplerDesc);
        logger.Info("[RHIFullDemo] Shadow create sampler done valid={0}\n", hShadowSampler.IsValid() ? 1 : 0);

        logger.Info("[RHIFullDemo] Shadow create renderpass begin\n");
        hShadowRP = device->CreateRenderPass(NkRenderPassDesc::ShadowMap());
        logger.Info("[RHIFullDemo] Shadow create renderpass done valid={0}\n", hShadowRP.IsValid() ? 1 : 0);

        NkFramebufferDesc fboD{};
        fboD.renderPass = hShadowRP;
        fboD.depthAttachment = hShadowTex;
        fboD.width = kShadowSize;
        fboD.height = kShadowSize;
        fboD.debugName = "ShadowFBO";
        logger.Info("[RHIFullDemo] Shadow create framebuffer begin\n");
        hShadowFBO = device->CreateFramebuffer(fboD);
        logger.Info("[RHIFullDemo] Shadow create framebuffer done valid={0}\n", hShadowFBO.IsValid() ? 1 : 0);

        hasShadowMap = hShadowTex.IsValid() && hShadowSampler.IsValid() && hShadowRP.IsValid() && hShadowFBO.IsValid();

        if (requestRealShadows && hasShadowMap) {
            NkShaderDesc shadowSd = MakeShadowShaderDesc(targetApi);
            if (!shadowSd.stages.IsEmpty()) {
                hShadowShader = device->CreateShader(shadowSd);
            }

            NkVertexLayout shadowVtxLayout;
            shadowVtxLayout
                .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
                .AddBinding(0, sizeof(Vtx3D));

            NkGraphicsPipelineDesc shadowPipeDesc{};
            shadowPipeDesc.shader = hShadowShader;
            shadowPipeDesc.vertexLayout = shadowVtxLayout;
            shadowPipeDesc.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            shadowPipeDesc.rasterizer = NkRasterizerDesc::ShadowMap();
            shadowPipeDesc.depthStencil = NkDepthStencilDesc::Default();
            shadowPipeDesc.blend = NkBlendDesc::Opaque();
            shadowPipeDesc.renderPass = hShadowRP;
            shadowPipeDesc.debugName = "ShadowPipeline";
            if (hLayout.IsValid()) {
                shadowPipeDesc.descriptorSetLayouts.PushBack(hLayout);
            }
            hShadowPipe = device->CreateGraphicsPipeline(shadowPipeDesc);
            useRealShadowPass = hShadowShader.IsValid() && hShadowPipe.IsValid();
        }

        for (int i = 0; i < 3; i++) {
            if (hDescSet[i].IsValid() && hShadowTex.IsValid() && hShadowSampler.IsValid()) {
                NkDescriptorWrite sw{};
                sw.set = hDescSet[i];
                sw.binding = 1;
                sw.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture = hShadowTex;
                sw.sampler = hShadowSampler;
                sw.textureLayout = NkResourceState::NK_DEPTH_READ;
                logger.Info("[RHIFullDemo] Shadow update desc set[{0}] begin\n", i);
                device->UpdateDescriptorSets(&sw, 1);
                logger.Info("[RHIFullDemo] Shadow update desc set[{0}] done\n", i);
            }
        }
        logger.Info("[RHIFullDemo] Shadow init end ({0})\n", apiName);
    }

    if (hasShadowMap) {
        logger.Info("[RHIFullDemo] Shadow map activee ({0}x{0}) mode={1}\n",
                    kShadowSize, useRealShadowPass ? "real" : "safe-clear");
    } else {
        logger.Info("[RHIFullDemo] Shadow map desactivee pour {0}\n", apiName);
    }

    // Pour les backends GPU avec sampler descriptor, on ne peut pas continuer sans shadow map valide
    if (shaderNeedsShadowSampler && !hasShadowMap) {
        logger.Info("[RHIFullDemo] Impossible de continuer: shader principal attend un shadow sampler valide ({0})\n", apiName);
        device->DestroyPipeline(hPipe);
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }
// ── Callbacks CPU pour le backend Software ────────────────────────────────
    if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);

        // ── Shadow shader : depth-only pass ──────────────────────────────
        if (hShadowShader.IsValid()) {
            NkSWShader* swSh = swDev->GetShader(hShadowShader.id);
            if (swSh) {
                swSh->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware {
                    const Vtx3D*   v   = static_cast<const Vtx3D*>(vdata) + idx;
                    const UboData* ubo = static_cast<const UboData*>(udata);
                    NkVertexSoftware out;
                    if (!ubo) { out.position = {v->pos.x, v->pos.y, v->pos.z, 1.f}; return out; }
                    auto mul4 = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                        return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w, m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                       m[2]*x+m[6]*y+m[10]*z+m[14]*w, m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                    };
                    NkVec4f wp  = mul4(ubo->model,   v->pos.x, v->pos.y, v->pos.z, 1.f);
                    NkVec4f lsp = mul4(ubo->lightVP, wp.x, wp.y, wp.z, wp.w);
                    out.position = lsp;
                    return out;
                };
                // Pas de fragFn : depth-only (null colorTarget, le depth est écrit par le rasterizer)
                swSh->fragFn = nullptr;
            }
        }

        // ── Main shader : Phong + shadow PCF ─────────────────────────────
        NkSWShader* sw = swDev->GetShader(hShader.id);
        // Récupère la texture depth de la shadow map pour la capture lambda
        NkSWTexture* swShadowTex = hasShadowMap ? swDev->GetTex(hShadowTex.id) : nullptr;

        if (sw) {
            // Vertex shader CPU
            static bool s_vertLoggedOnce = false;
            static uint32 s_fragCallCount = 0;
            static bool   s_loggedOnce    = false;

            sw->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware
            {
                const Vtx3D*   v   = static_cast<const Vtx3D*>(vdata) + idx;
                const UboData* ubo = static_cast<const UboData*>(udata);
            
                NkVertexSoftware out;
                if (!ubo) {
                    out.position = {v->pos.x, v->pos.y, v->pos.z, 1.f};
                    out.normal   = v->normal;
                    out.color    = {v->color.r, v->color.g, v->color.b, 1.f};
                    return out;
                }
            
                auto mul4 = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                    return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w,
                                m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
            
                NkVec4f wp  = mul4(ubo->model,   v->pos.x, v->pos.y, v->pos.z, 1.f);
                NkVec4f vp  = mul4(ubo->view,    wp.x,  wp.y,  wp.z,  wp.w);
                NkVec4f cp  = mul4(ubo->proj,    vp.x,  vp.y,  vp.z,  vp.w);
                out.position = cp;
            
                float nx = ubo->model[0]*v->normal.x + ubo->model[4]*v->normal.y + ubo->model[8]*v->normal.z;
                float ny = ubo->model[1]*v->normal.x + ubo->model[5]*v->normal.y + ubo->model[9]*v->normal.z;
                float nz = ubo->model[2]*v->normal.x + ubo->model[6]*v->normal.y + ubo->model[10]*v->normal.z;
                float nl = NkSqrt(nx*nx + ny*ny + nz*nz);
                if (nl > 0.001f) { nx/=nl; ny/=nl; nz/=nl; }
                out.normal = {nx, ny, nz};
                out.color  = {v->color.r, v->color.g, v->color.b, 1.f};
            
                out.attrs[0] = wp.x; out.attrs[1] = wp.y; out.attrs[2] = wp.z;
                NkVec4f lsc = mul4(ubo->lightVP, wp.x, wp.y, wp.z, wp.w);
                out.attrs[3] = lsc.x; out.attrs[4] = lsc.y;
                out.attrs[5] = lsc.z; out.attrs[6] = lsc.w;
                out.attrCount = 7;  // ← CRUCIAL : déclarer explicitement le nombre d'attrs
            
                if (!s_vertLoggedOnce && idx == 0) {
                    s_vertLoggedOnce = true;
                    logger.Infof("[VERT_DIAG] idx=0 pos=%.3f,%.3f,%.3f,%.3f",
                                cp.x, cp.y, cp.z, cp.w);
                    logger.Infof("[VERT_DIAG] wp=%.3f,%.3f,%.3f", wp.x, wp.y, wp.z);
                    logger.Infof("[VERT_DIAG] lsc=%.3f,%.3f,%.3f,%.3f", lsc.x,lsc.y,lsc.z,lsc.w);
                    logger.Infof("[VERT_DIAG] attrCount=%u", out.attrCount);
                }
            
                return out;
            };

            // Fragment shader CPU — 1-tap shadow pour limiter le coût CPU
            sw->fragFn = [swShadowTex](const NkVertexSoftware& frag,
                            const void* udata, const void* texPtr) -> math::NkVec4f
            {
                ++s_fragCallCount;
            
                // Log une seule fois pour voir ce qu'on reçoit
                if (!s_loggedOnce) {
                    s_loggedOnce = true;
                    logger.Infof("[FRAG_DIAG] attrCount=%u", frag.attrCount);
                    logger.Infof("[FRAG_DIAG] attrs[0..6] = %.3f %.3f %.3f | %.3f %.3f %.3f %.3f",
                                frag.attrs[0], frag.attrs[1], frag.attrs[2],
                                frag.attrs[3], frag.attrs[4], frag.attrs[5], frag.attrs[6]);
                    logger.Infof("[FRAG_DIAG] normal = %.3f %.3f %.3f",
                                frag.normal.x, frag.normal.y, frag.normal.z);
                    logger.Infof("[FRAG_DIAG] color  = %.3f %.3f %.3f",
                                frag.color.r, frag.color.g, frag.color.b);
                    logger.Infof("[FRAG_DIAG] udata=%p texPtr=%p swShadowTex=%p",
                                udata, texPtr, (void*)swShadowTex);
                    if (swShadowTex) {
                        logger.Infof("[FRAG_DIAG] shadowTex size=%ux%u format=%d mips=%u",
                                    swShadowTex->Width(), swShadowTex->Height(),
                                    (int)swShadowTex->desc.format,
                                    (uint32)swShadowTex->mips.Size());
                        // Lire quelques pixels de la shadow map pour voir si elle est remplie
                        if (!swShadowTex->mips.Empty()) {
                            float d0 = swShadowTex->Read(1024, 1024).r;
                            float d1 = swShadowTex->Read(0, 0).r;
                            float d2 = swShadowTex->Read(2047, 2047).r;
                            logger.Infof("[FRAG_DIAG] shadowMap samples: center=%.4f corner0=%.4f corner1=%.4f",
                                        d0, d1, d2);
                        }
                    }
                }
            
                // Retourner une couleur de debug pour voir si le shader est appelé du tout
                // Rouge = shader appelé, attrs[0] non nul = worldPos correct
                if (frag.attrs[0] == 0.f && frag.attrs[1] == 0.f && frag.attrs[2] == 0.f) {
                    return {1.f, 0.f, 0.f, 1.f}; // ROUGE = attrs perdus
                }
                if (frag.normal.x == 0.f && frag.normal.y == 0.f && frag.normal.z == 0.f) {
                    return {1.f, 1.f, 0.f, 1.f}; // JAUNE = normale perdue
                }
                return {0.f, 1.f, 0.f, 1.f}; // VERT = tout est là
            };
        }
    }

    NkVector<Vtx3D> testVerts;
    testVerts.PushBack({NkVec3f(-0.5f, -0.5f, 0.f), NkVec3f(0,0,1), NkVec3f(1,0,0)});
    testVerts.PushBack({NkVec3f( 0.5f, -0.5f, 0.f), NkVec3f(0,0,1), NkVec3f(0,1,0)});
    testVerts.PushBack({NkVec3f( 0.f,   0.5f, 0.f), NkVec3f(0,0,1), NkVec3f(0,0,1)});
    NkBufferHandle hTest = uploadVBO(testVerts);

    // ── Command Buffer ────────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger.Info("[RHIFullDemo] Échec création command buffer\n");
        NkDeviceFactory::Destroy(device);
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
    });

    // ── Constantes de convention NDC selon l'API ──────────────────────────────
    // depthZeroToOne : Vulkan/DX/Metal clip Z ∈ [0,1], OpenGL/SW clip Z ∈ [-1,1]
    const bool  depthZeroToOne =
        targetApi == NkGraphicsApi::NK_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX12 ||
        targetApi == NkGraphicsApi::NK_API_METAL;
    const float ndcZScale  = depthZeroToOne ? 1.0f : 0.5f;
    const float ndcZOffset = depthZeroToOne ? 0.0f : 0.5f;

    logger.Info("[RHIFullDemo] Boucle principale. ESC=quitter, WASD=caméra, Flèches=lumière\n");

    // =========================================================================
    // Boucle principale
    // =========================================================================
    while (running) {
        events.PollEvents();
        if (!running) break;

        if (W == 0 || H == 0) {
            continue;
        }

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) { 
            device->OnResize(W, H);
        }

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
            1.f, 20.f,
            depthZeroToOne);

        NkMat4f matLightVP = matLightProj * matLightView;

        // ── Frame ─────────────────────────────────────────────────────────────
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) {
            continue;
        }

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) {
            device->EndFrame(frame);
            continue;
        }

        NkRenderPassHandle latestSwapchainRP = device->GetSwapchainRenderPass();
        if (latestSwapchainRP.IsValid() && latestSwapchainRP.id != hRP.id) {
            hRP = latestSwapchainRP;
            pipeDesc.renderPass = hRP;
            if (hPipe.IsValid()) device->DestroyPipeline(hPipe);
            hPipe = device->CreateGraphicsPipeline(pipeDesc);
            if (!hPipe.IsValid()) {
                logger.Info("[RHIFullDemo] Pipeline principal invalide apres recreate swapchain\n");
                device->EndFrame(frame);
                continue;
            }
        }

        // Récupérer le framebuffer courant APRÈS BeginFrame (index mis à jour)
        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();

        cmd->Reset();
        if (!cmd->Begin()) {
            device->EndFrame(frame);
            continue;
        }

        // =====================================================================
        // Passe 1 : Shadow map (depth-only, POV de la lumière)
        // =====================================================================
        if (hasShadowMap && hShadowFBO.IsValid() && hShadowRP.IsValid()) {

            NkRect2D shadowArea{0, 0, (int32)kShadowSize, (int32)kShadowSize};
            const bool shadowPassBegan = cmd->BeginRenderPass(hShadowRP, hShadowFBO, shadowArea);
            if (shadowPassBegan && useRealShadowPass && hShadowPipe.IsValid()) {
                NkViewport svp{0.f, 0.f, (float)kShadowSize, (float)kShadowSize, 0.f, 1.f};
                svp.flipY = false; // shadow map depth pass : pas de Y-flip Vulkan
                cmd->SetViewport(svp);
                cmd->SetScissor(shadowArea);
                cmd->BindGraphicsPipeline(hShadowPipe);

                auto fillUboShadow = [&](UboData& su, const NkMat4f& mm) {
                    NkMat4f identity = NkMat4f::Identity();
                    Mat4ToArray(mm,         su.model);
                    Mat4ToArray(matLightVP, su.lightVP);
                    Mat4ToArray(identity,   su.view);
                    Mat4ToArray(identity,   su.proj);
                    su.ndcZScale  = ndcZScale;
                    su.ndcZOffset = ndcZOffset;
                };

                // Objet 0 : Cube rotatif
                {
                    NkMat4f mm = NkMat4f::RotationY(NkAngle(rotAngle))
                            * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));
                    UboData su{};
                    fillUboShadow(su, mm);
                    device->WriteBuffer(hUBO[0], &su, sizeof(su));
                    if (hDescSet[0].IsValid()) cmd->BindDescriptorSet(hDescSet[0], 0);
                    cmd->BindVertexBuffer(0, hCube);
                    cmd->Draw((uint32)cubeVerts.Size());
                }

                // Objet 1 : Sphere
                {
                    NkMat4f mm = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
                    UboData su{};
                    fillUboShadow(su, mm);
                    device->WriteBuffer(hUBO[1], &su, sizeof(su));
                    if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
                    cmd->BindVertexBuffer(0, hSphere);
                    cmd->Draw((uint32)sphereVerts.Size());
                }

                // Objet 2 : Plan
                {
                    NkMat4f mm = NkMat4f::Translation(NkVec3f(0.f, -1.0f, 0.f));
                    UboData su{};
                    fillUboShadow(su, mm);
                    device->WriteBuffer(hUBO[2], &su, sizeof(su));
                    if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
                    cmd->BindVertexBuffer(0, hPlane);
                    cmd->Draw((uint32)planeVerts.Size());
                }
            }

            // En mode safe-clear, cette passe sert juste a clear le depth shadow a 1.0.
            if (shadowPassBegan) cmd->EndRenderPass();

            // Synchronise l'écriture depth de la shadow map avant lecture en fragment shader.
            if (shadowPassBegan && hShadowTex.IsValid()) {
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
        if (!cmd->BeginRenderPass(hRP, hFBO, area)) {
            cmd->End();
            if (targetApi == NkGraphicsApi::NK_API_VULKAN && W > 0 && H > 0) {
                device->OnResize(W, H);
            }
            device->EndFrame(frame);
            continue;
        }

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
            ubo.ndcZScale    = ndcZScale;
            ubo.ndcZOffset   = ndcZOffset;
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
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(0.f, -1.0f, 0.f));
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
    window.Close();

    logger.Info("[RHIFullDemo] Terminé proprement.\n");
    return 0;
}