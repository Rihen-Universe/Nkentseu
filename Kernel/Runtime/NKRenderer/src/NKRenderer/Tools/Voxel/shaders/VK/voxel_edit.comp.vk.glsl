#version 450
// =============================================================================
// voxel_edit.comp.vk.glsl — NKRenderer (Tools/Voxel/shaders/VK/)
//
// SOURCE CANONIQUE (Vulkan GLSL). Mute le volume voxel (densite/couleur) dans
// la BOITE dispatchee (bornee a l'AABB de la brosse -> cout proportionnel au
// volume edite). Transpile vers GL/DX11/DX12/MSL par le loader.
//
// ⚠️ SQUELETTE. push_constant DOIT matcher renderer::NkVoxelBrushGPU.
// =============================================================================

layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(set = 0, binding = 0, r16f)  uniform image3D uDensity;
layout(set = 0, binding = 1, rgba8) uniform image3D uColor;

layout(push_constant) uniform PushConsts {
    vec3  center;     // centre en voxels
    float radius;
    vec4  color;
    uint  mode;       // NkVoxelBrushMode
    uint  falloff;    // NkVoxelFalloff
    float strength;
    float _pad0;
    int   boxOffsetX; // origine de la boite dispatchee
    int   boxOffsetY;
    int   boxOffsetZ;
    uint  _pad1;
} pc;

const uint MODE_ADD   = 0u;
const uint MODE_SUB   = 1u;
const uint MODE_PAINT = 2u;

float falloffWeight(float t, uint kind) {
    t = clamp(t, 0.0, 1.0);
    if (kind == 2u) return 1.0;                       // CONSTANT
    if (kind == 1u) return 1.0 - t;                   // LINEAR
    if (kind == 3u) return sqrt(max(0.0, 1.0 - t*t)); // SPHERE
    return 1.0 - smoothstep(0.0, 1.0, t);             // SMOOTH (defaut)
}

void main() {
    ivec3 p    = ivec3(pc.boxOffsetX, pc.boxOffsetY, pc.boxOffsetZ) + ivec3(gl_GlobalInvocationID.xyz);
    ivec3 size = imageSize(uDensity);
    if (any(lessThan(p, ivec3(0))) || any(greaterThanEqual(p, size))) return;

    float dist = distance(vec3(p) + vec3(0.5), pc.center);
    if (dist > pc.radius) return;

    float w = falloffWeight(dist / pc.radius, pc.falloff) * pc.strength;

    if (pc.mode == MODE_ADD || pc.mode == MODE_SUB) {
        float d   = imageLoad(uDensity, p).r;
        float dir = (pc.mode == MODE_ADD) ? 1.0 : -1.0;
        d = clamp(d + dir * w, 0.0, 1.0);
        imageStore(uDensity, p, vec4(d, 0.0, 0.0, 0.0));
        if (pc.mode == MODE_ADD) {
            vec4 c = imageLoad(uColor, p);
            imageStore(uColor, p, mix(c, pc.color, w));
        }
    } else if (pc.mode == MODE_PAINT) {
        vec4 c = imageLoad(uColor, p);
        imageStore(uColor, p, mix(c, pc.color, w));
    }
    // TODO: SMOOTH (moyenne du voisinage 3D), FLATTEN.
}
