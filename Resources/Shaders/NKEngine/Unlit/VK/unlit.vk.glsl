// =============================================================================
// unlit.vk.glsl — Shader Unlit Vulkan GLSL 4.50
// =============================================================================
// #vert
#version 450
layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;
layout(set=0, binding=0) uniform SceneUBO {
    mat4 uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4 uLightDir; vec4 uEyePos;
    float uTime; float uDt; float uNdcZS; float uNdcZO;
};
layout(push_constant) uniform PC { mat4 pcModel; vec4 pcTint; };
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;
void main() {
    vec4 wp = pcModel * vec4(aPos,1.0);
    vUV    = aUV;
    vColor = aColor * pcTint;
    gl_Position = uProj * uView * wp;
}

// #frag
#version 450
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(set=1, binding=0) uniform sampler2D uAlbedoMap;
layout(set=2, binding=0) uniform UnlitMat {
    vec4 uBaseColor; vec4 uEmissiveColor;
    float uAlphaClip; float uEmissiveScale; float _p0; float _p1;
};
layout(location=0) out vec4 oColor;
void main() {
    vec4 c = texture(uAlbedoMap, vUV) * uBaseColor * vColor;
    if (c.a < uAlphaClip) discard;
    c.rgb += uEmissiveColor.rgb * uEmissiveScale;
    oColor = c;
}
