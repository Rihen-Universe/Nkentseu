// hair.frag.vk.glsl — NKRenderer v4.0 — Hair Kajiya-Kay (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;  layout(location=4) in vec2 vUV;
layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 subsurfaceColor;}uObj;
layout(set=0,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=1,binding=0) uniform sampler2D tAlbedo;
layout(set=1,binding=1) uniform sampler2D tAlphaMask;  // hair opacity

void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    vec3 albedo=albSamp.rgb; float alpha=texture(tAlphaMask,vUV).r*albSamp.a;
    if(alpha<0.02) discard;
    vec3 T=normalize(vTangent); // hair strand tangent
    vec3 V=normalize(uCam.camPos.xyz-vWorldPos);
    vec3 Lo=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w;
        // Kajiya-Kay: diffuse
        float sinTL=length(cross(T,L));
        float diff=sinTL*max(dot(normalize(vNormal),L),0.0)*0.5+0.5;
        // Kajiya-Kay: specular (shift along tangent for Marschner R lobe)
        float shiftT=dot(T,L);
        float sinTV=length(cross(T,V));
        float spec=pow(max(shiftT*dot(T,V)+sinTL*sinTV,0.0),32.0);
        // Secondary specular (TT lobe - transmission)
        float spec2=pow(max(-shiftT*dot(T,V)+sinTL*sinTV,0.0),16.0)*0.5;
        Lo+=rad*(diff*albedo+spec*vec3(1.0,0.95,0.9)+spec2*albedo*0.3);
    }
    fragColor=vec4(Lo,alpha);
}
