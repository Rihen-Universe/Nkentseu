// =============================================================================
// NkShaders_DX12.h
// Shaders PBR pour DirectX 12 — HLSL Shader Model 6.x
// Compilés via dxc (DirectX Shader Compiler).
//
// Convention DX12 :
//   Root Signature (défini séparément dans NkDX12Renderer) :
//     Root constant buffer view (b0) → FrameUBO
//     Root constant buffer view (b1) → DrawUBO
//     Descriptor table (b2, t0–t4, s0–s4) → Material + textures
//     Root constant buffer view (b3) → LightsUBO
//
// Les shaders DX12 sont DISTINCTS des DX11 :
//   • SM 6.x uniquement (wave intrinsics, raytracing, mesh shaders prêts)
//   • pas de cbuffer/register(b) legacy — utilisation de ConstantBuffer<> template
//   • SamplerState déclarés avec static constexpr ou dynamic descriptor heap
// =============================================================================
#pragma once

namespace nkentseu {
namespace renderer {
namespace shaders {
namespace dx12 {

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader PBR 3D — DX12 SM6.x
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_VS = R"HLSL(
// DX12 / SM 6.x — ConstantBuffer template + space séparés
struct FrameData {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4   cameraPos;
    float4   time;
};
struct DrawData {
    float4x4 model;
    float4x4 modelIT;
    float4   objectColor;
};

ConstantBuffer<FrameData> Frame : register(b0, space0);
ConstantBuffer<DrawData>  Draw  : register(b1, space0);

struct VSIn {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float2 uv        : TEXCOORD0;
    float2 uv2       : TEXCOORD1;
    float4 color     : COLOR;
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
    float4 worldPos = mul(Draw.model, float4(i.position, 1.0));
    o.worldPos      = worldPos.xyz;

    float3x3 normalMat = (float3x3)Draw.modelIT;
    o.normal    = normalize(mul(normalMat, i.normal));
    o.tangent   = normalize(mul(normalMat, i.tangent));
    o.bitangent = cross(o.normal, o.tangent);
    o.uv        = i.uv;
    o.uv2       = i.uv2;
    o.color     = i.color * Draw.objectColor;
    o.position  = mul(Frame.viewProj, worldPos);
    return o;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Pixel shader PBR 3D — DX12 SM6.x
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_PS = R"HLSL(
struct FrameData {
    float4x4 view; float4x4 proj; float4x4 viewProj; float4 cameraPos; float4 time;
};
struct MaterialData {
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
struct LightsData {
    float4 dirLightDir;
    float4 dirLightColor;
    float4 pointPos[8];
    float4 pointColor[8];
    int    numPointLights;
    int    _pad[3];
};

ConstantBuffer<FrameData>    Frame  : register(b0, space0);
ConstantBuffer<MaterialData> Mat    : register(b2, space0);
ConstantBuffer<LightsData>   Lights : register(b3, space0);

Texture2D    uAlbedoMap     : register(t0, space0);
Texture2D    uNormalMap     : register(t1, space0);
Texture2D    uMetalRoughMap : register(t2, space0);
Texture2D    uOcclusionMap  : register(t3, space0);
Texture2D    uEmissiveMap   : register(t4, space0);
SamplerState uSampler       : register(s0, space0);

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

static const float PI = 3.14159265359f;

float DistributionGGX(float3 N, float3 H, float r) {
    float a=r*r,a2=a*a,NdH=max(dot(N,H),0),d=NdH*NdH*(a2-1)+1;
    return a2/(PI*d*d);
}
float GeometrySchlick(float NdV, float r) {
    float k=(r+1)*(r+1)/8;
    return NdV/(NdV*(1-k)+k);
}
float GeometrySmith(float3 N, float3 V, float3 L, float r) {
    return GeometrySchlick(max(dot(N,V),0),r)*GeometrySchlick(max(dot(N,L),0),r);
}
float3 FresnelSchlick(float cosT, float3 F0) {
    return F0+(1-F0)*pow(clamp(1-cosT,0,1),5);
}
float3 CalcPBR(float3 N, float3 V, float3 L, float3 lc,
               float3 albedo, float metallic, float roughness) {
    float3 H=normalize(V+L);
    float NdL=max(dot(N,L),0);
    float3 F0=lerp(float3(0.04,0.04,0.04),albedo,metallic);
    float3 F=FresnelSchlick(max(dot(H,V),0),F0);
    float3 spec=(DistributionGGX(N,H,roughness)*GeometrySmith(N,V,L,roughness)*F)
               /(4*max(dot(N,V),0)*NdL+0.0001f);
    float3 kD=(1-F)*(1-metallic);
    return (kD*albedo/PI+spec)*lc*NdL;
}

float4 PSMain(PSIn i) : SV_Target {
    float4 baseColor = (Mat.useAlbedoMap ? uAlbedoMap.Sample(uSampler,i.uv) : (float4)1)
                     * Mat.baseColorFactor * i.color;
    if (Mat.alphaCutoff > 0 && baseColor.a < Mat.alphaCutoff) discard;

    float3 N = normalize(i.normal);
    if (Mat.useNormalMap) {
        float3 nt = uNormalMap.Sample(uSampler,i.uv).xyz*2-1;
        float3x3 TBN = float3x3(normalize(i.tangent),normalize(i.bitangent),N);
        N = normalize(mul(nt,TBN));
    }
    if (Mat.doubleSided && !i.frontFace) N = -N;

    float metallic  = Mat.metallicFactor;
    float roughness = Mat.roughnessFactor;
    if (Mat.useMetalRoughMap) {
        float3 mr = uMetalRoughMap.Sample(uSampler,i.uv).rgb;
        metallic *= mr.b; roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.04f, 1.0f);

    float ao = 1;
    if (Mat.useOcclusionMap)
        ao = lerp(1, uOcclusionMap.Sample(uSampler,i.uv2).r, Mat.occlusionStrength);

    float3 V=normalize(Frame.cameraPos.xyz-i.worldPos), Lo=(float3)0;

    if (Lights.dirLightColor.w > 0.001f)
        Lo += CalcPBR(N,V,normalize(-Lights.dirLightDir.xyz),
                     Lights.dirLightColor.xyz*Lights.dirLightColor.w,
                     baseColor.rgb,metallic,roughness);

    for (int j=0; j<min(Lights.numPointLights,8); ++j) {
        float3 d=Lights.pointPos[j].xyz-i.worldPos;
        float dist=length(d),r=max(Lights.pointPos[j].w,0.001f);
        float a=clamp(1-pow(dist/r,2),0,1); a*=a;
        if (a<0.0001f) continue;
        Lo += CalcPBR(N,V,d/dist,Lights.pointColor[j].xyz*Lights.pointColor[j].w*a,
                     baseColor.rgb,metallic,roughness);
    }

    float3 ambient=0.03f*baseColor.rgb*ao;
    float3 emissive=Mat.emissiveFactor*Mat.emissiveStrength;
    if (Mat.useEmissiveMap) emissive *= uEmissiveMap.Sample(uSampler,i.uv).rgb;

    float3 color=(ambient+Lo+emissive)/(ambient+Lo+emissive+1);
    color=pow(color,1.0f/2.2f);
    return float4(color,baseColor.a);
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Unlit — DX12 SM6.x
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kUnlit_VS = R"HLSL(
struct FrameData { float4x4 view; float4x4 proj; float4x4 viewProj; float4 cameraPos; float4 time; };
struct DrawData  { float4x4 model; float4x4 modelIT; float4 objectColor; };
ConstantBuffer<FrameData> Frame : register(b0,space0);
ConstantBuffer<DrawData>  Draw  : register(b1,space0);
struct VSIn  { float3 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.uv  = i.uv;
    o.col = i.col * Draw.objectColor;
    o.pos = mul(Frame.viewProj, mul(Draw.model, float4(i.pos,1)));
    return o;
}
)HLSL";

inline constexpr const char* kUnlit_PS = R"HLSL(
struct MaterialData { float4 baseColorFactor; float _p[3]; int useAlbedoMap; float alphaCutoff; int _p2[2]; };
ConstantBuffer<MaterialData> Mat : register(b2,space0);
Texture2D uTex    : register(t0,space0);
SamplerState uSmp : register(s0,space0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tex=Mat.useAlbedoMap?uTex.Sample(uSmp,i.uv):(float4)1;
    float4 col=tex*Mat.baseColorFactor*i.col;
    if (Mat.alphaCutoff>0&&col.a<Mat.alphaCutoff) discard;
    return col;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// 2D — DX12
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* k2D_VS = R"HLSL(
struct VPData { float2 viewport; float2 _pad; float4x4 projection; };
struct DrawData { float4x4 transform; float4 objectColor; };
ConstantBuffer<VPData>   VP   : register(b0,space0);
ConstantBuffer<DrawData> Draw : register(b1,space0);
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn i) {
    VSOut o; o.uv=i.uv; o.col=i.col*Draw.objectColor;
    o.pos=mul(VP.projection,mul(Draw.transform,float4(i.pos,0,1)));
    return o;
}
)HLSL";

inline constexpr const char* k2D_PS = R"HLSL(
struct MatData { float4 tintColor; int useTexture; float _p[3]; };
ConstantBuffer<MatData> Mat : register(b2,space0);
Texture2D uTex    : register(t0,space0);
SamplerState uSmp : register(s0,space0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tex=Mat.useTexture?uTex.Sample(uSmp,i.uv):(float4)1;
    return tex*i.col*Mat.tintColor;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Shadow depth — DX12
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kShadow_VS = R"HLSL(
struct ShadowData { float4x4 lightVP; float4x4 model; };
ConstantBuffer<ShadowData> Shadow : register(b0,space0);
float4 VSMain(float3 pos:POSITION) : SV_Position {
    return mul(Shadow.lightVP, mul(Shadow.model, float4(pos,1)));
}
)HLSL";

inline constexpr const char* kShadow_PS = R"HLSL(
void PSMain() {}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Debug wireframe — DX12
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kDebug_VS = R"HLSL(
struct FrameData { float4x4 view; float4x4 proj; float4x4 viewProj; float4 cameraPos; float4 time; };
ConstantBuffer<FrameData> Frame : register(b0,space0);
struct VSIn  { float3 pos:POSITION; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float4 col:COLOR; };
VSOut VSMain(VSIn i) { VSOut o; o.col=i.col; o.pos=mul(Frame.viewProj,float4(i.pos,1)); return o; }
)HLSL";

inline constexpr const char* kDebug_PS = R"HLSL(
float4 PSMain(float4 col:COLOR) : SV_Target { return col; }
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Text atlas — DX12
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kText_PS = R"HLSL(
Texture2D    uFontAtlas : register(t0,space0);
SamplerState uSmp       : register(s0,space0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float a=uFontAtlas.Sample(uSmp,i.uv).r;
    if (a<0.01) discard;
    return float4(i.col.rgb, i.col.a*a);
}
)HLSL";

} // namespace dx12
} // namespace shaders
} // namespace renderer
} // namespace nkentseu