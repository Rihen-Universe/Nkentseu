// unlit.frag.dx12.hlsl — Unlit Pixel (DX12)
struct RC{uint camIdx,objIdx,albedoTex,samplerIdx;};
ConstantBuffer<RC> gRC:register(b0,space0);
Texture2D gTex[]:register(t0,space0); SamplerState gSamp[]:register(s0,space0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
float4 main(PSIn i):SV_Target{return gTex[gRC.albedoTex].Sample(gSamp[gRC.samplerIdx],i.uv)*i.color;}
