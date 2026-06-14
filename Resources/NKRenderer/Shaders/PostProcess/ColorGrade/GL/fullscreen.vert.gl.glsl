// fullscreen.vert.gl.glsl — Fullscreen Triangle Vertex (OpenGL 4.6)
// No VBO needed: draws 3 vertices covering the screen
#version 460 core
layout(location=0) out vec2 vUV;
void main(){
    vec2 pos[3]=vec2[3](vec2(-1.,-1.),vec2(3.,-1.),vec2(-1.,3.));
    gl_Position=vec4(pos[gl_VertexID],0.,1.);
    vUV=pos[gl_VertexID]*0.5+0.5;
}
