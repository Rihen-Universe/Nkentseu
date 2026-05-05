// water.frag.gl.glsl — NKRenderer v4.0 — Water Fragment (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec3 vViewDir;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(std140,binding=10) uniform WaterUBO{vec4 waveDir[4];float speed,foamThreshold,refractionStrength,_p;}uWater;
layout(binding=4) uniform sampler2D tNormal1;      // flowing normal map 1
layout(binding=5) uniform sampler2D tNormal2;      // flowing normal map 2 (different scale/speed)
layout(binding=6) uniform sampler2D tFoam;         // foam texture
layout(binding=7) uniform sampler2D tSceneColor;   // refraction (scene behind water)
layout(binding=8) uniform samplerCube tEnvPrefilter;// reflection
layout(binding=9) uniform sampler2D tDepth;        // for depth-based foam
void main(){
    // Blend two scrolling normals
    vec2 uv1=vUV+vec2(uCam.time*0.03,0.);
    vec2 uv2=vUV*1.5+vec2(-uCam.time*0.02,uCam.time*0.015);
    vec3 n1=texture(tNormal1,uv1).xyz*2.-1.;
    vec3 n2=texture(tNormal2,uv2).xyz*2.-1.;
    vec3 N=normalize(vNormal+n1*0.5+n2*0.3);
    vec3 V=normalize(vViewDir);
    // Fresnel
    float cosV=max(dot(N,V),0.);
    float F=0.04+(1.-0.04)*pow(1.-cosV,5.);
    // Refraction (screen-space distortion)
    vec2 screenUV=gl_FragCoord.xy/uCam.viewport;
    vec2 distort=(n1.xy+n2.xy)*uWater.refractionStrength*0.02;
    vec3 refracted=texture(tSceneColor,screenUV+distort).rgb;
    // Deep water tint
    vec3 waterColor=vec3(0.02,0.15,0.25);
    refracted=mix(refracted,waterColor,0.4);
    // Reflection from env cubemap
    vec3 R=reflect(-V,N);
    vec3 reflected=textureLod(tEnvPrefilter,R,0.5).rgb;
    // Specular highlights from lights
    vec3 spec=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        vec3 H=normalize(V+L);
        float NdH=max(dot(N,H),0.);
        spec+=pow(NdH,512.)*uLights.colors[i].rgb*uLights.colors[i].w*0.5;
    }
    // Foam near shore (using foam texture + depth)
    float foam=texture(tFoam,vUV*3.+vec2(uCam.time*0.1)).r;
    foam*=uWater.foamThreshold;
    // Combine
    vec3 color=mix(refracted,reflected,F)+spec+vec3(foam)*0.3;
    fragColor=vec4(color,0.9);
}
