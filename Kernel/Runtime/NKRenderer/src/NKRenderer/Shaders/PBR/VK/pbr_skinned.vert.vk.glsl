// pbr_skinned.vert.vk.glsl — Skinned PBR Vertex (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;
layout(location=6) in uvec4 aBoneIdx;
layout(location=7) in vec4  aBoneWeight;

layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(set=0,binding=5,std430) readonly buffer BonesSSBO{mat4 bones[];};

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;

void main(){
    mat4 sk=aBoneWeight.x*bones[aBoneIdx.x]+aBoneWeight.y*bones[aBoneIdx.y]
           +aBoneWeight.z*bones[aBoneIdx.z]+aBoneWeight.w*bones[aBoneIdx.w];
    vec4 wp=uObj.model*(sk*vec4(aPos,1.0)); vWorldPos=wp.xyz;
    mat3 nm=mat3(uObj.normalMatrix)*mat3(sk);
    vNormal=normalize(nm*aNormal); vTangent=normalize(nm*aTangent); vBitangent=cross(vNormal,vTangent);
    vUV=aUV; vUV2=aUV2; vColor=aColor*uObj.tint;
    gl_Position=uCam.viewProj*wp; gl_Position.y=-gl_Position.y;
}
