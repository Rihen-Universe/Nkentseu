#version 330 core
// =============================================================================
// PV3DE/Shaders/Eye.frag
// Shader yeux humains avec :
//   - Iris procédural (pigmentation + limbal ring + nervures)
//   - Réfraction de la cornée (parallax offset)
//   - Spéculaire cornéen (highlight ponctuel + diffus ambiant)
//   - Pupille réactive (diamètre contrôlé par u_pupilDiameter)
//   - Humeur aqueuse (effet de profondeur)
//   - Rougeur sclère (niveau d'anxiété/fatigue)
//
// Paramètres cliniques controlables depuis PatientLayer :
//   u_pupilDiameter   [0.1, 0.9] — 0.1=myosis max, 0.9=mydriase max
//   u_scleraRedness   [0, 1]     — injection conjonctivale
//   u_irisColor       vec3       — couleur de base de l'iris
//   u_eyeWetness      [0, 1]     — brillance cornée (coma=0, normal=1)
// =============================================================================

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_viewDir;

out vec4 fragColor;

// ── Textures ──────────────────────────────────────────────────────────────────
uniform sampler2D u_irisDetailMap;   // détail fin de l'iris (optionnel)
uniform sampler2D u_scleraMap;       // texture sclère de base

// ── Paramètres cliniques ──────────────────────────────────────────────────────
uniform float u_pupilDiameter  = 0.35;   // 0.1–0.9
uniform float u_scleraRedness  = 0.0;    // 0–1
uniform vec3  u_irisColor      = vec3(0.22, 0.48, 0.62);  // bleu-vert
uniform float u_eyeWetness     = 1.0;
uniform float u_gazeOffsetX    = 0.0;    // direction du regard
uniform float u_gazeOffsetY    = 0.0;

// ── Lumière principale ────────────────────────────────────────────────────────
uniform vec3  u_lightDir     = vec3(0.3, 1.0, 0.5);
uniform vec3  u_lightColor   = vec3(1.0, 0.97, 0.90);
uniform float u_lightIntensity = 1.2;
uniform vec3  u_ambientColor = vec3(0.06, 0.06, 0.08);

// ── Constantes ────────────────────────────────────────────────────────────────
const float PI = 3.14159265359;
const float IOR_CORNEA  = 1.376;   // indice réfraction cornée
const float IOR_AQUEOUS = 1.336;   // humeur aqueuse

// ── Iris procédural ───────────────────────────────────────────────────────────
// UV remappés sur un disque [-1,1]²
vec3 IrisProc(vec2 uv, float pupilR) {
    // Distorsion regard
    vec2 p = uv + vec2(u_gazeOffsetX, u_gazeOffsetY) * 0.06;
    float r = length(p);

    // Pupille (noir)
    if (r < pupilR) return vec3(0.02, 0.01, 0.01);

    // Bord pupillaire (anneau foncé)
    float pupilEdge = smoothstep(pupilR, pupilR + 0.04, r);

    // Iris de base
    vec3  iris = u_irisColor;

    // Nervures radiales (bruit simple)
    float angle = atan(p.y, p.x);
    float stripe = sin(angle * 18.0 + r * 12.0) * 0.5 + 0.5;
    float stripe2= sin(angle * 31.0 - r *  8.0) * 0.5 + 0.5;
    iris = mix(iris, iris * 0.6, stripe * 0.15 + stripe2 * 0.10);

    // Pigmentation radiale (plus clair au centre, plus sombre au limbe)
    float radGrad = 1.0 - smoothstep(pupilR + 0.05, 0.5, r);
    iris = mix(iris, iris * 1.3, radGrad * 0.4);

    // Anneau limbique (limbal ring — foncé au bord)
    float limbal = 1.0 - smoothstep(0.42, 0.50, r);
    iris = mix(iris * 0.3, iris, limbal);

    // Applique l'anneau pupillaire
    iris = mix(iris * 0.4, iris, pupilEdge);

    // Texture de détail fine (si disponible)
    vec3 detail = texture(u_irisDetailMap, uv * 0.5 + 0.5).rgb;
    iris = mix(iris, iris * detail, 0.25);

    return iris;
}

// ── Réfraction cornéenne (parallax) ──────────────────────────────────────────
vec2 CorneaParallax(vec2 uv, vec3 viewDir, vec3 N, float depth) {
    // Snell-Descartes simplifié : calcule le décalage UV
    float eta  = 1.0 / IOR_CORNEA;
    vec3  refr = refract(-viewDir, N, eta);
    vec2  offset = refr.xy * depth * 0.08;
    return uv + offset;
}

// =============================================================================
void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_viewDir);
    vec3 L = normalize(u_lightDir);
    vec3 H = normalize(V + L);

    // ── Réfraction — UV iris décalés ──────────────────────────────────────────
    vec2 refractUV = CorneaParallax(v_uv, V, N, 0.5);

    // Remapper [-0.5,0.5] → [-1,1]
    vec2 irisDiskUV = (refractUV - 0.5) * 2.0;
    float r = length(irisDiskUV);

    // ── Sclère ────────────────────────────────────────────────────────────────
    vec4 scleraSamp = texture(u_scleraMap, v_uv);
    vec3 sclera     = scleraSamp.rgb;

    // Rougeur (injection conjonctivale)
    sclera = mix(sclera,
                 sclera * vec3(1.4, 0.7, 0.7),
                 u_scleraRedness * smoothstep(0.48, 0.52, r));

    // ── Iris ou sclère ────────────────────────────────────────────────────────
    float pupilR    = u_pupilDiameter * 0.5;
    float irisOuter = 0.48;

    vec3  irisColor = IrisProc(irisDiskUV, pupilR);
    float isIris    = 1.0 - smoothstep(irisOuter - 0.01, irisOuter, r);
    vec3  baseColor = mix(sclera, irisColor, isIris);

    // ── Diffus ────────────────────────────────────────────────────────────────
    float NdL = max(dot(N, L), 0.0);
    vec3  diffuse = baseColor * u_lightColor * u_lightIntensity * NdL;
    vec3  ambient = u_ambientColor * baseColor;

    // ── Spéculaire cornée (Blinn-Phong très concentré) ───────────────────────
    float NdH     = max(dot(N, H), 0.0);
    float specPow = mix(50.0, 250.0, u_eyeWetness);
    float spec    = pow(NdH, specPow) * u_eyeWetness;
    vec3  specular= u_lightColor * spec * 0.95;

    // Second highlight plus doux (reflet ambiant)
    float spec2   = pow(NdH, 12.0) * u_eyeWetness * 0.15;
    specular += vec3(spec2);

    // ── Assemblage final ──────────────────────────────────────────────────────
    vec3 color = ambient + diffuse + specular;

    // Correction gamma
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}
