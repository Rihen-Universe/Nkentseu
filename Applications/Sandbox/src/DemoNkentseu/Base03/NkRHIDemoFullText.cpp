// =============================================================================
// NkRHIDemoFull.cpp
// DÃ©mo RHI complÃ¨te utilisant la couche NkIDevice (NKRenderer)
//
//   â€¢ Multi-backend : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//     sÃ©lectionnable via argument ligne de commande : --backend=opengl|vulkan|dx11|dx12|sw|metal
//   â€¢ GÃ©omÃ©trie 3D : cube rotatif + sphÃ¨re + plan (sol)
//   â€¢ Ã‰clairage Phong directionnel (ambient + diffuse + specular)
//   â€¢ Shadow mapping multi-backend (OpenGL / Vulkan / DX11 / DX12 / Software)
//   â€¢ NKMath : NkVec3f, NkMat4f (column-major, OpenGL convention)
//
// Pipeline d'initialisation :
//   NkWindow â†’ NkDeviceFactory â†’ ressources RHI â†’ boucle
// =============================================================================

// â”€â”€ DÃ©tection plateforme â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"

// â”€â”€ Headers NKEngine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"

// â”€â”€ RHI â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"

// â”€â”€ NKMath â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NKMath/NKMath.h"

// â”€â”€ NKLogger â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NKLogger/NkLog.h"
#include "NKImage/Core/NkImage.h"

// â”€â”€ std â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "NKContainers/Sequential/NkVector.h"

// â”€â”€ SPIR-V prÃ©compilÃ© pour Vulkan â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#include "NkRHIDemoFullVkSpv.inl"
#include "NkRHIDemoFullImageVkSpv.inl"
#include "NkRHIDemoFullTextVkSpv.inl"
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

namespace nkentseu {
    struct NkEntryState;
}

// =============================================================================
// Shaders GLSL (OpenGL 4.6)
// =============================================================================

// â”€â”€ Shadow depth pass : position seule, matrix = lightVP * model â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// static constexpr const char* kGLSL_ShadowVert = R"GLSL(
// #version 460 core
// layout(location = 0) in vec3 aPos;

// // On utilise un UBO sÃ©parÃ© pour la passe shadow (binding 0)
// layout(std140, binding = 0) uniform ShadowUBO {
//     mat4 model;
//     mat4 view;       // non utilisÃ© dans cette passe
//     mat4 proj;       // non utilisÃ© dans cette passe
//     mat4 lightVP;
//     vec4 lightDirW;
//     vec4 eyePosW;
// } ubo;

// void main() {
//     // Transforme directement dans l'espace lumiÃ¨re
//     gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
// }
// )GLSL";

// static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
// #version 460 core
// // Pas de sortie couleur â€” on Ã©crit uniquement le depth buffer
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

// â”€â”€ Passe principale Phong + shadow mapping PCF â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

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
layout(location = 3) out vec4 vShadowCoord;  // coordonnÃ©es dans l'espace lumiÃ¨re (non-divisÃ©es)
layout(location = 4) out vec2 vUV;

void main() {
    vec4 worldPos     = ubo.model * vec4(aPos, 1.0);
    vWorldPos         = worldPos.xyz;

    // Matrice normale = transposÃ©e inverse de la partie 3x3 du model
    // (correct mÃªme pour les objets non-uniformÃ©ment mis Ã  l'Ã©chelle)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    vNormal           = normalize(normalMatrix * aNormal);

    vColor            = aColor;

    // CoordonnÃ©es shadow : espace lumiÃ¨re homogÃ¨ne, pas encore divisÃ©es par w
    vShadowCoord      = ubo.lightVP * worldPos;
    vUV               = aUV;

    gl_Position       = ubo.proj * ubo.view * worldPos;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec4 vShadowCoord;   // coordonnÃ©es lumiÃ¨re homogÃ¨nes
layout(location = 4) in vec2 vUV;
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
layout(binding = 1) uniform sampler2DShadow uShadowMap;
layout(binding = 2) uniform sampler2D uAlbedoMap;

float ShadowFactor(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z  = projCoords.z * ubo.ndcZScale + ubo.ndcZOffset;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);
    projCoords.z -= bias;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(uShadowMap, vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    vec3 V = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H = normalize(L + V);
    vec3 baseColor = vColor;
    if (vColor.g > vColor.r && vColor.g > vColor.b) {
        baseColor *= texture(uAlbedoMap, clamp(vUV, 0.0, 1.0)).rgb;
    }
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    float shadow = max(ShadowFactor(vShadowCoord), 0.35);
    vec3 ambient  = 0.15 * baseColor;
    vec3 diffuse  = shadow * diff * baseColor;
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
struct VSIn  { float3 pos:POSITION; float3 norm:NORMAL; float3 col:COLOR; float2 uv:TEXCOORD0; };
struct VSOut {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
    float2 uv:TEXCOORD4;
};
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos,1));
    o.wp  = wp.xyz;
    o.n   = normalize(mul((float3x3)model, v.norm));
    o.c   = v.col;
    o.shadowPos = mul(lightVP, wp);
    o.uv = v.uv;
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
Texture2D uAlbedoMap : register(t2);
SamplerState uAlbedoSampler : register(s2);
struct PSIn {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
    float2 uv:TEXCOORD4;
};

float ShadowFactor(float4 shadowPos, float3 N) {
    float3 projCoords = shadowPos.xyz / shadowPos.w;
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
    float3 baseColor = i.c;
    if (i.c.y > i.c.x && i.c.y > i.c.z) {
        float2 uv = clamp(i.uv, float2(0.0, 0.0), float2(1.0, 1.0));
        baseColor *= uAlbedoMap.Sample(uAlbedoSampler, uv).rgb;
    }
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    float shadow = max(ShadowFactor(i.shadowPos, N), 0.35);
    float3 col = 0.15 * baseColor + shadow * diff * baseColor + shadow * spec * 0.4;
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

// Utilisation d'un constant buffer sÃ©parÃ© pour la passe shadow (binding 0)
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
    
    // Transforme directement dans l'espace lumiÃ¨re
    output.position = mul(lightVP, mul(model, float4(input.aPos, 1.0)));
    
    return output;
}
)HLSL";

static constexpr const char* kHLSL_ShadowFrag = R"HLSL(
// NkShadowFrag.hlsl
struct PSInput {
    float4 position : SV_POSITION;
};

// Pas de sortie couleur â€” on Ã©crit uniquement le depth buffer
// En HLSL, si on n'Ã©crit pas de couleur, le pipeline utilise la valeur de depth
// dÃ©jÃ  prÃ©sente de l'Ã©tape de rasterization
void main(PSInput input) {
    // Rien Ã  faire â€” le depth buffer est automatiquement mis Ã  jour
    // On peut Ã©ventuellement faire early depth test optimizations
}
)HLSL";

// =============================================================================
// Shaders TEXT (2D overlay + 3D billboard)
// binding 0 = UBO { mat4 mvp; vec4 color; }
// binding 1 = sampler2D uText (Gray8 atlas)
// =============================================================================
static constexpr const char* kGLSL_TextVert = R"GLSL(
#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(std140, binding = 0) uniform TextUBO { mat4 mvp; vec4 color; } ubo;
layout(location = 0) out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = ubo.mvp * vec4(aPos, 0.0, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_TextFrag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(std140, binding = 0) uniform TextUBO { mat4 mvp; vec4 color; } ubo;
layout(binding = 1) uniform sampler2D uText;
layout(location = 0) out vec4 outColor;
void main() {
    float a = texture(uText, vUV).r;
    outColor = vec4(ubo.color.rgb, ubo.color.a * a);
}
)GLSL";

static constexpr const char* kHLSL_TextVS = R"HLSL(
cbuffer TextUBO : register(b0) { column_major float4x4 mvp; float4 color; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
VSOut VSMain(VSIn v) {
    VSOut o;
    o.pos = mul(mvp, float4(v.pos, 0.0, 1.0));
    o.uv  = v.uv;
    return o;
}
)HLSL";

static constexpr const char* kHLSL_TextPS = R"HLSL(
cbuffer TextUBO : register(b0) { column_major float4x4 mvp; float4 color; };
Texture2D uText : register(t1);
SamplerState uTextSampler : register(s1);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
float4 PSMain(PSIn i) : SV_Target {
    float a = uText.Sample(uTextSampler, i.uv).r;
    return float4(color.rgb, color.a * a);
}
)HLSL";

static constexpr const char* kMSL_TextShaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct TextUBO { float4x4 mvp; float4 color; };
struct TVSIn  { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct TVSOut { float4 pos [[position]]; float2 uv; };
vertex TVSOut tvmain(TVSIn v [[stage_in]], constant TextUBO& u [[buffer(0)]]) {
    TVSOut o;
    o.pos = u.mvp * float4(v.pos, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
fragment float4 tfmain(TVSOut i [[stage_in]],
                       constant TextUBO& u [[buffer(0)]],
                       texture2d<float> tex [[texture(1)]],
                       sampler smp [[sampler(1)]]) {
    float a = tex.sample(smp, i.uv).r;
    return float4(u.color.rgb, u.color.a * a);
}
)MSL";

// =============================================================================
// Shaders MSL (Metal)
// =============================================================================
static constexpr const char* kMSL_Shaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UBO { float4x4 model,view,proj; float4 lightDirW,eyePosW; };
struct VSIn  { float3 pos [[attribute(0)]]; float3 norm [[attribute(1)]]; float3 col [[attribute(2)]]; float2 uv [[attribute(3)]]; };
struct VSOut { float4 pos [[position]]; float3 wp,n,c; float2 uv; };
vertex VSOut vmain(VSIn v [[stage_in]], constant UBO& u [[buffer(0)]]) {
    VSOut o;
    float4 wp = u.model * float4(v.pos,1);
    o.wp = wp.xyz;
    o.n  = normalize(float3x3(u.model[0].xyz,u.model[1].xyz,u.model[2].xyz) * v.norm);
    o.c  = v.col;
    o.uv = v.uv;
    o.pos = u.proj * u.view * wp;
    return o;
}
fragment float4 fmain(VSOut i [[stage_in]],
                      constant UBO& u [[buffer(0)]],
                      texture2d<float> albedoMap [[texture(2)]],
                      sampler albedoSampler [[sampler(2)]]) {
    float3 N = normalize(i.n);
    float3 L = normalize(-u.lightDirW.xyz);
    float3 V = normalize(u.eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float3 baseColor = i.c;
    if (i.c.y > i.c.x && i.c.y > i.c.z) {
        float2 uv = clamp(i.uv, float2(0.0), float2(1.0));
        baseColor *= albedoMap.sample(albedoSampler, uv).rgb;
    }
    float diff = max(dot(N,L), 0.0f);
    float spec = pow(max(dot(N,H), 0.0f), 32.0f);
    float3 col = 0.15f*baseColor + diff*baseColor + spec*0.4f;
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
    float lightVP[16];   // matrice VP de la lumiÃ¨re pour le shadow mapping
    float lightDirW[4];  // direction lumiÃ¨re world space (xyz) + pad
    float eyePosW[4];    // position camÃ©ra world space (xyz) + pad
    float ndcZScale;     // 0.5 pour OpenGL/SW (z NDC âˆˆ [-1,1]), 1.0 pour Vulkan/DX/Metal
    float ndcZOffset;    // 0.5 pour OpenGL/SW, 0.0 pour Vulkan/DX/Metal
    float _pad[2];
};

// =============================================================================
// GÃ©omÃ©trie
// =============================================================================
using namespace nkentseu;
using namespace nkentseu::math;

struct Vtx3D { NkVec3f pos; NkVec3f normal; NkVec3f color; NkVec2f uv; };

static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

// Cube (36 vertices, 6 faces Ã— 2 triangles)
static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P = 0.5f, N = -0.5f;
    struct Face { float vx[4][3]; float uv[4][2]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, {{1,0},{1,1},{0,1},{0,0}}, 0, 0, 1},   // front  +Z
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, {{1,0},{1,1},{0,1},{0,0}}, 0, 0,-1},   // back   -Z
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, {{0,1},{1,1},{1,0},{0,0}}, 0,-1, 0},   // bottom -Y
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, {{0,1},{1,1},{1,0},{0,0}}, 0, 1, 0},   // top    +Y
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}}, {{0,0},{1,0},{1,1},{0,1}},-1, 0, 0},   // left   -X
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, {{0,0},{1,0},{1,1},{0,1}}, 1, 0, 0},   // right  +X
    };
    static const int idx[6] = {0,1,2, 0,2,3};
    NkVector<Vtx3D> v;
    v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]),
                        NkVec3f(f.nx,f.ny,f.nz),
                        NkVec3f(r,g,b),
                        NkVec2f(f.uv[i][0], f.uv[i][1])});
    return v;
}

// SphÃ¨re UV
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
                const float u = th / (2.f * pi);
                const float vCoord = phi / pi;
                return {NkVec3f(x*.5f,y*.5f,z*.5f), NkVec3f(x,y,z), NkVec3f(r,g,b), NkVec2f(u, vCoord)};
            };
            Vtx3D a=mk(phi0,th0), b2=mk(phi0,th1), c=mk(phi1,th0), d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

// Plan sol
static NkVector<Vtx3D> MakePlane(float sz=3.f, float r=0.35f, float g=0.65f, float b=0.35f) {
    float h = sz * 0.5f;
    NkVector<Vtx3D> v;
    v.Reserve(6);
    // Triangle 1
    v.PushBack({NkVec3f(-h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(0.f, 0.f)});
    v.PushBack({NkVec3f( h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(1.f, 0.f)});
    v.PushBack({NkVec3f( h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(1.f, 1.f)});
    // Triangle 2
    v.PushBack({NkVec3f(-h,0.f, h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(0.f, 0.f)});
    v.PushBack({NkVec3f( h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(1.f, 1.f)});
    v.PushBack({NkVec3f(-h,0.f,-h), NkVec3f(0,1,0), NkVec3f(r,g,b), NkVec2f(0.f, 1.f)});
    return v;
}

// =============================================================================
// Selection du backend
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

static bool IsSupportedImageExtension(const std::string& extLower) {
    return extLower == ".png"  || extLower == ".jpg"  || extLower == ".jpeg" ||
           extLower == ".bmp"  || extLower == ".tga"  || extLower == ".hdr"  ||
           extLower == ".ppm"  || extLower == ".pgm"  || extLower == ".pbm"  ||
           extLower == ".qoi"  || extLower == ".gif"  || extLower == ".ico"  ||
           extLower == ".webp" || extLower == ".svg";
}

static std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static int TextureCandidateScore(const std::filesystem::path& p) {
    const std::string pathLower = ToLowerCopy(p.string());
    const std::string fileLower = ToLowerCopy(p.filename().string());
    int score = 0;

    if (fileLower.find("checker") != std::string::npos) score += 200;
    if (fileLower.find("albedo") != std::string::npos) score += 850;
    if (fileLower.find("diffuse") != std::string::npos) score += 900;
    if (fileLower.find("basecolor") != std::string::npos) score += 820;
    if (fileLower.find("color") != std::string::npos) score += 300;
    if (fileLower.find("brickwall") != std::string::npos) score += 300;
    if (fileLower.find("brick") != std::string::npos) score += 120;

    if (fileLower.find("ao") != std::string::npos ||
        fileLower.find("ambient") != std::string::npos) score -= 800;
    if (fileLower.find("normal") != std::string::npos) score -= 900;
    if (fileLower.find("rough") != std::string::npos) score -= 900;
    if (fileLower.find("metal") != std::string::npos) score -= 900;
    if (fileLower.find("spec") != std::string::npos) score -= 700;
    if (fileLower.find("height") != std::string::npos ||
        fileLower.find("disp") != std::string::npos ||
        fileLower.find("displace") != std::string::npos) score -= 700;
    if (fileLower.find("opacity") != std::string::npos ||
        fileLower.find("alpha") != std::string::npos ||
        fileLower.find("mask") != std::string::npos) score -= 300;

    if (pathLower.find("resources\\textures") != std::string::npos ||
        pathLower.find("resources/textures") != std::string::npos) {
        score += 120;
    }
    const std::string ext = ToLowerCopy(p.extension().string());
    if (ext == ".jpg" || ext == ".jpeg") score += 40;
    if (ext == ".png") score += 20;
    return score;
}

static std::string FindTextureInResources() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    auto pushUniqueRoot = [&](const fs::path& p) {
        const fs::path norm = p.lexically_normal();
        for (const fs::path& existing : roots) {
            if (existing == norm) return;
        }
        roots.push_back(norm);
    };

    pushUniqueRoot(fs::path("Resources/Textures"));
    pushUniqueRoot(fs::path("Resources"));

    std::error_code cwdEc;
    fs::path cursor = fs::current_path(cwdEc);
    if (!cwdEc && !cursor.empty()) {
        for (int i = 0; i < 8; ++i) {
            pushUniqueRoot(cursor / "Resources/Textures");
            pushUniqueRoot(cursor / "Resources");
            if (!cursor.has_parent_path() || cursor.parent_path() == cursor) {
                break;
            }
            cursor = cursor.parent_path();
        }
    }

    struct Candidate {
        std::string path;
        int score = 0;
    };

    std::vector<Candidate> candidates;
    for (const fs::path& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
            continue;
        }
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (IsSupportedImageExtension(ext)) {
                Candidate c;
                std::error_code absEc;
                fs::path absPath = fs::absolute(it->path(), absEc);
                c.path = (absEc ? it->path() : absPath).lexically_normal().string();
                c.score = TextureCandidateScore(it->path());
                candidates.push_back(c);
            }
        }
        if (!candidates.empty()) break;
    }

    if (candidates.empty()) return {};
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.path < b.path;
              });
    return candidates.front().path;
}
static bool SaveImageVariants(const NkImage& image,
                              const std::filesystem::path& outputDir,
                              const std::string& stem) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        logger.Info("[RHIFullDemoImage] Echec creation dossier sortie: {0}\\n", outputDir.string().c_str());
        return false;
    }

    const std::string base = (outputDir / stem).string();
    bool anyOk = false;

    const bool okPng  = image.SavePNG ((base + ".png").c_str());   anyOk = anyOk || okPng;
    const bool okJpg  = image.SaveJPEG((base + ".jpg").c_str(), 92); anyOk = anyOk || okJpg;
    const bool okBmp  = image.SaveBMP ((base + ".bmp").c_str());   anyOk = anyOk || okBmp;
    const bool okTga  = image.SaveTGA ((base + ".tga").c_str());   anyOk = anyOk || okTga;
    const bool okPpm  = image.SavePPM ((base + ".ppm").c_str());   anyOk = anyOk || okPpm;
    const bool okQoi  = image.SaveQOI ((base + ".qoi").c_str());   anyOk = anyOk || okQoi;
    const bool okHdr  = image.SaveHDR ((base + ".hdr").c_str());   anyOk = anyOk || okHdr;
    logger.Info("[RHIFullDemoImage] Save {0} => png={1} jpg={2} bmp={3} tga={4} ppm={5} qoi={6} hdr={7}\\n",
                stem.c_str(),
                okPng ? 1 : 0, okJpg ? 1 : 0, okBmp ? 1 : 0, okTga ? 1 : 0, okPpm ? 1 : 0,
                okQoi ? 1 : 0, okHdr ? 1 : 0);
    return anyOk;
}

static bool SaveSoftwareScene(NkSoftwareDevice* swDev,
                              const std::filesystem::path& outputDir,
                              const std::string& stem) {
    if (!swDev) return false;
    const uint8* pixels = swDev->BackbufferPixels();
    const uint32 w = swDev->BackbufferWidth();
    const uint32 h = swDev->BackbufferHeight();
    if (!pixels || w == 0 || h == 0) return false;

    NkImage* wrapped = NkImage::Wrap(const_cast<uint8*>(pixels),
                                     (int32)w,
                                     (int32)h,
                                     NkImagePixelFormat::NK_RGBA32);
    if (!wrapped || !wrapped->IsValid()) {
        if (wrapped) wrapped->Free();
        return false;
    }

    const bool ok = SaveImageVariants(*wrapped, outputDir, stem);
    wrapped->Free();
    return ok;
}

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
            AddSpirvOrFallback(sd, NkShaderStage::NK_VERTEX,   kGLSL_Vert, "Phong3DText.vert",
                kVkRHIFullDemoImageVertSpv, (uint64)kVkRHIFullDemoImageVertSpvWordCount * sizeof(uint32));
            AddSpirvOrFallback(sd, NkShaderStage::NK_FRAGMENT, kGLSL_Frag, "Phong3DText.frag",
                kVkRHIFullDemoImageFragSpv, (uint64)kVkRHIFullDemoImageFragSpvWordCount * sizeof(uint32));
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
            // Shadow pass MSL non implÃ©mentÃ© (Metal sans ombres)
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
// TEXT RENDERING — structs + helpers
// =============================================================================

// Vertex du texte : position 2D + UV
struct TextVtx { float x, y, u, v; };

// UBO du texte (std140-compatible : mat4 = 64 bytes + vec4 = 16 bytes)
struct alignas(16) TextUboData {
    float mvp[16];
    float color[4];
};

// Quad texte GPU (une ligne de texte = une texture + un VBO)
struct NkTextQuad {
    NkTextureHandle hTex;
    NkSamplerHandle hSampler;
    NkBufferHandle  hVBO;
    NkBufferHandle  hUBO;
    NkDescSetHandle hDesc;
    uint32          texW = 0;
    uint32          texH = 0;
    uint32          vtxCount = 0;
};

// Cherche un fichier de police TTF dans Resources/Fonts
static std::string FindFontFile() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    auto pushRoot = [&](const fs::path& p) {
        fs::path n = p.lexically_normal();
        for (auto& e : roots) if (e == n) return;
        roots.push_back(n);
    };
    pushRoot(fs::path("Resources/Fonts"));
    std::error_code ec;
    fs::path cursor = fs::current_path(ec);
    if (!ec) {
        for (int i = 0; i < 8; ++i) {
            pushRoot(cursor / "Resources/Fonts");
            if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
            cursor = cursor.parent_path();
        }
    }
    const std::vector<std::string> preferred = {
        "Roboto-Regular.ttf", "Roboto-Light.ttf", "OpenSans-Light.ttf",
        "Antonio-Regular.ttf", "Antonio-Light.ttf", "OCRAEXT.TTF"
    };
    for (const fs::path& root : roots) {
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) continue;
        for (const auto& pref : preferred) {
            fs::path candidate = root / pref;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                fs::path abs = fs::absolute(candidate, ec);
                return (ec ? candidate : abs).lexically_normal().string();
            }
        }
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            const std::string ext = ToLowerCopy(it->path().extension().string());
            if (ext == ".ttf" || ext == ".otf") {
                fs::path abs = fs::absolute(it->path(), ec);
                return (ec ? it->path() : abs).lexically_normal().string();
            }
        }
    }
    return {};
}

static std::vector<uint8_t> LoadFileBytes(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)sz);
    fread(buf.data(), 1, (size_t)sz, f);
    fclose(f);
    return buf;
}

// Rasterise une chaîne en bitmap Gray8 via NkFTFontFace
static std::vector<uint8_t> BuildStringBitmap(NkFontFace* face, const char* text, uint32& outW, uint32& outH) {
    if (!face || !text || !*text) { outW = outH = 0; return {}; }
    const int32 ascender  = face->GetAscender();
    const int32 descender = face->GetDescender();
    const int32 rowH      = ascender - descender;
    if (rowH <= 0) { outW = outH = 0; return {}; }

    // Mesurer la largeur totale
    uint32 totalW = 0;
    for (const char* p = text; *p; ++p) {
        NkGlyph g{};
        // if (face->GetGlyph((uint8_t)*p, g)) totalW += (uint32)g.xAdvance;
    }
    if (totalW == 0) totalW = 1;

    outW = totalW;
    outH = (uint32)rowH;
    std::vector<uint8_t> bmp(outW * outH, 0u);

    int32 penX = 0;
    // for (const char* p = text; *p; ++p) {
    //     NkGlyph g{};
    //     if (!face->GetGlyph((uint8_t)*p, g) || g.isEmpty || !g.bitmap) {
    //         if (face->GetGlyph((uint8_t)*p, g)) penX += g.xAdvance;
    //         continue;
    //     }
    //     int32 dstX = penX + g.bearingX;
    //     int32 dstY = ascender - g.bearingY;
    //     for (int32 gy = 0; gy < g.height; ++gy) {
    //         int32 dy = dstY + gy;
    //         if (dy < 0 || dy >= (int32)outH) continue;
    //         const uint8_t* srcRow = g.bitmap + gy * g.pitch;
    //         uint8_t* dstRow = bmp.data() + dy * outW;
    //         for (int32 gx = 0; gx < g.width; ++gx) {
    //             int32 dx = dstX + gx;
    //             if (dx < 0 || dx >= (int32)outW) continue;
    //             uint32 s = srcRow[gx], d = dstRow[dx];
    //             dstRow[dx] = (uint8_t)(d + s - (d * s) / 255u);
    //         }
    //     }
    //     penX += g.xAdvance;
    // }
    return bmp;
}

// MVP orthographique 2D pour overlay (pixels, coin haut-gauche)
// flipOrthoY : inverser l'axe Y pour Vulkan/DX12 dont le clip-space Y est inverse
static void Ortho2DMVP(float x, float y, float scaleX, float scaleY,
                        float W, float H, bool depthZeroToOne, bool flipOrthoY, float out[16]) {
    float l = 0.f, r = W;
    // Normal : t=0 (haut), b=H (bas) => y=0 -> clip Y=+1 (haut en OGL/DX)
    // Flip   : t=H, b=0              => y=0 -> clip Y=-1 (haut en Vulkan ou clip Y-1=top)
    float t = flipOrthoY ? H : 0.f;
    float b = flipOrthoY ? 0.f : H;
    float nearZ = -1.f, farZ = 1.f;
    float rl = 1.f / (r - l), tb = 1.f / (t - b), fn = 1.f / (farZ - nearZ);
    float ortho[16] = {
        2.f*rl,     0.f,        0.f,                                    0.f,
        0.f,        2.f*tb,     0.f,                                    0.f,
        0.f,        0.f,        (depthZeroToOne ?  fn : -2.f*fn),       0.f,
        -(r+l)*rl, -(t+b)*tb,  (depthZeroToOne ? -nearZ*fn : (farZ+nearZ)*fn), 1.f
    };
    float model[16] = {
        scaleX, 0.f,    0.f, 0.f,
        0.f,    scaleY, 0.f, 0.f,
        0.f,    0.f,    1.f, 0.f,
        x,      y,      0.f, 1.f
    };
    // MVP = ortho * model  (column-major)
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++) {
            float s = 0.f;
            for (int k = 0; k < 4; k++) s += ortho[k*4+row] * model[col*4+k];
            out[col*4+row] = s;
        }
}

// MVP billboard 3D : le quad fait face a la camera a la position worldPos
// flipBillY : inverser le vecteur up pour Vulkan/DX12 (clip Y inverse).
//   Sans flip : le billboard croit vers le haut en world-space mais apparait
//               vers le bas sur ecran Vulkan (clip Y+1=bas).
//   Avec flip : on inverse le sens => apparait au-dessus de l'objet sur ecran Vulkan.
static void Billboard3DMVP(const NkVec3f& worldPos, float worldW, float worldH,
                             const NkMat4f& view, const NkMat4f& proj,
                             bool flipBillY, float out[16]) {
    float rx = view.data[0], ry = view.data[4], rz = view.data[8];
    float ux = view.data[1], uy = view.data[5], uz = view.data[9];
    if (flipBillY) { ux = -ux; uy = -uy; uz = -uz; }
    NkMat4f model = NkMat4f::Identity();
    model.data[0] = rx * worldW;  model.data[1] = ry * worldW;
    model.data[2] = rz * worldW;  model.data[3] = 0.f;
    model.data[4] = ux * worldH;  model.data[5] = uy * worldH;
    model.data[6] = uz * worldH;  model.data[7] = 0.f;
    model.data[8] = 0.f; model.data[9] = 0.f; model.data[10] = 1.f; model.data[11] = 0.f;
    model.data[12] = worldPos.x; model.data[13] = worldPos.y;
    model.data[14] = worldPos.z; model.data[15] = 1.f;
    NkMat4f mvp = proj * view * model;
    mem::NkCopy(out, mvp.data, 16 * sizeof(float));
}

// ShaderDesc pour le pipeline texte
static NkShaderDesc MakeTextShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd;
    sd.debugName = "Text2D3D";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_TextVS, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_TextPS, "PSMain");
            break;
        case NkGraphicsApi::NK_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_TextShaders, "tvmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_TextShaders, "tfmain");
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkTextVertSpv,
                        (uint64)kVkTextVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkTextFragSpv,
                        (uint64)kVkTextFragSpvWordCount * sizeof(uint32));
            break;
        default:
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_TextVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_TextFrag);
            break;
    }
    return sd;
}

// flipV : inverser les coordonnees V de la texture.
//   flipV=false : V=0 en bas du quad (y=0), V=1 en haut (y=1).
//                 Correct pour l'overlay 2D (apres flipOrthoY, y=0=haut ecran, V=0=ascender).
//   flipV=true  : V=1 en bas du quad, V=0 en haut.
//                 Correct pour billboard 3D (y=1=haut ecran, V=0=ascender).
static NkTextQuad CreateTextQuad(NkIDevice* device, NkDescSetHandle hTextLayout,
                                  NkFontFace* face, const char* text, bool flipV = false) {
    NkTextQuad q{};
    if (!device || !face || !text || !*text) return q;
    uint32 bmpW = 0, bmpH = 0;
    std::vector<uint8_t> bmp = BuildStringBitmap(face, text, bmpW, bmpH);
    if (bmp.empty() || bmpW == 0 || bmpH == 0) return q;

    NkTextureDesc texDesc = NkTextureDesc::Tex2D(bmpW, bmpH, NkGPUFormat::NK_R8_UNORM, 1);
    texDesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
    texDesc.debugName = text;
    q.hTex = device->CreateTexture(texDesc);
    if (!q.hTex.IsValid()) return q;
    device->WriteTexture(q.hTex, bmp.data(), bmpW);

    NkSamplerDesc sampDesc{};
    sampDesc.magFilter = NkFilter::NK_LINEAR;
    sampDesc.minFilter = NkFilter::NK_LINEAR;
    sampDesc.mipFilter = NkMipFilter::NK_NONE;
    sampDesc.addressU  = NkAddressMode::NK_CLAMP_TO_EDGE;
    sampDesc.addressV  = NkAddressMode::NK_CLAMP_TO_EDGE;
    sampDesc.addressW  = NkAddressMode::NK_CLAMP_TO_EDGE;
    q.hSampler = device->CreateSampler(sampDesc);
    if (!q.hSampler.IsValid()) {
        device->DestroyTexture(q.hTex); q.hTex = {}; return q;
    }
    // Quad [0..1]x[0..1] — V varie selon flipV
    float v0 = flipV ? 1.f : 0.f;  // V au bas du quad (y=0)
    float v1 = flipV ? 0.f : 1.f;  // V en haut du quad (y=1)
    const TextVtx verts[6] = {
        {0.f,0.f,0.f,v0},{1.f,0.f,1.f,v0},{1.f,1.f,1.f,v1},
        {0.f,0.f,0.f,v0},{1.f,1.f,1.f,v1},{0.f,1.f,0.f,v1},
    };
    q.hVBO      = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts), verts));
    q.hUBO      = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextUboData)));
    q.texW      = bmpW;
    q.texH      = bmpH;
    q.vtxCount  = 6;
    if (hTextLayout.IsValid()) {
        q.hDesc = device->AllocateDescriptorSet(hTextLayout);
        if (q.hDesc.IsValid()) {
            NkDescriptorWrite w[2]{};
            w[0].set=q.hDesc; w[0].binding=0; w[0].type=NkDescriptorType::NK_UNIFORM_BUFFER;
            w[0].buffer=q.hUBO; w[0].bufferOffset=0; w[0].bufferRange=sizeof(TextUboData);
            w[1].set=q.hDesc; w[1].binding=1; w[1].type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w[1].texture=q.hTex; w[1].sampler=q.hSampler;
            w[1].textureLayout=NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(w, 2);
        }
    }
    return q;
}

static void DestroyTextQuad(NkIDevice* device, NkTextQuad& q) {
    if (!device) return;
    if (q.hDesc.IsValid())    device->FreeDescriptorSet(q.hDesc);
    if (q.hUBO.IsValid())     device->DestroyBuffer(q.hUBO);
    if (q.hVBO.IsValid())     device->DestroyBuffer(q.hVBO);
    if (q.hSampler.IsValid()) device->DestroySampler(q.hSampler);
    if (q.hTex.IsValid())     device->DestroyTexture(q.hTex);
    q = {};
}

static void RenderTextQuad(NkICommandBuffer* cmd, NkIDevice* device,
                            const NkTextQuad& q, NkPipelineHandle hTextPipe,
                            const float mvp[16], float r, float g, float b, float a) {
    if (!cmd || !device || !q.hVBO.IsValid() || !q.hUBO.IsValid() || !hTextPipe.IsValid()) return;
    TextUboData ubo{};
    mem::NkCopy(ubo.mvp, mvp, 16 * sizeof(float));
    ubo.color[0]=r; ubo.color[1]=g; ubo.color[2]=b; ubo.color[3]=a;
    device->WriteBuffer(q.hUBO, &ubo, sizeof(ubo));
    cmd->BindGraphicsPipeline(hTextPipe);
    if (q.hDesc.IsValid()) cmd->BindDescriptorSet(q.hDesc, 0);
    cmd->BindVertexBuffer(0, q.hVBO);
    cmd->Draw(q.vtxCount);
}

// =============================================================================
// nkmain â€” point d'entrÃ©e
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {

    // â”€â”€ SÃ©lection backend â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkShaderCache::Global().SetCacheDir("Build/ShaderCache");
    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    const char* apiName = NkGraphicsApiName(targetApi);
    logger.Info("[RHIFullDemo] Backend cible : {0}\n", apiName);

    // â”€â”€ FenÃªtre â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NkRHI Full Demo â€” {0}", apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Info("[RHIFullDemo] Ã‰chec crÃ©ation fenÃªtre\n");
        return 1;
    }

    // â”€â”€ Surface native + init info device â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo deviceInitInfo;
    deviceInitInfo.api = targetApi;
    deviceInitInfo.surface = surface;
    deviceInitInfo.height = window.GetSize().height;
    deviceInitInfo.width = window.GetSize().width;

    deviceInitInfo.context.vulkan.appName = "NkRHIDemoFull";
    deviceInitInfo.context.vulkan.engineName = "Unkeny";

    // â”€â”€ Device RHI â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkIDevice* device = NkDeviceFactory::Create(deviceInitInfo);
    if (!device || !device->IsValid()) {
        logger.Info("[RHIFullDemo] Ã‰chec crÃ©ation NkIDevice ({0})\n", apiName);
        window.Close();
        return 1;
    }
    logger.Info("[RHIFullDemo] Device {0} initialisÃ©. VRAM: {1} Mo\n",
                apiName, (unsigned long long)(device->GetCaps().vramBytes >> 20));

    // â”€â”€ Dimensions swapchain â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // â”€â”€ Shader principal â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    logger.Info("[RHIFullDemo] Init step: build main shader desc\n");
    NkShaderDesc shaderDesc = MakeShaderDesc(targetApi);
    logger.Info("[RHIFullDemo] Init step: create main shader begin\n");
    NkShaderHandle hShader = device->CreateShader(shaderDesc);
    logger.Info("[RHIFullDemo] Init step: create main shader done valid={0}\n", hShader.IsValid() ? 1 : 0);
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Ã‰chec compilation shader\n");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // â”€â”€ Vertex Layout â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0,               "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3*sizeof(float),  "NORMAL",  0)
        .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 6*sizeof(float),  "COLOR",   0)
        .AddAttribute(3, 0, NkGPUFormat::NK_RG32_FLOAT,  9*sizeof(float),  "TEXCOORD",0)
        .AddBinding(0, sizeof(Vtx3D));

    // â”€â”€ Render Pass swapchain (rÃ©cupÃ©rÃ© une fois, stable entre les frames) â”€â”€â”€â”€
    logger.Info("[RHIFullDemo] Init step: get swapchain render pass\n");
    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    // â”€â”€ Shadow map â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // shaderNeedsShadowSampler : le shader GPU lit la shadow map via un descriptor (binding 1).
    // wantsShadowResources : on crÃ©e les ressources RHI shadow (inclut Software).
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
    const bool shaderNeedsAlbedoSampler = targetApi != NkGraphicsApi::NK_API_SOFTWARE;
    static constexpr uint32 kShadowSize = 2048;

    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;
    bool hasShadowMap = false;
    bool useRealShadowPass = false;

    NkTextureHandle hAlbedoTex;
    NkSamplerHandle hAlbedoSampler;
    NkImage* loadedTextureImage = nullptr;
    std::string loadedTexturePath;

    // Descriptor set layout (binding 0 = UBO, binding 1 = shadow sampler, binding 2 = albedo sampler)
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    if (shaderNeedsShadowSampler)
        layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    if (shaderNeedsAlbedoSampler)
        layoutDesc.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    logger.Info("[RHIFullDemo] Init step: create descriptor set layout begin\n");
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);
    logger.Info("[RHIFullDemo] Init step: create descriptor set layout done valid={0}\n", hLayout.IsValid() ? 1 : 0);

    // â”€â”€ Pipeline principal â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
        logger.Info("[RHIFullDemo] Ã‰chec crÃ©ation pipeline\n");
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // â”€â”€ GÃ©omÃ©trie â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Uniform Buffers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // 3 UBO : un par objet (cube, sphÃ¨re, plan)
    // SÃ©parÃ©s pour Ã©viter les conflits d'Ã©criture entre passe shadow et passe principale
    NkBufferHandle hUBO[3];
    for (int i = 0; i < 3; i++) {
        hUBO[i] = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
        if (!hUBO[i].IsValid())
            logger.Info("[RHIFullDemo] Ã‰chec crÃ©ation UBO[{0}]\n", i);
    }
    logger.Info("[RHIFullDemo] Init step: UBO created {0}/{1}/{2}\n",
                hUBO[0].IsValid() ? 1 : 0, hUBO[1].IsValid() ? 1 : 0, hUBO[2].IsValid() ? 1 : 0);

    // â”€â”€ Descriptor Sets â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Ressources shadow â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (wantsShadowResources) {
        logger.Info("[RHIFullDemo] Shadow init begin ({0})\n", apiName);
        NkTextureDesc shadowTexDesc = NkTextureDesc::DepthStencil(
            kShadowSize, kShadowSize,
            NkGPUFormat::NK_D32_FLOAT,
            NkSampleCount::NK_S1);
        shadowTexDesc.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
        shadowTexDesc.debugName = "ShadowDepthTex";
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
            if (hLayout.IsValid())
                shadowPipeDesc.descriptorSetLayouts.PushBack(hLayout);

            hShadowPipe = device->CreateGraphicsPipeline(shadowPipeDesc);
            useRealShadowPass = hShadowShader.IsValid() && hShadowPipe.IsValid();
        }

        for (int i = 0; i < 3; i++) {
            if (hDescSet[i].IsValid() && hShadowTex.IsValid() && hShadowSampler.IsValid() && shaderNeedsShadowSampler) {
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

    if (shaderNeedsShadowSampler && !hasShadowMap) {
        logger.Info("[RHIFullDemo] Impossible de continuer: shader principal attend un shadow sampler valide ({0})\n", apiName);
        device->DestroyPipeline(hPipe);
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // Charge une texture depuis Resources (tous formats supportes par NKImage)
    loadedTexturePath = FindTextureInResources();
    if (!loadedTexturePath.empty()) {
        logger.Info("[RHIFullDemoImage] Texture candidate: {0}\\n", loadedTexturePath.c_str());
        loadedTextureImage = NkImage::Load(loadedTexturePath.c_str(), 4);
        // loadedTextureImage = NkImage::LoadSTB(loadedTexturePath.c_str(), 4);
        if (loadedTextureImage && loadedTextureImage->IsValid()) {
            logger.Info("[RHIFullDemoImage] Texture chargee: {0} ({1}x{2}, ch={3})\\n",
                        loadedTexturePath.c_str(),
                        loadedTextureImage->Width(),
                        loadedTextureImage->Height(),
                        loadedTextureImage->Channels());
        } else {
            logger.Info("[RHIFullDemoImage] Echec chargement texture candidate: {0}\\n",
                        loadedTexturePath.c_str());
        }
    }
    if (!loadedTextureImage || !loadedTextureImage->IsValid()) {
        if (loadedTextureImage) { loadedTextureImage->Free(); loadedTextureImage = nullptr; }
        loadedTextureImage = NkImage::Alloc(256, 256, NkImagePixelFormat::NK_RGBA32, true);
        if (loadedTextureImage && loadedTextureImage->IsValid()) {
            for (int y = 0; y < loadedTextureImage->Height(); ++y) {
                uint8* row = loadedTextureImage->RowPtr(y);
                for (int x = 0; x < loadedTextureImage->Width(); ++x) {
                    const int tile = ((x / 32) + (y / 32)) & 1;
                    const uint8 c = tile ? (uint8)220 : (uint8)40;
                    row[x * 4 + 0] = c;
                    row[x * 4 + 1] = c;
                    row[x * 4 + 2] = c;
                    row[x * 4 + 3] = 255;
                }
            }
            loadedTexturePath = "<procedural_checkerboard>";
            logger.Info("[RHIFullDemoImage] Fallback texture generee (checkerboard 256x256).\\n");
        }
    }
    if (loadedTextureImage && loadedTextureImage->IsValid()) {
        NkTextureDesc albedoDesc = NkTextureDesc::Tex2D(
            (uint32)loadedTextureImage->Width(),
            (uint32)loadedTextureImage->Height(),
            NkGPUFormat::NK_RGBA8_UNORM,
            1);
        albedoDesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        hAlbedoTex = device->CreateTexture(albedoDesc);
        if (hAlbedoTex.IsValid()) {
            if (!device->WriteTexture(hAlbedoTex,
                                      loadedTextureImage->Pixels(),
                                      (uint32)loadedTextureImage->Stride())) {
                logger.Info("[RHIFullDemoImage] Echec upload texture albedo.\\n");
            }
        }
        NkSamplerDesc albedoSamplerDesc{};
        albedoSamplerDesc.magFilter = NkFilter::NK_LINEAR;
        albedoSamplerDesc.minFilter = NkFilter::NK_LINEAR;
        albedoSamplerDesc.mipFilter = NkMipFilter::NK_NONE;
        albedoSamplerDesc.addressU = NkAddressMode::NK_CLAMP_TO_EDGE;
        albedoSamplerDesc.addressV = NkAddressMode::NK_CLAMP_TO_EDGE;
        albedoSamplerDesc.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
        hAlbedoSampler = device->CreateSampler(albedoSamplerDesc);
        for (int i = 0; i < 3; ++i) {
            if (!shaderNeedsAlbedoSampler) {
                break;
            }
            if (!hDescSet[i].IsValid() || !hAlbedoTex.IsValid() || !hAlbedoSampler.IsValid()) {
                continue;
            }
            NkDescriptorWrite tw{};
            tw.set = hDescSet[i];
            tw.binding = 2;
            tw.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            tw.texture = hAlbedoTex;
            tw.sampler = hAlbedoSampler;
            tw.textureLayout = NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(&tw, 1);
        }
    }
    if (!hAlbedoTex.IsValid() || (shaderNeedsAlbedoSampler && !hAlbedoSampler.IsValid())) {
        logger.Info("[RHIFullDemoImage] Impossible de continuer: texture/sampler invalides.\\n");
        device->DestroyPipeline(hPipe);
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);

        // Shadow shader : depth-only pass
        if (hShadowShader.IsValid()) {
            NkSWShader* swSh = swDev->GetShader(hShadowShader.id);
            if (swSh) {
                swSh->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware {
                    const Vtx3D* v = static_cast<const Vtx3D*>(vdata) + idx;
                    const UboData* ubo = static_cast<const UboData*>(udata);
                    NkVertexSoftware out;
                    auto mul4 = [](const float m[16], float x, float y, float z, float w) -> NkVec4f {
                        return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w, m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                       m[2]*x+m[6]*y+m[10]*z+m[14]*w, m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                    };
                    NkVec4f wp  = mul4(ubo->model,   v->pos.x, v->pos.y, v->pos.z, 1.f);
                    NkVec4f lsp = mul4(ubo->lightVP, wp.x, wp.y, wp.z, wp.w);
                    out.position = lsp;
                    return out;
                };
                swSh->fragFn = nullptr;
            }
        }

        NkSWShader* sw = swDev->GetShader(hShader.id);
        NkSWTexture* swShadowTex = hasShadowMap ? swDev->GetTex(hShadowTex.id) : nullptr;
        NkSWTexture* swAlbedoTex = hAlbedoTex.IsValid() ? swDev->GetTex(hAlbedoTex.id) : nullptr;

        if (sw) {
            sw->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware {
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
                    return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w, m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                   m[2]*x+m[6]*y+m[10]*z+m[14]*w, m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
                NkVec4f wp  = mul4(ubo->model,   v->pos.x, v->pos.y, v->pos.z, 1.f);
                NkVec4f vp  = mul4(ubo->view,    wp.x,  wp.y,  wp.z,  wp.w);
                NkVec4f cp  = mul4(ubo->proj,    vp.x,  vp.y,  vp.z,  vp.w);
                out.position = cp;

                float nx = ubo->model[0]*v->normal.x + ubo->model[4]*v->normal.y + ubo->model[8]*v->normal.z;
                float ny = ubo->model[1]*v->normal.x + ubo->model[5]*v->normal.y + ubo->model[9]*v->normal.z;
                float nz = ubo->model[2]*v->normal.x + ubo->model[6]*v->normal.y + ubo->model[10]*v->normal.z;
                float nl = NkSqrt(nx*nx + ny*ny + nz*nz);
                if (nl > 0.001f) { nx /= nl; ny /= nl; nz /= nl; }
                out.normal = {nx, ny, nz};
                out.color  = {v->color.r, v->color.g, v->color.b, 1.f};

                out.attrs[0] = wp.x; out.attrs[1] = wp.y; out.attrs[2] = wp.z;
                NkVec4f lsc = mul4(ubo->lightVP, wp.x, wp.y, wp.z, wp.w);
                out.attrs[3] = lsc.x; out.attrs[4] = lsc.y;
                out.attrs[5] = lsc.z; out.attrs[6] = lsc.w;
                out.attrs[7] = v->uv.x;
                out.attrs[8] = v->uv.y;
                return out;
            };

            sw->fragFn = [swShadowTex, swAlbedoTex](const NkVertexSoftware& frag, const void* udata, const void*) -> math::NkVec4f {
                const UboData* ubo = static_cast<const UboData*>(udata);
                float nx = frag.normal.x, ny = frag.normal.y, nz = frag.normal.z;

                float lx = -ubo->lightDirW[0], ly = -ubo->lightDirW[1], lz = -ubo->lightDirW[2];
                float ll2 = lx*lx + ly*ly + lz*lz;
                if (ll2 > 1e-6f) { float il = 1.f / NkSqrt(ll2); lx*=il; ly*=il; lz*=il; }
                float diff = nx*lx + ny*ly + nz*lz; if (diff < 0.f) diff = 0.f;

                float vx = ubo->eyePosW[0]-frag.attrs[0];
                float vy = ubo->eyePosW[1]-frag.attrs[1];
                float vz = ubo->eyePosW[2]-frag.attrs[2];
                float vl2 = vx*vx + vy*vy + vz*vz;
                if (vl2 > 1e-6f) { float iv = 1.f / NkSqrt(vl2); vx*=iv; vy*=iv; vz*=iv; }
                float hx = lx+vx, hy = ly+vy, hz = lz+vz;
                float hl2 = hx*hx + hy*hy + hz*hz;
                if (hl2 > 1e-6f) { float ih = 1.f / NkSqrt(hl2); hx*=ih; hy*=ih; hz*=ih; }
                float sp = nx*hx + ny*hy + nz*hz; if (sp < 0.f) sp = 0.f;
                float spec = sp*sp; spec *= spec; spec *= spec; spec *= spec;

                float br = frag.color.x;
                float bg = frag.color.y;
                float bb = frag.color.z;
                if (swAlbedoTex && bg > br && bg > bb) {
                    float u = NkClamp(frag.attrs[7], 0.f, 1.f);
                    float v = NkClamp(frag.attrs[8], 0.f, 1.f);
                    NkVec4f tex = swAlbedoTex->Sample(u, v, 0);
                    br *= tex.x; bg *= tex.y; bb *= tex.z;
                }

                float shadow = 1.f;
                if (swShadowTex) {
                    float sw4 = frag.attrs[6];
                    float invW = (sw4 != 0.f) ? 1.f / sw4 : 1.f;
                    float sx = frag.attrs[3] * invW;
                    float sy = frag.attrs[4] * invW;
                    float sz = frag.attrs[5] * invW;
                    float u = sx * 0.5f + 0.5f;
                    float v = -sy * 0.5f + 0.5f;
                    float cmpZ = sz * 0.5f + 0.5f - 0.002f;
                    if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f && cmpZ <= 1.f) {
                        uint32 px = (uint32)(u * (float)swShadowTex->Width());
                        uint32 py = (uint32)(v * (float)swShadowTex->Height());
                        if (px >= swShadowTex->Width())  px = swShadowTex->Width()-1;
                        if (py >= swShadowTex->Height()) py = swShadowTex->Height()-1;
                        shadow = (cmpZ <= swShadowTex->Read(px, py).r) ? 1.f : 0.f;
                    }
                }
                shadow = shadow < 0.35f ? 0.35f : shadow;

                float r = 0.15f*br + shadow*(diff*br + spec*0.4f);
                float g = 0.15f*bg + shadow*(diff*bg + spec*0.4f);
                float b = 0.15f*bb + shadow*(diff*bb + spec*0.4f);
                r = r < 0.f ? 0.f : (r > 1.f ? 1.f : r);
                g = g < 0.f ? 0.f : (g > 1.f ? 1.f : g);
                b = b < 0.f ? 0.f : (b > 1.f ? 1.f : b);
                return {r, g, b, 1.f};
            };
        }
    }

    // =========================================================================
    // Texte 2D overlay + 3D billboard
    // =========================================================================

    // Charger une police via FreeType
    // NkFTFontLibrary ftLib;
    NkFontLibrary ftLib;
    NkFontFace*   ftFace18 = nullptr;
    NkFontFace*   ftFace32 = nullptr;
    std::vector<uint8_t> fontBytes;
    {
        const std::string fontPath = FindFontFile();
        if (!fontPath.empty()) {
            fontBytes = LoadFileBytes(fontPath);
            logger.Info("[RHIFullText] Police: {0} ({1} octets)", fontPath.c_str(), (unsigned long long)fontBytes.size());
        }
        if (!fontBytes.empty()) {
            ftLib.Init();
            ftFace18 = ftLib.LoadFont(fontBytes.data(), (usize)fontBytes.size(), 18);
            ftFace32 = ftLib.LoadFont(fontBytes.data(), (usize)fontBytes.size(), 32);
        }
        // if (!ftFace18 || !ftFace18->IsValid())
        //     logger.Info("[RHIFullText] Avertissement: police 18px non chargee");
    }

    // Descriptor set layout texte (binding 0 = UBO, binding 1 = sampler2D)
    NkDescriptorSetLayoutDesc textLayoutDesc;
    textLayoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_ALL_GRAPHICS);
    textLayoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hTextLayout = device->CreateDescriptorSetLayout(textLayoutDesc);

    // Shader texte
    NkShaderDesc textShaderDesc = MakeTextShaderDesc(targetApi);
    NkShaderHandle hTextShader = device->CreateShader(textShaderDesc);

    // Vertex layout texte : float2 pos + float2 uv
    NkVertexLayout textVtxLayout;
    textVtxLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0,               "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 2*sizeof(float), "TEXCOORD", 0)
        .AddBinding(0, sizeof(TextVtx));

    // Pipeline texte (alpha blending, pas de depth write)
    NkPipelineHandle hTextPipe;
    if (hTextShader.IsValid() && hTextLayout.IsValid()) {
        NkGraphicsPipelineDesc tpd;
        tpd.shader       = hTextShader;
        tpd.vertexLayout = textVtxLayout;
        tpd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        tpd.rasterizer   = NkRasterizerDesc::Default();
        tpd.depthStencil = NkDepthStencilDesc::NoDepth();
        tpd.blend        = NkBlendDesc::Alpha();
        tpd.renderPass   = hRP;
        tpd.debugName    = "PipelineText";
        tpd.descriptorSetLayouts.PushBack(hTextLayout);
        hTextPipe = device->CreateGraphicsPipeline(tpd);
        if (!hTextPipe.IsValid())
            logger.Info("[RHIFullText] Avertissement: pipeline texte invalide");
    }

    // Quads texte : overlay 2D + labels 3D billboard
    NkTextQuad tqBackend = {};
    NkTextQuad tqFPS     = {};
    NkTextQuad tqCube    = {};
    NkTextQuad tqSphere  = {};
    NkTextQuad tqFloor   = {};
    bool textOk = hTextPipe.IsValid() && hTextLayout.IsValid();
    if (textOk) {
        // NkFontFace* f18 = (ftFace18 && ftFace18->IsValid()) ? ftFace18 : nullptr;
        // NkFontFace* f32 = (ftFace32 && ftFace32->IsValid()) ? ftFace32 : nullptr;
        // // 2D overlay : flipV=false (V=0=ascender en haut apres flipOrthoY)
        // // 3D billboard : flipV=true (V=0=ascender en haut apres flipBillY)
        // const bool flipBillboard = (targetApi == NkGraphicsApi::NK_API_VULKAN ||
        //                             targetApi == NkGraphicsApi::NK_API_DIRECTX12);
        // (void)flipBillboard; // utilise dans le render loop
        // if (f32) {
        //     tqBackend = CreateTextQuad(device, hTextLayout, f32, apiName, false);
        //     tqCube    = CreateTextQuad(device, hTextLayout, f32, "CUBE",   true);
        //     tqSphere  = CreateTextQuad(device, hTextLayout, f32, "SPHERE", true);
        //     tqFloor   = CreateTextQuad(device, hTextLayout, f32, "FLOOR",  true);
        // }
        // if (f18) {
        //     tqFPS = CreateTextQuad(device, hTextLayout, f18, "FPS: --", false);
        // }
    }
    float  fpsTimer      = 0.f;
    uint32 fpsFrameCount = 0;
    float  fpsValue      = 0.f;

    // â”€â”€ Command Buffer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger.Info("[RHIFullDemo] Ã‰chec crÃ©ation command buffer\n");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // â”€â”€ Ã‰tat de la simulation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    bool  running    = true;
    float rotAngle   = 0.f;
    float camYaw     = 0.f;
    float camPitch   = 20.f;
    float camDist    = 4.f;
    float lightYaw   = -45.f;
    float lightPitch = -30.f;
    bool  keys[512]  = {};
    bool  requestSaveScene  = false;
    bool  requestSaveSource = false;
    uint32 saveSceneCounter  = 0;
    uint32 saveSourceCounter = 0;
    const std::filesystem::path capturesDir = std::filesystem::path("Build") / "Captures" / "NkRHIDemoFullImage";

    NkClock clock;
    NkEventSystem& events = NkEvents();

    // â”€â”€ Callbacks Ã©vÃ©nements â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = true;
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
        if (e->GetKey() == NkKey::NK_F5) requestSaveScene = true;
        if (e->GetKey() == NkKey::NK_F6) requestSaveSource = true;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (uint32)e->GetWidth();
        H = (uint32)e->GetHeight();
    });

    // â”€â”€ Constantes de convention NDC selon l'API â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // depthZeroToOne : Vulkan/DX/Metal clip Z âˆˆ [0,1], OpenGL/SW clip Z âˆˆ [-1,1]
    const bool  depthZeroToOne =
        targetApi == NkGraphicsApi::NK_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX12 ||
        targetApi == NkGraphicsApi::NK_API_METAL;
    const float ndcZScale  = depthZeroToOne ? 1.0f : 0.5f;
    const float ndcZOffset = depthZeroToOne ? 0.0f : 0.5f;

    logger.Info("[RHIFullDemoImage] Boucle principale. ESC=quitter, WASD=camera, Fleches=lumiere, F5=save scene, F6=save source\n");

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

        // â”€â”€ Delta time â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        // FPS counter
        fpsFrameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 0.5f && textOk) {
            fpsValue = (fpsTimer > 0.f) ? (float)fpsFrameCount / fpsTimer : 0.f;
            fpsFrameCount = 0;
            fpsTimer = 0.f;
            // if (ftFace18 && ftFace18->IsValid() && hTextLayout.IsValid()) {
            //     char fpsBuf[32];
            //     snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.0f", fpsValue);
            //     DestroyTextQuad(device, tqFPS);
            //     tqFPS = CreateTextQuad(device, hTextLayout, ftFace18, fpsBuf, false);
            // }
        }


        // â”€â”€ ContrÃ´les clavier â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

        // â”€â”€ Matrices de transformation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        float aspect = (H > 0) ? (float)W / (float)H : 1.f;

        // CamÃ©ra orbitale
        float eyeX = camDist * NkCos(NkToRadians(camPitch)) * NkSin(NkToRadians(camYaw));
        float eyeY = camDist * NkSin(NkToRadians(camPitch));
        float eyeZ = camDist * NkCos(NkToRadians(camPitch)) * NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX, eyeY, eyeZ);
        NkVec3f center(0.f, 0.f, 0.f);
        NkVec3f up(0.f, 1.f, 0.f);

        NkMat4f matView = NkMat4f::LookAt(eye, center, up);
        NkMat4f matProj = NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 100.f);

        // Direction lumiÃ¨re
        float lx = NkCos(NkToRadians(lightPitch)) * NkSin(NkToRadians(lightYaw));
        float ly = NkSin(NkToRadians(lightPitch));
        float lz = NkCos(NkToRadians(lightPitch)) * NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx, ly, lz);

        // Matrice de la lumiÃ¨re (orthogonale, couvre les 3 objets)
        // La position de la lumiÃ¨re est opposÃ©e Ã  sa direction, Ã  distance suffisante
        NkVec3f lightPos = NkVec3f(-lightDir.x * 10.f,
                                    -lightDir.y * 10.f,
                                    -lightDir.z * 10.f);

        // Ã‰viter un up vector colinÃ©aire avec la direction lumiÃ¨re
        NkVec3f lightUp = (NkFabs(lightDir.y) > 0.9f)
                          ? NkVec3f(1.f, 0.f, 0.f)
                          : NkVec3f(0.f, 1.f, 0.f);

        NkMat4f matLightView = NkMat4f::LookAt(lightPos, center, lightUp);

        // Frustum orthogonal assez large pour couvrir toute la scÃ¨ne visible
        // (-5..5 en X/Y, 1..20 en profondeur)
        NkMat4f matLightProj = NkMat4f::Orthogonal(
            NkVec2f(-5.f, -5.f),
            NkVec2f( 5.f,  5.f),
            1.f, 20.f,
            depthZeroToOne);

        NkMat4f matLightVP = matLightProj * matLightView;

        // â”€â”€ Frame â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

        // RÃ©cupÃ©rer le framebuffer courant APRÃˆS BeginFrame (index mis Ã  jour)
        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();

        cmd->Reset();
        if (!cmd->Begin()) {
            device->EndFrame(frame);
            continue;
        }

        // =====================================================================
        // Passe 1 : Shadow map (depth-only, POV de la lumiÃ¨re)
        // =====================================================================
        if (hasShadowMap && hShadowFBO.IsValid() && hShadowRP.IsValid()) {

            NkRect2D shadowArea{0, 0, (int32)kShadowSize, (int32)kShadowSize};
            const bool shadowPassBegan = cmd->BeginRenderPass(hShadowRP, hShadowFBO, shadowArea);
            if (shadowPassBegan && useRealShadowPass && hShadowPipe.IsValid()) {
                NkViewport svp{0.f, 0.f, (float)kShadowSize, (float)kShadowSize, 0.f, 1.f};
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

            if (shadowPassBegan) cmd->EndRenderPass();

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
        // Passe 2 : Rendu principal (Phong + shadow + albedo texture)
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

        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(2.f, 0.f, 0.f));
            UboData ubo{};
            fillUboMain(ubo, matModel);
            device->WriteBuffer(hUBO[1], &ubo, sizeof(ubo));
            if (hDescSet[1].IsValid()) cmd->BindDescriptorSet(hDescSet[1], 0);
            cmd->BindVertexBuffer(0, hSphere);
            cmd->Draw((uint32)sphereVerts.Size());
        }

        {
            NkMat4f matModel = NkMat4f::Translation(NkVec3f(0.f, -1.0f, 0.f));
            UboData ubo{};
            fillUboMain(ubo, matModel);
            device->WriteBuffer(hUBO[2], &ubo, sizeof(ubo));
            if (hDescSet[2].IsValid()) cmd->BindDescriptorSet(hDescSet[2], 0);
            cmd->BindVertexBuffer(0, hPlane);
            cmd->Draw((uint32)planeVerts.Size());
        }


        // =====================================================================
        // Texte 3D billboard (labels CUBE/SPHERE/FLOOR en world space)
        // =====================================================================
        if (textOk) {
            // Vulkan et DX12 ont le clip-space Y inverse => flip billboard + ortho
            const bool flipY = (targetApi == NkGraphicsApi::NK_API_VULKAN ||
                                targetApi == NkGraphicsApi::NK_API_DIRECTX12);
            float bbMVP[16];
            // CUBE label : au-dessus du cube rotatif
            if (tqCube.vtxCount > 0) {
                NkVec3f cubeLabelPos = NkVec3f(0.f, 0.9f, 0.f);
                Billboard3DMVP(cubeLabelPos, 1.0f, 0.25f, matView, matProj, flipY, bbMVP);
                RenderTextQuad(cmd, device, tqCube, hTextPipe, bbMVP, 1.f, 0.85f, 0.3f, 1.f);
            }
            // SPHERE label : au-dessus de la sphere
            if (tqSphere.vtxCount > 0) {
                NkVec3f sphereLabelPos = NkVec3f(2.f, 0.7f, 0.f);
                Billboard3DMVP(sphereLabelPos, 1.4f, 0.25f, matView, matProj, flipY, bbMVP);
                RenderTextQuad(cmd, device, tqSphere, hTextPipe, bbMVP, 0.4f, 0.85f, 1.f, 1.f);
            }
            // FLOOR label : au-dessus du plan
            if (tqFloor.vtxCount > 0) {
                NkVec3f floorLabelPos = NkVec3f(-1.5f, -0.7f, 1.5f);
                Billboard3DMVP(floorLabelPos, 1.2f, 0.25f, matView, matProj, flipY, bbMVP);
                RenderTextQuad(cmd, device, tqFloor, hTextPipe, bbMVP, 0.4f, 1.f, 0.5f, 1.f);
            }
        }

        // =====================================================================
        // Texte 2D overlay (nom du backend + FPS)
        // =====================================================================
        if (textOk) {
            const bool flipY = (targetApi == NkGraphicsApi::NK_API_VULKAN ||
                                targetApi == NkGraphicsApi::NK_API_DIRECTX12);
            float oMVP[16];
            const float fw = (float)W, fh = (float)H;
            // Nom du backend en haut a gauche
            if (tqBackend.vtxCount > 0) {
                float tw = (float)tqBackend.texW, th = (float)tqBackend.texH;
                Ortho2DMVP(10.f, 10.f, tw, th, fw, fh, depthZeroToOne, flipY, oMVP);
                RenderTextQuad(cmd, device, tqBackend, hTextPipe, oMVP, 1.f, 1.f, 0.f, 1.f);
            }
            // FPS en haut a droite
            if (tqFPS.vtxCount > 0) {
                float tw = (float)tqFPS.texW, th = (float)tqFPS.texH;
                Ortho2DMVP(fw - tw - 10.f, 10.f, tw, th, fw, fh, depthZeroToOne, flipY, oMVP);
                RenderTextQuad(cmd, device, tqFPS, hTextPipe, oMVP, 0.f, 1.f, 0.f, 1.f);
            }
        }

        cmd->EndRenderPass();
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        if (requestSaveSource) {
            requestSaveSource = false;
            if (loadedTextureImage && loadedTextureImage->IsValid()) {
                ++saveSourceCounter;
                const std::string stem = NkFormat("source_{0}", (unsigned long long)saveSourceCounter).CStr();
                if (!SaveImageVariants(*loadedTextureImage, capturesDir, stem)) {
                    logger.Info("[RHIFullDemoImage] Echec save source image.\n");
                }
            } else {
                logger.Info("[RHIFullDemoImage] Save source ignore: texture source non disponible.\n");
            }
        }

        if (requestSaveScene) {
            requestSaveScene = false;
            ++saveSceneCounter;
            const std::string stem = NkFormat("scene_{0}", (unsigned long long)saveSceneCounter).CStr();
            if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
                auto* swDev = static_cast<NkSoftwareDevice*>(device);
                if (!SaveSoftwareScene(swDev, capturesDir, stem)) {
                    logger.Info("[RHIFullDemoImage] Echec save scene software.\n");
                }
            } else {
                logger.Info("[RHIFullDemoImage] Save scene GPU non implemente pour {0} (offscreen + readback a brancher dans la demo).\n",
                            NkGraphicsApiName(targetApi));
            }
        }
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
    if (hAlbedoSampler.IsValid()) device->DestroySampler(hAlbedoSampler);
    if (hAlbedoTex.IsValid())     device->DestroyTexture(hAlbedoTex);
    if (loadedTextureImage) {
        loadedTextureImage->Free();
        loadedTextureImage = nullptr;
    }

    // Texte
    DestroyTextQuad(device, tqFPS);
    DestroyTextQuad(device, tqBackend);
    DestroyTextQuad(device, tqFloor);
    DestroyTextQuad(device, tqSphere);
    DestroyTextQuad(device, tqCube);
    if (hTextPipe.IsValid())   device->DestroyPipeline(hTextPipe);
    if (hTextShader.IsValid()) device->DestroyShader(hTextShader);
    if (hTextLayout.IsValid()) device->DestroyDescriptorSetLayout(hTextLayout);
    // if (ftFace18) ftLib.FreeFont(ftFace18);
    // if (ftFace32) ftLib.FreeFont(ftFace32);
    ftLib.Destroy();

    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[RHIFullDemo] TerminÃ© proprement.\n");
    return 0;
}