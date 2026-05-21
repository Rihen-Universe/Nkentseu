// glow2d.frag.vk.glsl — Phase E Materials 2D : Glow sprite fragment
//
// Effet : sprite normal + halo radial additif depuis le centre du sprite.
// Le halo intensifie au bord (rim), avec couleur et intensite parametrables.
//
// @material("Glow2D")
// @color("glowColor",     default=(1,0.5,0.1,1))    vec4 glowColor
// @param("glowPower",     min=0.5, max=8, default=3) float glowPower
// @param("glowIntensity", min=0,   max=4, default=1) float glowIntensity
#version 460 core

#include "Include/NkColor.glsli"

layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;

layout(set=0, binding=0) uniform sampler2D tAtlas;

layout(push_constant) uniform PC {
    mat4 ortho;
    vec4 glowColor;     // RGB + intensity scalar
    vec4 glowParams;    // x=power, y=tint mix HSV<->glow, zw=pad
} pc;

void main() {
    // Sample texture sprite de base
    vec4 baseColor = texture(tAtlas, vUV) * vColor;
    if (baseColor.a < 0.01) discard;

    // Halo radial depuis le centre du sprite (UV space).
    // Au centre (uv=0.5,0.5) -> rim=0. Au bord (uv=0 ou 1) -> rim=1.
    vec2 fromCenter = vUV * 2.0 - 1.0;
    float distFromCenter = clamp(length(fromCenter), 0.0, 1.0);
    // Concentre vers le bord via power.
    float halo = pow(distFromCenter, pc.glowParams.x);

    // Couleur additive : halo color * intensity * rim attenuation.
    vec3 haloRGB = pc.glowColor.rgb * pc.glowColor.a * halo;

    vec3 final = baseColor.rgb + haloRGB;
    fragColor = vec4(final, baseColor.a);
}
