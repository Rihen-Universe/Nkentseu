// trail.vert.dx12.hlsl — Trail Vertex (DX12)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 vp;float time,dt;};
struct VSIn{float3 pos:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
VSOut main(VSIn v){VSOut o;o.pos=mul(viewProj,float4(v.pos,1.));o.uv=v.uv;o.color=v.color;return o;}
