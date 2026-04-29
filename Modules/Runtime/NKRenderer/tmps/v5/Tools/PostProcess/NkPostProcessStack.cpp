// =============================================================================
// NkPostProcessStack.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkPostProcessStack.h"
#include "../../Core/NkTextureLibrary.h"
#include "../../Mesh/NkMeshSystem.h"

namespace nkentseu {
namespace renderer {

    bool NkPostProcessStack::Init(NkIDevice* d, NkTextureLibrary* t,
                                    NkMeshSystem* m, uint32 w, uint32 h) {
        mDevice=d; mTex=t; mMesh=m; mW=w; mH=h;
        mQuad = m->GetQuad();
        CreateTextures();
        // Compile pipelines
        NkPipelineDesc pd; pd.type=NkPipelineType::NK_POSTPROCESS;
        pd.name="PP_SSAO";   mPipeSSAO =mDevice->CreatePipeline(pd);
        pd.name="PP_Bloom";  mPipeBloom=mDevice->CreatePipeline(pd);
        pd.name="PP_Tone";   mPipeTone =mDevice->CreatePipeline(pd);
        pd.name="PP_FXAA";   mPipeFXAA =mDevice->CreatePipeline(pd);
        return true;
    }

    void NkPostProcessStack::Shutdown() { }

    void NkPostProcessStack::OnResize(uint32 w, uint32 h) {
        mW=w; mH=h; CreateTextures();
    }

    void NkPostProcessStack::CreateTextures() {
        if (!mTex||!mDevice) return;
        mSSAOTex = mTex->CreateRenderTarget(mW/2,mH/2,NkGPUFormat::NK_R8_UNORM,false,true,"SSAO");
        for(int i=0;i<6;i++){
            uint32 s=1<<i;
            mBloomTex[i]=mTex->CreateRenderTarget(mW/s,mH/s,NkGPUFormat::NK_RGBA16F,false,true,"Bloom");
        }
        mToneTex  = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Tone");
        mFinalTex = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Final");
    }

    NkTexHandle NkPostProcessStack::Execute(NkICommandBuffer* cmd,
                                              NkTexHandle hdrIn, NkTexHandle depth,
                                              NkTexHandle velocity) {
        NkTexHandle cur = hdrIn;
        if (mCfg.ssao)          cur = RunSSAO(cmd, depth, NkTexHandle::Null());
        if (mCfg.bloom)         cur = RunBloom(cmd, cur);
        if (mCfg.toneMapping)   cur = RunTonemap(cmd, cur);
        if (mCfg.fxaa)          cur = RunFXAA(cmd, cur);
        return cur;
    }

    NkTexHandle NkPostProcessStack::RunSSAO(NkICommandBuffer* cmd,
                                              NkTexHandle depth, NkTexHandle normal) {
        // Fullscreen pass : bind depth, output SSAO
        (void)cmd; (void)depth; (void)normal;
        return mSSAOTex;
    }

    NkTexHandle NkPostProcessStack::RunBloom(NkICommandBuffer* cmd, NkTexHandle hdr) {
        // Downsample → upsample pass chain
        (void)cmd; (void)hdr;
        return mBloomTex[0];
    }

    NkTexHandle NkPostProcessStack::RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr) {
        // ACES/Reinhard + gamma correction
        (void)cmd; (void)hdr;
        return mToneTex;
    }

    NkTexHandle NkPostProcessStack::RunFXAA(NkICommandBuffer* cmd, NkTexHandle ldr) {
        (void)cmd; (void)ldr;
        return mFinalTex;
    }

} // namespace renderer
} // namespace nkentseu
