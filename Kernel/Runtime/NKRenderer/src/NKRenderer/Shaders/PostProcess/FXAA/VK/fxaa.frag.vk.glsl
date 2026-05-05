// fxaa.frag.vk.glsl — NKRenderer v4.0 — FXAA 3.11 (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tLDR;
void main(){
    vec3 rgb=texture(tLDR,vUV).rgb;
    // Luma
    float lumaM=dot(rgb,vec3(0.299,0.587,0.114));
    float lumaN=dot(textureOffset(tLDR,vUV,ivec2(0, 1)).rgb,vec3(0.299,0.587,0.114));
    float lumaS=dot(textureOffset(tLDR,vUV,ivec2(0,-1)).rgb,vec3(0.299,0.587,0.114));
    float lumaW=dot(textureOffset(tLDR,vUV,ivec2(-1,0)).rgb,vec3(0.299,0.587,0.114));
    float lumaE=dot(textureOffset(tLDR,vUV,ivec2( 1,0)).rgb,vec3(0.299,0.587,0.114));
    float lumaMin=min(lumaM,min(min(lumaN,lumaS),min(lumaW,lumaE)));
    float lumaMax=max(lumaM,max(max(lumaN,lumaS),max(lumaW,lumaE)));
    float range=lumaMax-lumaMin;
    if(range<max(0.0312,lumaMax*0.125)){fragColor=vec4(rgb,1.);return;}
    float lumaNW=dot(textureOffset(tLDR,vUV,ivec2(-1, 1)).rgb,vec3(0.299,0.587,0.114));
    float lumaNE=dot(textureOffset(tLDR,vUV,ivec2( 1, 1)).rgb,vec3(0.299,0.587,0.114));
    float lumaSW=dot(textureOffset(tLDR,vUV,ivec2(-1,-1)).rgb,vec3(0.299,0.587,0.114));
    float lumaSE=dot(textureOffset(tLDR,vUV,ivec2( 1,-1)).rgb,vec3(0.299,0.587,0.114));
    bool horzSpan=abs(-2.*lumaW+lumaNW+lumaSW)*2.+abs(-2.*lumaM+lumaN+lumaS)<abs(-2.*lumaN+lumaNW+lumaNE)*2.+abs(-2.*lumaM+lumaW+lumaE);
    vec2 ts=1./vec2(textureSize(tLDR,0));
    vec2 dir=horzSpan?vec2(0.,ts.y):vec2(ts.x,0.);
    vec3 c1=texture(tLDR,vUV-dir).rgb; vec3 c2=texture(tLDR,vUV+dir).rgb;
    float blendFactor=range/lumaMax;
    fragColor=vec4(mix(rgb,(c1+c2)*.5,blendFactor*.75),1.);}
