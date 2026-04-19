// =============================================================================
// NkRenderer.cpp — Façade principale du renderer NKRenderer
// Point d'entrée unique : Init / Shutdown / BeginFrame / EndFrame / RenderFrame
// =============================================================================
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Scene/NkScene.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKLogger/NkLog.h"
#include <chrono>

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        NkRenderer::NkRenderer()
            : mResources(new NkResourceManager())
            , mMaterials(new NkMaterialSystem())
            , mGraph    (new NkRenderGraph())
            , mRender2D (new NkRender2D())
            , mRender3D (new NkRender3D())
            , mPostProc (new NkPostProcessStack())
            , mOverlay  (new NkOverlayRenderer())
        {}

        NkRenderer::~NkRenderer() {
            Shutdown();
        }

        // =============================================================================
        bool NkRenderer::Init(NkIDevice* device, const NkRendererConfig& cfg) {
            if (!device || !device->IsValid()) {
                NK_ERROR("NkRenderer::Init — NkIDevice invalide");
                return false;
            }
            mDevice = device;
            mConfig = cfg;
            mWidth  = cfg.width;
            mHeight = cfg.height;

            // ── 1. ResourceManager ────────────────────────────────────────────────────
            if (!mResources->Init(device)) {
                NK_ERROR("NkRenderer: NkResourceManager::Init échoué");
                return false;
            }

            // ── 2. MaterialSystem ─────────────────────────────────────────────────────
            if (!mMaterials->Init(device, mResources.Get())) {
                NK_ERROR("NkRenderer: NkMaterialSystem::Init échoué");
                return false;
            }

            // ── 3. RenderGraph ────────────────────────────────────────────────────────
            if (!mGraph->Begin(device, mWidth, mHeight, 0)) {
                NK_ERROR("NkRenderer: NkRenderGraph::Begin échoué");
                return false;
            }

            // ── 4. Render2D ───────────────────────────────────────────────────────────
            NkRender2DConfig r2dCfg{};
            r2dCfg.maxVertices = cfg.render2DMaxVertices;
            r2dCfg.maxIndices  = cfg.render2DMaxIndices;
            if (!mRender2D->Init(device, mResources.Get(), {}, r2dCfg)) {
                NK_ERROR("NkRenderer: NkRender2D::Init échoué");
                return false;
            }

            // ── 5. Render3D ───────────────────────────────────────────────────────────
            if (!mRender3D->Init(device, mResources.Get(), mMaterials.Get(),
                                   mGraph.Get(), mWidth, mHeight)) {
                NK_ERROR("NkRenderer: NkRender3D::Init échoué");
                return false;
            }

            // ── 6. PostProcess ────────────────────────────────────────────────────────
            if (!mPostProc->Init(device, mResources.Get(), mWidth, mHeight)) {
                NK_ERROR("NkRenderer: NkPostProcessStack::Init échoué");
                // Non-fatal : on continue sans post-process
            }

            // ── 7. Overlay ────────────────────────────────────────────────────────────
            if (!mOverlay->Init(device, mResources.Get(), mRender2D.Get(),
                                  mWidth, mHeight)) {
                NK_ERROR("NkRenderer: NkOverlayRenderer::Init échoué");
            }

            // ── 8. HDR Render Target ──────────────────────────────────────────────────
            {
                mHDRColor = mResources->CreateRenderTarget(
                    mWidth, mHeight, NkPixelFormat::NK_RGBA16F,
                    cfg.msaa, "HDR_Color");
                mHDRDepth = mResources->CreateDepthTexture(
                    mWidth, mHeight, NkPixelFormat::NK_D32F,
                    cfg.msaa, "HDR_Depth");

                NkRenderTargetDesc rtDesc{};
                rtDesc.width = mWidth; rtDesc.height = mHeight;
                rtDesc.AddColor(mHDRColor);
                rtDesc.SetDepth(mHDRDepth);
                mHDRTarget = mResources->CreateRenderTarget(rtDesc);
            }

            // ── 9. Configurer le RenderGraph principal ────────────────────────────────
            BuildRenderGraph();

            mReady    = true;
            mFrameIdx = 0;
            NK_INFO("NkRenderer initialisé ({}×{}, MSAA={})", mWidth, mHeight, (int)cfg.msaa);
            return true;
        }

        // =============================================================================
        void NkRenderer::Shutdown() {
            if (!mDevice) return;

            mDevice->WaitIdle();

            // Libérer HDR targets
            if (mHDRTarget.IsValid()) mResources->ReleaseRenderTarget(mHDRTarget);
            if (mHDRColor.IsValid())  mResources->ReleaseTexture(mHDRColor);
            if (mHDRDepth.IsValid())  mResources->ReleaseTexture(mHDRDepth);
            mHDRTarget = {};
            mHDRColor  = {};
            mHDRDepth  = {};

            if (mOverlay)   mOverlay->Shutdown();
            if (mPostProc)  mPostProc->Shutdown();
            if (mRender3D)  mRender3D->Shutdown();
            if (mRender2D)  mRender2D->Shutdown();
            if (mGraph)     mGraph->Shutdown();
            if (mMaterials) mMaterials->Shutdown();
            if (mResources) mResources->Shutdown();

            mReady  = false;
            mDevice = nullptr;
        }

        // =============================================================================
        void NkRenderer::Resize(uint32 w, uint32 h) {
            if (w == mWidth && h == mHeight) return;
            mWidth = w; mHeight = h;

            // Recréer HDR target
            if (mDevice) mDevice->WaitIdle();
            if (mHDRTarget.IsValid()) mResources->ReleaseRenderTarget(mHDRTarget);
            if (mHDRColor.IsValid())  mResources->ReleaseTexture(mHDRColor);
            if (mHDRDepth.IsValid())  mResources->ReleaseTexture(mHDRDepth);

            mHDRColor = mResources->CreateRenderTarget(w, h, NkPixelFormat::NK_RGBA16F, mConfig.msaa, "HDR_Color");
            mHDRDepth = mResources->CreateDepthTexture(w, h, NkPixelFormat::NK_D32F,    mConfig.msaa, "HDR_Depth");
            NkRenderTargetDesc rtDesc{};
            rtDesc.width=w; rtDesc.height=h;
            rtDesc.AddColor(mHDRColor); rtDesc.SetDepth(mHDRDepth);
            mHDRTarget = mResources->CreateRenderTarget(rtDesc);

            if (mRender3D) mRender3D->Resize(w, h);
            if (mPostProc) mPostProc->Resize(w, h);
            if (mOverlay)  mOverlay->Resize(w, h);
        }

        // =============================================================================
        void NkRenderer::BuildRenderGraph() {
            // Passe 3D (opaque + shadows + transparent)
            mGraph->AddPass("Geometry3D")
                .Write(mHDRColor, "hdrColor", true, {0.1f,0.1f,0.1f,1.f})
                .SetDepth(mHDRDepth, true, 1.f)
                .SetCallback([this](NkRenderGraphCtx& ctx) {
                    // Le rendu 3D est déclenché manuellement via RenderFrame
                    // Ce callback est juste un placeholder pour le graph
                });

            // Passe PostProcess
            mGraph->AddPass("PostProcess")
                .Read(mHDRColor, "hdrColor")
                .WriteSwapchain()
                .After("Geometry3D")
                .SetCallback([this](NkRenderGraphCtx& ctx) {
                    // Également déclenché manuellement
                });

            mGraph->Compile();
        }

        // =============================================================================
        // BeginFrame / EndFrame
        // =============================================================================
        bool NkRenderer::BeginFrame(NkICommandBuffer* cmd) {
            if (!mReady || !cmd) return false;
            mCurrentCmd = cmd;
            mFrameTimer = std::chrono::high_resolution_clock::now();

            // Flush les updates matériaux en attente
            mMaterials->FlushPendingUpdates();

            // Réinitialiser le graph pour ce frame
            mGraph->Begin(mDevice, mWidth, mHeight, mFrameIdx);

            return true;
        }

        bool NkRenderer::EndFrame() {
            if (!mReady || !mCurrentCmd) return false;

            // Stats frame
            auto now = std::chrono::high_resolution_clock::now();
            float64 ms = std::chrono::duration<float64,std::milli>(now-mFrameTimer).count();
            mStats.cpuMs = (float32)ms;
            UpdateFPS((float32)(ms * 0.001));

            mCurrentCmd = nullptr;
            mFrameIdx++;
            return true;
        }

        // =============================================================================
        // RenderFrame — Pipeline complet 3D + post-process + 2D + overlay
        // =============================================================================
        void NkRenderer::RenderFrame(NkICommandBuffer* cmd,
                                      const NkRenderScene& scene,
                                      const NkPostProcessConfig* ppConfig)
        {
            if (!mReady || !cmd) return;

            // 1. Configurer post-process si fourni
            if (ppConfig && mPostProc) mPostProc->SetConfig(*ppConfig);

            // 2. Passe de rendu 3D → HDR target
            {
                cmd->BeginRenderTarget(mHDRTarget,
                    NkClearFlags::NK_COLOR | NkClearFlags::NK_DEPTH,
                    {0.05f,0.05f,0.08f,1.f}, 1.f, 0);
                cmd->SetViewport({0.f,0.f,(float32)mWidth,(float32)mHeight});
                cmd->SetScissor ({0,0,(int32)mWidth,(int32)mHeight});

                if (scene.hasCamera) {
                    NkSceneContext3D ctx3D{};
                    ctx3D.camera          = scene.camera;
                    ctx3D.lights          = scene.lights;
                    ctx3D.envMap          = scene.envMap;
                    ctx3D.ambientIntensity= scene.ambientIntensity;
                    ctx3D.time            = scene.time;
                    ctx3D.deltaTime       = scene.deltaTime;

                    mRender3D->BeginScene(ctx3D);
                    for (uint32 i = 0; i < (uint32)scene.drawCalls.Size(); ++i)
                        mRender3D->Submit(scene.drawCalls[i]);
                    mRender3D->EndScene(cmd);

                    // Copier stats 3D
                    const auto& s3 = mRender3D->GetStats();
                    mStats.drawCalls     += s3.drawCalls + s3.instancedCalls;
                    mStats.triangles     += s3.triangles;
                    mStats.culledObjects += s3.culledObjects;
                }

                cmd->EndRenderTarget();
            }

            // 3. Post-process sur le HDR target → résultat final
            NkTextureHandle finalTex = mHDRColor;
            if (mPostProc && mPostProc->IsReady()) {
                finalTex = mPostProc->Apply(cmd, mHDRColor, mHDRDepth,
                                              scene.time, scene.deltaTime);
            }

            // 4. Blit final vers swapchain
            cmd->BeginSwapchainRenderPass({0,0,0,1}, 1.f);
            cmd->SetViewport({0.f,0.f,(float32)mWidth,(float32)mHeight});
            cmd->SetScissor ({0,0,(int32)mWidth,(int32)mHeight});

            if (mPostProc) mPostProc->BlitToSwapchain(cmd, finalTex);

            // 5. Rendu 2D (HUD, UI) par-dessus
            // (L'utilisateur appelle GetRender2D()->Begin/End/Draw* ici depuis l'extérieur)

            // 6. Overlay de debug
            if (mConfig.enableDebugOverlay && mOverlay) {
                mStats.fps  = mCurrentFPS;
                mStats.cpuMs= mStats.cpuMs;
                mOverlay->BeginOverlay(cmd);
                mOverlay->PushRendererStats(mStats);
                mOverlay->EndOverlay();
            }

            cmd->EndSwapchainRenderPass();
        }

        // =============================================================================
        // Render2D autonome
        // =============================================================================
        void NkRenderer::Render2D(NkICommandBuffer* cmd,
                                    NkFunction<void(NkRender2D*)> drawFn,
                                    const NkCamera2D* camera)
        {
            if (!mReady || !cmd || !mRender2D) return;
            mRender2D->Begin(cmd, mWidth, mHeight, camera);
            if (drawFn) drawFn(mRender2D.Get());
            mRender2D->End();
        }

        // =============================================================================
        // FPS
        // =============================================================================
        void NkRenderer::UpdateFPS(float32 dt) {
            mFPSAccum += dt;
            mFPSFrames++;
            if (mFPSAccum >= 0.5f) {
                mCurrentFPS = (float32)mFPSFrames / mFPSAccum;
                mFPSAccum   = 0.f;
                mFPSFrames  = 0;
            }
        }

        // =============================================================================
        // Accesseurs
        // =============================================================================
        NkResourceManager* NkRenderer::GetResources() const { return mResources.Get(); }
        NkMaterialSystem*  NkRenderer::GetMaterials() const { return mMaterials.Get(); }
        NkRenderGraph*     NkRenderer::GetGraph()     const { return mGraph.Get(); }
        NkRender2D*        NkRenderer::Get2D()        const { return mRender2D.Get(); }
        NkRender3D*        NkRenderer::Get3D()        const { return mRender3D.Get(); }
        NkPostProcessStack* NkRenderer::GetPostProcess() const { return mPostProc.Get(); }
        NkOverlayRenderer* NkRenderer::GetOverlay()  const { return mOverlay.Get(); }

        const NkRenderer::Stats& NkRenderer::GetStats() const { return mStats; }

    } // namespace renderer
} // namespace nkentseu
