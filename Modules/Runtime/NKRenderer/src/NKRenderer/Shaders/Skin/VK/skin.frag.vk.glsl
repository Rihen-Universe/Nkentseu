// skin.frag.vk.glsl — Skin SSS Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos;layout(location=1) in vec3 vNormal;layout(location=2) in vec3 vTangent;layout(location=3) in vec3 vBitangent;layout(location=4) in vec2 vUV;layout(location=5) in vec2 vUV2;layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 subsurfaceColor;}uObj;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=1,binding=0) uniform sampler2D tAlbedo;layout(set=1,binding=1) uniform sampler2D tNormal;layout(set=1,binding=2) uniform sampler2D tORM;layout(set=1,binding=3) uniform sampler2D tEmissive;layout(set=1,binding=4) uniform samplerCube tEnvIrradiance;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    vec3 albedo=pow(albSamp.rgb,vec3(2.2)); float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 orm=texture(tORM,vUV).rgb;
    float ao=orm.r*uObj.aoStrength, rog=orm.g*uObj.roughness, met=orm.b*uObj.metallic;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; nTs.xy*=uObj.normalStrength;
    mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs), V=normalize(uCam.camPos.xyz-vWorldPos);
    // Skin: high scattering at grazing angles
    vec3 Lo=vec3(0.), sssOut=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=max(dot(N,L),0.);
        vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w;
        // Diffuse
        Lo+=NdL*albedo*rad;
        // SSS: wrap lighting + subsurface color bleed
        float wrapNdL=max(dot(N,L)+uObj.subsurface,0.)/(1.+uObj.subsurface);
        sssOut+=uObj.subsurfaceColor.rgb*albedo*rad*wrapNdL*uObj.subsurface;
        // Specular (Beckmann for skin)
        vec3 H=normalize(V+L);
        float spec=pow(max(dot(N,H),0.),32.)*NdL;
        Lo+=spec*vec3(0.3)*rad;
    }
    // Ambient + IBL approximation
    vec3 amb=texture(tEnvIrradiance,N).rgb*albedo*ao*0.3;
    // Emissive (blush map in B channel of emissive)
    vec3 emissive=texture(tEmissive,vUV).rgb*uObj.emissiveStrength;
    fragColor=vec4(amb+Lo+sssOut+emissive, alpha);
}