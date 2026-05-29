// ============================================================
// pp_tonemap.vert.vk.glsl — NKRenderer v5.0 — PostProcess Tonemap VS
//
// Fullscreen triangle (3 verts via gl_VertexIndex). UV calcule depuis NDC
// avec flip Y conditionnel selon le backend via push constant pc.p1.z :
//   yFlipUV = -1 en Vulkan (viewport Y-flippe) -> flip UV pour matcher
//             la convention storage VK (ligne 0 = top).
//   yFlipUV = +1 en OpenGL (pas de flip viewport) -> pas de flip UV
//             (storage GL ligne H-1 = top via UV.y=1).
// ============================================================
#version 460 core

layout(location=0) out vec2 vUV;

// Push constant partage avec le fragment (NK_ALL_GRAPHICS dans pipeline).
// Phase L : etendu 32->48 bytes (p2 = auto-exposure params).
// DOIT matcher EXACTEMENT pp_tonemap.frag.vk.glsl sinon OpenGL strict-link
// refuse "type mismatch between shaders for uniform (named _PushConstants[0])".
layout(push_constant) uniform PC {
    vec4 p0;   // (exposure, gamma, vignetteIntens, saturation)
    vec4 p1;   // (bloomStrength, lutStrength, yFlipUV, lutSize)
    vec4 p2;   // Phase L auto-exposure : (autoExpStrength, autoExpKey, _, _)
} pc;

void main() {
    vec2 pos = vec2((gl_VertexIndex == 1) ? 3.0 : -1.0,
                    (gl_VertexIndex == 2) ? 3.0 : -1.0);
    gl_Position = vec4(pos, 0.0, 1.0);

    vec2 uvBase = pos * 0.5 + 0.5;     // [-1,+3] -> [0, 2], clip a [0,1]
    float yFlipUV = pc.p1.z;
    vUV = (yFlipUV < 0.0) ? vec2(uvBase.x, 1.0 - uvBase.y) : uvBase;
}
