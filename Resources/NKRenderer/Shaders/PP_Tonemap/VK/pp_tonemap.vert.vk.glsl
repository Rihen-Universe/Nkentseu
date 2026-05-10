// ============================================================
// pp_tonemap.vert.vk.glsl — NKRenderer v5.0 — PostProcess Tonemap VS (Vulkan)
// Sync avec embedded GL fallback dans NkPostProcessStack.cpp (kFullscreenVS_GL).
// Identique au GL : pas d'uniform a migrer.
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV;
layout(location=0) out vec2 vUV;

void main() {
    // Flip vertical de l'UV pour Vulkan : la convention de stockage des textures
    // VK est row 0 = top (y-down image), alors que le quad mesh fournit aUV en
    // convention GL (y=0 = bottom). Sans flip, l'image rendue dans le HDR via
    // viewport VK natif (y-down) est sample inversee par le tonemap. Resultat
    // visible : scene 3D PBR a l'envers (sol en haut). Avec flip vUV.y, l'image
    // dans le swap apparait dans le bon sens.
    vUV = vec2(aUV.x, 1.0 - aUV.y);
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
