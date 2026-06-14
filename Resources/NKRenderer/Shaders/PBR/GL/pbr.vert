// pbr.vert — NKRenderer v4.0 — OpenGL GLSL 4.60 core
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;

layout(binding=0, std140) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invViewProj;
    vec4 camPos;
    vec2 viewportSize;
    float time;
    float deltaTime;
};

layout(binding=1, std140) uniform ObjectUBO {
    mat4 model;
    mat4 normalMatrix;
    vec4 tint;
    float metallic;
    float roughness;
    float aoStrength;
    float emissiveStrength;
};

void main() {
    vec4 worldPos4 = model * vec4(aPos, 1.0);
    vWorldPos  = worldPos4.xyz;
    mat3 nm    = mat3(normalMatrix);
    vNormal    = normalize(nm * aNormal);
    vTangent   = normalize(nm * aTangent);
    vBitangent = cross(vNormal, vTangent);
    vUV = aUV; vUV2 = aUV2; vColor = aColor * tint;
    gl_Position = viewProj * worldPos4;
}
