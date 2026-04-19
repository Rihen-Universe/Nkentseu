// =============================================================================
// postprocess.vk.glsl — Post-traitement Vulkan GLSL 4.50
// Contient 4 shaders: SSAO / Bloom / Tonemap / FXAA
// Chaque section est compilée séparément selon le define actif.
// =============================================================================

// --------------------------------------------------
// COMMUN : vertex fullscreen triangle (tous les passes)
// --------------------------------------------------
// #vert (partagé)
#version 450
layout(location=0) out vec2 vUV;
void main(){
    vec2 pos[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vUV=pos[gl_VertexID]*0.5+0.5;
    gl_Position=vec4(pos[gl_VertexID],0,1);
}

// ==================================================
// SSAO
// ==================================================
// #frag_ssao
#version 450
layout(location=0) in vec2 vUV;
layout(set=0,binding=0) uniform sampler2D uGDepth;
layout(set=0,binding=1) uniform sampler2D uGNormal;
layout(set=0,binding=2) uniform sampler2D uNoise;
layout(set=1,binding=0) uniform SSAOUBO {
    mat4 uProj,uProjInv;
    vec4 uKernel[64];
    vec2 uNoiseScale;
    float uRadius,uBias,uIntensity;
    int uKernelSize;
};
layout(location=0) out float oOcclusion;
vec3 ReconstructPos(vec2 uv,float d){
    vec4 c=vec4(uv*2.0-1.0,d*2.0-1.0,1.0);
    vec4 vp=uProjInv*c;
    return vp.xyz/vp.w;
}
void main(){
    float depth=texture(uGDepth,vUV).r;
    if(depth>=0.9999){oOcclusion=1.0;return;}
    vec3 fp=ReconstructPos(vUV,depth);
    vec3 N=normalize(texture(uGNormal,vUV).xyz*2.0-1.0);
    vec3 rv=normalize(texture(uNoise,vUV*uNoiseScale).xyz);
    vec3 T=normalize(rv-N*dot(rv,N));
    vec3 B=cross(N,T);
    mat3 TBN=mat3(T,B,N);
    float occ=0.0;
    for(int i=0;i<uKernelSize;++i){
        vec3 sp=TBN*uKernel[i].xyz;
        sp=fp+sp*uRadius;
        vec4 off=uProj*vec4(sp,1.0);
        off.xyz/=off.w;
        vec2 suv=off.xy*0.5+0.5;
        float sd=texture(uGDepth,suv).r;
        vec3 sfp=ReconstructPos(suv,sd);
        float rc=smoothstep(0.0,1.0,uRadius/abs(fp.z-sfp.z));
        occ+=(sfp.z>=sp.z+uBias?1.0:0.0)*rc;
    }
    oOcclusion=1.0-(occ/float(uKernelSize))*uIntensity;
}

// ==================================================
// TONEMAP
// ==================================================
// #frag_tonemap
#version 450
layout(location=0) in vec2 vUV;
layout(set=0,binding=0) uniform sampler2D uHDR;
layout(set=1,binding=0) uniform TonemapUBO {
    float uExposure,uGamma;
    int uOperator;
    float uContrast,uSaturation,uFilmGrain,uVignette,uVignetteSmooth,uCA,uWB,uTime,_p;
};
layout(location=0) out vec4 oColor;
vec3 ACES(vec3 x){const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);}
float Hash(vec2 p){p=fract(p*vec2(443.897,441.423));p+=dot(p,p.yx+19.19);return fract((p.x+p.y)*p.x);}
void main(){
    vec2 ca=(vUV-0.5)*uCA*0.01;
    vec3 hdr=vec3(texture(uHDR,vUV+ca).r,texture(uHDR,vUV).g,texture(uHDR,vUV-ca).b)*uExposure;
    vec3 c=uOperator==0?ACES(hdr):clamp(hdr/(1.0+hdr),0.0,1.0);
    c=pow(c,vec3(uContrast));
    float l=dot(c,vec3(0.2126,0.7152,0.0722));
    c=mix(vec3(l),c,uSaturation);
    c=pow(max(c,0.0),vec3(1.0/uGamma));
    float v=1.0-smoothstep(uVignette,uVignette+uVignetteSmooth,length(vUV-0.5)*1.6);
    c*=v;
    c+=(Hash(vUV+fract(uTime))*2.0-1.0)*uFilmGrain*0.02;
    oColor=vec4(c,1.0);
}

// ==================================================
// BLOOM composite
// ==================================================
// #frag_bloom
#version 450
layout(location=0) in vec2 vUV;
layout(set=0,binding=0) uniform sampler2D uHDR;
layout(set=0,binding=1) uniform sampler2D uBloom;
layout(set=1,binding=0) uniform BloomUBO {float uThreshold,uKnee,uIntensity,uFilterRadius; int uPass;};
layout(location=0) out vec4 oColor;
float Luma(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}
vec3 Threshold(vec3 c,float t,float k){float l=Luma(c);float rq=clamp(l-t+k,0.0,2.0*k);rq=rq*rq/(4.0*k+1e-5);return c*max(rq,l-t)/max(l,1e-5);}
void main(){
    vec2 ts=1.0/textureSize(uHDR,0);
    if(uPass==0){oColor=vec4(Threshold(texture(uHDR,vUV).rgb,uThreshold,uKnee),1.0);}
    else if(uPass==3){
        oColor=vec4(texture(uHDR,vUV).rgb+texture(uBloom,vUV).rgb*uIntensity,1.0);
    } else {
        vec3 c=vec3(0);
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)
            c+=texture(uHDR,vUV+vec2(x,y)*ts*(uPass==1?0.5:uFilterRadius)).rgb;
        oColor=vec4(c/9.0,1.0);
    }
}
