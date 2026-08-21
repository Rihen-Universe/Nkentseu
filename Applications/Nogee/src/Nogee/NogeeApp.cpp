#include "NogeeApp.h"
#include "NKLogger/NkLog.h"
#include "Noge/ECS/Scene/NkSceneManager.h"

namespace nkentseu {
	namespace noge {

		NogeApp::NogeApp(const NogeAppConfig &config) : NkApplication(config.appConfig), mUkConfig(config) {
		}

		NogeApp::~NogeApp() = default;

		// =====================================================================
		void NogeApp::OnInit() {
			logger.Infof("[NogeApp] Init — {}\n", mUkConfig.windowTitle.CStr());

			NkIDevice *dev = GetDevice();
			NkICommandBuffer *cmd = GetCmd();
			NkGraphicsApi api = GetConfig().deviceInfo.api;

			// ── Créer les layers dans l'ordre ─────────────────────────────────
			// 1. EditorLayer : systèmes éditeur (sélection, undo, assets, gizmos)
			auto *editor = new EditorLayer("EditorLayer", dev, cmd);
			// 2. ViewportLayer : FBO offscreen + rendu scène
			auto *viewport = new ViewportLayer("ViewportLayer", dev, cmd);
			// 3. UILayer (Overlay) : NKUI par-dessus tout
			auto *ui = new UILayer("UILayer", dev, cmd, api);

			// On connecte directement les pointeurs après empilement
			// (les layers vivent dans LayerStack — ownership)
			mEditorLayer = editor;
			mViewportLayer = viewport;
			mUILayer = ui;

			// Injecter la caméra éditeur dans ViewportLayer.
			// La caméra est partagée : propriété de NogeApp.
			mEditorCamera = new NkEditorCamera();
			viewport->SetEditorCamera(mEditorCamera);
			viewport->SetGizmoSystem(&editor->GetGizmoSystem());
			viewport->SetSelectionManager(&editor->GetSelectionManager());

			// UILayer reçoit toutes les références
			ui->SetEditorLayer(editor);
			ui->SetViewportLayer(viewport);

			// ── MONDE ECS — le câblage qui manquait (2026-08-17) ──────────────
			// `SetWorld`/`SetScene` existaient depuis toujours et n'étaient
			// appelés NULLE PART : `UILayer::mWorld` restait nul, et les panneaux
			// d'arbre et d'inspecteur sortaient immédiatement de leur `Render`.
			// Voir ROADMAP Noge §10quater pour la mesure.
			mScene = new ecs::NkSceneGraph(mWorld, "Scene");

			// ⚠️ DEUX FABRIQUES D'ENTITÉS AUX COMPOSANTS DISJOINTS — mesuré :
			//   SpawnNode            -> NkSceneNode, NkParent, NkChildren,
			//                           NkLocalTransform, NkWorldTransform
			//   NkGameObjectFactory  -> NkName, NkTag, NkTransform, NkParent,
			//                           NkChildren, NkBehaviourHost
			// Intersection : NkParent et NkChildren seulement. Or les panneaux
			// lisent DES DEUX CÔTÉS — l'arbre lit `NkSceneNode::name`, le panneau
			// de propriétés lit `NkName` et `NkTransform`. Aucune des deux
			// fabriques ne suffit donc à elle seule à peupler les deux panneaux.
			// On complète explicitement plutôt que d'en choisir une : c'est le
			// seul moyen que le témoin traverse réellement les deux lectures.
			// (Incohérence signalée, PAS corrigée : elle appartient à Noge.)
			{
				const ecs::NkEntityId racine = mScene->SpawnNode("TEMOIN_Racine");
				const ecs::NkEntityId enfantA = mScene->SpawnNode("TEMOIN_Enfant_A");
				const ecs::NkEntityId enfantB = mScene->SpawnNode("TEMOIN_Enfant_B");
				mScene->SetParent(enfantA, racine);
				mScene->SetParent(enfantB, racine);

				const ecs::NkEntityId ids[3] = {racine, enfantA, enfantB};
				const char *noms[3] = {"TEMOIN_Racine", "TEMOIN_Enfant_A", "TEMOIN_Enfant_B"};
				for (int i = 0; i < 3; ++i) {
					mWorld.Add<ecs::NkName>(ids[i], ecs::NkName(noms[i]));
					mWorld.Add<ecs::NkTransform>(ids[i]);
				}
				logger.Infof("[NogeApp] Monde ECS : 3 entites TEMOIN_* creees\n");
			}

			editor->SetScene(&mWorld, mScene);
			viewport->SetWorld(&mWorld);
			ui->SetWorld(&mWorld);
			ui->SetScene(mScene);

			// ── Empilement ────────────────────────────────────────────────────
			PushLayer(editor);
			PushLayer(viewport);
			PushOverlay(ui);

			// ── Projet de démarrage ───────────────────────────────────────────
			if (!mUkConfig.startupProjectPath.Empty()) {
				if (editor->GetProjectManager().Load(mUkConfig.startupProjectPath.CStr())) {
					logger.Infof("[NogeApp] Projet chargé: {}\n", mUkConfig.startupProjectPath.CStr());
				}
			}
		}

		// =====================================================================
		void NogeApp::OnStart() {
			// Scène de démonstration vide si aucun projet
			// (Une vraie scène est chargée par NkSceneManager depuis OnInit/projet)
		}

		void NogeApp::OnUpdate(float dt) {
			// Les layers ont leur propre OnUpdate dans la LayerStack
			(void)dt;
		}

		void NogeApp::OnRender() {
			// ViewportLayer::OnRender() gère le FBO
		}

		void NogeApp::OnUIRender() {
			// UILayer::OnUIRender() gère NKUI
		}

		void NogeApp::OnShutdown() {
			delete mEditorCamera;
			mEditorCamera = nullptr;
			// Le graphe de scene AVANT le monde : il en tient une reference.
			delete mScene;
			mScene = nullptr;
			logger.Infof("[NogeApp] Shutdown\n");
		}

		void NogeApp::OnClose() {
			// Auto-save si modifié
			if (mEditorLayer && mEditorLayer->GetProjectManager().IsModified()) {
				mEditorLayer->GetProjectManager().Save();
			}
			Quit();
		}

		void NogeApp::OnResize(nk_uint32 w, nk_uint32 h) {
			// Le FBO se redimensionne automatiquement via UILayer::ComputeLayout()
			(void)w;
			(void)h;
		}

	} // namespace noge
} // namespace nkentseu
