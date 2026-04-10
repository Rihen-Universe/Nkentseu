// =============================================================================
// NkShaders_Vulkan.h
// Shaders PBR pour Vulkan — GLSL avec décorateurs set/binding.
// Compilés en SPIR-V via NkShaderConverter (glslang).
//
// Convention Vulkan :
//   set = 0 → ressources par frame (FrameUBO, Lights)
//   set = 1 → ressources par pass / matériau (MaterialUBO, textures PBR)
//   set = 2 → ressources par draw (DrawUBO)
// Y-flip : gl_Position.y = -gl_Position.y (convention Vulkan NDC)
// Depth  : Z ∈ [0, 1] — pas de remap nécessaire
// =============================================================================
#pragma once

namespace nkentseu {
namespace renderer {
namespace shaders {
namespace vulkan {

// ─────────────────────────────────────────────────────────────────────────────
// Vertex shader PBR 3D — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_Vert = R"GLSL(
#version 460

// ── Vertex inputs ────────────────────────────────────────────────────────────
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec2 aUV;
layout(location = 4) in vec2 aUV2;
layout(location = 5) in vec4 aColor;

// ── Per-frame (set=0, binding=0) ─────────────────────────────────────────────
layout(set = 0, binding = 0, std140) uniform FrameUBO {
    mat4  view;
    mat4  proj;
    mat4  viewProj;
    vec4  cameraPos;
    vec4  time;
} frame;

// ── Per-draw (set=2, binding=0) ───────────────────────────────────────────────
layout(set = 2, binding = 0, std140) uniform DrawUBO {
    mat4  model;
    mat4  modelIT;
    vec4  objectColor;
} draw;

// ── Outputs ───────────────────────────────────────────────────────────────────
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vTangent;
layout(location = 3) out vec3 vBitangent;
layout(location = 4) out vec2 vUV;
layout(location = 5) out vec2 vUV2;
layout(location = 6) out vec4 vColor;

void main() {
    vec4 worldPos   = draw.model * vec4(aPosition, 1.0);
    vWorldPos       = worldPos.xyz;

    mat3 normalMat  = mat3(draw.modelIT);
    vNormal         = normalize(normalMat * aNormal);
    vTangent        = normalize(normalMat * aTangent);
    vBitangent      = cross(vNormal, vTangent);

    vUV             = aUV;
    vUV2            = aUV2;
    vColor          = aColor * draw.objectColor;

    vec4 clip       = frame.proj * frame.view * worldPos;
    clip.y          = -clip.y;    // Y-flip Vulkan
    gl_Position     = clip;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment shader PBR 3D — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kPBR_Frag = R"GLSL(
#version 460

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vTangent;
layout(location = 3) in vec3 vBitangent;
layout(location = 4) in vec2 vUV;
layout(location = 5) in vec2 vUV2;
layout(location = 6) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

// ── Per-frame ─────────────────────────────────────────────────────────────────
layout(set = 0, binding = 0, std140) uniform FrameUBO {
    mat4  view; mat4  proj; mat4  viewProj; vec4  cameraPos; vec4  time;
} frame;

// ── Lights (set=0, binding=1) ─────────────────────────────────────────────────
layout(set = 0, binding = 1, std140) uniform LightsUBO {
    vec4  dirLightDir;
    vec4  dirLightColor;
    vec4  pointPos[8];
    vec4  pointColor[8];
    int   numPointLights;
    int   _pad[3];
} lights;

// ── Material (set=1, binding=0) ───────────────────────────────────────────────
layout(set = 1, binding = 0, std140) uniform MaterialUBO {
    vec4  baseColorFactor;
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

// ── Textures PBR (set=1, binding=1–5) ────────────────────────────────────────
layout(set = 1, binding = 1) uniform sampler2D uAlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D uNormalMap;
layout(set = 1, binding = 3) uniform sampler2D uMetalRoughMap;
layout(set = 1, binding = 4) uniform sampler2D uOcclusionMap;
layout(set = 1, binding = 5) uniform sampler2D uEmissiveMap;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlick(float NdV, float roughness) {
    float r = roughness + 1.0;
    float k = (r*r) / 8.0;
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
    vec3 H   = normalize(V + L);
    float NdL= max(dot(N, L), 0.0);
    vec3 F0  = mix(vec3(0.04), albedo, metallic);
    vec3 F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF= DistributionGGX(N, H, roughness);
    float G  = GeometrySmith(N, V, L, roughness);
    vec3 spec= (NDF * G * F) / (4.0 * max(dot(N,V),0.0) * NdL + 0.0001);
    vec3 kD  = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + spec) * lightColor * NdL;
}

void main() {
    vec4 baseColor = ((mat.useAlbedoMap != 0) ? texture(uAlbedoMap, vUV) : vec4(1.0))
                   * mat.baseColorFactor * vColor;
    if (mat.alphaCutoff > 0.0 && baseColor.a < mat.alphaCutoff) discard;

    vec3 N = normalize(vNormal);
    if (mat.useNormalMap != 0) {
        vec3 nt  = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), N);
        N = normalize(TBN * nt);
    }

    float metallic  = mat.metallicFactor;
    float roughness = mat.roughnessFactor;
    if (mat.useMetalRoughMap != 0) {
        vec3 mr = texture(uMetalRoughMap, vUV).rgb;
        metallic *= mr.b; roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    float ao = 1.0;
    if (mat.useOcclusionMap != 0)
        ao = mix(1.0, texture(uOcclusionMap, vUV2).r, mat.occlusionStrength);

    vec3 V  = normalize(frame.cameraPos.xyz - vWorldPos);
    vec3 Lo = vec3(0.0);

    if (lights.dirLightColor.w > 0.001)
        Lo += CalcPBR(N, V, normalize(-lights.dirLightDir.xyz),
                     lights.dirLightColor.xyz * lights.dirLightColor.w,
                     baseColor.rgb, metallic, roughness);

    for (int i = 0; i < min(lights.numPointLights, 8); ++i) {
        vec3 d = lights.pointPos[i].xyz - vWorldPos;
        float dist = length(d);
        float r = max(lights.pointPos[i].w, 0.001);
        float a = clamp(1.0 - pow(dist/r, 2.0), 0.0, 1.0);
        a *= a;
        if (a < 0.0001) continue;
        Lo += CalcPBR(N, V, d/dist,
                     lights.pointColor[i].xyz * lights.pointColor[i].w * a,
                     baseColor.rgb, metallic, roughness);
    }

    vec3 ambient = vec3(0.03) * baseColor.rgb * ao;
    vec3 emissive = mat.emissiveFactor * mat.emissiveStrength;
    if (mat.useEmissiveMap != 0) emissive *= texture(uEmissiveMap, vUV).rgb;

    vec3 color = (ambient + Lo + emissive) / (ambient + Lo + emissive + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    fragColor = vec4(color, baseColor.a);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex/Fragment Unlit — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kUnlit_Vert = R"GLSL(
#version 460
layout(location=0) in vec3 aPosition;
layout(location=3) in vec2 aUV;
layout(location=5) in vec4 aColor;
layout(set=0,binding=0,std140) uniform FrameUBO { mat4 view; mat4 proj; mat4 viewProj; vec4 cameraPos; vec4 time; } frame;
layout(set=2,binding=0,std140) uniform DrawUBO  { mat4 model; mat4 modelIT; vec4 objectColor; } draw;
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor * draw.objectColor;
    vec4 clip = frame.viewProj * draw.model * vec4(aPosition, 1.0);
    clip.y = -clip.y;
    gl_Position = clip;
}
)GLSL";

inline constexpr const char* kUnlit_Frag = R"GLSL(
#version 460
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0,std140) uniform MaterialUBO { vec4 baseColorFactor; float _pad[3]; int useAlbedoMap; float alphaCutoff; int _pad2[2]; } mat;
layout(set=1,binding=1) uniform sampler2D uAlbedoMap;
void main() {
    vec4 tex = (mat.useAlbedoMap != 0) ? texture(uAlbedoMap, vUV) : vec4(1.0);
    vec4 col = tex * mat.baseColorFactor * vColor;
    if (mat.alphaCutoff > 0.0 && col.a < mat.alphaCutoff) discard;
    fragColor = col;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Vertex/Fragment 2D — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* k2D_Vert = R"GLSL(
#version 460
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(set=0,binding=0,std140) uniform ViewportUBO { vec2 viewport; vec2 _pad; mat4 projection; } vp;
layout(set=2,binding=0,std140) uniform DrawUBO { mat4 transform; vec4 objectColor; } draw;
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor * draw.objectColor;
    vec4 clip = vp.projection * draw.transform * vec4(aPosition, 0.0, 1.0);
    clip.y = -clip.y;
    gl_Position = clip;
}
)GLSL";

inline constexpr const char* k2D_Frag = R"GLSL(
#version 460
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0,std140) uniform MaterialUBO { vec4 tintColor; int useTexture; float _pad[3]; } mat;
layout(set=1,binding=1) uniform sampler2D uTexture;
void main() {
    vec4 tex = (mat.useTexture != 0) ? texture(uTexture, vUV) : vec4(1.0);
    fragColor = tex * vColor * mat.tintColor;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Shadow depth pass — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kShadow_Vert = R"GLSL(
#version 460
layout(location=0) in vec3 aPosition;
layout(set=0,binding=0,std140) uniform ShadowUBO { mat4 lightVP; mat4 model; } shadow;
void main() {
    vec4 clip = shadow.lightVP * shadow.model * vec4(aPosition, 1.0);
    clip.y = -clip.y;
    gl_Position = clip;
}
)GLSL";

inline constexpr const char* kShadow_Frag = R"GLSL(
#version 460
void main() {}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Debug wireframe — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kDebug_Vert = R"GLSL(
#version 460
layout(location=0) in vec3 aPosition;
layout(location=1) in vec4 aColor;
layout(set=0,binding=0,std140) uniform FrameUBO { mat4 view; mat4 proj; mat4 viewProj; vec4 cameraPos; vec4 time; } frame;
layout(location=0) out vec4 vColor;
void main() {
    vColor = aColor;
    vec4 clip = frame.viewProj * vec4(aPosition, 1.0);
    clip.y = -clip.y;
    gl_Position = clip;
}
)GLSL";

inline constexpr const char* kDebug_Frag = R"GLSL(
#version 460
layout(location=0) in vec4 vColor;
layout(location=0) out vec4 fragColor;
void main() { fragColor = vColor; }
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Text atlas — Vulkan
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr const char* kText_Frag = R"GLSL(
#version 460
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=1) uniform sampler2D uFontAtlas;
void main() {
    float alpha = texture(uFontAtlas, vUV).r;
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)GLSL";

} // namespace vulkan
} // namespace shaders
} // namespace renderer
} // namespace nkentseu