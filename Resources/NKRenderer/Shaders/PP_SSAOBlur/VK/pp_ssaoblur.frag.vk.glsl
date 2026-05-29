// ============================================================
// pp_ssaoblur.frag.vk.glsl — NKRenderer v5.0 — Gaussian blur SSAO
//
// Denoise pour le noise random du GTAO (rotation per pixel). Gaussian
// 5x5 simple sans edge-stopping (v0). Pour la v1, ajouter du cross-
// bilateral avec depth pour preserver les silhouettes (necessitera un
// 2eme binding sampler2D uDepth).
//
// API :
//   - sampler2D uAO au set=0, binding=0
//   - push constant 16 bytes : (invResW, invResH, yFlipUV, _pad)
// ============================================================
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uAO;

layout(push_constant) uniform PC {
    vec2  invResolution;
    float yFlipUV;
    float _pad;
} pc;

void main() {
    // Gaussian 5x5 separable (single pass, mais pas vraiment separable —
    // c'est une approx 2D directe). Sigma ~1.5.
    float aoSum = 0.0;
    float wSum  = 0.0;

    const int kRadius = 2;
    const float sigma = 1.5;
    const float twoSigSq = 2.0 * sigma * sigma;

    for (int y = -kRadius; y <= kRadius; y++) {
        for (int x = -kRadius; x <= kRadius; x++) {
            vec2 offset = vec2(float(x), float(y)) * pc.invResolution;
            float w = exp(-(float(x*x + y*y)) / twoSigSq);
            float sampleAO = texture(uAO, vUV + offset).r;
            aoSum += sampleAO * w;
            wSum  += w;
        }
    }

    oColor = vec4(aoSum / wSum, 0.0, 0.0, 1.0);
}
