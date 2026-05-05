// trail.vert.gl.glsl — NKRenderer v4.0 — Trail Ribbon Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(location=0) out vec2 vUV; layout(location=1) out vec4 vColor;
void main(){gl_Position=uCam.viewProj*vec4(aPos,1.);vUV=aUV;vColor=aColor;}
