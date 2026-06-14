// ============================================================
// pbr.frag.gl.glsl — NKRenderer v4.0 — PBR Fragment (OpenGL 4.6)
// Full PBR Metallic-Roughness + IBL + CSM shadows + Clearcoat + SSS
// ============================================================
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;
layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;
// Phase NkVSM : vShadowCoord retire — shadow coord est calcule dans le FS via
// la shadowMatrix du slot correspondant (ShadowSlotsUBO).

layout(location=0) out vec4 fragColor;

// ── UBOs ─────────────────────────────────────────────────────
layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos; vec4 camDir; vec2 viewport; float time; float deltaTime;
    float iblStrength;
    float _p0, _p1, _p2;
    mat4  mirrorViewProj;
    vec4  reflectionFlags;  // .x = isMirrorPass (skip shadow sampling)
} uCam;

layout(std140, set=1, binding=1) uniform ObjectUBO {
    mat4  model, normalMatrix; vec4 tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
    // NkVSM v1 : .x=receiveShadow, .y=castShadowAlphaTest(V1), .z=shadowBiasMul, .w=reserve
    vec4  shadowOverrides;
    // 2026-05-24 Triplanar : .x = tileSize en metres reels (0 = disabled),
    // .y = metersPerUnit, .z = enable flag (0/1), .w = reserve.
    vec4  triplanarParams;
} uObj;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4 positions[32], colors[32], directions[32], angles[32];
    int count; int _pad[3];
} uLights;

// Phase NkVSM : nouveau UBO multi-lights remplace l'ancien ShadowUBO cascade-only.
// Layout doit matcher ShadowSlotsUBOBlock cote C++ (NkVirtualShadowMaps.cpp).
struct NkShadowSlot {
    mat4 shadowMatrix;
    vec4 tileUV;          // .xy=minUV .zw=maxUV
    vec4 lightPosOrDir;   // .xyz pos/dir, .w range/splitFar
    vec4 packedIds;       // .x=lightIdx .y=slotType .z=subIdx .w=0
};
layout(std140, set=0, binding=3) uniform ShadowSlotsUBO {
    NkShadowSlot slots[256];
    vec4         firstSlotPerLight[8];  // 32 lights packes 4-per-vec4
    vec4         slotCountPerLight[8];
    vec4         globalCfg;             // .x=numSlots .y=softShadowMode
    vec4         biasParams;             // .x=shadowBias .y=normalBias .z=softness
} uShadows;

// ── Textures ─────────────────────────────────────────────────
// Materiau (set=2) : NkMaterialSystem bind albedo/normal/ORM/emissive
// dans set=2 binding 3-6. Auparavant ces textures etaient sur set=0
// binding 4-7 (defaut NkRender3D), ce qui empechait NkMaterialSystem
// d'overrider l'albedo per-instance. Move vers set=2 pour aligner sur
// la convention materiaux et permettre SetAlbedoMap a fonctionner.
layout(set=2, binding=3)  uniform sampler2D   tAlbedo;
layout(set=2, binding=4)  uniform sampler2D   tNormal;
layout(set=2, binding=5)  uniform sampler2D   tORM;        // R=AO G=Roughness B=Metallic
layout(set=2, binding=6)  uniform sampler2D   tEmissive;
layout(set=0, binding=8)  uniform samplerCube tEnvIrradiance;
layout(set=0, binding=9)  uniform samplerCube tEnvPrefilter;
layout(set=0, binding=10) uniform sampler2D   tBRDFLUT;
// Phase I : cubemap HDR brut (RGBA32F, sans Reinhard) pour le specular
// IBL des materiaux quasi-mirror (roughness ~ 0). Les metalliques purs
// recevront ainsi le vrai HDR > 1.0 et brilleront (bloom appliqué apres
// dans le PostProcess).
layout(set=0, binding=26) uniform samplerCube tSkyEnvCube;
// Phase H.6 : voxel AO grid (3D R8) pour cone-tracing long-range AO. Bounds
// world hardcoded (-10..+10 X/Z, -5..+5 Y). Atténue l'IBL pour les pixels
// qui ont des occluders dans leur hémisphère normale.
layout(set=0, binding=27) uniform sampler3D tVoxelOpacity;
layout(set=0, binding=11) uniform sampler2DShadow tShadowAtlas;
layout(set=0, binding=12) uniform sampler2D       tShadowAtlasRaw;
#include "Include/NkShadowAtlas.glsli"
// Phase E.6 : 8 light cookies (gobos) pour SPOT lights.
layout(set=0, binding=13) uniform sampler2D tLight3DCookie0;
layout(set=0, binding=14) uniform sampler2D tLight3DCookie1;
layout(set=0, binding=15) uniform sampler2D tLight3DCookie2;
layout(set=0, binding=16) uniform sampler2D tLight3DCookie3;
layout(set=0, binding=17) uniform sampler2D tLight3DCookie4;
layout(set=0, binding=18) uniform sampler2D tLight3DCookie5;
layout(set=0, binding=19) uniform sampler2D tLight3DCookie6;
layout(set=0, binding=20) uniform sampler2D tLight3DCookie7;

// Phase E.6b : 4 point light cookies (cubemap)
layout(set=0, binding=21) uniform samplerCube tLight3DCubeCookie0;
layout(set=0, binding=22) uniform samplerCube tLight3DCubeCookie1;
layout(set=0, binding=23) uniform samplerCube tLight3DCubeCookie2;
layout(set=0, binding=24) uniform samplerCube tLight3DCubeCookie3;

float SampleLight3DCubeCookie(int idx, vec3 dir) {
    if (idx == 0) return texture(tLight3DCubeCookie0, dir).r;
    if (idx == 1) return texture(tLight3DCubeCookie1, dir).r;
    if (idx == 2) return texture(tLight3DCubeCookie2, dir).r;
    if (idx == 3) return texture(tLight3DCubeCookie3, dir).r;
    return 1.0;
}

// Phase H.6 : Voxel AO helpers — extraits dans Material Function partagée
// pour que tous les shaders materiau (Layered, Toon, Anime, ...) puissent
// inclure le meme code.
#include "Include/NkVoxelAO.glsli"

float SampleLight3DCookie(int idx, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    if (idx == 0) return texture(tLight3DCookie0, uv).r;
    if (idx == 1) return texture(tLight3DCookie1, uv).r;
    if (idx == 2) return texture(tLight3DCookie2, uv).r;
    if (idx == 3) return texture(tLight3DCookie3, uv).r;
    if (idx == 4) return texture(tLight3DCookie4, uv).r;
    if (idx == 5) return texture(tLight3DCookie5, uv).r;
    if (idx == 6) return texture(tLight3DCookie6, uv).r;
    if (idx == 7) return texture(tLight3DCookie7, uv).r;
    return 1.0;
}

// ── PBR Functions ─────────────────────────────────────────────
const float PI = 3.14159265358979;

float D_GGX(vec3 N, vec3 H, float r) {
    float a=r*r, a2=a*a, NdH=max(dot(N,H),0.0);
    float d=NdH*NdH*(a2-1.0)+1.0;
    return a2/(PI*d*d+1e-4);
}
float G_Schlick(float x, float k) { return x/(x*(1.0-k)+k); }
float G_Smith(vec3 N, vec3 V, vec3 L, float r) {
    float k=(r+1.0)*(r+1.0)/8.0;
    return G_Schlick(max(dot(N,V),0.0),k)*G_Schlick(max(dot(N,L),0.0),k);
}
vec3 F_Schlick(float cosT, vec3 F0) {
    return F0+(1.0-F0)*pow(max(1.0-cosT,0.0),5.0);
}
vec3 F_SchlickR(float cosT, vec3 F0, float r) {
    return F0+(max(vec3(1.0-r),F0)-F0)*pow(max(1.0-cosT,0.0),5.0);
}

// ── PCSS soft shadows : blocker search + kernel adaptatif ────
const vec2 kPoissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

// Phase NkVSM : sampling shadow par light via NkShadowAtlas.glsli.
// La fonction SampleLightShadow(lightIdx, worldPos, N, L) dispatche selon le
// type de light (DIR/POINT/SPOT) et sample le tile correspondant de l'atlas.

// =============================================================================
// Triplanar projection helpers (2026-05-24)
//
// Quand uObj.triplanarParams.z > 0.5, la texture est projetee selon les 3
// plans monde (YZ, XZ, XY) et blendee par |normal|. Resultat : tiles vraiment
// carrees independant du scale/rotation, sans seams sur les coins du cube.
//
// tileSize est exprime en METRES REELS. Pour la projection world, on convertit
// en units via metersPerUnit (echelle Blender-style configurable).
//
// Pour la normal map : UDN blend (Unreal Devs Network) — chaque sample est
// reoriente selon l'axe de projection avant blend.
// =============================================================================

vec3 NkTriplanarWeights(vec3 N) {
    // Puissance 4 pour des transitions plus nettes entre axes (moins de blur).
    vec3 w = abs(N);
    w = pow(w, vec3(4.0));
    return w / max(w.x + w.y + w.z, 1e-5);
}

vec3 NkTriplanarSampleRGB(sampler2D tex, vec3 worldPos, vec3 w, float invTile) {
    // Sample sur les 3 plans monde. Pas de besoin de Y-flip car les textures
    // sont en UV standard (origin = top-left ou bottom-left selon backend, mais
    // ici c'est REPEAT donc invariant).
    vec3 cYZ = texture(tex, worldPos.zy * invTile).rgb;  // proj plan X (normal +/-X)
    vec3 cXZ = texture(tex, worldPos.xz * invTile).rgb;  // proj plan Y (normal +/-Y)
    vec3 cXY = texture(tex, worldPos.xy * invTile).rgb;  // proj plan Z (normal +/-Z)
    return cYZ * w.x + cXZ * w.y + cXY * w.z;
}

vec4 NkTriplanarSampleRGBA(sampler2D tex, vec3 worldPos, vec3 w, float invTile) {
    vec4 cYZ = texture(tex, worldPos.zy * invTile);
    vec4 cXZ = texture(tex, worldPos.xz * invTile);
    vec4 cXY = texture(tex, worldPos.xy * invTile);
    return cYZ * w.x + cXZ * w.y + cXY * w.z;
}

// UDN normal blend : reoriente chaque normal map sample selon l'axe de proj,
// puis blend par poids. Standard pour triplanar avec normal maps.
// Ref: Ben Golus "Normal Mapping for a Triplanar Shader" (2017).
vec3 NkTriplanarNormalUDN(sampler2D nMap, vec3 worldPos, vec3 N,
                          vec3 w, float invTile, float strength) {
    vec3 nYZ = texture(nMap, worldPos.zy * invTile).xyz * 2.0 - 1.0;
    vec3 nXZ = texture(nMap, worldPos.xz * invTile).xyz * 2.0 - 1.0;
    vec3 nXY = texture(nMap, worldPos.xy * invTile).xyz * 2.0 - 1.0;
    nYZ.xy *= strength;
    nXZ.xy *= strength;
    nXY.xy *= strength;
    // UDN : ajouter la geometric normal en XY de la tangent space sample,
    // puis swizzle pour aligner sur l'axe world correspondant.
    vec3 wN_YZ = vec3(nYZ.xy + N.zy, N.x * sign(N.x));   // plan X -> swizzle zy
    vec3 wN_XZ = vec3(nXZ.xy + N.xz, N.y * sign(N.y));   // plan Y -> swizzle xz
    vec3 wN_XY = vec3(nXY.xy + N.xy, N.z * sign(N.z));   // plan Z -> swizzle xy
    vec3 outN = normalize(
        vec3(wN_YZ.z, wN_YZ.y, wN_YZ.x) * w.x +
        vec3(wN_XZ.x, wN_XZ.z, wN_XZ.y) * w.y +
        vec3(wN_XY.x, wN_XY.y, wN_XY.z) * w.z
    );
    return outN;
}

void main() {
    // ── Triplanar setup ─────────────────────────────────────────
    // tileSize en metres -> en units world (divise par metersPerUnit).
    // UV = worldPos / tileSizeUnits = worldPos * invTile.
    bool  triplanarOn = uObj.triplanarParams.z > 0.5;
    float tileMeters  = max(uObj.triplanarParams.x, 1e-4);
    float mpu         = max(uObj.triplanarParams.y, 1e-4);
    float invTile     = mpu / tileMeters;   // 1 / (tileMeters / mpu)
    vec3  geomN       = normalize(vNormal);
    vec3  triW        = triplanarOn ? NkTriplanarWeights(geomN) : vec3(0.0);

    // ── Albedo + alpha ─────────────────────────────────────────
    vec4 albSample = triplanarOn
        ? NkTriplanarSampleRGBA(tAlbedo, vWorldPos, triW, invTile)
        : texture(tAlbedo, vUV);
    albSample *= vColor;
    vec3 albedo    = pow(albSample.rgb, vec3(2.2));
    float alpha    = albSample.a;
    if (alpha < 0.01) discard;

    // ── ORM + overrides ───────────────────────────────────────
    vec3 orm = triplanarOn
        ? NkTriplanarSampleRGB(tORM, vWorldPos, triW, invTile)
        : texture(tORM, vUV).rgb;
    float ao  = orm.r * uObj.aoStrength;
    float rog = orm.g * uObj.roughness;
    float met = orm.b * uObj.metallic;

    // ── Normal mapping ────────────────────────────────────────
    vec3 N;
    if (triplanarOn) {
        // UDN blend : aucune dependance vTangent/vBitangent (le cube primitive
        // a des tangents fragiles selon la face — triplanar les bypass).
        N = NkTriplanarNormalUDN(tNormal, vWorldPos, geomN, triW, invTile,
                                  uObj.normalStrength);
    } else {
        vec3 nTs = texture(tNormal, vUV).xyz * 2.0 - 1.0;
        nTs.xy  *= uObj.normalStrength;
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
        N = normalize(TBN * nTs);
    }
    // Phase Planar Reflection fix 2026-05-24 : en mirror pass, vWorldPos est en
    // espace "real geometry" (un-mirror Y fait dans le VS) -> camPos doit etre
    // un-mirror aussi pour que V represente la vue depuis la mirror cam.
    vec3 camPosForV = uCam.camPos.xyz;
    if (uCam.reflectionFlags.x > 0.5) camPosForV.y = -camPosForV.y;
    vec3 V    = normalize(camPosForV - vWorldPos);
    vec3 F0   = mix(vec3(0.04), albedo, met);

    // ── Direct lighting ───────────────────────────────────────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < uLights.count && i < 32; i++) {
        int   lt  = int(uLights.positions[i].w);
        vec3  L;
        float att = 1.0;

        if (lt == 0) {  // Directional
            L   = normalize(-uLights.directions[i].xyz);
            // Phase E.6 : cookie directional projete sur plan world (tile = range).
            int cookieIdx = int(uLights.angles[i].w);
            if (cookieIdx >= 0) {
                vec3 fwd = normalize(uLights.directions[i].xyz);
                vec3 worldUp = abs(fwd.y) > 0.99 ? vec3(1, 0, 0) : vec3(0, 1, 0);
                vec3 right = normalize(cross(worldUp, fwd));
                vec3 upL   = normalize(cross(fwd, right));
                float tile = max(uLights.directions[i].w, 0.01);
                float u = fract(dot(vWorldPos, right) / tile);
                float v = fract(dot(vWorldPos, upL)   / tile);
                att *= SampleLight3DCookie(cookieIdx, vec2(u, v));
            }
        } else {        // Point / Spot
            vec3 d = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L   = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att = att * att;
            // Phase E.6b : cookie cubemap pour POINT lights
            if (lt == 1) {
                int cookieIdx = int(uLights.angles[i].w);
                if (cookieIdx >= 0) {
                    att *= SampleLight3DCubeCookie(cookieIdx, normalize(-L));
                }
            }
            if (lt == 2) {  // Spot
                float th       = dot(L, normalize(-uLights.directions[i].xyz));
                float cosInner = uLights.angles[i].x;
                float cosOuter = uLights.angles[i].y;
                att *= clamp((th - cosOuter) / (cosInner - cosOuter + 1e-4), 0.0, 1.0);

                // Phase E.6 : light cookie projection (gobo)
                int cookieIdx = int(uLights.angles[i].w);
                if (cookieIdx >= 0) {
                    // fwd = direction du faisceau, dot(toFrag, fwd) > 0 dans le cone.
                    vec3 fwd = normalize(uLights.directions[i].xyz);
                    vec3 worldUp = abs(fwd.y) > 0.99 ? vec3(1, 0, 0) : vec3(0, 1, 0);
                    vec3 right = normalize(cross(worldUp, fwd));
                    vec3 upL   = normalize(cross(fwd, right));
                    vec3 toFrag = vWorldPos - uLights.positions[i].xyz;
                    float fwdDist = dot(toFrag, fwd);
                    if (fwdDist > 0.001) {
                        float sinO = sqrt(max(1.0 - cosOuter*cosOuter, 0.0));
                        float halfWidth = (sinO / max(cosOuter, 1e-3)) * fwdDist;
                        if (halfWidth > 1e-3) {
                            float u = dot(toFrag, right) / (2.0 * halfWidth) + 0.5;
                            float v = dot(toFrag, upL)   / (2.0 * halfWidth) + 0.5;
                            att *= SampleLight3DCookie(cookieIdx, vec2(u, v));
                        }
                    }
                }
            }
        }

        // NkVSM : shadow per-light via atlas multi-tiles. angles[i].z = castShadow flag.
        // NkVSM v1 : material override receiveShadow (.x) skip le sample
        // + shadowBiasMul (.z) scale le bias per-material.
        float csf = 1.0;
        if (uLights.angles[i].z > 0.5 && uObj.shadowOverrides.x > 0.5) {
            csf = SampleLightShadowEx(i, vWorldPos, N, L, uObj.shadowOverrides.z);
        }
        vec3 H    = normalize(V + L);
        float NdL = max(dot(N, L), 0.0);
        vec3 rad  = uLights.colors[i].rgb * uLights.colors[i].w * att;

        float NDF = D_GGX(N, H, rog);
        float G   = G_Smith(N, V, L, rog);
        vec3  F   = F_Schlick(max(dot(H,V),0.0), F0);
        vec3 spec = NDF*G*F / (4.0*max(dot(N,V),0.0)*NdL+1e-4);
        vec3 kD   = (1.0-F)*(1.0-met);
        Lo += csf * (kD*albedo/PI + spec) * rad * NdL;
    }

    // ── IBL ───────────────────────────────────────────────────
    // Phase Planar Reflection fix 2026-05-24 : en mirror pass, N et vWorldPos
    // sont deja en espace "real geometry" grace au un-mirror Y dans le VS,
    // donc R = reflect(-V, N) est en world space reel -> sample direct du
    // cubemap (qui est aussi en world space reel). Pas besoin de Y-flipper R.
    vec3 Fi   = F_SchlickR(max(dot(N,V),0.0), F0, rog);
    vec3 kDi  = (1.0-Fi)*(1.0-met);
    vec3 R    = reflect(-V, N);
    vec3 irr  = texture(tEnvIrradiance, N).rgb;
    // Phase I : mix entre cubemap HDR brut (mirror) et prefilter Reinhard (rough).
    // Le tSkyEnvCube preserve les valeurs > 1.0 du HDR original donc les
    // metalliques quasi-mirror brillent vraiment + recoivent du bloom apres
    // tonemap.
    // Range elargi (0.05 .. 0.50) pour qu'une sphere metal avec rough=0.5
    // garde un peu de contribution HDR brut, ce qui matche mieux le
    // comportement intuitif "metal brille fort en reflet".
    // - rough < 0.05  : 100% mirror (HDR brut)
    // - rough = 0.30  : 50/50 mix
    // - rough > 0.50  : 100% prefilter (Reinhard tonemape)
    vec3 prefMirror = texture(tSkyEnvCube, R).rgb;
    vec3 prefRough  = textureLod(tEnvPrefilter, R, rog*4.0).rgb;
    float mirrorMix = 1.0 - smoothstep(0.05, 0.50, rog);
    vec3 pref       = mix(prefRough, prefMirror, mirrorMix);
    vec2 brdf       = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), rog)).rg;
    // Phase H.6 : voxel cone-trace AO long-range. Atténue l'IBL irradiance
    // et specular pour les pixels qui ont des occluders dans leur hémisphère
    // (ex: sphères sous le sol qui ne devraient pas recevoir l'IBL sky).
    float voxAO = NkComputeVoxelAO(vWorldPos, N);
    vec3 amb  = (kDi*irr*albedo + pref*(Fi*brdf.x+brdf.y)) * ao * voxAO * uCam.iblStrength;

    // ── Clearcoat ─────────────────────────────────────────────
    vec3 ccContrib = vec3(0.0);
    if (uObj.clearcoat > 0.0) {
        float ccR = uObj.clearcoatRoughness;
        vec3  Fcc = F_Schlick(max(dot(N,V),0.0), vec3(0.04)) * uObj.clearcoat;
        vec3  prefCC = textureLod(tEnvPrefilter, R, ccR*4.0).rgb;
        vec2  brdfCC = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), ccR)).rg;
        ccContrib = prefCC * (Fcc * brdfCC.x + brdfCC.y);
    }

    // ── Subsurface Scattering (simple wrap) ───────────────────
    vec3 sssContrib = vec3(0.0);
    if (uObj.subsurface > 0.0) {
        for (int i = 0; i < uLights.count && i < 32; i++) {
            vec3 L = (int(uLights.positions[i].w)==0) ?
                normalize(-uLights.directions[i].xyz) :
                normalize(uLights.positions[i].xyz - vWorldPos);
            float wrap  = max(dot(N,L)+uObj.subsurface,0.0)/(1.0+uObj.subsurface);
            vec3  trans = uObj.subsurfaceColor.rgb * uLights.colors[i].rgb *
                          uLights.colors[i].w * wrap * uObj.subsurface;
            sssContrib += trans;
        }
    }

    // ── Emissive ──────────────────────────────────────────────
    vec3 emissive = texture(tEmissive, vUV).rgb * uObj.emissiveStrength;

    // ── Final ─────────────────────────────────────────────────
    vec3 color = amb + Lo + ccContrib + sssContrib + emissive;
    fragColor  = vec4(color, 1.0);
}
