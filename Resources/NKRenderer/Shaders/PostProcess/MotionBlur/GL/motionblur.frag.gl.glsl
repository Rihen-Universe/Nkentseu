// motionblur.frag.gl.glsl — NKRenderer v4.0 — Motion Blur (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tHDR;
layout(binding=1) uniform sampler2D tVelocity;
layout(std140,binding=2) uniform MotionBlurUBO{float shutterAngle;int numSamples;float _p[2];}uMB;
void main(){
    vec2 vel=texture(tVelocity,vUV).rg*uMB.shutterAngle;
    if(length(vel)<0.0001){fragColor=texture(tHDR,vUV);return;}
    vec3 accum=vec3(0.);
    for(int i=0;i<uMB.numSamples;i++){
        float t=float(i)/float(max(uMB.numSamples-1,1));
        vec2 offset=vel*(t-0.5);
        accum+=texture(tHDR,vUV+offset).rgb;
    }
    fragColor=vec4(accum/float(uMB.numSamples),1.);
}
