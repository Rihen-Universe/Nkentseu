#version 450
// =============================================================================
// nkrenderer2d.vert — Vertex shader for NkVulkanRenderer2D
//
// Compile to SPIR-V:
//   glslangValidator -V nkrenderer2d.vert -o nkrenderer2d_vert.spv
//   -- OR --
//   glslc nkrenderer2d.vert -o nkrenderer2d_vert.spv
// =============================================================================

// Push constant block: orthographic projection matrix (64 bytes)
layout(push_constant) uniform PC {
    mat4 proj;
} u_PC;

// Vertex inputs (match NkVertex2D layout)
layout(location = 0) in vec2 a_Pos;    // offset 0,  2×float
layout(location = 1) in vec2 a_UV;     // offset 8,  2×float
layout(location = 2) in vec4 a_Color;  // offset 16, 4×uint8 normalized

// Varyings to fragment shader
layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;

void main() {
    v_UV    = a_UV;
    v_Color = a_Color;
    gl_Position = u_PC.proj * vec4(a_Pos, 0.0, 1.0);
}
