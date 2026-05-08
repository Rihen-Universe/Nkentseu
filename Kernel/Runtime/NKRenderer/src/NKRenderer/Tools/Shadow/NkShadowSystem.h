#pragma once
// =============================================================================
// NkShadowSystem.h  — NKRenderer v4.0  (Tools/Shadow/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu { 
    namespace renderer {
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

                // Accesseurs RHI pour Render3D (qui binde le ShadowUBO + atlas dans son
                // descriptor set per-frame). En mode stub (D.3a), atlas = 1x1 depth et
                // mShadowUBO contient cascadeCount=0 -> le shader PBR retourne shadow=1.0.
                NkTextureHandle GetAtlasTexture() const { return mAtlasRhi; }
                NkSamplerHandle GetAtlasSampler() const { return mShadowSampler; }
                NkBufferHandle  GetShadowUBO()    const { return mUBOShadow; }
                const NkMat4f*  GetCascadeMats(uint32* n) const { *n=mActiveCascades; return mCascadeMats; }

            private:
                NkIDevice*          mDevice=nullptr;
                NkMeshSystem*       mMesh=nullptr;
                NkMaterialSystem*   mMat=nullptr;
                NkShadowSystemConfig mCfg;

                // Stub D.3a : atlas 1x1 depth (sera multi-cascade en D.3b).
                NkTextureHandle     mAtlasRhi;        // depth texture (RHI)
                NkSamplerHandle     mShadowSampler;   // sampler avec compare-mode
                NkBufferHandle      mUBOShadow;       // ShadowUBO (cascadeMats + splits + biases + count)

                NkMat4f             mCascadeMats[4]={};
                float32             mCascadeSplits[4]={};
                uint32              mActiveCascades=0;     // 0 en stub D.3a

                NkPipelineHandle    mPipeline;
                bool                mInPass=false;

                void UploadShadowUBO();
        };
    }
} // namespace
