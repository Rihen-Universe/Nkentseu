// vignette.frag.vk.glsl — NKRenderer v4.0 — Vignette + Film Grain (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tLDR;
layout(set=0,binding=1,std140) uniform VignetteUBO{float vigIntensity,vigSmooth,grainStrength;float time;}uVig;
float Hash(vec2 p){return fract(sin(dot(p+uVig.time,vec2(127.1,311.7)))*43758.5453);}
void main(){
    vec3 c=texture(tLDR,vUV).rgb;
    // Vignette
    vec2 uv=vUV*2.-1.;
    float vig=smoothstep(1.,1.-uVig.vigSmooth,length(uv)*uVig.vigIntensity);
    c*=vig;
    // Film grain
    float grain=Hash(vUV)-.5;
    c+=grain*uVig.grainStrength;
    fragColor=vec4(clamp(c,0.,1.),1.);
}
