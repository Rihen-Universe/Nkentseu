// debug2d.vert.dx11.hlsl — 2D Batch Vertex (DX11)
cbuffer OrthoUBO:register(b0){column_major float4x4 ortho;float time;float3 _pad;};
struct VSIn{float2 pos:POSITION;float2 uv:TEXCOORD0;uint color:COLOR0;uint flags:COLOR1;};
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;uint flags:TEXCOORD2;};
VSOut main(VSIn v){VSOut o;o.pos=mul(ortho,float4(v.pos,0.,1.));o.uv=v.uv;
    o.color=float4(float((v.color    )&0xff)/255.,float((v.color>> 8)&0xff)/255.,float((v.color>>16)&0xff)/255.,float((v.color>>24)&0xff)/255.);
    o.flags=v.flags;return o;}
