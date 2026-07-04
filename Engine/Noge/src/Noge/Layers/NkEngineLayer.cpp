// =============================================================================
// Nkentseu/Core/NkEngineLayer.cpp
// =============================================================================
#include "NkEngineLayer.h"
#include "../Core/NkApplication.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"
#include "Noge/ECS/Systems/NkParticleSystem.h"
#include "Noge/ECS/Entities/NkBehaviourSystem.h"
#include "Noge/ECS/Scripting/NkScriptSystem.h"
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

        // Le renderer 2D/3D est CRÉÉ et POSSÉDÉ par NkApplication. NkEngineLayer
        // l'emprunte simplement et y branche le pont ECS (NkRenderSystem).
        mRenderer = app.GetRenderer();
        if (!mRenderer) {
            logger.Errorf("[NkEngineLayer] NkApplication n'a pas de renderer\n");
            return;
        }
        mRendererInitialized = true;

        // Lie le RenderSystem au renderer de l'application.
        mRenderSystem.Init(mRenderer, app.GetCmd());

        logger.Infof("[NkEngineLayer] NkRenderer branché (%ux%u)\n",
                     mResizeW, mResizeH);
    }

    // =========================================================================
    // RegisterCoreSystems — Ordre critique
    // =========================================================================
    void NkEngineLayer::RegisterCoreSystems() noexcept {
        // PreUpdate — Transform avant tout
        mScheduler.AddSystem<NkTransformSystem>();

        // PreUpdate — Behaviours (OnStart → OnUpdate)
        mScheduler.AddSystem<ecs::NkBehaviourSystem>();
        mScheduler.AddSystem<ecs::NkBehaviourLateSystem>();
        mScheduler.AddSystem<ecs::NkBehaviourFixedSystem>();

        // FixedUpdate — Physique rigide (pont ECS -> NKCollision/NKPhysics)
        mScheduler.AddSystem<NkPhysicsSystem>();

        // Update — Scripts C++ natifs
        mScheduler.AddSystem<ecs::NkScriptSystem>();

        // Render + VFX — ponts ECS → NKRenderer (si renderer disponible).
        // AddSystem<T>() retourne l'instance OWNED par le scheduler : c'est ELLE
        // qu'il faut Init (et non un membre séparé) pour qu'elle s'exécute.
        if (mRendererInitialized && mRenderer) {
            NkICommandBuffer* cmd = NkApplication::Get().GetCmd();
            NkRenderSystem&   rs  = mScheduler.AddSystem<NkRenderSystem>();
            rs.Init(mRenderer, cmd);

            NkParticleSystem& ps = mScheduler.AddSystem<NkParticleSystem>();
            ps.Init(mRenderer);
        }

        logger.Infof("[NkEngineLayer] %u systèmes core enregistrés\n",
                     mScheduler.SystemCount());
    }

    // =========================================================================
    // ShutdownRenderer
    // =========================================================================
    void NkEngineLayer::ShutdownRenderer() noexcept {
        // Le renderer est possédé par NkApplication : on relâche juste l'emprunt.
        mRenderer = nullptr;
        mRendererInitialized = false;
    }

} // namespace nkentseu