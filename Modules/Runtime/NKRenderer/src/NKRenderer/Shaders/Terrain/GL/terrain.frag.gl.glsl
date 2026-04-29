// terrain.frag.gl.glsl — NKRenderer v4.0 — Terrain Fragment splatmap (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec2 vWorldXZ;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(binding=4) uniform sampler2D tSplatmap;    // RGBA: 4 layer weights
layout(binding=5) uniform sampler2D tAlbedo0;     // Grass
layout(binding=6) uniform sampler2D tAlbedo1;     // Dirt
layout(binding=7) uniform sampler2D tAlbedo2;     // Rock
layout(binding=8) uniform sampler2D tAlbedo3;     // Snow
layout(binding=9) uniform sampler2D tNormal0;
layout(binding=10) uniform sampler2D tNormal1;
layout(binding=11) uniform sampler2D tNormal2;
layout(binding=12) uniform sampler2D tNormal3;
void main(){
    vec4 splat=texture(tSplatmap,vWorldXZ);
    // Normalize splatmap weights
    float totalW=splat.r+splat.g+splat.b+splat.a;
    if(totalW<0.001) splat=vec4(1.,0.,0.,0.); else splat/=totalW;
    // Blend 4 terrain layers
    vec3 albedo=pow(texture(tAlbedo0,vUV).rgb,vec3(2.2))*splat.r
               +pow(texture(tAlbedo1,vUV).rgb,vec3(2.2))*splat.g
               +pow(texture(tAlbedo2,vUV).rgb,vec3(2.2))*splat.b
               +pow(texture(tAlbedo3,vUV).rgb,vec3(2.2))*splat.a;
    // Blend normals
    vec3 n0=texture(tNormal0,vUV).xyz*2.-1.; vec3 n1=texture(tNormal1,vUV).xyz*2.-1.;
    vec3 n2=texture(tNormal2,vUV).xyz*2.-1.; vec3 n3=texture(tNormal3,vUV).xyz*2.-1.;
    vec3 nTs=normalize(n0*splat.r+n1*splat.g+n2*splat.b+n3*splat.a);
    // Simple TBN from world normal
    vec3 N=normalize(vNormal); vec3 T=normalize(cross(vec3(0,0,1),N)); vec3 B=cross(N,T);
    N=normalize(mat3(T,B,N)*nTs);
    // Diffuse lighting
    vec3 V=normalize(uCam.camPos.xyz-vWorldPos), Lo=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=max(dot(N,L),0.); Lo+=albedo/3.14159*NdL*uLights.colors[i].rgb*uLights.colors[i].w;
    }
    vec3 amb=albedo*0.05;
    fragColor=vec4(amb+Lo,1.);
}
