// carpaint.frag.vk.glsl — NKRenderer v4.0 — Car Paint (Vulkan)
// Flake sparkle + clearcoat + metallic base
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;  layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV; layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=0,binding=8,std140) uniform CarPaintUBO{float flakeScale,flakeIntensity,specF0,_pad;}uCar;
layout(set=1,binding=4) uniform sampler2D tAlbedo;
layout(set=1,binding=5) uniform sampler2D tNormal;
layout(set=1,binding=6) uniform sampler2D tORM;
layout(set=1,binding=7) uniform sampler2D tFlake;          // sparkle normal map
layout(set=1,binding=8) uniform samplerCube tEnvIrradiance;
layout(set=1,binding=9) uniform samplerCube tEnvPrefilter;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; vec3 albedo=pow(albSamp.rgb,vec3(2.2)); float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 orm=texture(tORM,vUV).rgb; float ao=orm.r*uObj.aoStr,rog=orm.g*uObj.roughness,met=orm.b*uObj.metallic;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; nTs.xy*=uObj.normalStrength;
    // Flake normal: high-frequency noise modulating normal
    vec3 flakeN=texture(tFlake,vUV*uCar.flakeScale).xyz*2.-1.;
    flakeN=normalize(mix(nTs,flakeN,uCar.flakeIntensity));
    mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*flakeN), V=normalize(uCam.camPos.xyz-vWorldPos);
    // Base metallic PBR
    vec3 F0=mix(vec3(uCar.specF0),albedo,met);
    vec3 Lo=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        vec3 H=normalize(V+L); float NdL=max(dot(N,L),0.),NdH=max(dot(N,H),0.);
        float a=rog*rog,a2=a*a,d=NdH*NdH*(a2-1.)+1.;
        float NDF=a2/(3.14159*(d*d)+0.0001);
        float G=NdL/(NdL*(1.-rog/2.)+rog/2.)*max(dot(N,V),0.)/(max(dot(N,V),0.)*(1.-rog/2.)+rog/2.);
        vec3 F=F0+(1.-F0)*pow(max(1.-dot(H,V),0.),5.);
        vec3 spec=NDF*G*F/(4.*max(dot(N,V),0.)*NdL+0.001);
        Lo+=(albedo/3.14159*(1.-F)*(1.-met)+spec)*uLights.colors[i].rgb*uLights.colors[i].w*NdL;
    }
    // Clearcoat layer
    vec3 Ncc=normalize(TBN*nTs); // clearcoat uses base normal
    vec3 Fcc=vec3(uCar.specF0)+(1.-uCar.specF0)*pow(max(1.-max(dot(Ncc,V),0.),0.),5.);
    vec3 pref=textureLod(tEnvPrefilter,reflect(-V,Ncc),uObj.clearcoatRoughness*4.).rgb;
    vec3 ccContrib=pref*Fcc*uObj.clearcoat;
    vec3 amb=texture(tEnvIrradiance,N).rgb*albedo*ao*0.3;
    fragColor=vec4(amb+Lo+ccContrib,alpha);
}
