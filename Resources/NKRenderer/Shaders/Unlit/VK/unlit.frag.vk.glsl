// unlit.frag.vk.glsl — Unlit Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tAlbedo;
void main(){fragColor=texture(tAlbedo,vUV)*vColor;}
