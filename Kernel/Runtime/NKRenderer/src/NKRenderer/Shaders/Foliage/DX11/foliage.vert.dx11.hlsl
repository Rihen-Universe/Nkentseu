// ============================================================
// pbr.vert.dx11.hlsl — NKRenderer v4.0 — Foliage Vertex (DX11 SM5)
// Différences DX11 vs GLSL :
//   • cbuffer register(bN), Texture2D register(tN), SamplerState register(sN)
//   • column_major pour matrices (convention DX = row-major par défaut → on force column_major)
//   • mul(M, v) au lieu de M*v
//   • SV_Position, SV_Target
//   • UV Y non inversé (contrairement à Vulkan Metal)
// ============================================================
cbuffer CameraUBO : register(b0) {
    column_major float4x4 view, proj, viewProj, invViewProj;
    float4 camPos, camDir;
    float2 viewport;
    float time, deltaTime;
};
cbuffer ObjectUBO : register(b1) {
    column_major float4x4 model, normalMatrix;
    float4 tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    float4 subsurfaceColor;
};
cbuffer ShadowUBO : register(b3) {
    column_major float4x4 cascadeMats[4];
    float4 cascadeSplits;
    int cascadeCount; float shadowBias, normalBias; int softShadows;
};

struct VSIn {
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float3 tangent : TANGENT;
    float2 uv      : TEXCOORD0;
    float2 uv2     : TEXCOORD1;
    float4 color   : COLOR;
};
struct VSOut {
    float4 pos         : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float3 tangent     : TEXCOORD2;
    float3 bitangent   : TEXCOORD3;
    float2 uv          : TEXCOORD4;
    float2 uv2         : TEXCOORD5;
    float4 color       : TEXCOORD6;
    float4 shadowCoord0: TEXCOORD7;
    float4 shadowCoord1: TEXCOORD8;
    float4 shadowCoord2: TEXCOORD9;
    float4 shadowCoord3: TEXCOORD10;
};

VSOut main(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos, 1.0));
    o.worldPos   = wp.xyz;
    float3x3 nm  = (float3x3)normalMatrix;
    o.normal     = normalize(mul(nm, v.normal));
    o.tangent    = normalize(mul(nm, v.tangent));
    o.bitangent  = cross(o.normal, o.tangent);
    o.uv  = v.uv;
    o.uv2 = v.uv2;
    o.color = v.color * tint;
    o.shadowCoord0 = mul(cascadeMats[0], wp);
    o.shadowCoord1 = mul(cascadeMats[1], wp);
    o.shadowCoord2 = mul(cascadeMats[2], wp);
    o.shadowCoord3 = mul(cascadeMats[3], wp);
    o.pos = mul(viewProj, wp);
    return o;
}
