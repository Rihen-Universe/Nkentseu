// =============================================================================
// unlit.metal — Shader Unlit Metal Shading Language 2.x
// =============================================================================
#include <metal_stdlib>
using namespace metal;

struct SceneUBO { float4x4 model,view,proj,lightVP; float4 lightDir,eyePos; float t,dt,zs,zo; };
struct ObjPC    { float4x4 model; float4 tint; };
struct UnlitMat { float4 base,emit; float clip,emitScale,p0,p1; };

struct VIn  { float3 pos [[attribute(0)]]; float2 uv [[attribute(3)]]; float4 col [[attribute(4)]]; };
struct VOut { float4 pos [[position]]; float2 uv; float4 col; };

vertex VOut vertex_unlit(
    VIn v [[stage_in]],
    constant SceneUBO& scene [[buffer(0)]],
    constant ObjPC&    obj   [[buffer(1)]])
{
    VOut o;
    float4 wp = obj.model * float4(v.pos,1);
    o.pos = scene.proj * scene.view * wp;
    o.uv  = v.uv;
    o.col = v.col * obj.tint;
    return o;
}

fragment float4 fragment_unlit(
    VOut in [[stage_in]],
    constant UnlitMat& mat [[buffer(2)]],
    texture2d<float>   tAlbedo [[texture(0)]],
    sampler sLinear [[sampler(0)]])
{
    float4 c = tAlbedo.sample(sLinear, in.uv) * mat.base * in.col;
    if (c.a < mat.clip) discard_fragment();
    c.rgb += mat.emit.rgb * mat.emitScale;
    return c;
}
