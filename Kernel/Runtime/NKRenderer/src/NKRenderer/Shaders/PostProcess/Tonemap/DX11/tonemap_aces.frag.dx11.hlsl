// tonemap_aces.frag.dx11.hlsl — ACES Filmic Tonemap (DX11)
Texture2D tHDR:register(t0); SamplerState sLinear:register(s0);
cbuffer TonemapUBO:register(b1){float exposure,gamma,contrast,saturation;float _p[4];};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    float3 x=tHDR.Sample(sLinear,i.uv).rgb*exposure;
    float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
    float3 ldr=saturate((x*(a*x+b))/(x*(c*x+d)+e));
    ldr=pow(ldr,(float3)(1./gamma));
    return float4(ldr,1.);
}
