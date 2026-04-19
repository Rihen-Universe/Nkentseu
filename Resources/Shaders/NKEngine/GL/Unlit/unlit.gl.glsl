//==============================================================================
// unlit.gl.glsl — Shader Unlit (OpenGL 4.6 Core)
// Vert + Frag combinés via #ifdef VERTEX_SHADER / FRAGMENT_SHADER
//==============================================================================

//──────────────────────────────────────────────────────────────────────────────
// VERTEX
//──────────────────────────────────────────────────────────────────────────────
#ifdef VERTEX_SHADER
#version 460 core

layout(location = 0) in vec3 aPosition;
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

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    vTexCoord   = aTexCoord;
    vColor      = aColor * uObjectTint;
    gl_Position = uProj * uView * uObjectModel * vec4(aPosition, 1.0);
}
#endif // VERTEX_SHADER

//──────────────────────────────────────────────────────────────────────────────
// FRAGMENT
//──────────────────────────────────────────────────────────────────────────────
#ifdef FRAGMENT_SHADER
#version 460 core

in  vec2 vTexCoord;
in  vec4 vColor;
out vec4 oColor;

layout(binding = 0) uniform sampler2D uAlbedoMap;

layout(std140, binding = 2) uniform NkUnlitMaterialUBO {
    vec4  uBaseColor;
    vec4  uEmissiveColor;
    float uAlphaClip;
    float _pad[3];
};

void main() {
    vec4 color = texture(uAlbedoMap, vTexCoord) * uBaseColor * vColor;
    if (color.a < uAlphaClip) discard;
    oColor = color;
}
#endif // FRAGMENT_SHADER
