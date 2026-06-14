// ============================================================
// pp_bloomup.frag.vk.glsl — NKRenderer v5.0 — Bloom Upsample FS (Vulkan)
//
// 3x3 tent filter (Jorge Jimenez 2014 — COD: Advanced Warfare).
// Sample la mip plus petite avec un tent filter 9-tap (gaussian approx)
// pour diffuser doucement vers la mip plus grande.
//
// Le rendu est en blend ADDITIVE : la mip cible accumule la contribution
// de la mip courante par-dessus son contenu existant (resultat du
// downsample). Ca cree la pyramide upsample qui donne le halo radial doux.
//
// Pattern :
//     1 2 1
//     2 4 2     /16  (total = 16)
//     1 2 1
//
// API :
//   - sampler2D uSrc au set=0,binding=0 (mip source, plus petite)
//   - push constant 12 bytes : (srcInvW, srcInvH, strength)
//   - blend additive 1:1 (configure cote pipeline state)
//
// @material("PP_BloomUp")
// ============================================================
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uSrc;

layout(push_constant) uniform PC {
    vec2  srcInvSize;   // 1.0 / (srcWidth, srcHeight)
    float strength;     // contribution multiplicateur (typique 1.0)
    float yFlipUV;      // partage avec VS, ignore ici
} pc;

void main() {
    vec2 t = pc.srcInvSize;

    // Tent filter 3x3 — poids gaussian approx.
    vec3 col =
        texture(uSrc, vUV + t * vec2(-1.0,  1.0)).rgb * 1.0 +
        texture(uSrc, vUV + t * vec2( 0.0,  1.0)).rgb * 2.0 +
        texture(uSrc, vUV + t * vec2( 1.0,  1.0)).rgb * 1.0 +
        texture(uSrc, vUV + t * vec2(-1.0,  0.0)).rgb * 2.0 +
        texture(uSrc, vUV).rgb                          * 4.0 +
        texture(uSrc, vUV + t * vec2( 1.0,  0.0)).rgb * 2.0 +
        texture(uSrc, vUV + t * vec2(-1.0, -1.0)).rgb * 1.0 +
        texture(uSrc, vUV + t * vec2( 0.0, -1.0)).rgb * 2.0 +
        texture(uSrc, vUV + t * vec2( 1.0, -1.0)).rgb * 1.0;
    col *= 1.0 / 16.0;

    oColor = vec4(col * pc.strength, 1.0);
}
