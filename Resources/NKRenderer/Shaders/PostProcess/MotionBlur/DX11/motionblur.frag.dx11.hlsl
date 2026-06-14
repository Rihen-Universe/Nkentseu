// motionblur.frag.dx11.hlsl — Motion Blur (DX11)
Texture2D tHDR:register(t0);Texture2D tVel:register(t1);SamplerState sL:register(s0);
cbuffer MotionBlurUBO:register(b2){float shutterAngle;int numSamples;float _p[2];};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    float2 vel=tVel.Sample(sL,i.uv).rg*shutterAngle;if(length(vel)<.0001)return tHDR.Sample(sL,i.uv);
    float3 acc=(float3)0.;[loop]for(int li=0;li<numSamples;li++){float t=(float)li/(float)max(numSamples-1,1);acc+=tHDR.Sample(sL,i.uv+vel*(t-.5)).rgb;}
    return float4(acc/numSamples,1.);
}
