#version 450
// =============================================================================
// nkrenderer2d.frag — Fragment shader for NkVulkanRenderer2D
//
// Compile to SPIR-V:
//   glslangValidator -V nkrenderer2d.frag -o nkrenderer2d_frag.spv
//   -- OR --
//   glslc nkrenderer2d.frag -o nkrenderer2d_frag.spv
// =============================================================================

// Texture + sampler combined (set=0, binding=0)
layout(set = 0, binding = 0) uniform sampler2D u_Tex;

// Varyings from vertex shader
layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;

// Output
layout(location = 0) out vec4 out_Color;

void main() {
    out_Color = texture(u_Tex, v_UV) * v_Color;
}
