// toon.frag — NKRenderer v4.0 — OpenGL GLSL 4.60 core
// Toon shading avec outline, rim light, ombres par seuil
#version 460 core
layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;
layout(location=0) out vec4 fragColor;

layout(binding=0,std140) uniform CameraUBO {
    mat4 view,proj,viewProj,invViewProj; vec4 camPos; vec2 viewport; float time; float dt;
};
layout(binding=1,std140) uniform ObjectUBO {
    mat4 model,normalMatrix; vec4 tint;
    float metallic,roughness,ao,emissStr,normStr; float _p[3];
};
layout(binding=2,std140) uniform LightsUBO {
    vec4 lightPos[32], lightColor[32], lightDir[32], lightAngles[32];
    int lightCount; int _p[3];
};
layout(binding=3,std140) uniform ToonUBO {
    vec4  shadowColor;
    float shadowThreshold;
    float shadowSmooth;
    float outlineWidth;
    float rimIntensity;
    vec4  outlineColor;
    vec4  rimColor;
    float specHardness;
    float _pad[3];
};

layout(binding=4) uniform sampler2D tAlbedo;

void main() {
    vec4  albedo = texture(tAlbedo, vUV) * vColor;
    if (albedo.a < 0.01) discard;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos.xyz - vWorldPos);

    // Rim light
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, 3.0) * rimIntensity;

    vec3 finalColor = vec3(0.0);

    for (int i = 0; i < lightCount; i++) {
        int   ltype = int(lightPos[i].w);
        vec3  L;
        float att = 1.0;

        if (ltype == 0) {
            L = normalize(-lightDir[i].xyz);
        } else {
            vec3 d  = lightPos[i].xyz - vWorldPos;
            float dist = length(d);
            L = normalize(d);
            att = max(1.0 - dist/lightDir[i].w, 0.0);
            att *= att;
        }

        float NdL = dot(N, L);

        // Stepped shadow (toon threshold)
        float toonDiff = smoothstep(shadowThreshold - shadowSmooth,
                                     shadowThreshold + shadowSmooth,
                                     NdL) * att;

        // Specular toon (hard)
        vec3  H      = normalize(V + L);
        float spec   = pow(max(dot(N, H), 0.0), specHardness);
        float toonSpec = step(0.5, spec) * att;

        vec3 litColor   = albedo.rgb * lightColor[i].rgb * lightColor[i].w;
        vec3 shadColor  = shadowColor.rgb * albedo.rgb * lightColor[i].rgb;

        finalColor += mix(shadColor, litColor, toonDiff) + toonSpec * 0.4;
    }

    // Rim light
    finalColor += rimColor.rgb * rim;

    // Ambient minimal
    finalColor += albedo.rgb * 0.05;

    fragColor = vec4(finalColor, albedo.a);
}
