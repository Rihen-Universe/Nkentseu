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

			mProbeRowCount = 0; // la sonde repart de zero a chaque image
			for (nk_usize i = 0; i < roots.Size(); ++i)
				RenderEntity(ctx, roots[i], 0);

			// ── §7 : reparentage DIFFERE ──────────────────────────────────────
			// AcceptDragPayload livre PENDANT la recursion sur l'arbre, et
			// SetParent modifie les listes NkChildren qu'on est en train de
			// parcourir (l'avertissement est dans NkSceneGraph.cpp). On collecte
			// donc pendant la boucle et on applique ICI, apres elle.
			if (mPendingChild.IsValid() && mScene) {
				ApplyReparent(mPendingChild, mPendingParent);
				mPendingChild = ecs::NkEntityId::Invalid();
				mPendingParent = ecs::NkEntityId::Invalid();
			}

			// Menu contextuel : dessine une seule fois, hors de la recursion.
			if (mContextMenuEntity.IsValid())
				RenderContextMenu(ctx);
		}

		// =====================================================================
		// §7 : application du reparentage, avec ses gardes.
		//
		// ⚠️ SEMANTIQUE TRANSFORM, MESUREE (2026-08-17) : dans la coquille, la
		// parente est une APPARTENANCE, pas une chaine de transforms —
		//   - SetParent marque `NkWorldTransform` dirty, mais AUCUN systeme ne
		//     consomme NkLocalTransform/NkWorldTransform (grep vide sur
		//     ECS/Systems/, controle positif : NkTransformSystem consomme bien
		//     `NkTransform`) ; NkTransform.h les declare d'ailleurs REMPLACES ;
		//   - le systeme qui compose `world = parentWorld * local`
		//     (NkTransformSystem) n'est PAS tique par la coquille.
		// La position monde est donc trivialement preservee aujourd'hui. Le jour
		// ou la coquille tiquera NkTransformSystem, reparenter en gardant le
		// local CHANGERA la position monde : il faudra alors recalculer le local
		// pour la preserver (local' = inverse(parentWorld') * world).
		// =====================================================================
		void WorldOutlinerPanel::ApplyReparent(ecs::NkEntityId child, ecs::NkEntityId parent) noexcept {
			char msg[192];

			// Garde 1 — pas de cycle : si `parent` est un DESCENDANT de `child`
			// (ou child lui-meme), SetParent fabriquerait une boucle — il n'a
			// aucune garde interne (mesure : NkSceneGraph.cpp:110). Remontee
			// bornee : une hierarchie saine fait < 64 niveaux.
			ecs::NkEntityId walk = parent;
			for (int32 depth = 0; walk.IsValid() && depth < 64; ++depth) {
				if (walk == child) {
					std::snprintf(msg, sizeof(msg),
								  "[WorldOutlinerPanel] MESURE : reparentage REFUSE (cycle) : la cible "
								  "%u_%u descend de %u_%u\n",
								  parent.index, parent.gen, child.index, child.gen);
					logger.Info(msg);
					return;
				}
				const ecs::NkParent *p = mWorld->Get<ecs::NkParent>(walk);
				walk = p ? p->entity : ecs::NkEntityId::Invalid();
			}

			// Garde 2 — trou mesure de SetParent (NkSceneGraph.cpp, etape 2) : il
			// fait `Get<NkChildren>` SANS creer. Si la cible n'a pas NkChildren,
			// l'enfant garderait un NkParent valide sans figurer dans aucune
			// liste : il DISPARAITRAIT de l'arbre. On refuse plutot que de perdre
			// un noeud (les noeuds SpawnNode ont toujours NkChildren).
			if (!mWorld->Get<ecs::NkChildren>(parent)) {
				std::snprintf(msg, sizeof(msg),
							  "[WorldOutlinerPanel] MESURE : reparentage REFUSE : la cible %u_%u n'a pas "
							  "de NkChildren (trou SetParent : Get sans creation)\n",
							  parent.index, parent.gen);
				logger.Info(msg);
				return;
			}

			mScene->SetParent(child, parent);
			std::snprintf(msg, sizeof(msg),
						  "[WorldOutlinerPanel] TEMOIN : reparentage applique : %u_%u -> parent %u_%u "
						  "(appartenance seule — aucun systeme de transform tique dans la coquille)\n",
						  child.index, child.gen, parent.index, parent.gen);
			logger.Info(msg);
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

			// ── §7 : glisser-deposer de reparentage ───────────────────────────
			// La ligne qu'on vient de soumettre (TreeNodeEditable /
			// SelectableEditable passent par ButtonBehavior) est a la fois
			// SOURCE (on la glisse) et CIBLE (on lache dessus). La bibliotheque
			// porte l'etat : seuil de demarrage, fantome, surlignage, livraison
			// unique au relachement — tout vient de NKGui (commit 442fe8c7).
			if (BeginDragSource(ctx)) {
				SetDragPayload(ctx, "entity", &id, static_cast<int32>(sizeof(id)), node->name);
				EndDragSource(ctx);
			}
			if (BeginDropTarget(ctx)) {
				int32 sz = 0;
				if (const void *p = AcceptDragPayload(ctx, "entity", &sz)) {
					if (sz == static_cast<int32>(sizeof(ecs::NkEntityId))) {
						// DIFFERE : applique apres la boucle (cf. OnUI) — on est
						// en pleine recursion sur les listes NkChildren.
						std::memcpy(&mPendingChild, p, sizeof(mPendingChild));
						mPendingParent = id;
					}
				}
				EndDropTarget(ctx);
			}

			// Sonde drag-drop (--dragdrop-test) : releve du rect ECRAN reel de la
			// ligne — une mesure reproductible ne peut pas cliquer a la main, et
			// deviner la geometrie du dock reviendrait a mesurer sa supposition.
			if (mProbeEnabled && mProbeRowCount < kProbeRowMax) {
				mProbeRows[mProbeRowCount].id = id;
				mProbeRows[mProbeRowCount].rect = ctx.lastItemRect;
				++mProbeRowCount;
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
