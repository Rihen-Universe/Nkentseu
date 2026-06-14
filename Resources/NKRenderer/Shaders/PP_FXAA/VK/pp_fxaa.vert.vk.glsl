// pp_fxaa.vert.vk.glsl — FXAA VS (fullscreen triangle + flip Y conditionnel)
// Phase L finition 2026-05-23. Pattern identique aux autres PP_X shaders.
#version 460 core

layout(location=0) out vec2 vUV;

layout(push_constant) uniform PC {
    vec2  invResolution;  // 1/W, 1/H
    float yFlipUV;        // +1 OpenGL / -1 Vulkan (cf. autres PP shaders)
    float _pad;
} pc;

void main() {
    // Fullscreen triangle pattern (gl_VertexIndex 0/1/2 sans VBO).
    vec2 pos = vec2((gl_VertexIndex == 1) ? 3.0 : -1.0,
                    (gl_VertexIndex == 2) ? 3.0 : -1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
    vec2 uvBase = pos * 0.5 + 0.5;
    vUV = (pc.yFlipUV < 0.0) ? vec2(uvBase.x, 1.0 - uvBase.y) : uvBase;
}
