// =============================================================================
// NkRendererImpl.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRendererImpl.h"
#include "NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h"
#include "NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
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

            // 5a. Material parameter collection (Phase M.2 : UBO global partage).
            mMaterialCollection.Reset(AllocOwned<NkMaterialCollection>());
            if (!mMaterialCollection->Init(mDevice)) {
                logger.Warnf("[NkRendererImpl] NkMaterialCollection init failed (non bloquant)\n");
                mMaterialCollection.Reset();
            }

            // 5b. Material library (Phase G : .nkasset loader + hot-reload).
            // Sous-systeme bas niveau, toujours actif. ScanDirectory / Load /
            // EnableHotReload sont a la charge de l'application.
            mMaterialLibrary.Reset(AllocOwned<NkMaterialLibrary>());
            if (!mMaterialLibrary->Init(mDevice, mMaterials.Get(), mTextures.Get())) {
                logger.Warnf("[NkRendererImpl] NkMaterialLibrary init failed (non bloquant)\n");
                mMaterialLibrary.Reset();
            } else {
                mMaterials->SetLibrary(mMaterialLibrary.Get());
            }

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

            // Planar reflection system (auto). Init apres Render3D + Materials.
            // L'utilisateur enregistre des plans via GetPlanarReflection()->Register().
            if (needsR3D) {
                mPlanarReflection.Reset(AllocOwned<NkPlanarReflectionSystem>());
                if (!mPlanarReflection->Init(mDevice, mTextures.Get(), mMaterials.Get())) {
                    logger.Warnf("[NkRendererImpl] NkPlanarReflectionSystem init failed (non bloquant)\n");
                    mPlanarReflection.Reset();
                }
                // Phase M.2 : bind l'UBO collection aux global set rings de Render3D.
                if (mMaterialCollection)
                    mRender3D->SetMaterialCollection(mMaterialCollection.Get());

                // Phase H.6 : Voxel AO system. L'app enregistre les occluders
                // via GetVoxelAO()->RegisterOccluder() puis Build(). Le PBR
                // shader sample auto la texture 3D au binding=27.
                mVoxelAO.Reset(AllocOwned<NkVoxelAOSystem>());
                if (!mVoxelAO->Init(mDevice)) {
                    logger.Warnf("[NkRendererImpl] NkVoxelAOSystem init failed (non bloquant)\n");
                    mVoxelAO.Reset();
                }
                if (mVoxelAO && mRender3D)
                    mRender3D->SetVoxelAO(mVoxelAO.Get());
            }

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
            NkVirtualShadowMapsConfig sc;
            // Atlas size : on prefere 4096 par defaut pour pouvoir packer
            // plusieurs lights. Si la config NkShadowConfig demande une taille
            // plus petite (resolution per-tile), on la respecte mais sur l'atlas.
            sc.atlasSize       = (mCfg.shadow.resolution > 0)
                                  ? mCfg.shadow.resolution * 2 : 4096;
            sc.numCascades     = mCfg.shadow.cascadeCount > 0
                                  ? mCfg.shadow.cascadeCount : 3;
            sc.quality         = mCfg.shadow.pcss
                                  ? NkVSMShadowQuality::PCSS
                                  : (mCfg.shadow.softShadows
                                      ? NkVSMShadowQuality::PCF3x3
                                      : NkVSMShadowQuality::NONE);
            mShadow.Reset(AllocOwned<NkVirtualShadowMaps>());
            if (!mShadow->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(), sc,
                               mCfg.framesInFlight)) {
                mShadow.Reset();
                NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkVirtualShadowMaps::Init failed");
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
            mRender3D->SetIBLStrength(mCfg.ibl.iblStrength);
            // Phase N v0.5 : active la skybox HDR en background si l'app le
            // demande (recommande quand useHDR=true pour voir l'environnement
            // entier, pas juste ses reflets sur les objets).
            mRender3D->SetSkyboxEnabled(mCfg.ibl.drawSkybox);
            return true;
        }
        bool NkRendererImpl::InitEnvironment() {
            if (mEnvironment) return true;
            mEnvironment.Reset(AllocOwned<NkEnvironmentSystem>());

            // Phase N v0 : build NkEnvironmentConfig depuis mCfg.ibl (NkIBLConfig).
            // L'app peut customiser via cfg.ibl.useHDR + hdrPath OU les couleurs
            // procedurales skyTop/horizon/ground.
            NkEnvironmentConfig ecfg;
            ecfg.irradianceSize = mCfg.ibl.irradianceMapSize > 0 ? mCfg.ibl.irradianceMapSize : 32;
            ecfg.prefilterSize  = mCfg.ibl.specularMapSize   > 0 ? mCfg.ibl.specularMapSize   : 128;
            ecfg.prefilterMips  = mCfg.ibl.prefilterMipCount > 0 ? mCfg.ibl.prefilterMipCount : 5;
            ecfg.brdfLUTSize    = mCfg.ibl.brdfLUTSize       > 0 ? mCfg.ibl.brdfLUTSize       : 256;
            ecfg.source         = mCfg.ibl.useHDR
                                  ? NkEnvSource::NK_ENV_HDR_FILE
                                  : NkEnvSource::NK_ENV_PROCEDURAL;
            ecfg.hdrPath        = mCfg.ibl.hdrPath;
            ecfg.skyTop         = mCfg.ibl.skyTop;
            ecfg.horizon        = mCfg.ibl.horizon;
            ecfg.ground         = mCfg.ibl.ground;

            if (!mEnvironment->Init(mDevice, ecfg)) {
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

            for (auto* t : mOffscreenTargets) {
                t->Shutdown();
                delete t;
            }
            mOffscreenTargets.Clear();

            mVoxelAO.Reset();
            mPlanarReflection.Reset();
            mSimulation.Reset();
            mAnimation.Reset();
            mVFX.Reset();
            mOverlay.Reset();
            mPostProcess.Reset();
            mTextRenderer.Reset();
            mRender3D.Reset();
            mRender2D.Reset();
            mShadow.Reset();
            if (mMaterialLibrary) mMaterials->SetLibrary(nullptr);
            mMaterialLibrary.Reset();
            mMaterialCollection.Reset();
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
                     mShadow->RenderAllShadows(cmd);
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

            // ── Phase H.3 : SSAO (Screen Space Ambient Occlusion) ──────────────
            // Atténue l'ambient/IBL des zones occluses par geometrie proche
            // (objets sous le sol, dans les coins, sous une table, etc.).
            // Pass : Reads(mainDepth) -> SetColor(ssaoTex, R8_UNORM, W/2 x H/2).
            // Le tonemap multiplie HDR par le SSAO factor avant ACES.
            NkGraphResId ssaoTex        = NK_INVALID_RES_ID;
            NkGraphResId ssaoBlurredTex = NK_INVALID_RES_ID;
            const bool hasSSAO = has3D && hasPP && mPostProcess
                                 && mCfg.postProcess.ssao
                                 && mainDepth != NK_INVALID_RES_ID;
            if (hasSSAO) {
                uint32 sw = mCfg.width  / 2 ? mCfg.width  / 2 : 1;
                uint32 sh = mCfg.height / 2 ? mCfg.height / 2 : 1;
                ssaoTex = g.CreateTransient("SSAO",
                    NkTextureDesc::RenderTarget(sw, sh, NkGPUFormat::NK_R8_UNORM));

                auto& sp = g.AddPass("SSAO", NkPassType::NK_POST_PROCESS);
                sp.Reads(mainDepth);
                sp.SetColor(0, ssaoTex, NkLoadOp::NK_CLEAR, {1.f, 1.f, 1.f, 1.f});
                NkGraphResId depthId = mainDepth;
                sp.Execute([this, depthId, sw, sh](NkICommandBuffer* cmd) {
                    NkTextureHandle depthTex = mRenderGraph->GetResourceTexture(depthId);
                    if (mPostProcess && depthTex.IsValid()) {
                        mPostProcess->DrawSSAOPass(cmd, depthTex, sw, sh);
                    }
                });

                // Phase H.5b : pass blur denoise sur le ssaoTex noisy.
                // Le tonemap sample ssaoBlurredTex au lieu de ssaoTex.
                ssaoBlurredTex = g.CreateTransient("SSAO_Blurred",
                    NkTextureDesc::RenderTarget(sw, sh, NkGPUFormat::NK_R8_UNORM));
                auto& bp = g.AddPass("SSAO_Blur", NkPassType::NK_POST_PROCESS);
                bp.Reads(ssaoTex);
                bp.SetColor(0, ssaoBlurredTex, NkLoadOp::NK_CLEAR, {1.f, 1.f, 1.f, 1.f});
                NkGraphResId aoId = ssaoTex;
                bp.Execute([this, aoId, sw, sh](NkICommandBuffer* cmd) {
                    NkTextureHandle aoTex = mRenderGraph->GetResourceTexture(aoId);
                    if (mPostProcess && aoTex.IsValid()) {
                        mPostProcess->DrawSSAOBlurPass(cmd, aoTex, sw, sh);
                    }
                });
            }

            // ── Phase H.2 : Bloom Dual-Kawase multi-pass (Jorge Jimenez 2014) ──
            // Pyramide downsample (5 mips RGBA16F) + upsample additif + sample
            // dans le tonemap. State-of-the-art moderne (COD Advanced Warfare).
            // bloomMip[0] = W/2, bloomMip[5] = W/64. Le 1er downsample applique
            // un soft threshold (bright pass). Les upsamples blendent additif.
            constexpr int kBloomMipsRG = 6;
            NkGraphResId bloomMip[kBloomMipsRG];
            for (int i = 0; i < kBloomMipsRG; i++) bloomMip[i] = NK_INVALID_RES_ID;

            const bool hasBloom = has3D && hasPP && mPostProcess
                                  && mCfg.postProcess.bloom;
            if (hasBloom) {
                // Cree les transients pyramide.
                for (int i = 0; i < kBloomMipsRG; i++) {
                    uint32 div = 1u << (i + 1);   // mip 0 = W/2, mip 5 = W/64
                    uint32 bw = mCfg.width  / div ? mCfg.width  / div : 1;
                    uint32 bh = mCfg.height / div ? mCfg.height / div : 1;
                    char name[32];
                    snprintf(name, sizeof(name), "BloomMip%d", i);
                    bloomMip[i] = g.CreateTransient(name,
                        NkTextureDesc::RenderTarget(bw, bh, NkGPUFormat::NK_RGBA16_FLOAT));
                }

                // 6 passes downsample : extrait highlights + downsample x2 par mip.
                // Pass 0 : src = mainColor (HDR), threshold actif.
                // Pass 1..5 : src = bloomMip[i-1], threshold = 0 (passthrough).
                const float bloomThr = mCfg.postProcess.bloomThreshold;
                for (int i = 0; i < kBloomMipsRG; i++) {
                    char passName[32];
                    snprintf(passName, sizeof(passName), "Bloom_Down_%d", i);
                    auto& dp = g.AddPass(passName, NkPassType::NK_POST_PROCESS);
                    NkGraphResId src = (i == 0) ? mainColor : bloomMip[i-1];
                    dp.Reads(src);
                    dp.SetColor(0, bloomMip[i], NkLoadOp::NK_CLEAR, {0,0,0,1});
                    uint32 div = 1u << i;   // mip i source resolution = W/(2^i) avant downsample
                    uint32 srcW = (i == 0) ? mCfg.width  : (mCfg.width  / div ? mCfg.width  / div : 1);
                    uint32 srcH = (i == 0) ? mCfg.height : (mCfg.height / div ? mCfg.height / div : 1);
                    float thr = (i == 0) ? bloomThr : 0.0f;
                    dp.Execute([this, src, srcW, srcH, thr](NkICommandBuffer* cmd) {
                        NkTextureHandle srcTex = mRenderGraph->GetResourceTexture(src);
                        if (mPostProcess && srcTex.IsValid()) {
                            mPostProcess->DrawBloomDownPass(cmd, srcTex, srcW, srcH, thr);
                        }
                    });
                }

                // 5 passes upsample : tent filter 3x3 + blend additif sur la mip
                // courante (cible = mip plus grande, source = mip plus petite).
                // Ordre : Bloom_Up_4 (mip5->mip4), ..., Bloom_Up_0 (mip1->mip0).
                for (int i = kBloomMipsRG - 2; i >= 0; i--) {
                    char passName[32];
                    snprintf(passName, sizeof(passName), "Bloom_Up_%d", i);
                    auto& up = g.AddPass(passName, NkPassType::NK_POST_PROCESS);
                    up.Reads(bloomMip[i+1]);
                    // NK_LOAD pour preserver le downsample de la mip courante
                    // (la pass upsample blende additif par-dessus).
                    up.SetColor(0, bloomMip[i], NkLoadOp::NK_LOAD);
                    uint32 div = 1u << (i + 2);   // mip i+1 = W/(2^(i+2))
                    uint32 srcW = mCfg.width  / div ? mCfg.width  / div : 1;
                    uint32 srcH = mCfg.height / div ? mCfg.height / div : 1;
                    NkGraphResId src = bloomMip[i+1];
                    up.Execute([this, src, srcW, srcH](NkICommandBuffer* cmd) {
                        NkTextureHandle srcTex = mRenderGraph->GetResourceTexture(src);
                        if (mPostProcess && srcTex.IsValid()) {
                            mPostProcess->DrawBloomUpPass(cmd, srcTex, srcW, srcH, 1.0f);
                        }
                    });
                }
            }

            // ── Post-process ──────────────────────────────────────────────────
            if (has3D && hasPP) {
                // Phase L FXAA wirage : si FXAA actif, on split en 2 passes.
                //   Pass 1 PostProcess  -> ecrit dans transient ToneLDR
                //   Pass 2 FXAA_Final   -> lit ToneLDR, ecrit dans colorId
                // Le RG track auto les state transitions des transients
                // (contraire aux ImportTexture). Transient sera GC apres le draw.
                const bool fxaaOn = mPostProcess && mPostProcess->IsFXAAEnabled();
                NkGraphResId postTargetId = colorId;
                NkGraphResId toneTexId    = NK_INVALID_RES_ID;
                if (fxaaOn) {
                    auto tdesc = NkTextureDesc::RenderTarget(mCfg.width, mCfg.height,
                                                              NkGPUFormat::NK_RGBA8_UNORM);
                    tdesc.debugName = "ToneLDR_Transient";
                    toneTexId = g.CreateTransient("ToneLDR", tdesc);
                    if (toneTexId != NK_INVALID_RES_ID) {
                        postTargetId = toneTexId;
                    }
                }

                auto& pp = g.AddPass("PostProcess", NkPassType::NK_POST_PROCESS);
                pp.Reads(mainColor);
                if (mainDepth != NK_INVALID_RES_ID) pp.Reads(mainDepth);
                if (hasBloom && bloomMip[0] != NK_INVALID_RES_ID) pp.Reads(bloomMip[0]);
                if (hasSSAO && ssaoBlurredTex != NK_INVALID_RES_ID) pp.Reads(ssaoBlurredTex);
                pp.SetColor(0, postTargetId, NkLoadOp::NK_CLEAR, {0,0,0,1});
                NkGraphResId hdrColorId   = mainColor;   // capture by value
                NkGraphResId bloomColorId = hasBloom ? bloomMip[0]    : NK_INVALID_RES_ID;
                NkGraphResId ssaoColorId  = hasSSAO  ? ssaoBlurredTex : NK_INVALID_RES_ID;
                pp.Execute([this, hdrColorId, bloomColorId, ssaoColorId](NkICommandBuffer* cmd) {
                    NkTextureHandle hdr   = mRenderGraph->GetResourceTexture(hdrColorId);
                    NkTextureHandle bloom = (bloomColorId != NK_INVALID_RES_ID)
                                           ? mRenderGraph->GetResourceTexture(bloomColorId)
                                           : NkTextureHandle{};
                    NkTextureHandle ssao  = (ssaoColorId != NK_INVALID_RES_ID)
                                           ? mRenderGraph->GetResourceTexture(ssaoColorId)
                                           : NkTextureHandle{};
                    if (mPostProcess && hdr.IsValid()) {
                        mPostProcess->ExecuteRHI(cmd, hdr, bloom, ssao);
                    }
                });

                // Pass 2 : FXAA -> swapchain. Active uniquement si fxaaOn.
                // Le RG insere auto la barrier COLOR_ATTACHMENT -> SHADER_READ
                // pour le transient toneTexId entre la pass PostProcess (Writes)
                // et la pass FXAA_Final (Reads).
                if (fxaaOn && toneTexId != NK_INVALID_RES_ID) {
                    auto& fxaa = g.AddPass("FXAA_Final", NkPassType::NK_POST_PROCESS);
                    fxaa.Reads(toneTexId);
                    fxaa.SetColor(0, colorId, NkLoadOp::NK_CLEAR, {0,0,0,1});
                    NkGraphResId capturedToneId = toneTexId;
                    fxaa.Execute([this, capturedToneId](NkICommandBuffer* cmd) {
                        NkTextureHandle ldr = mRenderGraph->GetResourceTexture(capturedToneId);
                        if (mPostProcess && ldr.IsValid()) {
                            mPostProcess->ExecuteFXAA(cmd, ldr);
                        }
                    });
                }
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

            // FlushCompilations() retire de BeginFrame : il compilait tous les
            // pipelines avec mCurrentRP={} (avant le 1er Flush qui le set), donc
            // fallback swapchain RP — incompatible avec Geometry HDR. La compilation
            // est desormais 100% lazy au 1er BindInstance, ce qui garantit un
            // mCurrentRP valide. Hitch initial acceptable (5 templates compiles
            // au 1er drawcall) car amorti sur 1 frame.

            // Hot-reload des shaders user-overrides (throttle ~1x/sec a 60fps).
            // PollHotReload est no-op si aucun NkShaderProgram n'a vertPath/fragPath
            // renseignes (= aucun fichier override actif), donc cout negligeable.
            if (mShaders && (mFrameCounter % 60) == 0) mShaders->PollHotReload();
            mFrameCounter++;

            mCmd->Reset();
            mCmd->Begin();

            // Reset l'index du pool d'UBO objets de NkRender3D pour la nouvelle frame.
            // Doit etre fait ici (et pas dans BeginScene) sinon des passes multiples
            // dans la meme frame (passe miroir + passe principale, ex. Demo4) se
            // pietinent les UBOs avec les backends a commandes differees (GL).
            if (mRender3D.Get()) mRender3D->ResetFrame();
            // Phase M.2 : upload du UBO de la collection si dirty.
            if (mMaterialCollection) mMaterialCollection->Upload();
            return true;
        }

        void NkRendererImpl::EndFrame() {
            mDevice->EndFrame(mFrameCtx);
        }

        void NkRendererImpl::Present() {
            if (!mCmd) return;

            // Auto-rendering des planar reflections : execute AVANT les passes
            // du RenderGraph. Le RenderGraph ouvrira/fermera ses propres
            // render passes (Shadows, Geometry, ...) ; les BeginRenderPass
            // imbriques sont interdits cote Vulkan, donc les passes RT du
            // PlanarReflectionSystem doivent etre completees ici, AVANT toute
            // BeginRenderPass du graph. Le RT (color attachment du plane) est
            // gere par le system, hors graph.
            if (mPlanarReflection && mRender3D)
                mPlanarReflection->RenderReflections(mCmd, mRender3D.Get());

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