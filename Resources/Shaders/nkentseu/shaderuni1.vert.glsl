#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 normal;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 color;
} uniformBufferObject;

/*
push constants for material

Marerial{
    vec4 color;
}material;

push constants for material

Transformation{
   mat4 transform;
}transformation;

push constants for light

PointLight{
}pointlight;

SpotLight{
}spotlight;

DirectionalLight{
}directionalLight;

LightType{
    int _spot;
    int _point;
    int _direction;
}
*/

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = uniformBufferObject.proj * uniformBufferObject.view * uniformBufferObject.model * vec4(a_Position, 1.0);
    fragColor = uniformBufferObject.color;
    texCoord = a_TexCoord;
    normal = a_Normal;
}