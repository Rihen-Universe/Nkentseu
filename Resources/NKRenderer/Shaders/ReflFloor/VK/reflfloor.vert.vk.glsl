// reflfloor.vert.vk.glsl — NKRenderer v4.0 — Reflective Floor Vertex (Vulkan)
// Interface identique au Toon vertex : set=0 Camera, set=1 Object, 4 outputs.
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
    float iblStrength;
    float _pad0, _pad1, _pad2;   // std140 padding pour aligner mirrorViewProj sur 16 bytes
    mat4  mirrorViewProj;        // doit matcher la déclaration du fragment shader
} uCam;

layout(std140, set=1, binding=1) uniform ObjectUBO {
    mat4  model, normalMatrix; vec4 tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV;
layout(location=3) out vec4 vColor;

void main() {
    vec4 worldPos = uObj.model * vec4(aPos, 1.0);
    vWorldPos     = worldPos.xyz;
    vNormal       = normalize(mat3(uObj.normalMatrix) * aNormal);
    vUV           = aUV;
    vColor        = aColor * uObj.tint;
    gl_Position   = uCam.viewProj * worldPos;
}
