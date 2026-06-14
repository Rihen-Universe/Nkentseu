// dof.frag.dx12.hlsl — DOF Bokeh (DX12)
Texture2D tHDR:register(t0);Texture2D tDepth:register(t1);SamplerState sL:register(s0);
cbuffer DOFUBO:register(b3){float focusDistance,aperture,focalLength,maxBlur,nearP,farP;float _p[2];};
float LinD(float d){return(2.*nearP*farP)/(farP+nearP-d*(farP-nearP));}
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
static const float2 disk[16]={float2(0.,.5),float2(.707,.707),float2(.5,0.),float2(.707,-.707),float2(0.,-.5),float2(-.707,-.707),float2(-.5,0.),float2(-.707,.707),float2(0.,1.),float2(1.,0.),float2(0.,-1.),float2(-1.,0.),float2(.5,.866),float2(.866,.5),float2(.866,-.5),float2(.5,-.866)};
float4 main(PSIn i):SV_Target{
    float depth=LinD(tDepth.Sample(sL,i.uv).r);
    float coc=clamp((depth-focusDistance)*aperture/max(depth,.001),-maxBlur,maxBlur);
    float br=abs(coc);if(br<.5)return tHDR.Sample(sL,i.uv);
    uint tw,th;tHDR.GetDimensions(tw,th);float2 ts=1./float2(tw,th);
    float3 acc=(float3)0.;float w=0.;
    [unroll]for(int li=0;li<16;li++){float2 off=disk[li]*br*ts;float sd=LinD(tDepth.Sample(sL,i.uv+off).r);float sc=clamp((sd-focusDistance)*aperture/max(sd,.001),-maxBlur,maxBlur);float ww=(abs(sc)>=length(disk[li])*br)?1.:0.;acc+=tHDR.Sample(sL,i.uv+off).rgb*ww;w+=ww;}
    return float4(acc/max(w,1.),1.);
}
