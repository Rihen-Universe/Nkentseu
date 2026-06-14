// unlit.vert.dx11.hlsl — Unlit Vertex (DX11)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
cbuffer ObjectUBO:register(b1){column_major float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
struct VSIn{float3 pos:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
VSOut main(VSIn v){VSOut o;o.pos=mul(viewProj,mul(model,float4(v.pos,1.)));o.uv=v.uv;o.color=v.color*tint;return o;}
