// =============================================================================
// pbr_metallic.dx12.hlsl — PBR Metallic-Roughness DirectX 12 SM 6.0
// Utilise les Resource Binding Tier 3 (bindless via DescriptorHeap)
// =============================================================================
struct SceneCB {
    matrix model,view,proj,lightVP;
    float4 lightDir,eyePos;
    float time,dt,ndcZS,ndcZO;
};
struct MaterialCB {
    float4 baseColor,emissiveColor;
    float metallic,roughness,occlusion,normalScale;
    float alphaClip,emissiveScale,clearcoat,clearcoatRoughness;
    float transmission,ior,_p0,_p1;
};

ConstantBuffer<SceneCB>   gScene : register(b0);
ConstantBuffer<MaterialCB> gMat  : register(b2);

Texture2D    gAlbedo    : register(t0);
Texture2D    gNormal    : register(t1);
Texture2D    gORM       : register(t2);
Texture2D    gEmissive  : register(t3);
Texture2D    gShadow    : register(t4);
TextureCube  gIrrad     : register(t5);
TextureCube  gPrefilt   : register(t6);
Texture2D    gBRDF      : register(t7);

SamplerState            gLinear : register(s0);
SamplerState            gClamp  : register(s1);
SamplerComparisonState  gShadSamp : register(s2);

struct VSIn { float3 pos:POSITION; float3 nrm:NORMAL; float3 tan:TANGENT; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct PSIn {
    float4 pos:SV_POSITION; float3 fp:TEXCOORD0; float3 nrm:TEXCOORD1;
    float3 tan:TEXCOORD2; float3 btn:TEXCOORD3; float2 uv:TEXCOORD4;
    float4 col:COLOR; float4 fpLS:TEXCOORD5;
};

PSIn VSMain(VSIn v) {
    PSIn o;
    float4 wp=mul(float4(v.pos,1),gScene.model);
    o.fp=wp.xyz;
    float3x3 nm=(float3x3)transpose(gScene.model);
    o.nrm=normalize(mul(v.nrm,nm));
    o.tan=normalize(mul(v.tan,nm));
    o.btn=normalize(cross(o.nrm,o.tan));
    o.uv=v.uv; o.col=v.col;
    o.fpLS=mul(wp,gScene.lightVP);
    o.pos=mul(mul(wp,gScene.view),gScene.proj);
    return o;
}

// PBR helpers
float D_GGX(float3 N,float3 H,float r){float a2=r*r*r*r;float NdH=saturate(dot(N,H));float d=NdH*NdH*(a2-1)+1;return a2/(3.14159265*d*d);}
float G_Schlick(float NdV,float r){float k=(r+1)*(r+1)/8;return NdV/(NdV*(1-k)+k);}
float G_Smith(float3 N,float3 V,float3 L,float r){return G_Schlick(saturate(dot(N,V)),r)*G_Schlick(saturate(dot(N,L)),r);}
float3 F_Schlick(float c,float3 F0){return F0+(1-F0)*pow(saturate(1-c),5);}
float3 F_SchlickR(float c,float3 F0,float r){return F0+(max(float3(1-r,1-r,1-r),F0)-F0)*pow(saturate(1-c),5);}
float Shadow(float4 ls,float3 N,float3 L){
    float3 p=ls.xyz/ls.w; p.xy=p.xy*0.5+0.5; p.y=1-p.y;
    if(any(p<0)||any(p>1)||p.z>1) return 0;
    float bias=max(0.005*(1-dot(N,L)),0.0005);
    float sh=0; float2 ts=1.0/float2(2048,2048);
    [unroll] for(int x=-1;x<=1;++x) [unroll] for(int y=-1;y<=1;++y)
        sh+=gShadow.SampleCmpLevelZero(gShadSamp,p.xy+float2(x,y)*ts,p.z-bias);
    return sh/9.0;
}
float3 GetNormal(float3 N,float3 T,float3 B,float2 uv){
    float3 tn=gNormal.Sample(gLinear,uv).xyz*2-1;
    tn.xy*=gMat.normalScale;
    return normalize(mul(tn,float3x3(T,B,N)));
}

float4 PSMain(PSIn i) : SV_Target {
    float4 alb=pow(gAlbedo.Sample(gLinear,i.uv),2.2)*gMat.baseColor*i.col;
    clip(alb.a-gMat.alphaClip);
    float3 orm=gORM.Sample(gLinear,i.uv).rgb;
    float3 emitT=pow(gEmissive.Sample(gLinear,i.uv).rgb,2.2);
    float ao=orm.r*gMat.occlusion, rough=clamp(orm.g*gMat.roughness,0.04,1), metal=orm.b*gMat.metallic;
    float3 N=GetNormal(normalize(i.nrm),normalize(i.tan),normalize(i.btn),i.uv);
    float3 V=normalize(gScene.eyePos.xyz-i.fp), R=reflect(-V,N);
    float3 F0=lerp(float3(0.04,0.04,0.04),alb.rgb,metal);
    float3 Lo=0;
    {
        float3 L=normalize(-gScene.lightDir.xyz),H=normalize(V+L);
        float NdL=saturate(dot(N,L));
        float3 F=F_Schlick(saturate(dot(H,V)),F0);
        float3 spec=D_GGX(N,H,rough)*G_Smith(N,V,L,rough)*F/(4*saturate(dot(N,V))*NdL+1e-4);
        float3 kD=(1-F)*(1-metal);
        Lo+=(kD*alb.rgb/3.14159265+spec)*5.0*NdL*(1-Shadow(i.fpLS,N,L));
    }
    float3 F_ibl=F_SchlickR(saturate(dot(N,V)),F0,rough);
    float3 kD_ibl=(1-F_ibl)*(1-metal);
    float3 irr=gIrrad.Sample(gLinear,N).rgb;
    float3 pre=gPrefilt.SampleLevel(gLinear,R,rough*4).rgb;
    float2 brdf=gBRDF.Sample(gClamp,float2(saturate(dot(N,V)),rough)).rg;
    float3 amb=(kD_ibl*irr*alb.rgb+pre*(F_ibl*brdf.x+brdf.y))*ao;
    float3 emit=(emitT+gMat.emissiveColor.rgb)*gMat.emissiveScale;
    return float4(amb+Lo+emit,alb.a);
}
