// glow2d.vert.vk.glsl — Phase E Materials 2D : Glow sprite vertex
// Meme layout que render2d.vert.vk.glsl mais simplifie (pas de lighting/shadow).
#version 460 core

layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in uint  aFlags;

layout(push_constant) uniform PC {
    mat4 ortho;
    // glow params : offset 64..96 (vec4 + vec4 = 32 bytes)
    vec4 glowColor;        // RGB + intensity
    vec4 glowParams;       // x=power(rim concentration), y=radius scale, zw=pad
} pc;

layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;

vec4 UnpackRGBA(uint c) {
    return vec4(
        float((c >>  0) & 0xFFu) / 255.0,
        float((c >>  8) & 0xFFu) / 255.0,
        float((c >> 16) & 0xFFu) / 255.0,
        float((c >> 24) & 0xFFu) / 255.0
    );
}

void main() {
    vUV    = aUV;
    vColor = UnpackRGBA(aColor);
    gl_Position = pc.ortho * vec4(aPos, 0.0, 1.0);
}
