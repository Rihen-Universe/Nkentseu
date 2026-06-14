// reflfloor.frag.vk.glsl — NKRenderer v5.1 — Reflective Floor Fragment (Vulkan)
//
// Planar reflection via render-to-texture + screen-space UV lookup.
// NkPlanarReflectionSystem (NKRenderer Tools/Reflection) fait la passe miroir
// automatiquement (1 ou 2 selon faceMode) et bind les RTs sur ce shader :
//   - tReflection      (set=2 binding=3) = RT face avant (+N cote camera)
//   - tReflectionBack  (set=2 binding=4) = RT face arriere (-N), seulement si
//                                          faceMode == BACK_ONLY ou BOTH
//
// faceMode (uFloor.reflFloorFaceMode) :
//   0 = FRONT_ONLY  (face avant visible avec reflet, face arriere = discard)
//   1 = BACK_ONLY   (face arriere visible avec reflet, face avant = discard)
//   2 = BOTH        (les deux faces avec leur RT respectif)
//
// @material("Default_ReflFloor")
// @color("albedo", default=(0.55,0.55,0.60,1))  vec4 albedo
// @param("roughness", min=0.0, max=1.0, default=0.05)  float roughness
// @texture2D("reflection")     sampler2D tReflection      (binding=3)
// @texture2D("reflectionBack") sampler2D tReflectionBack  (binding=4)
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
    float iblStrength;
    float _pad0, _pad1, _pad2;
    mat4  mirrorViewProj;
} uCam;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4  positions[32], colors[32], directions[32], angles[32];
    int   count; int _pad[3];
} uLights;

// NkPBRParams — std140, 96 bytes. Doit matcher EXACTEMENT le struct C++.
// offsets : albedo=0, emissive=16, metallic=32, roughness=36, ao=40,
//           emissiveStrength=44, normalStrength=48, clearcoat=52,
//           clearcoatRough=56, subsurface=60, subsurfaceColor=64,
//           anisotropy=80, sheen=84, reflFloorFaceMode=88, _pad=92
layout(std140, set=2, binding=8) uniform ReflFloorUBO {
    vec4  albedo;
    vec4  emissive;
    float metallic;
    float roughness;
    float ao;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRough;
    float subsurface;
    vec4  subsurfaceColor;
    float anisotropy;
    float sheen;
    float reflFloorFaceMode;   // 0=FRONT_ONLY, 1=BACK_ONLY, 2=BOTH
    float _pad;
} uFloor;

layout(set=2, binding=3) uniform sampler2D tReflection;      // RT face avant
layout(set=2, binding=4) uniform sampler2D tReflectionBack;  // RT face arriere

void main() {
    vec3 N   = normalize(vNormal);
    vec3 V   = normalize(uCam.camPos.xyz - vWorldPos);
    float NdV_signed = dot(N, V);
    float NdV_abs    = abs(NdV_signed);

    vec3 baseColor = uFloor.albedo.rgb * vColor.rgb;

    vec3 diffuse = vec3(0.0);
    for (int i = 0; i < uLights.count && i < 32; i++) {
        vec3  L;
        float att = 1.0;
        int   lt  = int(uLights.positions[i].w);
        if (lt == 0) {
            L = normalize(-uLights.directions[i].xyz);
        } else {
            vec3  d    = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L   = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att = att * att;
        }
        float NdL = max(dot(N, L), 0.0);
        diffuse += uLights.colors[i].rgb * uLights.colors[i].w * att * NdL;
    }
    vec3 directLit = baseColor * clamp(diffuse, vec3(0.0), vec3(1.0)) * 0.35;
    vec3 ambient   = baseColor * 0.10;
    vec3 litBase   = ambient + directLit;

    // Decision face visible selon faceMode :
    int faceMode = int(uFloor.reflFloorFaceMode + 0.5);  // round
    bool seeFront = (NdV_signed >  0.0);
    bool seeBack  = (NdV_signed <= 0.0);
    bool discardFrag = false;
    if (faceMode == 0) {                  // FRONT_ONLY
        if (seeBack) discardFrag = true;
    } else if (faceMode == 1) {           // BACK_ONLY
        if (seeFront) discardFrag = true;
    }                                     // BOTH : pas de discard

    if (discardFrag) discard;

    // Fade aux angles rasants (commun aux deux cotes).
    float facing = smoothstep(0.0, 0.05, NdV_abs);

    // Fresnel (utilise |NdV| pour gerer les deux cotes uniformement).
    float f0      = 0.04;
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - NdV_abs, 5.0);

    float reflStr = (1.0 - uFloor.roughness) * mix(0.9, 1.0, fresnel) * facing;

    // UV ecran du fragment, project via mirrorViewProj (calc par le system).
    vec4 reflClip = uCam.mirrorViewProj * vec4(vWorldPos, 1.0);
    vec2 reflUV   = (reflClip.xy / reflClip.w) * 0.5 + 0.5;
    // Vulkan viewport flipY=true : RT stocke inversee verticalement. Flip UV.y.
    reflUV.y = 1.0 - reflUV.y;

    // Sample du bon RT selon le cote vu :
    //   face avant -> tReflection      (RT pos)
    //   face arriere -> tReflectionBack (RT neg, alloue si twoSided/BOTH/BACK_ONLY)
    // Note 2026-05-23 : le SWAP testé empire le bug HDRI inversé et casse en
    // plus le rendering des sphères dans le reflet. Bug racine = ailleurs
    // (probablement BuildMirrorMatrix CPU + HDR cubemap Y convention).
    // cf. memory/nkrenderer_planar_reflection_bugs.md
    vec3 reflColor;
    if (seeFront) {
        reflColor = texture(tReflection, reflUV).rgb;
    } else {
        reflColor = texture(tReflectionBack, reflUV).rgb;
    }

    vec3 color = mix(litBase, reflColor, clamp(reflStr, 0.0, 1.0));
    fragColor  = vec4(color, uFloor.albedo.a);
}
