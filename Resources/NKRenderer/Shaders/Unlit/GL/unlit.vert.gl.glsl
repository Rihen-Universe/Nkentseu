// unlit.vert.gl.glsl — Unlit Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 aPos; layout(location=3) in vec2 aUV; layout(location=5) in vec4 aColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(location=0) out vec2 vUV; layout(location=1) out vec4 vColor;
void main(){gl_Position=uCam.viewProj*(uObj.model*vec4(aPos,1.));vUV=aUV;vColor=aColor*uObj.tint;}
