// =============================================================================
// pbr_metallic.gl.vert — Shader Vertex PBR OpenGL 4.6 GLSL
// =============================================================================
#version 460 core

// ── Vertex attributes ────────────────────────────────────────────────────────
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;

// ── Uniform Buffer — Set 0 : données de scène ────────────────────────────────
layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel;
    mat4 uView;
    mat4 uProj;
    mat4 uLightVP;
    vec4 uLightDir;   // xyz=direction, w=unused
    vec4 uEyePos;     // xyz=position caméra
    float uTime;
    float uDeltaTime;
    float uNdcZScale;
    float uNdcZOffset;
};

// ── Sorties vers le fragment shader ──────────────────────────────────────────
out VS_OUT {
    vec3  fragPos;
    vec3  normal;
    vec3  tangent;
    vec3  bitangent;
    vec2  uv;
    vec4  color;
    vec4  fragPosLightSpace;
} vsOut;

void main() {
    vec4 worldPos  = uModel * vec4(aPos, 1.0);
    vsOut.fragPos  = worldPos.xyz;

    // Matrice normale (inverse-transposée du model)
    mat3 normalMat = transpose(inverse(mat3(uModel)));
    vsOut.normal   = normalize(normalMat * aNormal);
    vsOut.tangent  = normalize(normalMat * aTangent);
    vsOut.bitangent= normalize(cross(vsOut.normal, vsOut.tangent));

    vsOut.uv    = aUV;
    vsOut.color = aColor;

    // Light-space pour les shadow maps
    vsOut.fragPosLightSpace = uLightVP * worldPos;

    gl_Position = uProj * uView * worldPos;
}
