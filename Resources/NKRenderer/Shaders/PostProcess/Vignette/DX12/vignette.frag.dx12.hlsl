// vignette.frag.dx12.hlsl — Vignette + Grain (DX12)
Texture2D tLDR:register(t0);SamplerState sL:register(s0);
cbuffer VignetteUBO:register(b1){float vigIntensity,vigSmooth,grainStrength,time;};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float Hash(float2 p){return frac(sin(dot(p+time,float2(127.1,311.7)))*43758.5453);}
float4 main(PSIn i):SV_Target{
    float3 c=tLDR.Sample(sL,i.uv).rgb;
    float2 uv=i.uv*2.-1.;float vig=smoothstep(1.,1.-vigSmooth,length(uv)*vigIntensity);c*=vig;
    c+=((float3)(Hash(i.uv)-.5))*grainStrength;
    return float4(saturate(c),1.);
}
