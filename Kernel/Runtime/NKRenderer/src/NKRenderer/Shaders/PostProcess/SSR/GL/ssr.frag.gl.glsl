// ssr.frag.gl.glsl — NKRenderer v4.0 — SSR (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tHDR;
layout(binding=1) uniform sampler2D tDepth;
layout(binding=2) uniform sampler2D tNormal;
layout(binding=3) uniform sampler2D tORM;
layout(std140,binding=4) uniform SSRUBO{mat4 proj,invProj,view;float maxDistance,resolution,thickness;float _p;}uSSR;
vec3 ReconVS(vec2 uv,float d){vec4 n=vec4(uv*2.-1.,d*2.-1.,1.);vec4 v=uSSR.invProj*n;return v.xyz/v.w;}
void main(){
    float depth=texture(tDepth,vUV).r;
    if(depth>=0.9999){fragColor=texture(tHDR,vUV);return;}
    vec3 vsPos=ReconVS(vUV,depth);
    vec3 wsNormal=texture(tNormal,vUV).xyz*2.-1.;
    vec3 vsNormal=normalize((uSSR.view*vec4(wsNormal,0.)).xyz);
    vec3 vsDir=normalize(vsPos);
    vec3 vsRefl=reflect(vsDir,vsNormal);
    float roughness=texture(tORM,vUV).g;
    if(roughness>0.5){fragColor=texture(tHDR,vUV);return;}
    // Ray march in view space
    vec3 rayStart=vsPos+vsNormal*0.01;
    vec3 rayEnd=rayStart+vsRefl*uSSR.maxDistance;
    float hitFraction=-1.;
    int steps=int(uSSR.resolution*64.);
    for(int i=0;i<steps;i++){
        float t=float(i+1)/float(steps);
        vec3 sample=mix(rayStart,rayEnd,t);
        vec4 proj=uSSR.proj*vec4(sample,1.);
        vec3 ndc=proj.xyz/proj.w;
        if(any(lessThan(abs(ndc.xy),vec2(1.)))){
            vec2 suv=ndc.xy*0.5+0.5;
            float sDepth=texture(tDepth,suv).r;
            vec3 svsPos=ReconVS(suv,sDepth);
            if(svsPos.z<=sample.z&&sample.z-svsPos.z<uSSR.thickness){
                hitFraction=t; break;
            }
        }
    }
    if(hitFraction<0.){fragColor=texture(tHDR,vUV);return;}
    vec3 hitPoint=mix(rayStart,rayEnd,hitFraction);
    vec4 hitProj=uSSR.proj*vec4(hitPoint,1.);
    vec2 hitUV=hitProj.xy/hitProj.w*0.5+0.5;
    float fadeDist=hitFraction;float fadeEdge=1.-max(abs(hitUV.x-.5)*2.,abs(hitUV.y-.5)*2.);
    float fade=fadeEdge*fadeEdge*(1.-fadeDist)*(1.-roughness*2.);
    vec3 reflection=texture(tHDR,hitUV).rgb;
    vec3 base=texture(tHDR,vUV).rgb;
    fragColor=vec4(mix(base,reflection,fade*0.5),1.);
}
