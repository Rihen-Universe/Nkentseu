// particles.vert.dx12.hlsl — Particle Vertex (DX12) — expands in GS
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
struct VSIn{float3 pos:POSITION;float2 uv:TEXCOORD0;uint color:COLOR0;float size:TEXCOORD1;float rotation:TEXCOORD2;};
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;float size:TEXCOORD2;float rotation:TEXCOORD3;};
VSOut main(VSIn v){VSOut o;o.pos=float4(v.pos,1.);o.uv=v.uv;
    o.color=float4(float((v.color    )&0xff)/255.,float((v.color>> 8)&0xff)/255.,float((v.color>>16)&0xff)/255.,float((v.color>>24)&0xff)/255.);
    o.size=v.size;o.rotation=v.rotation;return o;}
