// debug2d.frag.dx12.hlsl — 2D Batch Pixel (DX12)
Texture2D tAtlas:register(t0); SamplerState sLinear:register(s0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;uint flags:TEXCOORD2;};
float4 main(PSIn i):SV_Target{
    if((i.flags&2u)!=0){float4 t=tAtlas.Sample(sLinear,i.uv);
        if((i.flags&1u)!=0){float d=t.a,aa=abs(ddx(d))+abs(ddy(d));float a=saturate((d-0.5+aa)/aa);clip(a-0.01);return float4(i.color.rgb,i.color.a*a);}
        float4 r=t*i.color;clip(r.a-0.01);return r;}
    clip(i.color.a-0.01);return i.color;
}
