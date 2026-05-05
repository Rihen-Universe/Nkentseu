// particles.frag.vk.glsl — Particle Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 gUV;
layout(location=1) in vec4 gColor;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tParticle;
void main(){
    vec4 t=texture(tParticle,gUV)*gColor;
    if(t.a<0.02)discard;
    fragColor=t;
}
