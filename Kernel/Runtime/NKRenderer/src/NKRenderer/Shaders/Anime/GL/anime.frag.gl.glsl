// anime.frag.gl.glsl — NKRenderer v4.0 — Anime Shading (OpenGL 4.6)
// Hard shadow steps + strong rim + ink outline in fragment
#version 460 core
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vUV; layout(location=3) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(std140,binding=4) uniform ToonUBO{vec4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;vec4 outlineColor,rimColor;float specHardness;float _p[3];}uToon;
layout(binding=4) uniform sampler2D tAlbedo;
layout(binding=5) uniform sampler2D tShadowRamp;  // custom shadow ramp texture
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; vec3 albedo=albSamp.rgb; float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 N=normalize(vNormal),V=normalize(uCam.camPos.xyz-vWorldPos);
    // Outline: detect edges via normal-view angle
    float edge=1.-max(dot(N,V),0.);
    if(edge>1.-uToon.outlineWidth*0.1){fragColor=vec4(uToon.outlineColor.rgb,alpha);return;}
    vec3 total=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=dot(N,L)*0.5+0.5; // half-lambert for anime
        // Ramp texture for stylized shadow
        float rampU=texture(tShadowRamp,vec2(NdL,0.5)).r;
        vec3 lit=mix(uToon.shadowColor.rgb*albedo,albedo,rampU)*uLights.colors[i].rgb*uLights.colors[i].w;
        // Hard specular
        vec3 H=normalize(V+L);
        float spec=step(0.85,pow(max(dot(N,H),0.),uToon.specHardness));
        lit+=spec*vec3(1.)*uLights.colors[i].w*0.4;
        total+=lit;
    }
    float rim=pow(1.-max(dot(N,V),0.),2.5)*uToon.rimIntensity;
    total+=uToon.rimColor.rgb*rim;
    fragColor=vec4(total,alpha);
}
