// =============================================================================
// shadow_depth.gl.glsl — Passe de profondeur shadow map OpenGL 4.6
// Rendu depuis le point de vue de la lumière (directionnelle ou spot).
// Fragment vide : on ne s'intéresse qu'à gl_FragDepth.
// =============================================================================
// #vert
#version 460 core
layout(location=0) in vec3 aPos;
layout(push_constant) uniform LightPC {
    mat4 pcLightVP;
    mat4 pcModel;
};
void main() {
    gl_Position = pcLightVP * pcModel * vec4(aPos,1.0);
}
// #frag
#version 460 core
void main() {
    // Profondeur écrite automatiquement dans le depth buffer
}
