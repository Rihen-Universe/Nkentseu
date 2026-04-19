// =============================================================================
// pbr_metallic.gl.frag — Shader Fragment PBR Metallic-Roughness OpenGL 4.6
// Implémente le modèle PBR Cook-Torrance avec IBL, shadow maps et émission.
// =============================================================================
#version 460 core

// ── Entrées depuis le vertex shader ──────────────────────────────────────────
in VS_OUT {
    vec3  fragPos;
    vec3  normal;
    vec3  tangent;
    vec3  bitangent;
    vec2  uv;
    vec4  color;
    vec4  fragPosLightSpace;
} fsIn;

// ── Samplers ──────────────────────────────────────────────────────────────────
layout(binding=0) uniform sampler2D uAlbedoMap;
layout(binding=1) uniform sampler2D uNormalMap;
layout(binding=2) uniform sampler2D uORMMap;        // Occlusion/Roughness/Metallic
layout(binding=3) uniform sampler2D uEmissiveMap;
layout(binding=4) uniform sampler2D uShadowMap;
layout(binding=5) uniform samplerCube uEnvIrradiance;
layout(binding=6) uniform samplerCube uEnvPrefilter;
layout(binding=7) uniform sampler2D   uBRDF_LUT;

// ── UBO Scène ────────────────────────────────────────────────────────────────
layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel;
    mat4 uView;
    mat4 uProj;
    mat4 uLightVP;
    vec4 uLightDir;
    vec4 uEyePos;
    float uTime;
    float uDeltaTime;
    float uNdcZScale;
    float uNdcZOffset;
};

// ── UBO Matériau ─────────────────────────────────────────────────────────────
layout(std140, binding=2) uniform MaterialUBO {
    vec4  uBaseColor;
    vec4  uEmissiveColor;
    float uMetallic;
    float uRoughness;
    float uOcclusion;
    float uNormalScale;
    float uAlphaClip;
    float uEmissiveScale;
    float uClearcoat;
    float uClearcoatRoughness;
    float uTransmission;
    float uIOR;
    float _pad0, _pad1;
};

// ── Sortie ────────────────────────────────────────────────────────────────────
out vec4 FragColor;

// =============================================================================
// CONSTANTES
// =============================================================================
const float PI     = 3.14159265358979323846;
const float EPSILON= 0.0001;

// =============================================================================
// FONCTIONS PBR
// =============================================================================

// Distribution des microfacettes — GGX / Trowbridge-Reitz
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a   = roughness * roughness;
    float a2  = a * a;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH*NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Géométrie — Schlick-GGX
float GeometrySchlickGGX(float NdV, float roughness) {
    float r = roughness + 1.0;
    float k = (r*r) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

// Géométrie — Smith (combinaison view + light)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdV = max(dot(N, V), 0.0);
    float NdL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdV, roughness) * GeometrySchlickGGX(NdL, roughness);
}

// Fresnel — Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel avec roughness (pour IBL)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// =============================================================================
// SHADOW MAP — PCF 3×3
// =============================================================================
float ComputeShadow(vec4 fragPosLS, vec3 N, vec3 L) {
    // Projection en [0,1]
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * uNdcZScale + uNdcZOffset; // NDC → [0,1]

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    // PCF 3×3
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(uShadowMap, projCoords.xy + vec2(x,y)*texelSize).r;
            shadow += projCoords.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// =============================================================================
// NORMAL MAP
// =============================================================================
vec3 ComputeNormal() {
    vec3 tangentNormal = texture(uNormalMap, fsIn.uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= uNormalScale;

    vec3 N = normalize(fsIn.normal);
    vec3 T = normalize(fsIn.tangent);
    vec3 B = normalize(fsIn.bitangent);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// =============================================================================
// MAIN
// =============================================================================
void main() {
    // ── Lire les textures ────────────────────────────────────────────────────
    vec4 albedoTex = texture(uAlbedoMap, fsIn.uv);
    vec4 orm       = texture(uORMMap,    fsIn.uv);
    vec4 emissive  = texture(uEmissiveMap, fsIn.uv);

    vec4 albedo = pow(albedoTex, vec4(2.2)) * uBaseColor * fsIn.color;
    if (albedo.a < uAlphaClip) discard;

    float ao        = orm.r * uOcclusion;
    float roughness = clamp(orm.g * uRoughness, 0.04, 1.0);
    float metallic  = orm.b * uMetallic;

    // ── Vecteurs ──────────────────────────────────────────────────────────────
    vec3 N = ComputeNormal();
    vec3 V = normalize(uEyePos.xyz - fsIn.fragPos);
    vec3 R = reflect(-V, N);

    // Fresnel de base : diélectrique=0.04, conducteur=albedo
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    // ── Lumière directionnelle ────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);
    {
        vec3 L  = normalize(-uLightDir.xyz);
        vec3 H  = normalize(V + L);
        float NdL = max(dot(N, L), 0.0);

        // Cook-Torrance BRDF
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator   = D * G * F;
        float denominator= 4.0 * max(dot(N,V),0.0) * NdL + EPSILON;
        vec3 specular    = numerator / denominator;

        // Énergie conservée
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 radiance = vec3(5.0); // intensité lumière directionnelle

        // Shadow
        float shadow = ComputeShadow(fsIn.fragPosLightSpace, N, L);

        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdL * (1.0 - shadow);
    }

    // ── IBL — Image Based Lighting ────────────────────────────────────────────
    vec3 F_ibl = FresnelSchlickRoughness(max(dot(N,V), 0.0), F0, roughness);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);

    // Diffuse irradiance
    vec3 irradiance = texture(uEnvIrradiance, N).rgb;
    vec3 diffuse_ibl = irradiance * albedo.rgb;

    // Specular prefiltered + BRDF LUT
    const float MAX_MIP = 4.0;
    vec3 prefilteredColor = textureLod(uEnvPrefilter, R, roughness * MAX_MIP).rgb;
    vec2 brdf = texture(uBRDF_LUT, vec2(max(dot(N,V),0.0), roughness)).rg;
    vec3 specular_ibl = prefilteredColor * (F_ibl * brdf.x + brdf.y);

    vec3 ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao;

    // ── Émission ──────────────────────────────────────────────────────────────
    vec3 emissiveOut = (pow(emissive.rgb, vec3(2.2)) + uEmissiveColor.rgb) * uEmissiveScale;

    // ── Résultat HDR ──────────────────────────────────────────────────────────
    vec3 color = ambient + Lo + emissiveOut;

    // Sortie HDR (le tonemap est fait en post-process)
    FragColor = vec4(color, albedo.a);
}
