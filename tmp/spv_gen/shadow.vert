#version 450
layout(location = 0) in vec3 aPos;

layout(std140, set = 0, binding = 0) uniform ShadowUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
