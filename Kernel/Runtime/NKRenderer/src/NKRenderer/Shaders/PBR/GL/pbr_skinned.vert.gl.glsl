// pbr_skinned.vert.gl.glsl — Skinned PBR Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;
layout(location=6) in uvec4 aBoneIdx;
layout(location=7) in vec4  aBoneWeight;

layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(std430,binding=5) readonly buffer BonesSSBO{mat4 bones[];};

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;

void main(){
    mat4 skinMat = aBoneWeight.x*bones[aBoneIdx.x]
                  +aBoneWeight.y*bones[aBoneIdx.y]
                  +aBoneWeight.z*bones[aBoneIdx.z]
                  +aBoneWeight.w*bones[aBoneIdx.w];
    vec4 skinnedPos = skinMat*vec4(aPos,1.0);
    vec4 worldPos   = uObj.model*skinnedPos;
    vWorldPos=worldPos.xyz;
    mat3 nm=mat3(uObj.normalMatrix)*mat3(skinMat);
    vNormal=normalize(nm*aNormal);
    vTangent=normalize(nm*aTangent);
    vBitangent=cross(vNormal,vTangent);
    vUV=aUV; vUV2=aUV2; vColor=aColor*uObj.tint;
    gl_Position=uCam.viewProj*worldPos;
}
