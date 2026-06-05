// =============================================================================
// sculpt_brush.comp.dx11.hlsl — NKRenderer (Tools/PixolSculpt/shaders/DX11/)
//
// Variante DirectX 11 (HLSL SM5, override optionnel). DX11 n'a pas de register
// spaces : le binding des UAV/CBV est gere par le RHI. push_constant -> cbuffer.
//
// ⚠️ SQUELETTE. Le layout du cbuffer doit matcher renderer::NkSculptBrushGPU.
// =============================================================================

RWTexture2D<float>  uPixolDepth  : register(u0);
RWTexture2D<float4> uPixolNormal : register(u1);
RWTexture2D<float4> uPixolColor  : register(u2);

cbuffer PushConsts : register(b0) {
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
        // TODO: recalcul normale.
    } else if (mode == MODE_PAINT) {
        float4 c = uPixolColor[p];
        uPixolColor[p] = lerp(c, color, w);
    }
    // TODO: autres modes.
}
