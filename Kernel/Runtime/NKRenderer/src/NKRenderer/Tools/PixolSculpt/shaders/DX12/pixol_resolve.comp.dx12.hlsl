// =============================================================================
// pixol_resolve.comp.dx12.hlsl — NKRenderer (Tools/PixolSculpt/shaders/DX12/)
// Variante DirectX 12 (HLSL SM 5.1+, override optionnel). register spaces.
// ⚠️ SQUELETTE. (DX12 = HLSL, pas GLSL.)
// =============================================================================

RWTexture2D<float>  uPixolDepth          : register(u0, space0);
RWTexture2D<float4> uPixolNormal         : register(u1, space0);
RWTexture2D<float4> uPixolColor          : register(u2, space0);
RWTexture2D<float4> uGBufAlbedoMetallic  : register(u3, space0);
RWTexture2D<float4> uGBufNormalRoughness : register(u4, space0);

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
