// toon.frag.vk.glsl — Toon Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=0,binding=4,std140) uniform ToonUBO{vec4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;vec4 outlineColor,rimColor;float specHardness;float _p[3];}uToon;
layout(set=1,binding=0) uniform sampler2D tAlbedo;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    vec3 albedo=albSamp.rgb; float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 N=normalize(vNormal); vec3 V=normalize(uCam.camPos.xyz-vWorldPos);
    vec3 totalLight=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        int lt=int(uLights.positions[i].w); vec3 L;
        if(lt==0){L=normalize(-uLights.directions[i].xyz);}
        else{L=normalize(uLights.positions[i].xyz-vWorldPos);}
        float NdL=dot(N,L);
        float toon=smoothstep(uToon.shadowThreshold-uToon.shadowSmoothness,
                               uToon.shadowThreshold+uToon.shadowSmoothness, NdL);
        vec3 litColor=mix(uToon.shadowColor.rgb,albedo,toon)*uLights.colors[i].rgb*uLights.colors[i].w;
        // Specular toon (hard step)
        vec3 H=normalize(V+L);
        float spec=step(0.5,pow(max(dot(N,H),0.),uToon.specHardness));
        litColor+=spec*0.3*uLights.colors[i].rgb;
        totalLight+=litColor;
    }
    // Rim light
    float rim=1.-max(dot(N,V),0.); rim=pow(rim,3.)*uToon.rimIntensity;
    totalLight+=uToon.rimColor.rgb*rim;
    fragColor=vec4(totalLight,alpha);
}