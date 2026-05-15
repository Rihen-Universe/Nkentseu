// =============================================================================
// layered.frag.vk.glsl — NKRenderer M.1 Material Layering (v0)
//
// 2 couches PBR superposees, blend par vColor.r :
//   layer0 (base) = uLayered.base    | metal, roughness, color
//   layer1 (top)  = uLayered.top     | metal, roughness, color
//   final = mix(eval(layer0), eval(layer1), vColor.r)
//
// v0 : eval simplifie (Lambert + spec Phong), pas IBL ni ombres. Suffit pour
// demontrer le mecanisme. Une fois la pipeline validee, on remplacera par
// le full PBR du shader PBR canonical (D_GGX + G_Smith + F_Schlick + IBL).
//
// Convention masque vColor :
//   .r : ratio mix (0 = layer0 pur, 1 = layer1 pur)
//   .gba : reserves pour layers supplementaires (M.1 v1 -> 4 layers RGBA).
// =============================================================================
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec2 vUV;
layout(location=4) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir;
    vec2  viewport;
    float time, deltaTime;
    float iblStrength;
} uCam;

layout(std140, set=0, binding=2) uniform LightsUBO {
    vec4 positions[32], colors[32], directions[32], angles[32];
    int count; int _pad[3];
} uLights;

// NkLayerParams = 2 PBR layers cote a cote (192 bytes std140 environ).
// Doit matcher EXACTEMENT le NkLayeredParams cote CPU.
struct LayerPBR {
    vec4 albedo;
    vec4 emissive;
    float metallic, roughness, ao, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRough, subsurface;
    vec4 subsurfaceColor;
    float anisotropy, sheen, _p0, _p1;
};

layout(std140, set=2, binding=8) uniform LayeredUBO {
    LayerPBR base;
    LayerPBR top;
    int maskSource;     // 0=vColor.r, 1=vColor.g, 2=vColor.b, 3=vColor.a, 4=texture(future)
    int _padA, _padB, _padC;
} uLayered;

// Evaluation simplifiee : Lambert diffus + Phong spec teinte metal/albedo.
vec3 EvalLayer(LayerPBR p, vec3 N, vec3 V) {
    vec3 albedo = p.albedo.rgb;
    vec3 result = albedo * 0.10; // ambient

    for (int i = 0; i < uLights.count && i < 32; i++) {
        int lt = int(uLights.positions[i].w);
        vec3 L;
        float att = 1.0;
        if (lt == 0) { // directional
            L = normalize(-uLights.directions[i].xyz);
        } else {       // point
            vec3 d = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att *= att;
        }
        float NdL = max(dot(N, L), 0.0);
        vec3 diffuse = albedo * NdL;

        // Phong spec, hardness derived from roughness (1=mat, 0=miroir)
        vec3 H = normalize(V + L);
        float spec = pow(max(dot(N, H), 0.0), mix(64.0, 8.0, p.roughness));
        // Metallic : tint spec par albedo, sinon blanc
        vec3 specColor = mix(vec3(1.0), albedo, p.metallic);
        vec3 specular = specColor * spec * (1.0 - p.roughness * 0.7);

        result += uLights.colors[i].rgb * uLights.colors[i].w * att * (diffuse + specular);
    }
    return result * p.ao + p.emissive.rgb * p.emissiveStrength;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCam.camPos.xyz - vWorldPos);

    // Selection du masque selon maskSource :
    //   0=vColor.r 1=.g 2=.b 3=.a 4=vUV.y (gradient vertical) 5=1-vUV.y
    // 4/5 utiles pour demo sans mesh vertex-colored : la sphere a un blend
    // vertical naturel (poles vs equateur via parametrisation UV).
    float mask = vColor.r;
    if (uLayered.maskSource == 1) mask = vColor.g;
    if (uLayered.maskSource == 2) mask = vColor.b;
    if (uLayered.maskSource == 3) mask = vColor.a;
    if (uLayered.maskSource == 4) mask = vUV.y;
    if (uLayered.maskSource == 5) mask = 1.0 - vUV.y;
    mask = clamp(mask, 0.0, 1.0);

    vec3 c0 = EvalLayer(uLayered.base, N, V);
    vec3 c1 = EvalLayer(uLayered.top,  N, V);
    vec3 color = mix(c0, c1, mask);

    fragColor = vec4(color, uLayered.base.albedo.a);
}
