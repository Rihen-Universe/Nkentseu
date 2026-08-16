#pragma once
// =============================================================================
// Noge/Panels/SceneTreePanel.h
// =============================================================================
// Panel qui affiche la hiérarchie ECS de la scène active.
// Utilise NkUI::TreeNode pour afficher les entités parent/enfant.
// Supporte sélection, rename, drag&drop, activation/désactivation.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/SceneComponent/NkSceneComponent.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Panels/Model/NkSceneTreeModel.h" // etat neutre (sans UI)

namespace nkentseu {
	namespace noge {

		// L'etat (noeuds deplies, renommage, menu contextuel) vit dans
		// NkSceneTreeModel, en-tete NEUTRE sans dependance d'interface — pour
		// qu'un portage vers NKGui le partage au lieu de le recopier.
		class SceneTreePanel : public NkSceneTreeModel {
			public:
				SceneTreePanel() = default;

				// ── Rendu ─────────────────────────────────────────────────────────
				// Dessine le panel dans la fenêtre NKUI fournie.
				// ctx/wm/dl/font/ls : contexte NKUI actif.
				// world : le monde ECS courant.
				// sel   : gestionnaire de sélection (modifié si l'utilisateur clique).
				// hist  : historique pour les renommages.
				void Render(nkui::NkUIContext &ctx, nkui::NkUIWindowManager &wm, nkui::NkUIDrawList &dl,
							nkui::NkUIFont &font, nkui::NkUILayoutStack &ls, ecs::NkWorld &world,
							ecs::NkSceneGraph *scene, NkSelectionManager &sel, CommandHistory *hist,
							nkui::NkRect rect) noexcept;

			private:
				// Rendu récursif d'une entité et de ses enfants
				void RenderEntity(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
								  nkui::NkUILayoutStack &ls, ecs::NkWorld &world, ecs::NkEntityId id,
								  NkSelectionManager &sel, CommandHistory *hist, nk_uint32 depth) noexcept;

				void RenderContextMenu(nkui::NkUIContext &ctx, nkui::NkUIDrawList &dl, nkui::NkUIFont &font,
									   ecs::NkWorld &world, ecs::NkEntityId id, ecs::NkSceneGraph *scene,
									   NkSelectionManager &sel) noexcept;

				// L'état interne (mContextMenuEntity, mRenamingEntity, mRenameBuffer,
				// mScrollToSelected, mOpenNodes/mOpenCount) et IsOpen/SetOpen sont
				// hérités de NkSceneTreeModel.
		};

	} // namespace noge
} // namespace nkentseu
