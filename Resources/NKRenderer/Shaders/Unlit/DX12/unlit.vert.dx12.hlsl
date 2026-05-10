// unlit.vert.dx12.hlsl — Unlit Vertex (DX12)
struct RC{uint camIdx,objIdx,albedoTex,samplerIdx;};
ConstantBuffer<RC> gRC:register(b0,space0);
struct CamD{float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 vp;float time,dt;};
struct ObjD{float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
ConstantBuffer<CamD> gCam[]:register(b0,space1); ConstantBuffer<ObjD> gObj[]:register(b0,space2);
Texture2D gTex[]:register(t0,space0); SamplerState gSamp[]:register(s0,space0);
struct VSIn{float3 pos:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
VSOut main(VSIn v){ObjD obj=gObj[gRC.objIdx];CamD cam=gCam[gRC.camIdx];VSOut o;o.pos=mul(cam.viewProj,mul(obj.model,float4(v.pos,1.)));o.uv=v.uv;o.color=v.color*obj.tint;return o;}
