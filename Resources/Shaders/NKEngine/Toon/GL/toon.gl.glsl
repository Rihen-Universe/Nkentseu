// =============================================================================
// toon.gl.glsl — Shader Toon / Cel-shading OpenGL 4.6
// Vert + Frag combinés (séparés par la section @stage)
// =============================================================================

// ============================================================
// VERTEX SHADER
// ============================================================
// #vert
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;

layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4 uLightDir; vec4 uEyePos;
    float uTime; float uDt; float uNdcZS; float uNdcZO;
};

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vUV;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal  = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV      = aUV;
    vColor   = aColor;
    gl_Position = uProj * uView * worldPos;
}

// ============================================================
// FRAGMENT SHADER
// ============================================================
// #frag
#version 460 core

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;
in vec4 vColor;

layout(binding=0) uniform sampler2D uAlbedoMap;

layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4 uLightDir; vec4 uEyePos;
    float uTime; float uDt; float uNdcZS; float uNdcZO;
};

layout(std140, binding=2) uniform ToonMaterial {
    vec4  uBaseColor;
    vec4  uShadowColor;
    vec4  uOutlineColor;
    float uOutlineWidth;
    float uShadowThreshold;
    float uShadowSmoothness;
    float uSpecThreshold;
    float uSpecSmoothness;
    uint  uShadeSteps;
    float _p0; float _p1;
};

out vec4 FragColor;

void main() {
    vec4 albedo = texture(uAlbedoMap, vUV) * uBaseColor * vColor;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir.xyz);
    vec3 V = normalize(uEyePos.xyz - vFragPos);
    vec3 H = normalize(V + L);

    float NdL = dot(N, L);

    // ── Cel-shading : quantification des niveaux ──────────────────────────────
    float shadow = smoothstep(uShadowThreshold - uShadowSmoothness,
                               uShadowThreshold + uShadowSmoothness,
                               NdL);

    // Quantification en N niveaux
    if (uShadeSteps > 1u) {
        float steps = float(uShadeSteps);
        shadow = floor(shadow * steps + 0.5) / steps;
    }

    // ── Speculaire stylisé ────────────────────────────────────────────────────
    float spec = dot(N, H);
    float specVal = smoothstep(uSpecThreshold - uSpecSmoothness,
                                uSpecThreshold + uSpecSmoothness,
                                spec);

    // ── Couleur finale ────────────────────────────────────────────────────────
    vec3 color = mix(uShadowColor.rgb, albedo.rgb, shadow);
    color += specVal * vec3(1.0);   // highlight blanc

    FragColor = vec4(color, albedo.a);
}
