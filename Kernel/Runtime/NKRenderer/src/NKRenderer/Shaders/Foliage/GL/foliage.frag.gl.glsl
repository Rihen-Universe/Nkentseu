// foliage.frag.gl.glsl — NKRenderer v4.0 — Foliage Fragment (OpenGL 4.6)
// Two-sided lighting + alpha cutout + subsurface transmission
#version 460 core
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;  layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV; layout(location=5) in vec2 vUV2; layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(std140,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(binding=4) uniform sampler2D tAlbedo;
layout(binding=5) uniform sampler2D tNormal;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; vec3 albedo=pow(albSamp.rgb,vec3(2.2)); float alpha=albSamp.a;
    // Alpha cutout for leaves
    if(alpha<0.1) discard;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs); vec3 V=normalize(uCam.camPos.xyz-vWorldPos);
    // Two-sided: flip normal if back face
    if(!gl_FrontFacing) N=-N;
    vec3 Lo=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=max(dot(N,L),0.); vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w;
        // Diffuse
        Lo+=albedo/3.14159*NdL*rad;
        // Subsurface transmission (light through leaf)
        float NdLBack=max(dot(-N,L),0.);
        vec3 sssColor=uObj.sssColor.rgb*albedo*uObj.sss;
        Lo+=sssColor*NdLBack*rad*0.5;
    }
    vec3 amb=albedo*0.05*uObj.aoStr;
    fragColor=vec4(amb+Lo,1.); // alpha=1 after cutout
}
