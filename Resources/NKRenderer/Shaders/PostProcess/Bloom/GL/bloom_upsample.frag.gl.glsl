// bloom_upsample.frag.gl.glsl — Bloom Upsample (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tSrc; layout(binding=1) uniform sampler2D tDst;
layout(std140,binding=2) uniform BloomUBO{float threshold;float bloomStrength;float upsampleRadius;float _p;};
void main(){ // bloom_upsample.frag — Bloom Upsample (tent filter)
    vec2 ts=1.0/vec2(textureSize(tSrc,0))*upsampleRadius;
    vec3 s=texture(tSrc,vUV+ts*vec2(-1,1)).rgb+texture(tSrc,vUV+ts*vec2(0,2)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(1,1)).rgb+texture(tSrc,vUV+ts*vec2(-2,0)).rgb*2.
           +texture(tSrc,vUV).rgb*4.+texture(tSrc,vUV+ts*vec2(2,0)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(-1,-1)).rgb+texture(tSrc,vUV+ts*vec2(0,-2)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(1,-1)).rgb;
    fragColor=vec4(s/16.*bloomStrength+texture(tDst,vUV).rgb,1.); }
