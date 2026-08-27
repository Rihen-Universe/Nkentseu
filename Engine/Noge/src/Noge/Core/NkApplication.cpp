#include "NkApplication.h"
#include "NkEventBus.h"						  // NkEventBus::Dispatch (pont événements -> layers)
#include "NKRenderer/NkRenderer.h"			  // moteur 2D/3D (façade NKRenderer)
#include "NKRenderer/Core/NkRendererConfig.h" // NkRendererConfig
#include "NKLogger/NkLog.h"

namespace nkentseu {

	NkApplication *NkApplication::sInstance = nullptr;

	// =========================================================================
	NkApplication::NkApplication(const NkApplicationConfig &config) : mConfig(config) {
		NKENTSEU_ASSERT_MSG(!sInstance, "Application: une instance existe déjà");
		sInstance = this;
	}

	NkApplication::~NkApplication() {
		Shutdown(); // RAII : libère device + fenêtre même si Run() non atteint
		sInstance = nullptr;
	}

	bool NkApplication::Init() {
		// Hook AVANT création fenêtre/device : l'utilisateur peut ajuster mConfig.
		OnPreInit();

		// ── Init plateforme & device ──────────────────────────────────────────
		if (!InitPlatform()) {
			logger.Errorf("[Application] Échec InitPlatform\n");
			return false;
		}
		if (!InitDevice()) {
			logger.Errorf("[Application] Échec InitDevice\n");
			ShutdownPlatform();
			return false;
		}

		mWidth = mDevice->GetSwapchainWidth();
		mHeight = mDevice->GetSwapchainHeight();

		// ── Callbacks utilisateur ─────────────────────────────────────────────
		OnInit();
		// [FIX 2026-07-25] PAS de second passage OnAttach ici : PushLayer/
		// PushOverlay (NkLayerStack) appellent DÉJÀ OnAttach à l'empilement.
		// L'ancien re-parcours attachait chaque layer DEUX fois (double init
		// NkUIContext, double création FBO → fuites GPU, cf. logs Nogee).
		OnStart();
		return true;
	}

	// =========================================================================
	// Run — point d'entrée de la boucle principale
	// =========================================================================
	void NkApplication::Run() {
		// L'initialisation (plateforme/device/OnInit/OnStart) est faite par
		// NkApplication::Init(), appelée AVANT Run() (cf. NkMainApp). Run() ne
		// contient QUE la boucle principale — pas de double InitPlatform.

		// ── Boucle principale ─────────────────────────────────────────────────
		mRunning = true;
		mAccumulator = 0.0f;
		NkClock clock;
		NkEventSystem &events = NkEvents();

		logger.Infof("[Application] Boucle démarrée — fixed step=%.4fs\n", mConfig.fixedTimeStep);

		while (mRunning) {
			// ── Événements ───────────────────────────────────────────────────
			events.PollEvents();
			if (!mRunning)
				break;

			// ── Delta time ───────────────────────────────────────────────────
			float dt = clock.Tick().delta;
			if (dt <= 0.0f || dt > mConfig.maxDeltaTime) {
				dt = mConfig.maxDeltaTime;
			}

			// ── Fixed update (physique) ───────────────────────────────────────
			if (mConfig.fixedTimeStep > 0.0f) {
				mAccumulator += dt;
				while (mAccumulator >= mConfig.fixedTimeStep) {
					// Layers (gauche → droite)
					for (NkLayer *layer : mLayerStack) {
						layer->OnFixedUpdate(mConfig.fixedTimeStep);
					}
					OnFixedUpdate(mConfig.fixedTimeStep);
					mAccumulator -= mConfig.fixedTimeStep;
				}
			}

			// ── Update logique ────────────────────────────────────────────────
			for (NkLayer *layer : mLayerStack) {
				layer->OnUpdate(dt);
			}
			OnUpdate(dt);

			// ── Rendu 2D/3D via NKRenderer (pipeline complet) ─────────────────
			if (mWidth == 0 || mHeight == 0)
				continue;

			// Resize : le renderer redimensionne le device + son render graph.
			if (mWidth != mRenderer->GetWidth() || mHeight != mRenderer->GetHeight()) {
				mRenderer->OnResize(mWidth, mHeight);
			}

			if (!mRenderer->BeginFrame())
				continue;				// ouvre device + frame de rendu
			mCmd = mRenderer->GetCmd(); // cmd buffer de la frame courante

			// Render layers : accèdent aux sous-systèmes via GetRenderer().
			for (NkLayer *layer : mLayerStack) {
				layer->OnRender();
			}
			OnRender();

			// UI render layers
			for (NkLayer *layer : mLayerStack) {
				layer->OnUIRender();
			}
			OnUIRender();

			// ORDRE CRITIQUE — Present() AVANT EndFrame(). Les noms disent l'inverse
			// de ce que fait le code : c'est Present() qui exécute le render graph,
			// ferme le command buffer et soumet ; EndFrame() ne fait que clore la
			// frame device. Inversés, la frame device est close avant d'avoir été
			// soumise : sous Vulkan, EndFrame prend alors le chemin « frame
			// abandonnée » (NkVulkanDevice.cpp:2397), pose mFrameAcquired=false et
			// recrée la swapchain, puis SubmitAndPresent sort à sa première ligne
			// (NkVulkanDevice.cpp:2273) — plus rien n'est soumis ni présenté, et
			// AUCUNE erreur pilote n'est levée. Cf. wiki/Runtime/NKRenderer/Frame-Contract.md
			mRenderer->Present();  // exécute le render graph, ferme le CB, soumet + présente
			mRenderer->EndFrame(); // clôt la frame device (fige les stats, avance l'index)
		}

		// ── Arrêt propre (idempotent ; le destructeur l'appelle aussi) ────────
		Shutdown();
	}

	// =========================================================================
	// Shutdown — nettoyage complet idempotent (OnShutdown + device + plateforme)
	// =========================================================================
	void NkApplication::Shutdown() {
		if (mShutdownDone)
			return; // déjà fait (Run() puis destructeur) → no-op
		mShutdownDone = true;

		logger.Infof("[Application] Nettoyage...\n");
		OnShutdown();		// callback utilisateur (libère ses ressources)
		ShutdownDevice();	// WaitIdle + détruit cmd buffer + device (null-safe)
		ShutdownPlatform(); // ferme la fenêtre
		logger.Infof("[Application] Terminé proprement.\n");
	}

	// =========================================================================
	void NkApplication::Quit() {
		mRunning = false;
	}

	// =========================================================================
	void NkApplication::PushLayer(NkLayer *layer) {
		mLayerStack.PushLayer(layer);
	}

	void NkApplication::PushOverlay(NkOverlay *overlay) {
		mLayerStack.PushOverlay(overlay);
	}

	// =========================================================================
	// InitPlatform — crée la fenêtre et s'abonne aux événements système
	// =========================================================================
	bool NkApplication::InitPlatform() {
		// Log init
		logger.SetLevel(mConfig.logLevel);
		logger.Infof("[Application] Init — {0} v{1}\n", mConfig.appName.CStr(), mConfig.appVersion.CStr());

		// Création de la fenêtre
		if (!mWindow.Create(mConfig.windowConfig)) {
			logger.Errorf("[Application] Fenêtre : échec\n");
			return false;
		}

		// ── Pont NkEventSystem → EventBus ────────────────────────────────────
		// NkEventSystem est le système natif (callbacks par type depuis NKWindow).
		// On redirige chaque type vers NkEventBus::DispatchRaw pour que tous les
		// layers puissent souscrire via EventBus sans coupler NkEventSystem.

		// Abonnement aux événements natifs
		NkEventSystem &events = NkEvents();

		// Fermeture
		events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				OnWindowClose(e);
		});

		// Resize
		events.AddEventCallback<NkWindowResizeEvent>([this](NkWindowResizeEvent *e) {
			OnWindowResize(e);		 // met à jour mWidth/mHeight + appelle OnResize()
			NkEventBus::Dispatch(e); // pas de consommation — tout le monde doit voir
		});

		// Propagation aux layers pour Clavier
		events.AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkKeyReleaseEvent>([this](NkKeyReleaseEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkTextInputEvent>([this](NkTextInputEvent *e) {
			NkEventBus::Dispatch(e);
			DispatchToLayers(e);
		});

		// Propagation aux layers pour Souris
		events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkMouseDoubleClickEvent>([this](NkMouseDoubleClickEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});

		events.AddEventCallback<NkMouseWheelVerticalEvent>([this](NkMouseWheelVerticalEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});
		events.AddEventCallback<NkMouseWheelHorizontalEvent>([this](NkMouseWheelHorizontalEvent *e) {
			bool consumed = NkEventBus::Dispatch(e);
			if (!consumed)
				DispatchToLayers(e);
		});

		logger.Infof("[Application] Fenêtre créée: {0}x{1}\n", mConfig.windowConfig.width, mConfig.windowConfig.height);
		return true;
	}

	// =========================================================================
	// InitDevice — crée le device RHI
	// =========================================================================
	bool NkApplication::InitDevice() {
		NkDeviceInitInfo &di = mConfig.deviceInfo;
		di.surface = mWindow.GetSurfaceDesc();
		di.width = mWindow.GetSize().width;
		di.height = mWindow.GetSize().height;

		mDevice = NkDeviceFactory::Create(di);
		if (!mDevice || !mDevice->IsValid()) {
			logger.Errorf("[Application] NkIDevice : échec (api={})\n", NkGraphicsApiName(di.api));
			return false;
		}

		mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
		if (!mCmd || !mCmd->IsValid()) {
			logger.Errorf("[Application] CommandBuffer : échec\n");
			NkDeviceFactory::Destroy(mDevice);
			mDevice = nullptr;
			return false;
		}

		logger.Infof("[Application] Device RHI OK — {0}\n", NkGraphicsApiName(di.api));

		// ── Moteur de rendu 2D/3D (NKRenderer, sur le device) ─────────────────
		// Façade exposant TOUS les sous-systèmes (Render2D/Render3D/ombres/IBL/
		// post-process/VFX/anim...). Create() appelle Initialize() en interne.
		renderer::NkRendererConfig rcfg; // configuration par défaut
		mRenderer = renderer::NkRenderer::Create(mDevice, rcfg);
		if (!mRenderer) {
			logger.Errorf("[Application] NKRenderer : échec d'initialisation\n");
			mDevice->DestroyCommandBuffer(mCmd);
			mCmd = nullptr;
			NkDeviceFactory::Destroy(mDevice);
			mDevice = nullptr;
			return false;
		}
		logger.Infof("[Application] NKRenderer OK (moteur 2D/3D)\n");
		return true;
	}

	// =========================================================================
	void NkApplication::ShutdownDevice() {
		if (mDevice) {
			mDevice->WaitIdle();
			// Détruire le renderer AVANT le device (il référence le device).
			if (mRenderer) {
				renderer::NkRenderer::Destroy(mRenderer); // appelle Shutdown() en interne
				mRenderer = nullptr;
			}
			if (mCmd) {
				mDevice->DestroyCommandBuffer(mCmd);
				mCmd = nullptr;
			}
			NkDeviceFactory::Destroy(mDevice);
			mDevice = nullptr;
		}
	}

	void NkApplication::ShutdownPlatform() {
		mWindow.Close();
	}

	// =========================================================================
	bool NkApplication::OnWindowClose(NkWindowCloseEvent * /*e*/) {
		OnClose();
		return true;
	}

	bool NkApplication::OnWindowResize(NkWindowResizeEvent *e) {
		mWidth = static_cast<nk_uint32>(e->GetWidth());
		mHeight = static_cast<nk_uint32>(e->GetHeight());
		OnResize(mWidth, mHeight);
		return false; // ne consomme pas
	}

	// ── Dispatch vers les layers (droite → gauche, avec consommation) ─────────
	void NkApplication::DispatchToLayers(NkEvent *event) {
		for (NkLayer **it = mLayerStack.rbegin(); it != mLayerStack.rend(); --it) {
			if ((*it)->OnEvent(event))
				break;
		}
	}

} // namespace nkentseu
