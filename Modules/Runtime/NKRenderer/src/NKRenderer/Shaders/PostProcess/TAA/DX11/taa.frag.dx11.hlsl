// taa.frag.dx11.hlsl — TAA (DX11)
Texture2D tCurr:register(t0);Texture2D tHist:register(t1);Texture2D tVel:register(t2);SamplerState sL:register(s0);
cbuffer TAAUBO:register(b3){float blendFactor;float _p[3];};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    float2 vel=tVel.Sample(sL,i.uv).rg; float2 huv=i.uv-vel;
    float3 curr=tCurr.Sample(sL,i.uv).rgb;
    float3 nMin=curr,nMax=curr;uint tw,th;tCurr.GetDimensions(tw,th);float2 ts=1./float2(tw,th);
    [unroll]for(int x=-1;x<=1;x++)[unroll]for(int y=-1;y<=1;y++){float3 n=tCurr.Sample(sL,i.uv+float2(x,y)*ts).rgb;nMin=min(nMin,n);nMax=max(nMax,n);}
    float3 hist=clamp(tHist.Sample(sL,huv).rgb,nMin,nMax);
    float blend=blendFactor;if(any(huv<0.)||any(huv>1.))blend=1.;
    return float4(lerp(hist,curr,blend),1.);
}
