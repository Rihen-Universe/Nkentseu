// ============================================================
// pp_tonemap.vert.vk.glsl — NKRenderer v5.0 — PostProcess Tonemap VS (Vulkan)
// Sync avec embedded GL fallback dans NkPostProcessStack.cpp (kFullscreenVS_GL).
// Identique au GL : pas d'uniform a migrer.
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV;
layout(location=0) out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
