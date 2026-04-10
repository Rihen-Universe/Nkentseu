#version 460 core

layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 color;
    color.rgb = pow(vColor.rgb, vec3(2.2));
    outColor = color;
}