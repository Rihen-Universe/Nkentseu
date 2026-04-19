// =============================================================================
// unlit.dx12.hlsl — Shader Unlit DirectX 12 SM 6.0
// =============================================================================
struct SceneCB { matrix model,view,proj,lightVP; float4 lightDir,eyePos; float t,dt,zs,zo; };
struct ObjCB   { matrix model; float4 tint; };
struct MatCB   { float4 base,emit; float clip,emitScale,p0,p1; };

ConstantBuffer<SceneCB> gScene : register(b0);
ConstantBuffer<ObjCB>   gObj   : register(b1);
ConstantBuffer<MatCB>   gMat   : register(b2);

Texture2D    gAlbedo  : register(t0);
SamplerState gSampler : register(s0);

struct VSIn { float3 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };

PSIn VSMain(VSIn v) {
    PSIn o;
    float4 wp = mul(float4(v.pos,1), gObj.model);
    o.pos = mul(mul(wp, gScene.view), gScene.proj);
    o.uv  = v.uv;
    o.col = v.col * gObj.tint;
    return o;
}
float4 PSMain(PSIn i) : SV_Target {
    float4 c = gAlbedo.Sample(gSampler,i.uv) * gMat.base * i.col;
    clip(c.a - gMat.clip);
    c.rgb += gMat.emit.rgb * gMat.emitScale;
    return c;
}
