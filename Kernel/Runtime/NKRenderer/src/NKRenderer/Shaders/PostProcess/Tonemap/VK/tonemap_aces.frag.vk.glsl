// tonemap_aces.frag.vk.glsl — ACES Filmic Tonemap (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=0,binding=0) uniform sampler2D tHDR;
layout(set=0,binding=1,std140) uniform TonemapUBO{float exposure,gamma,contrast,saturation;float _p[4];};
void main(){    vec3 x=texture(tHDR,vUV).rgb*exposure;
    float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
    vec3 ldr=clamp((x*(a*x+b))/(x*(c*x+d)+e),0.,1.);
    ldr=pow(ldr,vec3(1./gamma));
    fragColor=vec4(ldr,1.);}
