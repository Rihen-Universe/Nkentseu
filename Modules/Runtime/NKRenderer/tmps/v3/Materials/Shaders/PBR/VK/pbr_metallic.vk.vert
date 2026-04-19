// =============================================================================
// pbr_metallic.vk.vert  — Vertex PBR pour VULKAN GLSL
// DIFFÉRENCES vs OpenGL :
//   - layout(set=N, binding=M) obligatoire partout
//   - layout(push_constant) pour les données par-objet
//   - #version 460 (sans "core")
//   - gl_Position.y : Vulkan NDC Y pointe vers le bas
//     → soit on flip le viewport (VK_KHR_maintenance1), soit on flip Y ici
// =============================================================================
#version 460

// Inputs vertex (identiques GL/VK)
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;

// ── Frame UBO (set=0, binding=0) — obligatoire en VK, optionnel en GL ─────────
layout(set=0, binding=0, std140) uniform NkFrameData {
    mat4  nk_View;
    mat4  nk_Projection;
    mat4  nk_ViewProjection;
    mat4  nk_InvViewProjection;
    mat4  nk_PrevViewProjection;
    vec4  nk_CameraPosition;
    vec4  nk_CameraDirection;
    vec4  nk_ViewportSize;
    vec4  nk_Time;
    vec4  nk_SunDirection;
    vec4  nk_SunColor;
    mat4  nk_ShadowMatrix[4];
    vec4  nk_CascadeSplits;
    float nk_EnvIntensity;
    float nk_EnvRotation;
    uint  nk_LightCount;
    uint  nk_FrameIndex;
};

// ── Push Constants (set=N n'existe pas pour PC) — données per-objet rapides ──
layout(push_constant) uniform ObjectPC {
    mat4  modelMatrix;
    vec4  customColor;
    uint  materialId;
    uint  _pad[3];
} pc;

// Outputs
layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUV0;
layout(location=4) out vec2 vUV1;
layout(location=5) out vec3 vViewDir;
layout(location=6) out vec4 vShadowCoord[4];
layout(location=10) out vec4 vPrevClip;

void main() {
    vec4 worldPos4  = pc.modelMatrix * vec4(aPosition, 1.0);
    vWorldPos       = worldPos4.xyz;
    vUV0 = aUV0; vUV1 = aUV1;
    vViewDir        = normalize(nk_CameraPosition.xyz - vWorldPos);
    mat3 NM         = transpose(inverse(mat3(pc.modelMatrix)));
    vNormal         = normalize(NM * aNormal);
    vTangent        = vec4(normalize(NM * aTangent.xyz), aTangent.w);
    for(int i=0;i<4;i++) vShadowCoord[i] = nk_ShadowMatrix[i] * worldPos4;
    vPrevClip       = nk_PrevViewProjection * worldPos4;
    gl_Position     = nk_ViewProjection * worldPos4;

    // ── DIFFÉRENCE VULKAN : Y est inversé en NDC Vulkan ───────────────────────
    // Option A : flip via viewport (préféré — utiliser VkViewport.height négatif)
    // Option B : flip ici si le viewport flip n'est pas configuré :
    // gl_Position.y = -gl_Position.y;
    // Note: NkRenderer configure le viewport flip dans NkViewport.flipY=true
    // donc on NE flip PAS ici.
}
