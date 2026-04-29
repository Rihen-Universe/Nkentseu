// decal.vert.vk.glsl — NKRenderer v4.0 — Decal Projected Vertex (Vulkan)
// Unit cube drawn, projects albedo/normal onto scene via depth buffer
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 aPos;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform DecalUBO{mat4 decalMatrix;float opacity;float normalBlend;float _p[2];}uDecal;
layout(location=0) out vec3 vDecalPos;
void main(){
    vec4 worldPos=uDecal.decalMatrix*vec4(aPos,1.);
    vDecalPos=aPos*0.5+0.5; // 0..1 in decal space
    gl_Position=uCam.viewProj*worldPos;
    gl_Position.y=-gl_Position.y;
}
