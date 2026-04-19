// =============================================================================
// NkPBR_OpenGL.h
// Shaders PBR pour OpenGL 4.6 / GLSL 4.60
// Convention : binding-less (layout location uniquement, pas de set)
//              UBO au binding 0 (per-frame), binding 1 (per-draw)
//              Textures au binding 2–7
// =============================================================================
#pragma once

namespace nkentseu {
namespace renderer {
namespace shaders {
namespace opengl {

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader PBR 3D
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_Vert = R"GLSL(
#version 460 core

// ── Vertex attributes ────────────────────────────────────────────────────────
layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec3  aTangent;
layout(location = 3) in vec2  aUV;
layout(location = 4) in vec2  aUV2;
layout(location = 5) in vec4  aColor;

// ── Per-frame UBO (binding 0) ─────────────────────────────────────────────────
layout(std140, binding = 0) uniform FrameUBO {
    mat4  view;
    mat4  proj;
    mat4  viewProj;
    vec4  cameraPos;       // xyz=pos w=unused
    vec4  time;            // x=t y=dt z=unused w=unused
} frame;

// ── Per-draw UBO (binding 1) ──────────────────────────────────────────────────
layout(std140, binding = 1) uniform DrawUBO {
    mat4  model;
    mat4  modelIT;         // transposée inverse de model (pour les normales)
    vec4  objectColor;
} draw;

// ── Outputs vers le fragment shader ──────────────────────────────────────────
layout(location = 0) out vec3  vWorldPos;
layout(location = 1) out vec3  vNormal;
layout(location = 2) out vec3  vTangent;
layout(location = 3) out vec3  vBitangent;
layout(location = 4) out vec2  vUV;
layout(location = 5) out vec2  vUV2;
layout(location = 6) out vec4  vColor;

void main() {
    vec4 worldPos     = draw.model * vec4(aPosition, 1.0);
    vWorldPos         = worldPos.xyz;

    // Matrice normale : mat3 de modelIT (pas besoin de transposition supplémentaire)
    mat3 normalMat    = mat3(draw.modelIT);
    vNormal           = normalize(normalMat * aNormal);
    vTangent          = normalize(normalMat * aTangent);
    vBitangent        = cross(vNormal, vTangent);

    vUV               = aUV;
    vUV2              = aUV2;
    vColor            = aColor * draw.objectColor;

    gl_Position       = frame.proj * frame.view * worldPos;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment shader PBR 3D (metallic-roughness GGX)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_Frag = R"GLSL(
#version 460 core

layout(location = 0) in vec3  vWorldPos;
layout(location = 1) in vec3  vNormal;
layout(location = 2) in vec3  vTangent;
layout(location = 3) in vec3  vBitangent;
layout(location = 4) in vec2  vUV;
layout(location = 5) in vec2  vUV2;
layout(location = 6) in vec4  vColor;

layout(location = 0) out vec4 fragColor;

// ── Per-frame ─────────────────────────────────────────────────────────────────
layout(std140, binding = 0) uniform FrameUBO {
    mat4  view;
    mat4  proj;
    mat4  viewProj;
    vec4  cameraPos;
    vec4  time;
} frame;

// ── Material params ───────────────────────────────────────────────────────────
layout(std140, binding = 2) uniform MaterialUBO {
    vec4  baseColorFactor;    // RGBA multiplier
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    float emissiveStrength;
    vec3  emissiveFactor;
    float _pad0;
    int   useAlbedoMap;
    int   useNormalMap;
    int   useMetalRoughMap;
    int   useOcclusionMap;
    int   useEmissiveMap;
    float alphaCutoff;
    int   doubleSided;
    float _pad1;
} mat;

// ── Textures PBR (binding 3–7) ────────────────────────────────────────────────
layout(binding = 3) uniform sampler2D uAlbedoMap;
layout(binding = 4) uniform sampler2D uNormalMap;
layout(binding = 5) uniform sampler2D uMetalRoughMap;   // R=occlusion G=rough B=metal
layout(binding = 6) uniform sampler2D uOcclusionMap;
layout(binding = 7) uniform sampler2D uEmissiveMap;

// ── Lumières (jusqu'à 8 lumières ponctuelles + 1 directionnelle) ──────────────
layout(std140, binding = 8) uniform LightsUBO {
    vec4  dirLightDir;        // xyz=dir w=unused
    vec4  dirLightColor;      // xyz=color w=intensity
    vec4  pointPos[8];        // xyz=pos w=radius
    vec4  pointColor[8];      // xyz=color w=intensity
    int   numPointLights;
    int   _pad[3];
} lights;

// ── Constantes ────────────────────────────────────────────────────────────────
const float PI = 3.14159265359;

// ── GGX / Smith PBR ───────────────────────────────────────────────────────────
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlick(float NdV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlick(max(dot(N,V),0.0), roughness)
         * GeometrySchlick(max(dot(N,L),0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalcPBR(vec3 N, vec3 V, vec3 L, vec3 lightColor,
             vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NdL = max(dot(N, L), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);

    vec3  num   = NDF * G * F;
    float denom = 4.0 * max(dot(N,V),0.0) * NdL + 0.0001;
    vec3  spec  = num / denom;

    vec3  kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + spec) * lightColor * NdL;
}

void main() {
    // ── Albedo ────────────────────────────────────────────────────────────────
    vec4 albedoSample = (mat.useAlbedoMap != 0)
        ? texture(uAlbedoMap, vUV)
        : vec4(1.0);
    vec4 baseColor = albedoSample * mat.baseColorFactor * vColor;

    // Alpha cutoff (masked materials)
    if (mat.alphaCutoff > 0.0 && baseColor.a < mat.alphaCutoff) discard;

    // ── Normal map ────────────────────────────────────────────────────────────
    vec3 N = normalize(vNormal);
    if (mat.useNormalMap != 0) {
        vec3 nt = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), N);
        N = normalize(TBN * nt);
    }
    if (mat.doubleSided != 0 && !gl_FrontFacing) N = -N;

    // ── Metal / Rough ─────────────────────────────────────────────────────────
    float metallic  = mat.metallicFactor;
    float roughness = mat.roughnessFactor;
    if (mat.useMetalRoughMap != 0) {
        vec3 mr = texture(uMetalRoughMap, vUV).rgb;
        metallic  *= mr.b;
        roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    // ── AO ────────────────────────────────────────────────────────────────────
    float ao = 1.0;
    if (mat.useOcclusionMap != 0) {
        ao = mix(1.0, texture(uOcclusionMap, vUV2).r, mat.occlusionStrength);
    }

    vec3 V       = normalize(frame.cameraPos.xyz - vWorldPos);
    vec3 albedo  = baseColor.rgb;
    vec3 Lo      = vec3(0.0);

    // ── Lumière directionnelle ────────────────────────────────────────────────
    if (lights.dirLightColor.w > 0.001) {
        vec3 L = normalize(-lights.dirLightDir.xyz);
        Lo += CalcPBR(N, V, L,
                      lights.dirLightColor.xyz * lights.dirLightColor.w,
                      albedo, metallic, roughness);
    }

    // ── Lumières ponctuelles ──────────────────────────────────────────────────
    for (int i = 0; i < min(lights.numPointLights, 8); ++i) {
        vec3 toLight = lights.pointPos[i].xyz - vWorldPos;
        float dist   = length(toLight);
        float radius = max(lights.pointPos[i].w, 0.001);
        float atten  = clamp(1.0 - (dist / radius) * (dist / radius), 0.0, 1.0);
        atten *= atten;
        if (atten < 0.0001) continue;
        vec3 L = toLight / dist;
        Lo += CalcPBR(N, V, L,
                      lights.pointColor[i].xyz * lights.pointColor[i].w * atten,
                      albedo, metallic, roughness);
    }

    // ── Ambiance (IBL approx.) ────────────────────────────────────────────────
    vec3 ambient = vec3(0.03) * albedo * ao;

    // ── Émission ──────────────────────────────────────────────────────────────
    vec3 emissive = mat.emissiveFactor * mat.emissiveStrength;
    if (mat.useEmissiveMap != 0) {
        emissive *= texture(uEmissiveMap, vUV).rgb;
    }

    vec3 color = ambient + Lo + emissive;

    // Tone mapping (Reinhard) + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, baseColor.a);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader Unlit 3D
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kUnlit_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec2 aUV;
layout(location = 5) in vec4 aColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4  view; mat4  proj; mat4  viewProj; vec4  cameraPos; vec4  time;
} frame;
layout(std140, binding = 1) uniform DrawUBO {
    mat4  model; mat4  modelIT; vec4  objectColor;
} draw;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vUV       = aUV;
    vColor    = aColor * draw.objectColor;
    gl_Position = frame.viewProj * draw.model * vec4(aPosition, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment shader Unlit
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kUnlit_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 2) uniform MaterialUBO {
    vec4  baseColorFactor;
    float _pad[12];
    int   useAlbedoMap;
    float alphaCutoff;
    int   _pad2[2];
} mat;

layout(binding = 3) uniform sampler2D uAlbedoMap;

void main() {
    vec4 tex = (mat.useAlbedoMap != 0) ? texture(uAlbedoMap, vUV) : vec4(1.0);
    vec4 col = tex * mat.baseColorFactor * vColor;
    if (mat.alphaCutoff > 0.0 && col.a < mat.alphaCutoff) discard;
    fragColor = col;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader 2D (sprites, UI overlay)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* k2D_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(std140, binding = 0) uniform ViewportUBO {
    vec2  viewport;  // w, h en pixels
    vec2  _pad;
    mat4  projection;
} vp;

layout(std140, binding = 1) uniform DrawUBO {
    mat4  transform;  // 4x4 pour transformer la géométrie 2D
    vec4  objectColor;
} draw;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vUV      = aUV;
    vColor   = aColor * draw.objectColor;
    gl_Position = vp.projection * draw.transform * vec4(aPosition, 0.0, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment shader 2D
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* k2D_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 3) uniform sampler2D uTexture;
layout(std140, binding = 2) uniform MaterialUBO {
    vec4 tintColor;
    int  useTexture;
    float _pad[3];
} mat;

void main() {
    vec4 tex = (mat.useTexture != 0) ? texture(uTexture, vUV) : vec4(1.0);
    fragColor = tex * vColor * mat.tintColor;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex/Fragment shadow depth pass (OpenGL)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kShadow_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(std140, binding = 0) uniform ShadowUBO {
    mat4 lightVP;
    mat4 model;
} shadow;
void main() {
    gl_Position = shadow.lightVP * shadow.model * vec4(aPosition, 1.0);
}
)GLSL";

inline constexpr const char* kShadow_Frag = R"GLSL(
#version 460 core
void main() {}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex/Fragment debug wireframe (lignes colorées)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kDebug_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(std140, binding = 0) uniform FrameUBO {
    mat4 view; mat4 proj; mat4 viewProj; vec4 cameraPos; vec4 time;
} frame;
layout(location = 0) out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = frame.viewProj * vec4(aPosition, 1.0);
}
)GLSL";

inline constexpr const char* kDebug_Frag = R"GLSL(
#version 460 core
layout(location = 0) in  vec4 vColor;
layout(location = 0) out vec4 fragColor;
void main() { fragColor = vColor; }
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment shader texte (atlas de glyphes)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kText_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 3) uniform sampler2D uFontAtlas;
void main() {
    float alpha = texture(uFontAtlas, vUV).r;
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Toon shading
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kToon_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 6) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 view; mat4 proj; mat4 viewProj; vec4 cameraPos; vec4 time;
} frame;
layout(std140, binding = 8) uniform LightsUBO {
    vec4 dirLightDir;
    vec4 dirLightColor;
    vec4 pointPos[8];
    vec4 pointColor[8];
    int  numPointLights;
    int  _pad[3];
} lights;

float quantize(float v, float steps) {
    return floor(v * steps + 0.5) / steps;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-lights.dirLightDir.xyz);
    float NdL = max(dot(N, L), 0.0);

    // Quantification Toon 4 niveaux
    float toon = quantize(NdL, 4.0);

    // Outline rim
    vec3 V = normalize(frame.cameraPos.xyz - vWorldPos);
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = step(0.6, rim);

    vec3 color = vColor.rgb * (0.2 + toon * 0.8) * lights.dirLightColor.xyz;
    color = mix(color, vec3(0.0), rim * 0.8);
    fragColor = vec4(color, vColor.a);
}
)GLSL";

} // namespace opengl
} // namespace shaders
} // namespace renderer
} // namespace nkentseu