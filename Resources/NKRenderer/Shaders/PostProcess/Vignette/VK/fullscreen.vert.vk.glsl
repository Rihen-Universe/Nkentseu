// fullscreen.vert.vk.glsl — Fullscreen Triangle Vertex (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) out vec2 vUV;
void main(){
    vec2 pos[3]=vec2[3](vec2(-1.,-1.),vec2(3.,-1.),vec2(-1.,3.));
    gl_Position=vec4(pos[gl_VertexIndex],0.,1.);
    // Vulkan: Y is already correct in NDC for fullscreen tri
    vUV=vec2(pos[gl_VertexIndex].x*0.5+0.5, 0.5-pos[gl_VertexIndex].y*0.5);
}
