// taa.frag.gl.glsl — NKRenderer v4.0 — TAA (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tCurrent;
layout(binding=1) uniform sampler2D tHistory;
layout(binding=2) uniform sampler2D tVelocity;
layout(std140,binding=3) uniform TAAUBO{float blendFactor;float _p[3];}uTAA;
void main(){
    vec2 vel=texture(tVelocity,vUV).rg;
    vec2 histUV=vUV-vel;
    vec3 curr=texture(tCurrent,vUV).rgb;
    // Neighbourhood clamping (AABB)
    vec3 nMin=curr,nMax=curr;
    vec2 ts=1./vec2(textureSize(tCurrent,0));
    for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
        vec3 n=texture(tCurrent,vUV+vec2(x,y)*ts).rgb;
        nMin=min(nMin,n); nMax=max(nMax,n);
    }
    vec3 hist=clamp(texture(tHistory,histUV).rgb,nMin,nMax);
    // Blend current with clamped history
    float blend=uTAA.blendFactor;
    if(histUV.x<0.||histUV.x>1.||histUV.y<0.||histUV.y>1.) blend=1.;
    fragColor=vec4(mix(hist,curr,blend),1.);
}
