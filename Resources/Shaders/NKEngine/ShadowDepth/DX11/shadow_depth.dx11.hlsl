// =============================================================================
// shadow_depth.dx11.hlsl — Passe shadow map DirectX 11
// =============================================================================
cbuffer ShadowCB : register(b0) {
    matrix uLightVP;
    matrix uModel;
};
struct VSIn { float3 pos:POSITION; };
float4 VSMain(VSIn v) : SV_POSITION {
    return mul(mul(float4(v.pos,1),uModel),uLightVP);
}
// Pas de pixel shader — depth only pass
