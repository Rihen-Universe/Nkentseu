//==============================================================================
// pbr_metallic.gl.frag — PBR Fragment Shader (OpenGL 4.6 Core)
// Metallic-Roughness avec IBL, shadow map PCF, normal map
//==============================================================================
#version 460 core

// ── Inputs ────────────────────────────────────────────────────────────────────
in VS_OUT {
    vec3  WorldPos;
    vec3  Normal;
    vec3  Tangent;
    vec3  Bitangent;
    vec2  TexCoord;
    vec4  Color;
    vec4  ShadowCoord;
} fIn;

// ── Output ────────────────────────────────────────────────────────────────────
out vec4 oColor;

// ── Textures ──────────────────────────────────────────────────────────────────
layout(binding = 0) uniform sampler2D   uAlbedoMap;       // sRGB RGBA
layout(binding = 1) uniform sampler2D   uNormalMap;       // Linear RGB (tangent-space)
layout(binding = 2) uniform sampler2D   uORMMap;          // Occlusion(R) Roughness(G) Metallic(B)
layout(binding = 3) uniform sampler2D   uEmissiveMap;
layout(binding = 4) uniform sampler2D   uShadowMap;       // Depth shadow map
layout(binding = 5) uniform samplerCube uEnvIrradiance;   // IBL diffuse
layout(binding = 6) uniform samplerCube uEnvPrefilter;    // IBL specular (mip-mapped)
layout(binding = 7) uniform sampler2D   uBRDF_LUT;        // BRDF integration LUT

// ── Scene UBO ─────────────────────────────────────────────────────────────────
layout(std140, binding = 0) uniform NkSceneUBO {
    mat4  uModel;
    mat4  uView;
    mat4  uProj;
    mat4  uLightVP;
    vec4  uLightDir;
    vec4  uEyePos;
    float uTime;
    float uDeltaTime;
    float uNdcZScale;
    float uNdcZOffset;
};

// ── Material UBO ──────────────────────────────────────────────────────────────
layout(std140, binding = 2) uniform NkPBRMaterialUBO {
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
    float _pad[2];
};

// ── Lights UBO ────────────────────────────────────────────────────────────────
struct NkLight {
    vec4 positionAndType;   // xyz=pos, w=type (0=dir, 1=point, 2=spot)
    vec4 directionAndRange; // xyz=dir, w=range
    vec4 colorAndIntensity; // xyz=color, w=intensity
    vec4 spotParams;        // x=innerAngle, y=outerAngle, zw=reserved
};
layout(std140, binding = 1) uniform NkLightsUBO {
    NkLight uLights[16];
    int     uLightCount;
    int     _lpad[3];
};

// ── Constantes PBR ────────────────────────────────────────────────────────────
const float PI = 3.14159265358979323846;
const float EPSILON = 0.0001;

// ── Fonctions PBR ─────────────────────────────────────────────────────────────

// Distribution GGX (Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdH  = max(dot(N, H), 0.0);
    float NdH2 = NdH * NdH;
    float denom = (NdH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + EPSILON);
}

// Geometry Schlick-GGX
float GeometrySchlickGGX(float NdV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdV = max(dot(N, V), 0.0);
    float NdL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdV, roughness) * GeometrySchlickGGX(NdL, roughness);
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

// ── Shadow PCF ────────────────────────────────────────────────────────────────
float ShadowPCF(vec4 shadowCoord, float bias) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    if (projCoords.z > 1.0) return 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap,
                projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// ── Normal mapping ────────────────────────────────────────────────────────────
vec3 GetNormal() {
    vec3 N = normalize(fIn.Normal);
    vec3 T = normalize(fIn.Tangent);
    vec3 B = normalize(fIn.Bitangent);
    mat3 TBN = mat3(T, B, N);
    vec3 n = texture(uNormalMap, fIn.TexCoord).rgb * 2.0 - 1.0;
    n.xy *= uNormalScale;
    return normalize(TBN * n);
}

// ── Radiance d'une lumière locale ─────────────────────────────────────────────
vec3 LightRadiance(NkLight light, vec3 worldPos) {
    int  ltype = int(light.positionAndType.w);
    vec3 color = light.colorAndIntensity.xyz * light.colorAndIntensity.w;

    if (ltype == 0) {  // Directionnel
        return color;
    }
    // Point / Spot
    float dist  = length(light.positionAndType.xyz - worldPos);
    float range = light.directionAndRange.w;
    float atten = 1.0 / (dist * dist + EPSILON);
    atten *= max(0.0, 1.0 - (dist / range));
    return color * atten;
}

vec3 LightDirection(NkLight light, vec3 worldPos) {
    int ltype = int(light.positionAndType.w);
    if (ltype == 0) return normalize(-light.directionAndRange.xyz);
    return normalize(light.positionAndType.xyz - worldPos);
}

// =============================================================================
// MAIN
// =============================================================================
void main() {
    // ── Échantillonnage des textures ──────────────────────────────────────────
    vec4 albedoSample = texture(uAlbedoMap, fIn.TexCoord) * uBaseColor * fIn.Color;
    if (albedoSample.a < uAlphaClip) discard;

    vec3 albedo   = pow(albedoSample.rgb, vec3(2.2));  // sRGB → linear
    vec3 orm      = texture(uORMMap, fIn.TexCoord).rgb;
    float ao       = orm.r * uOcclusion;
    float roughness= orm.g * uRoughness;
    float metallic = orm.b * uMetallic;
    vec3 emissive  = texture(uEmissiveMap, fIn.TexCoord).rgb
                   * uEmissiveColor.rgb * uEmissiveScale;

    // ── Vecteurs ──────────────────────────────────────────────────────────────
    vec3 N = GetNormal();
    vec3 V = normalize(uEyePos.xyz - fIn.WorldPos);
    vec3 R = reflect(-V, N);

    // ── Paramètres PBR ────────────────────────────────────────────────────────
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── Direct lighting ───────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < uLightCount; ++i) {
        vec3 L        = LightDirection(uLights[i], fIn.WorldPos);
        vec3 H        = normalize(V + L);
        vec3 radiance = LightRadiance(uLights[i], fIn.WorldPos);
        float NdL     = max(dot(N, L), 0.0);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 specular = (NDF * G * F) /
            (4.0 * max(dot(N, V), 0.0) * NdL + EPSILON);

        Lo += (kD * albedo / PI + specular) * radiance * NdL;
    }

    // ── IBL ambient ───────────────────────────────────────────────────────────
    vec3 F_ibl  = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS_ibl = F_ibl;
    vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

    vec3 irradiance = texture(uEnvIrradiance, N).rgb;
    vec3 diffuse_ibl= irradiance * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(uEnvPrefilter, R,
        roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF  = texture(uBRDF_LUT,
        vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular_ibl = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);

    vec3 ambient  = (kD_ibl * diffuse_ibl + specular_ibl) * ao;

    // ── Ombres ────────────────────────────────────────────────────────────────
    float shadow = ShadowPCF(fIn.ShadowCoord, 0.005);
    Lo *= (1.0 - shadow * 0.8);

    // ── Couleur finale ────────────────────────────────────────────────────────
    vec3 color = ambient + Lo + emissive;

    // ── Tonemap ACES rapide ───────────────────────────────────────────────────
    color = color / (color + vec3(0.155)) * 1.019;
    // Correction gamma
    color = pow(color, vec3(1.0 / 2.2));

    oColor = vec4(color, albedoSample.a);
}
