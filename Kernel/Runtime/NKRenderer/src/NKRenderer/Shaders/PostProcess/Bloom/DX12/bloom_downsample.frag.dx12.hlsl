// bloom_downsample.frag.dx12.hlsl — Bloom Downsample (DX12)
Texture2D tSrc:register(t0); SamplerState sLinear:register(s0);
cbuffer BloomUBO:register(b1){float threshold,bloomStrength,upsampleRadius,_p;};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    uint tw,th; tSrc.GetDimensions(tw,th); float2 ts=1./float2(tw,th);
    float3 a=tSrc.Sample(sLinear,i.uv+ts*float2(-2,2)).rgb; float3 b=tSrc.Sample(sLinear,i.uv+ts*float2(0,2)).rgb;
    float3 c=tSrc.Sample(sLinear,i.uv+ts*float2(2,2)).rgb;  float3 d=tSrc.Sample(sLinear,i.uv+ts*float2(-2,0)).rgb;
    float3 e=tSrc.Sample(sLinear,i.uv).rgb;                  float3 f=tSrc.Sample(sLinear,i.uv+ts*float2(2,0)).rgb;
    float3 g=tSrc.Sample(sLinear,i.uv+ts*float2(-2,-2)).rgb; float3 h=tSrc.Sample(sLinear,i.uv+ts*float2(0,-2)).rgb;
    float3 k=tSrc.Sample(sLinear,i.uv+ts*float2(2,-2)).rgb;  float3 l=tSrc.Sample(sLinear,i.uv+ts*float2(-1,1)).rgb;
    float3 m=tSrc.Sample(sLinear,i.uv+ts*float2(1,1)).rgb;   float3 n=tSrc.Sample(sLinear,i.uv+ts*float2(-1,-1)).rgb;
    float3 o=tSrc.Sample(sLinear,i.uv+ts*float2(1,-1)).rgb;
    float3 r=e*.125+(a+c+g+k)*.03125+(b+d+f+h)*.0625+(l+m+n+o)*.125;
    float lum=dot(r,float3(0.2126,0.7152,0.0722));
    r*=smoothstep(threshold-.1,threshold+.1,lum);
    return float4(r,1.);
}
