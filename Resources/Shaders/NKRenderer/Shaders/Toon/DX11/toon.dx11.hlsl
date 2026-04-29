// =============================================================================
// toon.dx11.hlsl — Toon/Cel-shading DX11
// =============================================================================
cbuffer FrameData : register(b1) {
    float4x4 View, Projection, ViewProjection, InvVP, PrevVP;
    float4 CamPos, CamDir, Viewport, Time;
    float4 SunDir, SunColor;
    float4x4 ShadowMatrix[4]; float4 CascadeSplits;
    float EnvIntensity, EnvRotation; uint LightCount, FrameIdx;
};
cbuffer ObjectData : register(b0) { float4x4 modelMatrix; float4 customColor; uint matId; uint3 p; };
cbuffer ToonData   : register(b2) {
    float4 colorLight, colorShadow, rimColor, outlineColor;
    float steps, stepSharpness, rimStrength, rimThreshold;
    float outlineWidth, outlineDepthBias, _p0, _p1;
    int mode;
    int3 _p2;
};

struct VSIn  { float3 pos:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_Position; float3 worldPos:TEXCOORD0; float3 normal:TEXCOORD1; float2 uv:TEXCOORD2; float3 view:TEXCOORD3; };

Texture2D tAlbedo : register(t0);
SamplerState sLinear : register(s0);

VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp=mul(float4(v.pos,1.0),modelMatrix);
    o.worldPos=wp.xyz;
    o.uv=v.uv;
    o.view=normalize(CamPos.xyz-wp.xyz);
    float3x3 NM=(float3x3)modelMatrix;
    o.normal=normalize(mul(v.normal,NM));
    if(mode==1) {
        // OUTLINE : extrude en clip space
        float4 cp=mul(wp,ViewProjection);
        float2 ndcN=normalize(mul(float4(o.normal,0),ViewProjection).xy);
        ndcN.y*=Viewport.x/Viewport.y;
        cp.xy+=ndcN*outlineWidth*cp.w;
        cp.z-=outlineDepthBias;
        o.pos=cp;
    } else {
        o.pos=mul(wp,ViewProjection);
    }
    return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
    if(mode==1) return outlineColor;
    float4 albedo=tAlbedo.Sample(sLinear,i.uv)*colorLight*customColor;
    float3 N=normalize(i.normal), L=normalize(-SunDir.xyz), V=normalize(i.view);
    float NdotL=dot(N,L);
    // Bandes discrètes
    float lf=floor((NdotL*0.5+0.5)*steps)/steps;
    float fr=frac((NdotL*0.5+0.5)*steps);
    lf+=smoothstep(0.0,1.0-stepSharpness,fr)/steps;
    float3 shade=lerp(colorShadow.rgb,colorLight.rgb,lf);
    // Rim
    float rim=1.0-max(dot(V,N),0.0);
    rim=smoothstep(rimThreshold,rimThreshold+0.1,rim);
    float3 rc=rimColor.rgb*rim*rimStrength*max(sign(NdotL+0.3),0.0);
    // Specular step
    float3 H=normalize(V+L);
    float sp=step(0.5,pow(max(dot(N,H),0.0),64.0))*0.5*max(sign(NdotL),0.0);
    float3 color=albedo.rgb*shade+rc+sp;
    return float4(color,albedo.a);
}

// =============================================================================
// toon.metal — Toon MSL
// =============================================================================
/*
#include <metal_stdlib>
using namespace metal;

struct ToonUBO { float4 colorLight,colorShadow,rimColor,outlineColor;
    float steps,stepSharpness,rimStrength,rimThreshold;
    float outlineWidth,outlineDepthBias; int mode; };
struct FrameUBO { float4x4 ViewProjection,View; float4 SunDirection,SunColor,CameraPosition; float4 Viewport; };
struct ObjectUBO { float4x4 modelMatrix; float4 customColor; };
struct VIn  { float3 pos [[attribute(0)]]; float3 normal [[attribute(1)]]; float2 uv [[attribute(2)]]; };
struct V2F  { float4 pos [[position]]; float3 worldPos; float3 normal; float2 uv; float3 view; };

vertex V2F toonVert(VIn v [[stage_in]],
    constant FrameUBO&  frame [[buffer(8)]],
    constant ObjectUBO& obj   [[buffer(9)]],
    constant ToonUBO&   toon  [[buffer(10)]])
{
    V2F o;
    float4 wp=obj.modelMatrix*float4(v.pos,1.0);
    o.worldPos=wp.xyz;
    o.uv=v.uv;
    o.view=normalize(frame.CameraPosition.xyz-wp.xyz);
    float3x3 NM=float3x3(obj.modelMatrix[0].xyz,obj.modelMatrix[1].xyz,obj.modelMatrix[2].xyz);
    o.normal=normalize(NM*v.normal);
    if(toon.mode==1){
        float4 cp=frame.ViewProjection*wp;
        float2 ndcN=normalize((frame.ViewProjection*float4(o.normal,0)).xy);
        ndcN.y*=frame.Viewport.x/frame.Viewport.y;
        cp.xy+=ndcN*toon.outlineWidth*cp.w;
        cp.z-=toon.outlineDepthBias;
        o.pos=cp;
    } else { o.pos=frame.ViewProjection*wp; }
    return o;
}

fragment float4 toonFrag(V2F i [[stage_in]],
    constant FrameUBO&  frame [[buffer(8)]],
    constant ObjectUBO& obj   [[buffer(9)]],
    constant ToonUBO&   toon  [[buffer(10)]],
    texture2d<float> tAlbedo  [[texture(0)]],
    sampler sLinear           [[sampler(0)]])
{
    if(toon.mode==1) return toon.outlineColor;
    float4 albedo=tAlbedo.sample(sLinear,i.uv)*toon.colorLight*obj.customColor;
    float3 N=normalize(i.normal),L=normalize(-frame.SunDirection.xyz),V=normalize(i.view);
    float NdotL=dot(N,L);
    float lf=floor((NdotL*0.5f+0.5f)*toon.steps)/toon.steps;
    float fr=fract((NdotL*0.5f+0.5f)*toon.steps);
    lf+=smoothstep(0.0f,1.0f-toon.stepSharpness,fr)/toon.steps;
    float3 shade=mix(toon.colorShadow.rgb,toon.colorLight.rgb,lf);
    float rim=1.0f-max(dot(V,N),0.0f);
    rim=smoothstep(toon.rimThreshold,toon.rimThreshold+0.1f,rim);
    float3 rc=toon.rimColor.rgb*rim*toon.rimStrength*max(sign(NdotL+0.3f),0.0f);
    float3 H=normalize(V+L);
    float sp=step(0.5f,pow(max(dot(N,H),0.0f),64.0f))*0.5f*max(sign(NdotL),0.0f);
    return float4(albedo.rgb*shade+rc+sp, albedo.a);
}
*/
