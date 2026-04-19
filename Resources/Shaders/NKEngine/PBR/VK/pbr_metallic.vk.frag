// =============================================================================
// pbr_metallic.vk.frag — Shader Fragment PBR Vulkan GLSL 4.50
// =============================================================================
#version 450

layout(location=0) in vec3  iFragPos;
layout(location=1) in vec3  iNormal;
layout(location=2) in vec3  iTangent;
layout(location=3) in vec3  iBitangent;
layout(location=4) in vec2  iUV;
layout(location=5) in vec4  iColor;
layout(location=6) in vec4  iFragPosLS;

layout(set=0, binding=0) uniform SceneUBO {
    mat4 uModel; mat4 uView; mat4 uProj; mat4 uLightVP;
    vec4 uLightDir; vec4 uEyePos;
    float uTime; float uDeltaTime; float uNdcZScale; float uNdcZOffset;
};
layout(set=2, binding=0) uniform MaterialUBO {
    vec4 uBaseColor; vec4 uEmissiveColor;
    float uMetallic; float uRoughness; float uOcclusion; float uNormalScale;
    float uAlphaClip; float uEmissiveScale;
    float uClearcoat; float uClearcoatRoughness;
    float uTransmission; float uIOR; float _p0; float _p1;
};

layout(set=1, binding=0) uniform sampler2D   uAlbedoMap;
layout(set=1, binding=1) uniform sampler2D   uNormalMap;
layout(set=1, binding=2) uniform sampler2D   uORMMap;
layout(set=1, binding=3) uniform sampler2D   uEmissiveMap;
layout(set=1, binding=4) uniform sampler2D   uShadowMap;
layout(set=1, binding=5) uniform samplerCube uEnvIrradiance;
layout(set=1, binding=6) uniform samplerCube uEnvPrefilter;
layout(set=1, binding=7) uniform sampler2D   uBRDF_LUT;

layout(location=0) out vec4 FragColor;

const float PI = 3.14159265;
const float EPS = 0.0001;

float D_GGX(vec3 N, vec3 H, float r) {
    float a2 = r*r*r*r;
    float NdH = max(dot(N,H),0.0);
    float d = NdH*NdH*(a2-1.0)+1.0;
    return a2/(PI*d*d);
}
float G_Schlick(float NdV, float r) {
    float k=(r+1.0)*(r+1.0)/8.0;
    return NdV/(NdV*(1.0-k)+k);
}
float G_Smith(vec3 N, vec3 V, vec3 L, float r) {
    return G_Schlick(max(dot(N,V),0.0),r)*G_Schlick(max(dot(N,L),0.0),r);
}
vec3 F_Schlick(float c, vec3 F0) {
    return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);
}
vec3 F_SchlickR(float c, vec3 F0, float r) {
    return F0+(max(vec3(1.0-r),F0)-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);
}
float Shadow(vec4 ls, vec3 N, vec3 L) {
    vec3 p = ls.xyz/ls.w;
    p = p*uNdcZScale+uNdcZOffset;
    if (any(lessThan(p,vec3(0.0))) || any(greaterThan(p,vec3(1.0)))) return 0.0;
    float bias=max(0.005*(1.0-dot(N,L)),0.0005);
    float shadow=0.0;
    vec2 ts=1.0/textureSize(uShadowMap,0);
    for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y) {
        float d=texture(uShadowMap,p.xy+vec2(x,y)*ts).r;
        shadow+=p.z-bias>d?1.0:0.0;
    }
    return shadow/9.0;
}
vec3 GetNormal() {
    vec3 tn=texture(uNormalMap,iUV).xyz*2.0-1.0;
    tn.xy*=uNormalScale;
    mat3 TBN=mat3(normalize(iTangent),normalize(iBitangent),normalize(iNormal));
    return normalize(TBN*tn);
}

void main() {
    vec4 albedoT = texture(uAlbedoMap, iUV);
    vec4 orm     = texture(uORMMap,    iUV);
    vec4 emitT   = texture(uEmissiveMap, iUV);

    vec4 albedo   = pow(albedoT, vec4(2.2)) * uBaseColor * iColor;
    if (albedo.a < uAlphaClip) discard;

    float ao       = orm.r * uOcclusion;
    float rough    = clamp(orm.g * uRoughness, 0.04, 1.0);
    float metal    = orm.b * uMetallic;

    vec3 N = GetNormal();
    vec3 V = normalize(uEyePos.xyz - iFragPos);
    vec3 R = reflect(-V, N);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metal);

    // Directional light
    vec3 Lo = vec3(0.0);
    {
        vec3 L  = normalize(-uLightDir.xyz);
        vec3 H  = normalize(V+L);
        float NdL = max(dot(N,L),0.0);
        float D = D_GGX(N,H,rough);
        float G = G_Smith(N,V,L,rough);
        vec3  F = F_Schlick(max(dot(H,V),0.0),F0);
        vec3 spec = (D*G*F)/(4.0*max(dot(N,V),0.0)*NdL+EPS);
        vec3 kD = (vec3(1.0)-F)*(1.0-metal);
        float shadow = Shadow(iFragPosLS, N, L);
        Lo += (kD*albedo.rgb/PI + spec)*vec3(5.0)*NdL*(1.0-shadow);
    }

    // IBL
    vec3 F_ibl  = F_SchlickR(max(dot(N,V),0.0),F0,rough);
    vec3 kD_ibl = (vec3(1.0)-F_ibl)*(1.0-metal);
    vec3 irrad  = texture(uEnvIrradiance, N).rgb;
    vec3 diffIBL= irrad * albedo.rgb;
    vec3 prefilt= textureLod(uEnvPrefilter, R, rough*4.0).rgb;
    vec2 brdf   = texture(uBRDF_LUT, vec2(max(dot(N,V),0.0), rough)).rg;
    vec3 specIBL= prefilt*(F_ibl*brdf.x+brdf.y);
    vec3 ambient= (kD_ibl*diffIBL + specIBL)*ao;

    // Emissive
    vec3 emit = (pow(emitT.rgb,vec3(2.2))+uEmissiveColor.rgb)*uEmissiveScale;

    FragColor = vec4(ambient + Lo + emit, albedo.a);
}
