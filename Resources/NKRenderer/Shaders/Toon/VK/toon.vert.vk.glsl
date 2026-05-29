// toon.vert.vk.glsl — NKRenderer v4.0 — Toon/Cel-shading Vertex (Vulkan)
// Convention : GLSL Vulkan-style, source canonique cross-API.
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
    float _p0, _p1, _p2;
    mat4  mirrorViewProj;
    vec4  reflectionFlags;  // .x = isMirrorPass
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
    // Phase Planar Reflection fix 2026-05-24 : un-mirror Y sur worldPos/N en
    // mirror pass pour que lighting/shadow soient calcules en espace real.
    // Cf. pbr.vert.vk.glsl pour le détail.
    vec4 wp_mirror = uObj.model * vec4(aPos, 1.0);
    vec4 wp_real   = wp_mirror;
    vec3 N_real    = normalize(mat3(uObj.normalMatrix) * aNormal);
    if (uCam.reflectionFlags.x > 0.5) {
        wp_real.y = -wp_real.y;
        N_real.y  = -N_real.y;
    }
    vWorldPos     = wp_real.xyz;
    vNormal       = N_real;
    vUV           = aUV;
    vColor        = aColor * uObj.tint;
    gl_Position   = uCam.viewProj * wp_mirror;
}
