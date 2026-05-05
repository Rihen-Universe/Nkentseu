// ssao.frag.dx12.hlsl — SSAO (DX12)
Texture2D tDepth:register(t0);Texture2D tNoise:register(t1);SamplerState sL:register(s0);
cbuffer SSAOUBO:register(b2){column_major float4x4 proj,invProj;float4 samples[64];float2 noiseScale;float radius,bias;int numSamples;int _p[3];};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float3 ReconProj(float2 uv,float d,float4x4 ip){float4 ndc=float4(uv*2.-1.,-d*2.+1.,1.);float4 vp=mul(ip,ndc);return vp.xyz/vp.w;}
float main(PSIn i):SV_Target{
    float d=tDepth.Sample(sL,i.uv).r;if(d>=0.9999)return 1.;
    float3 fp=ReconProj(i.uv,d,invProj);
    float3 rvec=tNoise.Sample(sL,i.uv*noiseScale).xyz*2.-1.;
    float3 N=normalize(cross(ReconProj(i.uv+float2(1./1920.,0.),d,invProj)-fp,ReconProj(i.uv+float2(0.,1./1080.),d,invProj)-fp));
    float3 t=normalize(rvec-N*dot(rvec,N));float3x3 TBN=float3x3(t,cross(N,t),N);
    float occ=0.;
    [loop]for(int li=0;li<numSamples;li++){float3 sp=mul(TBN,samples[li].xyz);sp=fp+sp*radius;float4 off=mul(proj,float4(sp,1.));off.xyz/=off.w;off.xyz=off.xyz*.5+float3(.5,.5,.5);float sd=tDepth.Sample(sL,off.xy).r;float3 svp=ReconProj(off.xy,sd,invProj);float rc=smoothstep(0.,1.,radius/abs(fp.z-svp.z));occ+=(svp.z>=sp.z+bias?1.:0.)*rc;}
    return 1.-occ/numSamples;
}
