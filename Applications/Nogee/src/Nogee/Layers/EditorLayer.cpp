#include "EditorLayer.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace noge {

		EditorLayer::EditorLayer(const NkString &name, NkIDevice *device, NkICommandBuffer *cmd) noexcept
			: NkLayer(name), mDevice(device), mCmd(cmd) {
		}

		EditorLayer::~EditorLayer() = default;

		// =====================================================================
		void EditorLayer::OnAttach() {
			logger.Infof("[EditorLayer] Attaché\n");

			// Initialiser AssetManager avec le device
			mAssets.Init(mDevice, ".");

			// Log des changements de sélection (callback direct — cf.
			// NkSelectionManager : pas de bus d'événements pour ce type applicatif)
			mSel.SetOnChanged(
				[](void * /*user*/, const NkSelectionChangedEvent &e) {
					if (e.primary.IsValid())
						logger.Infof("[Editor] Sélection: entité {}.{}\n", e.primary.index, e.primary.gen);
				},
				this);

			logger.Infof("[EditorLayer] Systèmes initialisés\n");
		}

		void EditorLayer::OnDetach() {
			mSel.SetOnChanged(nullptr, nullptr);
			logger.Infof("[EditorLayer] Détaché\n");
		}

		// =====================================================================
		void EditorLayer::OnUpdate(float dt) {
			// Hot-reload des assets toutes les 2 secondes
			mHotReloadTimer += dt;
			if (mHotReloadTimer >= 2.f) {
				mHotReloadTimer = 0.f;
				mAssets.ProcessHotReload();
			}

			// Les gizmos sont mis à jour depuis ViewportLayer::OnUpdate()
			// car il faut connaître l'état souris dans le viewport.
		}

		void EditorLayer::OnFixedUpdate(float /*fdt*/) {
		}

		// =====================================================================
		bool EditorLayer::OnEvent(NkEvent *event) {
			if (!event)
				return false;
			// Raccourcis clavier — reçus via le flux d'événements des layers
			// (NkApplication::DispatchToLayers), pas de souscription EventBus.
			if (auto *kp = event->As<NkKeyPressEvent>()) {
				HandleShortcuts(kp);
			}
			return false; // ne consomme pas — UILayer peut aussi le recevoir
		}

		// =====================================================================
		void EditorLayer::HandleShortcuts(const NkKeyPressEvent *kp) noexcept {
			if (!kp)
				return;

			NkKey key = kp->GetKey();
			bool ctrl = kp->HasCtrl();
			bool noMod = !ctrl && !kp->HasShift() && !kp->HasAlt();

			// Ctrl+Z : Undo
			if (ctrl && key == NkKey::NK_Z) {
				if (mHistory.CanUndo()) {
					logger.Infof("[Editor] Undo: {}\n", mHistory.UndoName().CStr());
					mHistory.Undo();
				}
				return;
			}

			// Ctrl+Y : Redo
			if (ctrl && key == NkKey::NK_Y) {
				if (mHistory.CanRedo()) {
					logger.Infof("[Editor] Redo: {}\n", mHistory.RedoName().CStr());
					mHistory.Redo();
				}
				return;
			}

			// Ctrl+S : Save projet
			if (ctrl && key == NkKey::NK_S) {
				if (mProject.IsOpen()) {
					mProject.Save();
					mHistory.MarkSaved();
					logger.Infof("[Editor] Projet sauvegardé\n");
				}
				return;
			}

			// F5 : Play/Stop
			if (noMod && key == NkKey::NK_F5) {
				if (mPlaying)
					Stop();
				else
					Play();
				return;
			}

			// W/E/R : modes gizmo (seulement si viewport est focalisé)
			if (noMod && key == NkKey::NK_W) {
				mGizmos.mode = NkGizmoMode::Translate;
				return;
			}
			if (noMod && key == NkKey::NK_E) {
				mGizmos.mode = NkGizmoMode::Rotate;
				return;
			}
			if (noMod && key == NkKey::NK_R && !mPlaying) {
				mGizmos.mode = NkGizmoMode::Scale;
				return;
			}

			// Delete / Backspace : supprimer l'entité sélectionnée
			if (noMod && (key == NkKey::NK_DELETE || key == NkKey::NK_BACK)) {
				DeleteSelectedEntity();
				return;
			}

			// G : toggle espace gizmo World/Local
			if (noMod && key == NkKey::NK_G) {
				mGizmos.space = (mGizmos.space == NkGizmoSpace::World) ? NkGizmoSpace::Local : NkGizmoSpace::World;
				return;
			}
		}

		void EditorLayer::DeleteSelectedEntity() noexcept {
			if (!mWorld || !mSel.HasSelection())
				return;
			ecs::NkEntityId id = mSel.Primary();
			if (!mWorld->IsAlive(id))
				return;

			// Mémoriser le nom pour le log
			NkString name = "Entity";
			if (auto *n = mWorld->Get<ecs::NkName>(id))
				name = NkString(n->Get());

			mSel.Clear();

			// Destruction différée (sûre pendant l'itération)
			mWorld->DestroyDeferred(id);

			logger.Infof("[Editor] Entité supprimée: {}\n", name.CStr());
		}

		// =====================================================================
		void EditorLayer::Play() noexcept {
			if (mPlaying)
				return;
			mPlaying = true;
			if (mScene)
				mScene->BeginPlay();
			logger.Infof("[Editor] Play\n");
		}

		void EditorLayer::Stop() noexcept {
			if (!mPlaying)
				return;
			mPlaying = false;
			if (mScene)
				mScene->EndPlay();
			logger.Infof("[Editor] Stop\n");
		}

		void EditorLayer::Pause() noexcept {
			if (!mPlaying)
				return;
			if (mScene)
				mScene->Pause();
			logger.Infof("[Editor] Pause\n");
		}

	} // namespace noge
} // namespace nkentseu
