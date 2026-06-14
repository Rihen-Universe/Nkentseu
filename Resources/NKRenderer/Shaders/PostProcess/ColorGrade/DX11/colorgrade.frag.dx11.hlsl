// colorgrade.frag.dx11.hlsl — Color Grading (DX11)
Texture2D tLDR:register(t0);Texture3D tLUT:register(t1);SamplerState sL:register(s0);
cbuffer ColorGradeUBO:register(b2){float contrast,saturation,brightness,hueShift,shadows,midtones,highlights,lutStrength;float4 colorFilter,splitShadow,splitHighlight;};
struct PSIn{float4 pos:SV_Position;float2 uv:TEXCOORD0;};
float4 main(PSIn i):SV_Target{
    float3 c=tLDR.Sample(sL,i.uv).rgb*colorFilter.rgb;
    c=lerp(0.5,c,contrast)*brightness;
    float lum=dot(c,float3(0.2126,0.7152,0.0722));c=lerp(lum,c,saturation);
    float3 sh=lerp(c,splitShadow.rgb*splitShadow.a,1.-lum);
    float3 hl=lerp(c,splitHighlight.rgb*splitHighlight.a,lum);
    c=lerp(c,sh+hl-c,0.5);
    if(lutStrength>0.){float3 lu=clamp(c,0.,1.)*(31./32.)+.5/32.;c=lerp(c,tLUT.Sample(sL,lu).rgb,lutStrength);}
    return float4(saturate(c),1.);
}
