#pragma once
// =============================================================================
// Nkentseu/Core/NkEngineLayer.h — v2
// =============================================================================
// Layer qui initialise et coordonne tous les sous-systèmes du moteur.
//
// CYCLE DE VIE :
//   OnAttach()      → Init dans l'ordre : Assets, Renderer, Scheduler, Scènes
//   OnUpdate(dt)    → mScheduler.Run(world, dt) + mSceneMgr.Update(dt)
//   OnFixedUpdate() → mScheduler.RunFixed(world, fdt) [physique]
//   OnRender()      → NkRenderSystem exécuté via Scheduler groupe Render
//   OnDetach()      → Shutdown propre dans l'ordre inverse
//
// ACCÈS GLOBAL :
//   NkEngineLayer::Get() — depuis n'importe où dans le code applicatif
//
// USAGE TYPE :
//   class MyApp : public NkApplication {
//     void OnInit() override { PushLayer(new NkEngineLayer()); }
//     void OnStart() override {
//       auto& eng = NkEngineLayer::Get();
//       eng.RegisterScene("Main", MakeMainScene);
//       eng.LoadScene("Main");
//     }
//   };
// =============================================================================

#include "NkLayer.h"
#include "NkApplication.h"
#include "NkProfiler.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/System/NkScheduler.h"
#include "Nkentseu/ECS/Scene/NkSceneManager.h"
#include "Nkentseu/ECS/Scene/NkSceneLifecycleSystem.h"
#include "Nkentseu/ECS/Systems/NkTransformSystem.h"
#include "Nkentseu/ECS/Entities/NkBehaviourSystem.h"
#include "Nkentseu/ECS/Scripting/NkScriptSystem.h"
#include "Nkentseu/Renderer/NkRenderSystem.h"
#include "NKRenderer/src/NkRenderer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // =========================================================================
    // NkEngineLayer
    // =========================================================================
    class NkEngineLayer : public NkLayer {
        public:
            // ── Constructeur/Destructeur ──────────────────────────────────
            explicit NkEngineLayer() noexcept
                : NkLayer("NkEngineLayer")
                , mSceneMgr(mWorld)
            {}

            ~NkEngineLayer() noexcept override = default;

            // ── NkLayer lifecycle ─────────────────────────────────────────
            void OnAttach() override;
            void OnDetach() override;
            void OnUpdate(float dt) override;
            void OnFixedUpdate(float fixedDt) override;
            void OnRender() override;
            bool OnEvent(NkEvent* event) override;

            // ── Accès global singleton ────────────────────────────────────
            [[nodiscard]] static NkEngineLayer& Get() noexcept {
                NKECS_ASSERT(sInstance && "NkEngineLayer not attached");
                return *sInstance;
            }
            [[nodiscard]] static bool IsReady() noexcept {
                return sInstance != nullptr;
            }

            // ── Accès aux sous-systèmes ────────────────────────────────────
            [[nodiscard]] ecs::NkWorld&            GetWorld()        noexcept { return mWorld; }
            [[nodiscard]] ecs::NkScheduler&        GetScheduler()    noexcept { return mScheduler; }
            [[nodiscard]] ecs::NkSceneManager&     GetSceneManager() noexcept { return mSceneMgr; }
            [[nodiscard]] NkRenderSystem&          GetRenderSystem() noexcept { return mRenderSystem; }
            [[nodiscard]] renderer::NkRenderer&    GetRenderer()     noexcept { return mRenderer; }

            // ── Raccourcis scène ──────────────────────────────────────────
            /**
             * @brief Enregistre une factory de scène.
             */
            void RegisterScene(const NkString& name,
                               ecs::NkSceneFactory factory) noexcept {
                mSceneMgr.Register(name, static_cast<ecs::NkSceneFactory&&>(factory));
            }

            /**
             * @brief Charge une scène et enregistre son lifecycle dans le scheduler.
             * @return true si le chargement a réussi.
             */
            bool LoadScene(const NkString& name,
                           const ecs::NkSceneTransition& t =
                               ecs::NkSceneTransition::Instant()) noexcept {
                bool ok = mSceneMgr.LoadScene(name, t);
                if (ok) {
                    auto* scene = mSceneMgr.GetCurrent();
                    if (scene) {
                        ecs::RegisterSceneLifecycle(mScheduler, scene);
                        scene->BeginPlay();
                    }
                }
                return ok;
            }

            /**
             * @brief Accès direct à la scène courante.
             */
            [[nodiscard]] ecs::NkSceneGraph* GetCurrentScene() noexcept {
                return mSceneMgr.GetCurrent();
            }

            // ── Utilitaires ───────────────────────────────────────────────
            /**
             * @brief Redimensionne le renderer (appel depuis OnResize de l'app).
             */
            void Resize(nk_uint32 w, nk_uint32 h) noexcept {
                if (mRendererInitialized) {
                    mRenderer.Resize(w, h);
                    mResizeW = w;
                    mResizeH = h;
                }
            }

        private:
            // ── Init helpers ──────────────────────────────────────────────
            void InitRenderer() noexcept;
            void RegisterCoreSystems() noexcept;
            void ShutdownRenderer() noexcept;

            // ── État ──────────────────────────────────────────────────────
            ecs::NkWorld          mWorld;
            ecs::NkScheduler      mScheduler;
            ecs::NkSceneManager   mSceneMgr;

            renderer::NkRenderer  mRenderer;
            NkRenderSystem        mRenderSystem;

            bool                  mRendererInitialized = false;
            nk_uint32             mResizeW = 1280;
            nk_uint32             mResizeH = 720;

            static NkEngineLayer* sInstance;
    };

    // Définition du singleton (dans le .cpp)
    // inline NkEngineLayer* NkEngineLayer::sInstance = nullptr;

} // namespace nkentseu