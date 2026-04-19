// =============================================================================
// pbr_metallic.gl.frag — OpenGL GLSL Fragment Shader
// DIFFÉRENCES vs Vulkan GLSL :
//   1. layout(binding=N) uniform sampler2D — pas de set
//   2. out vec4 fragColor — pas de location parfois nécessaire
//      mais layout(location=0) out est préférable pour MRT
//   3. texture() fonctionne identiquement (GLSL standard)
//   4. Les shadow maps PCF utilisent sampler2DShadow pour le hardware PCF
//      (disponible en GL 1.4+) → pas besoin de comparer manuellement
//   5. gl_FragCoord disponible (comme vk)
//   6. textureSize(), textureLod() — identiques à VK GLSL
// =============================================================================
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec4 vTangent;
layout(location=3) in vec2 vUV0;
layout(location=4) in vec2 vUV1;
layout(location=5) in vec3 vViewDir;
layout(location=6) in vec4 vShadowCoord[4];

layout(location=0) out vec4 fragColor;

// ── UBOs OpenGL (binding sans set) ────────────────────────────────────────────
layout(binding=0, std140) uniform NkFrameData {
    mat4  nk_View, nk_Projection, nk_ViewProjection;
    mat4  nk_InvViewProjection, nk_PrevViewProjection;
    vec4  nk_CameraPosition, nk_CameraDirection, nk_ViewportSize, nk_Time;
    vec4  nk_SunDirection, nk_SunColor;
    mat4  nk_ShadowMatrix[4]; vec4 nk_CascadeSplits;
    float nk_EnvIntensity, nk_EnvRotation;
    uint  nk_LightCount, nk_FrameIndex;
};

layout(binding=1, std140) uniform ObjectData { mat4 modelMatrix; vec4 customColor; uint matId; uint p[3]; };

layout(binding=2, std140) uniform MaterialData {
    vec4  albedo, emissive;
    float metallic, roughness, ao, emissiveStrength;
    float clearcoat, clearcoatRough, normalStrength, uvScaleX;
    float uvScaleY, uvOffsetX, uvOffsetY, transmission;
} mat;

// ── Textures OpenGL — uniform sampler (pas de layout set) ────────────────────
layout(binding=3) uniform sampler2D   tAlbedo;
layout(binding=4) uniform sampler2D   tNormal;
layout(binding=5) uniform sampler2D   tMetalRough;
layout(binding=6) uniform sampler2D   tAO;
layout(binding=7) uniform sampler2D   tEmissive;
// OpenGL: sampler2DShadow pour le hardware PCF (compare automatiquement)
layout(binding=8) uniform sampler2DShadow tShadowMap;
layout(binding=9)  uniform samplerCube tEnvIrradiance;
layout(binding=10) uniform samplerCube tEnvPrefilter;
layout(binding=11) uniform sampler2D   tBRDFLUT;

const float PI = 3.14159265359;

float D_GGX(float NdotH,float r){float a2=pow(r,4.0);float d=NdotH*NdotH*(a2-1.0)+1.0;return a2/(PI*d*d);}
float G_Smith(float NdotV,float NdotL,float r){float k=(r+1.0)*(r+1.0)/8.0;return(NdotV/(NdotV*(1.0-k)+k))*(NdotL/(NdotL*(1.0-k)+k));}
vec3 F_Schlick(float c,vec3 F0){return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}
vec3 F_SchlickR(float c,vec3 F0,float r){return F0+(max(vec3(1.0-r),F0)-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}

// OpenGL shadow avec sampler2DShadow (hardware PCF intégré)
// textureProj() effectue la division perspective + compare automatiquement
float ShadowGL_HW(sampler2DShadow sm, vec4 sc) {
    // textureProj avec sampler2DShadow fait : compare depth et retourne [0,1]
    // PCF via textureOffset pour un kernel 3x3
    float shadow=0.0;
    vec2 ts=1.0/vec2(textureSize(sm,0));
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        shadow+=textureProj(sm, vec4(sc.xy/sc.w+vec2(x,y)*ts*sc.w, sc.z-0.005*sc.w, sc.w));
    }
    return shadow/9.0;
}

void main() {
    vec2 uv = vUV0 * vec2(mat.uvScaleX,mat.uvScaleY) + vec2(mat.uvOffsetX,mat.uvOffsetY);
    vec3 V  = normalize(vViewDir);
    vec3 N  = normalize(vNormal);
    vec3 T  = normalize(vTangent.xyz - dot(vTangent.xyz,N)*N);
    vec3 B  = cross(N,T)*vTangent.w;
    mat3 TBN= mat3(T,B,N);
    vec3 tn = texture(tNormal,uv).rgb*2.0-1.0;
    tn.xy  *= mat.normalStrength;
    N = normalize(TBN*tn);

    vec4  albTex= texture(tAlbedo,uv);
    vec3  base  = albTex.rgb*mat.albedo.rgb*customColor.rgb;
    vec2  mr    = texture(tMetalRough,uv).rg;
    float metal = mr.r*mat.metallic, rough=max(mr.g*mat.roughness,0.04);
    float ao    = texture(tAO,uv).r;
    vec3  F0    = mix(vec3(0.04),base,metal);

    float viewD = -(nk_View*vec4(vWorldPos,1.0)).z;
    float shadow= viewD<nk_CascadeSplits.x ? ShadowGL_HW(tShadowMap,vShadowCoord[0]):
                  viewD<nk_CascadeSplits.y ? ShadowGL_HW(tShadowMap,vShadowCoord[1]):
                  viewD<nk_CascadeSplits.z ? ShadowGL_HW(tShadowMap,vShadowCoord[2]):
                                              ShadowGL_HW(tShadowMap,vShadowCoord[3]);

    vec3  L=normalize(-nk_SunDirection.xyz),H=normalize(V+L);
    float NdotL=max(dot(N,L),0.),NdotV=max(dot(N,V),0.),NdotH=max(dot(N,H),0.),HdotV=max(dot(H,V),0.);
    vec3  F=F_Schlick(HdotV,F0);
    vec3  kD=(1.0-F)*(1.0-metal);
    vec3  Lo=(kD*base/PI+D_GGX(NdotH,rough)*G_Smith(NdotV,NdotL,rough)*F/max(4.0*NdotV*NdotL,0.001))*NdotL*nk_SunColor.xyz*nk_SunColor.w*shadow;

    float er=nk_EnvRotation;
    vec3 envN=vec3(N.x*cos(er)-N.z*sin(er),N.y,N.x*sin(er)+N.z*cos(er));
    vec3 envR=reflect(-V,N); envR=vec3(envR.x*cos(er)-envR.z*sin(er),envR.y,envR.x*sin(er)+envR.z*cos(er));
    vec3 irrad=texture(tEnvIrradiance,envN).rgb;
    vec3 pref =textureLod(tEnvPrefilter,envR,rough*7.0).rgb;
    vec2 brdf =texture(tBRDFLUT,vec2(NdotV,rough)).rg;
    vec3 kD2  =(1.0-F_SchlickR(NdotV,F0,rough))*(1.0-metal);
    vec3 ambient=(kD2*irrad*base+pref*(F_SchlickR(NdotV,F0,rough)*brdf.x+brdf.y))*ao*nk_EnvIntensity;

    vec3 em=texture(tEmissive,uv).rgb*mat.emissive.rgb*mat.emissiveStrength;
    fragColor=vec4((Lo+ambient+em)*nk_Time.w, albTex.a*mat.albedo.a);
}
