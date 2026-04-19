#pragma once
// =============================================================================
// NkRendererImpl.h
// Implémentation concrète de NkRenderer.
// =============================================================================
#include "NkRenderer.h"
#include "NkRenderGraph.h"
#include "NkTextureLibrary.h"
#include "NkMeshCache.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

namespace nkentseu {

    class NkRendererImpl : public NkRenderer {
        public:
            NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg);
            ~NkRendererImpl() override;

            // ── Cycle de vie ──────────────────────────────────────────────────────────
            bool Initialize() override;
            void Shutdown()   override;
            bool IsValid()    const override;

            // ── Frame ─────────────────────────────────────────────────────────────────
            bool BeginFrame()  override;
            void EndFrame()    override;
            void Present()     override;

            // ── Resize ────────────────────────────────────────────────────────────────
            void OnResize(uint32 width, uint32 height) override;

            // ── Sous-systèmes ─────────────────────────────────────────────────────────
            NkRender2D*         GetRender2D()     override { return mRender2D.get(); }
            NkRender3D*         GetRender3D()     override { return mRender3D.get(); }
            NkOverlayRenderer*  GetOverlay()      override { return mOverlay.get(); }
            NkPostProcessStack* GetPostProcess()  override { return mPostProcess.get(); }
            NkMaterialSystem*   GetMaterials()    override { return mMaterials.get(); }
            NkTextureLibrary*   GetTextures()     override { return mTextures.get(); }
            NkMeshCache*        GetMeshCache()    override { return mMeshes.get(); }
            NkRenderGraph*      GetRenderGraph()  override { return mGraph.get(); }

            // ── Offscreen ─────────────────────────────────────────────────────────────
            NkOffscreenTarget* CreateOffscreenTarget(const NkOffscreenDesc& desc) override;
            void               DestroyOffscreenTarget(NkOffscreenTarget*& target) override;

            // ── Config ────────────────────────────────────────────────────────────────
            void SetVSync(bool enabled)          override;
            void SetMSAA(NkSampleCount msaa)     override;
            void SetHDR(bool enabled)            override;
            void SetShadowQuality(uint32 quality)override;

            // ── Stats ─────────────────────────────────────────────────────────────────
            const NkRendererStats& GetStats() const override { return mStats; }
            void                   ResetStats()     override { mStats = {}; }

            // ── Accès bas niveau ──────────────────────────────────────────────────────
            NkIDevice*        GetDevice()          const override { return mDevice; }
            NkICommandBuffer* GetCommandBuffer()   const override { return mCmd; }
            uint32            GetFrameIndex()      const override { return mFrameIndex; }
            uint32            GetWidth()           const override { return mWidth; }
            uint32            GetHeight()          const override { return mHeight; }

        private:
            NkIDevice*          mDevice;
            NkRendererConfig    mCfg;
            NkICommandBuffer*   mCmd       = nullptr;
            uint32              mWidth     = 0;
            uint32              mHeight    = 0;
            uint32              mFrameIndex= 0;
            bool                mValid     = false;
            bool                mInsideFrame = false;

            // Sous-systèmes (propriétés)
            std::unique_ptr<NkTextureLibrary>   mTextures;
            std::unique_ptr<NkMeshCache>        mMeshes;
            std::unique_ptr<NkMaterialSystem>   mMaterials;
            std::unique_ptr<NkRender2D>         mRender2D;
            std::unique_ptr<NkRender3D>         mRender3D;
            std::unique_ptr<NkPostProcessStack> mPostProcess;
            std::unique_ptr<NkOverlayRenderer>  mOverlay;
            std::unique_ptr<NkRenderGraph>      mGraph;

            // Cibles offscreen gérées
            NkVector<NkOffscreenTarget*> mOffscreenTargets;

            // HDR framebuffer (intermédiaire avant post-process)
            NkOffscreenTarget* mHDRTarget   = nullptr;
            NkOffscreenTarget* mGBuffer     = nullptr;   // pour deferred
            NkOffscreenTarget* mShadowAtlas = nullptr;

            // Passes du render graph
            NkRenderPassHandle mForwardPass;
            NkRenderPassHandle mDeferredPass;
            NkRenderPassHandle mShadowPass;
            NkRenderPassHandle mPostProcessPass;
            NkRenderPassHandle mOverlayPass;

            NkRendererStats mStats;
            NkFrameContext  mFrameCtx;

            // ── Initialisation en séquence ────────────────────────────────────────────
            bool InitTextures();
            bool InitMeshes();
            bool InitMaterials();
            bool InitRenderPasses();
            bool InitRender2D();
            bool InitRender3D();
            bool InitPostProcess();
            bool InitOverlay();
            bool InitGraph();
            bool InitHDRTarget();

            // ── Orchestration de la frame ─────────────────────────────────────────────
            void RecordFrame();
            void RecordShadowPasses();
            void RecordGeometryPass();
            void RecordLightingPass();    // deferred uniquement
            void RecordPostProcessPass();
            void RecordOverlayPass();
            void RecordFinalBlit();       // blit HDR → swapchain

            // ── Nettoyage ─────────────────────────────────────────────────────────────
            void DestroyInternalTargets();
            void DestroyOffscreenTargets();
    };

} // namespace nkentseu

#pragma once
// =============================================================================
// NkRendererImpl.h — Implémentation concrète de NkRenderer
// =============================================================================
#include "NkRendererConfig.h"
#include "NkTextureLibrary.h"
#include "../Mesh/NkMeshSystem.h"
#include "../Materials/NkMaterialSystem.h"
#include "../Shader/NkShaderCompiler.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include <memory>

namespace nkentseu {

    // Forward declarations
    class NkRender2D;
    class NkRender3D;
    class NkPostProcessStack;
    class NkOverlayRenderer;
    class NkOffscreenTarget;
    class NkParticleSystem;
    class NkVFXSystem;
    class NkSimulationRenderer;
    class NkAnimationSystem;

    // =========================================================================
    // Statistiques de rendu
    // =========================================================================
    struct NkRendererStats {
        uint32  drawCalls      = 0;
        uint32  triangles      = 0;
        uint32  materialSwaps  = 0;
        uint32  textureBinds   = 0;
        uint32  shadowCasters  = 0;
        uint32  particleCount  = 0;
        float32 gpuTimeMs      = 0.f;
        float32 cpuBuildTimeMs = 0.f;
        uint64  gpuMemoryBytes = 0;
    };

    // =========================================================================
    // NkRenderer — Interface abstraite
    // =========================================================================
    class NkRenderer {
    public:
        static NkRenderer*  Create(NkIDevice* device, const NkRendererConfig& cfg);
        static void         Destroy(NkRenderer*& renderer);
        virtual ~NkRenderer() = default;

        virtual bool Initialize()  = 0;
        virtual void Shutdown()    = 0;
        virtual bool IsValid() const = 0;

        virtual bool BeginFrame()  = 0;
        virtual void EndFrame()    = 0;
        virtual void Present()     = 0;
        virtual void OnResize(uint32 w, uint32 h) = 0;

        // Sous-systèmes
        virtual NkRender2D*         GetRender2D()     = 0;
        virtual NkRender3D*         GetRender3D()     = 0;
        virtual NkOverlayRenderer*  GetOverlay()      = 0;
        virtual NkPostProcessStack* GetPostProcess()  = 0;
        virtual NkMaterialSystem*   GetMaterials()    = 0;
        virtual NkTextureLibrary*   GetTextures()     = 0;
        virtual NkMeshSystem*       GetMeshSystem()   = 0;
        virtual NkShaderCompiler*   GetShaderCompiler()= 0;
        virtual NkParticleSystem*   GetParticles()    = 0;
        virtual NkVFXSystem*        GetVFX()          = 0;
        virtual NkSimulationRenderer* GetSimulation() = 0;
        virtual NkAnimationSystem*  GetAnimation()    = 0;

        // Offscreen targets
        virtual NkOffscreenTarget* CreateOffscreenTarget(const struct NkOffscreenDesc& desc) = 0;
        virtual void               DestroyOffscreenTarget(NkOffscreenTarget*& t)             = 0;

        // Config dynamique
        virtual void SetVSync(bool v)          = 0;
        virtual void SetMSAA(NkSampleCount m)  = 0;
        virtual void SetHDR(bool v)            = 0;
        virtual void SetShadowQuality(uint32 q)= 0;

        // Stats
        virtual const NkRendererStats& GetStats()     const = 0;
        virtual void                   ResetStats()         = 0;

        // Accès bas-niveau
        virtual NkIDevice*        GetDevice()         const = 0;
        virtual NkICommandBuffer* GetCommandBuffer()  const = 0;
        virtual uint32            GetFrameIndex()     const = 0;
        virtual uint32            GetWidth()          const = 0;
        virtual uint32            GetHeight()         const = 0;
    };

    // =========================================================================
    // NkRendererImpl — Implémentation
    // =========================================================================
    class NkRendererImpl : public NkRenderer {
    public:
        NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg);
        ~NkRendererImpl() override;

        bool Initialize() override;
        void Shutdown()   override;
        bool IsValid()    const override { return mValid; }

        bool BeginFrame()  override;
        void EndFrame()    override;
        void Present()     override;
        void OnResize(uint32 w, uint32 h) override;

        NkRender2D*           GetRender2D()      override { return mRender2D.get(); }
        NkRender3D*           GetRender3D()      override { return mRender3D.get(); }
        NkOverlayRenderer*    GetOverlay()       override { return mOverlay.get(); }
        NkPostProcessStack*   GetPostProcess()   override { return mPostProcess.get(); }
        NkMaterialSystem*     GetMaterials()     override { return mMaterials.get(); }
        NkTextureLibrary*     GetTextures()      override { return mTextures.get(); }
        NkMeshSystem*         GetMeshSystem()    override { return mMeshSys.get(); }
        NkShaderCompiler*     GetShaderCompiler()override { return mShaderCompiler.get(); }
        NkParticleSystem*     GetParticles()     override { return mParticles.get(); }
        NkVFXSystem*          GetVFX()           override { return mVFX.get(); }
        NkSimulationRenderer* GetSimulation()    override { return mSimulation.get(); }
        NkAnimationSystem*    GetAnimation()     override { return mAnimation.get(); }

        NkOffscreenTarget* CreateOffscreenTarget(const struct NkOffscreenDesc& desc) override;
        void               DestroyOffscreenTarget(NkOffscreenTarget*& t)             override;

        void SetVSync(bool v)          override { mCfg.vsync=v; }
        void SetMSAA(NkSampleCount m)  override { mCfg.msaa=m; }
        void SetHDR(bool v)            override { mCfg.hdr=v; }
        void SetShadowQuality(uint32 q)override;

        const NkRendererStats& GetStats() const override { return mStats; }
        void                   ResetStats()     override { mStats={}; }

        NkIDevice*        GetDevice()        const override { return mDevice; }
        NkICommandBuffer* GetCommandBuffer() const override { return mCmd; }
        uint32            GetFrameIndex()    const override { return mFrameCtx.frameIndex; }
        uint32            GetWidth()         const override { return mWidth; }
        uint32            GetHeight()        const override { return mHeight; }

    private:
        NkIDevice*       mDevice;
        NkRendererConfig mCfg;
        NkICommandBuffer* mCmd       = nullptr;
        uint32            mWidth     = 0;
        uint32            mHeight    = 0;
        bool              mValid     = false;
        bool              mInsideFrame=false;
        NkFrameContext    mFrameCtx;
        NkRendererStats   mStats;

        // Sous-systèmes
        std::unique_ptr<NkTextureLibrary>    mTextures;
        std::unique_ptr<NkMeshSystem>        mMeshSys;
        std::unique_ptr<NkMaterialSystem>    mMaterials;
        std::unique_ptr<NkShaderCompiler>    mShaderCompiler;
        std::unique_ptr<NkRender2D>          mRender2D;
        std::unique_ptr<NkRender3D>          mRender3D;
        std::unique_ptr<NkPostProcessStack>  mPostProcess;
        std::unique_ptr<NkOverlayRenderer>   mOverlay;
        std::unique_ptr<NkParticleSystem>    mParticles;
        std::unique_ptr<NkVFXSystem>         mVFX;
        std::unique_ptr<NkSimulationRenderer>mSimulation;
        std::unique_ptr<NkAnimationSystem>   mAnimation;

        // Passes RHI
        NkRenderPassHandle  mForwardPass;
        NkRenderPassHandle  mDeferredPass;
        NkRenderPassHandle  mShadowPass;
        NkRenderPassHandle  mOverlayPass;

        // HDR framebuffer
        NkOffscreenTarget*  mHDRTarget   = nullptr;

        // Offscreen targets gérés
        NkVector<NkOffscreenTarget*> mOffscreenTargets;

        // Init
        bool InitTextures();
        bool InitMeshes();
        bool InitShaderCompiler();
        bool InitMaterials();
        bool InitRenderPasses();
        bool InitRender2D();
        bool InitRender3D();
        bool InitPostProcess();
        bool InitOverlay();
        bool InitParticles();
        bool InitVFX();
        bool InitSimulation();
        bool InitAnimation();
        bool InitHDRTarget();

        // Frame
        void RecordFrame();
        void RecordShadowPasses();
        void RecordGeometryPass();
        void RecordPostProcessPass();
        void RecordOverlayPass();
        void DestroyAll();
    };

} // namespace nkentseu
