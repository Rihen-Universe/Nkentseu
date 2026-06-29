// skin.frag.vk.glsl — Skin SSS Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vWorldPos;layout(location=1) in vec3 vNormal;layout(location=2) in vec3 vTangent;layout(location=3) in vec3 vBitangent;layout(location=4) in vec2 vUV;layout(location=5) in vec2 vUV2;layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;float iblStrength;}uCam;
layout(set=1,binding=1,std140) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,clearcoatRoughness,subsurface;vec4 subsurfaceColor;}uObj;
layout(set=0,binding=2,std140) uniform LightsUBO{vec4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];}uLights;
layout(set=2,binding=3) uniform sampler2D tAlbedo;layout(set=2,binding=4) uniform sampler2D tNormal;layout(set=2,binding=5) uniform sampler2D tORM;layout(set=2,binding=6) uniform sampler2D tEmissive;layout(set=0,binding=8) uniform samplerCube tEnvIrradiance;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor;
    // Peau = OPAQUE : pas d'alpha-test. (Un discard sur tAlbedo.a rejetait TOUT
    // le mesh sur DX quand le set materiau ne se liait pas -> alpha=0 -> invisible.)
    // Texture baseColor uploadee en sRGB -> le sampler GPU la delinearise DEJA.
    // L'ancien pow(2.2) appliquait une 2e conversion gamma (double-gamma) qui
    // sur-saturait les couleurs (ex. CesiumMan blanc-verdatre -> vert dominant).
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
        // Specular dielectrique FAIBLE, module par la rugosite. L'ancien
        // pow(...,32)*0.3 FIXE faisait briller TOUTE surface comme un miroir,
        // meme les materiaux mats (metallic=0, rugueux). Maintenant : surface
        // rugueuse -> spec quasi nul ; lisse -> highlight serre mais discret.
        vec3 H=normalize(V+L);
        float sm   = 1.0 - clamp(rog, 0.0, 1.0);      // smoothness
        float shin = mix(8.0, 128.0, sm);
        float spec = pow(max(dot(N,H),0.0), shin) * NdL;
        Lo += spec * 0.04 * (sm*sm) * rad;            // F0 ~0.04 dielectrique
    }
    // Ambient + IBL approximation
    vec3 amb=texture(tEnvIrradiance,N).rgb*albedo*ao*0.3;
    // Emissive (blush map in B channel of emissive)
    vec3 emissive=texture(tEmissive,vUV).rgb*uObj.emissiveStrength;
    vec3 col = amb+Lo+sssOut+emissive;
    // GARDE anti-NaN/Inf : un sample/normalize degenere (ex. normalize(vec3(0))
    // = NaN sur certains drivers Vulkan, ou lights UBO corrompu) produisait des
    // NaN qui se propageaient via le post-process (bloom/tonemap) en un GROS quad
    // noir plein ecran intermittent. On sanitise + clamp la sortie HDR.
    if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
    fragColor = vec4(clamp(col, vec3(0.0), vec3(64.0)), alpha);
}