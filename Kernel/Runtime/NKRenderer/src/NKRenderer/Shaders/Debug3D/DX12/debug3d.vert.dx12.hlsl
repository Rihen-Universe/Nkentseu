// debug3d.vert.dx12.hlsl — Debug 3D Vertex (DX12)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
cbuffer ObjectUBO:register(b1){column_major float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
struct VSIn{float3 pos:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float3 normal:TEXCOORD0;float2 uv:TEXCOORD1;float4 color:TEXCOORD2;float depth:TEXCOORD3;};
VSOut main(VSIn v){VSOut o;float4 wp=mul(model,float4(v.pos,1.));o.normal=normalize(mul((float3x3)normalMatrix,v.normal));o.uv=v.uv;o.color=v.color*tint;o.pos=mul(viewProj,wp);o.depth=mul(view,wp).z;return o;}
