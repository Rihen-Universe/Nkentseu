// =============================================================================
// NkShaders_DX11.h
// Shaders PBR pour DirectX 11 — HLSL Shader Model 5.0
// Compilés via fxc.
//
// Convention DX11 :
//   b0 → FrameUBO (par frame)
//   b1 → DrawUBO  (par draw)
//   b2 → MaterialUBO
//   b3 → LightsUBO
//   t0–t4 → textures PBR
//   s0–s4 → samplers correspondants
// Matrices column-major (même convention que OpenGL/Vulkan dans le code C++)
// NDC : Z ∈ [0, 1], Y vers le haut, UV Y=0 en haut
// =============================================================================
#pragma once

namespace nkentseu {
namespace renderer {
namespace shaders {
namespace dx11 {

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader PBR 3D — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_VS = R"HLSL(
cbuffer FrameUBO : register(b0) {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 viewProj;
    float4 cameraPos;
    float4 time;
};

cbuffer DrawUBO : register(b1) {
    column_major float4x4 model;
    column_major float4x4 modelIT;
    float4 objectColor;
};

struct VSIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
    float2 uv2      : TEXCOORD1;
    float4 color    : COLOR;
};

struct VSOut {
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float3 tangent   : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
    float2 uv        : TEXCOORD4;
    float2 uv2       : TEXCOORD5;
    float4 color     : COLOR;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 worldPos  = mul(model, float4(i.position, 1.0));
    o.worldPos       = worldPos.xyz;

    float3x3 normalMat = (float3x3)modelIT;
    o.normal    = normalize(mul(normalMat, i.normal));
    o.tangent   = normalize(mul(normalMat, i.tangent));
    o.bitangent = cross(o.normal, o.tangent);
    o.uv        = i.uv;
    o.uv2       = i.uv2;
    o.color     = i.color * objectColor;
    o.position  = mul(viewProj, worldPos);
    return o;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Pixel shader PBR 3D — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_PS = R"HLSL(
cbuffer FrameUBO : register(b0) {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 viewProj;
    float4 cameraPos;
    float4 time;
};

cbuffer MaterialUBO : register(b2) {
    float4  baseColorFactor;
    float   metallicFactor;
    float   roughnessFactor;
    float   occlusionStrength;
    float   emissiveStrength;
    float3  emissiveFactor;
    float   _pad0;
    int     useAlbedoMap;
    int     useNormalMap;
    int     useMetalRoughMap;
    int     useOcclusionMap;
    int     useEmissiveMap;
    float   alphaCutoff;
    int     doubleSided;
    float   _pad1;
};

cbuffer LightsUBO : register(b3) {
    float4  dirLightDir;
    float4  dirLightColor;
    float4  pointPos[8];
    float4  pointColor[8];
    int     numPointLights;
    int     _pad[3];
};

Texture2D    uAlbedoMap    : register(t0);
Texture2D    uNormalMap    : register(t1);
Texture2D    uMetalRoughMap: register(t2);
Texture2D    uOcclusionMap : register(t3);
Texture2D    uEmissiveMap  : register(t4);
SamplerState uSampler      : register(s0);

struct PSIn {
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float3 tangent   : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
    float2 uv        : TEXCOORD4;
    float2 uv2       : TEXCOORD5;
    float4 color     : COLOR;
    bool   frontFace : SV_IsFrontFace;
};

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float GeometrySchlick(float NdV, float roughness) {
    float r = roughness + 1.0;
    float k = (r*r)/8.0;
    return NdV / (NdV * (1.0-k) + k);
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    return GeometrySchlick(max(dot(N,V),0.0),roughness)
         * GeometrySchlick(max(dot(N,L),0.0),roughness);
}
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0-F0)*pow(clamp(1.0-cosTheta,0.0,1.0),5.0);
}
float3 CalcPBR(float3 N, float3 V, float3 L, float3 lightColor,
               float3 albedo, float metallic, float roughness) {
    float3 H = normalize(V + L);
    float NdL = max(dot(N, L), 0.0);
    float3 F0 = lerp(float3(0.04,0.04,0.04), albedo, metallic);
    float3 F  = FresnelSchlick(max(dot(H,V),0.0), F0);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 spec = (NDF*G*F)/(4.0*max(dot(N,V),0.0)*NdL+0.0001);
    float3 kD = (1.0-F)*(1.0-metallic);
    return (kD*albedo/PI + spec)*lightColor*NdL;
}

float4 PSMain(PSIn i) : SV_Target {
    float4 baseColor = (useAlbedoMap ? uAlbedoMap.Sample(uSampler, i.uv) : float4(1,1,1,1))
                     * baseColorFactor * i.color;
    if (alphaCutoff > 0.0 && baseColor.a < alphaCutoff) discard;

    float3 N = normalize(i.normal);
    if (useNormalMap) {
        float3 nt  = uNormalMap.Sample(uSampler, i.uv).xyz * 2.0 - 1.0;
        float3x3 TBN = float3x3(normalize(i.tangent), normalize(i.bitangent), N);
        N = normalize(mul(nt, TBN));
    }
    if (doubleSided && !i.frontFace) N = -N;

    float metallic  = metallicFactor;
    float roughness = roughnessFactor;
    if (useMetalRoughMap) {
        float3 mr = uMetalRoughMap.Sample(uSampler, i.uv).rgb;
        metallic *= mr.b; roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    float ao = 1.0;
    if (useOcclusionMap)
        ao = lerp(1.0, uOcclusionMap.Sample(uSampler, i.uv2).r, occlusionStrength);

    float3 V  = normalize(cameraPos.xyz - i.worldPos);
    float3 Lo = (float3)0;

    if (dirLightColor.w > 0.001)
        Lo += CalcPBR(N, V, normalize(-dirLightDir.xyz),
                     dirLightColor.xyz*dirLightColor.w,
                     baseColor.rgb, metallic, roughness);

    for (int j = 0; j < min(numPointLights, 8); ++j) {
        float3 d = pointPos[j].xyz - i.worldPos;
        float dist = length(d);
        float r = max(pointPos[j].w, 0.001);
        float a = clamp(1.0 - pow(dist/r, 2.0), 0.0, 1.0); a *= a;
        if (a < 0.0001) continue;
        Lo += CalcPBR(N, V, d/dist,
                     pointColor[j].xyz*pointColor[j].w*a,
                     baseColor.rgb, metallic, roughness);
    }

    float3 ambient  = 0.03*baseColor.rgb*ao;
    float3 emissive = emissiveFactor*emissiveStrength;
    if (useEmissiveMap) emissive *= uEmissiveMap.Sample(uSampler, i.uv).rgb;

    float3 color = (ambient + Lo + emissive)/(ambient + Lo + emissive + 1.0);
    color = pow(color, 1.0/2.2);
    return float4(color, baseColor.a);
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Unlit — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kUnlit_VS = R"HLSL(
cbuffer FrameUBO : register(b0) { column_major float4x4 view; column_major float4x4 proj; column_major float4x4 viewProj; float4 cameraPos; float4 time; };
cbuffer DrawUBO  : register(b1) { column_major float4x4 model; column_major float4x4 modelIT; float4 objectColor; };
struct VSIn  { float3 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.uv  = i.uv;
    o.col = i.col * objectColor;
    o.pos = mul(viewProj, mul(model, float4(i.pos,1)));
    return o;
}
)HLSL";

inline constexpr const char* kUnlit_PS = R"HLSL(
cbuffer MaterialUBO : register(b2) { float4 baseColorFactor; float _pad[3]; int useAlbedoMap; float alphaCutoff; int _pad2[2]; };
Texture2D uTex    : register(t0);
SamplerState uSmp : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tex = useAlbedoMap ? uTex.Sample(uSmp, i.uv) : float4(1,1,1,1);
    float4 col = tex * baseColorFactor * i.col;
    if (alphaCutoff > 0.0 && col.a < alphaCutoff) discard;
    return col;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// 2D — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* k2D_VS = R"HLSL(
cbuffer ViewportUBO : register(b0) { float2 viewport; float2 _pad; column_major float4x4 projection; };
cbuffer DrawUBO     : register(b1) { column_major float4x4 transform; float4 objectColor; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.uv  = i.uv;
    o.col = i.col * objectColor;
    o.pos = mul(projection, mul(transform, float4(i.pos, 0, 1)));
    return o;
}
)HLSL";

inline constexpr const char* k2D_PS = R"HLSL(
cbuffer MaterialUBO : register(b2) { float4 tintColor; int useTexture; float _pad[3]; };
Texture2D uTex    : register(t0);
SamplerState uSmp : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tex = useTexture ? uTex.Sample(uSmp, i.uv) : float4(1,1,1,1);
    return tex * i.col * tintColor;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Shadow depth — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kShadow_VS = R"HLSL(
cbuffer ShadowUBO : register(b0) { column_major float4x4 lightVP; column_major float4x4 model; };
float4 VSMain(float3 pos:POSITION) : SV_Position {
    return mul(lightVP, mul(model, float4(pos,1)));
}
)HLSL";

inline constexpr const char* kShadow_PS = R"HLSL(
void PSMain() {}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Debug — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kDebug_VS = R"HLSL(
cbuffer FrameUBO : register(b0) { column_major float4x4 view; column_major float4x4 proj; column_major float4x4 viewProj; float4 cameraPos; float4 time; };
struct VSIn  { float3 pos:POSITION; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float4 col:COLOR; };
VSOut VSMain(VSIn i) { VSOut o; o.col=i.col; o.pos=mul(viewProj,float4(i.pos,1)); return o; }
)HLSL";

inline constexpr const char* kDebug_PS = R"HLSL(
float4 PSMain(float4 col:COLOR) : SV_Target { return col; }
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Text atlas — DX11
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kText_PS = R"HLSL(
Texture2D    uFontAtlas : register(t0);
SamplerState uSmp       : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float a = uFontAtlas.Sample(uSmp, i.uv).r;
    if (a < 0.01) discard;
    return float4(i.col.rgb, i.col.a * a);
}
)HLSL";

} // namespace dx11
} // namespace shaders
} // namespace renderer
} // namespace nkentseu