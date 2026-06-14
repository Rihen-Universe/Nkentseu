// fullscreen_quad.vert — NKRenderer v4.0 — shared vertex for all post-process passes
#version 460 core
layout(location=0) out vec2 vUV;
void main() {
    // NDC fullscreen triangle (no VBO needed)
    vec2 pos[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
    gl_Position  = vec4(pos[gl_VertexID], 0.0, 1.0);
    vUV          = pos[gl_VertexID] * 0.5 + 0.5;
}
