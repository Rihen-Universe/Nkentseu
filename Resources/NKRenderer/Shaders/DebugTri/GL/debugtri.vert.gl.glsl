// DEBUG triangle minimal OpenGL — equivalent VK pour reference.
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=0) out vec3 vColor;

void main() {
    gl_Position = vec4(aPos.xy, 0.5, 1.0);
    vColor = vec3(aPos.x * 0.5 + 0.5, aPos.y * 0.5 + 0.5, 0.0);
}
