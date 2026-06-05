#version 430
// =============================================================================
// sculpt_brush.comp.gl.glsl — NKRenderer (Tools/PixolSculpt/shaders/GL/)
//
// Variante OpenGL (override optionnel). Differences vs la source VK :
//   - pas de "set =" dans les layouts (GL n'a que "binding").
//   - pas de push_constant : mappe sur un uniform block (UBO) cote RHI GL.
//   - GL 4.3+ requis (compute). Le device gate deja la version.
//
// ⚠️ SQUELETTE.
// =============================================================================

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r32f)    uniform image2D uPixolDepth;
layout(binding = 1, rgba16f) uniform image2D uPixolNormal;
layout(binding = 2, rgba8)   uniform image2D uPixolColor;

// GL n'a pas de push_constant -> UBO std140 (le backend RHI route PushConstants
// vers ce bloc lors de la traduction).
layout(binding = 0, std140) uniform PushConsts {
    vec2  center;
    float radius;
    float strength;
    vec4  color;
    uint  mode;
    uint  falloff;
    float hardness;
    float depthBias;
    int   tileOffsetX;
    int   tileOffsetY;
    uint  _pad0;
    uint  _pad1;
} pc;

const uint MODE_RAISE = 0u;
const uint MODE_LOWER = 1u;
const uint MODE_PAINT = 7u;

float Falloff(float t, uint kind, float hardness) {
    t = clamp(t, 0.0, 1.0);
    if (kind == 2u) return 1.0;
    if (kind == 1u) return 1.0 - t;
    if (kind == 3u) return pow(1.0 - t, 3.0);
    if (kind == 4u) return sqrt(max(0.0, 1.0 - t*t));
    float e = mix(1.0, 4.0, hardness);
    return pow(1.0 - smoothstep(0.0, 1.0, t), e);
}

void main() {
    ivec2 p = ivec2(pc.tileOffsetX, pc.tileOffsetY) + ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uPixolDepth);
    if (p.x < 0 || p.y < 0 || p.x >= size.x || p.y >= size.y) return;

    float dist = distance(vec2(p) + 0.5, pc.center);
    if (dist > pc.radius) return;
    float w = Falloff(dist / pc.radius, pc.falloff, pc.hardness) * pc.strength;

    if (pc.mode == MODE_RAISE || pc.mode == MODE_LOWER) {
        float z = imageLoad(uPixolDepth, p).r;
        float dir = (pc.mode == MODE_RAISE) ? -1.0 : 1.0;
        z += dir * w + pc.depthBias;
        imageStore(uPixolDepth, p, vec4(z, 0, 0, 0));
    } else if (pc.mode == MODE_PAINT) {
        vec4 c = imageLoad(uPixolColor, p);
        imageStore(uPixolColor, p, mix(c, pc.color, w));
    }
    // TODO: autres modes.
}
