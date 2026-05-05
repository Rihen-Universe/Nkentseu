// unlit.frag.gl.glsl — Unlit Fragment (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(binding=4) uniform sampler2D tAlbedo;
void main(){fragColor=texture(tAlbedo,vUV)*vColor;}
