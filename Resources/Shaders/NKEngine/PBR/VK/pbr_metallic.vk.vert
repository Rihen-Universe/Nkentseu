// =============================================================================
// pbr_metallic.vk.vert — Shader Vertex PBR Vulkan GLSL 4.50
// =============================================================================
#version 450

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;

layout(set=0, binding=0) uniform SceneUBO {
    mat4 uModel;
    mat4 uView;
    mat4 uProj;
    mat4 uLightVP;
    vec4 uLightDir;
    vec4 uEyePos;
    float uTime;
    float uDeltaTime;
    float uNdcZScale;
    float uNdcZOffset;
};

layout(location=0) out vec3  oFragPos;
layout(location=1) out vec3  oNormal;
layout(location=2) out vec3  oTangent;
layout(location=3) out vec3  oBitangent;
layout(location=4) out vec2  oUV;
layout(location=5) out vec4  oColor;
layout(location=6) out vec4  oFragPosLS;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    oFragPos = worldPos.xyz;

    mat3 normalMat = transpose(inverse(mat3(uModel)));
    oNormal    = normalize(normalMat * aNormal);
    oTangent   = normalize(normalMat * aTangent);
    oBitangent = normalize(cross(oNormal, oTangent));
    oUV        = aUV;
    oColor     = aColor;
    oFragPosLS = uLightVP * worldPos;

    // Vulkan : Y inversé
    gl_Position = uProj * uView * worldPos;
}
