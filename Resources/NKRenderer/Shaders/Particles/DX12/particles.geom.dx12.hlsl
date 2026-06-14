// particles.geom.dx12.hlsl — Particle Billboard GS (DX12)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
struct GSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;float size:TEXCOORD2;float rotation:TEXCOORD3;};
struct GSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:TEXCOORD1;};
[maxvertexcount(4)]
void main(point GSIn g[1], inout TriangleStream<GSOut> stream){
    float hs=g[0].size*0.5,rot=g[0].rotation;
    float co=cos(rot),si=sin(rot);
    float3 right=float3(view[0][0],view[1][0],view[2][0]);
    float3 up   =float3(view[0][1],view[1][1],view[2][1]);
    float3 center=g[0].pos.xyz;
    float3 rr=(right*co-up*si)*hs,ru=(right*si+up*co)*hs;
    float2 uvs[4]={float2(0,1),float2(1,1),float2(0,0),float2(1,0)};
    float3 corners[4]={center-rr-ru,center+rr-ru,center-rr+ru,center+rr+ru};
    [unroll]for(int i=0;i<4;i++){GSOut o;o.pos=mul(viewProj,float4(corners[i],1.));o.uv=uvs[i];o.color=g[0].color;stream.Append(o);}
}
