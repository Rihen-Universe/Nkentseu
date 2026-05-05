// water.vert.vk.glsl — NKRenderer v4.0 — Water Vertex with Gerstner Waves (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNormal; layout(location=3) in vec2 aUV;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(set=0,binding=10,std140) uniform WaterUBO{
    vec4 waveDir[4];   // xy=direction, z=amplitude, w=wavelength
    float speed;float foamThreshold;float refractionStrength;float _p;
}uWater;
layout(location=0) out vec3 vWorldPos; layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV; layout(location=3) out vec3 vViewDir;
const float PI=3.14159265;
// Gerstner wave displacement
vec3 GerstnerWave(vec4 waveDef, vec3 pos, float t, inout vec3 tangent, inout vec3 binormal){
    float k=2.*PI/max(waveDef.w,0.01);
    float speed_=sqrt(9.81/k);
    float f=k*(dot(waveDef.xy,pos.xz)-speed_*t);
    float A=waveDef.z;
    tangent+=vec3(1.-(waveDef.x*waveDef.x)*(A*k*sin(f)),waveDef.x*A*k*cos(f),-waveDef.x*waveDef.y*A*k*sin(f));
    binormal+=vec3(-waveDef.x*waveDef.y*A*k*sin(f),waveDef.y*A*k*cos(f),1.-(waveDef.y*waveDef.y)*(A*k*sin(f)));
    return vec3(waveDef.x*A*cos(f),A*sin(f),waveDef.y*A*cos(f));
    gl_Position.y=-gl_Position.y;
}
void main(){
    vec4 wp=uObj.model*vec4(aPos,1.);
    float t=uCam.time*uWater.speed;
    vec3 tang=vec3(1.,0.,0.), binom=vec3(0.,0.,1.), disp=vec3(0.);
    for(int i=0;i<4;i++) disp+=GerstnerWave(uWater.waveDir[i],wp.xyz,t,tang,binom);
    wp.xyz+=disp;
    vWorldPos=wp.xyz; vNormal=normalize(cross(binom,tang));
    vUV=aUV+vec2(uCam.time*0.02,uCam.time*0.01);
    vViewDir=normalize(uCam.camPos.xyz-wp.xyz);
    gl_Position=uCam.viewProj*wp;
    gl_Position.y=-gl_Position.y;
}
