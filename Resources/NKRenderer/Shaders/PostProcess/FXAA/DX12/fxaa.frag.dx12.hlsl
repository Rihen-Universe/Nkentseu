// fxaa.frag.dx12.hlsl — FXAA (DX12)
Texture2D tLDR:register(t0);SamplerState sL:register(s0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float Luma(float3 c){return dot(c,float3(0.299,0.587,0.114));}
float4 main(PSIn i):SV_Target{
    float3 rgb=tLDR.Sample(sL,i.uv).rgb;
    uint tw,th;tLDR.GetDimensions(tw,th);float2 ts=1./float2(tw,th);
    float lumaM=Luma(rgb);
    float lumaN=Luma(tLDR.Sample(sL,i.uv+float2(0,ts.y)).rgb);
    float lumaS=Luma(tLDR.Sample(sL,i.uv-float2(0,ts.y)).rgb);
    float lumaW=Luma(tLDR.Sample(sL,i.uv-float2(ts.x,0)).rgb);
    float lumaE=Luma(tLDR.Sample(sL,i.uv+float2(ts.x,0)).rgb);
    float range=max(lumaM,max(max(lumaN,lumaS),max(lumaW,lumaE)))-min(lumaM,min(min(lumaN,lumaS),min(lumaW,lumaE)));
    if(range<0.03125)return float4(rgb,1.);
    bool horiz=abs(-2.*lumaW+Luma(tLDR.Sample(sL,i.uv+float2(-ts.x,ts.y)).rgb)+Luma(tLDR.Sample(sL,i.uv+float2(-ts.x,-ts.y)).rgb))*2.+abs(-2.*lumaM+lumaN+lumaS)<abs(-2.*lumaN+Luma(tLDR.Sample(sL,i.uv+float2(-ts.x,ts.y)).rgb)+Luma(tLDR.Sample(sL,i.uv+float2(ts.x,ts.y)).rgb))*2.+abs(-2.*lumaM+lumaW+lumaE);
    float2 dir=horiz?float2(0,ts.y):float2(ts.x,0);
    float3 c=lerp(rgb,(tLDR.Sample(sL,i.uv-dir).rgb+tLDR.Sample(sL,i.uv+dir).rgb)*.5,range/(range+lumaM)*.75);
    return float4(c,1.);
}
