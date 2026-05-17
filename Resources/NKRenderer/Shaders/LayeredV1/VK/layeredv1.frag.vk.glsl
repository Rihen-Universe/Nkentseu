// layeredv1.frag.vk.glsl — M.1 v1 : Material Layering N=8 (Vulkan)
//
// Empile jusqu'a 8 couches PBR-simplifiees (albedo, metallic, roughness) blend
// via masks paramétriques (vColor, vUV, constant ou albedo.a).
//
// Lighting : Phong-like minimal (1 lumiere directionnelle "soleil" fixe + ambient)
// pour visualiser les couleurs/metalliques sans dependre du systeme PBR full.
//
// Layout UBO (std140) — voir struct C++ NkLayeredV1Params :
//   layers[8]       (8 * 32 = 256 bytes)
//   maskSources[8]  (8 * 4  = 32, stockes en vec4 packs via int[8])
//   maskConstants[8](8 * 4  = 32, idem)
//   numLayers       (4 bytes + pad)
//   _pad[3]         (12 bytes)
// Total : ~336 bytes (sous le seuil 16KB UBO).
//
// @material("LayeredV1")
#version 460 core

#include "Include/NkLayerBlend.glsli"
#include "Include/NkColor.glsli"

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
    float iblStrength;
} uCam;

// UBO LayeredV1 (set=2 binding=8 comme tous les materiaux per-instance).
// std140 force le padding sur 16 bytes — d'ou les arrays de int wrappes en
// vec4 (4 ints per vec4 = 2 vec4 pour 8 sources) puis float wrappes pareil.
layout(std140, set=2, binding=8) uniform LayeredV1UBO {
    NkPBRLayer layers[8];
    // 8 ints packes en 2 ivec4 pour respecter std140 (les arrays scalaires
    // gaspillent 12 bytes par element en std140 ; vec4 c'est plus compact).
    ivec4 maskSources0;   // sources des layers 0..3
    ivec4 maskSources1;   // sources des layers 4..7
    vec4  maskConstants0; // constants 0..3
    vec4  maskConstants1; // constants 4..7
    int   numLayers;
    int   _pad0, _pad1, _pad2;
} uMat;

// Helpers d'extraction par index dans les arrays packees ivec4/vec4.
// GLSL n'autorise pas l'indexation dynamique sur ivec4.xyzw sans extension,
// d'ou ce switch-like.
int maskSources0_at(int i) {
    if (i == 0) return uMat.maskSources0.x;
    if (i == 1) return uMat.maskSources0.y;
    if (i == 2) return uMat.maskSources0.z;
    return uMat.maskSources0.w;
}
int maskSources1_at(int i) {
    if (i == 0) return uMat.maskSources1.x;
    if (i == 1) return uMat.maskSources1.y;
    if (i == 2) return uMat.maskSources1.z;
    return uMat.maskSources1.w;
}
float maskConstants_at(int i) {
    if (i == 0) return uMat.maskConstants0.x;
    if (i == 1) return uMat.maskConstants0.y;
    if (i == 2) return uMat.maskConstants0.z;
    if (i == 3) return uMat.maskConstants0.w;
    if (i == 4) return uMat.maskConstants1.x;
    if (i == 5) return uMat.maskConstants1.y;
    if (i == 6) return uMat.maskConstants1.z;
    return uMat.maskConstants1.w;
}

void main() {
    int N = clamp(uMat.numLayers, 1, 8);

    // Layer 0 = base, sans mask.
    NkLayerAccum acc = NkLayerInit(uMat.layers[0]);

    // Layers 1..N-1 : blendes selon leur mask.
    for (int i = 1; i < 8; ++i) {
        if (i >= N) break;
        int src = (i < 4) ? maskSources0_at(i) : maskSources1_at(i - 4);
        float k = maskConstants_at(i);
        float mask = NkPickMask(src, vColor, vUV, k, uMat.layers[i].albedo.a);
        acc = NkLayerBlend(acc, uMat.layers[i], mask);
    }

    // ── Lighting Phong-like simple (suffit pour visualiser le layering) ──
    vec3 N3 = normalize(vNormal);
    vec3 L  = normalize(vec3(-0.3, 1.0, 0.4));
    vec3 V  = normalize(uCam.camPos.xyz - vWorldPos);
    vec3 H  = normalize(L + V);

    float NdotL = max(dot(N3, L), 0.0);
    float NdotH = max(dot(N3, H), 0.0);

    // Spec exponent depend de roughness : rough=0 → exp=128 (brillant), rough=1 → exp=2 (mat)
    float specExp = mix(128.0, 2.0, acc.roughness);
    float spec    = pow(NdotH, specExp);

    // Couleur spec : interpolation albedo<->blanc selon metallic
    vec3 specColor = mix(vec3(1.0), acc.albedo, acc.metallic);

    vec3 diffuse = acc.albedo * (NdotL + 0.18);   // ambient = 0.18
    vec3 specular = specColor * spec * (acc.metallic * 0.7 + 0.3);

    vec3 final = diffuse + specular;
    fragColor  = vec4(final, 1.0);
}
