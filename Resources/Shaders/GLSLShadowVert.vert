#version 460 core
layout(location = 0) in vec3 aPos;

// On utilise un UBO séparé pour la passe shadow (binding 0)
layout(std140, binding = 0) uniform ShadowUBO {
    mat4 model;
    mat4 view;       // non utilisé dans cette passe
    mat4 proj;       // non utilisé dans cette passe
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

void main() {
    // Transforme directement dans l'espace lumière
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}