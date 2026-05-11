// toon.frag.vk.glsl — NKRenderer v4.0 — Toon/Cel-shading Fragment (Vulkan)
//
// @material("Default_Toon")
// @color("albedo",          default=(1,1,1,1))         vec4  albedoColor
// @color("shadow_color",    default=(0.2,0.1,0.3,1))   vec4  shadowColor
// @color("outline_color",   default=(0,0,0,1))          vec4  outlineColor
// @color("rim_color",       default=(1,1,1,1))          vec4  rimColor
// @param("shadow_threshold", min=0.0, max=1.0,   default=0.3)  float shadowThreshold
// @param("shadow_smooth",    min=0.0, max=0.2,   default=0.05) float shadowSmooth
// @param("outline_width",    min=0.0, max=5.0,   default=2.0)  float outlineWidth
// @param("rim_intensity",    min=0.0, max=2.0,   default=0.5)  float rimIntensity
// @param("spec_hardness",    min=1.0, max=128.0, default=32.0) float specHardness
// @texture2D("albedo_map") sampler2D tAlbedo
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
} uCam;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4  positions[32], colors[32], directions[32], angles[32];
    int   count; int _pad[3];
} uLights;

// Per-instance : NkToonParams (set=2, binding=1)
layout(std140, set=2, binding=1) uniform ToonUBO {
    vec4  shadowColor;
    float shadowThreshold;
    float shadowSmooth;
    float outlineWidth;
    float rimIntensity;
    vec4  outlineColor;
    vec4  rimColor;
    float specHardness;
    float _pad[3];
} uToon;

layout(set=2, binding=3) uniform sampler2D tAlbedo;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCam.camPos.xyz - vWorldPos);
    vec4 albedo = texture(tAlbedo, vUV) * vColor;

    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpec    = vec3(0.0);

    for (int i = 0; i < uLights.count && i < 32; i++) {
        vec4 lPos   = uLights.positions[i];
        vec3 L      = (lPos.w < 0.5)
                    ? normalize(-uLights.directions[i].xyz)
                    : normalize(lPos.xyz - vWorldPos);
        float NdotL = dot(N, L);

        // Rampe cel (diffuse)
        float cel   = smoothstep(uToon.shadowThreshold - uToon.shadowSmooth,
                                 uToon.shadowThreshold + uToon.shadowSmooth,
                                 NdotL);
        vec3 lit    = uLights.colors[i].rgb * albedo.rgb;
        vec3 shad   = uToon.shadowColor.rgb  * albedo.rgb;
        totalDiffuse += mix(shad, lit, cel);

        // Specular quantifie
        vec3 H      = normalize(L + V);
        float s     = step(0.5, pow(max(dot(N, H), 0.0), uToon.specHardness));
        totalSpec  += uLights.colors[i].rgb * s;
    }
    if (uLights.count == 0) totalDiffuse = albedo.rgb;

    // Rim
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * uToon.rimIntensity;
    vec3 rimC = uToon.rimColor.rgb * rim;

    fragColor = vec4(totalDiffuse + totalSpec + rimC, albedo.a);
}
