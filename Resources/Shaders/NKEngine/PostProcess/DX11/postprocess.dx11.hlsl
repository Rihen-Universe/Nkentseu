// =============================================================================
// postprocess.dx11.hlsl — Post-traitement DirectX 11 SM 5.0
// Compilé avec define NK_PASS_TONEMAP / NK_PASS_BLOOM / NK_PASS_SSAO / NK_PASS_FXAA
// =============================================================================
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
VSOut VSMain(uint id:SV_VertexID) {
    VSOut o;
    float2 pos[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    o.uv=pos[id]*0.5+0.5; o.uv.y=1-o.uv.y;
    o.pos=float4(pos[id],0,1);
    return o;
}

// ==================================================
// TONEMAP
// ==================================================
#ifdef NK_PASS_TONEMAP
cbuffer TonemapCB : register(b0) {
    float uExposure,uGamma; int uOperator;
    float uContrast,uSaturation,uFilmGrain,uVignette,uVignetteSmooth,uCA,uWB,uTime,_p;
};
Texture2D tHDR : register(t0);
SamplerState sLinear : register(s0);
float3 ACES(float3 x){const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;return saturate((x*(a*x+b))/(x*(c*x+d)+e));}
float Hash(float2 p){p=frac(p*float2(443.897,441.423));p+=dot(p,p.yx+19.19);return frac((p.x+p.y)*p.x);}
float4 PSMain(VSOut i) : SV_Target {
    float2 ca=(i.uv-0.5)*uCA*0.01;
    float3 hdr=float3(tHDR.Sample(sLinear,i.uv+ca).r,tHDR.Sample(sLinear,i.uv).g,tHDR.Sample(sLinear,i.uv-ca).b)*uExposure;
    float3 c=uOperator==0?ACES(hdr):saturate(hdr/(1+hdr));
    c=pow(abs(c),uContrast);
    float l=dot(c,float3(0.2126,0.7152,0.0722));
    c=lerp(float3(l,l,l),c,uSaturation);
    c=pow(abs(c),1.0/uGamma);
    float v=1-smoothstep(uVignette,uVignette+uVignetteSmooth,length(i.uv-0.5)*1.6);
    c*=v;
    c+=(Hash(i.uv+frac(uTime))*2-1)*uFilmGrain*0.02;
    return float4(c,1);
}
#endif

// ==================================================
// BLOOM
// ==================================================
#ifdef NK_PASS_BLOOM
cbuffer BloomCB : register(b0){ float uThreshold,uKnee,uIntensity,uFilterRadius; int uPass; float3 _p; };
Texture2D tHDR   : register(t0);
Texture2D tBloom : register(t1);
SamplerState sLinear : register(s0);
float Luma(float3 c){return dot(c,float3(0.2126,0.7152,0.0722));}
float3 Threshold(float3 c,float t,float k){float l=Luma(c);float rq=clamp(l-t+k,0,2*k);rq=rq*rq/(4*k+1e-5);return c*max(rq,l-t)/max(l,1e-5);}
float4 PSMain(VSOut i) : SV_Target {
    float2 ts; tHDR.GetDimensions(ts.x,ts.y); ts=1.0/ts;
    if(uPass==0) return float4(Threshold(tHDR.Sample(sLinear,i.uv).rgb,uThreshold,uKnee),1);
    if(uPass==3) return float4(tHDR.Sample(sLinear,i.uv).rgb+tBloom.Sample(sLinear,i.uv).rgb*uIntensity,1);
    float3 c=0;
    for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
        c+=tHDR.Sample(sLinear,i.uv+float2(x,y)*ts*(uPass==1?0.5:uFilterRadius)).rgb;
    return float4(c/9,1);
}
#endif

// ==================================================
// FXAA
// ==================================================
#ifdef NK_PASS_FXAA
cbuffer FXAACB : register(b0){ float2 uInvSize; float uEdgeMin,uEdgeMax,uEdgeSearch,uEdgeGuess,_p0,_p1; };
Texture2D tColor : register(t0);
SamplerState sLinear : register(s0);
float Luma(float3 c){return dot(c,float3(0.299,0.587,0.114));}
float4 PSMain(VSOut i) : SV_Target {
    float3 nw=tColor.Sample(sLinear,i.uv+float2(-1,-1)*uInvSize).rgb;
    float3 ne=tColor.Sample(sLinear,i.uv+float2( 1,-1)*uInvSize).rgb;
    float3 sw=tColor.Sample(sLinear,i.uv+float2(-1, 1)*uInvSize).rgb;
    float3 se=tColor.Sample(sLinear,i.uv+float2( 1, 1)*uInvSize).rgb;
    float3 m =tColor.Sample(sLinear,i.uv).rgb;
    float lnw=Luma(nw),lne=Luma(ne),lsw=Luma(sw),lse=Luma(se),lm=Luma(m);
    float lmin=min(lm,min(min(lnw,lne),min(lsw,lse)));
    float lmax=max(lm,max(max(lnw,lne),max(lsw,lse)));
    if((lmax-lmin)<max(uEdgeMin,lmax*uEdgeMax)) return float4(m,1);
    float2 dir; dir.x=-((lnw+lne)-(lsw+lse)); dir.y=(lnw+lsw)-(lne+lse);
    float rr=max((lnw+lne+lsw+lse)*0.25*uEdgeGuess,1.0/128);
    float rcpMin=1.0/(min(abs(dir.x),abs(dir.y))+rr);
    dir=clamp(dir*rcpMin,-uEdgeSearch,uEdgeSearch)*uInvSize;
    float3 a=0.5*(tColor.Sample(sLinear,i.uv+dir*(1.0/3-0.5)).rgb+tColor.Sample(sLinear,i.uv+dir*(2.0/3-0.5)).rgb);
    float3 b=a*0.5+0.25*(tColor.Sample(sLinear,i.uv+dir*-0.5).rgb+tColor.Sample(sLinear,i.uv+dir*0.5).rgb);
    float lb=Luma(b);
    return float4(lb<lmin||lb>lmax?a:b,1);
}
#endif
