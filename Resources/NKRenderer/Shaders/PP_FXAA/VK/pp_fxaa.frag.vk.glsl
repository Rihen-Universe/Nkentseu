// pp_fxaa.frag.vk.glsl — Fast Approximate Anti-Aliasing (FXAA 3.11-style)
//
// Phase L finition 2026-05-23. Implementation simplifiee inspiree de
// NVIDIA FXAA 3.11 (Timothy Lottes). Effectuee apres tonemap sur du LDR
// sRGB. Detecte les edges par contraste de luma + blend along edge.
//
// Algo :
//   1. Sample 5 pixels (centre + N/S/E/W), calcule leur luma (Rec. 709 weights)
//   2. Si le contraste local (maxLuma - minLuma) < threshold -> skip (zone plate)
//   3. Sinon : calcule direction de l'edge depuis les gradients de luma
//   4. Sample 2 puis 4 fois le long de la direction et moyenne
//   5. Si moyenne est hors-range du local min/max -> revient au sample 2-pass
//
// Pas de subpix detail, pas de console-mode, pas de tunable lumaThreshold
// runtime pour V0. Tune par defaut conservatrice (subtle AA, pas blurry).
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uLDR;

layout(push_constant) uniform PC {
    vec2  invResolution;  // 1/W, 1/H en pixels UV
    float yFlipUV;
    float _pad;
} pc;

// Rec. 709 luma weights (perceptual brightness).
float NkLuma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// Seuils FXAA 3.11 standards :
//   contrastMin = 0.0312 = un edge detectable doit avoir > 3.1% de variation luma
//   contrastRel = 0.125 = ou bien > 12.5% du luma max local
//   dirReduce   = 1/128 = reduction de la direction pour eviter division par 0
//   spanMax     = 8.0 = max span sample en pixels
const float kContrastMin = 0.0312;
const float kContrastRel = 0.125;
const float kDirReduce   = 1.0/128.0;
const float kDirReduceMin= 1.0/8.0;
const float kSpanMax     = 8.0;

void main() {
    vec2 px = pc.invResolution;
    // 5 samples : centre + 4 voisins cardinaux.
    vec3 cC = texture(uLDR, vUV).rgb;
    vec3 cN = texture(uLDR, vUV + vec2(0.0,  px.y)).rgb;
    vec3 cS = texture(uLDR, vUV - vec2(0.0,  px.y)).rgb;
    vec3 cE = texture(uLDR, vUV + vec2(px.x, 0.0)).rgb;
    vec3 cW = texture(uLDR, vUV - vec2(px.x, 0.0)).rgb;

    float lC = NkLuma(cC);
    float lN = NkLuma(cN);
    float lS = NkLuma(cS);
    float lE = NkLuma(cE);
    float lW = NkLuma(cW);

    float lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float range = lMax - lMin;

    // Zone plate : pas d'edge a smoother, on retourne le pixel original.
    if (range < max(kContrastMin, lMax * kContrastRel)) {
        oColor = vec4(cC, 1.0);
        return;
    }

    // Direction edge depuis les gradients luma (formule FXAA 3.11).
    vec2 dir;
    dir.x = -((lN + lS) - (lE + lW));   // gradient horizontal
    dir.y =  ((lW + lE) - (lN + lS));   // gradient vertical

    // Normalise + clamp la direction. dirReduce evite la division par 0
    // pour les edges purs horizontaux/verticaux.
    float dirReduce = max((lN + lS + lE + lW) * 0.25 * kDirReduceMin, kDirReduce);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-kSpanMax), vec2(kSpanMax)) * px;

    // 1er passe : 2 samples le long de la direction edge.
    vec3 r1 = 0.5 * (
        texture(uLDR, vUV + dir * (1.0/3.0 - 0.5)).rgb +
        texture(uLDR, vUV + dir * (2.0/3.0 - 0.5)).rgb);
    // 2eme passe : 4 samples couvrant plus large.
    vec3 r2 = r1 * 0.5 + 0.25 * (
        texture(uLDR, vUV + dir * -0.5).rgb +
        texture(uLDR, vUV + dir *  0.5).rgb);

    // Si la moyenne 4-samples est en dehors du range local, l'edge etait
    // mal estime (high freq detail) -> on revient a la 2-samples plus safe.
    float lR2 = NkLuma(r2);
    oColor = vec4((lR2 < lMin || lR2 > lMax) ? r1 : r2, 1.0);
}
