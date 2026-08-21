#pragma once
// =============================================================================
// Nogee/Panels/ViewportPanel.h — la zone centrale, cible du glisser-deposer §9
// =============================================================================
// CE QUE CE PANNEAU EST : le MINIMUM qui donne une cible au glisser d'assets
// (§9 « glisser une carte vers le viewport ») et occupe la zone CENTRE du dock.
//
// CE QU'IL N'EST PAS : un viewport. Le rendu de scene n'existe pas encore sur
// ce chemin — c'est MESURE, pas suppose : `ViewportLayer::RenderScene()` et
// `RenderGizmos()` etaient des TODO en toutes lettres quand la coupe NKUI a ete
// faite (commit 16732511, ROADMAP §10sexies), le viewport NKUI ne rendait RIEN.
// Ce panneau le DIT a l'ecran plutot que de le simuler.
//
// La livraison d'un asset est un JOURNAL (`MESURE : charge livree`) + un
// affichage du dernier chemin recu — ET, depuis le PALIER A (2026-08-17,
// autorise par le guide), un SPAWN DANS LE MONDE ECS quand l'asset est un
// MESH : `SpawnNode(nom du fichier)` + NkName + NkTransform +
// NkMeshComponent{meshPath}. L'entite apparait dans l'Outliner et le Details
// — visible LA, pas encore a l'ecran 3D (palier B : rendu de scene, chiffre,
// attend Rodolf — « Layers ou fonction libre »). Le mesh lui-meme n'est PAS
// charge ici : NkRenderSystem::Execute l'importe paresseusement le jour ou il
// tourne (NkMeshSystem::Import, l.153) — AssetManager n'a rien a faire, on
// n'invente pas un poste.
// Un asset d'un autre type (texture, audio, inconnu) est LIVRE et JOURNALISE
// mais n'instancie rien : instancier une texture comme entite serait faux.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Nogee/Editor/NkSelectionManager.h"

namespace nkentseu {
	namespace noge {

		class ViewportPanel final : public editorkit::NkEditorPanel {
			public:
				ViewportPanel() noexcept
					: editorkit::NkEditorPanel("Viewport", editorkit::NkEditorDockSide::NK_CENTER) {
				}

				// Palier A : le monde ou spawner, la racine projet pour rendre le
				// chemin de la charge (relatif au projet) ABSOLU dans meshPath —
				// NkMeshSystem::Import ouvre depuis le cwd, pas depuis le projet.
				// Sans Bind, la livraison reste un journal (comportement d'avant).
				void Bind(ecs::NkWorld *world, ecs::NkSceneGraph *scene, NkSelectionManager *sel,
						  const char *projectDir) noexcept;

				void OnUI(editorkit::NkEditorFrameContext &ec) override;

				// ── SONDE drag-drop (--dragdrop-test) ─────────────────────────────
				void EnableProbe(bool on) noexcept {
					mProbeEnabled = on;
				}
				// Rect ECRAN reel de la zone de depot, mesure au dessin.
				bool ProbeRect(nkgui::NkRect *out) const noexcept {
					if (!mProbeRectValid)
						return false;
					if (out)
						*out = mProbeRect;
					return true;
				}
				// Dernier chemin d'asset livre ("" si aucun) + compteur de
				// livraisons — c'est ce que la sonde verifie, dans les deux sens.
				const char *LastDroppedPath() const noexcept {
					return mLastDropPath;
				}
				nk_int32 DropCount() const noexcept {
					return mDropCount;
				}
				// Palier A : derniere entite instanciee par un depot (Invalid si
				// aucune) + compteur — la sonde verifie qu'un depot MESH en cree
				// UNE et qu'un depot non-mesh / hors cible n'en cree AUCUNE.
				ecs::NkEntityId LastSpawned() const noexcept {
					return mLastSpawned;
				}
				nk_int32 SpawnCount() const noexcept {
					return mSpawnCount;
				}

			private:
				// Spawn d'une entite MESH depuis un chemin relatif projet. Retourne
				// false si le type n'est pas Mesh ou si le monde n'est pas lie.
				bool SpawnMeshFromAsset(const char *relPath) noexcept;

				char mLastDropPath[256] = {};
				nk_int32 mDropCount = 0;

				ecs::NkWorld *mWorld = nullptr;
				ecs::NkSceneGraph *mScene = nullptr;
				NkSelectionManager *mSel = nullptr;
				char mProjectDir[256] = {};
				ecs::NkEntityId mLastSpawned{};
				nk_int32 mSpawnCount = 0;

				bool mProbeEnabled = false;
				bool mProbeRectValid = false;
				nkgui::NkRect mProbeRect{0.f, 0.f, 0.f, 0.f};
		};

	} // namespace noge
} // namespace nkentseu
