// particles.vert.gl.glsl — Particle Billboard Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in float aSize;
layout(location=4) in float aRotation;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
void main(){
    // Pass-through: geometry shader handles billboard expansion
    gl_Position=viewProj*vec4(aPos,1.);
    vUV=aUV;
    vColor=vec4(float((aColor    )&0xFFu)/255.,float((aColor>> 8u)&0xFFu)/255.,float((aColor>>16u)&0xFFu)/255.,float((aColor>>24u)&0xFFu)/255.);
}
