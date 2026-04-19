// =============================================================================
// toon.dx12.hlsl — Shader Toon DirectX 12 SM 6.0
// =============================================================================
struct SceneCB { matrix model,view,proj,lightVP; float4 lightDir,eyePos; float t,dt,zs,zo; };
struct ToonMat { float4 base,shadow,outline; float outW,shThr,shSmooth,spThr,spSmooth; uint steps; float2 _p; };

ConstantBuffer<SceneCB> gScene : register(b0);
ConstantBuffer<ToonMat> gMat   : register(b2);
Texture2D    gAlbedo  : register(t0);
SamplerState gSampler : register(s0);

struct VSIn { float3 pos:POSITION; float3 nrm:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct PSIn { float4 pos:SV_POSITION; float3 nrm:NORMAL; float3 fp:TEXCOORD0; float2 uv:TEXCOORD1; float4 col:COLOR; };

PSIn VSMain(VSIn v) {
    PSIn o;
    float4 wp=mul(float4(v.pos,1),gScene.model);
    o.fp=wp.xyz; o.nrm=normalize(mul(v.nrm,(float3x3)gScene.model));
    o.uv=v.uv; o.col=v.col;
    o.pos=mul(mul(wp,gScene.view),gScene.proj);
    return o;
}
float4 PSMain(PSIn i) : SV_Target {
    float4 albedo=gAlbedo.Sample(gSampler,i.uv)*gMat.base*i.col;
    float3 N=normalize(i.nrm),L=normalize(-gScene.lightDir.xyz),V=normalize(gScene.eyePos.xyz-i.fp);
    float NdL=dot(N,L);
    float t=saturate((NdL-gMat.shThr+gMat.shSmooth)/(2.0*gMat.shSmooth));
    float shadow=t*t*(3-2*t);
    if(gMat.steps>1){ float s=(float)gMat.steps; shadow=floor(shadow*s+0.5)/s; }
    float st=saturate((dot(N,normalize(V+L))-gMat.spThr+gMat.spSmooth)/(2.0*gMat.spSmooth));
    float spec=st*st*(3-2*st);
    return float4(lerp(gMat.shadow.rgb,albedo.rgb,shadow)+spec,albedo.a);
}
