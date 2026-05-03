// =============================================================================
// NkRendererImpl.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRendererImpl.h"
#include "NkISwapchain.h"

namespace nkentseu {
namespace renderer {

    // ── Fabrique statique ─────────────────────────────────────────────────────
    NkRenderer* NkRenderer::Create(NkIDevice* device,
                                     const NkSurfaceDesc& surface,
                                     const NkRendererConfig& cfg) {
        auto* r = new NkRendererImpl(device, surface, cfg);
        if (!r->Initialize()) {
            delete r;
            return nullptr;
        }
        return r;
    }

    void NkRenderer::Destroy(NkRenderer*& renderer) {
        if (renderer) {
            renderer->Shutdown();
            delete renderer;
            renderer = nullptr;
        }
    }

    // ── Constructor / Destructor ──────────────────────────────────────────────
    NkRendererImpl::NkRendererImpl(NkIDevice* device,
                                     const NkSurfaceDesc& surface,
                                     const NkRendererConfig& cfg)
        : mDevice(device), mSurface(surface), mCfg(cfg) {}

    NkRendererImpl::~NkRendererImpl() {
        Shutdown();
    }

    // ── Initialize ────────────────────────────────────────────────────────────
    bool NkRendererImpl::Initialize() {
        if (mInitialized) return true;
        if (!mDevice) return false;

        // 1. RHI : swapchain
        if (!InitRHI()) return false;

        // 2. RenderGraph
        mRenderGraph.Reset(new NkRenderGraph(mDevice));

        // 3. Texture library
        mTextures.Reset(new NkTextureLibrary());
        if (!mTextures->Init(mDevice)) return false;

        // 4. Mesh system
        mMeshSystem.Reset(new NkMeshSystem());
        if (!mMeshSystem->Init(mDevice)) return false;

        // 5. Material system
        mMaterials.Reset(new NkMaterialSystem());
        if (!mMaterials->Init(mDevice, mTextures.Get())) return false;

        // 6. Shadow system
        NkShadowSystemConfig shadowCfg;
        shadowCfg.resolution   = mCfg.shadow.resolution;
        shadowCfg.numCascades  = mCfg.shadow.cascadeCount;
        shadowCfg.pcfMode      = mCfg.shadow.pcss
                                  ? NkPCFMode::PCSS
                                  : (mCfg.shadow.softShadows ? NkPCFMode::PCF3x3 : NkPCFMode::NONE);
        mShadow.Reset(new NkShadowSystem());
        if (!mShadow->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(), shadowCfg)) return false;

        // 7. Render2D
        mRender2D.Reset(new NkRender2D());
        if (!mRender2D->Init(mDevice, mTextures.Get())) return false;

        // 8. Render3D
        mRender3D.Reset(new NkRender3D());
        if (!mRender3D->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(),
                              mRenderGraph.Get(), mShadow.Get())) return false;

        // 9. Text renderer
        mTextRenderer.Reset(new NkTextRenderer());
        if (!mTextRenderer->Init(mDevice, mTextures.Get(), mRender2D.Get())) return false;

        // 10. Post-process
        mPostProcess.Reset(new NkPostProcessStack());
        if (!mPostProcess->Init(mDevice, mTextures.Get(), mMeshSystem.Get(),
                                 mCfg.width, mCfg.height)) return false;
        mPostProcess->SetConfig(mCfg.postProcess);

        // 11. Overlay
        mOverlay.Reset(new NkOverlayRenderer());
        if (!mOverlay->Init(mDevice, mRender2D.Get(), mTextRenderer.Get())) return false;

        // 12. VFX
        mVFX.Reset(new NkVFXSystem());
        if (!mVFX->Init(mDevice, mTextures.Get(), mMeshSystem.Get())) return false;

        // 13. Animation
        mAnimation.Reset(new NkAnimationSystem());
        if (!mAnimation->Init(mDevice, mRender3D.Get())) return false;

        // 14. Simulation renderer (stub pour PV3DE etc.)
        mSimulation.Reset(new NkSimulationRenderer());
        if (!mSimulation->Init(mDevice, mRender3D.Get(), mVFX.Get())) return false;

        // 15. Build default render graph passes
        BuildDefaultRenderGraph();

        mInitialized = true;
        return true;
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────
    void NkRendererImpl::Shutdown() {
        if (!mInitialized) return;

        // Détruire offscreens d'abord
        for (auto* t : mOffscreenTargets) {
            t->Shutdown();
            delete t;
        }
        mOffscreenTargets.Clear();

        // Inverse de l'ordre d'init
        mSimulation.Reset();
        mAnimation.Reset();
        mVFX.Reset();
        mOverlay.Reset();
        mPostProcess.Reset();
        mTextRenderer.Reset();
        mRender3D.Reset();
        mRender2D.Reset();
        mShadow.Reset();
        mMaterials.Reset();
        mMeshSystem.Reset();
        mTextures.Reset();
        mRenderGraph.Reset();

        if (mSwapchain) { mDevice->DestroySwapchain(mSwapchain); mSwapchain=nullptr; }

        mInitialized = false;
    }

    // ── RHI init ──────────────────────────────────────────────────────────────
    bool NkRendererImpl::InitRHI() {
        NkDeviceInitInfo initInfo;
        initInfo.surface = mSurface;
        initInfo.width   = mCfg.width;
        initInfo.height  = mCfg.height;

        NkSwapchainDesc sc;
        sc.width      = mCfg.width;
        sc.height     = mCfg.height;
        sc.vsync      = mCfg.vsync;
        sc.imageCount = 2;
        mSwapchain = mDevice->CreateSwapchain(initInfo, sc);
        return mSwapchain != nullptr;
    }

    // ── Build default render graph ─────────────────────────────────────────────
    void NkRendererImpl::BuildDefaultRenderGraph() {
        auto& g = *mRenderGraph;

        // Importer swapchain comme resource (texture handle stub — géré par NkISwapchain)
        auto colorId = g.ImportTexture("Swapchain",
                                        NkTextureHandle{},
                                        NkResourceState::NK_PRESENT);
        auto depthId = g.CreateTransient("MainDepth",
                                          NkTextureDesc::DepthStencil(mCfg.width, mCfg.height));
        auto hdrId   = g.CreateTransient("HDR",
                                          NkTextureDesc::RenderTarget(mCfg.width, mCfg.height,
                                                                        NkGPUFormat::NK_RGBA16_FLOAT));

        // Shadow pass
        if (mCfg.shadow.enabled) {
            auto shadowId = g.CreateTransient("ShadowAtlas",
                NkTextureDesc::DepthStencil(
                    mCfg.shadow.resolution * (int32)mCfg.shadow.cascadeCount,
                    mCfg.shadow.resolution));
            g.AddPass("Shadows", NkPassType::NK_SHADOW)
             .Writes(shadowId)
             .Execute([this](NkICommandBuffer* cmd) {
                 mShadow->RenderShadowPasses(cmd);
             });
        }

        // Geometry pass (3D)
        g.AddPass("Geometry", NkPassType::NK_GEOMETRY)
         .Writes(hdrId, depthId)
         .ClearWith({0.05f,0.05f,0.07f,1.f})
         .Execute([this](NkICommandBuffer* cmd) {
             mRender3D->Flush(cmd);
         });

        // VFX pass
        g.AddPass("VFX", NkPassType::NK_TRANSPARENT)
         .Reads(depthId).Writes(hdrId)
         .Execute([this](NkICommandBuffer* cmd) {
             (void)cmd;
         });

        // Post-process
        if (mCfg.postProcess.ssao || mCfg.postProcess.bloom || mCfg.postProcess.toneMapping) {
            g.AddPass("PostProcess", NkPassType::NK_POST_PROCESS)
             .Reads(hdrId, depthId).Writes(colorId)
             .Execute([this, hdrId, depthId](NkICommandBuffer* cmd) {
                 (void)hdrId; (void)depthId; (void)cmd;
             });
        }

        // 2D + UI overlay
        g.AddPass("Overlay2D", NkPassType::NK_UI_OVERLAY)
         .Writes(colorId)
         .Execute([this](NkICommandBuffer* cmd) {
             mRender2D->FlushPending(cmd);
             mOverlay->FlushPending(cmd);
         });

        g.Compile();
    }

    // ── Frame ──────────────────────────────────────────────────────────────────
    bool NkRendererImpl::BeginFrame() {
        if (!mInitialized) return false;
        mStats.Reset();
        NkFrameContext ctx;
        if (!mDevice->BeginFrame(ctx)) return false;
        mFrameIndex = ctx.frameIndex;
        if (mSwapchain) mSwapchain->AcquireNextImage(NkSemaphoreHandle{});
        if (!mCmd) mCmd = mDevice->CreateCommandBuffer();
        return mCmd != nullptr;
    }

    void NkRendererImpl::EndFrame() {
        if (!mCmd) return;
        mRenderGraph->Execute(mCmd);
        mRenderGraph->Reset();
        NkFrameContext ctx;
        ctx.frameIndex = mFrameIndex;
        mDevice->EndFrame(ctx);
        mCmd = nullptr;
    }

    void NkRendererImpl::Present() {
        if (mSwapchain) mSwapchain->Present();
    }

    void NkRendererImpl::OnResize(uint32 w, uint32 h) {
        if (w == 0 || h == 0) return;
        mCfg.width = w; mCfg.height = h;
        if (mSwapchain) mSwapchain->Resize(w, h);
        mPostProcess->OnResize(w, h);
        BuildDefaultRenderGraph();
    }

    // ── Config dynamique ───────────────────────────────────────────────────────
    void NkRendererImpl::SetVSync(bool e) {
        mCfg.vsync = e;
        // NkISwapchain has no vsync setter; re-create swapchain if needed
    }

    void NkRendererImpl::SetPostConfig(const NkPostConfig& pp) {
        mCfg.postProcess = pp;
        mPostProcess->SetConfig(pp);
    }

    void NkRendererImpl::SetWireframe(bool e) {
        mCfg.wireframe = e;
        mRender3D->SetWireframe(e);
    }

    // ── Offscreen ─────────────────────────────────────────────────────────────
    NkOffscreenTarget* NkRendererImpl::CreateOffscreen(const NkOffscreenDesc& desc) {
        auto* t = new NkOffscreenTarget();
        if (!t->Init(mDevice, mTextures.Get(), desc)) {
            delete t; return nullptr;
        }
        mOffscreenTargets.PushBack(t);
        return t;
    }

    void NkRendererImpl::DestroyOffscreen(NkOffscreenTarget*& t) {
        if (!t) return;
        for (uint32 i = 0; i < mOffscreenTargets.Size(); i++) {
            if (mOffscreenTargets[i] == t) {
                t->Shutdown();
                delete t;
                mOffscreenTargets.Erase(mOffscreenTargets.Begin() + i);
                break;
            }
        }
        t = nullptr;
    }

} // namespace renderer
} // namespace nkentseu
