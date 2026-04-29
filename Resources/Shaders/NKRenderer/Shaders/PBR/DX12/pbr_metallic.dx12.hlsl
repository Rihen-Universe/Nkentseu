// =============================================================================
// pbr_metallic.dx12.hlsl — HLSL SM6.0 DirectX 12
// DIFFÉRENCES vs DX11 :
//   1. Root Signature explicite (définie ici ou dans le code C++)
//   2. Root Constants (b0) = push constants DX12
//      "RootConstants(num32BitConstants=20, b0)" dans la root sig
//   3. Descriptor Tables (groupes de SRV/CBV)
//   4. SM6.0 : Wave intrinsics disponibles
//   5. SM6.6 : ResourceDescriptorHeap[index] pour le bindless
//   6. DXC obligatoire (pas FXC)
//   7. Inline samplers possibles dans la root signature
// =============================================================================

// ─── Root Signature ──────────────────────────────────────────────────────────
// Définit le layout des paramètres root (comment les données sont liées)
#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS \
    | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS), \
    RootConstants(num32BitConstants=20, b0, space0), \
    CBV(b1, space0), \
    CBV(b2, space0), \
    DescriptorTable(SRV(t0, numDescriptors=9), visibility=SHADER_VISIBILITY_PIXEL), \
    StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, addressU=TEXTURE_ADDRESS_WRAP, addressV=TEXTURE_ADDRESS_WRAP), \
    StaticSampler(s1, filter=FILTER_MIN_MAG_MIP_LINEAR, addressU=TEXTURE_ADDRESS_CLAMP, addressV=TEXTURE_ADDRESS_CLAMP), \
    StaticSampler(s2, filter=FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, comparisonFunc=COMPARISON_LESS_EQUAL)"

// ─── Root Constants (b0) — 20 floats = équivalent push constants ─────────────
// DX12 : les root constants sont des floats 32-bit inline dans la command list
struct RootConstants {
    float4x4 modelMatrix;   // 16 floats
    float4   customColor;   // 4 floats
    // Total : 20 * sizeof(float) = 80 bytes
};
ConstantBuffer<RootConstants> pc : register(b0, space0);

// ─── Frame CBV (b1) ───────────────────────────────────────────────────────────
struct FrameData {
    float4x4 View, Projection, ViewProjection;
    float4x4 InvViewProjection, PrevViewProjection;
    float4   CameraPosition, CameraDirection, ViewportSize, Time;
    float4   SunDirection, SunColor;
    float4x4 ShadowMatrix[4];
    float4   CascadeSplits;
    float    EnvIntensity, EnvRotation;
    uint     LightCount, FrameIndex;
};
ConstantBuffer<FrameData> Frame : register(b1, space0);

// ─── Matériau CBV (b2) ────────────────────────────────────────────────────────
struct MaterialCBV {
    float4 albedo, emissive;
    float  metallic, roughness, ao, emissiveStrength;
    float  clearcoat, clearcoatRough, normalStrength, uvScaleX;
    float  uvScaleY, uvOffsetX, uvOffsetY, transmission;
    float  ior; float3 _pad;
};
ConstantBuffer<MaterialCBV> Mat : register(b2, space0);

// ─── Textures (Descriptor Table) ─────────────────────────────────────────────
Texture2D    tAlbedo        : register(t0);
Texture2D    tNormal        : register(t1);
Texture2D    tMetalRough    : register(t2);
Texture2D    tAO            : register(t3);
Texture2D    tEmissive      : register(t4);
Texture2D    tShadowMap     : register(t5);
TextureCube  tEnvIrradiance : register(t6);
TextureCube  tEnvPrefilter  : register(t7);
Texture2D    tBRDFLUT       : register(t8);

// ─── Samplers statiques (dans la root signature) ─────────────────────────────
SamplerState       sLinear      : register(s0);
SamplerState       sLinearClamp : register(s1);
SamplerComparisonState sShadow  : register(s2);

// ─── Structures I/O ──────────────────────────────────────────────────────────
struct VSIn {
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 tangent: TANGENT;
    float2 uv0    : TEXCOORD0;
    float2 uv1    : TEXCOORD1;
};
struct VSOut {
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float4 tangent   : TEXCOORD2;
    float2 uv0       : TEXCOORD3;
    float2 uv1       : TEXCOORD4;
    float3 viewDir   : TEXCOORD5;
    float4 shadow[4] : TEXCOORD6;   // shadow[0..3]
};

// ─── Vertex Shader ────────────────────────────────────────────────────────────
[RootSignature(RS)]
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp    = mul(float4(v.pos,1.0), pc.modelMatrix);
    o.worldPos   = wp.xyz;
    o.position   = mul(wp, Frame.ViewProjection);
    o.uv0 = v.uv0; o.uv1 = v.uv1;
    o.viewDir    = normalize(Frame.CameraPosition.xyz - wp.xyz);
    float3x3 NM  = (float3x3)pc.modelMatrix;
    o.normal     = normalize(mul(v.normal, NM));
    o.tangent    = float4(normalize(mul(v.tangent.xyz, NM)), v.tangent.w);
    for(int i=0;i<4;i++) o.shadow[i] = mul(wp, Frame.ShadowMatrix[i]);
    return o;
}

// ─── Helpers BRDF ─────────────────────────────────────────────────────────────
static const float PI = 3.14159265359;
float D_GGX(float NdotH,float r){float a2=pow(r,4);float d=NdotH*NdotH*(a2-1)+1;return a2/(PI*d*d);}
float G_Smith(float NdotV,float NdotL,float r){float k=(r+1)*(r+1)/8;return (NdotV/(NdotV*(1-k)+k))*(NdotL/(NdotL*(1-k)+k));}
float3 F_Schlick(float c,float3 F0){return F0+(1-F0)*pow(clamp(1-c,0,1),5);}
float3 F_SchlickR(float c,float3 F0,float r){return F0+(max(1-r,F0)-F0)*pow(clamp(1-c,0,1),5);}

// Shadow avec SamplerComparisonState (DX12 native PCF)
float ShadowPCF_DX12(float4 sc, Texture2D sm, SamplerComparisonState css) {
    float3 proj=sc.xyz/sc.w;
    float2 uv=proj.xy*0.5+0.5; uv.y=1.0-uv.y;
    float s=0; float2 ts=1.0/1024.0;
    [unroll] for(int x=-1;x<=1;x++) [unroll] for(int y=-1;y<=1;y++)
        s+=sm.SampleCmpLevelZero(css,uv+float2(x,y)*ts,proj.z-0.005);
    return s/9.0;
}

// ─── Pixel Shader ─────────────────────────────────────────────────────────────
[RootSignature(RS)]
float4 PSMain(VSOut i) : SV_Target0 {
    float2 uv = i.uv0 * float2(Mat.uvScaleX,Mat.uvScaleY) + float2(Mat.uvOffsetX,Mat.uvOffsetY);
    float3 V  = normalize(i.viewDir);
    float3 N  = normalize(i.normal);
    float3 T  = normalize(i.tangent.xyz - dot(i.tangent.xyz,N)*N);
    float3 B  = cross(N,T)*i.tangent.w;
    float3x3 TBN = float3x3(T,B,N);
    float3 tn = tNormal.Sample(sLinear,uv).rgb*2-1;
    tn.xy    *= Mat.normalStrength;
    N = normalize(mul(tn,TBN));

    float4 albTex = tAlbedo.Sample(sLinear,uv);
    float3 base   = albTex.rgb*Mat.albedo.rgb*pc.customColor.rgb;
    float2 mr     = tMetalRough.Sample(sLinear,uv).rg;
    float  metal  = mr.r*Mat.metallic, rough=max(mr.g*Mat.roughness,0.04);
    float  ao     = tAO.Sample(sLinear,uv).r;
    float3 F0     = lerp(0.04,base,metal);

    // Shadow
    float viewD   = -(mul(float4(i.worldPos,1),Frame.View)).z;
    float shadow  = viewD<Frame.CascadeSplits.x ? ShadowPCF_DX12(i.shadow[0],tShadowMap,sShadow) :
                   viewD<Frame.CascadeSplits.y ? ShadowPCF_DX12(i.shadow[1],tShadowMap,sShadow) :
                   viewD<Frame.CascadeSplits.z ? ShadowPCF_DX12(i.shadow[2],tShadowMap,sShadow) :
                                                  ShadowPCF_DX12(i.shadow[3],tShadowMap,sShadow);

    float3 L=normalize(-Frame.SunDirection.xyz);
    float3 H=normalize(V+L);
    float NdotL=max(dot(N,L),0),NdotV=max(dot(N,V),0),NdotH=max(dot(N,H),0),HdotV=max(dot(H,V),0);
    float3 F=F_Schlick(HdotV,F0);
    float3 kD=(1-F)*(1-metal);
    float3 Lo=(kD*base/PI+D_GGX(NdotH,rough)*G_Smith(NdotV,NdotL,rough)*F/max(4*NdotV*NdotL,0.001))*NdotL*Frame.SunColor.xyz*Frame.SunColor.w*shadow;

    // IBL
    float3 R=reflect(-V,N);
    float3 irrad=tEnvIrradiance.Sample(sLinear,N).rgb;
    float3 pref =tEnvPrefilter.SampleLevel(sLinearClamp,R,rough*7).rgb;
    float2 brdf =tBRDFLUT.Sample(sLinearClamp,float2(NdotV,rough)).rg;
    float3 ambient=((1-F_SchlickR(NdotV,F0,rough))*(1-metal)*irrad*base + pref*(F_SchlickR(NdotV,F0,rough)*brdf.x+brdf.y))*ao*Frame.EnvIntensity;

    float3 em = tEmissive.Sample(sLinear,uv).rgb*Mat.emissive.rgb*Mat.emissiveStrength;
    return float4((Lo+ambient+em)*Frame.Time.w, albTex.a*Mat.albedo.a);
}
