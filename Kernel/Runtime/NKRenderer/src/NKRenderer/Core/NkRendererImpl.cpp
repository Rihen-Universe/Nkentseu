// =============================================================================
// NkRendererImpl.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRendererImpl.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace renderer {

        // Helper : alloue via le NkAllocator par defaut (NkAllocator::New utilise
        // _aligned_malloc, donc NkUniquePtr::Reset peut faire _aligned_free
        // proprement). Eviter `new T()` qui passe par malloc et provoque une
        // corruption heap au moment du free.
        template<typename T, typename... Args>
        static inline T* AllocOwned(Args&&... args) {
            return memory::NkGetDefaultAllocator().New<T>(traits::NkForward<Args>(args)...);
        }

        // ── Fabrique statique ─────────────────────────────────────────────────────
        NkRenderer* NkRenderer::Create(NkIDevice* device, const NkRendererConfig& cfg) {
            auto* renderer = new NkRendererImpl(device, cfg);
            if (!renderer->Initialize()) {
                delete renderer;
                return nullptr;
            }
            return renderer;
        }

        void NkRenderer::Destroy(NkRenderer*& renderer) {
            if (renderer) {
                renderer->Shutdown();
                delete renderer;
                renderer = nullptr;
            }
        }

        // ── Constructor / Destructor ──────────────────────────────────────────────
        NkRendererImpl::NkRendererImpl(NkIDevice* device, const NkRendererConfig& cfg)
            : mDevice(device), mCfg(cfg) {}

        NkRendererImpl::~NkRendererImpl() {
            Shutdown();
        }

        // ── Initialize ────────────────────────────────────────────────────────────
        // Init conditionnelle selon mCfg.subsystems (NkSubsystemFlags).
        // Chaque sous-systeme n'est cree QUE si son flag est present.
        // Les dependances internes sont validees : par exemple TEXT necessite
        // RENDER2D ; UI necessite RENDER2D + TEXT ; OVERLAY idem ; SHADOW
        // necessite RENDER3D ; etc.
        bool NkRendererImpl::Initialize() {
            if (mInitialized) return true;
            if (!mDevice) {
                NkRSetLastError(NkRResult::NK_ERR_INVALID_DEVICE,
                                "NkRendererImpl::Initialize device==nullptr");
                return false;
            }
            logger.Info("[NkRendererImpl] Initialize start (api={0})\n", (int)mCfg.api);

            // 0. RHI (toujours requis)
            logger.Info("[NkRendererImpl]  step 0: InitRHI\n");
            if (!InitRHI()) return false;

            // 1. NkResources (toujours actif — default tex/samplers/layouts)
            logger.Info("[NkRendererImpl]  step 1: NkResources::Init\n");
            mResources.Reset(AllocOwned<NkResources>());
            if (!NkROk(mResources->Init(mDevice))) return false;

            // 2. NkShaderLibrary (toujours actif — compile et cache des shaders GLSL/HLSL/MSL)
            logger.Info("[NkRendererImpl]  step 2: NkShaderLibrary::Init\n");
            mShaders.Reset(AllocOwned<NkShaderLibrary>());
            if (!mShaders->Init(mDevice, mCfg.api, /*useNkSL=*/false)) {
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkShaderLibrary::Init failed");
                return false;
            }

            // 3. RenderGraph (toujours actif — orchestre les sous-systemes)
            logger.Info("[NkRendererImpl]  step 3: NkRenderGraph::ctor\n");
            mRenderGraph.Reset(AllocOwned<NkRenderGraph>(mDevice));

            // 4. Texture library (toujours actif — partage avec NkResources defaults)
            logger.Info("[NkRendererImpl]  step 4: NkTextureLibrary::Init\n");
            mTextures.Reset(AllocOwned<NkTextureLibrary>());
            if (!NkROk(mTextures->Init(mDevice, mResources.Get()))) return false;

            // 4. Mesh system (toujours actif — primitives utilisees par toutes les passes)
            logger.Info("[NkRendererImpl]  step 5: NkMeshSystem::Init\n");
            mMeshSystem.Reset(AllocOwned<NkMeshSystem>());
            if (!mMeshSystem->Init(mDevice)) return false;

            // 5. Material system (toujours actif — fournit les templates PBR/Unlit)
            logger.Info("[NkRendererImpl]  step 6: NkMaterialSystem::Init\n");
            mMaterials.Reset(AllocOwned<NkMaterialSystem>());
            if (!mMaterials->Init(mDevice, mTextures.Get(), mShaders.Get(), mCfg.api)) return false;

            // ─────────────────────────────────────────────────────────────────
            // Sous-systemes opt-in (NkSubsystemFlags) — declenchent les helpers
            // partages avec EnableSubsystem.
            // ─────────────────────────────────────────────────────────────────
            // Render2D (requis indirect par TEXT/UI/OVERLAY)
            const bool needsR2D = mCfg.Has(NK_SS_RENDER2D) || mCfg.Has(NK_SS_TEXT)
                               || mCfg.Has(NK_SS_UI)        || mCfg.Has(NK_SS_OVERLAY);
            if (needsR2D)                                       if (!InitRender2D())     return false;

            // Render3D (requis indirect par SHADOW/ANIMATION/SIMULATION)
            const bool needsR3D = mCfg.Has(NK_SS_RENDER3D) || mCfg.Has(NK_SS_SHADOW)
                               || mCfg.Has(NK_SS_ANIMATION)|| mCfg.Has(NK_SS_SIMULATION);
            // Shadow + Environment doivent etre init AVANT Render3D pour que ce dernier
            // puisse binder le ShadowUBO/atlas et les cubemaps IBL dans son frame set.
            if (needsR3D)                                       { logger.Info("[NkRendererImpl]  step 7: InitShadow\n");       if (!InitShadow())       return false; }
            if (needsR3D)                                       { logger.Info("[NkRendererImpl]  step 8: InitEnvironment\n");  if (!InitEnvironment())  return false; }
            if (needsR3D)                                       { logger.Info("[NkRendererImpl]  step 9: InitRender3D\n");     if (!InitRender3D())     return false; }
            if (mCfg.Has(NK_SS_TEXT))                           { logger.Info("[NkRendererImpl]  step10: InitTextRenderer\n"); if (!InitTextRenderer()) return false; }
            if (mCfg.Has(NK_SS_POST_PROCESS))                   { logger.Info("[NkRendererImpl]  step11: InitPostProcess\n");  if (!InitPostProcess())  return false; }
            if (mCfg.Has(NK_SS_OVERLAY))                        { logger.Info("[NkRendererImpl]  step12: InitOverlay\n");      if (!InitOverlay())      return false; }
            if (mCfg.Has(NK_SS_VFX))                            { logger.Info("[NkRendererImpl]  step13: InitVFX\n");          if (!InitVFX())          return false; }
            if (mCfg.Has(NK_SS_ANIMATION))                      { logger.Info("[NkRendererImpl]  step14: InitAnimation\n");    if (!InitAnimation())    return false; }
            if (mCfg.Has(NK_SS_SIMULATION))                     { logger.Info("[NkRendererImpl]  step15: InitSimulation\n");   if (!InitSimulation())   return false; }

            // Build initial render graph
            logger.Info("[NkRendererImpl]  step16: BuildDefaultRenderGraph\n");
            BuildDefaultRenderGraph();

            mInitialized = true;
            logger.Info("[NkRendererImpl] Initialize done\n");
            return true;
        }

        // =====================================================================
        // Helpers d'init par sous-systeme (idempotents : si deja alloue, no-op).
        // =====================================================================
        bool NkRendererImpl::InitShadow() {
            if (mShadow) return true;
            NkShadowSystemConfig sc;
            sc.resolution  = mCfg.shadow.resolution;
            sc.numCascades = mCfg.shadow.cascadeCount;
            sc.pcfMode     = mCfg.shadow.pcss
                              ? NkPCFMode::PCSS
                              : (mCfg.shadow.softShadows ? NkPCFMode::PCF3x3 : NkPCFMode::NONE);
            mShadow.Reset(AllocOwned<NkShadowSystem>());
            if (!mShadow->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(), sc)) {
                mShadow.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkShadowSystem::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitRender2D() {
            if (mRender2D) return true;
            mRender2D.Reset(AllocOwned<NkRender2D>());
            if (!mRender2D->Init(mDevice, mTextures.Get(), mShaders.Get())) {
                mRender2D.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkRender2D::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitRender3D() {
            if (mRender3D) return true;
            mRender3D.Reset(AllocOwned<NkRender3D>());
            if (!mRender3D->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(),
                                  mRenderGraph.Get(), mShadow.Get(),
                                  mEnvironment.Get(), mShaders.Get(),
                                  mResources.Get(),
                                  mCfg.framesInFlight)) {
                mRender3D.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkRender3D::Init failed");
                return false;
            }
            // Wire la connexion inverse : NkShadowSystem itere les opaques de
            // mRender3D dans sa passe shadow. Necessaire pour D.3b.
            if (mShadow) mShadow->SetRenderer3D(mRender3D.Get());
            return true;
        }
        bool NkRendererImpl::InitEnvironment() {
            if (mEnvironment) return true;
            mEnvironment.Reset(AllocOwned<NkEnvironmentSystem>());
            if (!mEnvironment->Init(mDevice)) {
                mEnvironment.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkEnvironmentSystem::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitTextRenderer() {
            if (mTextRenderer) return true;
            if (!mRender2D && !InitRender2D()) return false;     // dep
            mTextRenderer.Reset(AllocOwned<NkTextRenderer>());
            if (!mTextRenderer->Init(mDevice, mTextures.Get(), mRender2D.Get())) {
                mTextRenderer.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkTextRenderer::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitPostProcess() {
            if (mPostProcess) return true;
            mPostProcess.Reset(AllocOwned<NkPostProcessStack>());
            if (!mPostProcess->Init(mDevice, mTextures.Get(), mMeshSystem.Get(),
                                      mShaders.Get(), mResources.Get(),
                                      mCfg.width, mCfg.height)) {
                mPostProcess.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkPostProcessStack::Init failed");
                return false;
            }
            mPostProcess->SetConfig(mCfg.postProcess);
            return true;
        }
        bool NkRendererImpl::InitOverlay() {
            if (mOverlay) return true;
            if (!mRender2D     && !InitRender2D())     return false;
            if (!mTextRenderer && !InitTextRenderer()) return false;
            mOverlay.Reset(AllocOwned<NkOverlayRenderer>());
            if (!mOverlay->Init(mDevice, mRender2D.Get(), mTextRenderer.Get())) {
                mOverlay.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkOverlayRenderer::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitVFX() {
            if (mVFX) return true;
            mVFX.Reset(AllocOwned<NkVFXSystem>());
            if (!mVFX->Init(mDevice, mTextures.Get(), mMeshSystem.Get())) {
                mVFX.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkVFXSystem::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitAnimation() {
            if (mAnimation) return true;
            if (!mRender3D && !InitRender3D()) return false;
            mAnimation.Reset(AllocOwned<NkAnimationSystem>());
            if (!mAnimation->Init(mDevice, mRender3D.Get())) {
                mAnimation.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkAnimationSystem::Init failed");
                return false;
            }
            return true;
        }
        bool NkRendererImpl::InitSimulation() {
            if (mSimulation) return true;
            if (!mRender3D && !InitRender3D()) return false;
            if (!mVFX       && !InitVFX())       return false;
            mSimulation.Reset(AllocOwned<NkSimulationRenderer>());
            if (!mSimulation->Init(mDevice, mRender3D.Get(), mVFX.Get())) {
                mSimulation.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkSimulationRenderer::Init failed");
                return false;
            }
            return true;
        }

        // =====================================================================
        // Reconstruction du render graph (apres enable/disable runtime ou resize).
        // =====================================================================
        void NkRendererImpl::RebuildRenderGraph() {
            if (mRenderGraph) mRenderGraph->Reset();
            BuildDefaultRenderGraph();
        }

        // =====================================================================
        // EnableSubsystem / DisableSubsystem / IsSubsystemActive
        // =====================================================================
        bool NkRendererImpl::EnableSubsystem(NkSubsystemFlags flags) {
            if (!mInitialized) return false;
            bool any = false;
            // L'ordre respecte les dependances : Shadow + Environment AVANT Render3D
            // (Render3D bind le ShadowUBO et les cubemaps env dans son frame set au Init).
            if (NkHasFlag(flags, NK_SS_RENDER2D))     { if (!mRender2D)      any |= InitRender2D();     }
            if (NkHasFlag(flags, NK_SS_RENDER3D))     { if (!mShadow)        any |= InitShadow();       }
            if (NkHasFlag(flags, NK_SS_RENDER3D))     { if (!mEnvironment)   any |= InitEnvironment();  }
            if (NkHasFlag(flags, NK_SS_RENDER3D))     { if (!mRender3D)      any |= InitRender3D();     }
            if (NkHasFlag(flags, NK_SS_SHADOW))       { if (!mShadow)        any |= InitShadow();       }
            if (NkHasFlag(flags, NK_SS_TEXT))         { if (!mTextRenderer)  any |= InitTextRenderer(); }
            if (NkHasFlag(flags, NK_SS_POST_PROCESS)) { if (!mPostProcess)   any |= InitPostProcess();  }
            if (NkHasFlag(flags, NK_SS_OVERLAY))      { if (!mOverlay)       any |= InitOverlay();      }
            if (NkHasFlag(flags, NK_SS_VFX))          { if (!mVFX)           any |= InitVFX();          }
            if (NkHasFlag(flags, NK_SS_ANIMATION))    { if (!mAnimation)     any |= InitAnimation();    }
            if (NkHasFlag(flags, NK_SS_SIMULATION))   { if (!mSimulation)    any |= InitSimulation();   }

            // Update config flags pour refleter l'etat reel
            mCfg.Enable(flags);
            // Reconstruire le graph pour integrer les nouvelles passes
            if (any) RebuildRenderGraph();
            return any;
        }

        void NkRendererImpl::DisableSubsystem(NkSubsystemFlags flags) {
            if (!mInitialized) return;
            // L'ordre inverse pour respecter les dependances :
            //   SIMULATION/ANIMATION/OVERLAY consomment d'autres systemes en premier,
            //   puis on libere ces derniers.
            // Si on desactive RENDER2D, on libere d'abord OVERLAY/UI/TEXT qui en
            // dependent (cascade).
            if (NkHasFlag(flags, NK_SS_RENDER2D)) {
                flags = flags | NK_SS_TEXT | NK_SS_UI | NK_SS_OVERLAY;
            }
            if (NkHasFlag(flags, NK_SS_RENDER3D)) {
                flags = flags | NK_SS_SHADOW | NK_SS_ANIMATION | NK_SS_SIMULATION;
            }

            if (NkHasFlag(flags, NK_SS_SIMULATION))   mSimulation.Reset();
            if (NkHasFlag(flags, NK_SS_ANIMATION))    mAnimation.Reset();
            if (NkHasFlag(flags, NK_SS_VFX))          mVFX.Reset();
            if (NkHasFlag(flags, NK_SS_OVERLAY))      mOverlay.Reset();
            if (NkHasFlag(flags, NK_SS_POST_PROCESS)) mPostProcess.Reset();
            if (NkHasFlag(flags, NK_SS_TEXT))         mTextRenderer.Reset();
            if (NkHasFlag(flags, NK_SS_SHADOW))       mShadow.Reset();
            if (NkHasFlag(flags, NK_SS_RENDER3D))   { mRender3D.Reset(); mEnvironment.Reset(); }
            if (NkHasFlag(flags, NK_SS_RENDER2D))     mRender2D.Reset();

            mCfg.Disable(flags);
            RebuildRenderGraph();
        }

        bool NkRendererImpl::IsSubsystemActive(NkSubsystemFlags flags) const {
            // Tous les flags demandes doivent correspondre a un sous-systeme alloue.
            const NkSubsystemFlags active = const_cast<NkRendererImpl*>(this)->GetActiveSubsystems();
            return (static_cast<uint32>(active) & static_cast<uint32>(flags)) == static_cast<uint32>(flags);
        }

        NkSubsystemFlags NkRendererImpl::GetActiveSubsystems() const {
            uint32 a = 0;
            if (mRender2D)     a |= NK_SS_RENDER2D;
            if (mRender3D)     a |= NK_SS_RENDER3D;
            if (mTextRenderer) a |= NK_SS_TEXT;
            if (mShadow)       a |= NK_SS_SHADOW;
            if (mPostProcess)  a |= NK_SS_POST_PROCESS;
            if (mOverlay)      a |= NK_SS_OVERLAY;
            if (mVFX)          a |= NK_SS_VFX;
            if (mAnimation)    a |= NK_SS_ANIMATION;
            if (mSimulation)   a |= NK_SS_SIMULATION;
            return static_cast<NkSubsystemFlags>(a);
        }

        // ── Shutdown ──────────────────────────────────────────────────────────────
        void NkRendererImpl::Shutdown() {
            if (!mInitialized) return;

            // Detruire offscreens d'abord
            for (auto* t : mOffscreenTargets) {
                t->Shutdown();
                delete t;
            }
            mOffscreenTargets.Clear();

            // Inverse de l'ordre d'init (les Reset sur des NkUniquePtr nul sont no-op)
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
            mShaders.Reset();
            mResources.Reset();

            if (mCmd) mDevice->DestroyCommandBuffer(mCmd);

            mInitialized = false;
        }

        // ── RHI init ──────────────────────────────────────────────────────────────
        bool NkRendererImpl::InitRHI() {
            if (!mDevice->IsValid()) return false;
            uint32 w = mDevice->GetSwapchainWidth();
            uint32 h = mDevice->GetSwapchainHeight();
            if (w > 0) mCfg.width  = w;
            if (h > 0) mCfg.height = h;

            mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
            if (!mCmd || !mCmd->IsValid()) {
                logger.Errorf("[NkRenderer] CommandBuffer fail\n");
                return false;
            }

            return true;
        }

        // ── Build default render graph ─────────────────────────────────────────────
        // Construit un graphe de rendu opt-in en fonction des sous-systemes actifs.
        // Si l'utilisateur a desactive RENDER3D, on n'ajoute ni Shadow ni Geometry.
        // Si POST_PROCESS est off, on ecrit Geometry directement dans Swapchain.
        void NkRendererImpl::BuildDefaultRenderGraph() {
            auto& g = *mRenderGraph;
            const bool has3D       = (mRender3D.Get()    != nullptr);
            const bool has2D       = (mRender2D.Get()    != nullptr);
            const bool hasShadow   = (mShadow.Get()      != nullptr) && mCfg.shadow.enabled;
            // D.4b : on n'active le HDR transient que si NkPostProcessStack a au
            // moins un effet wire (tonemap pour l'instant). HasAnyEffect lit la
            // config courante et evite d'allouer un HDR target inutile.
            // const bool hasPP       = false;
            const bool hasPP       = (mPostProcess.Get() != nullptr) && mPostProcess->HasAnyEffect();
            const bool hasVFX      = (mVFX.Get()         != nullptr);
            const bool hasOverlay  = (mOverlay.Get()     != nullptr);

            // Swapchain (toujours imported — c'est l'output final de la frame)
            auto colorId = g.ImportTexture("Swapchain", NkTextureHandle{}, NkResourceState::NK_PRESENT);

            // Cible 3D : si POST_PROCESS active → HDR transient ; sinon ecrit directement dans Swapchain.
            NkGraphResId mainColor = colorId;
            NkGraphResId mainDepth = NK_INVALID_RES_ID;
            if (has3D) {
                mainDepth = g.CreateTransient("MainDepth", NkTextureDesc::DepthStencil(mCfg.width, mCfg.height));
                if (hasPP) {
                    mainColor = g.CreateTransient("HDR", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA16_FLOAT));
                }
            }

            // ── Shadow pass ──────────────────────────────────────────────────
            // D.3b : NkShadowSystem possede son propre atlas + son propre FBO et
            // gere son BeginRenderPass / EndRenderPass en interne. Cette passe
            // est juste un point d'ordonnancement — le RenderGraph ne touche pas
            // au RenderPass automatique (pas de SetColor/SetDepth declares).
            if (hasShadow) {
                g.AddPass("Shadows", NkPassType::NK_SHADOW)
                 .SetAlwaysExecute(true)   // outputs hors-graph (FBO interne au ShadowSystem)
                 .Execute([this](NkICommandBuffer* cmd) {
                     mShadow->RenderShadowPasses(cmd);
                 });
            }

            // ── Geometry pass (3D opaque) ─────────────────────────────────────
            if (has3D) {
                auto& geom = g.AddPass("Geometry", NkPassType::NK_GEOMETRY);
                geom.SetColor(0, mainColor, NkLoadOp::NK_CLEAR, {0.05f, 0.05f, 0.07f, 1.f})
                    .SetDepth(mainDepth, NkLoadOp::NK_CLEAR, 1.f);
                // shadowId n'est plus dans le graph (NkShadowSystem gere son atlas
                // hors-graph). Le sequencing Shadows->Geometry est garanti par
                // l'ordre d'AddPass et le bind du shadow atlas se fait via le
                // descriptor set frame de Render3D (set au Init).
                geom.Execute([this](NkICommandBuffer* cmd) { mRender3D->Flush(cmd); });
            }

            // ── VFX pass (transparents) ───────────────────────────────────────
            if (has3D && hasVFX) {
                g.AddPass("VFX", NkPassType::NK_TRANSPARENT)
                 .Reads(mainDepth)
                 .SetColor(0, mainColor, NkLoadOp::NK_LOAD)
                 .Execute([this](NkICommandBuffer* cmd) {
                     // VFX flush integre par le sous-systeme VFX
                     (void)cmd;
                 });
            }

            // ── Post-process ──────────────────────────────────────────────────
            if (has3D && hasPP) {
                auto& pp = g.AddPass("PostProcess", NkPassType::NK_POST_PROCESS);
                pp.Reads(mainColor);
                if (mainDepth != NK_INVALID_RES_ID) pp.Reads(mainDepth);
                pp.SetColor(0, colorId, NkLoadOp::NK_CLEAR, {0,0,0,1});
                NkGraphResId hdrColorId = mainColor;   // capture by value
                pp.Execute([this, hdrColorId](NkICommandBuffer* cmd) {
                    // Resoud le handle RHI du transient HDR et passe-le au PostProcess
                    // qui dessine le tonemap fullscreen vers le swapchain (FBO 0,
                    // bindé par BeginRenderPass de cette passe car colorId=swapchain).
                    NkTextureHandle hdr = mRenderGraph->GetResourceTexture(hdrColorId);
                    if (mPostProcess && hdr.IsValid()) {
                        mPostProcess->ExecuteRHI(cmd, hdr);
                    }
                });
            }

            // ── DEBUG : pass dediee dessin direct triangle (DESACTIVEE) ────────
            // Etait utilisee pour isoler le bug PBR Vulkan. RenderDoc a confirme
            // que le PBR fonctionne. On garde le code en place pour tests futurs.
            if (false /* has3D */) {
                auto& dbg = g.AddPass("DebugDirect", NkPassType::NK_UI_OVERLAY);
                dbg.SetColor(0, colorId, NkLoadOp::NK_LOAD, {0.05f, 0.05f, 0.07f, 1.f});
                dbg.Execute([this](NkICommandBuffer* cmd) {
                    if (mRender3D) mRender3D->DebugDrawDirectSwapchain(cmd);
                });
            }

            // ── 2D + UI overlay ───────────────────────────────────────────────
            // Si aucune passe 3D ne clear le swapchain (config 2D-only), on clear ici.
            if (has2D || hasOverlay) {
                auto& ov = g.AddPass("Overlay2D", NkPassType::NK_UI_OVERLAY);
                const auto loadOp = has3D ? NkLoadOp::NK_LOAD : NkLoadOp::NK_CLEAR;
                ov.SetColor(0, colorId, loadOp, {0.05f, 0.05f, 0.07f, 1.f});
                ov.Execute([this](NkICommandBuffer* cmd) {
                    if (mRender2D) mRender2D->FlushPending(cmd);
                    if (mOverlay)  mOverlay->FlushPending(cmd);
                });
            }

            g.Compile();
        }

        // ── Frame ──────────────────────────────────────────────────────────────────
        bool NkRendererImpl::BeginFrame() {
            if (!mInitialized) return false;
            mStats.Reset();
            mFrameCtx = {};
            if (!mDevice->BeginFrame(mFrameCtx)) return false;

            // Auto-resize
            uint32 sw=mDevice->GetSwapchainWidth(), sh=mDevice->GetSwapchainHeight();
            if ((sw!=mCfg.width||sh!=mCfg.height)&&sw>0&&sh>0) OnResize(sw,sh);

            // Flush dirty shader compilations before rendering
            if (mMaterials) mMaterials->FlushCompilations();

            // Hot-reload des shaders user-overrides (throttle ~1x/sec a 60fps).
            // PollHotReload est no-op si aucun NkShaderProgram n'a vertPath/fragPath
            // renseignes (= aucun fichier override actif), donc cout negligeable.
            if (mShaders && (mFrameCounter % 60) == 0) mShaders->PollHotReload();
            mFrameCounter++;

            mCmd->Reset();
            mCmd->Begin();
            return true;
        }

        void NkRendererImpl::EndFrame() {
            mDevice->EndFrame(mFrameCtx);
        }

        void NkRendererImpl::Present() {
            if (!mCmd) return;
            mRenderGraph->Execute(mCmd);
            // NB : pas de Reset() ici — le graph persiste entre frames.
            // RebuildRenderGraph() le reset+rebuilds quand les sous-systemes
            // changent (Enable/Disable). Le destructor du graph fait le clean final.
            mCmd->End();
            mDevice->SubmitAndPresent(mCmd);
        }

        void NkRendererImpl::OnResize(uint32 w, uint32 h) {
            if (w == 0 || h == 0) return;
            mCfg.width = w; mCfg.height = h;
            // Propage au RHI pour mettre a jour la swapchain virtuelle (viewport / FBO 0).
            if (mDevice) mDevice->OnResize(w, h);
            // Propage a tous les sous-systemes optionnels (selon la config courante).
            // For2D ne cree pas mPostProcess/mRender3D/mShadow, donc null check avant.
            if (mRender2D)     mRender2D->OnResize(w, h);
            if (mRender3D)     mRender3D->OnResize(w, h);
            if (mOverlay)      mOverlay->OnResize(w, h);
            if (mPostProcess)  mPostProcess->OnResize(w, h);
            // Reset + rebuild du RenderGraph : les ressources transitoires (HDR target,
            // depth buffer) sont dimensionnees via mCfg.width/height au moment du build,
            // donc on doit les recreer apres un changement de taille. RebuildRenderGraph()
            // appelle Reset() puis BuildDefaultRenderGraph() — ne PAS appeler le second
            // tout seul (ca empile des passes au lieu de les remplacer).
            RebuildRenderGraph();
        }

        // ── Config dynamique ───────────────────────────────────────────────────────
        void NkRendererImpl::SetVSync(bool e) {
            mCfg.vsync = e;
        }

        void NkRendererImpl::SetPostConfig(const NkPostConfig& pp) {
            mCfg.postProcess = pp;
            if (mPostProcess) mPostProcess->SetConfig(pp);
        }

        void NkRendererImpl::SetWireframe(bool e) {
            mCfg.wireframe = e;
            if (mRender3D) mRender3D->SetWireframe(e);
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
                    mOffscreenTargets.RemoveAt(i);
                    break;
                }
            }
            t = nullptr;
        }

    } // namespace renderer
} // namespace nkentseu