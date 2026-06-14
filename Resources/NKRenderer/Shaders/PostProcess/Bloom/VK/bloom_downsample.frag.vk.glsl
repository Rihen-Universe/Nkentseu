// bloom_downsample.frag.vk.glsl — Bloom Downsample (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tSrc;
layout(set=0,binding=1,std140) uniform BloomUBO{float threshold;float bloomStrength;float upsampleRadius;float _p;};
void main(){ // bloom_downsample.frag — Bloom Downsample 13-tap Karis Average
    vec2 ts=1.0/vec2(textureSize(tSrc,0));
    vec3 a=texture(tSrc,vUV+ts*vec2(-2, 2)).rgb; vec3 b=texture(tSrc,vUV+ts*vec2( 0, 2)).rgb;
    vec3 c=texture(tSrc,vUV+ts*vec2( 2, 2)).rgb; vec3 d=texture(tSrc,vUV+ts*vec2(-2, 0)).rgb;
    vec3 e=texture(tSrc,vUV).rgb;                vec3 f=texture(tSrc,vUV+ts*vec2( 2, 0)).rgb;
    vec3 g=texture(tSrc,vUV+ts*vec2(-2,-2)).rgb; vec3 h=texture(tSrc,vUV+ts*vec2( 0,-2)).rgb;
    vec3 k=texture(tSrc,vUV+ts*vec2( 2,-2)).rgb; vec3 l=texture(tSrc,vUV+ts*vec2(-1, 1)).rgb;
    vec3 m=texture(tSrc,vUV+ts*vec2( 1, 1)).rgb; vec3 n=texture(tSrc,vUV+ts*vec2(-1,-1)).rgb;
    vec3 o=texture(tSrc,vUV+ts*vec2( 1,-1)).rgb;
    vec3 result=e*.125+(a+c+g+k)*.03125+(b+d+f+h)*.0625+(l+m+n+o)*.125;
    float lum=dot(result,vec3(0.2126,0.7152,0.0722));
    result*=smoothstep(threshold-.1,threshold+.1,lum);
    fragColor=vec4(result,1.); }
