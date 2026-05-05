// toon.vert.dx11.hlsl — Toon Vertex (DX11)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
cbuffer ObjectUBO:register(b1){column_major float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
struct VSIn{float3 pos:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:TEXCOORD3;};
VSOut main(VSIn v){VSOut o;float4 wp=mul(model,float4(v.pos,1.));o.worldPos=wp.xyz;o.normal=normalize(mul((float3x3)normalMatrix,v.normal));o.uv=v.uv;o.color=v.color*tint;o.pos=mul(viewProj,wp);return o;}
