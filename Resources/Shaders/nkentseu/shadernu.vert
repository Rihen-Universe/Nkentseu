#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 normal;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(a_Position, 1.0);
    fragColor = a_Normal;
    texCoord = a_TexCoord;
    normal = a_Normal;
}