// bloom_upsample.frag.vk.glsl — Bloom Upsample (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tSrc; layout(set=1,binding=1) uniform sampler2D tDst;
layout(set=0,binding=2,std140) uniform BloomUBO{float threshold;float bloomStrength;float upsampleRadius;float _p;};
void main(){ // bloom_upsample.frag — Bloom Upsample (tent filter)
    vec2 ts=1.0/vec2(textureSize(tSrc,0))*upsampleRadius;
    vec3 s=texture(tSrc,vUV+ts*vec2(-1,1)).rgb+texture(tSrc,vUV+ts*vec2(0,2)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(1,1)).rgb+texture(tSrc,vUV+ts*vec2(-2,0)).rgb*2.
           +texture(tSrc,vUV).rgb*4.+texture(tSrc,vUV+ts*vec2(2,0)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(-1,-1)).rgb+texture(tSrc,vUV+ts*vec2(0,-2)).rgb*2.
           +texture(tSrc,vUV+ts*vec2(1,-1)).rgb;
    fragColor=vec4(s/16.*bloomStrength+texture(tDst,vUV).rgb,1.); }
