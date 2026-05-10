#pragma once
// =============================================================================
// NkPostProcessStack.h  — NKRenderer v4.0  (Tools/PostProcess/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {
        class NkTextureLibrary;
        class NkMeshSystem;
        class NkShaderLibrary;
        class NkResources;

        class NkPostProcessStack {
            public:
                bool Init(NkIDevice* d, NkTextureLibrary* t, NkMeshSystem* m,
                          NkShaderLibrary* sl, NkResources* res,
                          uint32 w, uint32 h);
                void Shutdown();
                void OnResize(uint32 w, uint32 h);
                void SetConfig(const NkPostConfig& c) { mCfg=c; }
                NkPostConfig& GetConfig() { return mCfg; }
                NkTexHandle Execute(NkICommandBuffer* cmd,
                                    NkTexHandle hdrIn, NkTexHandle depth,
                                    NkTexHandle velocity=NkTexHandle::Null());

                // Variante consommant directement des handles RHI (utilise par
                // NkRenderGraph qui stocke des NkTextureHandle dans ses transients).
                // Ne wrappe pas dans NkTextureLibrary — le bind est fait direct
                // via mDevice->BindTextureSampler dans DrawFullscreen.
                void ExecuteRHI(NkICommandBuffer* cmd, NkTextureHandle hdrIn);
                NkTexHandle RunSSAO   (NkICommandBuffer* cmd, NkTexHandle depth, NkTexHandle normal);
                NkTexHandle RunBloom  (NkICommandBuffer* cmd, NkTexHandle hdr);
                NkTexHandle RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr);
                NkTexHandle RunFXAA   (NkICommandBuffer* cmd, NkTexHandle ldr);

                // Vrai si au moins un effet est actif dans la config — utilise par
                // BuildDefaultRenderGraph pour decider d'activer le path HDR transient.
                bool HasAnyEffect() const noexcept {
                    return mCfg.ssao || mCfg.bloom || mCfg.toneMapping || mCfg.fxaa || mCfg.aces;
                }

            private:
                NkIDevice*        mDevice    = nullptr;
                NkTextureLibrary* mTex       = nullptr;
                NkMeshSystem*     mMesh      = nullptr;
                NkShaderLibrary*  mShaderLib = nullptr;
                NkResources*      mResources = nullptr;
                NkPostConfig      mCfg;
                uint32 mW=0,mH=0;
                NkTexHandle mSSAOTex, mBloomTex[6], mToneTex, mFinalTex;
                NkPipelineHandle mPipeSSAO,mPipeBloom,mPipeTone,mPipeFXAA;

                // Shaders RHI (handle revoyes par NkShaderLibrary)
                ::nkentseu::NkShaderHandle mShaderTone;

                // Descriptor set layout (1 sampler) + set alloue, refresh par Run*
                NkDescSetHandle  mInputTexLayout;
                NkDescSetHandle  mInputTexSet;
                NkMeshHandle     mQuad;

                void CreateTextures();
                void DrawFullscreen(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                                    NkTexHandle src, const void* pushConst, uint32 pcSize);
        };
    }
} // namespace
