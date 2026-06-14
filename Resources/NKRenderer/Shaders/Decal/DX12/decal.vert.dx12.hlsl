// decal.vert.dx12.hlsl — Decal Vertex (DX12)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 vp;float time,dt;};
cbuffer DecalUBO:register(b1){column_major float4x4 decalMatrix;float opacity,normalBlend;float _p[2];};
struct VSIn{float3 pos:POSITION;};struct VSOut{float4 pos:SV_Position;float3 decalPos:TEXCOORD0;};
VSOut main(VSIn v){VSOut o;float4 wp=mul(decalMatrix,float4(v.pos,1.));o.decalPos=v.pos*.5+.5;o.pos=mul(viewProj,wp);return o;}
