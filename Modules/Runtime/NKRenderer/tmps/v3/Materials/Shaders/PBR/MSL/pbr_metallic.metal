// =============================================================================
// pbr_metallic.metal — Metal Shading Language 2.x
// DIFFÉRENCES majeures vs GLSL/HLSL :
//   1. vertex/fragment/kernel comme qualificateurs de FONCTION (pas de entry point séparé)
//   2. [[position]] est le tag sur le MEMBRE de la struct de retour du vertex
//   3. Buffers : constant MyUBO& ubo [[buffer(N)]]
//      Convention NkRenderer : [[buffer(0..7)]] = vertex data, [[buffer(8..15)]] = uniforms
//   4. Textures : texture2d<float> tex [[texture(N)]]
//   5. Samplers : sampler s [[sampler(N)]] OU constexpr sampler inline
//   6. Pas de push constants → argument buffer ou [[buffer(0)]]
//   7. Matrices : float4x4 (column-major, comme GLSL)
//   8. mul(A,B) : operator* en MSL (pas de mul() comme HLSL)
//   9. Metal 3 : mesh shaders, argument buffers Tier 2 (bindless)
//  10. Precision : pas de highp/mediump — tout est 32-bit par défaut
// =============================================================================
#include <metal_stdlib>
using namespace metal;

// ─── Structures de données ────────────────────────────────────────────────────

// Frame UBO — passé via [[buffer(8)]] (convention NK : 8..15 = uniforms)
struct NkFrameUBO {
    float4x4 View, Projection, ViewProjection;
    float4x4 InvViewProjection, PrevViewProjection;
    float4   CameraPosition, CameraDirection, ViewportSize, Time;
    float4   SunDirection, SunColor;
    float4x4 ShadowMatrix[4];
    float4   CascadeSplits;
    float    EnvIntensity, EnvRotation;
    uint     LightCount, FrameIndex;
};

// Object data — passé via [[buffer(9)]]
struct ObjectUBO {
    float4x4 modelMatrix;
    float4   customColor;
    uint     materialId;
    uint3    _pad;
};

// Material UBO — passé via [[buffer(10)]]
struct MaterialUBO {
    float4 albedo, emissive;
    float  metallic, roughness, ao, emissiveStrength;
    float  clearcoat, clearcoatRough, normalStrength, uvScaleX;
    float  uvScaleY, uvOffsetX, uvOffsetY, transmission;
};

// ─── Vertex Input (depuis le vertex buffer [[buffer(0)]]) ─────────────────────
struct VertexIn {
    float3 position  [[attribute(0)]];
    float3 normal    [[attribute(1)]];
    float4 tangent   [[attribute(2)]];  // w=bitangent sign
    float2 uv0       [[attribute(3)]];
    float2 uv1       [[attribute(4)]];
};

// ─── Vertex → Fragment ────────────────────────────────────────────────────────
struct V2F {
    float4 position  [[position]];    // MSL: [[position]] sur le MEMBRE de sortie
    float3 worldPos;
    float3 normal;
    float4 tangent;
    float2 uv0;
    float2 uv1;
    float3 viewDir;
    float4 shadow0, shadow1, shadow2, shadow3;
};

// ─── Vertex Shader ────────────────────────────────────────────────────────────
vertex V2F vertMain(
    VertexIn v                          [[stage_in]],      // données vertex
    constant NkFrameUBO& frame          [[buffer(8)]],     // frame uniform
    constant ObjectUBO&  obj            [[buffer(9)]])     // per-object
{
    V2F o;
    float4 worldPos4 = obj.modelMatrix * float4(v.position, 1.0);
    o.worldPos   = worldPos4.xyz;
    o.uv0 = v.uv0; o.uv1 = v.uv1;
    o.viewDir    = normalize(frame.CameraPosition.xyz - o.worldPos);

    // Normal matrix : MSL utilise l'opérateur * directement
    // Note: pour les transformations de normales, il faudrait inverse+transpose
    // Ici on suppose pas de scale non-uniforme (simplification)
    float3x3 NM  = float3x3(obj.modelMatrix[0].xyz, obj.modelMatrix[1].xyz, obj.modelMatrix[2].xyz);
    o.normal     = normalize(NM * v.normal);
    float3 T     = normalize(NM * v.tangent.xyz);
    o.tangent    = float4(T, v.tangent.w);

    // Shadow coords
    o.shadow0 = frame.ShadowMatrix[0] * worldPos4;
    o.shadow1 = frame.ShadowMatrix[1] * worldPos4;
    o.shadow2 = frame.ShadowMatrix[2] * worldPos4;
    o.shadow3 = frame.ShadowMatrix[3] * worldPos4;

    o.position = frame.ViewProjection * worldPos4;
    // Note: Metal utilise NDC [-1,1] en X/Y et [0,1] en Z (comme DX12, pas OpenGL)
    // La projection doit être ajustée en conséquence dans la matrice de projection
    return o;
}

// ─── Helpers BRDF ─────────────────────────────────────────────────────────────
constant float PI_METAL = 3.14159265359f;

float D_GGX(float NdotH, float rough) {
    float a2=pow(rough,4.0f); float d=NdotH*NdotH*(a2-1.0f)+1.0f;
    return a2/(PI_METAL*d*d);
}
float G_Smith(float NdotV, float NdotL, float rough) {
    float k=(rough+1.0f)*(rough+1.0f)/8.0f;
    return (NdotV/(NdotV*(1.0f-k)+k))*(NdotL/(NdotL*(1.0f-k)+k));
}
float3 F_Schlick(float c, float3 F0) { return F0+(1.0f-F0)*pow(clamp(1.0f-c,0.0f,1.0f),5.0f); }
float3 F_SchlickR(float c,float3 F0,float r){return F0+(max(float3(1.0f-r),F0)-F0)*pow(clamp(1.0f-c,0.0f,1.0f),5.0f);}

// Shadow 3x3 PCF
float ShadowPCF(float4 sc, texture2d<float,access::sample> sm, sampler s) {
    float3 proj=sc.xyz/sc.w;
    // Metal NDC : [0,1] donc pas de *0.5+0.5 pour le Z, mais oui pour XY
    float2 uv=proj.xy*0.5f+0.5f;
    float2 ts=1.0f/float2(sm.get_width(),sm.get_height());
    float shadow=0.0f;
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        float sd=sm.sample(s,uv+float2(x,y)*ts).r;
        shadow+=(proj.z-0.005f>sd)?1.0f:0.0f;
    }
    return 1.0f-shadow/9.0f;
}

// ─── Fragment Shader ──────────────────────────────────────────────────────────
fragment float4 fragMain(
    V2F i                               [[stage_in]],
    constant NkFrameUBO& frame          [[buffer(8)]],
    constant ObjectUBO&  obj            [[buffer(9)]],
    constant MaterialUBO& mat           [[buffer(10)]],
    texture2d<float>   tAlbedo          [[texture(0)]],
    texture2d<float>   tNormal          [[texture(1)]],
    texture2d<float>   tMetalRough      [[texture(2)]],
    texture2d<float>   tAO              [[texture(3)]],
    texture2d<float>   tEmissive        [[texture(4)]],
    texture2d<float>   tShadowMap       [[texture(5)]],
    texturecube<float> tEnvIrradiance   [[texture(6)]],
    texturecube<float> tEnvPrefilter    [[texture(7)]],
    texture2d<float>   tBRDFLUT        [[texture(8)]],
    sampler            sLinear          [[sampler(0)]],  // linear wrap
    sampler            sLinearClamp     [[sampler(1)]],  // linear clamp
    sampler            sShadow          [[sampler(2)]])  // comparison
{
    // MSL : on peut aussi créer des samplers inline (plus courant) :
    // constexpr sampler sLin(filter::linear, address::repeat);
    // Ici on utilise des samplers passés en paramètre

    float2 uv = i.uv0 * float2(mat.uvScaleX, mat.uvScaleY)
                      + float2(mat.uvOffsetX, mat.uvOffsetY);

    float3 V = normalize(i.viewDir);
    float3 N = normalize(i.normal);
    float3 T = normalize(i.tangent.xyz - dot(i.tangent.xyz,N)*N);
    float3 B = cross(N,T)*i.tangent.w;
    float3x3 TBN = float3x3(T,B,N);

    // Normal map (MSL: * pour mul, pas mul())
    float3 tn = tNormal.sample(sLinear,uv).rgb*2.0f-1.0f;
    tn.xy    *= mat.normalStrength;
    N = normalize(TBN * tn);

    float4 albTex = tAlbedo.sample(sLinear, uv);
    float3 base   = albTex.rgb * mat.albedo.rgb * obj.customColor.rgb;
    float2 mr     = tMetalRough.sample(sLinear, uv).rg;
    float  metal  = mr.r*mat.metallic, rough=max(mr.g*mat.roughness,0.04f);
    float  ao     = tAO.sample(sLinear, uv).r;
    float3 F0     = mix(float3(0.04f), base, metal);

    // Shadow
    float viewD = -(frame.View * float4(i.worldPos,1.0f)).z;
    float shadow = viewD<frame.CascadeSplits.x ? ShadowPCF(i.shadow0,tShadowMap,sShadow):
                  viewD<frame.CascadeSplits.y ? ShadowPCF(i.shadow1,tShadowMap,sShadow):
                  viewD<frame.CascadeSplits.z ? ShadowPCF(i.shadow2,tShadowMap,sShadow):
                                                ShadowPCF(i.shadow3,tShadowMap,sShadow);

    float3 L=normalize(-frame.SunDirection.xyz);
    float3 H=normalize(V+L);
    float NdotL=max(dot(N,L),0.0f),NdotV=max(dot(N,V),0.0f);
    float NdotH=max(dot(N,H),0.0f),HdotV=max(dot(H,V),0.0f);
    float3 F=F_Schlick(HdotV,F0);
    float3 kD=(1.0f-F)*(1.0f-metal);
    float3 Lo=(kD*base/PI_METAL+D_GGX(NdotH,rough)*G_Smith(NdotV,NdotL,rough)*F/max(4.0f*NdotV*NdotL,0.001f))*NdotL*frame.SunColor.xyz*frame.SunColor.w*shadow;

    // IBL
    float3 R=reflect(-V,N);
    float er=frame.EnvRotation;
    float3 envN=float3(N.x*cos(er)-N.z*sin(er),N.y,N.x*sin(er)+N.z*cos(er));
    float3 envR=float3(R.x*cos(er)-R.z*sin(er),R.y,R.x*sin(er)+R.z*cos(er));
    float3 irrad=tEnvIrradiance.sample(sLinear,envN).rgb;
    float3 pref =tEnvPrefilter.sample(sLinearClamp,envR,level(rough*7.0f)).rgb;  // MSL: level() pour mip
    float2 brdf =tBRDFLUT.sample(sLinearClamp,float2(NdotV,rough)).rg;
    float3 kD2  =(1.0f-F_SchlickR(NdotV,F0,rough))*(1.0f-metal);
    float3 ambient=(kD2*irrad*base+pref*(F_SchlickR(NdotV,F0,rough)*brdf.x+brdf.y))*ao*frame.EnvIntensity;

    float3 em = tEmissive.sample(sLinear,uv).rgb*mat.emissive.rgb*mat.emissiveStrength;
    float3 color = (Lo+ambient+em)*frame.Time.w;
    return float4(color, albTex.a*mat.albedo.a);
}
