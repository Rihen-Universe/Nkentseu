// fullscreen.vert.dx12.hlsl — Fullscreen Triangle Vertex (DX11/DX12)
struct VSOut{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
VSOut main(uint id:SV_VertexID){
    VSOut o;
    float2 p=float2((id&1u)?3.:-1.,(id&2u)?-3.:1.);
    o.pos=float4(p,0.,1.);
    o.uv=float2(p.x*.5+.5,-p.y*.5+.5);
    return o;
}
