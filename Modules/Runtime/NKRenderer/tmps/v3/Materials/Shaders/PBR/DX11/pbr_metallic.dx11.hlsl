// =============================================================================
// pbr_metallic.dx11.hlsl — HLSL SM5.0 DirectX 11
// DIFFÉRENCES vs GLSL :
//   - cbuffer pour les uniforms, pas de UBO/set/binding
//   - Textures et Samplers toujours séparés
//   - Semantics sur les structures (POSITION, NORMAL, TEXCOORD, etc.)
//   - SV_Position (système) pour la position clip
//   - float4x4 est row_major par défaut → mul(vec,mat) ou transpose
//   - Pas de push constants → cbuffer b0 réservé aux données per-objet
//   - HLSL intrinsics : normalize, dot, cross, lerp, saturate, mul...
//   - Sampler créé séparément dans l'appli (ID3D11SamplerState)
// =============================================================================

// ─── Constantes per-frame (b0 réservé per-object, b1 = frame) ─────────────────
cbuffer FrameData : register(b1) {
    float4x4 nk_View;
    float4x4 nk_Projection;
    float4x4 nk_ViewProjection;
    float4x4 nk_InvViewProjection;
    float4x4 nk_PrevViewProjection;
    float4   nk_CameraPosition;
    float4   nk_CameraDirection;
    float4   nk_ViewportSize;
    float4   nk_Time;           // x=time y=dt z=frame w=exposure
    float4   nk_SunDirection;
    float4   nk_SunColor;
    float4x4 nk_ShadowMatrix0;  // DX11 n'a pas les tableaux dans cbuffer facilement
    float4x4 nk_ShadowMatrix1;
    float4x4 nk_ShadowMatrix2;
    float4x4 nk_ShadowMatrix3;
    float4   nk_CascadeSplits;
    float    nk_EnvIntensity;
    float    nk_EnvRotation;
    uint     nk_LightCount;
    uint     nk_FrameIndex;
};

// ─── Constantes per-object (b0) — equivalent push constants ───────────────────
cbuffer ObjectData : register(b0) {
    float4x4 modelMatrix;
    float4   customColor;
    uint     materialId;
    uint3    _pad;
};

// ─── Matériau (b2) ────────────────────────────────────────────────────────────
cbuffer MaterialData : register(b2) {
    float4 mat_albedo;
    float4 mat_emissive;
    float  mat_metallic;
    float  mat_roughness;
    float  mat_ao;
    float  mat_emissiveStrength;
    float  mat_clearcoat;
    float  mat_clearcoatRough;
    float  mat_normalStrength;
    float  mat_uvScaleX;
    float  mat_uvScaleY;
    float2 mat_uvOffset;
    float  mat_transmission;
    float  mat_ior;
    float2 _padMat;
};

// ─── Textures (toutes séparées du sampler en DX11) ────────────────────────────
Texture2D    tAlbedo      : register(t0);
Texture2D    tNormal      : register(t1);
Texture2D    tMetalRough  : register(t2);  // R=metal G=rough
Texture2D    tAO          : register(t3);
Texture2D    tEmissive    : register(t4);
Texture2D    tShadowMap   : register(t5);
TextureCube  tEnvIrradiance : register(t6);
TextureCube  tEnvPrefilter  : register(t7);
Texture2D    tBRDFLUT       : register(t8);

// ─── Samplers (créés séparément côté CPU) ─────────────────────────────────────
SamplerState sLinear      : register(s0);  // linear wrap
SamplerState sLinearClamp : register(s1);  // linear clamp
SamplerState sShadow      : register(s2);  // comparison sampler (PCF)

// ─── Structures de vertex I/O ─────────────────────────────────────────────────
struct VSIn {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float4 tangent   : TANGENT;
    float2 uv0       : TEXCOORD0;
    float2 uv1       : TEXCOORD1;
};

struct VSOut {
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float4 tangent   : TEXCOORD2;
    float2 uv0       : TEXCOORD3;
    float2 uv1       : TEXCOORD4;
    float3 viewDir   : TEXCOORD5;
    float4 shadow0   : TEXCOORD6;
    float4 shadow1   : TEXCOORD7;
    float4 shadow2   : TEXCOORD8;
    float4 shadow3   : TEXCOORD9;
};

// ─── Vertex Shader ────────────────────────────────────────────────────────────
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 worldPos4 = mul(float4(v.position, 1.0), modelMatrix);
    o.worldPos       = worldPos4.xyz;

    // ATTENTION: mul(vec4, mat4x4) en HLSL = vecteur-ligne × matrice
    // mul(mat4x4, vec4) = matrice × vecteur-colonne (comme GLSL)
    o.position  = mul(worldPos4, nk_ViewProjection);
    o.uv0 = v.uv0; o.uv1 = v.uv1;
    o.viewDir   = normalize(nk_CameraPosition.xyz - worldPos4.xyz);

    // Normal matrix : inverse transpose (approximation par cofacteurs pour SM5)
    float3x3 NM = (float3x3)modelMatrix;  // Pour les objets sans scale non-uniforme
    o.normal    = normalize(mul(v.normal, NM));
    float3 T    = normalize(mul(v.tangent.xyz, NM));
    o.tangent   = float4(T, v.tangent.w);

    // Coordonnées shadow
    o.shadow0 = mul(worldPos4, nk_ShadowMatrix0);
    o.shadow1 = mul(worldPos4, nk_ShadowMatrix1);
    o.shadow2 = mul(worldPos4, nk_ShadowMatrix2);
    o.shadow3 = mul(worldPos4, nk_ShadowMatrix3);

    return o;
}

// ─── Helpers PBR ──────────────────────────────────────────────────────────────
static const float PI = 3.14159265359;

float D_GGX(float NdotH, float rough) {
    float a2=pow(rough,4.0); float d=NdotH*NdotH*(a2-1.0)+1.0;
    return a2/(PI*d*d);
}
float G_Smith(float NdotV, float NdotL, float rough) {
    float k=(rough+1.0)*(rough+1.0)/8.0;
    return (NdotV/(NdotV*(1.0-k)+k))*(NdotL/(NdotL*(1.0-k)+k));
}
float3 F_Schlick(float cosT, float3 F0) {
    return F0+(1.0-F0)*pow(clamp(1.0-cosT,0.0,1.0),5.0);
}
float3 F_SchlickR(float cosT, float3 F0, float rough) {
    return F0+(max(1.0-rough,F0)-F0)*pow(clamp(1.0-cosT,0.0,1.0),5.0);
}

// ─── Shadow PCF 3x3 ───────────────────────────────────────────────────────────
float ShadowPCF(float4 sc, Texture2D sm, SamplerState s) {
    float3 proj=sc.xyz/sc.w;
    float2 uv=proj.xy*0.5+0.5; uv.y=1.0-uv.y;  // DX11: UV Y inversé
    float depth=proj.z-0.005;
    float shadow=0.0;
    float2 ts=1.0/float2(1024.0,1024.0);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        shadow+=(depth>sm.Sample(s,uv+float2(x,y)*ts).r)?1.0:0.0;
    }
    return 1.0-shadow/9.0;
}

// ─── Pixel Shader ─────────────────────────────────────────────────────────────
float4 PSMain(VSOut i) : SV_Target0 {
    float2 uv   = i.uv0 * float2(mat_uvScaleX, mat_uvScaleY) + mat_uvOffset;
    float3 V    = normalize(i.viewDir);
    float3 N    = normalize(i.normal);
    float3 T    = normalize(i.tangent.xyz - dot(i.tangent.xyz,N)*N);
    float3 B    = cross(N,T)*i.tangent.w;
    float3x3 TBN= float3x3(T,B,N);  // HLSL: ligne = vecteurs de la base

    // Normal map
    float3 tn   = tNormal.Sample(sLinear,uv).rgb*2.0-1.0;
    tn.xy      *= mat_normalStrength;
    N = normalize(mul(tn,TBN));  // HLSL: vec × mat (différent de GLSL!)

    // Matériau
    float4 albTex = tAlbedo.Sample(sLinear,uv);
    float3 base   = albTex.rgb * mat_albedo.rgb * customColor.rgb;
    float  alpha  = albTex.a  * mat_albedo.a;
    float2 mr     = tMetalRough.Sample(sLinear,uv).rg;
    float  metal  = mr.r * mat_metallic;
    float  rough  = max(mr.g * mat_roughness, 0.04);
    float  ao     = tAO.Sample(sLinear,uv).r;

    float3 F0     = lerp(0.04.xxx, base, metal);

    // ── Soleil ────────────────────────────────────────────────────────────────
    float viewDepth = -(mul(float4(i.worldPos,1.0),nk_View)).z;
    float shadow=1.0;
    if(viewDepth<nk_CascadeSplits.x)      shadow=ShadowPCF(i.shadow0,tShadowMap,sShadow);
    else if(viewDepth<nk_CascadeSplits.y) shadow=ShadowPCF(i.shadow1,tShadowMap,sShadow);
    else if(viewDepth<nk_CascadeSplits.z) shadow=ShadowPCF(i.shadow2,tShadowMap,sShadow);
    else                                   shadow=ShadowPCF(i.shadow3,tShadowMap,sShadow);

    float3 L=normalize(-nk_SunDirection.xyz);
    float3 H=normalize(V+L);
    float NdotL=max(dot(N,L),0.0), NdotV=max(dot(N,V),0.0);
    float NdotH=max(dot(N,H),0.0), HdotV=max(dot(H,V),0.0);
    float  D=D_GGX(NdotH,rough), G=G_Smith(NdotV,NdotL,rough);
    float3 F=F_Schlick(HdotV,F0);
    float3 kD=(1.0-F)*(1.0-metal);
    float3 Lo=(kD*base/PI + D*G*F/max(4.0*NdotV*NdotL,0.001))*NdotL*nk_SunColor.xyz*nk_SunColor.w*shadow;

    // ── IBL ───────────────────────────────────────────────────────────────────
    float3 R=reflect(-V,N);
    float3 irrad  = tEnvIrradiance.Sample(sLinear,N).rgb;
    float3 kD2    = (1.0-F_SchlickR(NdotV,F0,rough))*(1.0-metal);
    float3 diff   = kD2*irrad*base;
    float3 pref   = tEnvPrefilter.SampleLevel(sLinearClamp,R,rough*7.0).rgb;
    float2 brdf   = tBRDFLUT.Sample(sLinearClamp,float2(NdotV,rough)).rg;
    float3 spec   = pref*(F_SchlickR(NdotV,F0,rough)*brdf.x+brdf.y);
    float3 ambient=(diff+spec)*ao*nk_EnvIntensity;

    // Emission
    float3 em = tEmissive.Sample(sLinear,uv).rgb * mat_emissive.rgb * mat_emissiveStrength;

    float3 color = (Lo + ambient + em) * nk_Time.w;
    return float4(color, alpha);
}
