// emissive.frag.gl.glsl — NKRenderer v4.0 — Emissive (OpenGL 4.6)
// Pure emissive with additive bloom-friendly output
#version 460 core
layout(location=4) in vec2 vUV; layout(location=6) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(binding=4) uniform sampler2D tAlbedo;
layout(binding=7) uniform sampler2D tEmissive;
layout(binding=12) uniform sampler2D tEmissiveMask;
void main(){
    vec4 albSamp=texture(tAlbedo,vUV)*vColor; float alpha=albSamp.a; if(alpha<0.01)discard;
    vec3 emissive=texture(tEmissive,vUV).rgb;
    float mask=texture(tEmissiveMask,vUV).r;
    // HDR emissive output (will be tone-mapped in post)
    fragColor=vec4(albSamp.rgb*0.05+emissive*uObj.emissStr*mask,alpha);
}
