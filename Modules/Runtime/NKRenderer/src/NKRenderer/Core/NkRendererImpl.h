#pragma once
// =============================================================================
// NkRendererImpl.h  — NKRenderer v4.0
// Implémentation concrète de NkRenderer.
// Possède tous les sous-systèmes. Thread-safe sur Init/Shutdown.
// =============================================================================
#include "NKRenderer/NkRenderer.h"
#include "NkRenderGraph.h"
#include "NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Simulation/NkSimulationRenderer.h"
#include "NKRenderer/Tools/Culling/NkCullingSystem.h"
#include "NKRenderer/Tools/Raytracing/NkRaytracingSystem.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"
#include "NKRenderer/Tools/Denoiser/NkDenoiserSystem.h"
#include "NKRenderer/Tools/AIRendering/NkAIRenderingTarget.h"
#include "NKRenderer/Passes/Deferred/NkDeferredPass.h"
#include "NKRenderer/Streaming/NkStreamingSystem.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {
namespace renderer {

    class NkRendererImpl final : public NkRenderer {
    public:
        NkRendererImpl(NkIDevice* device,
                        const NkSurfaceDesc& surface,
                        const NkRendererConfig& cfg);
        ~NkRendererImpl() override;

        // NkRenderer interface
        bool Initialize() override;
        void Shutdown()   override;
        bool IsValid()    const override { return mInitialized; }

        bool BeginFrame() override;
        void EndFrame()   override;
        void Present()    override;
        void OnResize(uint32 w, uint32 h) override;

        NkRenderGraph*        GetRenderGraph()  override { return mRenderGraph.Get(); }
        NkTextureLibrary*     GetTextures()     override { return mTextures.Get(); }
        NkMeshSystem*         GetMeshSystem()   override { return mMeshSystem.Get(); }
        NkMaterialSystem*     GetMaterials()    override { return mMaterials.Get(); }
        NkRender2D*           GetRender2D()     override { return mRender2D.Get(); }
        NkRender3D*           GetRender3D()     override { return mRender3D.Get(); }
        NkTextRenderer*       GetTextRenderer() override { return mTextRenderer.Get(); }
        NkPostProcessStack*   GetPostProcess()  override { return mPostProcess.Get(); }
        NkOverlayRenderer*    GetOverlay()      override { return mOverlay.Get(); }
        NkShadowSystem*       GetShadow()       override { return mShadow.Get(); }
        NkVFXSystem*          GetVFX()          override { return mVFX.Get(); }
        NkAnimationSystem*    GetAnimation()    override { return mAnimation.Get(); }
        NkSimulationRenderer* GetSimulation()   override { return mSimulation.Get(); }

        NkCullingSystem*      GetCulling()      override { return mCulling.Get(); }
        NkDeferredPass*       GetDeferred()     override { return mDeferred.Get(); }
        NkStreamingSystem*    GetStreaming()     override { return mStreaming.Get(); }
        NkRaytracingSystem*   GetRaytracing()   override { return mRaytracing.Get(); }
        NkIKSystem*           GetIK()           override { return mIK.Get(); }
        NkDenoiserSystem*     GetDenoiser()     override { return mDenoiser.Get(); }
        NkAIRenderingSystem*  GetAIRendering()  override { return mAIRendering.Get(); }

        NkOffscreenTarget* CreateOffscreen(const NkOffscreenDesc& desc) override;
        void               DestroyOffscreen(NkOffscreenTarget*& t)      override;

        void SetVSync     (bool e)          override;
        void SetPostConfig(const NkPostConfig& pp) override;
        void SetWireframe (bool e)          override;

        const NkRendererStats& GetStats()   const override { return mStats; }
        void                   ResetStats()       override { mStats.Reset(); }

        NkIDevice*              GetDevice()     const override { return mDevice; }
        NkICommandBuffer*       GetCmd()        const override { return mCmd; }
        uint32                  GetFrameIndex() const override { return mFrameIndex; }
        uint32                  GetWidth()      const override { return mCfg.width; }
        uint32                  GetHeight()     const override { return mCfg.height; }
        const NkRendererConfig& GetConfig()     const override { return mCfg; }

    private:
        NkIDevice*       mDevice  = nullptr;
        NkSurfaceDesc    mSurface;
        NkRendererConfig mCfg;
        NkICommandBuffer*mCmd     = nullptr;
        NkISwapchain*    mSwapchain= nullptr;
        uint32           mFrameIndex = 0;
        bool             mInitialized= false;
        NkRendererStats  mStats;

        // Sous-systèmes (ordre d'initialisation = ordre de déclaration)
        memory::NkUniquePtr<NkRenderGraph>        mRenderGraph;
        memory::NkUniquePtr<NkTextureLibrary>     mTextures;
        memory::NkUniquePtr<NkMeshSystem>         mMeshSystem;
        memory::NkUniquePtr<NkMaterialSystem>     mMaterials;
        memory::NkUniquePtr<NkShadowSystem>       mShadow;
        memory::NkUniquePtr<NkRender2D>           mRender2D;
        memory::NkUniquePtr<NkRender3D>           mRender3D;
        memory::NkUniquePtr<NkTextRenderer>       mTextRenderer;
        memory::NkUniquePtr<NkPostProcessStack>   mPostProcess;
        memory::NkUniquePtr<NkOverlayRenderer>    mOverlay;
        memory::NkUniquePtr<NkVFXSystem>          mVFX;
        memory::NkUniquePtr<NkAnimationSystem>    mAnimation;
        memory::NkUniquePtr<NkSimulationRenderer> mSimulation;
        // Nouveaux sous-systèmes
        memory::NkUniquePtr<NkCullingSystem>      mCulling;
        memory::NkUniquePtr<NkDeferredPass>       mDeferred;
        memory::NkUniquePtr<NkStreamingSystem>    mStreaming;
        memory::NkUniquePtr<NkRaytracingSystem>   mRaytracing;
        memory::NkUniquePtr<NkIKSystem>           mIK;
        memory::NkUniquePtr<NkDenoiserSystem>     mDenoiser;
        memory::NkUniquePtr<NkAIRenderingSystem>  mAIRendering;

        NkVector<NkOffscreenTarget*> mOffscreenTargets;

        bool InitRHI();
        void BuildDefaultRenderGraph();
    };

} // namespace renderer
} // namespace nkentseu
