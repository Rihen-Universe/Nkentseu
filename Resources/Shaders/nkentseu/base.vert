#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 normal;

layout(binding = 0) uniform CameraProperty{
    mat4 view;
    mat4 proj;
} camera;

layout(binding = 1) readonly buffer MaterialProperty { 
    vec4 baseColor; 
} material; // nous affecterons cette variable dans le code OpenGL.

layout(binding = 2) readonly buffer ObjectProperty {
    mat4 model;
} object; // nous affecterons cette variable dans le code OpenGL.

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = camera.proj * camera.view * object.model * vec4(a_Position, 1.0);
    fragColor = material.baseColor.xyz;
    texCoord = a_TexCoord;
    normal = a_Normal;
}