// =============================================================================
// layered.frag.vk.glsl — NKRenderer M.1 Material Layering (v0)
//
// 2 couches PBR superposees, blend par vColor.r :
//   layer0 (base) = uLayered.base    | metal, roughness, color
//   layer1 (top)  = uLayered.top     | metal, roughness, color
//   final = mix(eval(layer0), eval(layer1), vColor.r)
//
// v0 : eval simplifie (Lambert + spec Phong), pas IBL ni ombres. Suffit pour
// demontrer le mecanisme. Une fois la pipeline validee, on remplacera par
// le full PBR du shader PBR canonical (D_GGX + G_Smith + F_Schlick + IBL).
//
// Convention masque vColor :
//   .r : ratio mix (0 = layer0 pur, 1 = layer1 pur)
//   .gba : reserves pour layers supplementaires (M.1 v1 -> 4 layers RGBA).
// =============================================================================
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec2 vUV;
layout(location=4) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir;
    vec2  viewport;
    float time, deltaTime;
    float iblStrength;
    float _p0, _p1, _p2;
    mat4  mirrorViewProj;
    vec4  reflectionFlags;  // .x = isMirrorPass (skip shadow sampling)
} uCam;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4 positions[32], colors[32], directions[32], angles[32];
    int count; int _pad[3];
} uLights;

// Phase NkVSM : nouveau UBO multi-lights remplace l'ancien ShadowUBO cascade-only.
// Layout doit matcher ShadowSlotsUBOBlock cote C++ (NkVirtualShadowMaps.cpp).
struct NkShadowSlot {
    mat4 shadowMatrix;
    vec4 tileUV;
    vec4 lightPosOrDir;
    vec4 packedIds;
};
layout(std140, set=0, binding=3) uniform ShadowSlotsUBO {
    NkShadowSlot slots[256];
    vec4 firstSlotPerLight[8];
    vec4 slotCountPerLight[8];
    vec4 globalCfg;
    vec4 biasParams;
} uShadows;

layout(set=0, binding=11) uniform sampler2DShadow tShadowAtlas;
layout(set=0, binding=12) uniform sampler2D       tShadowAtlasRaw;
#include "Include/NkShadowAtlas.glsli"

// Phase H.6 : voxel AO grid (3D R8) au binding=27.
layout(set=0, binding=27) uniform sampler3D tVoxelOpacity;
#include "Include/NkVoxelAO.glsli"

// Phase M.2 : Material Parameter Collection (pool global de params partages).
// Convention de slots stables (cf. NkMaterialCollection::Init) :
//   params[0] = globalTint (vec4)    -> teinte multiplicative globale
//   params[1] = gameTime  (float .x) -> time accumulator pour animation
//   params[2] = windDirection (vec3) -> direction du vent
//   params[3] = windStrength (float .x) -> intensite du vent
layout(std140, set=0, binding=25) uniform MPC_UBO {
    vec4 params[64];
} uMPC;

// NkLayerParams = 2 PBR layers cote a cote (192 bytes std140 environ).
// Doit matcher EXACTEMENT le NkLayeredParams cote CPU.
struct LayerPBR {
    vec4 albedo;
    vec4 emissive;
    float metallic, roughness, ao, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRough, subsurface;
    vec4 subsurfaceColor;
    float anisotropy, sheen, _p0, _p1;
};

layout(std140, set=2, binding=8) uniform LayeredUBO {
    LayerPBR base;
    LayerPBR top;
    int maskSource;     // 0=vColor.r, 1=vColor.g, 2=vColor.b, 3=vColor.a, 4=texture(future)
    int _padA, _padB, _padC;
} uLayered;

// Evaluation simplifiee : Lambert diffus + Phong spec teinte metal/albedo.
// Phase NkVSM : shadow per-light est sample via SampleLightShadow (atlas).
// Dispatch automatique selon le type de light (DIR/POINT/SPOT).
vec3 EvalLayer(LayerPBR p, vec3 N, vec3 V, float voxAO) {
    vec3 albedo = p.albedo.rgb;
    // Phase H.6 : ambient attenue par voxel AO (cones traverse le sol etc).
    vec3 result = albedo * 0.10 * voxAO;

    for (int i = 0; i < uLights.count && i < 32; i++) {
        int lt = int(uLights.positions[i].w);
        vec3 L;
        float att = 1.0;
        if (lt == 0) { // directional
            L = normalize(-uLights.directions[i].xyz);
        } else {       // point/spot
            vec3 d = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att *= att;
        }
        // NkVSM : sample shadow atlas pour cette light. angles[i].z = castShadow.
        float lightShadow = (uLights.angles[i].z > 0.5)
                            ? SampleLightShadow(i, vWorldPos, N, L) : 1.0;
        float NdL = max(dot(N, L), 0.0);
        vec3 diffuse = albedo * NdL;

        // Phong spec, hardness derived from roughness (1=mat, 0=miroir)
        vec3 H = normalize(V + L);
        float spec = pow(max(dot(N, H), 0.0), mix(64.0, 8.0, p.roughness));
        // Metallic : tint spec par albedo, sinon blanc
        vec3 specColor = mix(vec3(1.0), albedo, p.metallic);
        vec3 specular = specColor * spec * (1.0 - p.roughness * 0.7);

        result += uLights.colors[i].rgb * uLights.colors[i].w * att * lightShadow
                * (diffuse + specular);
    }
    return result * p.ao + p.emissive.rgb * p.emissiveStrength;
}

void main() {
    vec3 N = normalize(vNormal);
    // Phase Planar Reflection fix 2026-05-24 : un-mirror camPos pour V quand on
    // shade un fragment d'une passe miroir (vWorldPos deja en espace real).
    vec3 camPosForV = uCam.camPos.xyz;
    if (uCam.reflectionFlags.x > 0.5) camPosForV.y = -camPosForV.y;
    vec3 V = normalize(camPosForV - vWorldPos);

    // Selection du masque selon maskSource :
    //   0=vColor.r 1=.g 2=.b 3=.a 4=vUV.y (gradient vertical) 5=1-vUV.y
    // 4/5 utiles pour demo sans mesh vertex-colored : la sphere a un blend
    // vertical naturel (poles vs equateur via parametrisation UV).
    float mask = vColor.r;
    if (uLayered.maskSource == 1) mask = vColor.g;
    if (uLayered.maskSource == 2) mask = vColor.b;
    if (uLayered.maskSource == 3) mask = vColor.a;
    if (uLayered.maskSource == 4) mask = vUV.y;
    if (uLayered.maskSource == 5) mask = 1.0 - vUV.y;

    // Phase M.2 : module le mask par gameTime pour animer la transition.
    // Anime UNIQUEMENT pour les masks UV (4=vUV.y, 5=1-vUV.y) car les masks
    // vColor (0-3) sont typiquement controles par l'utilisateur (Dynamic Paint
    // M.6) -> on les garde STATIQUES pour ne pas pertuber l'observation.
    if (uLayered.maskSource >= 4) {
        float gameTime = uMPC.params[1].x;
        mask = mask + 0.5 * sin(gameTime * 1.5);
    }
    mask = clamp(mask, 0.0, 1.0);

    // Echantillonne le CSM une fois pour le fragment ; partage entre les 2
    // layers (meme position monde).
    // Phase NkVSM : shadow par-light deja sample dans EvalLayer.
    // Phase H.6 : voxel AO partage entre les 2 layers (meme position monde).
    float voxAO  = NkComputeVoxelAO(vWorldPos, N);

    vec3 c0 = EvalLayer(uLayered.base, N, V, voxAO);
    vec3 c1 = EvalLayer(uLayered.top,  N, V, voxAO);
    vec3 color = mix(c0, c1, mask);

    // Phase M.2 : applique globalTint depuis la Material Parameter Collection.
    // Defaut (1,1,1,1) -> no-op multiplicative. Demo5 cycle des couleurs pour
    // demontrer l'update partage entre tous les shaders qui lisent params[0].
    vec3 globalTint = uMPC.params[0].rgb;
    color *= globalTint;

    fragColor = vec4(color, uLayered.base.albedo.a);
}
