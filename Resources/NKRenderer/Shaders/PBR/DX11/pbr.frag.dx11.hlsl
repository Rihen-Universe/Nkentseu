// ============================================================
// pbr.frag.dx11.hlsl — NKRenderer v4.0 — PBR Pixel (DX11 SM5)
// ============================================================
cbuffer CameraUBO : register(b0) {
    column_major float4x4 view,proj,viewProj,invViewProj;
    float4 camPos,camDir; float2 viewport; float time,dt;
};
cbuffer ObjectUBO : register(b1) {
    column_major float4x4 model,normalMatrix; float4 tint;
    float metallic,roughness,aoStrength,emissiveStrength,normalStrength,clearcoat,ccRough,sss;
    float4 sssColor;
};
cbuffer LightsUBO : register(b2) {
    float4 positions[32],colors[32],directions[32],angles[32];
    int count; int _pad[3];
};
// NkVSM : atlas d'ombre multi-tuiles, UNE OMBRE PAR LUMIERE.
// Ce cbuffer doit matcher ShadowSlotsUBOBlock (NkVirtualShadowMaps.cpp) --
// c'est bien lui que le runtime envoie en b3 pour TOUS les backends. Le
// shader DX11 y lisait encore l'ancien ShadowUBO cascade-only : il
// interpretait donc des octets qui ne lui correspondaient plus, et
// appliquait a TOUTES les lumieres l'unique ombre de la cascade 0 du
// soleil. D'ou des ombres au mauvais endroit sous les point/spot, et
// aucune occlusion reelle par la lumiere concernee.
struct NkShadowSlot {
    column_major float4x4 shadowMatrix;
    float4 tileUV;        // .xy = UV min, .zw = UV max dans l'atlas
    float4 lightPosOrDir; // .xyz pos/dir, .w portee ou split de cascade
    float4 packedIds;     // .x=lightIdx .y=slotType .z=subIdx
};
cbuffer ShadowSlotsUBO : register(b3) {
    NkShadowSlot slots[256];
    float4 firstSlotPerLight[8]; // 32 lumieres, 4 par float4
    float4 slotCountPerLight[8];
    float4 globalCfg;  // .x=nbSlots .y=mode doux .z=remap Z .w=flip V (DX)
    float4 biasParams; // .x=biais de pente .y=biais normal .z=douceur
};

Texture2D    tAlbedo        : register(t4);
Texture2D    tNormal        : register(t5);
Texture2D    tORM           : register(t6);
Texture2D    tEmissive      : register(t7);
TextureCube  tEnvIrradiance : register(t8);
TextureCube  tEnvPrefilter  : register(t9);
Texture2D    tBRDFLUT       : register(t10);
Texture2D<float> tShadowMap : register(t11);

SamplerState       sLinear  : register(s0);
SamplerState       sLinearC : register(s1);  // cubemap
SamplerComparisonState sShadow : register(s2);

static const float PI = 3.14159265358979;

float D_GGX(float3 N,float3 H,float r){float a=r*r,a2=a*a,NdH=max(dot(N,H),0.);float d=NdH*NdH*(a2-1.)+1.;return a2/(PI*d*d+1e-4);}
float G_S(float x,float k){return x/(x*(1.-k)+k);}
float G_Smith(float3 N,float3 V,float3 L,float r){float k=(r+1.)*(r+1.)/8.;return G_S(max(dot(N,V),0.),k)*G_S(max(dot(N,L),0.),k);}
float3 F_Sch(float c,float3 F0){return F0+(1.-F0)*pow(max(1.-c,0.),5.);}
float3 F_SchR(float c,float3 F0,float r){return F0+(max((float3)(1.-r),F0)-F0)*pow(max(1.-c,0.),5.);}

// ── ECHANTILLONNAGE DE L'ATLAS D'OMBRE (transpose de NkShadowAtlas.glsli) ──
// Le biais de PENTE grandit aux angles rasants, la ou l'acne apparait.
float NkSlopeBias(float3 N, float3 L) {
    float NdL = max(dot(N, L), 0.0);
    return biasParams.x * max(1.0, 4.0 * (1.0 - NdL));
}
// Le biais NORMAL decale le point le long de sa normale, en unites monde :
// c'est lui qui empeche une surface de porter sa propre ombre.
float3 NkNormalBiasPos(float3 wp, float3 N, float3 L) {
    float NdL = max(dot(N, L), 0.0);
    return wp + N * biasParams.y * ((1.0 - NdL) * 0.5 + 0.5);
}
float NkSampleTile(int slotIdx, float3 wp, float bias) {
    if (slotIdx < 0 || slotIdx >= 256) return 1.0;
    float4 coord = mul(slots[slotIdx].shadowMatrix, float4(wp, 1.0));
    if (coord.w <= 0.0) return 1.0;
    float3 p = coord.xyz / coord.w;
    p.x = p.x * 0.5 + 0.5;
    p.y = p.y * 0.5 + 0.5;
    p.y = lerp(p.y, 1.0 - p.y, globalCfg.w); // convention V de l'atlas sur DX
    p.z = lerp(p.z, p.z * 0.5 + 0.5, globalCfg.z) - bias;
    if (p.x < 0.0 || p.x > 1.0) return 1.0;
    if (p.y < 0.0 || p.y > 1.0) return 1.0;
    if (p.z < 0.0 || p.z > 1.0) return 1.0;
    float4 tuv = slots[slotIdx].tileUV;
    float2 atlasUV = lerp(tuv.xy, tuv.zw, p.xy);
    uint sw, sh; tShadowMap.GetDimensions(sw, sh);
    float2 ts = 1.0 / float2(sw, sh);
    // La DOUCEUR elargit le noyau : c'est le reglage expose au panneau Rendu.
    float2 step = ts * max(1.0, biasParams.z * float(sw) * 0.25);
    float s = 0.0;
    [unroll] for (int x = -1; x <= 1; x++)
    [unroll] for (int y = -1; y <= 1; y++) {
        // Rester DANS la tuile : deborder irait lire l'ombre d'une autre
        // lumiere rangee juste a cote dans l'atlas.
        float2 uv = clamp(atlasUV + float2(x, y) * step, tuv.xy + ts, tuv.zw - ts);
        s += tShadowMap.SampleCmpLevelZero(sShadow, uv, p.z);
    }
    return s / 9.0;
}
// Cascade choisie par la profondeur vue, comme cote Vulkan.
float NkDirShadow(int li, float3 wp, float3 N, float3 L) {
    int first = int(firstSlotPerLight[li >> 2][li & 3]);
    int cnt   = int(slotCountPerLight[li >> 2][li & 3]);
    if (first < 0 || cnt <= 0) return 1.0;
    float absDepth = -mul(view, float4(wp, 1.0)).z;
    float bias = NkSlopeBias(N, L);
    float3 bp = NkNormalBiasPos(wp, N, L);
    [loop] for (int c = 0; c < cnt && c < 4; c++) {
        if (absDepth < slots[first + c].lightPosOrDir.w)
            return NkSampleTile(first + c, bp, bias);
    }
    return 1.0;
}
// Face du cube choisie par l'axe dominant (0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z).
float NkPointShadow(int li, float3 wp, float3 N, float3 L) {
    int first = int(firstSlotPerLight[li >> 2][li & 3]);
    if (first < 0) return 1.0;
    float3 d = wp - slots[first].lightPosOrDir.xyz;
    float3 a = abs(d);
    int face;
    if (a.x > a.y && a.x > a.z)      face = d.x > 0.0 ? 0 : 1;
    else if (a.y > a.z)              face = d.y > 0.0 ? 2 : 3;
    else                             face = d.z > 0.0 ? 4 : 5;
    return NkSampleTile(first + face, NkNormalBiasPos(wp, N, L), NkSlopeBias(N, L));
}
float NkSpotShadow(int li, float3 wp, float3 N, float3 L) {
    int first = int(firstSlotPerLight[li >> 2][li & 3]);
    if (first < 0) return 1.0;
    return NkSampleTile(first, NkNormalBiasPos(wp, N, L), NkSlopeBias(N, L));
}
float NkLightShadow(int li, int type, float3 wp, float3 N, float3 L) {
    if (type == 0) return NkDirShadow(li, wp, N, L);
    if (type == 1) return NkPointShadow(li, wp, N, L);
    if (type == 2) return NkSpotShadow(li, wp, N, L);
    return 1.0; // area : pas d'ombre en V0
}

struct PSIn {
    float4 pos         : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float3 tangent     : TEXCOORD2;
    float3 bitangent   : TEXCOORD3;
    float2 uv          : TEXCOORD4;
    float2 uv2         : TEXCOORD5;
    float4 color       : TEXCOORD6;
    float4 shadowCoord0: TEXCOORD7;
    float4 shadowCoord1: TEXCOORD8;
    float4 shadowCoord2: TEXCOORD9;
    float4 shadowCoord3: TEXCOORD10;
};

float4 main(PSIn i) : SV_Target {
    float4 albSamp = tAlbedo.Sample(sLinear,i.uv)*i.color;
    float3 albedo  = pow(albSamp.rgb,(float3)2.2);
    float alpha    = albSamp.a;
    clip(alpha-0.01);

    float3 orm = tORM.Sample(sLinear,i.uv).rgb;
    float ao=orm.r*aoStrength, rog=orm.g*roughness, met=orm.b*metallic;

    float3 nTs  = tNormal.Sample(sLinear,i.uv).xyz*2.-1.; nTs.xy*=normalStrength;
    float3x3 TBN = float3x3(normalize(i.tangent),normalize(i.bitangent),normalize(i.normal));
    float3 N = normalize(mul(nTs,TBN)); // TBN^T * nTs
    float3 V = normalize(camPos.xyz-i.worldPos);
    float3 F0= lerp((float3)0.04,albedo,met);

    float3 Lo=(float3)0.;
    [loop] for(int li=0;li<count&&li<32;li++){
        int lt=int(positions[li].w); float3 L; float att=1.;
        if(lt==0){L=normalize(-directions[li].xyz);}
        else{float3 d=positions[li].xyz-i.worldPos;float dist=length(d);L=normalize(d);att=max(1.-dist/max(directions[li].w,.001),0.);att*=att;
             if(lt==2){float th=dot(L,normalize(-directions[li].xyz));att*=saturate((th-angles[li].y)/(angles[li].x-angles[li].y+1e-4));}}
        // CHAQUE lumiere consulte SA propre ombre dans l'atlas.
        float csf=(angles[li].z>.5)?NkLightShadow(li,lt,i.worldPos,N,L):1.;
        float3 H=normalize(V+L); float NdL=max(dot(N,L),0.); float3 rad=colors[li].rgb*colors[li].w*att;
        float NDF=D_GGX(N,H,rog); float G=G_Smith(N,V,L,rog); float3 F=F_Sch(max(dot(H,V),0.),F0);
        float3 spec=NDF*G*F/(4.*max(dot(N,V),0.)*NdL+1e-4);
        Lo+=csf*(((1.-F)*(1.-met))*albedo/PI+spec)*rad*NdL;
    }

    float3 Fi=F_SchR(max(dot(N,V),0.),F0,rog), kDi=(1.-Fi)*(1.-met);
    float3 irr=tEnvIrradiance.Sample(sLinearC,N).rgb, R=reflect(-V,N);
    float3 pref=tEnvPrefilter.SampleLevel(sLinearC,R,rog*4.).rgb;
    float2 brdf=tBRDFLUT.Sample(sLinear,float2(max(dot(N,V),0.),rog)).rg;
    float3 amb=(kDi*irr*albedo+pref*(Fi*brdf.x+brdf.y))*ao;
    float3 emissive=tEmissive.Sample(sLinear,i.uv).rgb*emissiveStrength;
    return float4(amb+Lo+emissive, alpha);
}
