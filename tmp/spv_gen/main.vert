#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, set = 0, binding = 0) uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec4 vShadowCoord;

void main() {
    vec4 worldPos     = ubo.model * vec4(aPos, 1.0);
    vWorldPos         = worldPos.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    vNormal           = normalize(normalMatrix * aNormal);
    vColor            = aColor;
    vShadowCoord      = ubo.lightVP * worldPos;
    gl_Position       = ubo.proj * ubo.view * worldPos;
}
