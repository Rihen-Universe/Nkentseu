// particles.frag.dx11.hlsl — Particle Pixel (DX11)
Texture2D tParticle:register(t0); SamplerState sLinear:register(s0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
float4 main(PSIn i):SV_Target{float4 t=tParticle.Sample(sLinear,i.uv)*i.color;clip(t.a-0.02);return t;}
