// =============================================================================
// toon.dx11.hlsl — Shader Toon / Cel-shading DirectX 11 SM 5.0
// =============================================================================
cbuffer SceneCB : register(b0) {
    matrix uModel,uView,uProj,uLightVP;
    float4 uLightDir,uEyePos;
    float uTime,uDt,uNdcZS,uNdcZO;
};
cbuffer ToonMat : register(b2) {
    float4 uBaseColor,uShadowColor,uOutlineColor;
    float uOutlineWidth,uShadowThreshold,uShadowSmoothness;
    float uSpecThreshold,uSpecSmoothness;
    uint  uShadeSteps;
    float _p0,_p1;
};
Texture2D tAlbedo : register(t0);
SamplerState sLinear : register(s0);

struct VSIn  { float3 pos:POSITION; float3 nrm:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct PSIn  { float4 pos:SV_POSITION; float3 nrm:NORMAL; float3 fp:TEXCOORD0; float2 uv:TEXCOORD1; float4 col:COLOR; };

PSIn VSMain(VSIn v) {
    PSIn o;
    float4 wp  = mul(float4(v.pos,1),uModel);
    o.fp       = wp.xyz;
    o.nrm      = normalize(mul(v.nrm,(float3x3)uModel));
    o.uv       = v.uv; o.col = v.col;
    o.pos      = mul(mul(wp,uView),uProj);
    return o;
}

float smoothstep_f(float edge0,float edge1,float x){ float t=saturate((x-edge0)/(edge1-edge0)); return t*t*(3-2*t); }

float4 PSMain(PSIn i) : SV_Target {
    float4 albedo = tAlbedo.Sample(sLinear,i.uv)*uBaseColor*i.col;
    float3 N=normalize(i.nrm), L=normalize(-uLightDir.xyz), V=normalize(uEyePos.xyz-i.fp);
    float NdL=dot(N,L);
    float shadow=smoothstep_f(uShadowThreshold-uShadowSmoothness,uShadowThreshold+uShadowSmoothness,NdL);
    if(uShadeSteps>1){ float s=(float)uShadeSteps; shadow=floor(shadow*s+0.5)/s; }
    float spec=smoothstep_f(uSpecThreshold-uSpecSmoothness,uSpecThreshold+uSpecSmoothness,dot(N,normalize(V+L)));
    float3 color=lerp(uShadowColor.rgb,albedo.rgb,shadow)+spec;
    return float4(color,albedo.a);
}
