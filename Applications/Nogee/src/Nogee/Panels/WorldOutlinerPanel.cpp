// =============================================================================
// Nogee/Panels/WorldOutlinerPanel.cpp — portage NKUI -> NKGui, vise sur §7 (cf. .h)
// =============================================================================
#include "WorldOutlinerPanel.h"
#include "NKGui/NKGui.h"
#include "NKLogger/NkLog.h" // temoin du cablage (une ligne, une fois)
#include "Noge/ECS/NkEcsUtil.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/SceneComponent/NkSceneComponent.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		// =====================================================================
		// Filtre de recherche (§7 « barre de recherche »).
		// Une branche reste visible si ELLE ou l'un de ses descendants
		// correspond : filtrer sur le seul parent rendrait ses enfants
		// inatteignables, ce qui transformerait la recherche en masquage.
		//
		// SENSIBLE A LA CASSE, deliberement : c'est la convention deja en place
		// pour le filtre de la Console (`NkConsoleModel::Passes` -> `Contains`).
		// Aucune des trois specs ne tranche la casse ; on s'aligne sur
		// l'existant plutot que d'introduire une deuxieme regle. Aucun helper
		// insensible a la casse n'existe dans Kernel/ ni Engine/ (verifie).
		// =====================================================================
		bool WorldOutlinerPanel::PassesFilter(ecs::NkEntityId id) const noexcept {
			if (mFilterBuf[0] == '\0')
				return true;
			if (!mWorld)
				return false;

			const ecs::NkSceneNode *node = mWorld->Get<ecs::NkSceneNode>(id);
			if (node && std::strstr(node->name, mFilterBuf) != nullptr)
				return true;

			const ecs::NkChildren *children = mWorld->Get<ecs::NkChildren>(id);
			if (children) {
				for (nk_uint32 i = 0; i < children->count; ++i) {
					if (PassesFilter(children->children[i]))
						return true;
				}
			}
			return false;
		}

		// =====================================================================
		// Corps du panneau. Le shell a deja ouvert la fenetre ancree : on ne
		// dessine QUE le contenu.
		// =====================================================================
		void WorldOutlinerPanel::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			if (!mWorld) {
				Text(ctx, "Aucun monde lie.");
				return;
			}

			// ── §7 : barre de recherche + bouton « + » ────────────────────────
			InputText(ctx, "Rechercher##wo_filter", mFilterBuf, static_cast<int32>(sizeof(mFilterBuf)));

			if (ec.Button("+##wo_add")) {
				if (mScene) {
					const ecs::NkEntityId newId = mScene->SpawnNode("Entity");
					if (mSel)
						mSel->Select(newId);
				}
			}
			ctx.SameLine();
			Checkbox(ctx, "Layer##wo_col", mShowLayerColumn);

			Separator(ctx);

			// ── §7 : l'arbre ──────────────────────────────────────────────────
			// Racines = entites sans parent valide. `Without<NkInactive>` est
			// conserve tel quel depuis le chemin NKUI : c'est la meme semantique
			// de scene, et ce portage ne la rediscute pas.
			NkVector<ecs::NkEntityId> roots;
			mWorld->Query<const ecs::NkSceneNode>().Without<ecs::NkInactive>().ForEach(
				[&](ecs::NkEntityId id, const ecs::NkSceneNode &) {
					const ecs::NkParent *p = mWorld->Get<ecs::NkParent>(id);
					if (!p || !p->entity.IsValid())
						roots.PushBack(id);
				});

			// ── TEMOIN DU CABLAGE COTE SHELL (2026-08-17) ─────────────────────
			// Meme motif que le temoin du panneau NKUI (SceneTreePanel.cpp) : une
			// ligne, une fois, dans le VRAI chemin de rendu. Formatee via
			// snprintf — logger.Infof n'interpole pas les {i} (famille printf).
			{
				static bool sReported = false;
				if (!sReported) {
					sReported = true;
					const char *premier = "(aucune)";
					if (roots.Size() > 0) {
						if (const ecs::NkSceneNode *n0 = mWorld->Get<ecs::NkSceneNode>(roots[0]))
							premier = n0->name;
					}
					char msg[192];
					std::snprintf(msg, sizeof(msg),
								  "[WorldOutlinerPanel] TEMOIN : rendu execute via le shell, %u racine(s), "
								  "premiere = '%s'\n",
								  (unsigned)roots.Size(), premier);
					logger.Info(msg);
				}
			}

			for (nk_usize i = 0; i < roots.Size(); ++i)
				RenderEntity(ctx, roots[i], 0);

			// Menu contextuel : dessine une seule fois, hors de la recursion.
			if (mContextMenuEntity.IsValid())
				RenderContextMenu(ctx);
		}

		// =====================================================================
		// Une ligne d'entite, puis ses enfants.
		// =====================================================================
		void WorldOutlinerPanel::RenderEntity(NkGuiContext &ctx, ecs::NkEntityId id, nk_uint32 depth) noexcept {
			ecs::NkSceneNode *node = mWorld->Get<ecs::NkSceneNode>(id);
			if (!node)
				return;
			if (!PassesFilter(id))
				return;

			const ecs::NkChildren *children = mWorld->Get<ecs::NkChildren>(id);
			const bool hasChildren = children && children->count > 0;
			const bool isSelected = mSel && mSel->IsSelected(id);

			// Identifiant STABLE de la ligne : index+generation. Le libelle ne
			// peut pas servir d'id, il change au renommage.
			char idStr[32];
			std::snprintf(idStr, sizeof(idStr), "wo_%u_%u", id.index, id.gen);

			// ── §7 : bascule de visibilite ────────────────────────────────────
			// ⛔ Rendue par une case a cocher : NKGui n'expose pas d'atlas
			// d'icones ici, donc pas d'oeil. La FONCTION est la, pas l'icone.
			char visId[40];
			std::snprintf(visId, sizeof(visId), "##vis_%u_%u", id.index, id.gen);
			Checkbox(ctx, visId, node->visible);
			ctx.SameLine();

			bool clicked = false;
			bool opened = false;

			if (hasChildren) {
				// ⭐ Renommage inline NATIF : double-clic edite `node->name` en
				// place. Le chemin NKUI ecrit cette machine a etats a la main.
				opened = TreeNodeEditable(ctx, idStr, node->name, static_cast<int32>(sizeof(node->name)));
				clicked = ctx.IsItemHovered() && ctx.input.mouseClicked[0];
			} else {
				clicked = SelectableEditable(ctx, idStr, node->name, static_cast<int32>(sizeof(node->name)),
											 isSelected);
			}

			// ── §7 : colonne optionnelle « Layer » ────────────────────────────
			if (mShowLayerColumn) {
				ctx.SameLine();
				char lay[24];
				std::snprintf(lay, sizeof(lay), "L%u", static_cast<nk_uint32>(node->layer));
				Text(ctx, lay);
			}

			// ── §7 : selection multiple Ctrl, synchronisee avec le viewport ───
			// NKGui expose un `ctrlDown` unique ; NKUI demandait de tester
			// NK_LCTRL et NK_RCTRL separement.
			if (clicked && mSel) {
				if (ctx.input.ctrlDown)
					mSel->SelectToggle(id);
				else
					mSel->Select(id);
			}

			// Clic droit → menu contextuel, ancre sous la souris.
			if (ctx.IsItemHovered() && ctx.input.mouseClicked[1]) {
				mContextMenuEntity = id;
				ctx.OpenPopupAt(ctx.GetId("##wo_ctx"), ctx.input.mousePos);
			}

			// ── Enfants ───────────────────────────────────────────────────────
			if (hasChildren && opened) {
				for (nk_uint32 i = 0; i < children->count; ++i)
					RenderEntity(ctx, children->children[i], depth + 1);
				TreePop(ctx);
			}
		}

		// =====================================================================
		// Menu contextuel. `mContextMenuEntity` (modele NEUTRE partage) dit sur
		// QUI il porte — c'est l'un des deux champs du modele qui servent encore
		// sur ce chemin.
		// =====================================================================
		void WorldOutlinerPanel::RenderContextMenu(NkGuiContext &ctx) noexcept {
			if (!BeginPopupMenu(ctx, "##wo_ctx")) {
				mContextMenuEntity = ecs::NkEntityId::Invalid();
				return;
			}

			const ecs::NkEntityId id = mContextMenuEntity;

			// « Renommer » n'arme plus un tampon : le renommage est natif au
			// widget (double-clic). L'entree reste pour la decouvrabilite et
			// pour le raccourci annonce par la cible.
			if (MenuItem(ctx, "Renommer", "F2")) {
				mContextMenuEntity = ecs::NkEntityId::Invalid();
			}

			if (MenuItem(ctx, "Dupliquer", "Ctrl+D")) {
				if (mScene) {
					const ecs::NkEntityId dup = mScene->SpawnNode("Entity_dup");
					if (mSel)
						mSel->Select(dup);
				}
				mContextMenuEntity = ecs::NkEntityId::Invalid();
			}

			Separator(ctx);

			if (MenuItem(ctx, "Supprimer", "Suppr")) {
				if (mScene)
					mScene->DestroyRecursive(id);
				if (mSel)
					mSel->Clear();
				mContextMenuEntity = ecs::NkEntityId::Invalid();
			}

			EndPopupMenu(ctx);
		}

	} // namespace noge
} // namespace nkentseu
