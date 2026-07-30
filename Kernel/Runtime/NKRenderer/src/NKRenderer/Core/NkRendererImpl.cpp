// =============================================================================
// NkRendererImpl.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRendererImpl.h"
#include "NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h"
#include "NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"
#include "NKTime/NkChrono.h" // cap FPS : pacing haute précision (Now/Sleep)
#include <cstdlib>			 // getenv (NK_FPS_CAP)

// Windows : ::Sleep() a une granularité ~15,6 ms par défaut -> le sleep du pacing
// FPS déborde le spin -> jitter (saccade/clignotement des ombres). timeBeginPeriod(1)
// passe la résolution du timer système à 1 ms -> Sleep précis -> pacing lisse.
// Forward-decl (winmm) pour éviter d'inclure <windows.h> dans ce TU.
#if defined(NKENTSEU_PLATFORM_WINDOWS)
extern "C" unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
extern "C" unsigned int __stdcall timeEndPeriod(unsigned int uPeriod);
#endif

namespace nkentseu {
	namespace renderer {

		// Helper : alloue via le NkAllocator par defaut (NkAllocator::New utilise
		// _aligned_malloc, donc NkUniquePtr::Reset peut faire _aligned_free
		// proprement). Eviter `new T()` qui passe par malloc et provoque une
		// corruption heap au moment du free.
		template <typename T, typename... Args> static inline T *AllocOwned(Args &&...args) {
			return memory::NkGetDefaultAllocator().New<T>(traits::NkForward<Args>(args)...);
		}

		// ── Fabrique statique ─────────────────────────────────────────────────────
		NkRenderer *NkRenderer::Create(NkIDevice *device, const NkRendererConfig &cfg) {
			auto *renderer = memory::NkGetDefaultAllocator().New<NkRendererImpl>(device, cfg);
			if (!renderer->Initialize()) {
				memory::NkGetDefaultAllocator().Delete(renderer);
				return nullptr;
			}
			return renderer;
		}

		void NkRenderer::Destroy(NkRenderer *&renderer) {
			if (renderer) {
				renderer->Shutdown();
				memory::NkGetDefaultAllocator().Delete(static_cast<NkRendererImpl *>(renderer));
				renderer = nullptr;
			}
		}

		// ── Constructor / Destructor ──────────────────────────────────────────────
		NkRendererImpl::NkRendererImpl(NkIDevice *device, const NkRendererConfig &cfg) : mDevice(device), mCfg(cfg) {
		}

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
			if (mInitialized)
				return true;
			if (!mDevice) {
				NkRSetLastError(NkRResult::NK_ERR_INVALID_DEVICE, "NkRendererImpl::Initialize device==nullptr");
				return false;
			}
			logger.Info("[NkRendererImpl] Initialize start (api={0})\n", (int)mCfg.api);

			// ── CORRECTIF course CPU/GPU (2026-07-28) : la PROFONDEUR DES RINGS DOIT
			//    ÊTRE CELLE DU DEVICE, pas une valeur de config indépendante. ────────
			// Tous les rings du renderer (UBO camera/lights/objets/bones/instances,
			// descriptor sets, batch d'arêtes n-gon, UBO d'ombres) choisissent leur slot
			// par `mDevice->GetFrameIndex() % framesInFlight`. Or GetFrameIndex() est
			// CYCLIQUE MODULO LA PROFONDEUR DU DEVICE (MAX_FRAMES = 3 sur GL/VK/DX11/DX12)
			// et c'est la fence de CE slot-là que BeginFrame attend.
			// Avec framesInFlight = 2 et un device à 3, la suite de slots devient
			// 0,1,0 | 0,1,0 | ... : le slot 0 revient sur DEUX FRAMES CONSÉCUTIVES.
			// Une frame sur trois, le CPU réécrit donc (memcpy en mémoire mappée) le
			// buffer que le GPU est encore en train de lire pour la frame précédente
			// -> vertices/UBO déchirés -> CLIGNOTEMENT rapide (mesuré : 13 collisions
			// sur 40 frames, exactement 1/3). Prendre la profondeur du device rend
			// `GetFrameIndex() % framesInFlight` BIJECTIF : deux frames consécutives ne
			// partagent plus jamais un slot, et chaque slot est couvert par sa fence.
			{
				const uint32 devFIF = mDevice->GetMaxFramesInFlight();
				if (devFIF > 0 && mCfg.framesInFlight != devFIF) {
					logger.Info("[NkRendererImpl] framesInFlight config={0} -> {1} (profondeur REELLE du "
								"device : les rings doivent la suivre, sinon deux frames consecutives "
								"partagent un slot)\n",
								mCfg.framesInFlight, devFIF);
					mCfg.framesInFlight = devFIF;
				}
			}

			// Cap FPS par défaut = 0 (DESACTIVE). Le rythme + la protection thermique
			// sont assurés par le VSYNC (GL wglSwapIntervalEXT / VK FIFO) : la présentation
			// se cale sur le vblank de l'écran -> pas de tearing, pas de GPU 100%.
			//
			// IMPORTANT : le cap-sleep ci-dessous NE DOIT PAS tourner en même temps que le
			// vsync -> ils se battent (le sleep déborde APRES un SwapBuffers déjà bloqué au
			// vblank) et capper SOUS la fréquence écran (ex. 60 sur 144 Hz) produit un
			// JUDDER (clignotement du mouvement). Le cap reste OPT-IN via NK_FPS_CAP ou
			// SetFrameRateCap()/F1 (utile sans vsync ou pour brider volontairement).
			{
				const char *fc = getenv("NK_FPS_CAP");
				mFrameCapFps = fc ? (float32)atof(fc) : 0.f;
				if (mFrameCapFps < 0.f)
					mFrameCapFps = 0.f;
				logger.Info("[NkRendererImpl] FPS cap = {0} (0=vsync gere le rythme, env NK_FPS_CAP)\n", mFrameCapFps);
#if defined(NKENTSEU_PLATFORM_WINDOWS)
				timeBeginPeriod(1); // Sleep précis (1 ms) pour un pacing FPS sans jitter.
#endif
			}

			// 0. RHI (toujours requis)
			logger.Info("[NkRendererImpl]  step 0: InitRHI\n");
			if (!InitRHI())
				return false;

			// 1. NkResources (toujours actif — default tex/samplers/layouts)
			logger.Info("[NkRendererImpl]  step 1: NkResources::Init\n");
			mResources.Reset(AllocOwned<NkResources>());
			if (!NkROk(mResources->Init(mDevice)))
				return false;

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
			if (!NkROk(mTextures->Init(mDevice, mResources.Get())))
				return false;

			// 4. Mesh system (toujours actif — primitives utilisees par toutes les passes)
			logger.Info("[NkRendererImpl]  step 5: NkMeshSystem::Init\n");
			mMeshSystem.Reset(AllocOwned<NkMeshSystem>());
			if (!mMeshSystem->Init(mDevice))
				return false;

			// 5. Material system (toujours actif — fournit les templates PBR/Unlit)
			logger.Info("[NkRendererImpl]  step 6: NkMaterialSystem::Init\n");
			mMaterials.Reset(AllocOwned<NkMaterialSystem>());
			if (!mMaterials->Init(mDevice, mTextures.Get(), mShaders.Get(), mCfg.api))
				return false;

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
			const bool needsR2D =
				mCfg.Has(NK_SS_RENDER2D) || mCfg.Has(NK_SS_TEXT) || mCfg.Has(NK_SS_UI) || mCfg.Has(NK_SS_OVERLAY);
			if (needsR2D)
				if (!InitRender2D())
					return false;

			// Render3D (requis indirect par SHADOW/ANIMATION/SIMULATION)
			const bool needsR3D = mCfg.Has(NK_SS_RENDER3D) || mCfg.Has(NK_SS_SHADOW) || mCfg.Has(NK_SS_ANIMATION) ||
								  mCfg.Has(NK_SS_SIMULATION);
			// Shadow + Environment doivent etre init AVANT Render3D pour que ce dernier
			// puisse binder le ShadowUBO/atlas et les cubemaps IBL dans son frame set.
			if (needsR3D) {
				logger.Info("[NkRendererImpl]  step 7: InitShadow\n");
				if (!InitShadow())
					return false;
			}
			if (needsR3D) {
				logger.Info("[NkRendererImpl]  step 8: InitEnvironment\n");
				if (!InitEnvironment())
					return false;
			}
			if (needsR3D) {
				logger.Info("[NkRendererImpl]  step 9: InitRender3D\n");
				if (!InitRender3D())
					return false;
			}

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

			if (mCfg.Has(NK_SS_TEXT)) {
				logger.Info("[NkRendererImpl]  step10: InitTextRenderer\n");
				if (!InitTextRenderer())
					return false;
			}
			if (mCfg.Has(NK_SS_POST_PROCESS)) {
				logger.Info("[NkRendererImpl]  step11: InitPostProcess\n");
				if (!InitPostProcess())
					return false;
			}
			if (mCfg.Has(NK_SS_OVERLAY)) {
				logger.Info("[NkRendererImpl]  step12: InitOverlay\n");
				if (!InitOverlay())
					return false;
			}
			if (mCfg.Has(NK_SS_VFX)) {
				logger.Info("[NkRendererImpl]  step13: InitVFX\n");
				if (!InitVFX())
					return false;
			}
			if (mCfg.Has(NK_SS_ANIMATION)) {
				logger.Info("[NkRendererImpl]  step14: InitAnimation\n");
				if (!InitAnimation())
					return false;
			}
			if (mCfg.Has(NK_SS_SIMULATION)) {
				logger.Info("[NkRendererImpl]  step15: InitSimulation\n");
				if (!InitSimulation())
					return false;
			}

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
			if (mShadow)
				return true;
			NkVirtualShadowMapsConfig sc;
			// Atlas size : on prefere 4096 par defaut pour pouvoir packer
			// plusieurs lights. Si la config NkShadowConfig demande une taille
			// plus petite (resolution per-tile), on la respecte mais sur l'atlas.
			sc.atlasSize = (mCfg.shadow.resolution > 0) ? mCfg.shadow.resolution * 2 : 4096;
			sc.numCascades = mCfg.shadow.cascadeCount > 0 ? mCfg.shadow.cascadeCount : 3;
			sc.quality = mCfg.shadow.pcss
							 ? NkVSMShadowQuality::PCSS
							 : (mCfg.shadow.softShadows ? NkVSMShadowQuality::PCF3x3 : NkVSMShadowQuality::NONE);
			mShadow.Reset(AllocOwned<NkVirtualShadowMaps>());
			if (!mShadow->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(), sc, mCfg.framesInFlight)) {
				mShadow.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkVirtualShadowMaps::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitRender2D() {
			if (mRender2D)
				return true;
			mRender2D.Reset(AllocOwned<NkRender2D>());
			if (!mRender2D->Init(mDevice, mTextures.Get(), mShaders.Get())) {
				mRender2D.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkRender2D::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitRender3D() {
			if (mRender3D)
				return true;
			mRender3D.Reset(AllocOwned<NkRender3D>());
			if (!mRender3D->Init(mDevice, mMeshSystem.Get(), mMaterials.Get(), mRenderGraph.Get(), mShadow.Get(),
								 mEnvironment.Get(), mShaders.Get(), mResources.Get(), mCfg.framesInFlight)) {
				mRender3D.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkRender3D::Init failed");
				return false;
			}
			// Initialise la taille courante du Render3D à la résolution configurée :
			// OnResize n'est appelé QUE sur un vrai changement de taille (cf. BeginFrame
			// auto-resize) ; si la fenêtre s'ouvre pile à la taille config, mW/mH
			// resteraient à 0. Or CompositeSelectionOutline dérive la taille du pixel
			// (1/w, 1/h) de mW/mH pour l'edge-detect du liseré de sélection — à 0, les
			// offsets d'échantillonnage explosent (1.0 en UV) et le liseré disparaît.
			mRender3D->OnResize(mCfg.width, mCfg.height);
			// Wire la connexion inverse : NkShadowSystem itere les opaques de
			// mRender3D dans sa passe shadow. Necessaire pour D.3b.
			if (mShadow)
				mShadow->SetRenderer3D(mRender3D.Get());
			mRender3D->SetIBLStrength(mCfg.ibl.iblStrength);
			// Phase N v0.5 : active la skybox HDR en background si l'app le
			// demande (recommande quand useHDR=true pour voir l'environnement
			// entier, pas juste ses reflets sur les objets).
			mRender3D->SetSkyboxEnabled(mCfg.ibl.drawSkybox);
			return true;
		}

		bool NkRendererImpl::InitEnvironment() {
			if (mEnvironment)
				return true;
			mEnvironment.Reset(AllocOwned<NkEnvironmentSystem>());

			// Phase N v0 : build NkEnvironmentConfig depuis mCfg.ibl (NkIBLConfig).
			// L'app peut customiser via cfg.ibl.useHDR + hdrPath OU les couleurs
			// procedurales skyTop/horizon/ground.
			NkEnvironmentConfig ecfg;
			ecfg.irradianceSize = mCfg.ibl.irradianceMapSize > 0 ? mCfg.ibl.irradianceMapSize : 32;
			ecfg.prefilterSize = mCfg.ibl.specularMapSize > 0 ? mCfg.ibl.specularMapSize : 128;
			ecfg.prefilterMips = mCfg.ibl.prefilterMipCount > 0 ? mCfg.ibl.prefilterMipCount : 5;
			ecfg.brdfLUTSize = mCfg.ibl.brdfLUTSize > 0 ? mCfg.ibl.brdfLUTSize : 256;
			ecfg.source = mCfg.ibl.useHDR ? NkEnvSource::NK_ENV_HDR_FILE : NkEnvSource::NK_ENV_PROCEDURAL;
			ecfg.hdrPath = mCfg.ibl.hdrPath;
			ecfg.skyTop = mCfg.ibl.skyTop;
			ecfg.horizon = mCfg.ibl.horizon;
			ecfg.ground = mCfg.ibl.ground;

			if (!mEnvironment->Init(mDevice, ecfg)) {
				mEnvironment.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkEnvironmentSystem::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitTextRenderer() {
			if (mTextRenderer)
				return true;
			if (!mRender2D && !InitRender2D())
				return false; // dep
			mTextRenderer.Reset(AllocOwned<NkTextRenderer>());
			if (!mTextRenderer->Init(mDevice, mTextures.Get(), mRender2D.Get())) {
				mTextRenderer.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkTextRenderer::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitPostProcess() {
			if (mPostProcess)
				return true;
			mPostProcess.Reset(AllocOwned<NkPostProcessStack>());
			if (!mPostProcess->Init(mDevice, mTextures.Get(), mMeshSystem.Get(), mShaders.Get(), mResources.Get(),
									mCfg.width, mCfg.height)) {
				mPostProcess.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkPostProcessStack::Init failed");
				return false;
			}
			mPostProcess->SetConfig(mCfg.postProcess);
			return true;
		}

		bool NkRendererImpl::InitOverlay() {
			if (mOverlay)
				return true;
			if (!mRender2D && !InitRender2D())
				return false;
			if (!mTextRenderer && !InitTextRenderer())
				return false;
			mOverlay.Reset(AllocOwned<NkOverlayRenderer>());
			if (!mOverlay->Init(mDevice, mRender2D.Get(), mTextRenderer.Get())) {
				mOverlay.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkOverlayRenderer::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitVFX() {
			if (mVFX)
				return true;
			mVFX.Reset(AllocOwned<NkVFXSystem>());
			if (!mVFX->Init(mDevice, mTextures.Get(), mMeshSystem.Get())) {
				mVFX.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkVFXSystem::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitAnimation() {
			if (mAnimation)
				return true;
			if (!mRender3D && !InitRender3D())
				return false;
			mAnimation.Reset(AllocOwned<NkAnimationSystem>());
			if (!mAnimation->Init(mDevice, mRender3D.Get())) {
				mAnimation.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkAnimationSystem::Init failed");
				return false;
			}
			return true;
		}

		bool NkRendererImpl::InitSimulation() {
			if (mSimulation)
				return true;
			if (!mRender3D && !InitRender3D())
				return false;
			if (!mVFX && !InitVFX())
				return false;
			mSimulation.Reset(AllocOwned<NkSimulationRenderer>());
			if (!mSimulation->Init(mDevice, mRender3D.Get(), mVFX.Get())) {
				mSimulation.Reset();
				NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkSimulationRenderer::Init failed");
				return false;
			}
			return true;
		}

		// =====================================================================
		// SetUIOverlayCallback — overlay UI applicatif (cf. NkRenderer.h).
		// Reconstruit le graph : la passe Overlay2D dépend de la présence du
		// callback quand ni Render2D ni OverlayRenderer ne sont actifs.
		// [AJOUT 2026-07-25]
		// =====================================================================
		void NkRendererImpl::SetUIOverlayCallback(const NkUIOverlayCallback &cb) {
			mUIOverlayCb = cb;
			if (mInitialized)
				RebuildRenderGraph();
		}

		// =====================================================================
		// Reconstruction du render graph (apres enable/disable runtime ou resize).
		// =====================================================================
		void NkRendererImpl::RebuildRenderGraph() {
			if (mRenderGraph)
				mRenderGraph->Reset();
			// L'historique du TAA est un transient DU GRAPH : le Reset ci-dessus le
			// detruit, et BuildDefaultRenderGraph en recree un VIERGE. Il faut donc
			// desarmer l'accumulation, sinon la premiere frame d'apres melange 90 %
			// d'une cible neuve (noire) a l'image -> l'ecran s'assombrit puis remonte
			// sur ~20 frames a chaque redimensionnement, changement d'option, ou
			// redirection de cible (capture / enregistrement). Mesure a l'origine de
			// ce constat : luminance 19,6 au lieu de 92,9 trois frames apres un
			// rebuild, exactement 92,9*(1-0,9^3).
			mTAAHasPrev = false;
			// Compteur de rebuilds : un rebuild par frame passerait inapercu tout en
			// desarmant en permanence les effets temporels (l'accumulation du TAA ne
			// demarrerait jamais). On trace donc les premiers, puis par paliers.
			static uint32 sRebuilds = 0;
			++sRebuilds;
			if (sRebuilds <= 8 || (sRebuilds % 100) == 0)
				logger.Info("[NkRendererImpl] RebuildRenderGraph #{0}\n", sRebuilds);
			BuildDefaultRenderGraph();
		}

		// =====================================================================
		// EnableSubsystem / DisableSubsystem / IsSubsystemActive
		// =====================================================================
		bool NkRendererImpl::EnableSubsystem(NkSubsystemFlags flags) {
			if (!mInitialized)
				return false;
			bool any = false;
			// L'ordre respecte les dependances : Shadow + Environment AVANT Render3D
			// (Render3D bind le ShadowUBO et les cubemaps env dans son frame set au Init).
			if (NkHasFlag(flags, NK_SS_RENDER2D)) {
				if (!mRender2D)
					any |= InitRender2D();
			}
			if (NkHasFlag(flags, NK_SS_RENDER3D)) {
				if (!mShadow)
					any |= InitShadow();
			}
			if (NkHasFlag(flags, NK_SS_RENDER3D)) {
				if (!mEnvironment)
					any |= InitEnvironment();
			}
			if (NkHasFlag(flags, NK_SS_RENDER3D)) {
				if (!mRender3D)
					any |= InitRender3D();
			}
			if (NkHasFlag(flags, NK_SS_SHADOW)) {
				if (!mShadow)
					any |= InitShadow();
			}
			if (NkHasFlag(flags, NK_SS_TEXT)) {
				if (!mTextRenderer)
					any |= InitTextRenderer();
			}
			if (NkHasFlag(flags, NK_SS_POST_PROCESS)) {
				if (!mPostProcess)
					any |= InitPostProcess();
			}
			if (NkHasFlag(flags, NK_SS_OVERLAY)) {
				if (!mOverlay)
					any |= InitOverlay();
			}
			if (NkHasFlag(flags, NK_SS_VFX)) {
				if (!mVFX)
					any |= InitVFX();
			}
			if (NkHasFlag(flags, NK_SS_ANIMATION)) {
				if (!mAnimation)
					any |= InitAnimation();
			}
			if (NkHasFlag(flags, NK_SS_SIMULATION)) {
				if (!mSimulation)
					any |= InitSimulation();
			}

			// Update config flags pour refleter l'etat reel
			mCfg.Enable(flags);
			// Reconstruire le graph pour integrer les nouvelles passes
			if (any)
				RebuildRenderGraph();
			return any;
		}

		void NkRendererImpl::DisableSubsystem(NkSubsystemFlags flags) {
			if (!mInitialized)
				return;
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

			if (NkHasFlag(flags, NK_SS_SIMULATION))
				mSimulation.Reset();
			if (NkHasFlag(flags, NK_SS_ANIMATION))
				mAnimation.Reset();
			if (NkHasFlag(flags, NK_SS_VFX))
				mVFX.Reset();
			if (NkHasFlag(flags, NK_SS_OVERLAY))
				mOverlay.Reset();
			if (NkHasFlag(flags, NK_SS_POST_PROCESS))
				mPostProcess.Reset();
			if (NkHasFlag(flags, NK_SS_TEXT))
				mTextRenderer.Reset();
			if (NkHasFlag(flags, NK_SS_SHADOW))
				mShadow.Reset();
			if (NkHasFlag(flags, NK_SS_RENDER3D)) {
				mRender3D.Reset();
				mEnvironment.Reset();
			}
			if (NkHasFlag(flags, NK_SS_RENDER2D))
				mRender2D.Reset();

			mCfg.Disable(flags);
			RebuildRenderGraph();
		}

		bool NkRendererImpl::IsSubsystemActive(NkSubsystemFlags flags) const {
			// Tous les flags demandes doivent correspondre a un sous-systeme alloue.
			const NkSubsystemFlags active = const_cast<NkRendererImpl *>(this)->GetActiveSubsystems();
			return (static_cast<uint32>(active) & static_cast<uint32>(flags)) == static_cast<uint32>(flags);
		}

		NkSubsystemFlags NkRendererImpl::GetActiveSubsystems() const {
			uint32 a = 0;
			if (mRender2D)
				a |= NK_SS_RENDER2D;
			if (mRender3D)
				a |= NK_SS_RENDER3D;
			if (mTextRenderer)
				a |= NK_SS_TEXT;
			if (mShadow)
				a |= NK_SS_SHADOW;
			if (mPostProcess)
				a |= NK_SS_POST_PROCESS;
			if (mOverlay)
				a |= NK_SS_OVERLAY;
			if (mVFX)
				a |= NK_SS_VFX;
			if (mAnimation)
				a |= NK_SS_ANIMATION;
			if (mSimulation)
				a |= NK_SS_SIMULATION;
			return static_cast<NkSubsystemFlags>(a);
		}

		// ── Shutdown ──────────────────────────────────────────────────────────────
		void NkRendererImpl::Shutdown() {
			if (!mInitialized)
				return;

			for (auto *t : mOffscreenTargets) {
				t->Shutdown();
				memory::NkGetDefaultAllocator().Delete(t);
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
			if (mMaterialLibrary)
				mMaterials->SetLibrary(nullptr);
			mMaterialLibrary.Reset();
			mMaterialCollection.Reset();
			mMaterials.Reset();
			mMeshSystem.Reset();
			mTextures.Reset();
			mRenderGraph.Reset();
			mShaders.Reset();
			mResources.Reset();

			if (mCmd)
				mDevice->DestroyCommandBuffer(mCmd);

			mInitialized = false;
		}

		// ── RHI init ──────────────────────────────────────────────────────────────
		bool NkRendererImpl::InitRHI() {
			if (!mDevice->IsValid())
				return false;
			uint32 w = mDevice->GetSwapchainWidth();
			uint32 h = mDevice->GetSwapchainHeight();
			if (w > 0)
				mCfg.width = w;
			if (h > 0)
				mCfg.height = h;

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
			auto &g = *mRenderGraph;
			const bool has3D = (mRender3D.Get() != nullptr);
			const bool has2D = (mRender2D.Get() != nullptr);
			const bool hasShadow = (mShadow.Get() != nullptr) && mCfg.shadow.enabled;
			// D.4b : on n'active le HDR transient que si NkPostProcessStack a au
			// moins un effet wire (tonemap pour l'instant). HasAnyEffect lit la
			// config courante et evite d'allouer un HDR target inutile.
			// const bool hasPP       = false;
			const bool hasPP = (mPostProcess.Get() != nullptr) && mPostProcess->HasAnyEffect();
			const bool hasVFX = (mVFX.Get() != nullptr);
			const bool hasOverlay = (mOverlay.Get() != nullptr);

			// Swapchain (toujours imported — c'est l'output final de la frame).
			// Si une cible finale externe est fournie (viewport editeur sur device
			// partage), on redirige la sortie du graph vers elle (RT echantillonnable)
			// au lieu de la swapchain — pipeline COMPLET (ombres/eclairage/IBL/tonemap).
			NkGraphResId colorId;
			if (mFinalColorOverride.IsValid()) {
				NkTextureDesc fd = NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, mDevice->GetSwapchainFormat());
				colorId = g.ImportTexture("Swapchain", mFinalColorOverride, NkResourceState::NK_SHADER_READ, fd);
			} else {
				colorId = g.ImportTexture("Swapchain", NkTextureHandle{}, NkResourceState::NK_PRESENT);
			}

			// Cible 3D : si POST_PROCESS active → HDR transient ; sinon ecrit directement dans Swapchain.
			NkGraphResId mainColor = colorId;
			NkGraphResId mainDepth = NK_INVALID_RES_ID;
			if (has3D) {
				mainDepth = g.CreateTransient("MainDepth", NkTextureDesc::DepthStencil(mCfg.width, mCfg.height));
				if (hasPP) {
					mainColor = g.CreateTransient(
						"HDR", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA16_FLOAT));
				}
			}

			// ── Shadow pass ──────────────────────────────────────────────────
			// D.3b : NkShadowSystem possede son propre atlas + son propre FBO et
			// gere son BeginRenderPass / EndRenderPass en interne. Cette passe
			// est juste un point d'ordonnancement — le RenderGraph ne touche pas
			// au RenderPass automatique (pas de SetColor/SetDepth declares).
			if (hasShadow) {
				g.AddPass("Shadows", NkPassType::NK_SHADOW)
					.SetAlwaysExecute(true) // outputs hors-graph (FBO interne au ShadowSystem)
					.Execute([this](NkICommandBuffer *cmd) { mShadow->RenderAllShadows(cmd); });
			}

			// ── Geometry pass (3D opaque) ─────────────────────────────────────
			// DEFERRED v1 (cfg.deferred + post-process actif) : G-buffer MRT →
			// lighting fullscreen → reste en forward par-dessus. Sinon : passe
			// Geometry forward classique.
			const bool useDeferred = has3D && mCfg.deferred && hasPP;
			if (useDeferred) {
				NkGraphResId gbufA = g.CreateTransient(
					"GBufAlbedo", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA8_UNORM));
				NkGraphResId gbufN = g.CreateTransient(
					"GBufNormal", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA16_FLOAT));
				NkGraphResId gbufE = g.CreateTransient(
					"GBufEmissive", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA16_FLOAT));

				auto &geom = g.AddPass("DeferredGeom", NkPassType::NK_GEOMETRY);
				geom.SetColor(0, gbufA, NkLoadOp::NK_CLEAR, {0.f, 0.f, 0.f, 0.f})
					.SetColor(1, gbufN, NkLoadOp::NK_CLEAR, {0.5f, 0.5f, 1.f, 1.f})
					.SetColor(2, gbufE, NkLoadOp::NK_CLEAR, {0.f, 0.f, 0.f, 1.f})
					.SetDepth(mainDepth, NkLoadOp::NK_CLEAR, 1.f);
				geom.Execute([this](NkICommandBuffer *cmd) { mRender3D->FlushDeferredGeometry(cmd); });

				auto &lit = g.AddPass("DeferredLight", NkPassType::NK_POST_PROCESS);
				lit.Reads(gbufA);
				lit.Reads(gbufN);
				lit.Reads(gbufE);
				lit.Reads(mainDepth);
				lit.SetColor(0, mainColor, NkLoadOp::NK_CLEAR, {0.05f, 0.05f, 0.07f, 1.f});
				lit.Execute([this, gbufA, gbufN, gbufE, mainDepth](NkICommandBuffer *cmd) {
					mRender3D->RenderDeferredLighting(cmd, mRenderGraph->GetResourceTexture(gbufA),
													  mRenderGraph->GetResourceTexture(gbufN),
													  mRenderGraph->GetResourceTexture(gbufE),
													  mRenderGraph->GetResourceTexture(mainDepth));
				});

				// Reste en FORWARD par-dessus le HDR (skybox, instancies, skins,
				// grille, transparents, debug) avec la MEME depth (LOAD).
				auto &fwd = g.AddPass("ForwardRest", NkPassType::NK_GEOMETRY);
				fwd.SetColor(0, mainColor, NkLoadOp::NK_LOAD)
					.SetDepth(mainDepth, NkLoadOp::NK_LOAD);
				fwd.Execute([this](NkICommandBuffer *cmd) { mRender3D->FlushForwardRest(cmd); });
			} else if (has3D) {
				auto &geom = g.AddPass("Geometry", NkPassType::NK_GEOMETRY);
				geom.SetColor(0, mainColor, NkLoadOp::NK_CLEAR, {0.05f, 0.05f, 0.07f, 1.f})
					.SetDepth(mainDepth, NkLoadOp::NK_CLEAR, 1.f);
				// shadowId n'est plus dans le graph (NkShadowSystem gere son atlas
				// hors-graph). Le sequencing Shadows->Geometry est garanti par
				// l'ordre d'AddPass et le bind du shadow atlas se fait via le
				// descriptor set frame de Render3D (set au Init).
				geom.Execute([this](NkICommandBuffer *cmd) { mRender3D->Flush(cmd); });
			}

			// ── VFX pass (transparents) ───────────────────────────────────────
			if (has3D && hasVFX) {
				g.AddPass("VFX", NkPassType::NK_TRANSPARENT)
					.Reads(mainDepth)
					.SetColor(0, mainColor, NkLoadOp::NK_LOAD)
					.Execute([this](NkICommandBuffer *cmd) {
						// VFX flush integre par le sous-systeme VFX
						(void)cmd;
					});
			}

			// ── Phase H.3 : SSAO (Screen Space Ambient Occlusion) ──────────────
			// Atténue l'ambient/IBL des zones occluses par geometrie proche
			// (objets sous le sol, dans les coins, sous une table, etc.).
			// Pass : Reads(mainDepth) -> SetColor(ssaoTex, R8_UNORM, W/2 x H/2).
			// Le tonemap multiplie HDR par le SSAO factor avant ACES.
			NkGraphResId ssaoTex = NK_INVALID_RES_ID;
			NkGraphResId ssaoBlurredTex = NK_INVALID_RES_ID;
			const bool hasSSAO =
				has3D && hasPP && mPostProcess && mCfg.postProcess.ssao && mainDepth != NK_INVALID_RES_ID;
			if (hasSSAO) {
				uint32 sw = mCfg.width / 2 ? mCfg.width / 2 : 1;
				uint32 sh = mCfg.height / 2 ? mCfg.height / 2 : 1;
				ssaoTex = g.CreateTransient("SSAO", NkTextureDesc::RenderTarget(sw, sh, NkGPUFormat::NK_R8_UNORM));

				auto &sp = g.AddPass("SSAO", NkPassType::NK_POST_PROCESS);
				sp.Reads(mainDepth);
				sp.SetColor(0, ssaoTex, NkLoadOp::NK_CLEAR, {1.f, 1.f, 1.f, 1.f});
				NkGraphResId depthId = mainDepth;
				sp.Execute([this, depthId, sw, sh](NkICommandBuffer *cmd) {
					NkTextureHandle depthTex = mRenderGraph->GetResourceTexture(depthId);
					if (mPostProcess && depthTex.IsValid()) {
						mPostProcess->DrawSSAOPass(cmd, depthTex, sw, sh);
					}
				});

				// Phase H.5b : pass blur denoise sur le ssaoTex noisy.
				// Le tonemap sample ssaoBlurredTex au lieu de ssaoTex.
				ssaoBlurredTex =
					g.CreateTransient("SSAO_Blurred", NkTextureDesc::RenderTarget(sw, sh, NkGPUFormat::NK_R8_UNORM));
				auto &bp = g.AddPass("SSAO_Blur", NkPassType::NK_POST_PROCESS);
				bp.Reads(ssaoTex);
				bp.SetColor(0, ssaoBlurredTex, NkLoadOp::NK_CLEAR, {1.f, 1.f, 1.f, 1.f});
				NkGraphResId aoId = ssaoTex;
				bp.Execute([this, aoId, sw, sh](NkICommandBuffer *cmd) {
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
			for (int i = 0; i < kBloomMipsRG; i++)
				bloomMip[i] = NK_INVALID_RES_ID;

			const bool hasBloom = has3D && hasPP && mPostProcess && mCfg.postProcess.bloom;
			if (hasBloom) {
				// Cree les transients pyramide.
				for (int i = 0; i < kBloomMipsRG; i++) {
					uint32 div = 1u << (i + 1); // mip 0 = W/2, mip 5 = W/64
					uint32 bw = mCfg.width / div ? mCfg.width / div : 1;
					uint32 bh = mCfg.height / div ? mCfg.height / div : 1;
					char name[32];
					snprintf(name, sizeof(name), "BloomMip%d", i);
					bloomMip[i] =
						g.CreateTransient(name, NkTextureDesc::RenderTarget(bw, bh, NkGPUFormat::NK_RGBA16_FLOAT));
				}

				// 6 passes downsample : extrait highlights + downsample x2 par mip.
				// Pass 0 : src = mainColor (HDR), threshold actif.
				// Pass 1..5 : src = bloomMip[i-1], threshold = 0 (passthrough).
				const float bloomThr = mCfg.postProcess.bloomThreshold;
				for (int i = 0; i < kBloomMipsRG; i++) {
					char passName[32];
					snprintf(passName, sizeof(passName), "Bloom_Down_%d", i);
					auto &dp = g.AddPass(passName, NkPassType::NK_POST_PROCESS);
					NkGraphResId src = (i == 0) ? mainColor : bloomMip[i - 1];
					dp.Reads(src);
					dp.SetColor(0, bloomMip[i], NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
					uint32 div = 1u << i; // mip i source resolution = W/(2^i) avant downsample
					uint32 srcW = (i == 0) ? mCfg.width : (mCfg.width / div ? mCfg.width / div : 1);
					uint32 srcH = (i == 0) ? mCfg.height : (mCfg.height / div ? mCfg.height / div : 1);
					float thr = (i == 0) ? bloomThr : 0.0f;
					dp.Execute([this, src, srcW, srcH, thr](NkICommandBuffer *cmd) {
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
					auto &up = g.AddPass(passName, NkPassType::NK_POST_PROCESS);
					up.Reads(bloomMip[i + 1]);
					// NK_LOAD pour preserver le downsample de la mip courante
					// (la pass upsample blende additif par-dessus).
					up.SetColor(0, bloomMip[i], NkLoadOp::NK_LOAD);
					uint32 div = 1u << (i + 2); // mip i+1 = W/(2^(i+2))
					uint32 srcW = mCfg.width / div ? mCfg.width / div : 1;
					uint32 srcH = mCfg.height / div ? mCfg.height / div : 1;
					NkGraphResId src = bloomMip[i + 1];
					up.Execute([this, src, srcW, srcH](NkICommandBuffer *cmd) {
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
				// TAA (Phase L) : ALTERNATIVE au FXAA, pas un cumul — enchainer les
				// deux flouterait deux fois. Le TAA a priorite quand il est actif.
				// Le jitter de projection est active au meme endroit : sans lui,
				// toutes les frames echantillonnent au meme point dans le pixel et
				// l'accumulation temporelle n'apporte STRICTEMENT rien.
				const bool taaOn = mPostProcess && mPostProcess->IsTAAEnabled() && has3D;
				if (mRender3D)
					mRender3D->SetTAAJitterEnabled(taaOn);
				const bool fxaaOn = !taaOn && mPostProcess && mPostProcess->IsFXAAEnabled();
				NkGraphResId postTargetId = colorId;
				NkGraphResId toneTexId = NK_INVALID_RES_ID;
				if (fxaaOn || taaOn) {
					auto tdesc = NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA8_UNORM);
					tdesc.debugName = "ToneLDR_Transient";
					toneTexId = g.CreateTransient("ToneLDR", tdesc);
					if (toneTexId != NK_INVALID_RES_ID) {
						postTargetId = toneTexId;
					}
				}

				// ── Auto-exposure V1 (Phase L, 2026-07-30) ────────────────────
				// Mesure la luminance moyenne de la scene HDR dans une cible 1x1
				// AVANT le tonemap, qui la consomme au binding 4. La cible est
				// hors-graph (ping-pong persistant possede par le post-process) :
				// cette passe ne declare donc AUCUN attachement et ouvre son propre
				// render pass — meme modele que la passe d'ombres. Reads(mainColor)
				// reste necessaire pour que le RG insere la barriere
				// COLOR_ATTACHMENT -> SHADER_READ sur le HDR avant l'echantillonnage.
				if (mPostProcess && mPostProcess->IsAutoExposureEnabled()) {
					auto &ae = g.AddPass("AutoExposure", NkPassType::NK_POST_PROCESS);
					ae.Reads(mainColor);
					ae.SetAlwaysExecute(true); // sortie hors-graph (cible 1x1 interne)
					NkGraphResId aeHdrId = mainColor;
					ae.Execute([this, aeHdrId](NkICommandBuffer *cmd) {
						NkTextureHandle hdr = mRenderGraph->GetResourceTexture(aeHdrId);
						if (mPostProcess && hdr.IsValid())
							mPostProcess->RunAutoExposure(cmd, hdr);
					});
				}

				auto &pp = g.AddPass("PostProcess", NkPassType::NK_POST_PROCESS);
				pp.Reads(mainColor);
				if (mainDepth != NK_INVALID_RES_ID)
					pp.Reads(mainDepth);
				if (hasBloom && bloomMip[0] != NK_INVALID_RES_ID)
					pp.Reads(bloomMip[0]);
				if (hasSSAO && ssaoBlurredTex != NK_INVALID_RES_ID)
					pp.Reads(ssaoBlurredTex);
				pp.SetColor(0, postTargetId, NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
				NkGraphResId hdrColorId = mainColor; // capture by value
				NkGraphResId bloomColorId = hasBloom ? bloomMip[0] : NK_INVALID_RES_ID;
				NkGraphResId ssaoColorId = hasSSAO ? ssaoBlurredTex : NK_INVALID_RES_ID;
				pp.Execute([this, hdrColorId, bloomColorId, ssaoColorId](NkICommandBuffer *cmd) {
					NkTextureHandle hdr = mRenderGraph->GetResourceTexture(hdrColorId);
					NkTextureHandle bloom = (bloomColorId != NK_INVALID_RES_ID)
												? mRenderGraph->GetResourceTexture(bloomColorId)
												: NkTextureHandle{};
					NkTextureHandle ssao = (ssaoColorId != NK_INVALID_RES_ID)
											   ? mRenderGraph->GetResourceTexture(ssaoColorId)
											   : NkTextureHandle{};
					if (mPostProcess && hdr.IsValid()) {
						mPostProcess->ExecuteRHI(cmd, hdr, bloom, ssao);
					}
				});

				// ── TAA : historique ping-pong DANS le graph, puis blit ecran ─────
				// ⚠️ POURQUOI LES CIBLES SONT DES RESSOURCES DU GRAPH (fix 2026-07-30).
				// Version precedente : la passe TAA n'avait AUCUN attachement (elle
				// ouvrait son propre render pass sur un historique hors-graph, sur le
				// modele de la passe d'ombres) et lisait le transient ToneLDR. Elle
				// sortait NOIR : le RenderGraph ne transitionne un transient en
				// SHADER_READ que pour une passe declarant un VRAI attachement, donc
				// ToneLDR n'etait jamais rendu lisible — `Reads(id)` seul ne suffit pas.
				// Preuve d'alors : en forcant le fragment shader a une couleur
				// constante, l'ecran devenait integralement rouge (le draw et le blit
				// marchaient, seul l'echantillonnage echouait). La passe AutoExposure
				// s'en sort de la meme position parce que PostProcess relit mainColor
				// juste apres AVEC un attachement, et declenche la transition pour elle.
				//
				// Correctif : les deux moities de l'historique deviennent des transients
				// du graph (ils PERSISTENT entre frames — le graph n'est reset qu'au
				// rebuild/resize) et la passe declare un vrai attachement. Le graph pose
				// alors toutes les barrieres : ToneLDR et la profondeur en SHADER_READ,
				// la cible en RENDER_TARGET, l'historique lu en SHADER_READ.
				//
				// TROIS passes a cibles FIXES, et non deux passes qui alterneraient
				// leur cible : le cache de framebuffers du graph est indexe PAR NOM DE
				// PASSE, donc une cible qui change d'une frame a l'autre est hors du
				// modele. La variante essayee (TAA_0/TAA_1 en ping-pong, une seule
				// dessinant par frame, l'autre ouvrant son render pass en LOAD) a ete
				// MESUREE DEFAILLANTE : l'historique relu etait noir (sonde
				// NK_TAA_DEBUG=2 : 97 % de pixels noirs), le clamp de voisinage
				// masquant le defaut en le ramenant au minimum local — l'image avait
				// l'air correcte a 1,2 % pres alors qu'aucune accumulation n'avait lieu.
				// Ici chaque passe a une cible fixe et s'execute a chaque frame :
				//   TAA         : ToneLDR + historique -> TAAOut
				//   TAA_Store   : TAAOut -> historique (pour la frame suivante)
				//   TAA_Present : TAAOut -> ecran
				// Cout : une copie plein ecran par frame (~0,1 ms en 1080p), en echange
				// d'un comportement qui ne depend plus de la facon dont chaque backend
				// honore un loadOp LOAD sur une passe qui ne dessine rien.
				// Contournement ECARTE : ajouter Reads(toneTexId) sur TAA_Present — la
				// barriere serait posee APRES le draw du TAA.
				if (taaOn && toneTexId != NK_INVALID_RES_ID) {
					auto hdesc = NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_RGBA8_UNORM);
					hdesc.debugName = "TAAOut";
					NkGraphResId taaOutId = g.CreateTransient("TAAOut", hdesc);
					hdesc.debugName = "TAAHistory";
					NkGraphResId histId = g.CreateTransient("TAAHist", hdesc);

					if (taaOutId != NK_INVALID_RES_ID && histId != NK_INVALID_RES_ID) {
						const NkGraphResId taaToneId = toneTexId;
						const NkGraphResId taaDepthId = mainDepth;

						auto &taa = g.AddPass("TAA", NkPassType::NK_POST_PROCESS);
						taa.Reads(taaToneId);
						taa.Reads(histId); // resultat de la frame -1 (ecrit par TAA_Store)
						if (taaDepthId != NK_INVALID_RES_ID)
							taa.Reads(taaDepthId);
						taa.SetColor(0, taaOutId, NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
						taa.Execute([this, taaToneId, histId, taaDepthId](NkICommandBuffer *cmd) {
							if (!mPostProcess || !mRender3D)
								return;
							NkTextureHandle ldr = mRenderGraph->GetResourceTexture(taaToneId);
							NkTextureHandle hist = mRenderGraph->GetResourceTexture(histId);
							NkTextureHandle depth = (taaDepthId != NK_INVALID_RES_ID)
														? mRenderGraph->GetResourceTexture(taaDepthId)
														: NkTextureHandle{};
							if (!ldr.IsValid())
								return;
							// Reprojection = viewProj de la frame PRECEDENTE composee
							// avec l'inverse de la viewProj COURANTE. On utilise les
							// matrices reellement utilisees au rendu (jitter et clip-Z
							// compris), exposees par NkRender3D : les recalculer ici
							// dupliquerait ces corrections et deriverait de la
							// profondeur echantillonnee.
							const NkMat4f cur = mRender3D->GetRenderViewProj();
							NkMat4f reproj = NkMat4f::Identity();
							if (mTAAHasPrev)
								reproj = mTAAPrevViewProj * mRender3D->GetRenderInvViewProj();
							mPostProcess->RunTAAInPass(cmd, ldr, hist, depth, reproj, mTAAHasPrev,
													   mRenderGraph->GetPassRenderPass("TAA"));
							// NK_TAA_PREVLAG=N : n'actualiser la matrice de la frame
							// precedente qu'une frame sur N. Outil de MESURE, pas une
							// option de rendu : quand la camera bouge lentement, reproj
							// est quasi l'identite et les conventions Y s'annulent
							// (ndcYSign^2 = 1), donc une erreur de signe est
							// indetectable. Espacer les deux matrices amplifie le
							// mouvement inter-frame et rend l'erreur mesurable.
							static int sPrevLag = -1;
							if (sPrevLag < 0) {
								const char *v = getenv("NK_TAA_PREVLAG");
								sPrevLag = (v && v[0]) ? atoi(v) : 1;
								if (sPrevLag < 1)
									sPrevLag = 1;
							}
							static int sLagCount = 0;
							if ((sLagCount++ % sPrevLag) == 0)
								mTAAPrevViewProj = cur;
							mTAAHasPrev = true;
						});

						// Recopie du resultat dans l'historique. Declaree AVANT
						// TAA_Present : elle lit TAAOut (donc s'ordonne apres TAA) et
						// ecrit l'historique, que seule la passe TAA lit — le tri
						// topologique reste acyclique car au moment ou TAA est traitee,
						// l'historique n'a pas encore de producteur declare.
						auto &taaStore = g.AddPass("TAA_Store", NkPassType::NK_POST_PROCESS);
						taaStore.Reads(taaOutId);
						taaStore.SetColor(0, histId, NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
						taaStore.Execute([this, taaOutId](NkICommandBuffer *cmd) {
							if (!mPostProcess)
								return;
							NkTextureHandle src = mRenderGraph->GetResourceTexture(taaOutId);
							if (src.IsValid())
								mPostProcess->ExecuteBlitToRT(cmd, src,
															  mRenderGraph->GetPassRenderPass("TAA_Store"));
						});

						auto &taaPresent = g.AddPass("TAA_Present", NkPassType::NK_POST_PROCESS);
						taaPresent.Reads(taaOutId);
						taaPresent.SetColor(0, colorId, NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
						taaPresent.Reads(histId); // cf. sonde NK_TAA_PRESENT_HIST
						taaPresent.Execute([this, taaOutId, histId](NkICommandBuffer *cmd) {
							if (!mPostProcess)
								return;
							// Sonde : presenter l'HISTORIQUE au lieu du resultat permet de
							// trancher entre "la recopie n'ecrit pas" et "la relecture
							// echoue" — les deux se manifestent par un historique noir.
							static int sPresentHist = -1;
							if (sPresentHist < 0) {
								const char *v = getenv("NK_TAA_PRESENT_HIST");
								sPresentHist = (v && v[0] && v[0] != '0') ? 1 : 0;
							}
							NkTextureHandle res =
								mRenderGraph->GetResourceTexture(sPresentHist ? histId : taaOutId);
							if (res.IsValid())
								mPostProcess->ExecuteBlit(cmd, res);
						});
					}
				}

				// Pass 2 : FXAA -> swapchain. Active uniquement si fxaaOn.
				// Le RG insere auto la barrier COLOR_ATTACHMENT -> SHADER_READ
				// pour le transient toneTexId entre la pass PostProcess (Writes)
				// et la pass FXAA_Final (Reads).
				if (fxaaOn && toneTexId != NK_INVALID_RES_ID) {
					auto &fxaa = g.AddPass("FXAA_Final", NkPassType::NK_POST_PROCESS);
					fxaa.Reads(toneTexId);
					fxaa.SetColor(0, colorId, NkLoadOp::NK_CLEAR, {0, 0, 0, 1});
					NkGraphResId capturedToneId = toneTexId;
					fxaa.Execute([this, capturedToneId](NkICommandBuffer *cmd) {
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
				auto &dbg = g.AddPass("DebugDirect", NkPassType::NK_UI_OVERLAY);
				dbg.SetColor(0, colorId, NkLoadOp::NK_LOAD, {0.05f, 0.05f, 0.07f, 1.f});
				dbg.Execute([this](NkICommandBuffer *cmd) {
					if (mRender3D)
						mRender3D->DebugDrawDirectSwapchain(cmd);
				});
			}

			// ── Sélection « outline silhouette » façon Blender ────────────────
			// Option DISTINCTE de l'AABB du gizmo. Deux passes, ajoutées seulement
			// quand l'option est active (NkRender3D::SetSelectionOutline -> rebuild) :
			//   1) SelectionMask : les objets sélectionnés rendus SEULS (blanc) dans une
			//      cible R8 -> silhouette pleine.
			//   2) SelectionOutline : plein écran, dilatation-différence du masque ->
			//      fin liseré orange composité (LOAD) sur l'image finale (colorId).
			// Placées APRÈS le post-process et AVANT l'overlay 2D : le liseré se dessine
			// par-dessus la scène finale (façon Blender, overlay), sous l'UI.
			if (has3D && mRender3D && mRender3D->IsSelectionOutlineEnabled()) {
				NkGraphResId selMask = g.CreateTransient(
					"SelMask", NkTextureDesc::RenderTarget(mCfg.width, mCfg.height, NkGPUFormat::NK_R8_UNORM));

				// Color-only (pas de depth) : silhouette pleine, ordre de rasterisation
				// indifférent -> type POST_PROCESS comme les autres passes color-only.
				auto &mp = g.AddPass("SelectionMask", NkPassType::NK_POST_PROCESS);
				mp.SetColor(0, selMask, NkLoadOp::NK_CLEAR, {0.f, 0.f, 0.f, 1.f});
				mp.Execute([this](NkICommandBuffer *cmd) {
					if (mRender3D)
						mRender3D->RenderSelectionMask(cmd);
				});

				auto &op = g.AddPass("SelectionOutline", NkPassType::NK_UI_OVERLAY);
				op.Reads(selMask);
				op.SetColor(0, colorId, NkLoadOp::NK_LOAD);
				NkGraphResId maskId = selMask;
				op.Execute([this, maskId](NkICommandBuffer *cmd) {
					if (mRender3D)
						mRender3D->CompositeSelectionOutline(cmd, mRenderGraph->GetResourceTexture(maskId));
				});
			}

			// ── 2D + UI overlay ───────────────────────────────────────────────
			// Si aucune passe 3D ne clear le swapchain (config 2D-only), on clear ici.
			// [AJOUT 2026-07-25] La passe existe aussi si un callback UI applicatif
			// est enregistré (SetUIOverlayCallback — ex. NKUI de l'éditeur Nogee) ;
			// il est invoqué en fin de passe, render pass active sur la sortie finale.
			if (has2D || hasOverlay || mUIOverlayCb.IsValid()) {
				auto &ov = g.AddPass("Overlay2D", NkPassType::NK_UI_OVERLAY);
				const auto loadOp = has3D ? NkLoadOp::NK_LOAD : NkLoadOp::NK_CLEAR;
				ov.SetColor(0, colorId, loadOp, {0.05f, 0.05f, 0.07f, 1.f});
				ov.Execute([this](NkICommandBuffer *cmd) {
					if (mRender2D)
						mRender2D->FlushPending(cmd);
					if (mOverlay)
						mOverlay->FlushPending(cmd);
					if (mUIOverlayCb.IsValid())
						mUIOverlayCb(cmd);
				});
			}

			// ── MirrorPresent : « voir + enregistrer » ─────────────────────────
			// Quand la cible finale est redirigee (capture/enregistrement) ET que
			// le miroir ecran est demande, une derniere passe recopie la cible
			// redirigee vers le VRAI swapchain (fullscreen blit, ~1 draw) : la
			// fenetre reste vivante pendant l'enregistrement, la capture recoit
			// exactement la meme image, et le rendu n'est jamais bloque.
			if (mFinalColorOverride.IsValid() && mMirrorToScreen && mPostProcess.Get()) {
				NkGraphResId screenId = g.ImportTexture("Screen", NkTextureHandle{}, NkResourceState::NK_PRESENT);
				auto &mir = g.AddPass("MirrorPresent", NkPassType::NK_POST_PROCESS);
				mir.Reads(colorId);
				mir.SetColor(0, screenId, NkLoadOp::NK_CLEAR, {0.f, 0.f, 0.f, 1.f});
				mir.Execute([this](NkICommandBuffer *cmd) {
					if (mPostProcess)
						mPostProcess->ExecuteBlit(cmd, mFinalColorOverride);
				});
			}

			g.Compile();
		}

		// ── Frame ──────────────────────────────────────────────────────────────────
		bool NkRendererImpl::BeginFrame() {
			if (!mInitialized)
				return false;
			mStats.Reset();
			mFrameCtx = {};
			if (!mDevice->BeginFrame(mFrameCtx))
				return false;

			// Auto-resize — PAS en mode SetRenderSizeOverride (le rendu est a une
			// resolution independante ; la swapchain fenetre vit sa vie, le blit
			// MirrorPresent fait le pont).
			if (mRenderOverrideW == 0) {
				uint32 sw = mDevice->GetSwapchainWidth(), sh = mDevice->GetSwapchainHeight();
				if ((sw != mCfg.width || sh != mCfg.height) && sw > 0 && sh > 0)
					OnResize(sw, sh);
			}

			// Sélection « outline silhouette » : (dés)activer l'option ajoute/retire les
			// passes SelectionMask + SelectionOutline du graph -> rebuild à l'aplomb de
			// la frame (avant l'exécution du graph dans Present), comme pour un resize.
			if (mRender3D.Get() && mRender3D->ConsumeSelOutlineGraphDirty())
				RebuildRenderGraph();

			// FlushCompilations() retire de BeginFrame : il compilait tous les
			// pipelines avec mCurrentRP={} (avant le 1er Flush qui le set), donc
			// fallback swapchain RP — incompatible avec Geometry HDR. La compilation
			// est desormais 100% lazy au 1er BindInstance, ce qui garantit un
			// mCurrentRP valide. Hitch initial acceptable (5 templates compiles
			// au 1er drawcall) car amorti sur 1 frame.

			// Hot-reload des shaders user-overrides (throttle ~1x/sec a 60fps).
			// PollHotReload est no-op si aucun NkShaderProgram n'a vertPath/fragPath
			// renseignes (= aucun fichier override actif), donc cout negligeable.
			if (mShaders && (mFrameCounter % 60) == 0)
				mShaders->PollHotReload();
			mFrameCounter++;

			mCmd->Reset();
			mCmd->Begin();

			// Reset l'index du pool d'UBO objets de NkRender3D pour la nouvelle frame.
			// Doit etre fait ici (et pas dans BeginScene) sinon des passes multiples
			// dans la meme frame (passe miroir + passe principale, ex. Demo4) se
			// pietinent les UBOs avec les backends a commandes differees (GL).
			if (mRender3D.Get())
				mRender3D->ResetFrame();
			// Phase M.2 : upload du UBO de la collection si dirty.
			if (mMaterialCollection)
				mMaterialCollection->Upload();
			return true;
		}

		void NkRendererImpl::EndFrame() {
			mDevice->EndFrame(mFrameCtx);
		}

		void NkRendererImpl::Present() {
			if (!mCmd)
				return;

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

			// ── Cap FPS (pacing haute précision) ──────────────────────────────────
			// Plafonne la cadence sans jitter : sleep gros grain jusqu'à ~1.2 ms de la
			// cible (la granularité OS du sleep est ~1-15 ms), puis busy-wait (spin) le
			// reste. On ANCRE la prochaine échéance sur l'horaire idéal (mPaceNs +=
			// target), pas sur "now" -> période régulière -> dt lisse (pas de saccade
			// ni de crawl d'ombres). Resync si on prend > 1 frame de retard.
			if (mFrameCapFps > 0.f) {
				const float64 targetNs = 1.0e9 / (float64)mFrameCapFps;
				const float64 nowNs = ::nkentseu::NkChrono::Now().nanoseconds;
				if (mPaceNs > 0.0) {
					const float64 nextNs = mPaceNs + targetNs;
					const float64 remainNs = nextNs - nowNs;
					if (remainNs > 0.0) {
						const float64 spinNs = 1.2e6; // 1.2 ms de spin final
						if (remainNs > spinNs)
							::nkentseu::NkChrono::Sleep((int64)((remainNs - spinNs) / 1.0e6));
						while (::nkentseu::NkChrono::Now().nanoseconds < nextNs) { /* spin */
						}
					}
					mPaceNs = nextNs;
					const float64 after = ::nkentseu::NkChrono::Now().nanoseconds;
					if (after - mPaceNs > targetNs)
						mPaceNs = after; // trop en retard -> resync
				} else {
					mPaceNs = nowNs;
				}
			} else {
				mPaceNs = 0.0;
			}
		}

		void NkRendererImpl::OnResize(uint32 w, uint32 h) {
			ApplyRenderSize(w, h, /*touchDevice=*/true);
		}

		void NkRendererImpl::SetRenderSizeOverride(uint32 w, uint32 h) {
			// Rendu a une resolution INDEPENDANTE de la fenetre (ex : export 4K
			// pendant affichage 720p). Redimensionne cibles/transients/graph SANS
			// toucher la swapchain (la passe MirrorPresent re-echantillonne vers
			// l'ecran, le viewport etant pose PAR render pass a sa taille).
			// (0,0) = retour a la taille de la fenetre (swapchain).
			if (mRenderOverrideW == w && mRenderOverrideH == h)
				return;
			mRenderOverrideW = w;
			mRenderOverrideH = h;
			const uint32 tw = w ? w : (mDevice ? mDevice->GetSwapchainWidth() : mCfg.width);
			const uint32 th = h ? h : (mDevice ? mDevice->GetSwapchainHeight() : mCfg.height);
			ApplyRenderSize(tw, th, /*touchDevice=*/false);
		}

		void NkRendererImpl::ApplyRenderSize(uint32 w, uint32 h, bool touchDevice) {
			if (w == 0 || h == 0)
				return;
			mCfg.width = w;
			mCfg.height = h;
			// Propage au RHI pour mettre a jour la swapchain virtuelle (viewport / FBO 0).
			// PAS en mode override de taille de rendu : la swapchain reste a la
			// taille de la fenetre (le blit MirrorPresent fait le pont).
			if (mDevice && touchDevice)
				mDevice->OnResize(w, h);
			// Propage a tous les sous-systemes optionnels (selon la config courante).
			// For2D ne cree pas mPostProcess/mRender3D/mShadow, donc null check avant.
			if (mRender2D)
				mRender2D->OnResize(w, h);
			if (mRender3D)
				mRender3D->OnResize(w, h);
			if (mOverlay)
				mOverlay->OnResize(w, h);
			if (mPostProcess)
				mPostProcess->OnResize(w, h);
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

		void NkRendererImpl::SetPostConfig(const NkPostConfig &pp) {
			mCfg.postProcess = pp;
			if (mPostProcess)
				mPostProcess->SetConfig(pp);
		}

		void NkRendererImpl::SetWireframe(bool e) {
			mCfg.wireframe = e;
			if (mRender3D)
				mRender3D->SetWireframe(e);
		}

		// ── Offscreen ─────────────────────────────────────────────────────────────
		NkOffscreenTarget *NkRendererImpl::CreateOffscreen(const NkOffscreenDesc &desc) {
			auto *t = memory::NkGetDefaultAllocator().New<NkOffscreenTarget>();
			if (!t->Init(mDevice, mTextures.Get(), desc)) {
				memory::NkGetDefaultAllocator().Delete(t);
				return nullptr;
			}
			mOffscreenTargets.PushBack(t);
			return t;
		}

		void NkRendererImpl::DestroyOffscreen(NkOffscreenTarget *&t) {
			if (!t)
				return;
			for (uint32 i = 0; i < mOffscreenTargets.Size(); i++) {
				if (mOffscreenTargets[i] == t) {
					t->Shutdown();
					memory::NkGetDefaultAllocator().Delete(t);
					mOffscreenTargets.RemoveAt(i);
					break;
				}
			}
			t = nullptr;
		}

		void NkRendererImpl::SetFinalColorTarget(NkTextureHandle target) {
			if (mFinalColorOverride.id == target.id)
				return;
			mFinalColorOverride = target;
			mMirrorToScreen = false; // reset : le miroir se demande via ...Mirror()
			if (mInitialized)
				RebuildRenderGraph(); // l'import swapchain change de cible
		}

		void NkRendererImpl::SetFinalColorTargetMirror(NkTextureHandle target, bool mirrorToScreen) {
			if (mFinalColorOverride.id == target.id && mMirrorToScreen == mirrorToScreen)
				return;
			mFinalColorOverride = target;
			mMirrorToScreen = mirrorToScreen && target.IsValid();
			if (mInitialized)
				RebuildRenderGraph();
		}

	} // namespace renderer
} // namespace nkentseu