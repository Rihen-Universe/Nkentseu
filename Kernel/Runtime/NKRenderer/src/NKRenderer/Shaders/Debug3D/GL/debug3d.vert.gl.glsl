// debug3d.vert.gl.glsl — Debug 3D Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNormal;
layout(location=3) in vec2 aUV;  layout(location=5) in vec4 aColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(std140,binding=4) uniform DebugUBO{int mode;float nearPlane,farPlane,_pad;}uDbg;
// mode: 0=normals, 1=UV checker, 2=depth, 3=AO, 4=wireframe
layout(location=0) out vec3 vNormal; layout(location=1) out vec2 vUV;
layout(location=2) out vec4 vColor;  layout(location=3) out float vDepth;
void main(){
    vec4 wp=uObj.model*vec4(aPos,1.);
    vNormal=normalize(mat3(uObj.normalMatrix)*aNormal);
    vUV=aUV; vColor=aColor*uObj.tint;
    gl_Position=uCam.viewProj*wp;
    vDepth=(uCam.view*wp).z;
}
