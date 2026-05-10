// unlit.vert.vk.glsl — Unlit Vertex (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 aPos; layout(location=3) in vec2 aUV; layout(location=5) in vec4 aColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(location=0) out vec2 vUV; layout(location=1) out vec4 vColor;
void main(){gl_Position=uCam.viewProj*(uObj.model*vec4(aPos,1.));gl_Position.y=-gl_Position.y;vUV=aUV;vColor=aColor*uObj.tint;}
