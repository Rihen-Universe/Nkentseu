// bloom_upsample.frag.dx11.hlsl — Bloom Upsample (DX11)
Texture2D tSrc:register(t0);Texture2D tDst:register(t1);SamplerState sL:register(s0);
cbuffer BloomUBO:register(b2){float threshold,bloomStrength,upsampleRadius,_p;};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    uint tw,th;tSrc.GetDimensions(tw,th);float2 ts=1./float2(tw,th)*upsampleRadius;
    float3 s=tSrc.Sample(sL,i.uv+ts*float2(-1,1)).rgb+tSrc.Sample(sL,i.uv+ts*float2(0,2)).rgb*2.
             +tSrc.Sample(sL,i.uv+ts*float2(1,1)).rgb+tSrc.Sample(sL,i.uv+ts*float2(-2,0)).rgb*2.
             +tSrc.Sample(sL,i.uv).rgb*4.+tSrc.Sample(sL,i.uv+ts*float2(2,0)).rgb*2.
             +tSrc.Sample(sL,i.uv+ts*float2(-1,-1)).rgb+tSrc.Sample(sL,i.uv+ts*float2(0,-2)).rgb*2.
             +tSrc.Sample(sL,i.uv+ts*float2(1,-1)).rgb;
    return float4(s/16.*bloomStrength+tDst.Sample(sL,i.uv).rgb,1.);
}
