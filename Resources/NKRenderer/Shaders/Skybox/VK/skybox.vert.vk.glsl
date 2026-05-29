// skybox.vert.vk.glsl — Phase N v0.5 : Background HDR skybox vertex
//
// Fullscreen triangle (3 verts, pas de VBO). Depth = 1.0 (far plane).
// Passe juste les coords NDC au frag — le calcul de direction monde se fait
// dans le frag pour avoir une precision par pixel (l'interpolation lineaire
// d'une direction entre 3 verts fullscreen donne de la distortion radiale).
#version 460 core

layout(location=0) out vec2 vNDC;

void main() {
    // Fullscreen triangle qui couvre [-1, +3] x [-1, +3] dans clip space.
    vec2 pos = vec2(
        (gl_VertexIndex == 1) ? 3.0 : -1.0,
        (gl_VertexIndex == 2) ? 3.0 : -1.0
    );
    gl_Position = vec4(pos, 1.0, 1.0);
    vNDC = pos;
}
