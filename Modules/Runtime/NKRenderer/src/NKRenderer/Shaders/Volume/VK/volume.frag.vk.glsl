// volume.frag.vk.glsl — NKRenderer v4.0 — Volume / Fog / Smoke (Vulkan)
// Ray-marching volume: fog, smoke, flames
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos; layout(location=4) in vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(set=0,binding=13,std140) uniform VolumeUBO{vec4 absorptionColor;vec4 scatterColor;float density;float stepCount;float shadowDensity;float _p;}uVol;
layout(set=1,binding=4) uniform sampler3D tVolumeDensity;   // 3D density field
layout(set=1,binding=5) uniform sampler3D tVolumeAlbedo;    // 3D color field
void main(){
    // Ray march from camera to fragment
    vec3 rayDir=normalize(vWorldPos-uCam.camPos.xyz);
    vec3 rayPos=uCam.camPos.xyz;
    float stepSize=length(vWorldPos-uCam.camPos.xyz)/max(uVol.stepCount,1.);
    vec3 accum=vec3(0.); float transmittance=1.;
    for(float s=0.;s<uVol.stepCount;s++){
        vec3 samplePos=(rayPos+rayDir*s*stepSize-vWorldPos)*0.5+0.5;
        if(any(lessThan(samplePos,vec3(0.)))||any(greaterThan(samplePos,vec3(1.)))) continue;
        float d=texture(tVolumeDensity,samplePos).r*uVol.density;
        if(d<0.001) continue;
        vec3 albedo=texture(tVolumeAlbedo,samplePos).rgb;
        // Beer's law extinction
        float ext=exp(-d*stepSize);
        vec3 scatter=uVol.scatterColor.rgb*albedo*d;
        accum+=transmittance*(1.-ext)*scatter*uObj.tint.rgb;
        transmittance*=ext;
        if(transmittance<0.001) break;
    }
    fragColor=vec4(accum,1.-transmittance);
}
