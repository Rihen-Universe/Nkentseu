// ============================================================
// pbr.frag.dx11.hlsl — NKRenderer v4.0 — PBR Pixel (DX11 SM5)
// ============================================================
cbuffer CameraUBO : register(b0) {
    column_major float4x4 view,proj,viewProj,invViewProj;
    float4 camPos,camDir; float2 viewport; float time,dt;
};
cbuffer ObjectUBO : register(b1) {
    column_major float4x4 model,normalMatrix; float4 tint;
    float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,ccRough,sss;
    float4 sssColor;
};
cbuffer LightsUBO : register(b2) {
    float4 positions[32],colors[32],directions[32],angles[32];
    int count; int _pad[3];
};
cbuffer ShadowUBO : register(b3) {
    column_major float4x4 cascadeMats[4]; float4 cascadeSplits;
    int cascadeCount; float shadowBias,normalBias; int softShadows;
};

Texture2D    tAlbedo        : register(t4);
Texture2D    tNormal        : register(t5);
Texture2D    tORM           : register(t6);
Texture2D    tEmissive      : register(t7);
TextureCube  tEnvIrradiance : register(t8);
TextureCube  tEnvPrefilter  : register(t9);
Texture2D    tBRDFLUT       : register(t10);
Texture2D<float> tShadowMap : register(t11);

SamplerState       sLinear  : register(s0);
SamplerState       sLinearC : register(s1);  // cubemap
SamplerComparisonState sShadow : register(s2);

static const float PI = 3.14159265358979;

float D_GGX(float3 N,float3 H,float r){float a=r*r,a2=a*a,NdH=max(dot(N,H),0.);float d=NdH*NdH*(a2-1.)+1.;return a2/(PI*d*d+1e-4);}
float G_S(float x,float k){return x/(x*(1.-k)+k);}
float G_Smith(float3 N,float3 V,float3 L,float r){float k=(r+1.)*(r+1.)/8.;return G_S(max(dot(N,V),0.),k)*G_S(max(dot(N,L),0.),k);}
float3 F_Sch(float c,float3 F0){return F0+(1.-F0)*pow(max(1.-c,0.),5.);}
float3 F_SchR(float c,float3 F0,float r){return F0+(max((float3)(1.-r),F0)-F0)*pow(max(1.-c,0.),5.);}

float ShadowPCF(float4 coord) {
    float3 p = coord.xyz / coord.w;
    // DX: NDC y-flip for UV, z in [0,1]
    p.x =  p.x * 0.5 + 0.5;
    p.y = -p.y * 0.5 + 0.5;
    p.z -= shadowBias;
    if (any(p.xy < 0.0)||any(p.xy > 1.0)||p.z > 1.0) return 1.0;
    uint sw,sh; tShadowMap.GetDimensions(sw,sh);
    float2 ts = 1.0/float2(sw,sh);
    float s=0.0;
    [unroll] for(int x=-1;x<=1;x++) [unroll] for(int y=-1;y<=1;y++)
        s += tShadowMap.SampleCmpLevelZero(sShadow, p.xy+float2(x,y)*ts, p.z);
    return s/9.0;
}

struct PSIn {
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

float4 main(PSIn i) : SV_Target {
    float4 albSamp = tAlbedo.Sample(sLinear,i.uv)*i.color;
    float3 albedo  = pow(albSamp.rgb,(float3)2.2);
    float alpha    = albSamp.a;
    clip(alpha-0.01);

    float3 orm = tORM.Sample(sLinear,i.uv).rgb;
    float ao=orm.r*aoStrength, rog=orm.g*roughness, met=orm.b*metallic;

    float3 nTs  = tNormal.Sample(sLinear,i.uv).xyz*2.-1.; nTs.xy*=normalStrength;
    float3x3 TBN = float3x3(normalize(i.tangent),normalize(i.bitangent),normalize(i.normal));
    float3 N = normalize(mul(nTs,TBN)); // TBN^T * nTs
    float3 V = normalize(camPos.xyz-i.worldPos);
    float3 F0= lerp((float3)0.04,albedo,met);

    float shadow = ShadowPCF(i.shadowCoord0); // cascade 0 (simplifié)

    float3 Lo=(float3)0.;
    [loop] for(int li=0;li<count&&li<32;li++){
        int lt=int(positions[li].w); float3 L; float att=1.;
        if(lt==0){L=normalize(-directions[li].xyz);}
        else{float3 d=positions[li].xyz-i.worldPos;float dist=length(d);L=normalize(d);att=max(1.-dist/max(directions[li].w,.001),0.);att*=att;
             if(lt==2){float th=dot(L,normalize(-directions[li].xyz));att*=saturate((th-angles[li].y)/(angles[li].x-angles[li].y+1e-4));}}
        float csf=(angles[li].z>.5)?shadow:1.;
        float3 H=normalize(V+L); float NdL=max(dot(N,L),0.); float3 rad=colors[li].rgb*colors[li].w*att;
        float NDF=D_GGX(N,H,rog); float G=G_Smith(N,V,L,rog); float3 F=F_Sch(max(dot(H,V),0.),F0);
        float3 spec=NDF*G*F/(4.*max(dot(N,V),0.)*NdL+1e-4);
        Lo+=csf*(((1.-F)*(1.-met))*albedo/PI+spec)*rad*NdL;
    }

    float3 Fi=F_SchR(max(dot(N,V),0.),F0,rog), kDi=(1.-Fi)*(1.-met);
    float3 irr=tEnvIrradiance.Sample(sLinearC,N).rgb, R=reflect(-V,N);
    float3 pref=tEnvPrefilter.SampleLevel(sLinearC,R,rog*4.).rgb;
    float2 brdf=tBRDFLUT.Sample(sLinear,float2(max(dot(N,V),0.),rog)).rg;
    float3 amb=(kDi*irr*albedo+pref*(Fi*brdf.x+brdf.y))*ao;
    float3 emissive=tEmissive.Sample(sLinear,i.uv).rgb*emissiveStrength;
    return float4(amb+Lo+emissive, alpha);
}
