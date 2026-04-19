// =============================================================================
// shadow_depth.vk.glsl — Passe de profondeur shadow map Vulkan GLSL 4.50
// =============================================================================
// #vert
#version 450
layout(location=0) in vec3 aPos;
layout(push_constant) uniform LightPC { mat4 lightVP; mat4 model; };
void main() {
    gl_Position = lightVP * model * vec4(aPos,1.0);
}
// #frag
#version 450
void main() {}
