#version 450
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec4 vShadowCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, set = 0, binding = 0) uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

layout(set = 0, binding = 1) uniform sampler2DShadow uShadowMap;

float ShadowFactor(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);
    projCoords.z -= bias;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(uShadowMap, vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N      = normalize(vNormal);
    vec3 L      = normalize(-ubo.lightDirW.xyz);
    vec3 V      = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H      = normalize(L + V);

    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 32.0);
    float shadow = ShadowFactor(vShadowCoord);

    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = shadow * diff * vColor;
    vec3 specular = shadow * spec * vec3(0.4);

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
