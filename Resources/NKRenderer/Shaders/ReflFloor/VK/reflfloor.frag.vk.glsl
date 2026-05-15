// reflfloor.frag.vk.glsl — NKRenderer v4.0 — Reflective Floor Fragment (Vulkan)
//
// Planar reflection via render-to-texture + screen-space UV lookup.
// Le sol échantillonne le RT de réflexion à gl_FragCoord.xy/viewport,
// garantissant que chaque pixel du sol affiche ce que la caméra miroir voit
// exactement à cette position écran — pas besoin de coordonnées UV de mesh.
//
// Paramètres via NkPBRParams (set=2, binding=8) :
//   albedo.rgb   = couleur de base du sol (gris par défaut)
//   roughness    = 0 → miroir parfait, 1 → aucun reflet
//
// @material("Default_ReflFloor")
// @color("albedo", default=(0.55,0.55,0.60,1))  vec4 albedo
// @param("roughness", min=0.0, max=1.0, default=0.05)  float roughness
// @texture2D("reflection")  sampler2D tReflection  (binding=3)
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
    float _pad0, _pad1, _pad2;   // std140 padding pour aligner mirrorViewProj sur 16 bytes
    mat4  mirrorViewProj;        // Phase Planar Reflection : projection cam miroir
} uCam;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4  positions[32], colors[32], directions[32], angles[32];
    int   count; int _pad[3];
} uLights;

// NkPBRParams — std140, 96 bytes (doit correspondre exactement au struct C++)
// offsets : albedo=0, emissive=16, metallic=32, roughness=36, ao=40,
//           emissiveStrength=44, normalStrength=48, clearcoat=52,
//           clearcoatRough=56, subsurface=60, subsurfaceColor=64,
//           anisotropy=80, sheen=84, _pad=88
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
    float _pad0;
    float _pad1;
} uFloor;

// binding=3 = slot "albedo" du MaterialSystem → ici la texture RT de reflet
layout(set=2, binding=3) uniform sampler2D tReflection;

void main() {
    vec3 N   = normalize(vNormal);
    vec3 V   = normalize(uCam.camPos.xyz - vWorldPos);
    float NdV = max(dot(N, V), 0.0);

    // Couleur de base du sol modulée par la teinte par-objet
    vec3 baseColor = uFloor.albedo.rgb * vColor.rgb;

    // Éclairage diffus simple pour la couleur de base
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
    // Lighting du sol : atténué pour éviter la saturation avec des lumières fortes.
    // Le sol est principalement un miroir — la couleur de base reste subtile.
    vec3 directLit = baseColor * clamp(diffuse, vec3(0.0), vec3(1.0)) * 0.35;
    vec3 ambient   = baseColor * 0.10;
    vec3 litBase   = ambient + directLit;

    // Fresnel (Schlick) : plus de reflet aux angles rasants
    float f0      = 0.04;
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - NdV, 5.0);

    // Force du reflet dominante : roughness=0 → ~95% miroir.
    // mix entre un plancher de 0.7 (reflet visible même à incidence normale)
    // et 1.0 aux angles rasants via Fresnel.
    float reflStr = (1.0 - uFloor.roughness) * mix(0.7, 1.0, fresnel);

    // UV espace-écran pour le lookup du RT de réflexion.
    // gl_FragCoord.xy = position pixel du fragment dans le framebuffer courant.
    // Diviser par viewport donne [0,1] dans les deux axes.
    // Pas de flip Y : la caméra miroir rend déjà dans l'orientation correcte.
    // Sample du RT via projection mirror viewProj. Pour un fragment du sol
    // P_floor, reflUV = position screen du fragment ; le RT à cet UV contient
    // l'image de l'objet réfléchi (rendu via cam.viewProj * mirror_matrix * P).
    vec4 reflClip = uCam.mirrorViewProj * vec4(vWorldPos, 1.0);
    vec2 reflUV   = (reflClip.xy / reflClip.w) * 0.5 + 0.5;
    // Vulkan : viewport flipY=true inverse Y au rendering -> le RT est stocke
    // avec les donnees inversees verticalement. Le sample lecture UV (0,0)=top
    // recupere donc ce qui a ete ecrit en bas image NDC. Pour matcher,
    // flipper reflUV.y. NB : en OpenGL la convention storage est aussi
    // top-down via SPIRV-Cross + glClipControl LOWER_LEFT/ZERO_TO_ONE.
    reflUV.y = 1.0 - reflUV.y;
    vec3 reflColor = texture(tReflection, reflUV).rgb;

    // Mix simple : sol = très majoritairement le reflet du RT (95%) + un peu
    // de couleur de base (5%) pour avoir une teinte ardoise au lieu de noir
    // pur dans les zones non-reflétées. Les reflets sont visibles car peu
    // dilués par litBase.
    vec3 color = reflColor + litBase * 0.05;
    fragColor  = vec4(color, uFloor.albedo.a);
}
