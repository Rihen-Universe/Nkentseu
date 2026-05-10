// decal.frag.vk.glsl — NKRenderer v4.0 — Decal Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vDecalPos;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(set=0,binding=1,std140) uniform DecalUBO{mat4 decalMatrix;float opacity;float normalBlend;float _p[2];}uDecal;
layout(set=1,binding=2) uniform sampler2D tAlbedo;
layout(set=1,binding=3) uniform sampler2D tNormal;
layout(set=1,binding=4) uniform sampler2D tSceneDepth;  // to reconstruct world position
void main(){
    // Clip outside decal bounds ([-1,1] cube)
    if(any(lessThan(vDecalPos,vec3(0.01)))||any(greaterThan(vDecalPos,vec3(0.99)))){discard;}
    vec2 uv=vDecalPos.xz; // project along Y axis
    vec4 albSamp=texture(tAlbedo,uv);
    if(albSamp.a<0.01)discard;
    vec3 normal=texture(tNormal,uv).xyz*2.-1.;
    fragColor=vec4(albSamp.rgb,albSamp.a*uDecal.opacity);
}
