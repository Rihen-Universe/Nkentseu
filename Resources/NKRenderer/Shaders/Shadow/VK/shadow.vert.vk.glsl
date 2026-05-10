// ============================================================
// shadow.vert.vk.glsl — NKRenderer v5.0 — Shadow depth-only VS (Vulkan)
// Sync avec embedded GL fallback dans NkRender3D_PBRShaders.inl (kShadow_VS).
// Difference VK : push_constant block + set=0 sur ObjectUBO.
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;

// set=1 (Object) pour matcher la convention NkRender3D (set=0=global, set=1=object).
// Le pipeline layout Shadow declare donc [globalLayout, objectLayout] meme si seul
// le second est utilise par ce shader.
layout(std140, set=1, binding=1) uniform ObjectUBO {
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

layout(push_constant) uniform PC {
    mat4 lightVP;
} pc;

void main() {
    gl_Position = pc.lightVP * uObj.model * vec4(aPos, 1.0);
}
