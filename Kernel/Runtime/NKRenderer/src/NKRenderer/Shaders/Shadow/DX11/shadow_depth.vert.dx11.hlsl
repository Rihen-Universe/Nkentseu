// shadow_depth.vert.dx11.hlsl — Shadow Depth Vertex (DX11)
cbuffer ShadowPassUBO:register(b0){column_major float4x4 model;column_major float4x4 lightViewProj;float ndcZScale;float ndcZOffset;float2 pad;};
struct VSIn{float3 pos:POSITION;};
float4 main(VSIn v):SV_Position{
    float4 wp=mul(model,float4(v.pos,1.0));
    float4 p=mul(lightViewProj,wp);
    p.z=p.z*ndcZScale+p.w*ndcZOffset;
    return p;
}
