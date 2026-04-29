// ============================================================
// pbr.vert.dx12.hlsl — NKRenderer v4.0 — Car Paint Vertex (DX12 SM6)
// Différences DX12 vs DX11 :
//   • RootConstants / ConstantBuffer<T>
//   • ResourceDescriptorHeap[] pour le bindless
//   • DXC requis (pas fxc)
//   • Wave intrinsics disponibles (WaveActiveSum, etc.)
//   • SM 6.6 : dynamic resources
// ============================================================
// Indices root constants
struct RootConstants { uint cameraIdx; uint objectIdx; uint shadowIdx; uint pad; };
ConstantBuffer<RootConstants> gRC : register(b0, space0);

struct CameraData {
    float4x4 view,proj,viewProj,invViewProj;
    float4 camPos,camDir; float2 viewport; float time,dt;
};
struct ObjectData {
    float4x4 model,normalMatrix; float4 tint;
    float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;
    float4 sssColor;
};
struct ShadowData {
    float4x4 cascadeMats[4]; float4 splits;
    int cascadeCount; float shadowBias,normalBias; int softShadows;
};

// Bindless resource access (DX12 SM6.6)
ConstantBuffer<CameraData> gCameras[] : register(b0, space1);
ConstantBuffer<ObjectData>  gObjects[] : register(b0, space2);
ConstantBuffer<ShadowData>  gShadows[] : register(b0, space3);

struct VSIn {
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float3 tangent : TANGENT;
    float2 uv      : TEXCOORD0;
    float2 uv2     : TEXCOORD1;
    float4 color   : COLOR;
};
struct VSOut {
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float3 tangent   : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
    float2 uv        : TEXCOORD4;
    float2 uv2       : TEXCOORD5;
    float4 color     : TEXCOORD6;
    float4 sc0       : TEXCOORD7;
    float4 sc1       : TEXCOORD8;
    float4 sc2       : TEXCOORD9;
    float4 sc3       : TEXCOORD10;
};

VSOut main(VSIn v) {
    CameraData cam = gCameras[gRC.cameraIdx];
    ObjectData obj = gObjects[gRC.objectIdx];
    ShadowData shd = gShadows[gRC.shadowIdx];

    VSOut o;
    float4 wp = mul(obj.model, float4(v.pos,1.0));
    o.worldPos   = wp.xyz;
    float3x3 nm  = (float3x3)obj.normalMatrix;
    o.normal     = normalize(mul(nm, v.normal));
    o.tangent    = normalize(mul(nm, v.tangent));
    o.bitangent  = cross(o.normal, o.tangent);
    o.uv=v.uv; o.uv2=v.uv2; o.color=v.color*obj.tint;
    o.sc0 = mul(shd.cascadeMats[0], wp);
    o.sc1 = mul(shd.cascadeMats[1], wp);
    o.sc2 = mul(shd.cascadeMats[2], wp);
    o.sc3 = mul(shd.cascadeMats[3], wp);
    o.pos = mul(cam.viewProj, wp);
    return o;
}
