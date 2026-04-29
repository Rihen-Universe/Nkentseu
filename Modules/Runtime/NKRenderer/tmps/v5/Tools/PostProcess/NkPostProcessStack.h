#pragma once
// =============================================================================
// NkPostProcessStack.h  — NKRenderer v4.0  (Tools/PostProcess/)
// =============================================================================
#include "../../Core/NkRendererTypes.h"
#include "../../Core/NkRendererConfig.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
namespace nkentseu { namespace renderer {
    class NkTextureLibrary; class NkMeshSystem;

    class NkPostProcessStack {
    public:
        bool Init(NkIDevice* d, NkTextureLibrary* t, NkMeshSystem* m, uint32 w, uint32 h);
        void Shutdown();
        void OnResize(uint32 w, uint32 h);
        void SetConfig(const NkPostConfig& c) { mCfg=c; }
        NkPostConfig& GetConfig() { return mCfg; }
        NkTexHandle Execute(NkICommandBuffer* cmd,
                              NkTexHandle hdrIn, NkTexHandle depth,
                              NkTexHandle velocity=NkTexHandle::Null());
        NkTexHandle RunSSAO   (NkICommandBuffer* cmd, NkTexHandle depth, NkTexHandle normal);
        NkTexHandle RunBloom  (NkICommandBuffer* cmd, NkTexHandle hdr);
        NkTexHandle RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr);
        NkTexHandle RunFXAA   (NkICommandBuffer* cmd, NkTexHandle ldr);
    private:
        NkIDevice* mDevice=nullptr; NkTextureLibrary* mTex=nullptr;
        NkMeshSystem* mMesh=nullptr; NkPostConfig mCfg;
        uint32 mW=0,mH=0;
        NkTexHandle mSSAOTex, mBloomTex[6], mToneTex, mFinalTex;
        NkPipelineHandle mPipeSSAO,mPipeBloom,mPipeTone,mPipeFXAA;
        NkMeshHandle     mQuad;
        void CreateTextures();
    };
}} // namespace
