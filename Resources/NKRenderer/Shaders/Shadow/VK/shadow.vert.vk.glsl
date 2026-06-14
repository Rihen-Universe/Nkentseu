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
    vec4 clip = pc.lightVP * uObj.model * vec4(aPos, 1.0);
    // Correction Vulkan clip-space Z : la matrice lightVP produit du NDC Z
    // [-1,1] (convention GL/orthogonal NkMat4f). Vulkan attend [0,1] sinon
    // la moitie near de chaque cascade est CLIPPEE -> atlas a moitie vide
    // -> le sample PBR retourne 1.0 (pas d'ombre) sur la moitie des pixels.
    // Apply z' = 0.5*z + 0.5*w pour mapper [-1,1] -> [0,1].
    clip.z = 0.5 * clip.z + 0.5 * clip.w;
    gl_Position = clip;
}
