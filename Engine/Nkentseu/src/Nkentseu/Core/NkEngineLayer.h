#pragma once
// =============================================================================
// Engine/Nkentseu/Core/NkEngineLayer.h
// =============================================================================
// Layer applicatif qui intègre :
//   - NkWorld          : registre ECS
//   - NkSceneManager   : gestion des scènes
//   - NkScheduler      : ordonnancement des systèmes
//   - NkTransformSystem: propagation des transforms
//   - NkUIInputBridge  : pont NkEvent → NKUI (réutilisé de Unkeny)
//
// Usage :
//   class MyApp : public Application {
//       void OnInit() override {
//           PushLayer(new NkEngineLayer());
//       }
//   };
//
// Depuis n'importe où :
//   NkEngineLayer& eng = NkEngineLayer::Get();
//   eng.GetSceneManager().LoadScene("MainMenu");
//   eng.GetWorld().Query<NkTransformComponent>()...
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "Nkentseu/Core/Application.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/Scene/NkSceneGraph.h"
#include "NKECS/Scene/NkSceneManager.h"
#include "NKECS/Scene/NkSceneLifecycleSystem.h"
#include "NKECS/System/NkScheduler.h"
#include "NKECS/Systems/NkTransformSystem.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {

    class NkEngineLayer : public Layer {
    public:
        NkEngineLayer() noexcept
            : Layer("NkEngineLayer")
            , mSceneMgr(mWorld) {}

        ~NkEngineLayer() override = default;

        // ── Layer lifecycle ───────────────────────────────────────────────────
        void OnAttach() override {
            sInstance = this;

            // Systèmes de base
            mScheduler.AddSystem(new ecs::NkTransformSystem());

            // Lifecycle de scène — délègue à la scène courante du manager
            // Les systèmes lifecycle sont ajoutés après le premier LoadScene()
            // via RegisterSceneLifecycle().

            mScheduler.Init(mWorld);
            logger.Infof("[NkEngineLayer] Initialisé\n");
        }

        void OnDetach() override {
            sInstance = nullptr;
        }

        void OnUpdate(float dt) override {
            mScheduler.Run(mWorld, dt);
            mSceneMgr.Update(dt);
        }

        void OnFixedUpdate(float fixedDt) override {
            (void)fixedDt;
            // NkPhysicsSystem + fixed scheduler ici (Phase 3)
        }

        bool OnEvent(NkEvent* event) override {
            (void)event;
            return false;
        }

        // ── Accès ─────────────────────────────────────────────────────────────
        [[nodiscard]] ecs::NkWorld&        GetWorld()         noexcept { return mWorld; }
        [[nodiscard]] ecs::NkSceneManager& GetSceneManager()  noexcept { return mSceneMgr; }
        [[nodiscard]] ecs::NkScheduler&    GetScheduler()     noexcept { return mScheduler; }

        static NkEngineLayer& Get() {
            NK_ASSERT_MSG(sInstance, "NkEngineLayer non initialisé");
            return *sInstance;
        }

        static bool IsReady() noexcept { return sInstance != nullptr; }

        // ── Raccourcis pratiques ──────────────────────────────────────────────

        // Charge une scène + enregistre son lifecycle dans le scheduler
        bool LoadScene(const NkString& name,
                       const ecs::NkSceneTransition& t =
                           ecs::NkSceneTransition::Instant()) noexcept {
            bool ok = mSceneMgr.LoadScene(name, t);
            if (ok && mSceneMgr.GetCurrent()) {
                ecs::RegisterSceneLifecycle(mScheduler, mSceneMgr.GetCurrent());
            }
            return ok;
        }

        // Enregistre une factory de scène
        void RegisterScene(const NkString& name,
                           ecs::NkSceneFactory factory) noexcept {
            mSceneMgr.Register(name, static_cast<ecs::NkSceneFactory&&>(factory));
        }

    private:
        ecs::NkWorld        mWorld;
        ecs::NkSceneManager mSceneMgr;
        ecs::NkScheduler    mScheduler;

        static NkEngineLayer* sInstance;
    };

    inline NkEngineLayer* NkEngineLayer::sInstance = nullptr;

} // namespace nkentseu
