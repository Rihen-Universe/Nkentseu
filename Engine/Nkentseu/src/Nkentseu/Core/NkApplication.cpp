#include "NkApplication.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    NkApplication* NkApplication::sInstance = nullptr;

    // =========================================================================
    NkApplication::NkApplication(const NkApplicationConfig& config) : mConfig(config) {
        NK_ASSERT_MSG(!sInstance, "Application: une instance existe déjà");
        sInstance = this;
    }

    NkApplication::~NkApplication() {
        sInstance = nullptr;
    }

    bool NkApplication::Init() {
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

        mWidth  = mDevice->GetSwapchainWidth();
        mHeight = mDevice->GetSwapchainHeight();

        // ── Callbacks utilisateur ─────────────────────────────────────────────
        OnInit();
        for (NkLayer* layer : mLayerStack) {
            layer->OnAttach(); // déjà appelé par PushLayer, sécurité supplémentaire optionnelle
        }
        OnStart();
        return true;
    }

    // =========================================================================
    // Run — point d'entrée de la boucle principale
    // =========================================================================
    void NkApplication::Run() {

        // ── Boucle principale ─────────────────────────────────────────────────
        mRunning    = true;
        mAccumulator = 0.0f;
        NkClock clock;
        NkEventSystem& events = NkEvents();

        logger.Infof("[Application] Boucle démarrée — fixed step=%.4fs\n", mConfig.fixedTimeStep);

        while (mRunning) {
            // ── Événements ───────────────────────────────────────────────────
            events.PollEvents();
            if (!mRunning) break;

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
                    for (NkLayer* layer : mLayerStack) {
                        layer->OnFixedUpdate(mConfig.fixedTimeStep);
                    }
                    OnFixedUpdate(mConfig.fixedTimeStep);
                    mAccumulator -= mConfig.fixedTimeStep;
                }
            }

            // ── Update logique ────────────────────────────────────────────────
            for (NkLayer* layer : mLayerStack) {
                layer->OnUpdate(dt);
            }
            OnUpdate(dt);

            // ── Rendu GPU ─────────────────────────────────────────────────────
            if (mWidth == 0 || mHeight == 0) continue;

            // Resize swapchain si la fenêtre a changé de taille
            if (mWidth != mDevice->GetSwapchainWidth() || mHeight != mDevice->GetSwapchainHeight()) {
                mDevice->OnResize(mWidth, mHeight);
            }

            NkFrameContext frame;
            if (!mDevice->BeginFrame(frame)) continue;

            mWidth  = mDevice->GetSwapchainWidth();
            mHeight = mDevice->GetSwapchainHeight();
            if (mWidth == 0 || mHeight == 0) {
                mDevice->EndFrame(frame);
                continue;
            }

            // Render layers
            for (NkLayer* layer : mLayerStack) {
                layer->OnRender();
            }
            OnRender();

            // UI render layers
            for (NkLayer* layer : mLayerStack) {
                layer->OnUIRender();
            }
            OnUIRender();

            mDevice->SubmitAndPresent(mCmd);
            mDevice->EndFrame(frame);
        }

        // ── Arrêt propre ──────────────────────────────────────────────────────
        logger.Infof("[Application] Arrêt demandé, nettoyage...\n");

        OnShutdown();

        // Detach des couches (inverse de l'attachement)
        for (nk_isize i = (nk_isize)mLayerStack.Size() - 1; i >= 0; --i) {
        }

        ShutdownDevice();
        ShutdownPlatform();

        logger.Infof("[Application] Terminé proprement.\n");
    }

    // =========================================================================
    // Run
    // =========================================================================
    void NkApplication::Run() {
        if (!InitPlatform()) {
            logger.Errorf("[Application] Échec InitPlatform\n");
            return;
        }
        if (!InitDevice()) {
            logger.Errorf("[Application] Échec InitDevice\n");
            ShutdownPlatform();
            return;
        }

        mWidth  = mDevice->GetSwapchainWidth();
        mHeight = mDevice->GetSwapchainHeight();

        OnInit();
        OnStart();

        mRunning     = true;
        mAccumulator = 0.0f;
        NkClock clock;
        NkEventSystem& events = NkEvents();

        logger.Infof("[Application] Boucle — fixed={:.4f}s maxDt={:.3f}s\n",
                     mConfig.fixedTimeStep, mConfig.maxDeltaTime);
    }

    // =========================================================================
    void NkApplication::Quit() {
        mRunning = false;
    }

    // =========================================================================
    void NkApplication::PushLayer(NkLayer* layer) {
        mLayerStack.PushLayer(layer);
    }

    void NkApplication::PushOverlay(NkOverlay* overlay) {
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
        NkEventSystem& events = NkEvents();

        // Fermeture
        events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) OnWindowClose(e);
        });

        // Resize
        events.AddEventCallback<NkWindowResizeEvent>([this](NkWindowResizeEvent* e) {
            mWidth  = static_cast<nk_uint32>(e->GetWidth());
            mHeight = static_cast<nk_uint32>(e->GetHeight());
            OnResize(mWidth, mHeight);
            NkEventBus::Dispatch(e); // pas de consommation — tout le monde doit voir
        });

        // Propagation aux layers pour Clavier
        events.AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkKeyReleaseEvent>([this](NkKeyReleaseEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkTextInputEvent>([this](NkTextInputEvent* e) {
            NkEventBus::Dispatch(e);
            DispatchToLayers(e);
        });

        // Propagation aux layers pour Souris
        events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkMouseDoubleClickEvent>([this](NkMouseDoubleClickEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        
        events.AddEventCallback<NkMouseWheelVerticalEvent>([this](NkMouseWheelVerticalEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });
        events.AddEventCallback<NkMouseWheelHorizontalEvent>([this](NkMouseWheelHorizontalEvent* e) {
            bool consumed = NkEventBus::Dispatch(e);
            if (!consumed) DispatchToLayers(e);
        });

        logger.Infof("[Application] Fenêtre créée: {0}x{1}\n", mConfig.windowConfig.width, mConfig.windowConfig.height);
        return true;
    }

    // =========================================================================
    // InitDevice — crée le device RHI
    // =========================================================================
    bool NkApplication::InitDevice() {
        NkDeviceInitInfo& di = mConfig.deviceInfo;
        di.surface           = mWindow.GetSurfaceDesc();
        di.width             = mWindow.GetSize().width;
        di.height            = mWindow.GetSize().height;

        mDevice = NkDeviceFactory::Create(di);
        if (!mDevice || !mDevice->IsValid()) {
            logger.Errorf("[Application] NkIDevice : échec (api={})\n",
                          NkGraphicsApiName(di.api));
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
        return true;
    }

    // =========================================================================
    void NkApplication::ShutdownDevice() {
        if (mDevice) {
            mDevice->WaitIdle();
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
    // DispatchEvent — propage aux layers (droite → gauche, consommation possible)
    // =========================================================================
    void NkApplication::DispatchEvent(NkEvent* event) {
        if (!event) return;
        // Parcours des overlays puis des layers (droite → gauche)
        for (NkLayer** it = mLayerStack.rbegin(); it != mLayerStack.rend(); --it) {
            if ((*it)->OnEvent(event)) break; // consommé
        }
    }

    // =========================================================================
    bool NkApplication::OnWindowClose(NkWindowCloseEvent* /*e*/) {
        OnClose();
        return true;
    }

    bool NkApplication::OnWindowResize(NkWindowResizeEvent* e) {
        mWidth  = static_cast<nk_uint32>(e->GetWidth());
        mHeight = static_cast<nk_uint32>(e->GetHeight());
        OnResize(mWidth, mHeight);
        return false; // ne consomme pas
    }

    // ── Dispatch vers les layers (droite → gauche, avec consommation) ─────────
    void NkApplication::DispatchToLayers(NkEvent* event) {
        for (NkLayer** it = mLayerStack.rbegin(); it != mLayerStack.rend(); --it) {
            if ((*it)->OnEvent(event)) break;
        }
    }

} // namespace nkentseu

