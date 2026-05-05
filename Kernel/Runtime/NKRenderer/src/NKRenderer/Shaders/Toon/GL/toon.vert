// toon.vert — NKRenderer v4.0 — OpenGL GLSL 4.60 core
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=5) in vec4 aColor;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV;
layout(location=3) out vec4 vColor;

layout(binding=0,std140) uniform CameraUBO {
    mat4 view, proj, viewProj, invViewProj;
    vec4 camPos; vec2 viewport; float time; float dt;
};
layout(binding=1,std140) uniform ObjectUBO {
    mat4 model, normalMatrix; vec4 tint;
    float metallic, roughness, ao, emissStr, normStr; float _p[3];
};

void main() {
    vec4 wp4      = model * vec4(aPos, 1.0);
    vWorldPos     = wp4.xyz;
    vNormal       = normalize(mat3(normalMatrix) * aNormal);
    vUV           = aUV;
    vColor        = aColor * tint;
    gl_Position   = viewProj * wp4;
}
