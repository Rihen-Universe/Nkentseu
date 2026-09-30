#pragma once
// =============================================================================
// Nkentseu/Core/NkEngineLayer.h — v2
// =============================================================================
// Layer qui initialise et coordonne tous les sous-systèmes du moteur.
//
// CYCLE DE VIE :
//   OnAttach()      → Init dans l'ordre : Assets, Renderer, Scheduler, Scènes
//   OnUpdate(dt)    → mInput.Update + mScheduler.Run(world, dt) + mSceneMgr.Update(dt)
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
//
// ENTREES DU JEU (30/09) :
//   La couche porte la carte d'entree du joueur 1 (NkInputMap, NKEvent) : le
//   MEME systeme qu'Unkeny. Elle la nourrit (OnEvent) et l'avance (OnUpdate,
//   AVANT les systemes : ils lisent l'entree de CETTE image). Un jeu declare
//   ses actions et ses liaisons, en code ou par un fichier texte :
//       auto &in = NkEngineLayer::Get().GetInput();
//       in.Load(texte);   // ou LoadInputFile("entrees.nkinput")
//       if (in.WasPressed(in.FindAction("Sauter"))) { ... }
//   Les joueurs 2..4 sont d'autres NkInputMap, que le jeu nourrit de meme.
// =============================================================================

#include "../Core/NkLayer.h"
#include "../Core/NkApplication.h"
#include "../Core/NkProfiler.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/System/NkScheduler.h"
#include "Noge/ECS/Scene/NkSceneManager.h"
#include "Noge/ECS/Scene/NkSceneLifecycleSystem.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"
#include "Noge/ECS/Entities/NkBehaviourSystem.h"
#include "Noge/ECS/Scripting/NkScriptSystem.h"
#include "Noge/ECS/Systems/NkRenderSystem.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"
#include "NKEvent/NkInputMap.h"

namespace nkentseu {

	// =========================================================================
	// NkEngineLayer
	// =========================================================================
	class NkEngineLayer : public NkLayer {
		public:
			// ── Constructeur/Destructeur ──────────────────────────────────
			explicit NkEngineLayer() noexcept : NkLayer("NkEngineLayer"), mSceneMgr(mWorld) {
			}

			~NkEngineLayer() noexcept override = default;

			// ── NkLayer lifecycle ─────────────────────────────────────────
			void OnAttach() override;
			void OnDetach() override;
			void OnUpdate(float dt) override;
			void OnFixedUpdate(float fixedDt) override;
			void OnRender() override;
			bool OnEvent(NkEvent *event) override;

			// ── Accès global singleton ────────────────────────────────────
			[[nodiscard]] static NkEngineLayer &Get() noexcept {
				NKECS_ASSERT(sInstance && "NkEngineLayer not attached");
				return *sInstance;
			}

			[[nodiscard]] static bool IsReady() noexcept {
				return sInstance != nullptr;
			}

			// ── Accès aux sous-systèmes ────────────────────────────────────
			[[nodiscard]] ecs::NkWorld &GetWorld() noexcept {
				return mWorld;
			}

			[[nodiscard]] ecs::NkScheduler &GetScheduler() noexcept {
				return mScheduler;
			}

			[[nodiscard]] ecs::NkSceneManager &GetSceneManager() noexcept {
				return mSceneMgr;
			}

			[[nodiscard]] NkRenderSystem &GetRenderSystem() noexcept {
				return mRenderSystem;
			}

			[[nodiscard]] renderer::NkRenderer *GetRenderer() noexcept {
				return mRenderer;
			}

			/// La carte d'entree du joueur 1 (voir « ENTREES DU JEU » en tete).
			[[nodiscard]] NkInputMap &GetInput() noexcept {
				return mInput;
			}

			/// Lit un fichier texte d'entrees (format de NkInputMap::Load). Rend
			/// le rapport : un fichier absent est une erreur NOMMEE, pas un silence.
			NkInputMapReport LoadInputFile(const char *path);
			/// Ecrit la carte (NkInputMap::Save). false si l'ecriture echoue.
			bool SaveInputFile(const char *path) const;

			// ── Raccourcis scène ──────────────────────────────────────────
			/**
			 * @brief Enregistre une factory de scène.
			 */
			void RegisterScene(const NkString &name, ecs::NkSceneFactory factory) noexcept {
				mSceneMgr.Register(name, static_cast<ecs::NkSceneFactory &&>(factory));
			}

			/**
			 * @brief Charge une scène et enregistre son lifecycle dans le scheduler.
			 * @return true si le chargement a réussi.
			 */
			bool LoadScene(const NkString &name,
						   const ecs::NkSceneTransition &t = ecs::NkSceneTransition::Instant()) noexcept {
				bool ok = mSceneMgr.LoadScene(name, t);
				if (ok) {
					auto *scene = mSceneMgr.GetCurrent();
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
			[[nodiscard]] ecs::NkSceneGraph *GetCurrentScene() noexcept {
				return mSceneMgr.GetCurrent();
			}

			// ── Utilitaires ───────────────────────────────────────────────
			/**
			 * @brief Redimensionne le renderer (appel depuis OnResize de l'app).
			 */
			void Resize(nk_uint32 w, nk_uint32 h) noexcept {
				// Le resize du renderer est piloté par NkApplication (propriétaire).
				// On mémorise juste la taille pour les systèmes ECS.
				mResizeW = w;
				mResizeH = h;
			}

		private:
			// ── Init helpers ──────────────────────────────────────────────
			void InitRenderer() noexcept;
			void RegisterCoreSystems() noexcept;
			void ShutdownRenderer() noexcept;

			// ── État ──────────────────────────────────────────────────────
			ecs::NkWorld mWorld;
			ecs::NkScheduler mScheduler;
			ecs::NkSceneManager mSceneMgr;

			NkInputMap mInput; ///< entrees du joueur 1, nourries par OnEvent, avancees par OnUpdate
			renderer::NkRenderer *mRenderer = nullptr; // EMPRUNTÉ à NkApplication (non possédé)
			NkRenderSystem mRenderSystem;

			bool mRendererInitialized = false;
			nk_uint32 mResizeW = 1280;
			nk_uint32 mResizeH = 720;

			static NkEngineLayer *sInstance;
	};

	// Définition du singleton (dans le .cpp)
	// inline NkEngineLayer* NkEngineLayer::sInstance = nullptr;

} // namespace nkentseu