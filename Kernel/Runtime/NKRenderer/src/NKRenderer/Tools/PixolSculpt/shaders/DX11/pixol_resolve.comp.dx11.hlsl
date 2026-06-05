// =============================================================================
// pixol_resolve.comp.dx11.hlsl — NKRenderer (Tools/PixolSculpt/shaders/DX11/)
// Variante DirectX 11 (HLSL SM5, override optionnel). ⚠️ SQUELETTE.
// =============================================================================

RWTexture2D<float>  uPixolDepth          : register(u0);
RWTexture2D<float4> uPixolNormal         : register(u1);
RWTexture2D<float4> uPixolColor          : register(u2);
RWTexture2D<float4> uGBufAlbedoMetallic  : register(u3);
RWTexture2D<float4> uGBufNormalRoughness : register(u4);

[numthreads(16, 16, 1)]
void main(uint3 gid : SV_DispatchThreadID) {
    int2 p = int2(gid.xy);
    uint w_, h_;
    uPixolDepth.GetDimensions(w_, h_);
    if (p.x >= (int)w_ || p.y >= (int)h_) return;

    float4 col = uPixolColor[p];
    float4 nrm = uPixolNormal[p];

    uGBufAlbedoMetallic[p]  = float4(col.rgb, 0.0);
    uGBufNormalRoughness[p] = float4(normalize(nrm.xyz + 1e-5), 0.5);
}
