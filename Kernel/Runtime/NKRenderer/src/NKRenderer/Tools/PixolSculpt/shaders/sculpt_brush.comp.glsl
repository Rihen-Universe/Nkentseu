#version 450
// =============================================================================
// sculpt_brush.comp.glsl — NKRenderer (Tools/PixolSculpt/)
//
// Kernel de brosse : mute le canvas pixol (depth/normal/color) dans la tuile
// dispatchee. Borne par le dirty rect cote CPU -> cout constant en resolution.
//
// ⚠️ SQUELETTE. NkSL non fonctionnel => on ecrit en GLSL, compile en SPIR-V
//    via glslang (NkShaderConverter::GlslToSpirv).
// ⚠️ Le push_constant DOIT matcher renderer::NkSculptBrushGPU (NkSculptBrush.h).
// =============================================================================

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Storage images = cibles pixol (NK_UNORDERED_ACCESS cote RHI).
layout(set = 0, binding = 0, r32f)    uniform image2D uPixolDepth;
layout(set = 0, binding = 1, rgba16f) uniform image2D uPixolNormal;
layout(set = 0, binding = 2, rgba8)   uniform image2D uPixolColor;

layout(push_constant) uniform PushConsts {
    vec2  center;      // pixels ecran
    float radius;
    float strength;
    vec4  color;
    uint  mode;        // NkSculptBrushMode
    uint  falloff;     // NkSculptFalloff
    float hardness;
    float depthBias;
    int   tileOffsetX; // origine de la tuile dispatchee
    int   tileOffsetY;
    uint  _pad0;
    uint  _pad1;
} pc;

// Modes (cf. NkSculptBrushMode)
const uint MODE_RAISE   = 0u;
const uint MODE_LOWER   = 1u;
const uint MODE_SMOOTH  = 2u;
const uint MODE_PINCH   = 3u;
const uint MODE_INFLATE = 4u;
const uint MODE_FLATTEN = 5u;
const uint MODE_MASK    = 6u;
const uint MODE_PAINT   = 7u;

float Falloff(float t, uint kind, float hardness) {
    // t in [0..1], 0 au centre, 1 au bord.
    t = clamp(t, 0.0, 1.0);
    if (kind == 2u) return 1.0;                  // CONSTANT
    if (kind == 1u) return 1.0 - t;              // LINEAR
    if (kind == 3u) return pow(1.0 - t, 3.0);    // SHARP
    if (kind == 4u) return sqrt(max(0.0, 1.0 - t*t)); // SPHERE
    float e = mix(1.0, 4.0, hardness);           // SMOOTH (defaut)
    float s = 1.0 - smoothstep(0.0, 1.0, t);
    return pow(s, e);
}

void main() {
    // Coordonnee pixel absolue = origine de la tuile + thread local.
    ivec2 p = ivec2(pc.tileOffsetX, pc.tileOffsetY) + ivec2(gl_GlobalInvocationID.xy);

    ivec2 size = imageSize(uPixolDepth);
    if (p.x < 0 || p.y < 0 || p.x >= size.x || p.y >= size.y) return;

    float dist = distance(vec2(p) + 0.5, pc.center);
    if (dist > pc.radius) return;

    float w = Falloff(dist / pc.radius, pc.falloff, pc.hardness) * pc.strength;

    // TODO(PixolSculpt): implementer chaque mode. Exemple RAISE/LOWER :
    if (pc.mode == MODE_RAISE || pc.mode == MODE_LOWER) {
        float z = imageLoad(uPixolDepth, p).r;
        float dir = (pc.mode == MODE_RAISE) ? -1.0 : 1.0; // -Z vers la camera
        z += dir * w + pc.depthBias;
        imageStore(uPixolDepth, p, vec4(z, 0, 0, 0));
        // TODO: recalculer/accumuler la normale a partir du voisinage de profondeur.
    }
    else if (pc.mode == MODE_PAINT) {
        vec4 c = imageLoad(uPixolColor, p);
        imageStore(uPixolColor, p, mix(c, pc.color, w));
    }
    // TODO: SMOOTH (moyenne du voisinage), PINCH, INFLATE (le long normale),
    //       FLATTEN (vers plan moyen), MASK (canal masque dedie).
}


// aussi tu pense comment je peux aussi integrer un system comme ca pour les voxel dans nkrenderer? mais avant je veux que tu fasse une analyse complete et approfondis de nksl donc du dossier sl dans nkrhi et tu me dis si le system est complet et robuste, dis moi ce qui manque ce quon peut complete et ajouter etc.
