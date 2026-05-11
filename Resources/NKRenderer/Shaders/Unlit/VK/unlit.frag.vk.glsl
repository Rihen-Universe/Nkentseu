// unlit.frag.vk.glsl — NKRenderer v4.0 — Unlit Fragment (Vulkan)
//
// @material("Default_Unlit")
// @color("albedo",           default=(1,1,1,1))    vec4  albedoColor
// @param("emissive_strength",min=0.0,max=10.0,default=1.0) float emissiveStrength
// @texture2D("albedo_map") sampler2D tAlbedo
#version 460 core

layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 fragColor;

// Per-instance (set=2, binding=1) — NkPBRParams layout
layout(std140, set=2, binding=1) uniform UnlitUBO {
    vec4  albedo;
    vec4  emissive;
    float metallic, roughness, ao, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRough, subsurface;
    vec4  subsurfaceColor;
    float anisotropy, sheen; float _pad[2];
} uMat;

layout(set=2, binding=3) uniform sampler2D tAlbedo;

void main() {
    vec4 tex  = texture(tAlbedo, vUV);
    vec4 col  = tex * uMat.albedo * vColor;
    col.rgb  += uMat.emissive.rgb * uMat.emissiveStrength;
    fragColor = col;
}
