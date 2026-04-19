//==============================================================================
// toon.gl.glsl — Toon/Cel-shading Shader (OpenGL 4.6 Core)
// Contours par backface expansion + étapes de lumière configurable
//==============================================================================
#version 460 core

//──────────────────────────────────────────────────────────────────────────────
// VERTEX — défini dans le programme principal ou compilé séparément
//──────────────────────────────────────────────────────────────────────────────
#ifdef VERTEX_SHADER

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec2 aTexCoord;
layout(location = 4) in vec4 aColor;

layout(std140, binding = 0) uniform NkSceneUBO {
    mat4  uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4  uLightDir; vec4 uEyePos;
    float uTime; float uDeltaTime; float uNdcZScale; float uNdcZOffset;
};
layout(std140, binding = 3) uniform NkObjectPC {
    mat4 uObjectModel; vec4 uObjectTint;
};

out vec3 vWorldNormal;
out vec3 vWorldPos;
out vec2 vTexCoord;
out vec4 vColor;

void main() {
    mat4 normalMat = transpose(inverse(uObjectModel));
    vec4 worldPos  = uObjectModel * vec4(aPosition, 1.0);
    vWorldPos    = worldPos.xyz;
    vWorldNormal = normalize(mat3(normalMat) * aNormal);
    vTexCoord    = aTexCoord;
    vColor       = aColor * uObjectTint;
    gl_Position  = uProj * uView * worldPos;
}
#endif // VERTEX_SHADER

//──────────────────────────────────────────────────────────────────────────────
// FRAGMENT
//──────────────────────────────────────────────────────────────────────────────
#ifdef FRAGMENT_SHADER

in  vec3 vWorldNormal;
in  vec3 vWorldPos;
in  vec2 vTexCoord;
in  vec4 vColor;
out vec4 oColor;

layout(binding = 0) uniform sampler2D uAlbedoMap;

layout(std140, binding = 0) uniform NkSceneUBO {
    mat4  uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4  uLightDir; vec4 uEyePos;
    float uTime; float uDeltaTime; float uNdcZScale; float uNdcZOffset;
};
layout(std140, binding = 2) uniform NkToonMaterialUBO {
    vec4  uBaseColor;
    vec4  uShadowColor;
    vec4  uOutlineColor;
    float uOutlineWidth;
    float uShadowThreshold;
    float uShadowSmoothness;
    float uSpecThreshold;
    float uSpecSmoothness;
    uint  uShadeSteps;
    float _pad[2];
};

float StepSmooth(float edge, float val, float smooth_) {
    return smoothstep(edge - smooth_, edge + smooth_, val);
}

void main() {
    vec3 albedo = texture(uAlbedoMap, vTexCoord).rgb * uBaseColor.rgb * vColor.rgb;
    vec3 N      = normalize(vWorldNormal);
    vec3 L      = normalize(-uLightDir.xyz);

    // Diffuse quantifié par étapes
    float NdL = dot(N, L) * 0.5 + 0.5;

    // Version étapes continues
    float stepped = floor(NdL * float(uShadeSteps)) / float(uShadeSteps);
    float shadow  = StepSmooth(uShadowThreshold, NdL, uShadowSmoothness);

    vec3 shaded = mix(uShadowColor.rgb * albedo,
                      albedo,
                      shadow);

    // Specular Toon
    vec3 V   = normalize(uEyePos.xyz - vWorldPos);
    vec3 H   = normalize(L + V);
    float NdH = dot(N, H) * 0.5 + 0.5;
    float spec = StepSmooth(uSpecThreshold, NdH, uSpecSmoothness);
    shaded += spec * vec3(1.0);

    oColor = vec4(shaded, uBaseColor.a);
}
#endif // FRAGMENT_SHADER
