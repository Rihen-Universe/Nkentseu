// ============================================================
// pbr.frag.vk.glsl — NKRenderer v4.0 — PBR Fragment (Vulkan)
// ============================================================
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;
layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;
layout(location=7) in vec4 vShadowCoord0;
layout(location=8) in vec4 vShadowCoord1;
layout(location=9) in vec4 vShadowCoord2;
layout(location=10) in vec4 vShadowCoord3;

layout(location=0) out vec4 fragColor;

layout(set=0,binding=0,std140) uniform CameraUBO {
    mat4 view,proj,viewProj,invViewProj; vec4 camPos,camDir; vec2 viewport; float time,dt;
} uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO {
    mat4 model,normalMatrix; vec4 tint;
    float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,ccRough,sss;
    vec4 sssColor;
} uObj;
layout(set=0,binding=2,std140) uniform LightsUBO {
    vec4 positions[32],colors[32],directions[32],angles[32];
    int count; int _pad[3];
} uLights;
layout(set=0,binding=3,std140) uniform ShadowUBO {
    mat4 cascadeMats[4]; float cascadeSplits[4];
    int cascadeCount; float shadowBias,normalBias; int softShadows;
} uShadow;

layout(set=1,binding=0) uniform sampler2D   tAlbedo;
layout(set=1,binding=1) uniform sampler2D   tNormal;
layout(set=1,binding=2) uniform sampler2D   tORM;
layout(set=1,binding=3) uniform sampler2D   tEmissive;
layout(set=1,binding=4) uniform samplerCube tEnvIrradiance;
layout(set=1,binding=5) uniform samplerCube tEnvPrefilter;
layout(set=1,binding=6) uniform sampler2D   tBRDFLUT;
layout(set=1,binding=7) uniform sampler2DShadow tShadowMap;

const float PI = 3.14159265358979;

float D_GGX(vec3 N,vec3 H,float r){float a=r*r,a2=a*a,NdH=max(dot(N,H),0.);float d=NdH*NdH*(a2-1.)+1.;return a2/(PI*d*d+1e-4);}
float G_S(float x,float k){return x/(x*(1.-k)+k);}
float G_Smith(vec3 N,vec3 V,vec3 L,float r){float k=(r+1.)*(r+1.)/8.;return G_S(max(dot(N,V),0.),k)*G_S(max(dot(N,L),0.),k);}
vec3 F_Sch(float c,vec3 F0){return F0+(1.-F0)*pow(max(1.-c,0.),5.);}
vec3 F_SchR(float c,vec3 F0,float r){return F0+(max(vec3(1.-r),F0)-F0)*pow(max(1.-c,0.),5.);}

float ShadowPCF(vec4 coord) {
    // Vulkan: Z in [0,1] — no remap needed for Z
    vec3 p = coord.xyz / coord.w;
    p.xy   = p.xy * 0.5 + 0.5;
    p.z   -= uShadow.shadowBias;
    if (any(lessThan(p,vec3(0.0)))||any(greaterThan(p,vec3(1.0)))) return 1.0;
    vec2 ts = 1.0/vec2(textureSize(tShadowMap,0));
    float s=0.0;
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++)
        s+=texture(tShadowMap,vec3(p.xy+vec2(x,y)*ts,p.z));
    return s/9.0;
}

float GetShadow() {
    float depth = abs((uCam.view*vec4(vWorldPos,1.0)).z);
    if (depth < uShadow.cascadeSplits[0]) return ShadowPCF(vShadowCoord0);
    if (depth < uShadow.cascadeSplits[1]) return ShadowPCF(vShadowCoord1);
    if (depth < uShadow.cascadeSplits[2]) return ShadowPCF(vShadowCoord2);
    return ShadowPCF(vShadowCoord3);
}

void main() {
    vec4 albSample = texture(tAlbedo,vUV)*vColor;
    vec3 albedo    = pow(albSample.rgb,vec3(2.2));
    float alpha    = albSample.a;
    if(alpha<0.01) discard;

    vec3 orm=texture(tORM,vUV).rgb;
    float ao=orm.r*uObj.aoStrength, rog=orm.g*uObj.roughness, met=orm.b*uObj.metallic;

    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; nTs.xy*=uObj.normalStrength;
    mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs), V=normalize(uCam.camPos.xyz-vWorldPos);
    vec3 F0=mix(vec3(0.04),albedo,met);

    vec3 Lo=vec3(0.); float shadow=GetShadow();
    for(int i=0;i<uLights.count&&i<32;i++){
        int lt=int(uLights.positions[i].w); vec3 L; float att=1.;
        if(lt==0){L=normalize(-uLights.directions[i].xyz);}
        else{vec3 d=uLights.positions[i].xyz-vWorldPos;float dist=length(d);L=normalize(d);att=max(1.-dist/max(uLights.directions[i].w,.001),0.);att*=att;
             if(lt==2){float th=dot(L,normalize(-uLights.directions[i].xyz));att*=clamp((th-uLights.angles[i].y)/(uLights.angles[i].x-uLights.angles[i].y+1e-4),0.,1.);}}
        float csf=(uLights.angles[i].z>.5)?shadow:1.;
        vec3 H=normalize(V+L); float NdL=max(dot(N,L),0.); vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w*att;
        float NDF=D_GGX(N,H,rog); float G=G_Smith(N,V,L,rog); vec3 F=F_Sch(max(dot(H,V),0.),F0);
        vec3 spec=NDF*G*F/(4.*max(dot(N,V),0.)*NdL+1e-4);
        Lo+=csf*(((1.-F)*(1.-met))*albedo/PI+spec)*rad*NdL;
    }

    vec3 Fi=F_SchR(max(dot(N,V),0.),F0,rog), kDi=(1.-Fi)*(1.-met);
    vec3 irr=texture(tEnvIrradiance,N).rgb, R=reflect(-V,N);
    vec3 pref=textureLod(tEnvPrefilter,R,rog*4.).rgb;
    vec2 brdf=texture(tBRDFLUT,vec2(max(dot(N,V),0.),rog)).rg;
    vec3 amb=(kDi*irr*albedo+pref*(Fi*brdf.x+brdf.y))*ao;

    vec3 emissive=texture(tEmissive,vUV).rgb*uObj.emissiveStrength;
    fragColor=vec4(amb+Lo+emissive,alpha);
}
