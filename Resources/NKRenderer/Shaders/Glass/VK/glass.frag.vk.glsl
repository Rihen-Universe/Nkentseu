// glass.frag.vk.glsl — Glass Fragment (Vulkan) — same logic, descriptor sets
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos;layout(location=1) in vec3 vNormal;layout(location=2) in vec3 vTangent;layout(location=3) in vec3 vBitangent;layout(location=4) in vec2 vUV;layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 sssColor;}uObj;
layout(set=1,binding=0) uniform sampler2D tAlbedo;layout(set=1,binding=1) uniform sampler2D tNormal;layout(set=1,binding=5) uniform samplerCube tEnvPrefilter;layout(set=1,binding=6) uniform sampler2D tSceneColor;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; vec3 albedo=albSamp.rgb; float alpha=albSamp.a;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs),V=normalize(uCam.camPos.xyz-vWorldPos);
    float cosV=max(dot(N,V),0.),F0val=0.04,F=F0val+(1.-F0val)*pow(1.-cosV,5.);
    vec3 R=reflect(-V,N),reflected=textureLod(tEnvPrefilter,R,uObj.roughness*4.).rgb;
    vec2 screenUV=vec2(gl_FragCoord.x,uCam.viewport.y-gl_FragCoord.y)/uCam.viewport;
    vec3 refracted=texture(tSceneColor,screenUV+nTs.xy*0.02*uObj.normalStrength).rgb;
    fragColor=vec4(mix(refracted*albedo,reflected,F),alpha);
}
