// debug3d.frag.dx11.hlsl — Debug 3D Pixel (DX11)
cbuffer DebugUBO:register(b4){int mode;float nearPlane,farPlane,_pad;};
struct PSIn{float4 pos:SV_Position;float3 normal:TEXCOORD0;float2 uv:TEXCOORD1;float4 color:TEXCOORD2;float depth:TEXCOORD3;};
float4 main(PSIn i):SV_Target{
    if(mode==0) return float4(i.normal*0.5+0.5,1.);
    if(mode==1){float2 c=floor(i.uv*8.);float ch=fmod(c.x+c.y,2.);return float4((float3)lerp(0.2,0.8,ch),1.);}
    if(mode==2){float nd=(-i.depth-nearPlane)/(farPlane-nearPlane);return float4((float3)nd,1.);}
    return i.color;
}
