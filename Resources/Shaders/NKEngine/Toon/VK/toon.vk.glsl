// =============================================================================
// toon.vk.glsl — Shader Toon Vulkan GLSL 4.50
// =============================================================================
// #vert
#version 450
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;
layout(set=0,binding=0) uniform SceneUBO {
    mat4 uModel,uView,uProj,uLightVP;
    vec4 uLightDir,uEyePos; float uTime,uDt,uNdcZS,uNdcZO;
};
layout(location=0) out vec3 vNormal;
layout(location=1) out vec3 vFragPos;
layout(location=2) out vec2 vUV;
layout(location=3) out vec4 vColor;
void main() {
    vec4 wp = uModel * vec4(aPos,1.0);
    vFragPos = wp.xyz;
    vNormal  = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV = aUV; vColor = aColor;
    gl_Position = uProj * uView * wp;
}
// #frag
#version 450
layout(location=0) in vec3 vNormal;
layout(location=1) in vec3 vFragPos;
layout(location=2) in vec2 vUV;
layout(location=3) in vec4 vColor;
layout(set=0,binding=0) uniform SceneUBO {
    mat4 uModel,uView,uProj,uLightVP;
    vec4 uLightDir,uEyePos; float uTime,uDt,uNdcZS,uNdcZO;
};
layout(set=1,binding=0) uniform sampler2D uAlbedoMap;
layout(set=2,binding=0) uniform ToonMat {
    vec4 uBaseColor,uShadowColor,uOutlineColor;
    float uOutlineWidth,uShadowThreshold,uShadowSmoothness;
    float uSpecThreshold,uSpecSmoothness; uint uShadeSteps; float _p0,_p1;
};
layout(location=0) out vec4 oColor;
void main() {
    vec4 albedo = texture(uAlbedoMap,vUV)*uBaseColor*vColor;
    vec3 N=normalize(vNormal), L=normalize(-uLightDir.xyz), V=normalize(uEyePos.xyz-vFragPos);
    float NdL=dot(N,L);
    float shadow=smoothstep(uShadowThreshold-uShadowSmoothness,uShadowThreshold+uShadowSmoothness,NdL);
    if(uShadeSteps>1u){ float s=float(uShadeSteps); shadow=floor(shadow*s+0.5)/s; }
    float spec=smoothstep(uSpecThreshold-uSpecSmoothness,uSpecThreshold+uSpecSmoothness,dot(N,normalize(V+L)));
    vec3 color=mix(uShadowColor.rgb,albedo.rgb,shadow)+spec;
    oColor=vec4(color,albedo.a);
}
