// hair.frag.dx12.hlsl — Hair Kajiya-Kay (DX12)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 vp;float time,dt;};
cbuffer LightsUBO:register(b2){float4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];};
Texture2D tAlbedo:register(t4); Texture2D tAlphaMask:register(t5); SamplerState sLinear:register(s0);
struct PSIn{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float3 tangent:TEXCOORD2;float3 bitangent:TEXCOORD3;float2 uv:TEXCOORD4;float2 uv2:TEXCOORD5;float4 color:TEXCOORD6;};
float4 main(PSIn i):SV_Target{
    float4 albSamp=tAlbedo.Sample(sLinear,i.uv)*i.color;
    float3 albedo=albSamp.rgb; float alpha=tAlphaMask.Sample(sLinear,i.uv).r*albSamp.a;
    clip(alpha-0.02);
    float3 T=normalize(i.tangent), V=normalize(camPos.xyz-i.worldPos), Lo=(float3)0.;
    [loop]for(int li=0;li<count&&li<32;li++){
        float3 L=(int(positions[li].w)==0)?normalize(-directions[li].xyz):normalize(positions[li].xyz-i.worldPos);
        float3 rad=colors[li].rgb*colors[li].w;
        float sinTL=length(cross(T,L)),diff=sinTL*max(dot(normalize(i.normal),L),0.)*0.5+0.5;
        float shiftT=dot(T,L),sinTV=length(cross(T,V));
        float spec=pow(max(shiftT*dot(T,V)+sinTL*sinTV,0.),32.);
        float spec2=pow(max(-shiftT*dot(T,V)+sinTL*sinTV,0.),16.)*0.5;
        Lo+=rad*(diff*albedo+spec*float3(1.,0.95,0.9)+spec2*albedo*0.3);
    }
    return float4(Lo,alpha);
}
