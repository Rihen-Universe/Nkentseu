// glass.frag.gl.glsl — NKRenderer v4.0 — Glass (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 vWorldPos;layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent; layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV; layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 sssColor;}uObj;
layout(binding=4) uniform sampler2D  tAlbedo;
layout(binding=5) uniform sampler2D  tNormal;
layout(binding=8) uniform samplerCube tEnvPrefilter;
layout(binding=10) uniform sampler2D tSceneColor;  // screen-space background
layout(binding=11) uniform sampler2D tSceneDepth;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    vec3 albedo=albSamp.rgb; float alpha=albSamp.a;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.;
    mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs), V=normalize(uCam.camPos.xyz-vWorldPos);
    float ior=1.45; // borosilicate glass
    // Fresnel (Schlick)
    float cosV=max(dot(N,V),0.0);
    float F0=(1.-ior)/(1.+ior); F0*=F0;
    float F=F0+(1.-F0)*pow(1.-cosV,5.0);
    // Reflection from env cubemap
    vec3 R=reflect(-V,N);
    vec3 reflected=textureLod(tEnvPrefilter,R,uObj.roughness*4.0).rgb;
    // Refraction from screen-space (simple distortion)
    vec2 screenUV=gl_FragCoord.xy/uCam.viewport;
    vec2 distort=nTs.xy*0.02*uObj.normalStrength;
    vec3 refracted=texture(tSceneColor,screenUV+distort).rgb;
    // Mix refraction and reflection based on Fresnel + tint
    vec3 color=mix(refracted*albedo,reflected,F);
    fragColor=vec4(color,alpha);
}
