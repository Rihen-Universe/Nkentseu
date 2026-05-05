// particles.vert.vk.glsl — Particle Billboard Vertex (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in float aSize;
layout(location=4) in float aRotation;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
void main(){
    // Pass-through: geometry shader handles billboard expansion
    gl_Position=uCam.viewProj*vec4(aPos,1.);
    gl_Position.y=-gl_Position.y;
    vUV=aUV;
    vColor=vec4(float((aColor    )&0xFFu)/255.,float((aColor>> 8u)&0xFFu)/255.,float((aColor>>16u)&0xFFu)/255.,float((aColor>>24u)&0xFFu)/255.);
}
