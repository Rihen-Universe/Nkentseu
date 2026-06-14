// ============================================================
// debugtri.vert.vk.glsl — DEBUG triangle minimal Vulkan
// Aucun UBO, aucun set, position NDC trivial.
// Permet d'isoler le bug PBR en testant le pipeline VK le plus simple.
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=0) out vec3 vColor;

void main() {
    // Position NDC directe : aPos est suppose etre dans [-1,1]^3.
    // En Z=0.5 garanti dans [0,1] pour Vulkan.
    gl_Position = vec4(aPos.xy, 0.5, 1.0);
    vColor = vec3(aPos.x * 0.5 + 0.5, aPos.y * 0.5 + 0.5, 0.0);
}
