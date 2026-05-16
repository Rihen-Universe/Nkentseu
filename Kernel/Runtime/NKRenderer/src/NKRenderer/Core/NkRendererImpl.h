#pragma once
// =============================================================================
// NkRendererImpl.h  — NKRenderer v4.0
// Implémentation concrète de NkRenderer.
// Possède tous les sous-systèmes. Thread-safe sur Init/Shutdown.
// =============================================================================
#include "NKRenderer/NkRenderer.h"
#include "NkRenderGraph.h"
#include "NkTextureLibrary.h"
#include "NkResources.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Materials/NkMaterialLibrary.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Simulation/NkSimulationRenderer.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {
    namespace renderer {

        class NkRendererImpl final : public NkRenderer {
            public:
                NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg);
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
                NkShaderLibrary*      GetShaders()      override { return mShaders.Get(); }
                NkMeshSystem*         GetMeshSystem()   override { return mMeshSystem.Get(); }
                NkMaterialSystem*     GetMaterials()    override { return mMaterials.Get(); }
                NkRender2D*           GetRender2D()     override { return mRender2D.Get(); }
                NkRender3D*           GetRender3D()     override { return mRender3D.Get(); }
                class NkMaterialCollection* GetMaterialCollection() override { return mMaterialCollection.Get(); }
                NkTextRenderer*       GetTextRenderer() override { return mTextRenderer.Get(); }
                NkPostProcessStack*   GetPostProcess()  override { return mPostProcess.Get(); }
                NkOverlayRenderer*    GetOverlay()      override { return mOverlay.Get(); }
                NkShadowSystem*       GetShadow()       override { return mShadow.Get(); }
                NkVFXSystem*          GetVFX()          override { return mVFX.Get(); }
                NkAnimationSystem*    GetAnimation()    override { return mAnimation.Get(); }
                NkSimulationRenderer* GetSimulation()   override { return mSimulation.Get(); }

                NkOffscreenTarget* CreateOffscreen(const NkOffscreenDesc& desc) override;
                void               DestroyOffscreen(NkOffscreenTarget*& t)      override;

                class NkPlanarReflectionSystem* GetPlanarReflection() override { return mPlanarReflection.Get(); }

                void SetVSync     (bool e)          override;
                void SetPostConfig(const NkPostConfig& pp) override;
                void SetWireframe (bool e)          override;

                // Runtime subsystem toggle
                bool             EnableSubsystem  (NkSubsystemFlags flags)       override;
                void             DisableSubsystem (NkSubsystemFlags flags)       override;
                bool             IsSubsystemActive(NkSubsystemFlags flags) const override;
                NkSubsystemFlags GetActiveSubsystems()                    const override;

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
                NkRendererConfig mCfg;
                NkICommandBuffer*mCmd     = nullptr;
                NkISwapchain*    mSwapchain= nullptr;
                uint32           mFrameIndex = 0;
                uint32           mFrameCounter = 0; // throttle counter for hot-reload polling
                NkFrameContext   mFrameCtx;
                bool             mInitialized= false;
                NkRendererStats  mStats;

                // Sous-systèmes (ordre d'initialisation = ordre de déclaration)
                memory::NkUniquePtr<NkResources>          mResources;     // toujours actif (default tex/samplers/layouts)
                memory::NkUniquePtr<NkShaderLibrary>      mShaders;       // toujours actif (compile/cache des shaders)
                memory::NkUniquePtr<NkRenderGraph>        mRenderGraph;
                memory::NkUniquePtr<NkTextureLibrary>     mTextures;
                memory::NkUniquePtr<NkMeshSystem>         mMeshSystem;
                memory::NkUniquePtr<NkMaterialSystem>     mMaterials;
                memory::NkUniquePtr<NkMaterialLibrary>    mMaterialLibrary; // Phase G
                memory::NkUniquePtr<class NkMaterialCollection> mMaterialCollection; // Phase M.2
                memory::NkUniquePtr<NkShadowSystem>       mShadow;
                memory::NkUniquePtr<NkEnvironmentSystem>  mEnvironment;
                memory::NkUniquePtr<NkRender2D>           mRender2D;
                memory::NkUniquePtr<NkRender3D>           mRender3D;
                memory::NkUniquePtr<NkTextRenderer>       mTextRenderer;
                memory::NkUniquePtr<NkPostProcessStack>   mPostProcess;
                memory::NkUniquePtr<NkOverlayRenderer>    mOverlay;
                memory::NkUniquePtr<NkVFXSystem>          mVFX;
                memory::NkUniquePtr<NkAnimationSystem>    mAnimation;
                memory::NkUniquePtr<NkSimulationRenderer> mSimulation;
                memory::NkUniquePtr<class NkPlanarReflectionSystem> mPlanarReflection;

                NkVector<NkOffscreenTarget*> mOffscreenTargets;

                bool InitRHI();
                void BuildDefaultRenderGraph();

                // ── Helpers d'init/teardown par sous-systeme (utilises a la fois
                //    par Initialize() et par EnableSubsystem/DisableSubsystem) ────
                bool InitShadow();
                bool InitEnvironment();
                bool InitRender2D();
                bool InitRender3D();
                bool InitTextRenderer();
                bool InitPostProcess();
                bool InitOverlay();
                bool InitVFX();
                bool InitAnimation();
                bool InitSimulation();

                // Reconstruit le render graph apres changement de sous-systemes
                void RebuildRenderGraph();
        };

    } // namespace renderer
} // namespace nkentseu
