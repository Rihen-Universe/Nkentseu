// ============================================================
// pbr.frag.dx12.hlsl — NKRenderer v4.0 — PBR Pixel (DX12 SM6)
// ============================================================
struct RootConstants { uint cameraIdx,objectIdx,lightIdx,shadowIdx;
    uint albedoTex,normalTex,ormTex,emissiveTex;
    uint envIrrTex,envPrefTex,brdfLutTex,shadowMapTex;
    uint samplerIdx,shadowSamplerIdx; uint pad[2]; };
ConstantBuffer<RootConstants> gRC : register(b0, space0);

struct CameraData {float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
struct ObjectData {float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
struct LightsData {float4 pos[32],col[32],dir[32],ang[32];int count;int pad[3];};
struct ShadowData {float4x4 cm[4];float4 splits;int cnt;float bias,normBias;int soft;};

ConstantBuffer<CameraData> gCameras[] : register(b0,space1);
ConstantBuffer<ObjectData>  gObjects[] : register(b0,space2);
ConstantBuffer<LightsData>  gLights[]  : register(b0,space3);
ConstantBuffer<ShadowData>  gShadows[] : register(b0,space4);
Texture2D    gTextures2D[]   : register(t0,space0);
TextureCube  gTexturesCube[] : register(t0,space1);
SamplerState gSamplers[]     : register(s0,space0);
SamplerComparisonState gShadowSamplers[] : register(s0,space1);

static const float PI = 3.14159265358979;
float D_GGX(float3 N,float3 H,float r){float a=r*r,a2=a*a,d=max(dot(N,H),0.);d=d*d*(a2-1.)+1.;return a2/(PI*d*d+1e-4);}
float GS(float x,float k){return x/(x*(1.-k)+k);}
float GSm(float3 N,float3 V,float3 L,float r){float k=(r+1.)*(r+1.)/8.;return GS(max(dot(N,V),0.),k)*GS(max(dot(N,L),0.),k);}
float3 FS(float c,float3 F0){return F0+(1.-F0)*pow(max(1.-c,0.),5.);}
float3 FSR(float c,float3 F0,float r){return F0+(max((float3)(1.-r),F0)-F0)*pow(max(1.-c,0.),5.);}

struct PSIn{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float3 tangent:TEXCOORD2;float3 bitangent:TEXCOORD3;float2 uv:TEXCOORD4;float2 uv2:TEXCOORD5;float4 color:TEXCOORD6;float4 sc0:TEXCOORD7;float4 sc1:TEXCOORD8;float4 sc2:TEXCOORD9;float4 sc3:TEXCOORD10;};

float ShadowPCF(Texture2D<float> sm, SamplerComparisonState ss2, float4 coord, float bias) {
    float3 p=coord.xyz/coord.w; p.x=p.x*.5+.5; p.y=-p.y*.5+.5; p.z-=bias;
    if(any(p.xy<0.)||any(p.xy>1.)||p.z>1.) return 1.;
    uint sw,sh; sm.GetDimensions(sw,sh); float2 ts=1./float2(sw,sh);
    float s=0.; [unroll]for(int x=-1;x<=1;x++)[unroll]for(int y=-1;y<=1;y++) s+=sm.SampleCmpLevelZero(ss2,p.xy+float2(x,y)*ts,p.z);
    return s/9.;
}

float4 main(PSIn i) : SV_Target {
    CameraData cam=gCameras[gRC.cameraIdx]; ObjectData obj=gObjects[gRC.objectIdx];
    LightsData lights=gLights[gRC.lightIdx]; ShadowData shd=gShadows[gRC.shadowIdx];
    SamplerState ss=gSamplers[gRC.samplerIdx];

    float4 albSamp=gTextures2D[gRC.albedoTex].Sample(ss,i.uv)*i.color;
    float3 albedo=pow(albSamp.rgb,(float3)2.2); float alpha=albSamp.a; clip(alpha-.01);
    float3 orm=gTextures2D[gRC.ormTex].Sample(ss,i.uv).rgb;
    float ao=orm.r*obj.aoStr, rog=orm.g*obj.roughness, met=orm.b*obj.metallic;
    float3 nTs=gTextures2D[gRC.normalTex].Sample(ss,i.uv).xyz*2.-1.; nTs.xy*=obj.normStr;
    float3x3 TBN=float3x3(normalize(i.tangent),normalize(i.bitangent),normalize(i.normal));
    float3 N=normalize(mul(nTs,TBN)), V=normalize(cam.camPos.xyz-i.worldPos);
    float3 F0=lerp((float3)0.04,albedo,met);
    Texture2D<float> shadowTex=gTextures2D[gRC.shadowMapTex];
    SamplerComparisonState ssc=gShadowSamplers[gRC.shadowSamplerIdx];
    float shadow=ShadowPCF(shadowTex,ssc,i.sc0,shd.bias);
    float3 Lo=(float3)0.;
    [loop]for(int li=0;li<lights.count&&li<32;li++){
        int lt=int(lights.pos[li].w);float3 L;float att=1.;
        if(lt==0){L=normalize(-lights.dir[li].xyz);}
        else{float3 d=lights.pos[li].xyz-i.worldPos;float dist=length(d);L=normalize(d);att=max(1.-dist/max(lights.dir[li].w,.001),0.);att*=att;}
        float csf=(lights.ang[li].z>.5)?shadow:1.;
        float3 H=normalize(V+L);float NdL=max(dot(N,L),0.);float3 rad=lights.col[li].rgb*lights.col[li].w*att;
        float NDF=D_GGX(N,H,rog);float G=GSm(N,V,L,rog);float3 F=FS(max(dot(H,V),0.),F0);
        Lo+=csf*(((1.-F)*(1.-met))*albedo/PI+NDF*G*F/(4.*max(dot(N,V),0.)*NdL+1e-4))*rad*NdL;
    }
    float3 Fi=FSR(max(dot(N,V),0.),F0,rog),kDi=(1.-Fi)*(1.-met);
    float3 irr=gTexturesCube[gRC.envIrrTex].Sample(ss,N).rgb, R=reflect(-V,N);
    float3 pref=gTexturesCube[gRC.envPrefTex].SampleLevel(ss,R,rog*4.).rgb;
    float2 brdf=gTextures2D[gRC.brdfLutTex].Sample(ss,float2(max(dot(N,V),0.),rog)).rg;
    float3 amb=(kDi*irr*albedo+pref*(Fi*brdf.x+brdf.y))*ao;
    float3 emissive=gTextures2D[gRC.emissiveTex].Sample(ss,i.uv).rgb*obj.emissStr;
    return float4(amb+Lo+emissive,alpha);
}
