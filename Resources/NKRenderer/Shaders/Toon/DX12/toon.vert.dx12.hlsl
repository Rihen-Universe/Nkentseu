// toon.vert.dx12.hlsl — Toon Vertex (DX12 SM6)
// Reuses same code as DX11 with root constants for bindless
struct RC{uint camIdx,objIdx,lightIdx,toonIdx;};
ConstantBuffer<RC> gRC:register(b0,space0);
struct CamD{float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
struct ObjD{float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
ConstantBuffer<CamD> gCam[]:register(b0,space1);
ConstantBuffer<ObjD> gObj[]:register(b0,space2);
struct VSIn{float3 pos:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:TEXCOORD3;};
VSOut main(VSIn v){ObjD obj=gObj[gRC.objIdx];CamD cam=gCam[gRC.camIdx];VSOut o;float4 wp=mul(obj.model,float4(v.pos,1.));o.worldPos=wp.xyz;o.normal=normalize(mul((float3x3)obj.normalMatrix,v.normal));o.uv=v.uv;o.color=v.color*obj.tint;o.pos=mul(cam.viewProj,wp);return o;}
