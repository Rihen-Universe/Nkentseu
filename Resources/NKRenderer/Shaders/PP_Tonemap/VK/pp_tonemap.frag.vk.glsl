// ============================================================
// pp_tonemap.frag.vk.glsl — NKRenderer v5.0 — PostProcess Tonemap FS (Vulkan)
// Sync avec embedded GL fallback dans NkPostProcessStack.cpp (kTonemapFS_GL).
// Difference VK : push_constant block au lieu d'uniform vec4 _PushConstants[].
// ============================================================
#version 460 core

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uHDR;

layout(push_constant) uniform PC {
    float exposure;
    float gamma;
    float _pad0;
    float _pad1;
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
    vec3 hdr    = texture(uHDR, vUV).rgb * pc.exposure;
    vec3 mapped = ACESFilm(hdr);
    mapped      = pow(mapped, vec3(1.0 / pc.gamma));
    oColor      = vec4(mapped, 1.0);
}
