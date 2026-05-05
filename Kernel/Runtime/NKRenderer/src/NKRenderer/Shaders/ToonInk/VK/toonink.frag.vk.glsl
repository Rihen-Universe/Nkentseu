// toonink.frag.vk.glsl — NKRenderer v4.0 — Toon Ink / Manga (Vulkan)
// Cel shading + hatching + strong ink outlines
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=0,binding=4,std140) uniform ToonUBO{vec4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;vec4 outlineColor,rimColor;float specHardness;float _p[3];}uToon;
layout(set=1,binding=4) uniform sampler2D tAlbedo;
layout(set=1,binding=5) uniform sampler2D tHatch;     // crosshatch pattern texture
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;vec3 albedo=albSamp.rgb;float alpha=albSamp.a;if(alpha<0.01)discard;
    vec3 N=normalize(vNormal),V=normalize(uCam.camPos.xyz-vWorldPos);
    // Edge outline via derivative
    float edge=length(vec2(dFdx(dot(N,V)),dFdy(dot(N,V))));
    float inkLine=smoothstep(0.05,0.1,edge)*uToon.outlineWidth;
    vec3 total=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=dot(N,L);
        float toon=smoothstep(uToon.shadowThreshold-uToon.shadowSmoothness,uToon.shadowThreshold+uToon.shadowSmoothness,NdL);
        // Hatching in shadow areas
        float hatch=texture(tHatch,vUV*4.).r;
        float shadowHatch=mix(0.,hatch,1.-toon);
        vec3 lit=mix(uToon.shadowColor.rgb,albedo,toon-shadowHatch*0.3)*uLights.colors[i].rgb*uLights.colors[i].w;
        total+=lit;
    }
    // Ink outline color mixing
    total=mix(total,uToon.outlineColor.rgb,inkLine);
    fragColor=vec4(total,alpha);
}
