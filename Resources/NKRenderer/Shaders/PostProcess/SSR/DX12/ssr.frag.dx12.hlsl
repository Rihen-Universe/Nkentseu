// ssr.frag.dx12.hlsl — SSR (DX12)
Texture2D tHDR:register(t0);Texture2D tDepth:register(t1);Texture2D tNormal:register(t2);Texture2D tORM:register(t3);SamplerState sL:register(s0);
cbuffer SSRUBO:register(b4){column_major float4x4 proj,invProj,view;float maxDistance,resolution,thickness;float _p;};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float3 RecVS(float2 uv,float d){float4 n=float4(uv*2.-1.,-d*2.+1.,1.);float4 v=mul(invProj,n);return v.xyz/v.w;}
float4 main(PSIn i):SV_Target{
    float d=tDepth.Sample(sL,i.uv).r;if(d>=.9999)return tHDR.Sample(sL,i.uv);
    float3 vp=RecVS(i.uv,d);float3 wn=tNormal.Sample(sL,i.uv).xyz*2.-1.;
    float3 vn=normalize(mul((float3x3)view,wn));float3 vd=normalize(vp);float3 vr=reflect(vd,vn);
    float rog=tORM.Sample(sL,i.uv).g;if(rog>.5)return tHDR.Sample(sL,i.uv);
    float3 rs=vp+vn*.01,re=rs+vr*maxDistance;float hit=-1.;int steps=(int)(resolution*64.);
    [loop]for(int li=0;li<steps;li++){float t=(float)(li+1)/(float)steps;float3 sp=lerp(rs,re,t);float4 pp=mul(proj,float4(sp,1.));float3 ndc=pp.xyz/pp.w;if(all(abs(ndc.xy)<1.)){float2 su=ndc.xy*.5+.5;su.y=1.-su.y;float sd=tDepth.Sample(sL,su).r;float3 sv=RecVS(su,sd);if(sv.z<=sp.z&&sp.z-sv.z<thickness){hit=t;break;}}}
    if(hit<0.)return tHDR.Sample(sL,i.uv);
    float3 hp=lerp(rs,re,hit);float4 hpr=mul(proj,float4(hp,1.));float2 hu=hpr.xy/hpr.w*.5+.5;hu.y=1.-hu.y;
    float fade=(1.-max(abs(hu.x-.5)*2.,abs(hu.y-.5)*2.));fade*=fade*(1.-hit)*(1.-rog*2.);
    return float4(lerp(tHDR.Sample(sL,i.uv).rgb,tHDR.Sample(sL,hu).rgb,fade*.5),1.);
}
