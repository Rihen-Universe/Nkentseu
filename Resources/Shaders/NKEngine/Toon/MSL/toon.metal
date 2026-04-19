// =============================================================================
// toon.metal — Shader Toon Metal Shading Language 2.x
// =============================================================================
#include <metal_stdlib>
using namespace metal;

struct SceneUBO { float4x4 model,view,proj,lightVP; float4 lightDir,eyePos; float t,dt,zs,zo; };
struct ToonMat  { float4 base,shadow,outline; float outW,shThr,shSmooth,spThr,spSmooth; uint steps; float2 _p; };

struct VIn  { float3 pos [[attribute(0)]]; float3 nrm [[attribute(1)]]; float2 uv [[attribute(3)]]; float4 col [[attribute(4)]]; };
struct VOut { float4 pos [[position]]; float3 nrm; float3 fp; float2 uv; float4 col; };

vertex VOut vertex_toon(VIn v [[stage_in]],
    constant SceneUBO& scene [[buffer(0)]])
{
    VOut o;
    float4 wp=scene.model*float4(v.pos,1);
    o.fp=wp.xyz;
    o.nrm=normalize(float3x3(scene.model[0].xyz,scene.model[1].xyz,scene.model[2].xyz)*v.nrm);
    o.uv=v.uv; o.col=v.col;
    o.pos=scene.proj*scene.view*wp;
    return o;
}

float smooth3(float e0,float e1,float x){float t=saturate((x-e0)/(e1-e0));return t*t*(3-2*t);}

fragment float4 fragment_toon(VOut in [[stage_in]],
    constant SceneUBO& scene [[buffer(0)]],
    constant ToonMat& mat    [[buffer(2)]],
    texture2d<float> tAlbedo [[texture(0)]],
    sampler sLinear          [[sampler(0)]])
{
    float4 albedo=tAlbedo.sample(sLinear,in.uv)*mat.base*in.col;
    float3 N=normalize(in.nrm),L=normalize(-scene.lightDir.xyz),V=normalize(scene.eyePos.xyz-in.fp);
    float shadow=smooth3(mat.shThr-mat.shSmooth,mat.shThr+mat.shSmooth,dot(N,L));
    if(mat.steps>1){float s=(float)mat.steps;shadow=floor(shadow*s+0.5)/s;}
    float spec=smooth3(mat.spThr-mat.spSmooth,mat.spThr+mat.spSmooth,dot(N,normalize(V+L)));
    float3 color=mix(mat.shadow.rgb,albedo.rgb,shadow)+spec;
    return float4(color,albedo.a);
}
