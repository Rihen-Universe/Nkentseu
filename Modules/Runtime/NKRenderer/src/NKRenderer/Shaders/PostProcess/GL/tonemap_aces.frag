// tonemap_aces.frag — NKRenderer v4.0 — OpenGL GLSL 4.60
#version 460 core
layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 fragColor;

layout(binding=0) uniform sampler2D tHDR;
layout(binding=0,std140) uniform TonemapUBO {
    float exposure;
    float gamma;
    float _p[2];
};

// ACES filmic tonemapping (Narkowicz approximation)
vec3 ACESFilm(vec3 x) {
    float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(tHDR, vUV).rgb * exposure;
    vec3 ldr = ACESFilm(hdr);
    // Gamma correction
    ldr = pow(ldr, vec3(1.0 / gamma));
    fragColor = vec4(ldr, 1.0);
}
