// toon.frag.dx11.hlsl — Toon Pixel (DX11)
cbuffer CameraUBO:register(b0){column_major float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 viewport;float time,dt;};
cbuffer LightsUBO:register(b2){float4 positions[32],colors[32],directions[32],angles[32];int count;int _pad[3];};
cbuffer ToonUBO:register(b4){float4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;float4 outlineColor,rimColor;float specHardness;float _p[3];};
Texture2D tAlbedo:register(t4); SamplerState sLinear:register(s0);
struct PSIn{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:TEXCOORD3;};
float4 main(PSIn i):SV_Target{
    float4 albSamp=tAlbedo.Sample(sLinear,i.uv)*i.color; float3 albedo=albSamp.rgb; float alpha=albSamp.a; clip(alpha-0.01);
    float3 N=normalize(i.normal),V=normalize(camPos.xyz-i.worldPos),total=0.;
    for(int li=0;li<count&&li<32;li++){
        int lt=int(positions[li].w);float3 L=(lt==0)?normalize(-directions[li].xyz):normalize(positions[li].xyz-i.worldPos);
        float NdL=dot(N,L),toon=smoothstep(shadowThreshold-shadowSmoothness,shadowThreshold+shadowSmoothness,NdL);
        float3 H=normalize(V+L);float spec=step(0.5,pow(max(dot(N,H),0.),specHardness));
        total+=lerp(shadowColor.rgb,albedo,toon)*colors[li].rgb*colors[li].w+spec*0.3*colors[li].rgb;
    }
    float rim=pow(1.-max(dot(N,V),0.),3.)*rimIntensity; total+=rimColor.rgb*rim;
    return float4(total,alpha);
}
