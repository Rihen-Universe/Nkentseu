// foliage.vert.gl.glsl — NKRenderer v4.0 — Foliage Vertex (OpenGL 4.6)
// Wind sway via vertex displacement using world position + time
#version 460 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent; layout(location=3) in vec2 aUV;
layout(location=5) in vec4 aColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(std140,binding=9) uniform FoliageUBO{float windStrength,windFrequency,windTurbulence,bendFactor;}uFoliage;
layout(location=0) out vec3 vWorldPos; layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;  layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV; layout(location=5) out vec2 vUV2; layout(location=6) out vec4 vColor;
void main(){
    vec4 worldPos=uObj.model*vec4(aPos,1.);
    // Wind displacement: affects tips more than base (using aColor.a as stiffness weight)
    float stiffness=aColor.a; // 0=tip (moves a lot), 1=root (fixed)
    float phase=worldPos.x*0.3+worldPos.z*0.2+uCam.time*uFoliage.windFrequency;
    float turbPhase=worldPos.x*1.1+worldPos.z*0.7+uCam.time*uFoliage.windFrequency*2.1;
    float wind=sin(phase)*uFoliage.windStrength+sin(turbPhase)*uFoliage.windStrength*uFoliage.windTurbulence;
    worldPos.x+=wind*(1.-stiffness)*uFoliage.bendFactor;
    worldPos.z+=wind*0.3*(1.-stiffness)*uFoliage.bendFactor;
    vWorldPos=worldPos.xyz;
    mat3 nm=mat3(uObj.normalMatrix);
    vNormal=normalize(nm*aNormal); vTangent=normalize(nm*aTangent); vBitangent=cross(vNormal,vTangent);
    vUV=aUV; vUV2=vec2(0.); vColor=aColor*uObj.tint;
    gl_Position=uCam.viewProj*worldPos;
}
