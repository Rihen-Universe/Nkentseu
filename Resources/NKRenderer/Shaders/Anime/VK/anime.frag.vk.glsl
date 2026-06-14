// anime.frag.vk.glsl — NKRenderer v4.0 — Anime Shading (Vulkan)
// Hard shadow steps + strong rim + ink outline in fragment
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{
    mat4 view,proj,viewProj,invViewProj;
    vec4 camPos,camDir; vec2 viewport; float time,dt;
    float iblStrength;
    float _p0,_p1,_p2;
    mat4 mirrorViewProj;
    vec4 reflectionFlags;
}uCam;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
// Phase NkVSM : multi-lights shadow atlas.
struct NkShadowSlot {
    mat4 shadowMatrix;
    vec4 tileUV;
    vec4 lightPosOrDir;
    vec4 packedIds;
};
layout(std140,set=0,binding=3) uniform ShadowSlotsUBO {
    NkShadowSlot slots[256];
    vec4 firstSlotPerLight[8];
    vec4 slotCountPerLight[8];
    vec4 globalCfg;
    vec4 biasParams;
} uShadows;
layout(set=0,binding=11) uniform sampler2DShadow tShadowAtlas;
#include "Include/NkShadowAtlas.glsli"
// ToonUBO — meme layout que toon.frag (96 bytes std140) :
//   offset  0 : albedoColor (vec4)
//   offset 16 : shadowColor (vec4)
//   offset 32 : shadowThreshold, shadowSmoothness, outlineWidth, rimIntensity (4×float)
//   offset 48 : outlineColor (vec4)
//   offset 64 : rimColor (vec4)
//   offset 80 : specHardness, _pad[3] (4×float)
layout(set=2,binding=8,std140) uniform ToonUBO{vec4 albedoColor;vec4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;vec4 outlineColor,rimColor;float specHardness,metallic;float _p[2];}uToon;
layout(set=2,binding=3) uniform sampler2D tAlbedo;
layout(set=2,binding=4) uniform sampler2D tShadowRamp;  // custom shadow ramp (slot normal map)
void main(){
    // albedoColor est la source de verite du materiau (SetAlbedo).
    // vColor (ObjectUBO.tint) n'est pas multiplie pour eviter le double-tint.
    vec4 albSamp=texture(tAlbedo,vUV)*uToon.albedoColor; vec3 albedo=albSamp.rgb; float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 N=normalize(vNormal);
    vec3 camPosForV = uCam.camPos.xyz;
    if (uCam.reflectionFlags.x > 0.5) camPosForV.y = -camPosForV.y;
    vec3 V = normalize(camPosForV - vWorldPos);
    float NdotV=dot(N,V);
    // Outline: silhouette via N·V.
    // Guard NdotV > 0 : evite que le back-hemisphere devienne outline noir.
    // Pas de discard global sur NdotV<=0 (ferait disparaitre le corps sur mesh low-poly).
    if(uToon.outlineWidth>0.0&&NdotV>0.0){float edge=1.-NdotV;if(edge>1.-uToon.outlineWidth*0.1){fragColor=vec4(uToon.outlineColor.rgb,alpha);return;}}
    vec3 total=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        // NkVSM : shadow per-light via atlas. angles[i].z = castShadow flag.
        float shadow = (uLights.angles[i].z > 0.5)
                       ? SampleLightShadow(i, vWorldPos, N, L) : 1.0;
        float NdL=dot(N,L)*0.5+0.5; // half-lambert for anime
        // Ramp texture for stylized shadow
        float rampU=texture(tShadowRamp,vec2(NdL,0.5)).r;
        vec3 lit=mix(uToon.shadowColor.rgb*albedo,albedo,rampU)*uLights.colors[i].rgb*uLights.colors[i].w*shadow;
        // Hard specular — teinté par albedo si metallic > 0 (effet métal cel)
        vec3 H=normalize(V+L);
        float spec=step(0.85,pow(max(dot(N,H),0.),uToon.specHardness));
        vec3 specColor=mix(vec3(1.),albedo,uToon.metallic);
        lit+=spec*specColor*uLights.colors[i].w*0.4;
        total+=lit;
    }
    float rim=pow(1.-max(NdotV,0.),2.5)*uToon.rimIntensity;
    total+=uToon.rimColor.rgb*rim;
    fragColor=vec4(total,alpha);
}
