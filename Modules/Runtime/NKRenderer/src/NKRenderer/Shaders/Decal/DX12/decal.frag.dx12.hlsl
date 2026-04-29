// decal.frag.dx12.hlsl — Decal Pixel (DX12)
cbuffer DecalUBO:register(b1){column_major float4x4 decalMatrix;float opacity,normalBlend;float _p[2];};
Texture2D tAlbedo:register(t2);SamplerState sL:register(s0);
struct PSIn{float4 pos:SV_Position;float3 decalPos:TEXCOORD0;};
float4 main(PSIn i):SV_Target{if(any(i.decalPos<0.01)||any(i.decalPos>0.99))discard;float4 a=tAlbedo.Sample(sL,i.decalPos.xz);clip(a.a-.01);return float4(a.rgb,a.a*opacity);}
