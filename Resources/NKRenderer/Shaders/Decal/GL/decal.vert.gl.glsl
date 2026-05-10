// decal.vert.gl.glsl — NKRenderer v4.0 — Decal Projected Vertex (OpenGL 4.6)
// Unit cube drawn, projects albedo/normal onto scene via depth buffer
#version 460 core
layout(location=0) in vec3 aPos;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform DecalUBO{mat4 decalMatrix;float opacity;float normalBlend;float _p[2];}uDecal;
layout(location=0) out vec3 vDecalPos;
void main(){
    vec4 worldPos=uDecal.decalMatrix*vec4(aPos,1.);
    vDecalPos=aPos*0.5+0.5; // 0..1 in decal space
    gl_Position=uCam.viewProj*worldPos;
}
