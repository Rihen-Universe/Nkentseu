#pragma once
// =============================================================================
// NkRendererImpl.h  — NKRenderer v4.0
// Implémentation concrète de NkRenderer.
// Possède tous les sous-systèmes. Thread-safe sur Init/Shutdown.
// =============================================================================
#include "../NkRenderer.h"
#include "NkRenderGraph.h"
#include "NkTextureLibrary.h"
#include "../Mesh/NkMeshSystem.h"
#include "../Materials/NkMaterialSystem.h"
#include "../Tools/Render2D/NkRender2D.h"
#include "../Tools/Render3D/NkRender3D.h"
#include "../Tools/Text/NkTextRenderer.h"
#include "../Tools/Shadow/NkShadowSystem.h"
#include "../Tools/PostProcess/NkPostProcessStack.h"
#include "../Tools/Overlay/NkOverlayRenderer.h"
#include "../Tools/Offscreen/NkOffscreenTarget.h"
#include "../Tools/VFX/NkVFXSystem.h"
#include "../Tools/Animation/NkAnimationSystem.h"
#include "../Tools/Simulation/NkSimulationRenderer.h"
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

        NkVector<NkOffscreenTarget*> mOffscreenTargets;

        bool InitRHI();
        void BuildDefaultRenderGraph();
    };

} // namespace renderer
} // namespace nkentseu
