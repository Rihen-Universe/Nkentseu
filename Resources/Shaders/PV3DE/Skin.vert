#version 330 core
// =============================================================================
// PV3DE/Shaders/Skin.vert
// Vertex shader pour le rendu de peau humaine.
// Applique les blendshapes FACS + skinning squelettique (optionnel).
//
// Inputs :
//   a_position   — position de base
//   a_normal     — normale de base
//   a_tangent    — tangente (w = signe bitangente)
//   a_uv         — coordonnées UV
//   a_boneIds    — indices des joints (optionnel si u_hasSkeleton)
//   a_weights    — poids des joints
//
// Blendshapes :
//   Binding 3 — UBO u_BS contenant les 64 poids flottants
//   Les deltas sont encodés dans des textures flottantes :
//     u_bsDeltaPos[i]  — atlas des deltas position
//     u_bsDeltaNorm[i] — atlas des deltas normales
//   (simplifié : en production les deltas sont dans des SSBOs dédiés)
//
// Outputs → Skin.frag :
//   v_worldPos   — position monde (pour SSS et IBL)
//   v_normal     — normale monde (avec blendshape)
//   v_tangent    — tangente monde
//   v_bitangent  — bitangente monde
//   v_uv         — UV
//   v_viewDir    — direction de vue (monde)
// =============================================================================

// ── Inputs vertex ─────────────────────────────────────────────────────────────
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;   // xyz=tangent, w=sign
layout(location = 3) in vec2 a_uv;
layout(location = 4) in ivec4 a_boneIds;
layout(location = 5) in vec4  a_weights;

// ── Uniforms matrices ─────────────────────────────────────────────────────────
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_normalMatrix;   // transpose(inverse(u_model))
uniform vec3 u_cameraPos;

// ── Blendshapes ───────────────────────────────────────────────────────────────
// Binding 3 : poids des 64 blendshapes (UBO)
layout(std140, binding = 3) uniform BS_WEIGHTS_BUFFER {
    float u_bsWeights[64];
    int   u_bsCount;
};

// Deltas blendshapes encodés dans des textures float RGB (atlas 8x8 blendshapes max)
// Format : texture2DArray — layer = index blendshape, uv = index vertex
// (simplifié pour Phase 5 : on passe directement les deltas via SSBO)
// TODO Phase 6 : texture atlas BS_DELTA_POS, BS_DELTA_NORM

// ── Skinning squelettique ─────────────────────────────────────────────────────
uniform bool u_hasSkeleton;
layout(std140, binding = 4) uniform BONE_MATRIX_BUFFER {
    mat4 u_boneMatrices[128];
};

// ── Outputs → fragment ────────────────────────────────────────────────────────
out vec3 v_worldPos;
out vec3 v_normal;
out vec4 v_tangent;
out vec2 v_uv;
out vec3 v_viewDir;

// ── Fonctions helper ──────────────────────────────────────────────────────────
mat4 SkinMatrix() {
    if (!u_hasSkeleton) return mat4(1.0);
    mat4 m = u_boneMatrices[a_boneIds.x] * a_weights.x
           + u_boneMatrices[a_boneIds.y] * a_weights.y
           + u_boneMatrices[a_boneIds.z] * a_weights.z
           + u_boneMatrices[a_boneIds.w] * a_weights.w;
    return m;
}

void main() {
    // ── 1. Blendshapes ────────────────────────────────────────────────────────
    // En Phase 5 : les deltas sont dans un SSBO indexé par gl_VertexID
    // Simplifié ici : pas de delta texture (poids appliqués via SSBO Phase 6)
    vec3 blendPos  = a_position;
    vec3 blendNorm = a_normal;

    // ── 2. Skinning ───────────────────────────────────────────────────────────
    mat4 skinMat = SkinMatrix();
    vec4 skinnedPos  = skinMat * vec4(blendPos, 1.0);
    vec3 skinnedNorm = mat3(skinMat) * blendNorm;

    // ── 3. Espace monde ───────────────────────────────────────────────────────
    vec4 worldPos4 = u_model * skinnedPos;
    v_worldPos     = worldPos4.xyz;

    // Normale monde
    v_normal  = normalize(mat3(u_normalMatrix) * skinnedNorm);

    // Tangente monde (avec reconstruction bitangente)
    vec3 worldTangent = normalize(mat3(u_model) * (mat3(skinMat) * a_tangent.xyz));
    v_tangent = vec4(worldTangent, a_tangent.w);

    v_uv      = a_uv;
    v_viewDir = normalize(u_cameraPos - v_worldPos);

    gl_Position = u_projection * u_view * worldPos4;
}
