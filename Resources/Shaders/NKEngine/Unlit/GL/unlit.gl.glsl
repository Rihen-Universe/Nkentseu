// =============================================================================
// unlit.gl.glsl — Shader Unlit OpenGL 4.6 (pas d'éclairage)
// Utilisé pour : UI, sprites, debug, shadow depth pass, fond
// =============================================================================

// #vert
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;   // ignoré pour unlit
layout(location=2) in vec3 aTangent;  // ignoré pour unlit
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;

layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4 uLightDir; vec4 uEyePos;
    float uTime; float uDt; float uNdcZS; float uNdcZO;
};

// Push constant pour les objets
layout(push_constant) uniform ObjectPC {
    mat4  pcModel;
    vec4  pcTint;
};

out vec2 vUV;
out vec4 vColor;

void main() {
    vec4 worldPos = pcModel * vec4(aPos, 1.0);
    vUV    = aUV;
    vColor = aColor * pcTint;
    gl_Position = uProj * uView * worldPos;
}

// #frag
#version 460 core

in vec2 vUV;
in vec4 vColor;

layout(binding=0) uniform sampler2D uAlbedoMap;

layout(std140, binding=2) uniform UnlitMaterial {
    vec4  uBaseColor;
    vec4  uEmissiveColor;
    float uAlphaClip;
    float uEmissiveScale;
    float _p0; float _p1;
};

out vec4 FragColor;

void main() {
    vec4 tex = texture(uAlbedoMap, vUV);
    vec4 color = tex * uBaseColor * vColor;
    if (color.a < uAlphaClip) discard;
    color.rgb += uEmissiveColor.rgb * uEmissiveScale;
    FragColor = color;
}
