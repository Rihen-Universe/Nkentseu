#version 330 core
// =============================================================================
// PV3DE/Shaders/Skin.frag
// Fragment shader PBR + Subsurface Scattering (SSS) approximé.
//
// Modèle PBR :
//   BRDF Cook-Torrance : D (GGX) + G (Smith) + F (Schlick)
//
// SSS simplifié (Translucency approximation) :
//   Technique : "wrapped diffuse + thin slab" — pas de vrai ray marching.
//   La lumière pénètre légèrement le dos de la peau.
//   Paramètre : u_sssStrength, u_sssRadius, u_sssColor
//
// Textures :
//   Binding 0 — u_albedoMap     (couleur de base)
//   Binding 1 — u_normalMap     (normales tangentes)
//   Binding 2 — u_ormMap        (Occlusion/Roughness/Metallic)
//   Binding 3 — u_sssMap        (masque SSS — zones sensibles)
//   Binding 4 — u_emissiveMap   (rougeurs, cyanose)
// =============================================================================

in vec3 v_worldPos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec3 v_viewDir;

out vec4 fragColor;

// ── Textures ──────────────────────────────────────────────────────────────────
uniform sampler2D u_albedoMap;
uniform sampler2D u_normalMap;
uniform sampler2D u_ormMap;      // R=AO, G=roughness, B=metallic
uniform sampler2D u_sssMap;      // R=sss mask
uniform sampler2D u_emissiveMap; // rougeurs, cyanose

// ── Matériau ──────────────────────────────────────────────────────────────────
uniform float u_roughness       = 0.45;
uniform float u_metallic        = 0.02;
uniform float u_sssStrength     = 0.35;  // force du SSS [0,1]
uniform vec3  u_sssColor        = vec3(0.85, 0.30, 0.20); // teinte SSS (rouge-orangé)
uniform float u_sssRadius       = 0.8;   // rayon de scattering
uniform vec4  u_skinTint        = vec4(1.0); // teinte appliquée à l'albedo
uniform float u_emissiveStrength= 1.0;

// ── Lumières (simplifiées — Phase 5 : passer par un UBO de lumières) ──────────
uniform vec3  u_lightPos[4];
uniform vec3  u_lightColor[4];
uniform float u_lightIntensity[4];
uniform int   u_lightCount;
uniform vec3  u_ambientColor    = vec3(0.05, 0.05, 0.07);

// ── Constantes ────────────────────────────────────────────────────────────────
const float PI = 3.14159265359;
const float INV_PI = 0.31830988618;

// ── PBR helpers ───────────────────────────────────────────────────────────────
float D_GGX(float NdH, float rough) {
    float a  = rough * rough;
    float a2 = a * a;
    float d  = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-7);
}

float G_Smith(float NdV, float NdL, float rough) {
    float r  = rough + 1.0;
    float k  = r * r / 8.0;
    float gv = NdV / (NdV * (1.0 - k) + k);
    float gl = NdL / (NdL * (1.0 - k) + k);
    return gv * gl;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ── Normal mapping ────────────────────────────────────────────────────────────
vec3 GetNormal() {
    vec3 tangentNormal = texture(u_normalMap, v_uv).xyz * 2.0 - 1.0;
    tangentNormal.xy  *= 1.2; // légère exagération pour la peau

    vec3 N = normalize(v_normal);
    vec3 T = normalize(v_tangent.xyz);
    vec3 B = cross(N, T) * v_tangent.w;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// ── SSS approximé ─────────────────────────────────────────────────────────────
vec3 ComputeSSS(vec3 N, vec3 L, vec3 albedo, float sssMask) {
    // Wrapped diffuse : étend la lumière sur le dos de la surface
    float wrap     = 0.5;
    float NdL_wrap = (dot(N, L) + wrap) / (1.0 + wrap);
    NdL_wrap       = max(NdL_wrap, 0.0);

    // Translucency : lumière traversant la peau (dos éclairé)
    float transNdL = max(dot(-N, L), 0.0);
    float trans    = transNdL * u_sssRadius;

    vec3 sssContrib = u_sssColor * albedo * (NdL_wrap + trans * 0.4);
    return sssContrib * sssMask * u_sssStrength;
}

// =============================================================================
void main() {
    // ── Échantillonnage textures ───────────────────────────────────────────────
    vec4 albedoSamp  = texture(u_albedoMap,   v_uv) * u_skinTint;
    vec3 albedo      = pow(albedoSamp.rgb, vec3(2.2)); // sRGB → linear

    vec3 orm         = texture(u_ormMap,      v_uv).rgb;
    float ao         = orm.r;
    float roughness  = clamp(orm.g * u_roughness, 0.04, 1.0);
    float metallic   = orm.b * u_metallic;

    float sssMask    = texture(u_sssMap, v_uv).r;
    vec3  emissive   = texture(u_emissiveMap, v_uv).rgb * u_emissiveStrength;

    // ── Vecteurs ──────────────────────────────────────────────────────────────
    vec3 N  = GetNormal();
    vec3 V  = normalize(v_viewDir);
    float NdV = max(dot(N, V), 0.0001);

    // F0 peau : entre diélectrique (0.04) et légèrement conducteur
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── Accum lumières ────────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < u_lightCount && i < 4; ++i) {
        vec3  Ldir  = normalize(u_lightPos[i] - v_worldPos);
        vec3  H     = normalize(V + Ldir);
        float dist  = length(u_lightPos[i] - v_worldPos);
        float atten = 1.0 / (dist * dist + 0.01);
        vec3  radiance = u_lightColor[i] * u_lightIntensity[i] * atten;

        float NdL = max(dot(N, Ldir), 0.0);
        float NdH = max(dot(N, H),    0.0);
        float HdV = max(dot(H, V),    0.0);

        // Spéculaire PBR Cook-Torrance
        float D = D_GGX(NdH, roughness);
        float G = G_Smith(NdV, NdL, roughness);
        vec3  F = F_Schlick(HdV, F0);

        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 specular = D * G * F / (4.0 * NdV * NdL + 1e-7);
        vec3 diffuse  = kD * albedo * INV_PI;

        // SSS sur la composante diffuse
        vec3 sss = ComputeSSS(N, Ldir, albedo, sssMask);

        Lo += (diffuse + specular + sss) * radiance * NdL;
    }

    // ── Ambiance + AO ─────────────────────────────────────────────────────────
    vec3 ambient = u_ambientColor * albedo * ao;

    vec3 color = ambient + Lo + emissive;

    // ── Tone mapping (ACES approximé) + gamma ────────────────────────────────
    color = color / (color + vec3(0.155)) * 1.019;  // ACES filmic approximé
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    fragColor = vec4(color, albedoSamp.a);
}
