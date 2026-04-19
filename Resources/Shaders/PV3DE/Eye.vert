#version 330 core
// =============================================================================
// PV3DE/Shaders/Eye.vert
// =============================================================================

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_normalMatrix;
uniform vec3 u_cameraPos;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_viewDir;

void main() {
    vec4 world = u_model * vec4(a_position, 1.0);
    v_worldPos = world.xyz;
    v_normal   = normalize(mat3(u_normalMatrix) * a_normal);
    v_uv       = a_uv;
    v_viewDir  = normalize(u_cameraPos - world.xyz);
    gl_Position = u_projection * u_view * world;
}
