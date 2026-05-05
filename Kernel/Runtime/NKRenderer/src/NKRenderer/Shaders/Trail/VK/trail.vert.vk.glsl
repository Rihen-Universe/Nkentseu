// trail.vert.vk.glsl — NKRenderer v4.0 — Trail Ribbon Vertex (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(location=0) out vec2 vUV; layout(location=1) out vec4 vColor;
void main(){gl_Position=uCam.viewProj*vec4(aPos,1.);vUV=aUV;vColor=aColor;}
