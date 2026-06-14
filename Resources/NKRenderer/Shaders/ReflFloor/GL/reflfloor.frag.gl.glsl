// reflfloor.frag.gl.glsl — NKRenderer v4.0 — Reflective Floor Fragment (OpenGL 4.6)
// Miroir planaire via screen-space UV sur le render target de réflexion.
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir; vec2 viewport; float time, deltaTime;
    float iblStrength;
} uCam;

layout(std140, binding=2) uniform LightsUBO {
    vec4  positions[32], colors[32], directions[32], angles[32];
    int   count; int _pad[3];
} uLights;

layout(std140, binding=8) uniform ReflFloorUBO {
    vec4  albedo;
    vec4  emissive;
    float metallic;
    float roughness;
    float ao;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRough;
    float subsurface;
    vec4  subsurfaceColor;
    float anisotropy;
    float sheen;
    float _pad0;
    float _pad1;
} uFloor;

layout(binding=3) uniform sampler2D tReflection;

void main() {
    vec3 N    = normalize(vNormal);
    vec3 V    = normalize(uCam.camPos.xyz - vWorldPos);
    float NdV = max(dot(N, V), 0.0);

    vec3 baseColor = uFloor.albedo.rgb * vColor.rgb;

    vec3 diffuse = vec3(0.0);
    for (int i = 0; i < uLights.count && i < 32; i++) {
        vec3  L;
        float att = 1.0;
        int   lt  = int(uLights.positions[i].w);
        if (lt == 0) {
            L = normalize(-uLights.directions[i].xyz);
        } else {
            vec3  d    = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L   = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att = att * att;
        }
        diffuse += uLights.colors[i].rgb * uLights.colors[i].w * att * max(dot(N, L), 0.0);
    }
    vec3 litBase = baseColor * (diffuse + 0.12);

    float f0      = 0.04;
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - NdV, 5.0);
    float reflStr = (1.0 - uFloor.roughness) * clamp(fresnel + 0.5, 0.0, 1.0);

    vec2 screenUV  = gl_FragCoord.xy / uCam.viewport;
    vec3 reflColor = texture(tReflection, screenUV).rgb;

    fragColor = vec4(mix(litBase, reflColor, clamp(reflStr, 0.0, 1.0)), uFloor.albedo.a);
}
