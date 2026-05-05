// ============================================================
// pbr.vert.vk.glsl — NKRenderer v4.0 — Hair Vertex (Vulkan)
// Différences VK vs GL :
//   • set=0/set=1 descriptor sets
//   • gl_Position.y = -gl_Position.y (NDC Y-flip)
//   • gl_VertexIndex au lieu de gl_VertexID
//   • push_constant disponible
// ============================================================
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(set=0, binding=0, std140) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir;
    vec2  viewport;
    float time, deltaTime;
} uCam;

layout(set=0, binding=1, std140) uniform ObjectUBO {
    mat4  model, normalMatrix;
    vec4  tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(set=0, binding=3, std140) uniform ShadowUBO {
    mat4  cascadeMats[4];
    float cascadeSplits[4];
    int   cascadeCount;
    float shadowBias, normalBias;
    int   softShadows;
} uShadow;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;
layout(location=7) out vec4 vShadowCoord0;
layout(location=8) out vec4 vShadowCoord1;
layout(location=9) out vec4 vShadowCoord2;
layout(location=10) out vec4 vShadowCoord3;

void main() {
    vec4 worldPos  = uObj.model * vec4(aPos, 1.0);
    vWorldPos      = worldPos.xyz;

    mat3 nm    = mat3(uObj.normalMatrix);
    vNormal    = normalize(nm * aNormal);
    vTangent   = normalize(nm * aTangent);
    vBitangent = cross(vNormal, vTangent);

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    vShadowCoord0 = uShadow.cascadeMats[0] * worldPos;
    vShadowCoord1 = uShadow.cascadeMats[1] * worldPos;
    vShadowCoord2 = uShadow.cascadeMats[2] * worldPos;
    vShadowCoord3 = uShadow.cascadeMats[3] * worldPos;

    gl_Position   = uCam.viewProj * worldPos;
    gl_Position.y = -gl_Position.y;  // Vulkan NDC Y-flip
}
