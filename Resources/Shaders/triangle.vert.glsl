#version 450 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;

layout(location = 0) out vec3 vColor;

void main() {
    gl_PointSize = 1.0;
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}