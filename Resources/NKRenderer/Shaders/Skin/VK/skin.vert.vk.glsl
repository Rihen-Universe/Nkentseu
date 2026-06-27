// ============================================================
// skin.vert.vk.glsl — NKRenderer — Skin Vertex (Vulkan, REAL GPU skinning)
//
// Linear blend skinning (LBS) : 4 influences par vertex.
//   aBoneIdx    (vec4)  : indices de bones (stockes en float, voir NkVertexSkinned)
//   aBoneWeight (vec4)  : poids associes (somme normalisee a 1 cote loader)
//   bones[64]   (UBO)   : joint matrices = globalNodeTransform * inverseBind
//
// Convention bindings NKRenderer :
//   set=0 binding=0  CameraUBO  (frame global set)
//   set=1 binding=1  ObjectUBO  (per-draw object set)
//   set=1 binding=2  BonesUBO   (per-draw object set ; bind par FlushSkinned)
//
// NOTE : les bones passent par un UBO (std140, mat4 bones[64]) plutot qu'un
// SSBO. Un UBO est portable et solide sur GL/VK/DX11/DX12 (SPIRV-Cross le
// convertit en cbuffer DX / uniform block GL-VK). Le SSBO precedent avait un
// binding runtime fragile (casse DX11/DX12 : StructuredBuffer/SRV n'atteignait
// pas le shader -> skinMat=0 -> mesh invisible ; course Vulkan). 64 bones max.
//
// Y-clip : Vulkan inverse Y (gl_Position.y = -gl_Position.y) comme pbr.vert/VK.
// Canonique : ce .vk.glsl est converti VK->GL/HLSL/MSL par SPIRV-Cross au run.
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
layout(location=6) in vec4 aBoneIdx;     // indices float (cast int en shader)
layout(location=7) in vec4 aBoneWeight;  // poids LBS

layout(set=0, binding=0, std140) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir;
    vec2  viewport;
    float time, dt;
    float iblStrength;
} uCam;

layout(set=1, binding=1, std140) uniform ObjectUBO {
    mat4  model, normalMatrix;
    vec4  tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

// Joint matrices (deja = globalTransform * inverseBind cote CPU).
// UBO std140 borne a 64 bones (4096 octets). Portable GL/VK/DX11/DX12.
layout(set=1, binding=2, std140) uniform NkBonesUBO {
    mat4 bones[64];
} uBones;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;

void main() {
    // Indices float -> int (arrondi). Le loader clamp deja ; on clamp aussi a
    // [0,63] pour rester dans les bornes de l'UBO (acces hors borne = UB).
    ivec4 bi = clamp(ivec4(aBoneIdx + 0.5), ivec4(0), ivec4(63));

    // Linear blend skinning : combinaison ponderee des 4 joint matrices.
    mat4 skinMat = aBoneWeight.x * uBones.bones[bi.x]
                 + aBoneWeight.y * uBones.bones[bi.y]
                 + aBoneWeight.z * uBones.bones[bi.z]
                 + aBoneWeight.w * uBones.bones[bi.w];

    // Si tous les poids sont nuls (vertex non riggé), retombe sur l'identite
    // pour ne pas collapser le vertex a l'origine.
    float wsum = aBoneWeight.x + aBoneWeight.y + aBoneWeight.z + aBoneWeight.w;
    if (wsum < 1e-4) skinMat = mat4(1.0);

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec4 worldPos   = uObj.model * skinnedPos;
    vWorldPos       = worldPos.xyz;

    // Normale transformee par (model normalMatrix) * partie 3x3 de skinMat.
    mat3 nm    = mat3(uObj.normalMatrix) * mat3(skinMat);
    vNormal    = normalize(nm * aNormal);
    vTangent   = normalize(nm * aTangent);
    vBitangent = cross(vNormal, vTangent);

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    gl_Position   = uCam.viewProj * worldPos;
    // PAS de `gl_Position.y = -gl_Position.y` : la chaine de conversion
    // (SpirvToGlsl/SpirvToHlsl) + le viewProj gerent deja la convention Y par
    // backend (comme le shader PBR qui n'a pas de flip manuel). Un flip manuel ici
    // DOUBLE-flippait le mesh (tete en bas) -> masque sur le strip symetrique
    // SimpleSkin, mais visible sur les humanoides asymetriques (CesiumMan/BrainStem).
    // Retire = orientation correcte verifiee sur GL/VK/DX11/DX12.
}
