// layeredv1.vert.vk.glsl — M.1 v1 : Material Layering N=8 (Vulkan)
// Pass-through pos/normal/uv/color, comme Layered v0.
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
} uCam;

layout(std140, set=1, binding=1) uniform ObjectUBO {
    mat4  model, normalMatrix;
    vec4  tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV;
layout(location=3) out vec4 vColor;

void main() {
    vec4 wp     = uObj.model * vec4(aPos, 1.0);
    vWorldPos   = wp.xyz;
    vNormal     = normalize(mat3(uObj.normalMatrix) * aNormal);
    vUV         = aUV;
    vColor      = aColor * uObj.tint;
    gl_Position = uCam.viewProj * wp;
}
