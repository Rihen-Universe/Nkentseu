// pp_ssao.vert.vk.glsl — SSAO Vertex (fullscreen triangle, flip Y conditionnel).
#version 460 core

layout(location=0) out vec2 vUV;

layout(push_constant) uniform PC {
    vec2  invResolution;
    float radius;
    float yFlipUV;
} pc;

void main() {
    vec2 pos = vec2((gl_VertexIndex == 1) ? 3.0 : -1.0,
                    (gl_VertexIndex == 2) ? 3.0 : -1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
    vec2 uvBase = pos * 0.5 + 0.5;
    vUV = (pc.yFlipUV < 0.0) ? vec2(uvBase.x, 1.0 - uvBase.y) : uvBase;
}
