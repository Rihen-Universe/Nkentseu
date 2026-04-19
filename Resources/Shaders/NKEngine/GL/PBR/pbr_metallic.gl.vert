//==============================================================================
// pbr_metallic.gl.vert — PBR Vertex Shader (OpenGL 4.6 Core)
// NKRenderer / NkMaterialSystem
//==============================================================================
#version 460 core

// ── Vertex attributes ────────────────────────────────────────────────────────
layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec3  aTangent;
layout(location = 3) in vec2  aTexCoord;
layout(location = 4) in vec4  aColor;

// ── Uniform blocks ───────────────────────────────────────────────────────────
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

// ── Push constants (via uniform pour GL) ─────────────────────────────────────
layout(std140, binding = 3) uniform NkObjectPC {
    mat4  uObjectModel;
    vec4  uObjectTint;
};

// ── Outputs vers le fragment ──────────────────────────────────────────────────
out VS_OUT {
    vec3  WorldPos;
    vec3  Normal;
    vec3  Tangent;
    vec3  Bitangent;
    vec2  TexCoord;
    vec4  Color;
    vec4  ShadowCoord;   // Coordonnées shadow map (espace lumière)
} vOut;

void main() {
    mat4 model    = uObjectModel;
    mat4 normalMat= transpose(inverse(model));

    vec4 worldPos = model * vec4(aPosition, 1.0);

    vOut.WorldPos   = worldPos.xyz;
    vOut.Normal     = normalize(mat3(normalMat) * aNormal);
    vOut.Tangent    = normalize(mat3(model)     * aTangent);
    vOut.Bitangent  = cross(vOut.Normal, vOut.Tangent);
    vOut.TexCoord   = aTexCoord;
    vOut.Color      = aColor * uObjectTint;

    // Shadow map coords
    const mat4 biasMatrix = mat4(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );
    vOut.ShadowCoord = biasMatrix * uLightVP * worldPos;

    gl_Position = uProj * uView * worldPos;
}
