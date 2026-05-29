// ============================================================
// pp_ssao.frag.vk.glsl — NKRenderer v5.0 — SSAO v0 stable
//
// Version v0 simple : 16 samples poisson disk fixe (sans random rotation),
// comparaison de depths, fraction occluse atténuée par distance.
//
// Le screen-space AO classique a des limites fondamentales :
//   - Rayon limité (~quelques % UV)
//   - Pas applicable à l'occlusion long-range (objets sous le sol qui
//     reçoivent l'IBL sky)
//   - Approximation de l'occlusion ambiente non-physique
//
// Phase H.6 (TODO future) : voxel-based AO (UE5 Lumen-style) ou Distance
// Field AO pour résoudre l'AO long-range et l'IBL non-occluded.
//
// API :
//   - sampler2D uDepth au set=0, binding=0
//   - push constant 16 bytes : (invResW, invResH, radius, yFlipUV)
//
// @material("PP_SSAO")
// ============================================================
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uDepth;

layout(push_constant) uniform PC {
    vec2  invResolution;
    float radius;
    float yFlipUV;
} pc;

const vec2 kPoisson16[16] = vec2[](
    vec2( 0.527,  0.000), vec2(-0.527,  0.000), vec2( 0.000,  0.527), vec2( 0.000, -0.527),
    vec2( 0.373,  0.373), vec2(-0.373,  0.373), vec2( 0.373, -0.373), vec2(-0.373, -0.373),
    vec2( 0.838,  0.215), vec2(-0.838,  0.215), vec2( 0.838, -0.215), vec2(-0.838, -0.215),
    vec2( 0.215,  0.838), vec2(-0.215,  0.838), vec2( 0.215, -0.838), vec2(-0.215, -0.838)
);

void main() {
    float centerDepth = texture(uDepth, vUV).r;
    if (centerDepth >= 0.9999) { oColor = vec4(1.0); return; }

    const float bias = 0.001;
    const float maxDelta = 0.05;

    float occlusion = 0.0;
    float totalWeight = 0.0;

    for (int i = 0; i < 16; i++) {
        vec2 offset = kPoisson16[i] * pc.radius;
        float sampleDepth = texture(uDepth, vUV + offset).r;
        float deltaZ = centerDepth - sampleDepth;
        if (deltaZ > bias && deltaZ < maxDelta) {
            float weight = 1.0 - smoothstep(0.0, maxDelta, deltaZ);
            occlusion += weight;
        }
        totalWeight += 1.0;
    }

    float aoFactor = 1.0 - (occlusion / totalWeight) * 0.5;
    oColor = vec4(aoFactor, 0.0, 0.0, 1.0);
}
