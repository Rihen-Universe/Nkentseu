// ============================================================
// emissive.vert.gl.glsl — NKRenderer v4.0 — Emissive Vertex (OpenGL 4.6)
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos;   // xyz=pos, w=near
    vec4  camDir;   // xyz=dir, w=far
    vec2  viewport;
    float time;
    float deltaTime;
} uCam;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model;
    mat4  normalMatrix;
    vec4  tint;
    float metallic;
    float roughness;
    float aoStrength;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRoughness;
    float subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(std140, binding=3) uniform ShadowUBO {
    mat4  cascadeMats[4];
    float cascadeSplits[4];
    int   cascadeCount;
    float shadowBias;
    float normalBias;
    int   softShadows;
} uShadow;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;
layout(location=7) out vec4 vShadowCoord[4]; // up to 4 cascades

void main() {
    vec4 worldPos  = uObj.model * vec4(aPos, 1.0);
    vWorldPos      = worldPos.xyz;

    mat3 nm        = mat3(uObj.normalMatrix);
    vNormal        = normalize(nm * aNormal);
    vTangent       = normalize(nm * aTangent);
    vBitangent     = cross(vNormal, vTangent);

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    // Cascade shadow coords
    for (int c = 0; c < 4; c++)
        vShadowCoord[c] = uShadow.cascadeMats[c] * worldPos;

    gl_Position = uCam.viewProj * worldPos;
}
