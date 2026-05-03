#pragma once
// =============================================================================
// NkShadowSystem.h  — NKRenderer v4.0  (Tools/Shadow/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu { namespace renderer {
    class NkMeshSystem; class NkMaterialSystem;

    enum class NkPCFMode : uint8 { NONE=0, PCF3x3, PCF5x5, POISSON, PCSS };

    struct NkShadowSystemConfig {
        uint32    resolution   = 2048;
        uint32    numCascades  = 3;
        NkPCFMode pcfMode      = NkPCFMode::PCF3x3;
        float32   nearPlane    = 0.1f;
        float32   farPlane     = 200.f;
        float32   lambda       = 0.75f;
        float32   normalBias   = 0.005f;
        float32   depthBias    = 1.5f;
        bool      stable       = true;
        bool      visualize    = false;
    };

    class NkShadowSystem {
    public:
        bool Init(NkIDevice* d, NkMeshSystem* m, NkMaterialSystem* mat,
                   const NkShadowSystemConfig& cfg = {});
        void Shutdown();
        void SetConfig(const NkShadowSystemConfig& c) { mCfg=c; }
        void BeginShadowPass(NkICommandBuffer* cmd, NkVec3f lightDir,
                              const NkCamera3D& mainCam);
        void EndShadowPass(NkICommandBuffer* cmd);
        void RenderShadowPasses(NkICommandBuffer* cmd);
        NkTexHandle GetCascadeAtlas()  const { return mAtlas; }
        const NkMat4f* GetCascadeMats(uint32* n) const { *n=mCfg.numCascades; return mCascadeMats; }

    private:
        NkIDevice*          mDevice=nullptr;
        NkMeshSystem*       mMesh=nullptr;
        NkMaterialSystem*   mMat=nullptr;
        NkShadowSystemConfig mCfg;
        NkTexHandle         mAtlas;
        NkMat4f             mCascadeMats[4]={};
        NkPipelineHandle    mPipeline;
        bool                mInPass=false;
    };
}} // namespace
