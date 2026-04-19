// =============================================================================
// pbr_metallic.vk.frag — Vulkan GLSL Fragment PBR
// DIFFÉRENCES vs OpenGL : layout(set=N,binding=M) sur tout, pas de sampler2DShadow
// =============================================================================
#version 460
layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec4 vTangent;
layout(location=3) in vec2 vUV0;
layout(location=4) in vec2 vUV1;
layout(location=5) in vec3 vViewDir;
layout(location=6) in vec4 vShadow[4];
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform Frame{mat4 V,P,VP,iVP,pVP;vec4 cp,cd,vs,t,sd,sc;mat4 sm[4];vec4 cs;float ei,er;uint lc,fi;};
layout(set=1,binding=0,std140) uniform Mat{vec4 albedo,em;float metal,rough,ao,emStr,cc,ccR,normStr,uvSX,uvSY,uvOX,uvOY,trans,ior;float p[3];};
layout(set=1,binding=1) uniform sampler2D tAlb;
layout(set=1,binding=2) uniform sampler2D tNrm;
layout(set=1,binding=3) uniform sampler2D tMR;
layout(set=1,binding=4) uniform sampler2D tAO;
layout(set=1,binding=5) uniform sampler2D tEm;
layout(set=1,binding=6) uniform sampler2D tShd;
layout(set=1,binding=7) uniform samplerCube tIrr;
layout(set=1,binding=8) uniform samplerCube tPre;
layout(set=1,binding=9) uniform sampler2D tBRDF;
layout(push_constant) uniform PC{mat4 m;vec4 c;uint mid;uint p2[3];};
const float PI=3.14159265359;
float D(float h,float r){float a=r*r*r*r;float d=h*h*(a-1.)+1.;return a/(PI*d*d);}
float G(float v,float l,float r){float k=(r+1.)*(r+1.)/8.;return(v/(v*(1.-k)+k))*(l/(l*(1.-k)+k));}
vec3 F(float c,vec3 f){return f+(1.-f)*pow(clamp(1.-c,0.,1.),5.);}
vec3 FR(float c,vec3 f,float r){return f+(max(vec3(1.-r),f)-f)*pow(clamp(1.-c,0.,1.),5.);}
float ShdPCF(vec4 s){vec3 p=s.xyz/s.w;vec2 u=p.xy*.5+.5;float sh=0.;vec2 ts=1./vec2(textureSize(tShd,0));for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)sh+=(p.z-.005>texture(tShd,u+vec2(x,y)*ts).r)?1.:0.;return 1.-sh/9.*.85;}
void main(){
    vec2 uv=vUV0*vec2(uvSX,uvSY)+vec2(uvOX,uvOY);
    vec3 V=normalize(vViewDir),N=normalize(vNormal);
    vec3 T=normalize(vTangent.xyz-dot(vTangent.xyz,N)*N),B=cross(N,T)*vTangent.w;
    mat3 TBN=mat3(T,B,N);
    vec3 tn=texture(tNrm,uv).rgb*2.-1.;tn.xy*=normStr;N=normalize(TBN*tn);
    vec4 at=texture(tAlb,uv);vec3 base=at.rgb*albedo.rgb*c.rgb;
    vec2 mr=texture(tMR,uv).rg;float m2=mr.r*metal,r2=max(mr.g*rough,.04),a2=texture(tAO,uv).r;
    vec3 F0=mix(vec3(.04),base,m2);
    float vd=-(V*vec4(vWorldPos,1.)).z;
    float shd=vd<cs.x?ShdPCF(vShadow[0]):vd<cs.y?ShdPCF(vShadow[1]):vd<cs.z?ShdPCF(vShadow[2]):ShdPCF(vShadow[3]);
    vec3 L=normalize(-sd.xyz),H=normalize(V+L);
    float nl=max(dot(N,L),0.),nv=max(dot(N,V),0.),nh=max(dot(N,H),0.),hv=max(dot(H,V),0.);
    vec3 fv=F(hv,F0);vec3 Lo=(((1.-fv)*(1.-m2))*base/PI+D(nh,r2)*G(nv,nl,r2)*fv/max(4.*nv*nl,.001))*nl*sc.xyz*sc.w*shd;
    vec3 eN=vec3(N.x*cos(er)-N.z*sin(er),N.y,N.x*sin(er)+N.z*cos(er));
    vec3 eR=reflect(-V,N);eR=vec3(eR.x*cos(er)-eR.z*sin(er),eR.y,eR.x*sin(er)+eR.z*cos(er));
    vec3 irr=texture(tIrr,eN).rgb;
    vec3 kD2=(1.-FR(nv,F0,r2))*(1.-m2);
    vec3 pre=textureLod(tPre,eR,r2*7.).rgb;
    vec2 brdf=texture(tBRDF,vec2(nv,r2)).rg;
    vec3 amb=(kD2*irr*base+pre*(FR(nv,F0,r2)*brdf.x+brdf.y))*a2*ei;
    vec3 ems=texture(tEm,uv).rgb*em.rgb*emStr;
    fragColor=vec4((Lo+amb+ems)*t.w,at.a*albedo.a);
}
