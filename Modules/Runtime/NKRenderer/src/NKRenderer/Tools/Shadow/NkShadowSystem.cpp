// =============================================================================
// NkShadowSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkShadowSystem.h"
#include "../../Mesh/NkMeshSystem.h"
#include "../../Materials/NkMaterialSystem.h"
#include <cmath>

namespace nkentseu {
namespace renderer {

    bool NkShadowSystem::Init(NkIDevice* d, NkMeshSystem* m,
                               NkMaterialSystem* mat, const NkShadowSystemConfig& cfg) {
        mDevice=d; mMesh=m; mMat=mat; mCfg=cfg;
        // Atlas = resolution * numCascades wide
        uint32 atlasW = cfg.resolution * cfg.numCascades;
        NkTextureDesc td;
        td.width=atlasW; td.height=cfg.resolution;
        td.format=NkGPUFormat::NK_D32_FLOAT;
        td.usage =NkTextureUsage::NK_DEPTH_STENCIL | NkTextureUsage::NK_SHADER_RESOURCE;
        td.name  ="ShadowAtlas";
        // mAtlas handled by TextureLibrary in practice, stub here
        NkMaterialTemplateDesc md; md.type=NkMaterialType::NK_UNLIT; md.name="ShadowDepth";
        return true;
    }

    void NkShadowSystem::Shutdown() {}

    void NkShadowSystem::BeginShadowPass(NkICommandBuffer* cmd,
                                           NkVec3f lightDir, const NkCamera3D& cam) {
        // Compute CSM split distances (logarithmic)
        float32 n=mCfg.nearPlane, f=mCfg.farPlane;
        float32 lambda=mCfg.lambda;
        for(uint32 c=0;c<mCfg.numCascades;c++){
            float32 clog = n * powf(f/n, (float32)(c+1)/mCfg.numCascades);
            float32 clin = n + (f-n)*((float32)(c+1)/mCfg.numCascades);
            float32 dist = lambda*clog + (1.f-lambda)*clin;
            // Build light-space projection for this cascade
            // Simplified: identity for stub
            mCascadeMats[c] = NkMat4f::Identity();
        }
        mInPass=true;
    }

    void NkShadowSystem::EndShadowPass(NkICommandBuffer* cmd) { mInPass=false; }

    void NkShadowSystem::RenderShadowPasses(NkICommandBuffer* cmd) {
        // Called from RenderGraph shadow pass callback
        // In full impl: loop cascades, bind atlas slice, draw scene depth
    }

} // namespace renderer
} // namespace nkentseu
