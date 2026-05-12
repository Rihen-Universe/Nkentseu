// =============================================================================
// NkRender3D_PBRShaders.inl
// Shaders PBR embarques en raw string pour eviter une dependance file-system.
// Contenu identique a Kernel/Runtime/NKRenderer/src/NKRenderer/Shaders/PBR/GL/{pbr.vert,pbr.frag}.gl.glsl
// (gardes en sync manuellement pour l'instant).
// =============================================================================

namespace {

static const char* kPBR_VS = R"GLSL(
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos;
    vec4  camDir;
    vec2  viewport;
    float time;
    float deltaTime;
    float iblStrength;
} uCam;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model;
    mat4  normalMatrix;
    vec4  tint;
    float metallic;
    float roughness;
    float aoStrength;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRoughness;
    float subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(std140, binding=3) uniform ShadowUBO {
    mat4  cascadeMats[4];
    float cascadeSplits[4];
    vec4  cascadeTileBounds[4];
    int   cascadeCount;
    float shadowBias;
    float normalBias;
    int   softShadows;
    float softness;
    float _pad0, _pad1, _pad2;
} uShadow;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;
layout(location=7) out vec4 vShadowCoord[4];

void main() {
    vec4 worldPos  = uObj.model * vec4(aPos, 1.0);
    vWorldPos      = worldPos.xyz;

    mat3 nm        = mat3(uObj.normalMatrix);
    vNormal        = normalize(nm * aNormal);

    // Construction robuste du repere TBN : si aTangent est degenere
    // (length=0, cas typique des meshes generes proceduralement comme
    // icosphere), on construit un tangent orthogonal a vNormal via
    // un axe de fallback. Sans ca, normalize((0,0,0)) -> NaN -> TBN
    // casse -> N=NaN dans le FS -> couleur noire.
    if (dot(aTangent, aTangent) > 1e-6) {
        vTangent = normalize(nm * aTangent);
        // Re-orthogonalize against vNormal (Gram-Schmidt) au cas ou
        // les transformations non-uniformes auraient deformer le frame
        vTangent = normalize(vTangent - dot(vTangent, vNormal) * vNormal);
    } else {
        // Choix de l'axe de fallback : si vNormal est presque vertical,
        // on prend X comme reference, sinon Y. Garantit que le cross
        // produit n'est jamais nul.
        vec3 fallback = abs(vNormal.y) < 0.99 ? vec3(0,1,0) : vec3(1,0,0);
        vTangent = normalize(cross(fallback, vNormal));
    }
    vBitangent = cross(vNormal, vTangent);

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    for (int c = 0; c < 4; c++)
        vShadowCoord[c] = uShadow.cascadeMats[c] * worldPos;

    gl_Position = uCam.viewProj * worldPos;
}
)GLSL";

static const char* kPBR_FS = R"GLSL(
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;
layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;
layout(location=7) in vec4 vShadowCoord[4];

layout(location=0) out vec4 fragColor;

layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos;
    vec4  camDir;
    vec2  viewport;
    float time;
    float deltaTime;
    float iblStrength;
} uCam;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model, normalMatrix; vec4 tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(std140, binding=2) uniform LightsUBO {
    vec4 positions[32], colors[32], directions[32], angles[32];
    int count; int _pad[3];
} uLights;

layout(std140, binding=3) uniform ShadowUBO {
    mat4  cascadeMats[4];
    float cascadeSplits[4];
    vec4  cascadeTileBounds[4];   // xy=min UV, zw=max UV (atlas region per cascade)
    int   cascadeCount;
    float shadowBias;
    float normalBias;
    int   softShadows;
} uShadow;

layout(binding=4)  uniform sampler2D   tAlbedo;
layout(binding=5)  uniform sampler2D   tNormal;
layout(binding=6)  uniform sampler2D   tORM;
layout(binding=7)  uniform sampler2D   tEmissive;
layout(binding=8)  uniform samplerCube tEnvIrradiance;
layout(binding=9)  uniform samplerCube tEnvPrefilter;
layout(binding=10) uniform sampler2D   tBRDFLUT;
layout(binding=11) uniform sampler2DShadow tShadowMap;
layout(binding=12) uniform sampler2D       tShadowMapRaw;

// Phase E.6 : 8 light cookies (gobos) pour SPOT lights principalement.
layout(binding=13) uniform sampler2D tLight3DCookie0;
layout(binding=14) uniform sampler2D tLight3DCookie1;
layout(binding=15) uniform sampler2D tLight3DCookie2;
layout(binding=16) uniform sampler2D tLight3DCookie3;
layout(binding=17) uniform sampler2D tLight3DCookie4;
layout(binding=18) uniform sampler2D tLight3DCookie5;
layout(binding=19) uniform sampler2D tLight3DCookie6;
layout(binding=20) uniform sampler2D tLight3DCookie7;

// Phase E.6b : 4 point light cookies (cubemap). Index dans NkLightDesc::cookieIdx
// quand le light type = NK_POINT (lt=1). Sample avec direction normalize(toFrag).
layout(binding=21) uniform samplerCube tLight3DCubeCookie0;
layout(binding=22) uniform samplerCube tLight3DCubeCookie1;
layout(binding=23) uniform samplerCube tLight3DCubeCookie2;
layout(binding=24) uniform samplerCube tLight3DCubeCookie3;

float SampleLight3DCubeCookie(int idx, vec3 dir) {
    if (idx == 0) return texture(tLight3DCubeCookie0, dir).r;
    if (idx == 1) return texture(tLight3DCubeCookie1, dir).r;
    if (idx == 2) return texture(tLight3DCubeCookie2, dir).r;
    if (idx == 3) return texture(tLight3DCubeCookie3, dir).r;
    return 1.0;
}

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

// ── Disk Poisson samples (16 taps) pour blocker search + PCF adaptatif ──
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

// PCSS step 1 : blocker search. Sample N taps autour de p.xy, compte ceux
// dont la profondeur stockee est plus proche de la lumiere que le receiver.
// Retourne avgBlockerDepth (ou -1 si aucun blocker).
float FindBlockerDepth(vec2 uv, float receiverDepth, float searchRadius,
                        vec2 tileMin, vec2 tileMax, vec2 ts) {
    vec2 safeMin = tileMin + ts;
    vec2 safeMax = tileMax - ts;
    float blockerSum = 0.0;
    int   blockerCnt = 0;
    for (int i = 0; i < 16; i++) {
        vec2 sUV = clamp(uv + kPoissonDisk[i] * searchRadius, safeMin, safeMax);
        float storedDepth = texture(tShadowMapRaw, sUV).r;
        if (storedDepth < receiverDepth) {
            blockerSum += storedDepth;
            blockerCnt++;
        }
    }
    if (blockerCnt == 0) return -1.0;
    return blockerSum / float(blockerCnt);
}

// PCSS step 3 : PCF avec kernel adaptatif (radius en UV space).
float PCFAdaptive(int cascade, vec2 uv, float refDepth, float radius,
                   vec2 tileMin, vec2 tileMax, vec2 ts) {
    vec2 safeMin = tileMin + ts;
    vec2 safeMax = tileMax - ts;
    // Limite minimale : si radius est tres petit (proche du blocker), on tombe
    // sur du PCF 1-tap dur. On force au moins 1 texel pour garder un peu de
    // softness anti-alias.
    radius = max(radius, ts.x);
    float s = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 sUV = clamp(uv + kPoissonDisk[i] * radius, safeMin, safeMax);
        s += texture(tShadowMap, vec3(sUV, refDepth));
    }
    return s / 16.0;
}

float ShadowPCF(int cascade, vec4 coord, float bias) {
    if (cascade >= uShadow.cascadeCount) return 1.0;
    vec3 p = coord.xyz / coord.w;
    p.xy   = p.xy * 0.5 + 0.5;
    p.z    = p.z  * 0.5 + 0.5 - bias;
    vec2 tileMin = uShadow.cascadeTileBounds[cascade].xy;
    vec2 tileMax = uShadow.cascadeTileBounds[cascade].zw;
    if (any(lessThan(p.xy, tileMin)) || any(greaterThan(p.xy, tileMax))) return 1.0;
    if (p.z < 0.0 || p.z > 1.0) return 1.0;

    vec2 ts = 1.0 / vec2(textureSize(tShadowMap, 0));

    // Mode PCSS : softShadows=1 active blocker search + kernel adaptatif.
    // Mode PCF dur : softShadows=0 -> kernel fixe 3x3 (legacy).
    if (uShadow.softShadows != 0) {
        // PCF Poisson 16-tap avec kernel = softness en UV space.
        // Le PCSS complet (blocker search) est skippe : le sampling raw du
        // depth atlas comme sampler2D pose probleme sur certaines drivers,
        // a fixer en D.3d.1.
        float radius = max(uShadow.softness, ts.x);
        return PCFAdaptive(cascade, p.xy, p.z, radius, tileMin, tileMax, ts);
    } else {
        // PCF 3x3 dur (fallback)
        vec2 safeMin = tileMin + ts;
        vec2 safeMax = tileMax - ts;
        float s = 0.0;
        for (int x=-1;x<=1;x++) for (int y=-1;y<=1;y++) {
            vec2 uv = clamp(p.xy + vec2(x,y) * ts, safeMin, safeMax);
            s += texture(tShadowMap, vec3(uv, p.z));
        }
        return s / 9.0;
    }
}

float GetShadow() {
    if (uShadow.cascadeCount <= 0) return 1.0;
    float depth = (uCam.view * vec4(vWorldPos,1.0)).z;

    // Slope-scaled bias : plus la surface est rasante / parallele aux rayons
    // lumiere (NdL faible), plus l'erreur de profondeur entre fragments adjacents
    // devient grande -> on doit pousser le bias proportionnellement pour eviter
    // le shadow acne (pattern moire) sur les surfaces inclinees sans pour autant
    // creer de peter-panning sur les faces directement face a la lumiere (NdL=1).
    vec3 N = normalize(vNormal);
    vec3 L = vec3(0.0, 1.0, 0.0);   // fallback up si pas de directional
    if (uLights.count > 0 && int(uLights.positions[0].w) == 0) {
        L = normalize(-uLights.directions[0].xyz);
    }
    float NdL  = max(dot(N, L), 0.0);
    float bias = uShadow.shadowBias * max(1.0, 4.0 * (1.0 - NdL));

    for (int c = 0; c < uShadow.cascadeCount; c++) {
        if (depth > -uShadow.cascadeSplits[c])
            return ShadowPCF(c, vShadowCoord[c], bias);
    }
    return 1.0;
}

void main() {
    vec4 albSample = texture(tAlbedo, vUV) * vColor;
    vec3 albedo    = pow(albSample.rgb, vec3(2.2));
    float alpha    = albSample.a;
    if (alpha < 0.01) discard;

    vec3 orm  = texture(tORM, vUV).rgb;
    float ao  = orm.r * uObj.aoStrength;
    float rog = orm.g * uObj.roughness;
    float met = orm.b * uObj.metallic;

    vec3 nTs  = texture(tNormal, vUV).xyz * 2.0 - 1.0;
    nTs.xy   *= uObj.normalStrength;
    mat3 TBN  = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    vec3 N    = normalize(TBN * nTs);
    vec3 V    = normalize(uCam.camPos.xyz - vWorldPos);
    vec3 F0   = mix(vec3(0.04), albedo, met);

    vec3 Lo = vec3(0.0);
    float shadow = GetShadow();

    for (int i = 0; i < uLights.count && i < 32; i++) {
        int   lt  = int(uLights.positions[i].w);
        vec3  L;
        float att = 1.0;

        if (lt == 0) {
            L   = normalize(-uLights.directions[i].xyz);
            // Phase E.6 : cookie pour DIRECTIONAL — projection sur plan world
            // perpendiculaire a L, avec tile size = range (reuse). Pattern
            // repete pour faire des "cloud shadows" / "leaves shadows".
            int cookieIdx = int(uLights.angles[i].w);
            if (cookieIdx >= 0) {
                vec3 fwd = normalize(uLights.directions[i].xyz);
                vec3 worldUp = abs(fwd.y) > 0.99 ? vec3(1, 0, 0) : vec3(0, 1, 0);
                vec3 right = normalize(cross(worldUp, fwd));
                vec3 upL   = normalize(cross(fwd, right));
                float tile = max(uLights.directions[i].w, 0.01);   // range = tile world-units
                float u = fract(dot(vWorldPos, right) / tile);
                float v = fract(dot(vWorldPos, upL)   / tile);
                att *= SampleLight3DCookie(cookieIdx, vec2(u, v));
            }
        } else {
            vec3 d = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L   = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att = att * att;
            // Phase E.6b : cookie cubemap pour POINT lights (lt=1). cookieIdx
            // dans angles.w refere l'index dans tLight3DCubeCookie*. Direction
            // utilisee = light->fragment (-L). Permet faisceau directionnel
            // en omnidirection (lanterne avec vitrail, pulse magique etc).
            if (lt == 1) {
                int cookieIdx = int(uLights.angles[i].w);
                if (cookieIdx >= 0) {
                    att *= SampleLight3DCubeCookie(cookieIdx, normalize(-L));
                }
            }
            if (lt == 2) {
                // angles.x = cos(inner), angles.y = cos(outer) (precompute CPU)
                float th       = dot(L, normalize(-uLights.directions[i].xyz));
                float cosInner = uLights.angles[i].x;
                float cosOuter = uLights.angles[i].y;
                att *= clamp((th - cosOuter) / (cosInner - cosOuter + 1e-4), 0.0, 1.0);

                // Phase E.6 : light cookie projection (gobo). cookieIdx in
                // angles.w (-1 = pas de cookie). Projete vWorldPos dans le
                // repere local de la light et sample le motif en UV[0,1].
                int cookieIdx = int(uLights.angles[i].w);
                if (cookieIdx >= 0) {
                    // fwd = direction du faisceau (light vers scene). dot(toFrag, fwd)
                    // doit etre POSITIF pour un fragment dans le cone.
                    vec3 fwd = normalize(uLights.directions[i].xyz);
                    vec3 worldUp = abs(fwd.y) > 0.99 ? vec3(1, 0, 0) : vec3(0, 1, 0);
                    vec3 right = normalize(cross(worldUp, fwd));
                    vec3 upL   = normalize(cross(fwd, right));
                    vec3 toFrag = vWorldPos - uLights.positions[i].xyz;
                    float fwdDist = dot(toFrag, fwd);
                    if (fwdDist > 0.001) {
                        // Demi-largeur du cone a cette distance (utilise outerAngle
                        // reconstitue depuis cosOuter via tan = sqrt(1-c^2)/c).
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

        float csf = (uLights.angles[i].z > 0.5) ? shadow : 1.0;
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

    vec3 Fi   = F_SchlickR(max(dot(N,V),0.0), F0, rog);
    vec3 kDi  = (1.0-Fi)*(1.0-met);
    vec3 irr  = texture(tEnvIrradiance, N).rgb;
    vec3 R    = reflect(-V, N);
    vec3 pref = textureLod(tEnvPrefilter, R, rog*4.0).rgb;
    vec2 brdf = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), rog)).rg;
    vec3 amb  = (kDi*irr*albedo + pref*(Fi*brdf.x+brdf.y)) * ao * uCam.iblStrength;

    vec3 ccContrib = vec3(0.0);
    if (uObj.clearcoat > 0.0) {
        float ccR = uObj.clearcoatRoughness;
        vec3  Fcc = F_Schlick(max(dot(N,V),0.0), vec3(0.04)) * uObj.clearcoat;
        vec3  prefCC = textureLod(tEnvPrefilter, R, ccR*4.0).rgb;
        vec2  brdfCC = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), ccR)).rg;
        ccContrib = prefCC * (Fcc * brdfCC.x + brdfCC.y);
    }

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

    vec3 emissive = texture(tEmissive, vUV).rgb * uObj.emissiveStrength;

    vec3 color = amb + Lo + ccContrib + sssContrib + emissive;
    fragColor  = vec4(color, alpha);
}
)GLSL";

// =============================================================================
// Shaders shadow pass (D.3b) : depth-only render depuis la perspective de la
// lumiere directionnelle. Reutilise ObjectUBO (binding=1) pour le model — meme
// layout que le shader PBR principal. La lightVP est passee en push constant.
// =============================================================================
static const char* kShadow_VS = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model;
    mat4  normalMatrix;
    vec4  tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

uniform vec4 _PushConstants[4];   // mat4 lightVP en 4 vec4 column-major

void main() {
    mat4 lightVP = mat4(_PushConstants[0], _PushConstants[1],
                        _PushConstants[2], _PushConstants[3]);
    gl_Position = lightVP * uObj.model * vec4(aPos, 1.0);
}
)GLSL";

static const char* kShadow_FS = R"GLSL(
#version 460 core
// Depth-only : pas d'output color. GL ecrit gl_FragDepth implicite.
void main() {}
)GLSL";

} // anonymous namespace
