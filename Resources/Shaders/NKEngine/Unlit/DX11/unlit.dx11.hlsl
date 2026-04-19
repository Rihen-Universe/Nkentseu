// =============================================================================
// unlit.dx11.hlsl — Shader Unlit DirectX 11 SM 5.0
// =============================================================================
cbuffer SceneCB : register(b0) {
    matrix uModel; matrix uView; matrix uProj; matrix uLightVP;
    float4 uLightDir; float4 uEyePos;
    float uTime; float uDt; float uNdcZS; float uNdcZO;
};
cbuffer ObjectCB : register(b1) {
    matrix pcModel; float4 pcTint;
};
cbuffer MaterialCB : register(b2) {
    float4 uBaseColor; float4 uEmissiveColor;
    float uAlphaClip; float uEmissiveScale; float2 _pad;
};
Texture2D tAlbedo : register(t0);
SamplerState sLinear : register(s0);

struct VSIn  { float3 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct PSIn  { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };

PSIn VSMain(VSIn v) {
    PSIn o;
    float4 wp = mul(float4(v.pos,1), pcModel);
    float4 vp = mul(wp, uView);
    o.pos = mul(vp, uProj);
    o.uv  = v.uv;
    o.col = v.col * pcTint;
    return o;
}
float4 PSMain(PSIn i) : SV_Target {
    float4 c = tAlbedo.Sample(sLinear, i.uv) * uBaseColor * i.col;
    clip(c.a - uAlphaClip);
    c.rgb += uEmissiveColor.rgb * uEmissiveScale;
    return c;
}
