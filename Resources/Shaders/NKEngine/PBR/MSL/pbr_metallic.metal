// =============================================================================
// pbr_metallic.metal — PBR Metallic-Roughness Metal Shading Language 2.x
// =============================================================================
#include <metal_stdlib>
using namespace metal;

// ── Structs ───────────────────────────────────────────────────────────────────
struct SceneUBO {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 lightVP;
    float4   lightDir;
    float4   eyePos;
    float    time;
    float    dt;
    float    ndcZScale;
    float    ndcZOffset;
};

struct MaterialUBO {
    float4 baseColor;
    float4 emissiveColor;
    float  metallic;
    float  roughness;
    float  occlusion;
    float  normalScale;
    float  alphaClip;
    float  emissiveScale;
    float  clearcoat;
    float  clearcoatRoughness;
    float  transmission;
    float  ior;
    float2 _pad;
};

struct VertexIn {
    float3 pos      [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float3 tangent  [[attribute(2)]];
    float2 uv       [[attribute(3)]];
    float4 color    [[attribute(4)]];
};

struct VertexOut {
    float4 pos      [[position]];
    float3 fragPos;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv;
    float4 color;
    float4 fragPosLS;
};

// ── Fonctions PBR ─────────────────────────────────────────────────────────────
float D_GGX(float3 N, float3 H, float rough) {
    float a2 = rough*rough*rough*rough;
    float NdH = saturate(dot(N,H));
    float d = NdH*NdH*(a2-1)+1;
    return a2/(M_PI_F*d*d);
}
float G_Schlick(float NdV, float rough) {
    float k = (rough+1)*(rough+1)/8;
    return NdV/(NdV*(1-k)+k);
}
float G_Smith(float3 N, float3 V, float3 L, float rough) {
    return G_Schlick(saturate(dot(N,V)),rough)*G_Schlick(saturate(dot(N,L)),rough);
}
float3 F_Schlick(float c, float3 F0) {
    return F0+(1-F0)*pow(saturate(1-c),5);
}
float3 F_SchlickR(float c, float3 F0, float rough) {
    return F0+(max(float3(1-rough),F0)-F0)*pow(saturate(1-c),5);
}

// ── Vertex Shader ─────────────────────────────────────────────────────────────
vertex VertexOut vertex_pbr(
    VertexIn v           [[stage_in]],
    constant SceneUBO& scene [[buffer(0)]])
{
    VertexOut o;
    float4 worldPos = scene.model * float4(v.pos,1);
    o.fragPos   = worldPos.xyz;
    float3x3 nm = float3x3(scene.model[0].xyz,scene.model[1].xyz,scene.model[2].xyz);
    o.normal    = normalize(nm * v.normal);
    o.tangent   = normalize(nm * v.tangent);
    o.bitangent = normalize(cross(o.normal, o.tangent));
    o.uv        = v.uv;
    o.color     = v.color;
    o.fragPosLS = scene.lightVP * worldPos;
    o.pos       = scene.proj * scene.view * worldPos;
    return o;
}

// ── Fragment Shader ───────────────────────────────────────────────────────────
fragment float4 fragment_pbr(
    VertexOut in              [[stage_in]],
    constant SceneUBO&   scene [[buffer(0)]],
    constant MaterialUBO& mat  [[buffer(2)]],
    texture2d<float>   tAlbedo   [[texture(0)]],
    texture2d<float>   tNormal   [[texture(1)]],
    texture2d<float>   tORM      [[texture(2)]],
    texture2d<float>   tEmissive [[texture(3)]],
    texture2d<float>   tShadow   [[texture(4)]],
    texturecube<float> tIrrad    [[texture(5)]],
    texturecube<float> tPrefilt  [[texture(6)]],
    texture2d<float>   tBRDFLUT  [[texture(7)]],
    sampler sLinear [[sampler(0)]],
    sampler sClamp  [[sampler(1)]])
{
    float4 albedoT = pow(tAlbedo.sample(sLinear, in.uv), 2.2);
    float3 orm     = tORM.sample(sLinear, in.uv).rgb;
    float3 emitT   = pow(tEmissive.sample(sLinear, in.uv).rgb, 2.2);

    float4 albedo = albedoT * mat.baseColor * in.color;
    if (albedo.a < mat.alphaClip) discard_fragment();

    float ao    = orm.r * mat.occlusion;
    float rough = clamp(orm.g * mat.roughness, 0.04f, 1.0f);
    float metal = orm.b * mat.metallic;

    // Normal map
    float3 tn = tNormal.sample(sLinear, in.uv).xyz * 2 - 1;
    tn.xy *= mat.normalScale;
    float3x3 TBN = float3x3(normalize(in.tangent),normalize(in.bitangent),normalize(in.normal));
    float3 N = normalize(TBN * tn);

    float3 V  = normalize(scene.eyePos.xyz - in.fragPos);
    float3 R  = reflect(-V,N);
    float3 F0 = mix(float3(0.04), albedo.rgb, metal);

    // Directional light
    float3 Lo = float3(0);
    {
        float3 L  = normalize(-scene.lightDir.xyz);
        float3 H  = normalize(V+L);
        float  NdL= saturate(dot(N,L));
        float  D  = D_GGX(N,H,rough);
        float  G  = G_Smith(N,V,L,rough);
        float3 F  = F_Schlick(saturate(dot(H,V)),F0);
        float3 spec = D*G*F/(4*saturate(dot(N,V))*NdL+0.0001);
        float3 kD   = (1-F)*(1-metal);
        Lo += (kD*albedo.rgb/M_PI_F+spec)*float3(5.0)*NdL;
    }

    // IBL
    float3 F_ibl  = F_SchlickR(saturate(dot(N,V)),F0,rough);
    float3 kD_ibl = (1-F_ibl)*(1-metal);
    float3 irrad  = tIrrad.sample(sLinear,N).rgb;
    float3 diffIBL= irrad*albedo.rgb;
    constexpr sampler mipmapSampler(filter::linear,mip_filter::linear);
    float3 prefilt= tPrefilt.sample(mipmapSampler,R,level(rough*4)).rgb;
    float2 brdf   = tBRDFLUT.sample(sClamp, float2(saturate(dot(N,V)),rough)).rg;
    float3 specIBL= prefilt*(F_ibl*brdf.x+brdf.y);
    float3 ambient= (kD_ibl*diffIBL+specIBL)*ao;

    float3 emit = (emitT+mat.emissiveColor.rgb)*mat.emissiveScale;

    return float4(ambient+Lo+emit, albedo.a);
}
