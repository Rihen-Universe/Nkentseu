// ============================================================
// debugtri.frag.vk.glsl — DEBUG triangle minimal Vulkan
// Sortie couleur basee sur position NDC. Aucun UBO, aucun sampler.
// ============================================================
#version 460 core

layout(location=0) in vec3 vColor;
layout(location=0) out vec4 fragColor;

void main() {
    // Couleur RGB derivee de la position vertex : permet de voir si le
    // rasterizer interpole correctement entre vertices.
    fragColor = vec4(vColor, 1.0);
}
