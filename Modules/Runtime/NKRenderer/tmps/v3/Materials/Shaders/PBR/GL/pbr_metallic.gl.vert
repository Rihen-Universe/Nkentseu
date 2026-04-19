// =============================================================================
// pbr_metallic.gl.vert — OpenGL GLSL
// DIFFÉRENCES vs Vulkan GLSL :
//   1. Pas de layout(set=N,...) — uniquement layout(binding=N) pour GL 4.1+
//   2. Pas de layout(push_constant) — remplacé par un uniform block classique
//   3. Samplers : uniform sampler2D tAlbedo; (pas de set)
//   4. #version 460 core (avec "core")
//   5. gl_Position : Y est vers le haut (convention OpenGL standard)
//      → pas besoin de flip Y contrairement à Vulkan
//   6. NDC Z : [-1,1] (OpenGL) vs [0,1] (Vulkan/DX/Metal)
//      → la matrice de projection doit être adaptée
//   7. Depuis OpenGL 4.2 : layout(binding=N) sur les samplers
//   8. UBO : layout(binding=N, std140) uniform MyBlock { ... };
//      → pas de set, juste binding
// =============================================================================
#version 460 core

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;

// ── Frame UBO — binding sans set (OpenGL n'a pas de descriptor sets) ─────────
layout(binding=0, std140) uniform NkFrameData {
    mat4  nk_View, nk_Projection, nk_ViewProjection;
    mat4  nk_InvViewProjection, nk_PrevViewProjection;
    vec4  nk_CameraPosition, nk_CameraDirection, nk_ViewportSize, nk_Time;
    vec4  nk_SunDirection, nk_SunColor;
    mat4  nk_ShadowMatrix[4]; vec4 nk_CascadeSplits;
    float nk_EnvIntensity, nk_EnvRotation;
    uint  nk_LightCount, nk_FrameIndex;
};

// ── Per-Object UBO — pas de push_constant en OpenGL, on utilise un UBO ────────
// (mis à jour avec glBufferSubData ou glUniformBlockBinding avant chaque draw)
layout(binding=1, std140) uniform ObjectData {
    mat4  modelMatrix;
    vec4  customColor;
    uint  materialId;
    uint  _pad[3];
};

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUV0;
layout(location=4) out vec2 vUV1;
layout(location=5) out vec3 vViewDir;
layout(location=6) out vec4 vShadowCoord[4];

void main() {
    vec4 worldPos4 = modelMatrix * vec4(aPosition, 1.0);
    vWorldPos  = worldPos4.xyz;
    vUV0 = aUV0; vUV1 = aUV1;
    vViewDir   = normalize(nk_CameraPosition.xyz - vWorldPos);
    mat3 NM    = transpose(inverse(mat3(modelMatrix)));
    vNormal    = normalize(NM * aNormal);
    vTangent   = vec4(normalize(NM * aTangent.xyz), aTangent.w);
    for(int i=0;i<4;i++) vShadowCoord[i] = nk_ShadowMatrix[i]*worldPos4;
    gl_Position = nk_ViewProjection * worldPos4;
    // OpenGL : gl_Position.y vers le HAUT — pas de flip nécessaire
    // (contrairement à Vulkan où Y NDC pointe vers le bas)
}
