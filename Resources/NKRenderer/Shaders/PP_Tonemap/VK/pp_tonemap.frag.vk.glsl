// ============================================================
// pp_tonemap.frag.vk.glsl -- NKRenderer v5.0 -- PostProcess Tonemap FS (Vulkan)
// Sync avec embedded GL fallback dans NkPostProcessStack.cpp (kTonemapFS_GL).
// Difference VK : push_constant block au lieu d'uniform vec4 _PushConstants[].
// PC layout (16 bytes) : exposure | gamma | vignetteIntens | saturation
// ============================================================
#version 460 core

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uHDR;

layout(push_constant) uniform PC {
    float exposure;
    float gamma;
    float vignetteIntens;  // 0 = desactive, >0 = vignette circulaire
    float saturation;      // 1.0 = neutre, <1 desature, >1 sature
} pc;

vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    // Vulkan : pc.gamma=1.0 -> pas de correction (swapchain sRGB s'en charge).
    // OpenGL : pc.gamma=2.2 -> correction manuelle pow(1/gamma).
    vec3 hdr    = texture(uHDR, vUV).rgb * pc.exposure;
    vec3 mapped = ACESFilm(hdr);
    if (pc.gamma > 1.01) mapped = pow(mapped, vec3(1.0 / pc.gamma));

    // Saturation (style film : leger boost ~1.1-1.2)
    if (abs(pc.saturation - 1.0) > 0.01) {
        float lum = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        mapped = mix(vec3(lum), mapped, pc.saturation);
        mapped = clamp(mapped, 0.0, 1.0);
    }

    // Vignette circulaire (style film : ~0.4-0.6)
    if (pc.vignetteIntens > 0.001) {
        vec2 uv   = vUV * 2.0 - 1.0;
        float vig = 1.0 - dot(uv, uv) * pc.vignetteIntens;
        mapped   *= clamp(vig, 0.0, 1.0);
    }

    oColor = vec4(mapped, 1.0);
}
