#include "pch.h"
// =============================================================================
// NkVoxelPipelines.cpp  — NKRenderer v5.0  (Tools/Voxel/)
// Pipelines compute via NkComputeContext::GetOrCompileGLSL (cache interne).
// Les kernels embarques refletent shaders/VK/voxel_*.comp.vk.glsl.
// =============================================================================
#include "NKRenderer/Tools/Voxel/NkVoxelPipelines.h"
#include "NKRHI/Core/NkComputeContext.h"

namespace nkentseu {
    namespace renderer {

        // ── Kernel d'edition (cf. shaders/VK/voxel_edit.comp.vk.glsl) ─────────────
        static const char* kVoxelEditGLSL = R"GLSL(#version 450
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
layout(set = 0, binding = 0, r16f)  uniform image3D uDensity;
layout(set = 0, binding = 1, rgba8) uniform image3D uColor;
layout(push_constant) uniform PushConsts {
    vec3 center; float radius; vec4 color;
    uint mode; uint falloff; float strength; float _pad0;
    int boxOffsetX; int boxOffsetY; int boxOffsetZ; uint _pad1;
} pc;
const uint MODE_ADD=0u, MODE_SUB=1u, MODE_PAINT=2u;
float falloffWeight(float t, uint kind){
    t = clamp(t,0.0,1.0);
    if (kind==2u) return 1.0;
    if (kind==1u) return 1.0-t;
    if (kind==3u) return sqrt(max(0.0,1.0-t*t));
    return 1.0 - smoothstep(0.0,1.0,t);
}
void main(){
    ivec3 p = ivec3(pc.boxOffsetX,pc.boxOffsetY,pc.boxOffsetZ) + ivec3(gl_GlobalInvocationID.xyz);
    ivec3 sz = imageSize(uDensity);
    if (any(lessThan(p,ivec3(0))) || any(greaterThanEqual(p,sz))) return;
    float dist = distance(vec3(p)+vec3(0.5), pc.center);
    if (dist > pc.radius) return;
    float w = falloffWeight(dist/pc.radius, pc.falloff) * pc.strength;
    if (pc.mode==MODE_ADD || pc.mode==MODE_SUB){
        float d = imageLoad(uDensity,p).r;
        float dir = (pc.mode==MODE_ADD)?1.0:-1.0;
        d = clamp(d + dir*w, 0.0, 1.0);
        imageStore(uDensity,p, vec4(d,0.0,0.0,0.0));
        if (pc.mode==MODE_ADD){ vec4 c=imageLoad(uColor,p); imageStore(uColor,p, mix(c,pc.color,w)); }
    } else if (pc.mode==MODE_PAINT){
        vec4 c=imageLoad(uColor,p); imageStore(uColor,p, mix(c,pc.color,w));
    }
}
)GLSL";

        // ── Kernel de raymarch (squelette ; cf. voxel_raymarch.comp.vk.glsl) ──────
        static const char* kVoxelRaymarchGLSL = R"GLSL(#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0, r16f)  uniform image3D uDensity;
layout(set = 0, binding = 1, rgba8) uniform image3D uColor;
layout(set = 0, binding = 2, rgba8)   uniform image2D uGBufAlbedo;
layout(set = 0, binding = 3, rgba16f) uniform image2D uGBufNormal;
void main(){
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = imageSize(uGBufAlbedo);
    if (px.x>=res.x || px.y>=res.y) return;
    // TODO(Voxel): reconstruire le rayon + marcher le volume + ecrire le G-buffer.
}
)GLSL";

        bool NkVoxelPipelines::Init(NkComputeContext* ctx) noexcept {
            mCtx = ctx;
            return mCtx != nullptr;
        }

        void NkVoxelPipelines::Shutdown() noexcept {
            // Les pipelines appartiennent au cache du NkComputeContext (libere par lui).
            mCtx = nullptr;
        }

        NkPipelineHandle NkVoxelPipelines::Edit() noexcept {
            if (!mCtx) return NkPipelineHandle{};
            return mCtx->GetOrCompileGLSL("nkvoxel_edit", kVoxelEditGLSL, "Voxel_Edit");
        }

        NkPipelineHandle NkVoxelPipelines::Raymarch() noexcept {
            if (!mCtx) return NkPipelineHandle{};
            return mCtx->GetOrCompileGLSL("nkvoxel_raymarch", kVoxelRaymarchGLSL, "Voxel_Raymarch");
        }

    } // namespace renderer
} // namespace nkentseu
