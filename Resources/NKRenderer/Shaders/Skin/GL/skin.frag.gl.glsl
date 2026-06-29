// skin.frag.gl.glsl — NKRenderer v4.0 — Skin SSS Fragment (OpenGL 4.6)
#version 460 core
layout(location=0) in vec3 vWorldPos; layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;  layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;       layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;float iblStrength;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 subsurfaceColor;}uObj;
layout(std140,binding=2) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(binding=4) uniform sampler2D tAlbedo;
layout(binding=5) uniform sampler2D tNormal;
layout(binding=6) uniform sampler2D tORM;
layout(binding=7) uniform sampler2D tEmissive;
layout(binding=8) uniform samplerCube tEnvIrradiance;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    // Texture sRGB deja delinearisee par le sampler -> PAS de pow(2.2) (double-gamma
    // -> sur-saturation/vert). Opaque -> pas de discard.
    vec3 albedo=albSamp.rgb; float alpha=1.0;
    vec3 orm=texture(tORM,vUV).rgb;
    float ao=orm.r*uObj.aoStrength, rog=orm.g*uObj.roughness, met=orm.b*uObj.metallic;
    vec3 nTs=texture(tNormal,vUV).xyz*2.-1.; nTs.xy*=uObj.normalStrength;
    mat3 TBN=mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N=normalize(TBN*nTs), V=normalize(uCam.camPos.xyz-vWorldPos);
    // Skin: high scattering at grazing angles
    vec3 Lo=vec3(0.), sssOut=vec3(0.);
    for(int i=0;i<uLights.count&&i<32;i++){
        vec3 L=(int(uLights.positions[i].w)==0)?normalize(-uLights.directions[i].xyz):normalize(uLights.positions[i].xyz-vWorldPos);
        float NdL=max(dot(N,L),0.);
        vec3 rad=uLights.colors[i].rgb*uLights.colors[i].w;
        // Diffuse
        Lo+=NdL*albedo*rad;
        // SSS: wrap lighting + subsurface color bleed
        float wrapNdL=max(dot(N,L)+uObj.subsurface,0.)/(1.+uObj.subsurface);
        sssOut+=uObj.subsurfaceColor.rgb*albedo*rad*wrapNdL*uObj.subsurface;
        // Specular dielectrique FAIBLE module par la rugosite (avant: pow(..,32)
        // *0.3 fixe -> tout brillait comme un miroir, meme les mats).
        vec3 H=normalize(V+L);
        float sm   = 1.0 - clamp(rog, 0.0, 1.0);
        float shin = mix(8.0, 128.0, sm);
        float spec = pow(max(dot(N,H),0.0), shin) * NdL;
        Lo += spec * 0.04 * (sm*sm) * rad;
    }
    // Ambient + IBL approximation
    vec3 amb=texture(tEnvIrradiance,N).rgb*albedo*ao*0.3;
    // Emissive (blush map in B channel of emissive)
    vec3 emissive=texture(tEmissive,vUV).rgb*uObj.emissiveStrength;
    vec3 col = amb+Lo+sssOut+emissive;
    if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
    fragColor = vec4(clamp(col, vec3(0.0), vec3(64.0)), alpha);
}