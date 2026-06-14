// trail.frag.dx11.hlsl — Trail Pixel (DX11)
Texture2D tTrail:register(t0);SamplerState sL:register(s0);
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
float4 main(PSIn i):SV_Target{float4 t=tTrail.Sample(sL,i.uv)*i.color;float e=smoothstep(0.,.15,i.uv.x)*smoothstep(0.,.15,1.-i.uv.x);t.a*=e;clip(t.a-.01);return t;}
