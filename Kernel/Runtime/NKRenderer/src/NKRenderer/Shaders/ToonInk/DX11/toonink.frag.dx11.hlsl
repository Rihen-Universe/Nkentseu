// toonink.frag.dx11.hlsl — ToonInk Fragment (DX11)
// See toonink.frag.gl.glsl for full algorithm documentation
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
cbuffer ObjectUBO:register(b1){column_major float4x4 model,normalMatrix;float4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;float4 sssColor;};
cbuffer LightsUBO:register(b2){float4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];};
SamplerState sLinear:register(s0);

Texture2D    tAlbedo:register(t4);
Texture2D    tNormal:register(t5);
Texture2D    tORM:register(t6);
Texture2D    tEmissive:register(t7);
struct PSIn{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float3 tangent:TEXCOORD2;float3 bitangent:TEXCOORD3;float2 uv:TEXCOORD4;float2 uv2:TEXCOORD5;float4 color:TEXCOORD6;};
float4 main(PSIn i):SV_Target{
    float4 albSamp=tAlbedo.Sample(sLinear,i.uv)*i.color;float3 albedo=pow(albSamp.rgb,(float3)2.2);float alpha=albSamp.a;clip(alpha-.01);
    float3 nTs=tNormal.Sample(sLinear,i.uv).xyz*2.-1.;float3x3 TBN=float3x3(normalize(i.tangent),normalize(i.bitangent),normalize(i.normal));float3 N=normalize(mul(nTs,TBN));float3 V=normalize(camPos.xyz-i.worldPos);
    float3 Lo=(float3)0.;
    // Hatching + ink outline
    [loop]for(int li=0;li<count&&li<32;li++){
        float3 L=(int(positions[li].w)==0)?normalize(-directions[li].xyz):normalize(positions[li].xyz-i.worldPos);
        float NdL=max(dot(N,L),0.);float3 rad=colors[li].rgb*colors[li].w;
        Lo+=albedo/3.14159*NdL*rad;
    }
    float3 emissive=tEmissive.Sample(sLinear,i.uv).rgb*emissStr;
    return float4(albedo*0.05+Lo+emissive,alpha);
}
