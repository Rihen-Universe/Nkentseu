// =============================================================================
// pbr_metallic.dx11.hlsl — PBR Metallic-Roughness DirectX 11 / SM 5.0
// =============================================================================

// ── Uniform Buffers ───────────────────────────────────────────────────────────
cbuffer SceneCB : register(b0) {
    matrix uModel;
    matrix uView;
    matrix uProj;
    matrix uLightVP;
    float4 uLightDir;
    float4 uEyePos;
    float  uTime;
    float  uDeltaTime;
    float  uNdcZScale;
    float  uNdcZOffset;
};
cbuffer MaterialCB : register(b2) {
    float4 uBaseColor;
    float4 uEmissiveColor;
    float  uMetallic;
    float  uRoughness;
    float  uOcclusion;
    float  uNormalScale;
    float  uAlphaClip;
    float  uEmissiveScale;
    float  uClearcoat;
    float  uClearcoatRoughness;
    float  uTransmission;
    float  uIOR;
    float2 _pad;
};

// ── Samplers et Textures ──────────────────────────────────────────────────────
Texture2D   tAlbedo      : register(t0);
Texture2D   tNormal      : register(t1);
Texture2D   tORM         : register(t2);
Texture2D   tEmissive    : register(t3);
Texture2D   tShadow      : register(t4);
TextureCube tEnvIrrad    : register(t5);
TextureCube tEnvPrefilt  : register(t6);
Texture2D   tBRDF_LUT    : register(t7);

SamplerState sLinear     : register(s0);
SamplerState sClamp      : register(s1);
SamplerComparisonState sShadow : register(s2);

// ── Structures ────────────────────────────────────────────────────────────────
struct VSInput {
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

struct PSInput {
    float4 pos      : SV_POSITION;
    float3 fragPos  : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float3 tangent  : TEXCOORD2;
    float3 bitangent: TEXCOORD3;
    float2 uv       : TEXCOORD4;
    float4 color    : COLOR;
    float4 fragPosLS: TEXCOORD5;
};

// ── Vertex Shader ────────────────────────────────────────────────────────────
PSInput VSMain(VSInput v) {
    PSInput o;
    float4 worldPos = mul(float4(v.pos,1), uModel);
    o.fragPos  = worldPos.xyz;

    float3x3 normalMat = (float3x3)transpose((float3x3)uModel); // approx
    o.normal   = normalize(mul(v.normal,   normalMat));
    o.tangent  = normalize(mul(v.tangent,  normalMat));
    o.bitangent= normalize(cross(o.normal, o.tangent));
    o.uv       = v.uv;
    o.color    = v.color;
    o.fragPosLS= mul(worldPos, uLightVP);

    float4 vp  = mul(worldPos, uView);
    o.pos      = mul(vp,       uProj);
    return o;
}

// ── Fonctions PBR ────────────────────────────────────────────────────────────
float D_GGX(float3 N, float3 H, float rough) {
    float a2 = rough*rough*rough*rough;
    float NdH= saturate(dot(N,H));
    float d  = NdH*NdH*(a2-1)+1;
    return a2/(3.14159265*d*d);
}
float G_Schlick(float NdV, float rough) {
    float k = (rough+1)*(rough+1)/8;
    return NdV/(NdV*(1-k)+k);
}
float G_Smith(float3 N, float3 V, float3 L, float rough) {
    return G_Schlick(saturate(dot(N,V)),rough)*G_Schlick(saturate(dot(N,L)),rough);
}
float3 F_Schlick(float c, float3 F0) {
    return F0 + (1-F0)*pow(saturate(1-c),5);
}
float3 F_SchlickR(float c, float3 F0, float rough) {
    return F0 + (max(float3(1-rough,1-rough,1-rough),F0)-F0)*pow(saturate(1-c),5);
}
float ComputeShadow(float4 ls, float3 N, float3 L) {
    float3 p = ls.xyz/ls.w;
    p.xy = p.xy*0.5+0.5;
    p.y  = 1.0-p.y;  // DX : Y=0 en haut
    if (any(p<0)||any(p>1)||p.z>1) return 0;
    float bias = max(0.005*(1-dot(N,L)),0.0005);
    float shadow = 0;
    float2 ts = 1.0/float2(2048,2048);
    [unroll] for(int x=-1;x<=1;++x)
    [unroll] for(int y=-1;y<=1;++y) {
        shadow += tShadow.SampleCmpLevelZero(sShadow, p.xy+float2(x,y)*ts, p.z-bias);
    }
    return shadow/9.0;
}
float3 GetNormal(float3 N, float3 T, float3 B, float2 uv) {
    float3 tn = tNormal.Sample(sLinear,uv).xyz*2-1;
    tn.xy *= uNormalScale;
    return normalize(mul(tn, float3x3(T,B,N)));
}

// ── Fragment / Pixel Shader ───────────────────────────────────────────────────
float4 PSMain(PSInput i) : SV_Target {
    float4 albedoT = pow(tAlbedo.Sample(sLinear,i.uv), 2.2);
    float3 orm     = tORM.Sample(sLinear, i.uv).rgb;
    float3 emitT   = pow(tEmissive.Sample(sLinear,i.uv).rgb, 2.2);

    float4 albedo = albedoT * uBaseColor * i.color;
    if (albedo.a < uAlphaClip) discard;

    float ao    = orm.r * uOcclusion;
    float rough = clamp(orm.g * uRoughness, 0.04, 1.0);
    float metal = orm.b * uMetallic;

    float3 N = GetNormal(normalize(i.normal), normalize(i.tangent), normalize(i.bitangent), i.uv);
    float3 V = normalize(uEyePos.xyz - i.fragPos);
    float3 R = reflect(-V,N);
    float3 F0= lerp(float3(0.04,0.04,0.04), albedo.rgb, metal);

    // Directional light
    float3 Lo = 0;
    {
        float3 L  = normalize(-uLightDir.xyz);
        float3 H  = normalize(V+L);
        float NdL = saturate(dot(N,L));
        float D   = D_GGX(N,H,rough);
        float G   = G_Smith(N,V,L,rough);
        float3 F  = F_Schlick(saturate(dot(H,V)),F0);
        float3 spec = D*G*F/(4*saturate(dot(N,V))*NdL+0.0001);
        float3 kD   = (1-F)*(1-metal);
        float shadow= ComputeShadow(i.fragPosLS, N, L);
        Lo += (kD*albedo.rgb/3.14159265 + spec)*float3(5,5,5)*NdL*(1-shadow);
    }

    // IBL
    float3 F_ibl  = F_SchlickR(saturate(dot(N,V)),F0,rough);
    float3 kD_ibl = (1-F_ibl)*(1-metal);
    float3 irrad  = tEnvIrrad.Sample(sLinear,N).rgb;
    float3 diffIBL= irrad*albedo.rgb;
    float3 prefilt= tEnvPrefilt.SampleLevel(sLinear,R,rough*4).rgb;
    float2 brdf   = tBRDF_LUT.Sample(sClamp, float2(saturate(dot(N,V)),rough)).rg;
    float3 specIBL= prefilt*(F_ibl*brdf.x+brdf.y);
    float3 ambient= (kD_ibl*diffIBL+specIBL)*ao;

    // Emissive
    float3 emit = (emitT + uEmissiveColor.rgb)*uEmissiveScale;

    return float4(ambient + Lo + emit, albedo.a);
}
