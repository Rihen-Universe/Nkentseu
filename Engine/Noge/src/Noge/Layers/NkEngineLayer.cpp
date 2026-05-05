// =============================================================================
// Nkentseu/Core/NkEngineLayer.cpp
// =============================================================================
#include "NkEngineLayer.h"
#include "NkApplication.h"
#include "Nkentseu/ECS/Systems/NkTransformSystem.h"
#include "Nkentseu/ECS/Entities/NkBehaviourSystem.h"
#include "Nkentseu/ECS/Scripting/NkScriptSystem.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    NkEngineLayer* NkEngineLayer::sInstance = nullptr;

    // =========================================================================
    // OnAttach — Initialisation de tous les sous-systèmes
    // =========================================================================
    void NkEngineLayer::OnAttach() {
        NK_PROFILE_SCOPE("NkEngineLayer::OnAttach");
        NKECS_ASSERT(!sInstance && "Only one NkEngineLayer allowed");
        sInstance = this;

        logger.Infof("[NkEngineLayer] Initialisation...\n");

        // ── 1. Renderer ───────────────────────────────────────────────────
        InitRenderer();

        // ── 2. Systèmes ECS ───────────────────────────────────────────────
        RegisterCoreSystems();

        // ── 3. Scheduler ─────────────────────────────────────────────────
        mScheduler.Init(mWorld);

        logger.Infof("[NkEngineLayer] Prêt — %u systèmes enregistrés\n",
                     mScheduler.SystemCount());
    }

    // =========================================================================
    // OnDetach — Shutdown propre dans l'ordre inverse
    // =========================================================================
    void NkEngineLayer::OnDetach() {
        NK_PROFILE_SCOPE("NkEngineLayer::OnDetach");

        // Décharger la scène courante
        if (auto* scene = mSceneMgr.GetCurrent()) {
            scene->EndPlay();
        }

        ShutdownRenderer();
        sInstance = nullptr;
        logger.Infof("[NkEngineLayer] Shutdown complet\n");
    }

    // =========================================================================
    // OnUpdate — Boucle principale
    // =========================================================================
    void NkEngineLayer::OnUpdate(float dt) {
        NK_PROFILE_SCOPE("NkEngineLayer::Update");

        // Mise à jour du scheduler (tous les groupes sauf Render et FixedUpdate)
        mScheduler.Run(mWorld, dt);

        // Transitions de scène (fade, chargement)
        mSceneMgr.Update(dt);
    }

    // =========================================================================
    // OnFixedUpdate — Physique à taux fixe
    // =========================================================================
    void NkEngineLayer::OnFixedUpdate(float fixedDt) {
        NK_PROFILE_SCOPE("NkEngineLayer::FixedUpdate");
        mScheduler.RunFixed(mWorld, fixedDt);
    }

    // =========================================================================
    // OnRender — Soumission des commandes GPU
    // =========================================================================
    void NkEngineLayer::OnRender() {
        NK_PROFILE_SCOPE("NkEngineLayer::Render");

        if (!mRendererInitialized) return;

        auto& app = NkApplication::Get();
        auto* cmd = app.GetCmd();
        if (!cmd) return;

        // Mise à jour du command buffer courant dans le système de rendu
        mRenderSystem.SetCommandBuffer(cmd);

        // Le NkRenderSystem est exécuté par le scheduler groupe Render
        // Il a déjà été appellé dans mScheduler.Run() si le groupe Render est inclus
        // Ici on s'assure que les stats sont disponibles après coup
        const auto& stats = mRenderSystem.GetStats();
        (void)stats; // Utilisé par l'overlay debug
    }

    // =========================================================================
    // OnEvent
    // =========================================================================
    bool NkEngineLayer::OnEvent(NkEvent* event) {
        (void)event;
        // Future: dispatch vers NkUISystem, NkInputSystem
        return false;
    }

    // =========================================================================
    // InitRenderer
    // =========================================================================
    void NkEngineLayer::InitRenderer() noexcept {
        auto& app = NkApplication::Get();
        auto* device = app.GetDevice();

        if (!device) {
            logger.Errorf("[NkEngineLayer] Pas de device GPU disponible\n");
            return;
        }

        const auto& cfg = app.GetConfig();
        mResizeW = cfg.windowConfig.width;
        mResizeH = cfg.windowConfig.height;

        renderer::NkRendererConfig rendCfg;
        rendCfg.width          = mResizeW;
        rendCfg.height         = mResizeH;
        rendCfg.shadowMapSize  = 2048;
        rendCfg.enableShadows  = true;
        rendCfg.msaa           = renderer::NkMSAA::NK_4X;

        if (!mRenderer.Init(device, rendCfg)) {
            logger.Errorf("[NkEngineLayer] Échec init NkRenderer\n");
            return;
        }

        mRendererInitialized = true;

        // Lie le RenderSystem au renderer
        mRenderSystem.Init(&mRenderer, app.GetCmd());

        logger.Infof("[NkEngineLayer] NkRenderer initialisé (%ux%u)\n",
                     mResizeW, mResizeH);
    }

    // =========================================================================
    // RegisterCoreSystems — Ordre critique
    // =========================================================================
    void NkEngineLayer::RegisterCoreSystems() noexcept {
        // PreUpdate — Transform avant tout
        mScheduler.AddSystem<ecs::NkTransformSystem>();

        // PreUpdate — Behaviours (OnStart → OnUpdate)
        mScheduler.AddSystem<ecs::NkBehaviourSystem>();
        mScheduler.AddSystem<ecs::NkBehaviourLateSystem>();
        mScheduler.AddSystem<ecs::NkBehaviourFixedSystem>();

        // Update — Scripts C++ natifs
        mScheduler.AddSystem<ecs::NkScriptSystem>();

        // Render — Bridge ECS → GPU (si renderer disponible)
        if (mRendererInitialized) {
            // NkRenderSystem est enregistré directement (pas via template car déjà créé)
            // Utiliser AddSystem avec pointeur owné
            mScheduler.AddSystem<NkRenderSystem>();
        }

        logger.Infof("[NkEngineLayer] %u systèmes core enregistrés\n",
                     mScheduler.SystemCount());
    }

    // =========================================================================
    // ShutdownRenderer
    // =========================================================================
    void NkEngineLayer::ShutdownRenderer() noexcept {
        if (mRendererInitialized) {
            mRenderer.Shutdown();
            mRendererInitialized = false;
        }
    }

} // namespace nkentseu