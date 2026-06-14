// unlit.frag.dx11.hlsl — Unlit Pixel (DX11)
Texture2D tAlbedo:register(t4); SamplerState sLinear:register(s0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
float4 main(PSIn i):SV_Target{return tAlbedo.Sample(sLinear,i.uv)*i.color;}
