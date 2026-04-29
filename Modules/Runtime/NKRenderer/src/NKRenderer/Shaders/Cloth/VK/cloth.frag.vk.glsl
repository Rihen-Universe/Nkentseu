// cloth.frag.vk.glsl — NKRenderer v4.0 — Cloth Ashikhmin-Shirley (Vulkan)
// Velvet and silk: cross-polarized retroreflection, sheen lobe
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;  layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV; layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=0,binding=7,std140) uniform ClothUBO{vec4 sheenColor;float sheenRoughness;float anisotropy;float _p[2];}uCloth;
layout(set=1,binding=4) uniform sampler2D tAlbedo;
layout(set=1,binding=5) uniform sampler2D tNormal;
layout(set=1,binding=6) uniform sampler2D tORM;
const float PI=3.14159265;
// Ashikhmin-Shirley anisotropic NDF for cloth
float D_Ashikhmin(float NdH, float roughness){
    float inv=1./(1.-NdH*NdH+0.0001);
    float r2=roughness*roughness;
    return r2/(PI*(1.+(r2-1.)*NdH*NdH)*(1.+(r2-1.)*NdH*NdH));
}
// Sheen lobe (Charlie distribution) for velvet
float D_Charlie(float NdH, float roughness){
    float inv=1.0/max(1.0-NdH*NdH,0.0001);
    float sinTheta=sqrt(1.0-NdH*NdH);
    float a=roughness*roughness;
    return (2.0+1.0/a)*pow(sinTheta,1.0/a)*0.5/PI;
}
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; vec3 albedo=pow(albSamp.rgb,vec3(2.2)); float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 orm=texture(tORM,vUV).rgb; float ao=orm.r*uObj.aoStr,rog=orm.g*uObj.roughness;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs), V=normalize(uCam.camPos.xyz-vWorldPos);
    vec3 Lo=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        vec3 H=normalize(V+L); float NdL=max(dot(N,L),0.),NdH=max(dot(N,H),0.);
        vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w;
        // Diffuse Lambertian
        vec3 diff=albedo/PI*NdL;
        // Sheen (velvet/silk highlight)
        float sheen=D_Charlie(NdH,uCloth.sheenRoughness)*NdL;
        vec3 sheenContrib=uCloth.sheenColor.rgb*sheen;
        // Anisotropic highlight along tangent
        float NdT=dot(N,vTangent); float sinNT=sqrt(1.-NdT*NdT);
        float TdL=dot(vTangent,L); float TdV=dot(vTangent,V);
        float aniso=sqrt(max(1.-TdL*TdL,0.)*max(1.-TdV*TdV,0.))-TdL*TdV;
        aniso=max(aniso,0.)*uCloth.anisotropy;
        Lo+=rad*(diff+sheenContrib+vec3(aniso)*0.1);
    }
    vec3 amb=albedo*0.05*ao;
    vec3 emissive=uObj.emissStr>0.?vec3(0.):vec3(0.);
    fragColor=vec4(amb+Lo,alpha);
}
