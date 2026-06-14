// ============================================================
// render2d.vert.vk.glsl — NKRenderer v5.0 — Render2D Vertex (Vulkan)
// Sync avec embedded GL fallback dans NkRender2D.cpp (kRender2D_VS).
// Difference VK : push_constant block au lieu d'uniform vec4 array.
// ============================================================
#version 460 core

layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in uint  aFlags;

layout(location=0) out vec2     vUV;
layout(location=1) out vec4     vColor;
layout(location=2) out flat uint vFlags;
layout(location=3) out vec2     vWorldPos;

layout(push_constant) uniform PC {
    mat4 ortho;
} pc;

void main() {
    gl_Position = pc.ortho * vec4(aPos, 0.0, 1.0);
    vUV       = aUV;
    vWorldPos = aPos;
    vColor = vec4(float((aColor      ) & 0xFFu) / 255.0,
                  float((aColor >>  8u) & 0xFFu) / 255.0,
                  float((aColor >> 16u) & 0xFFu) / 255.0,
                  float((aColor >> 24u) & 0xFFu) / 255.0);
    vFlags = aFlags;
}
