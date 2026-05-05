// toon.frag.dx12.hlsl — Toon Pixel (DX12 SM6)
struct RC{uint camIdx,objIdx,lightIdx,toonIdx,albedoTex,samplerIdx;uint pad[2];};
ConstantBuffer<RC> gRC:register(b0,space0);
struct CamD{float4x4 view,proj,viewProj,invViewProj;float4 camPos,camDir;float2 vp;float time,dt;};
struct LightD{float4 pos[32],col[32],dir[32],ang[32];int count;int pad[3];};
struct ToonD{float4 shadowColor;float shadowThreshold,shadowSmoothness,outlineWidth,rimIntensity;float4 outlineColor,rimColor;float specHardness;float _p[3];};
ConstantBuffer<CamD> gCam[]:register(b0,space1);ConstantBuffer<LightD> gLight[]:register(b0,space3);ConstantBuffer<ToonD> gToon[]:register(b0,space4);
Texture2D gTex[]:register(t0,space0); SamplerState gSamp[]:register(s0,space0);
struct PSIn{float4 pos:SV_Position;float3 worldPos:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:TEXCOORD3;};
float4 main(PSIn i):SV_Target{
    CamD cam=gCam[gRC.camIdx];LightD lt=gLight[gRC.lightIdx];ToonD toon=gToon[gRC.toonIdx];
    float4 albSamp=gTex[gRC.albedoTex].Sample(gSamp[gRC.samplerIdx],i.uv)*i.color;
    float3 albedo=albSamp.rgb;float alpha=albSamp.a;clip(alpha-.01);
    float3 N=normalize(i.normal),V=normalize(cam.camPos.xyz-i.worldPos),total=(float3)0.;
    [loop]for(int li=0;li<lt.count&&li<32;li++){
        int ltype=int(lt.pos[li].w);float3 L=(ltype==0)?normalize(-lt.dir[li].xyz):normalize(lt.pos[li].xyz-i.worldPos);
        float NdL=dot(N,L),t=smoothstep(toon.shadowThreshold-toon.shadowSmoothness,toon.shadowThreshold+toon.shadowSmoothness,NdL);
        float3 H=normalize(V+L);float spec=step(0.5,pow(max(dot(N,H),0.),toon.specHardness));
        total+=lerp(toon.shadowColor.rgb,albedo,t)*lt.col[li].rgb*lt.col[li].w+spec*0.3*lt.col[li].rgb;
    }
    float rim=pow(1.-max(dot(N,V),0.),3.)*toon.rimIntensity;total+=toon.rimColor.rgb*rim;
    return float4(total,alpha);
}
