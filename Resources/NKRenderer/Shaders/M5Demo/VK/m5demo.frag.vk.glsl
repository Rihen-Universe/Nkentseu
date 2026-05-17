// m5demo.frag.vk.glsl — M.5 Material Functions DEMO
//
// Utilise les fonctions .glsli du dossier Include/ via #include :
//   - NkNoise.glsli   : FBM 2D procedural
//   - NkColor.glsli   : HSV<->RGB, saturation
//   - NkToonRamp.glsli: ramp 2-step cel-shaded
//   - NkFresnel.glsli : Fresnel-Schlick scalaire
//
// Effet : sphere stylisee avec
//   - couleur HSV cycling sur la teinte (override possible via uMat.albedo)
//   - texture FBM modulee
//   - ramp Toon a partir du NdotL fictif (dot(N, fakeLight))
//   - halo Fresnel sur les bords (couleur + intensite + power parametrables)
//
// Mapping NkPBRParams (UBO standard) -> params M5Demo :
//   albedo.rgb          : couleur tinte (multiplie le HSV cycle)
//   albedo.a            : mix HSV<->tinte (0=cycle pur, 1=tinte pure)
//   emissive.rgb        : couleur du halo Fresnel
//   emissiveStrength    : intensite du halo (0..10+)
//   roughness           : "rim power" — plus grand = halo plus concentre au bord
//                         (mappe sur pow(fres, 1+roughness*8))
//
// @material("M5Demo")
// @color("haloColor",     default=(1,1,0.95,1))  vec4 emissive
// @color("tint",          default=(1,1,1,0))     vec4 albedo
// @param("haloIntensity", min=0, max=10, default=2.5) float emissiveStrength
// @param("rimPower",      min=0, max=1,  default=0.25) float roughness
#version 460 core

#include "Include/NkNoise.glsli"
#include "Include/NkColor.glsli"
#include "Include/NkToonRamp.glsli"
#include "Include/NkFresnel.glsli"

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;
layout(location=4) in float vTime;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
    float iblStrength;
} uCam;

// UBO materiau : layout NkPBRParams (96 bytes) — le NkMaterialSystem upload ce
// struct quand le type est NK_CUSTOM (defaut PBR). Voir mapping dans l'entete.
layout(std140, set=2, binding=8) uniform M5DemoUBO {
    vec4  albedo;
    vec4  emissive;
    float metallic, roughness, ao, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRough, subsurface;
    vec4  subsurfaceColor;
    float anisotropy, sheen, reflFloorFaceMode, _pad;
} uMat;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCam.camPos.xyz - vWorldPos);
    float NdV = clamp(dot(N, V), 0.0, 1.0);

    // Lumiere fictive fixe (top-front) pour ramp Toon
    vec3 L = normalize(vec3(-0.3, 1.0, 0.4));
    float NdL = clamp(dot(N, L), 0.0, 1.0);

    // 1. Couleur de base : cycle HSV en fonction du temps + UV
    float hue = fract(vTime * 0.15 + vUV.x);
    vec3 baseHSV = vec3(hue, 0.85, 0.95);
    vec3 cycledRGB = NkHSVToRGB(baseHSV);
    // Mix avec tinte materiau via albedo.a (0=cycle, 1=tinte fixe)
    vec3 baseRGB = mix(cycledRGB, uMat.albedo.rgb, uMat.albedo.a);

    // 2. Texture FBM (procedural) : module la saturation par couches de bruit
    float n = NkFBM2D(vUV * 6.0 + vec2(vTime * 0.2, 0.0), 4);
    baseRGB = NkSaturate(baseRGB, 0.4 + 0.6 * n);

    // 3. Toon ramp 2-step depuis NdotL (shadow = baseRGB * 0.35)
    vec3 litColor    = baseRGB;
    vec3 shadowColor = baseRGB * 0.35;
    vec3 toned       = NkToonShade(litColor, shadowColor, NdL, 0.45, 0.06);

    // 4. Halo Fresnel parametrable :
    //    - rim power : pow(fres, 1 + roughness * 8)  [roughness in 0..1]
    //    - couleur   : uMat.emissive.rgb
    //    - intensite : uMat.emissiveStrength
    float fres   = NkFresnelSchlickScalar(NdV, 0.04);
    float rimPow = 1.0 + uMat.roughness * 8.0;
    float rim    = pow(fres, rimPow);
    vec3 halo    = uMat.emissive.rgb * rim * uMat.emissiveStrength;

    vec3 final = toned + halo;
    fragColor = vec4(final, 1.0) * vColor;
}
