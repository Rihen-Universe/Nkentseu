#include "pch.h"
// =============================================================================
// NkSculptPipelines.cpp  — NKRenderer v5.0  (Tools/PixolSculpt/)
// Pipelines compute via NkComputeContext::GetOrCompileGLSL (cache interne).
// Kernels embarques = reflet de shaders/VK/*.comp.vk.glsl.
// =============================================================================
#include "NKRenderer/Tools/PixolSculpt/NkSculptPipelines.h"
#include "NKRHI/Core/NkComputeContext.h"

namespace nkentseu {
    namespace renderer {

        // ── Kernel de brosse (cf. shaders/VK/sculpt_brush.comp.vk.glsl) ───────────
        static const char* kSculptBrushGLSL = R"GLSL(#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(set=0, binding=0, r32f)    uniform image2D uPixolDepth;
layout(set=0, binding=1, rgba16f) uniform image2D uPixolNormal;
layout(set=0, binding=2, rgba8)   uniform image2D uPixolColor;
layout(push_constant) uniform PushConsts {
    vec2 center; float radius; float strength; vec4 color;
    uint mode; uint falloff; float hardness; float depthBias;
    int tileOffsetX; int tileOffsetY; uint _pad0; uint _pad1;
} pc;
const uint MODE_RAISE=0u, MODE_LOWER=1u, MODE_PAINT=7u;
float falloffWeight(float t, uint kind, float hardness){
    t = clamp(t,0.0,1.0);
    if (kind==2u) return 1.0;
    if (kind==1u) return 1.0-t;
    if (kind==3u) return pow(1.0-t,3.0);
    if (kind==4u) return sqrt(max(0.0,1.0-t*t));
    float e = mix(1.0,4.0,hardness);
    return pow(1.0 - smoothstep(0.0,1.0,t), e);
}
void main(){
    ivec2 p = ivec2(pc.tileOffsetX,pc.tileOffsetY) + ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = imageSize(uPixolDepth);
    if (p.x<0||p.y<0||p.x>=sz.x||p.y>=sz.y) return;
    float dist = distance(vec2(p)+vec2(0.5), pc.center);
    if (dist > pc.radius) return;
    float w = falloffWeight(dist/pc.radius, pc.falloff, pc.hardness) * pc.strength;
    if (pc.mode==MODE_RAISE || pc.mode==MODE_LOWER){
        float z = imageLoad(uPixolDepth,p).r;
        float dir = (pc.mode==MODE_RAISE)?-1.0:1.0;
        z += dir*w + pc.depthBias;
        imageStore(uPixolDepth,p, vec4(z,0.0,0.0,0.0));
    } else if (pc.mode==MODE_PAINT){
        vec4 c = imageLoad(uPixolColor,p);
        imageStore(uPixolColor,p, mix(c,pc.color,w));
    }
}
)GLSL";

        // ── Kernel de resolve (cf. shaders/VK/pixol_resolve.comp.vk.glsl) ─────────
        static const char* kSculptResolveGLSL = R"GLSL(#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(set=0, binding=0, r32f)    uniform image2D uPixolDepth;
layout(set=0, binding=1, rgba16f) uniform image2D uPixolNormal;
layout(set=0, binding=2, rgba8)   uniform image2D uPixolColor;
layout(set=0, binding=3, rgba8)   uniform image2D uGBufAlbedo;
layout(set=0, binding=4, rgba16f) uniform image2D uGBufNormal;
void main(){
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = imageSize(uPixolDepth);
    if (p.x>=sz.x || p.y>=sz.y) return;
    vec4 col = imageLoad(uPixolColor,p);
    vec4 nrm = imageLoad(uPixolNormal,p);
    imageStore(uGBufAlbedo, p, vec4(col.rgb, 0.0));
    imageStore(uGBufNormal, p, vec4(normalize(nrm.xyz + vec3(0.00001)), 0.5));
}
)GLSL";

        bool NkSculptPipelines::Init(NkComputeContext* ctx) noexcept {
            mCtx = ctx;
            return mCtx != nullptr;
        }

        void NkSculptPipelines::Shutdown() noexcept { mCtx = nullptr; }

        NkPipelineHandle NkSculptPipelines::Brush() noexcept {
            if (!mCtx) return NkPipelineHandle{};
            return mCtx->GetOrCompileGLSL("nkpixol_brush", kSculptBrushGLSL, "PixolSculpt_Brush");
        }

        NkPipelineHandle NkSculptPipelines::Resolve() noexcept {
            if (!mCtx) return NkPipelineHandle{};
            return mCtx->GetOrCompileGLSL("nkpixol_resolve", kSculptResolveGLSL, "PixolSculpt_Resolve");
        }

    } // namespace renderer
} // namespace nkentseu
