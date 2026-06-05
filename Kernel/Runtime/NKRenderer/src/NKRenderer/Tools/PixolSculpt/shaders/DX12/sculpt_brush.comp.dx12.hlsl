// =============================================================================
// sculpt_brush.comp.dx12.hlsl — NKRenderer (Tools/PixolSculpt/shaders/DX12/)
//
// Variante DirectX 12 (HLSL SM 5.1+, override optionnel). DX12 utilise les
// register SPACES pour mapper les descriptor sets Vulkan (space0 == set 0).
// push_constant -> root constants (cbuffer b0, space0).
//
// ⚠️ SQUELETTE. (NB: DX12 = HLSL, pas GLSL.) Layout = renderer::NkSculptBrushGPU.
// =============================================================================

RWTexture2D<float>  uPixolDepth  : register(u0, space0);
RWTexture2D<float4> uPixolNormal : register(u1, space0);
RWTexture2D<float4> uPixolColor  : register(u2, space0);

cbuffer PushConsts : register(b0, space0) {
    float2 center;
    float  radius;
    float  strength;
    float4 color;
    uint   mode;
    uint   falloff;
    float  hardness;
    float  depthBias;
    int    tileOffsetX;
    int    tileOffsetY;
    uint   _pad0;
    uint   _pad1;
};

static const uint MODE_RAISE = 0u;
static const uint MODE_LOWER = 1u;
static const uint MODE_PAINT = 7u;

float Falloff(float t, uint kind, float hardness) {
    t = saturate(t);
    if (kind == 2u) return 1.0;
    if (kind == 1u) return 1.0 - t;
    if (kind == 3u) return pow(1.0 - t, 3.0);
    if (kind == 4u) return sqrt(max(0.0, 1.0 - t*t));
    float e = lerp(1.0, 4.0, hardness);
    return pow(1.0 - smoothstep(0.0, 1.0, t), e);
}

[numthreads(16, 16, 1)]
void main(uint3 gid : SV_DispatchThreadID) {
    int2 p = int2(tileOffsetX, tileOffsetY) + int2(gid.xy);

    uint w_, h_;
    uPixolDepth.GetDimensions(w_, h_);
    if (p.x < 0 || p.y < 0 || p.x >= (int)w_ || p.y >= (int)h_) return;

    float dist = distance(float2(p) + 0.5, center);
    if (dist > radius) return;
    float w = Falloff(dist / radius, falloff, hardness) * strength;

    if (mode == MODE_RAISE || mode == MODE_LOWER) {
        float z = uPixolDepth[p];
        float dir = (mode == MODE_RAISE) ? -1.0 : 1.0;
        z += dir * w + depthBias;
        uPixolDepth[p] = z;
    } else if (mode == MODE_PAINT) {
        float4 c = uPixolColor[p];
        uPixolColor[p] = lerp(c, color, w);
    }
    // TODO: autres modes.
}
