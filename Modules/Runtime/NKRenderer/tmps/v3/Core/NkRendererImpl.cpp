// =============================================================================
// NkRendererImpl.cpp
// Implémentation de la façade NkRenderer.
// =============================================================================
#include "NkRendererImpl.h"
#include "NKLogger/NkLog.h"
#include <memory>

namespace nkentseu {

    // ── Factory ───────────────────────────────────────────────────────────────────

    NkRenderer* NkRenderer::Create(NkIDevice* device, const NkRendererConfig& cfg) {
        auto* r = new NkRendererImpl(device, cfg);
        if (!r->Initialize()) {
            logger.Errorf("[NkRenderer] Initialization failed\n");
            delete r;
            return nullptr;
        }
        return r;
    }

    void NkRenderer::Destroy(NkRenderer*& renderer) {
        if (!renderer) return;
        renderer->Shutdown();
        delete renderer;
        renderer = nullptr;
    }

    // ── NkRendererImpl ────────────────────────────────────────────────────────────

    NkRendererImpl::NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg)
        : mDevice(device), mCfg(cfg) {
        mWidth  = cfg.width  > 0 ? cfg.width  : device->GetSwapchainWidth();
        mHeight = cfg.height > 0 ? cfg.height : device->GetSwapchainHeight();
    }

    NkRendererImpl::~NkRendererImpl() {
        if (mValid) Shutdown();
    }

    bool NkRendererImpl::Initialize() {
        logger.Infof("[NkRenderer] Initializing (%ux%u, %s)\n",
                     mWidth, mHeight,
                     mCfg.renderMode == NkRenderMode::NK_FORWARD ? "Forward" :
                     mCfg.renderMode == NkRenderMode::NK_DEFERRED ? "Deferred" : "Forward+");

        // Ordre critique : Textures → Meshes → Materials → Passes → Tools
        if (!InitTextures())     { logger.Errorf("[NkRenderer] InitTextures failed\n");     return false; }
        if (!InitMeshes())       { logger.Errorf("[NkRenderer] InitMeshes failed\n");       return false; }
        if (!InitRenderPasses()) { logger.Errorf("[NkRenderer] InitRenderPasses failed\n"); return false; }
        if (!InitMaterials())    { logger.Errorf("[NkRenderer] InitMaterials failed\n");    return false; }
        if (!InitRender2D())     { logger.Errorf("[NkRenderer] InitRender2D failed\n");     return false; }
        if (!InitRender3D())     { logger.Errorf("[NkRenderer] InitRender3D failed\n");     return false; }
        if (!InitHDRTarget())    { logger.Errorf("[NkRenderer] InitHDRTarget failed\n");    return false; }
        if (!InitPostProcess())  { logger.Errorf("[NkRenderer] InitPostProcess failed\n");  return false; }
        if (!InitOverlay())      { logger.Errorf("[NkRenderer] InitOverlay failed\n");      return false; }
        if (!InitGraph())        { logger.Errorf("[NkRenderer] InitGraph failed\n");        return false; }

        mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
        if (!mCmd || !mCmd->IsValid()) {
            logger.Errorf("[NkRenderer] Failed to create command buffer\n");
            return false;
        }

        mValid = true;
        logger.Infof("[NkRenderer] Ready\n");
        return true;
    }

    void NkRendererImpl::Shutdown() {
        if (!mValid) return;
        mDevice->WaitIdle();

        // Nettoyage dans l'ordre inverse
        DestroyOffscreenTargets();
        DestroyInternalTargets();

        if (mOverlay)      mOverlay->Shutdown();
        if (mPostProcess)  mPostProcess->Shutdown();
        if (mRender3D)     mRender3D->Shutdown();
        if (mRender2D)     mRender2D->Shutdown();
        if (mMaterials)    mMaterials->Shutdown();
        if (mMeshes)       mMeshes->Shutdown();
        if (mTextures)     mTextures->Shutdown();

        if (mCmd) { mDevice->DestroyCommandBuffer(mCmd); }

        mOverlay.reset();
        mPostProcess.reset();
        mRender3D.reset();
        mRender2D.reset();
        mGraph.reset();
        mMaterials.reset();
        mMeshes.reset();
        mTextures.reset();

        mValid = false;
        logger.Infof("[NkRenderer] Shutdown complete\n");
    }

    bool NkRendererImpl::IsValid() const { return mValid; }

    // ── Frame ─────────────────────────────────────────────────────────────────────

    bool NkRendererImpl::BeginFrame() {
        if (!mValid) return false;

        // Synchronisation swapchain
        if (!mDevice->BeginFrame(mFrameCtx)) return false;
        mFrameIndex = mFrameCtx.frameIndex;

        // Resize dynamique si le swapchain a changé
        uint32 swW = mDevice->GetSwapchainWidth();
        uint32 swH = mDevice->GetSwapchainHeight();
        if ((swW != mWidth || swH != mHeight) && swW > 0 && swH > 0) {
            OnResize(swW, swH);
        }

        mCmd->Reset();
        mCmd->Begin();

        // Flush les matériaux dirty avant le rendu
        mMaterials->FlushDirty();

        mInsideFrame = true;
        return true;
    }

    void NkRendererImpl::EndFrame() {
        if (!mValid || !mInsideFrame) return;
        RecordFrame();
        mCmd->End();
        mDevice->SubmitAndPresent(mCmd);
        mDevice->EndFrame(mFrameCtx);
        mInsideFrame = false;
        mFrameIndex  = (mFrameIndex + 1) % mDevice->GetMaxFramesInFlight();
    }

    void NkRendererImpl::Present() {
        // Déjà géré dans EndFrame via SubmitAndPresent.
        // Exposé pour les cas où BeginFrame/EndFrame/Present sont séparés.
    }

    // ── Orchestration de la frame ─────────────────────────────────────────────────

    void NkRendererImpl::RecordFrame() {
        // 1. Passes de shadow (CSM)
        RecordShadowPasses();

        // 2. Geometry : forward ou G-buffer
        RecordGeometryPass();

        // 3. Lighting (deferred uniquement)
        if (mCfg.renderMode == NkRenderMode::NK_DEFERRED ||
            mCfg.renderMode == NkRenderMode::NK_TILED_DEFERRED) {
            RecordLightingPass();
        }

        // 4. Post-process (bloom, SSAO, tone map...)
        RecordPostProcessPass();

        // 5. Overlay (debug, UI, gizmos)
        RecordOverlayPass();

        // 6. Blit final → swapchain
        RecordFinalBlit();

        // Reset les listes de draw pour la prochaine frame
        mRender3D->Reset();
        ResetStats();
    }

    void NkRendererImpl::RecordShadowPasses() {
        if (!mCfg.shadow.enabled) return;
        mRender3D->SubmitShadowPasses(mCmd);
    }

    void NkRendererImpl::RecordGeometryPass() {
        // Commence la render pass HDR
        if (mHDRTarget) {
            mHDRTarget->Begin(mCmd, NkLoadOp::NK_CLEAR, NkLoadOp::NK_CLEAR);
            mCmd->SetViewport(mHDRTarget->GetFullViewport());
            mCmd->SetScissor(mHDRTarget->GetFullRect());
        } else {
            // Fallback sur swapchain direct
            NkRenderPassHandle rp = mDevice->GetSwapchainRenderPass();
            NkFramebufferHandle fb = mDevice->GetSwapchainFramebuffer();
            NkRect2D area{0,0,(int32)mWidth,(int32)mHeight};
            mCmd->BeginRenderPass(rp, fb, area);
        }

        // Rendu 3D opaque
        mRender3D->SubmitGeometryPass(mCmd);

        // Rendu 2D (scène 2D dans le monde 3D, ex: sprites)
        mRender2D->SubmitBatch(mCmd);

        if (mHDRTarget) mHDRTarget->End(mCmd);
        else            mCmd->EndRenderPass();
    }

    void NkRendererImpl::RecordLightingPass() {
        // Passe de lighting deferred (lire G-buffer, écrire HDR)
        // Implémentation complète dans NkDeferredLightingPass (séparé)
        // TODO: déléguer à NkDeferredLightingPass
    }

    void NkRendererImpl::RecordPostProcessPass() {
        if (!mHDRTarget) return;
        NkTextureHandle hdrColor = mHDRTarget->GetColorHandle(0);
        NkTextureHandle depth    = mHDRTarget->GetDepthHandle();
        mPostProcess->Execute(mCmd, hdrColor, depth,
                               {},  // normal (null pour forward)
                               {},  // motion vectors
                               mFrameIndex);
    }

    void NkRendererImpl::RecordOverlayPass() {
        if (!mOverlay) return;

        // Commence la passe overlay sur le swapchain directement
        NkRenderPassHandle rp  = mDevice->GetSwapchainRenderPass();
        NkFramebufferHandle fb = mDevice->GetSwapchainFramebuffer();
        NkRect2D area{0,0,(int32)mWidth,(int32)mHeight};

        mCmd->BeginRenderPass(rp, fb, area);
        mCmd->SetViewport({0.f,0.f,(float32)mWidth,(float32)mHeight});
        mCmd->SetScissor(area);

        mOverlay->Submit(mCmd);

        mCmd->EndRenderPass();
    }

    void NkRendererImpl::RecordFinalBlit() {
        // Le blit final (postprocess → swapchain) est géré par NkPostProcessStack
        // qui écrit directement dans la cible finale.
        // Si on n'a pas de post-process, on a déjà rendu sur le swapchain.
    }

    // ── Resize ────────────────────────────────────────────────────────────────────

    void NkRendererImpl::OnResize(uint32 width, uint32 height) {
        if (width == 0 || height == 0) return;
        mWidth  = width;
        mHeight = height;

        mDevice->OnResize(width, height);

        if (mHDRTarget)   mHDRTarget->Resize(width, height);
        if (mGBuffer)     mGBuffer->Resize(width, height);
        if (mRender2D)    mRender2D->OnResize(width, height);
        if (mRender3D)    mRender3D->OnResize(width, height);
        if (mPostProcess) mPostProcess->OnResize(width, height);
        if (mOverlay)     mOverlay->OnResize(width, height);

        logger.Infof("[NkRenderer] Resized to %ux%u\n", width, height);
    }

    // ── Offscreen ─────────────────────────────────────────────────────────────────

    NkOffscreenTarget* NkRendererImpl::CreateOffscreenTarget(const NkOffscreenDesc& desc) {
        auto* t = new NkOffscreenTarget(mDevice, mTextures.get(), desc);
        if (!t->Initialize()) {
            delete t;
            return nullptr;
        }
        mOffscreenTargets.PushBack(t);
        return t;
    }

    void NkRendererImpl::DestroyOffscreenTarget(NkOffscreenTarget*& target) {
        if (!target) return;
        for (usize i = 0; i < mOffscreenTargets.Size(); ++i) {
            if (mOffscreenTargets[i] == target) {
                mOffscreenTargets.Erase(mOffscreenTargets.Begin() + i);
                break;
            }
        }
        target->Shutdown();
        delete target;
        target = nullptr;
    }

    // ── Config dynamique ──────────────────────────────────────────────────────────

    void NkRendererImpl::SetVSync(bool enabled) {
        mCfg.vsync = enabled;
        // TODO: propager au swapchain
    }

    void NkRendererImpl::SetMSAA(NkSampleCount msaa) {
        if (mCfg.msaa == msaa) return;
        mCfg.msaa = msaa;
        // Requiert recreation des render passes et pipelines
        mDevice->WaitIdle();
        // ... reconstruction (simplifié)
    }

    void NkRendererImpl::SetHDR(bool enabled) {
        mCfg.hdr = enabled;
        // TODO: recréer les framebuffers HDR
    }

    void NkRendererImpl::SetShadowQuality(uint32 quality) {
        static const uint32 kResolutions[] = {0, 512, 1024, 2048, 4096};
        if (quality >= 5) quality = 4;
        mCfg.shadow.enabled    = (quality > 0);
        mCfg.shadow.resolution = kResolutions[quality];
        mCfg.shadow.softShadows= (quality >= 3);
    }

    // ── Init helpers ──────────────────────────────────────────────────────────────

    bool NkRendererImpl::InitTextures() {
        mTextures = std::make_unique<NkTextureLibrary>(mDevice);
        return mTextures->Initialize();
    }

    bool NkRendererImpl::InitMeshes() {
        mMeshes = std::make_unique<NkMeshCache>(
            mDevice,
            mCfg.maxVertexBufferBytes,
            mCfg.maxIndexBufferBytes);
        return mMeshes->Initialize();
    }

    bool NkRendererImpl::InitRenderPasses() {
        // Forward pass
        NkRenderPassDesc forwardDesc = NkRenderPassDesc::Forward(
            mCfg.hdr ? NkGPUFormat::NK_RGBA16_FLOAT : NkGPUFormat::NK_RGBA8_SRGB,
            NkGPUFormat::NK_D32_FLOAT,
            mCfg.msaa);
        forwardDesc.debugName = "NkRenderer_ForwardPass";
        mForwardPass = mDevice->CreateRenderPass(forwardDesc);
        if (!mForwardPass.IsValid()) return false;

        // Deferred G-Buffer pass
        if (mCfg.renderMode == NkRenderMode::NK_DEFERRED ||
            mCfg.renderMode == NkRenderMode::NK_TILED_DEFERRED) {
            NkRenderPassDesc gDesc = NkRenderPassDesc::GBuffer();
            gDesc.debugName = "NkRenderer_GBufferPass";
            mDeferredPass = mDevice->CreateRenderPass(gDesc);
        }

        // Shadow pass (depth only)
        NkRenderPassDesc shadowDesc = NkRenderPassDesc::ShadowMap();
        shadowDesc.debugName = "NkRenderer_ShadowPass";
        mShadowPass = mDevice->CreateRenderPass(shadowDesc);
        if (!mShadowPass.IsValid()) return false;

        // On réutilise le forward pass pour l'overlay
        mOverlayPass = mForwardPass;
        mPostProcessPass = mForwardPass;

        return true;
    }

    bool NkRendererImpl::InitMaterials() {
        mMaterials = std::make_unique<NkMaterialSystem>(mDevice, mTextures.get());
        return mMaterials->Initialize(mForwardPass,
                                       mDeferredPass.IsValid() ? mDeferredPass : mForwardPass,
                                       mCfg.msaa);
    }

    bool NkRendererImpl::InitRender2D() {
        mRender2D = std::make_unique<NkRender2D>(mDevice, mTextures.get());
        return mRender2D->Initialize(mForwardPass, 131072);
    }

    bool NkRendererImpl::InitRender3D() {
        mRender3D = std::make_unique<NkRender3D>(mDevice, mMeshes.get(),
                                                   mMaterials.get(), mTextures.get());
        return mRender3D->Initialize(mCfg.shadow, mForwardPass, mShadowPass, mCfg.msaa);
    }

    bool NkRendererImpl::InitHDRTarget() {
        if (!mCfg.hdr) return true; // pas de HDR target, on rend direct sur swapchain

        NkOffscreenDesc desc;
        desc.width       = mWidth;
        desc.height      = mHeight;
        desc.colorCount  = 1;
        desc.colorFormats[0] = NkGPUFormat::NK_RGBA16_FLOAT;
        desc.hasDepth    = true;
        desc.depthFormat = NkGPUFormat::NK_D32_FLOAT;
        desc.msaa        = mCfg.msaa;
        desc.resizable   = true;
        desc.debugName   = "NkRenderer_HDR";
        mHDRTarget = CreateOffscreenTarget(desc);
        return (mHDRTarget != nullptr);
    }

    bool NkRendererImpl::InitPostProcess() {
        mPostProcess = std::make_unique<NkPostProcessStack>(mDevice, mTextures.get());
        if (!mPostProcess->Initialize(mWidth, mHeight,
                                       mCfg.hdr ? NkGPUFormat::NK_RGBA16_FLOAT
                                                 : NkGPUFormat::NK_RGBA8_SRGB)) {
            return false;
        }
        mPostProcess->SetConfig(mCfg.postProcess);
        return true;
    }

    bool NkRendererImpl::InitOverlay() {
        mOverlay = std::make_unique<NkOverlayRenderer>(mDevice,
                                                        mRender2D.get(),
                                                        mRender3D.get());
        return mOverlay->Initialize(mOverlayPass, mWidth, mHeight);
    }

    bool NkRendererImpl::InitGraph() {
        mGraph = std::make_unique<NkRenderGraph>(mDevice);
        return true;
    }

    void NkRendererImpl::DestroyInternalTargets() {
        if (mHDRTarget) {
            mHDRTarget->Shutdown();
            // On ne le détruit pas via DestroyOffscreenTarget car il est déjà dans la liste
        }
        if (mGBuffer) {
            mGBuffer->Shutdown();
        }
    }

    void NkRendererImpl::DestroyOffscreenTargets() {
        mDevice->WaitIdle();
        for (auto* t : mOffscreenTargets) {
            if (t) { t->Shutdown(); delete t; }
        }
        mOffscreenTargets.Clear();
        mHDRTarget   = nullptr;
        mGBuffer     = nullptr;
        mShadowAtlas = nullptr;
    }

} // namespace nkentseu

// =============================================================================
// NkRendererImpl.cpp
// =============================================================================
#include "NkRendererImpl.h"
#include "../Tools/Render2D/NkRender2D.h"
#include "../Tools/Render3D/NkRender3D.h"
#include "../Tools/PostProcess/NkPostProcessStack.h"
#include "../Tools/Overlay/NkOverlayRenderer.h"
#include "../Tools/Offscreen/NkOffscreenTarget.h"
#include "../Tools/Particles/NkParticleSystem.h"
#include "../Tools/VFX/NkVFXSystem.h"
#include "../Tools/Simulation/NkSimulationRenderer.h"
#include "../Tools/Animation/NkAnimationSystem.h"
#include "../Shader/NkShaderLibrary.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    // =========================================================================
    // Factory
    // =========================================================================
    NkRenderer* NkRenderer::Create(NkIDevice* device, const NkRendererConfig& cfg) {
        auto* r = new NkRendererImpl(device, cfg);
        if (!r->Initialize()) {
            logger.Errorf("[NkRenderer] Initialization failed\n");
            delete r;
            return nullptr;
        }
        return r;
    }

    void NkRenderer::Destroy(NkRenderer*& r) {
        if (!r) return;
        r->Shutdown();
        delete r;
        r = nullptr;
    }

    // =========================================================================
    // NkRendererImpl
    // =========================================================================
    NkRendererImpl::NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg)
        : mDevice(device), mCfg(cfg) {
        mWidth  = cfg.width  > 0 ? cfg.width  : device->GetSwapchainWidth();
        mHeight = cfg.height > 0 ? cfg.height : device->GetSwapchainHeight();
    }

    NkRendererImpl::~NkRendererImpl() { if(mValid) Shutdown(); }

    bool NkRendererImpl::Initialize() {
        logger.Infof("[NkRenderer] Init %s %ux%u API=%s\n",
            mCfg.renderMode==NkRenderMode::NK_FORWARD?"Forward":
            mCfg.renderMode==NkRenderMode::NK_DEFERRED?"Deferred":"Forward+",
            mWidth, mHeight, NkGraphicsApiName(mDevice->GetApi()));

        if (!InitShaderCompiler()) { logger.Errorf("[NkRenderer] ShaderCompiler fail\n"); return false; }
        if (!InitTextures())       { logger.Errorf("[NkRenderer] Textures fail\n");       return false; }
        if (!InitMeshes())         { logger.Errorf("[NkRenderer] Meshes fail\n");         return false; }
        if (!InitRenderPasses())   { logger.Errorf("[NkRenderer] RenderPasses fail\n");   return false; }
        if (!InitMaterials())      { logger.Errorf("[NkRenderer] Materials fail\n");      return false; }
        if (!InitHDRTarget())      { logger.Warnf ("[NkRenderer] HDRTarget fail (ok if no HDR)\n"); }
        if (!InitRender2D())       { logger.Errorf("[NkRenderer] Render2D fail\n");       return false; }
        if (!InitRender3D())       { logger.Errorf("[NkRenderer] Render3D fail\n");       return false; }
        if (!InitPostProcess())    { logger.Errorf("[NkRenderer] PostProcess fail\n");    return false; }
        if (!InitOverlay())        { logger.Warnf ("[NkRenderer] Overlay fail (non-fatal)\n"); }
        if (!InitParticles())      { logger.Warnf ("[NkRenderer] Particles fail (non-fatal)\n"); }
        if (!InitAnimation())      { logger.Warnf ("[NkRenderer] Animation fail (non-fatal)\n"); }

        mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
        if (!mCmd || !mCmd->IsValid()) {
            logger.Errorf("[NkRenderer] CommandBuffer fail\n");
            return false;
        }

        mValid = true;
        logger.Infof("[NkRenderer] Ready ✓\n");
        return true;
    }

    void NkRendererImpl::Shutdown() {
        if (!mValid) return;
        mDevice->WaitIdle();

        // Détruire les offscreen targets
        for (auto* t : mOffscreenTargets) { if(t){t->Shutdown();delete t;} }
        mOffscreenTargets.Clear();
        mHDRTarget = nullptr;

        if (mOverlay)      { mOverlay->Shutdown(); }
        if (mPostProcess)  { mPostProcess->Shutdown(); }
        if (mRender3D)     { mRender3D->Shutdown(); }
        if (mRender2D)     { mRender2D->Shutdown(); }
        if (mParticles)    { mParticles->Shutdown(); }
        if (mAnimation)    { mAnimation->Shutdown(); }
        if (mMaterials)    { mMaterials->Shutdown(); }
        if (mMeshSys)      { mMeshSys->Shutdown(); }
        if (mTextures)     { mTextures->Shutdown(); }

        if (mCmd) mDevice->DestroyCommandBuffer(mCmd);

        mOverlay.reset(); mPostProcess.reset(); mRender3D.reset(); mRender2D.reset();
        mParticles.reset(); mAnimation.reset(); mVFX.reset(); mSimulation.reset();
        mMaterials.reset(); mMeshSys.reset(); mTextures.reset(); mShaderCompiler.reset();

        mValid = false;
        logger.Infof("[NkRenderer] Shutdown ✓\n");
    }

    // =========================================================================
    // Frame
    // =========================================================================
    bool NkRendererImpl::BeginFrame() {
        if (!mValid) return false;
        if (!mDevice->BeginFrame(mFrameCtx)) return false;

        // Auto-resize
        uint32 sw=mDevice->GetSwapchainWidth(), sh=mDevice->GetSwapchainHeight();
        if ((sw!=mWidth||sh!=mHeight)&&sw>0&&sh>0) OnResize(sw,sh);

        // Flush matériaux dirty avant rendu
        if (mMaterials) mMaterials->FlushDirty();

        mCmd->Reset();
        mCmd->Begin();
        mInsideFrame = true;
        return true;
    }

    void NkRendererImpl::EndFrame() {
        if (!mValid || !mInsideFrame) return;
        RecordFrame();
        mCmd->End();
        mDevice->SubmitAndPresent(mCmd);
        mDevice->EndFrame(mFrameCtx);
        mInsideFrame = false;
    }

    void NkRendererImpl::Present() { /* géré dans EndFrame */ }

    void NkRendererImpl::RecordFrame() {
        RecordShadowPasses();
        RecordGeometryPass();
        RecordPostProcessPass();
        RecordOverlayPass();
        if (mRender3D) mRender3D->Reset();
    }

    void NkRendererImpl::RecordShadowPasses() {
        if (!mCfg.shadow.enabled || !mRender3D) return;
        mRender3D->SubmitShadowPasses(mCmd);
    }

    void NkRendererImpl::RecordGeometryPass() {
        NkRenderPassHandle rp = mHDRTarget ?
            mHDRTarget->GetRenderPass() : mDevice->GetSwapchainRenderPass();
        NkFramebufferHandle fb = mHDRTarget ?
            mHDRTarget->GetFramebuffer() : mDevice->GetSwapchainFramebuffer();
        NkRect2D area{0,0,(int32)mWidth,(int32)mHeight};

        if (mCmd->BeginRenderPass(rp, fb, area)) {
            NkViewport vp{0.f,0.f,(float32)mWidth,(float32)mHeight,0.f,1.f};
            mCmd->SetViewport(vp); mCmd->SetScissor(area);
            if (mRender3D) mRender3D->SubmitGeometryPass(mCmd);
            if (mRender2D) mRender2D->SubmitBatch(mCmd);
            if (mParticles) mParticles->Render(mCmd, mRender3D ? mRender3D->GetCamera() : NkCamera3D{});
            mCmd->EndRenderPass();
        }
    }

    void NkRendererImpl::RecordPostProcessPass() {
        if (!mHDRTarget || !mPostProcess) return;
        mPostProcess->Execute(mCmd,
            mHDRTarget->GetColorHandle(),
            mHDRTarget->GetDepthHandle(),
            {},{}, mFrameCtx.frameIndex);
    }

    void NkRendererImpl::RecordOverlayPass() {
        if (!mOverlay) return;
        NkRenderPassHandle rp=mDevice->GetSwapchainRenderPass();
        NkFramebufferHandle fb=mDevice->GetSwapchainFramebuffer();
        NkRect2D area{0,0,(int32)mWidth,(int32)mHeight};
        if(mCmd->BeginRenderPass(rp,fb,area)){
            mCmd->SetViewport({0.f,0.f,(float32)mWidth,(float32)mHeight});
            mCmd->SetScissor(area);
            mOverlay->Submit(mCmd);
            mCmd->EndRenderPass();
        }
    }

    // =========================================================================
    // Resize
    // =========================================================================
    void NkRendererImpl::OnResize(uint32 w, uint32 h) {
        if(!w||!h) return;
        mWidth=w; mHeight=h;
        mDevice->OnResize(w,h);
        if(mHDRTarget)   mHDRTarget->Resize(w,h);
        if(mRender2D)    mRender2D->OnResize(w,h);
        if(mRender3D)    mRender3D->OnResize(w,h);
        if(mPostProcess) mPostProcess->OnResize(w,h);
        if(mOverlay)     mOverlay->OnResize(w,h);
        logger.Infof("[NkRenderer] Resized → %ux%u\n",w,h);
    }

    void NkRendererImpl::SetShadowQuality(uint32 q) {
        static const uint32 kRes[]={0,512,1024,2048,4096};
        q=q<5?q:4;
        mCfg.shadow.enabled    =(q>0);
        mCfg.shadow.resolution =kRes[q];
        mCfg.shadow.softShadows=(q>=3);
    }

    // =========================================================================
    // Offscreen
    // =========================================================================
    NkOffscreenTarget* NkRendererImpl::CreateOffscreenTarget(const NkOffscreenDesc& desc) {
        auto* t=new NkOffscreenTarget(mDevice,mTextures.get(),desc);
        if(!t->Initialize()){delete t;return nullptr;}
        mOffscreenTargets.PushBack(t);
        return t;
    }

    void NkRendererImpl::DestroyOffscreenTarget(NkOffscreenTarget*& t){
        if(!t) return;
        for(usize i=0;i<mOffscreenTargets.Size();i++){
            if(mOffscreenTargets[i]==t){mOffscreenTargets.Erase(mOffscreenTargets.Begin()+i);break;}
        }
        t->Shutdown(); delete t; t=nullptr;
    }

    // =========================================================================
    // Init helpers
    // =========================================================================
    bool NkRendererImpl::InitShaderCompiler() {
        mShaderCompiler=std::make_unique<NkShaderCompiler>(mDevice);
        if(!mShaderCompiler->Initialize(mCfg.shaderCacheDir)) return false;
        // Enregistrer la bibliothèque GLSL built-in
        // NkShaderLibrary::RegisterAll(nullptr); // à implémenter quand NkShaderSystem est intégré
        return true;
    }

    bool NkRendererImpl::InitTextures() {
        mTextures=std::make_unique<NkTextureLibrary>(mDevice);
        return mTextures->Initialize();
    }

    bool NkRendererImpl::InitMeshes() {
        mMeshSys=std::make_unique<NkMeshSystem>(mDevice,nullptr,mTextures.get());
        return mMeshSys->Initialize(mCfg.maxVertexBufferBytes,mCfg.maxIndexBufferBytes);
    }

    bool NkRendererImpl::InitRenderPasses() {
        // Forward pass (HDR ou LDR selon config)
        NkGPUFormat colorFmt = mCfg.hdr ? NkGPUFormat::NK_RGBA16_FLOAT : NkGPUFormat::NK_RGBA8_SRGB;
        NkRenderPassDesc fpDesc = NkRenderPassDesc::Forward(colorFmt,NkGPUFormat::NK_D32_FLOAT,mCfg.msaa);
        fpDesc.debugName="ForwardPass";
        mForwardPass=mDevice->CreateRenderPass(fpDesc);
        if(!mForwardPass.IsValid()) return false;

        // Shadow pass
        NkRenderPassDesc spDesc=NkRenderPassDesc::ShadowMap();
        spDesc.debugName="ShadowPass";
        mShadowPass=mDevice->CreateRenderPass(spDesc);

        // Deferred (optionnel)
        if(mCfg.renderMode==NkRenderMode::NK_DEFERRED||mCfg.renderMode==NkRenderMode::NK_TILED_DEFERRED){
            NkRenderPassDesc gDesc=NkRenderPassDesc::GBuffer();
            gDesc.debugName="GBufferPass";
            mDeferredPass=mDevice->CreateRenderPass(gDesc);
        }

        mOverlayPass=mForwardPass;
        return true;
    }

    bool NkRendererImpl::InitMaterials() {
        mMaterials=std::make_unique<NkMaterialSystem>(mDevice,mTextures.get());
        return mMaterials->Initialize(mForwardPass,
            mDeferredPass.IsValid()?mDeferredPass:mForwardPass,mCfg.msaa);
    }

    bool NkRendererImpl::InitHDRTarget() {
        if(!mCfg.hdr) return true;
        NkOffscreenDesc d;
        d.width=mWidth; d.height=mHeight;
        d.colorFormats[0]=NkGPUFormat::NK_RGBA16_FLOAT;
        d.hasDepth=true; d.resizable=true; d.debugName="HDR";
        mHDRTarget=CreateOffscreenTarget(d);
        return mHDRTarget!=nullptr;
    }

    bool NkRendererImpl::InitRender2D() {
        mRender2D=std::make_unique<NkRender2D>(mDevice,mTextures.get());
        return mRender2D->Initialize(mForwardPass,131072);
    }

    bool NkRendererImpl::InitRender3D() {
        mRender3D=std::make_unique<NkRender3D>(mDevice,mMeshSys.get(),
                                                mMaterials.get(),mTextures.get());
        return mRender3D->Initialize(mCfg.shadow,mForwardPass,mShadowPass,mCfg.msaa);
    }

    bool NkRendererImpl::InitPostProcess() {
        mPostProcess=std::make_unique<NkPostProcessStack>(mDevice,mTextures.get());
        if(!mPostProcess->Initialize(mWidth,mHeight,
            mCfg.hdr?NkGPUFormat::NK_RGBA16_FLOAT:NkGPUFormat::NK_RGBA8_SRGB)) return false;
        mPostProcess->SetConfig(mCfg.postProcess);
        return true;
    }

    bool NkRendererImpl::InitOverlay() {
        mOverlay=std::make_unique<NkOverlayRenderer>(mDevice,mRender2D.get(),mRender3D.get());
        return mOverlay->Initialize(mOverlayPass,mWidth,mHeight);
    }

    bool NkRendererImpl::InitParticles() {
        mParticles=std::make_unique<NkParticleSystem>(mDevice,mTextures.get(),mMaterials.get());
        return mParticles->Initialize(mForwardPass);
    }

    bool NkRendererImpl::InitAnimation() {
        mAnimation=std::make_unique<NkAnimationSystem>(mDevice,mMeshSys.get());
        return mAnimation->Initialize();
    }

} // namespace nkentseu
