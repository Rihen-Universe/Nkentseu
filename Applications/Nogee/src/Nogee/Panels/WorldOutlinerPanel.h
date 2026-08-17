#pragma once
// =============================================================================
// Nogee/Panels/WorldOutlinerPanel.h
// =============================================================================
// PORTAGE 2/4 (2026-08-17) — l'arbre de scene, ecrit sur NKGui/NKEditorKit au
// lieu de NKUI, et vise sur la CIBLE (§7 « World Outliner ») et non sur la
// reproduction de `SceneTreePanel`.
//
// ⚠️ LE NOM CHANGE, ET CE N'EST PAS COSMETIQUE. Les trois specifications de
// `Applications/Nogee/design/` ne contiennent NI « Inspector » NI « Scene
// Tree » : le vocabulaire cible est *World Outliner* (§7) et *Details Panel*
// (§8). Ce fichier porte donc le nom de la cible. `SceneTreePanel` (NKUI)
// reste vivant et intact — livrable seul, comme le pilote Console.
//
// -----------------------------------------------------------------------------
// CE QUE LA CIBLE §7 DEMANDE, ET OU ON EN EST
// -----------------------------------------------------------------------------
//   ✅ barre de recherche + filtre .............. fait (filtre par nom)
//   ✅ bouton « + » (nouvel element) ............ fait
//   ✅ arbre hierarchique, chevrons repliables .. fait (TreeNode/TreePop)
//   ✅ bascule de visibilite par ligne .......... fait (NkSceneNode::visible)
//   ✅ renommage inline au double-clic .......... fait — NATIF NKGui
//   ✅ selection multiple Ctrl, synchro viewport  fait (NkSelectionManager)
//   ✅ colonne optionnelle « Layer » ............ fait (NkSceneNode::layer)
//   ⛔ icone d'oeil / icones de type ............ EN ATTENTE d'un jeu d'icones :
//        NKGui n'a pas d'atlas d'icones expose ici. La bascule existe et
//        fonctionne, elle est rendue par une case a cocher, pas par un oeil.
//   ✅ glisser-deposer de reparentage ........... fait (2026-08-17) : l'API de
//        charge utile NKGui existe (commit 442fe8c7). Chaque ligne est source
//        ET cible (BeginDragSource/BeginDropTarget, type "entity"),
//        application DIFFEREE apres la boucle (SetParent pendant la recursion
//        modifierait les listes parcourues), garde anti-cycle + garde sur le
//        trou `Get<NkChildren>` de SetParent. ⚠️ Semantique mesuree : la
//        parente est une APPARTENANCE aujourd'hui (aucun systeme de transform
//        tique dans la coquille) — cf. ApplyReparent dans le .cpp.
//   ⛔ colonnes « Type » / « Nb de triangles » ... NON FAIT (pas de source de
//        donnees : aucun compte de triangles accessible depuis NkSceneNode).
//
// -----------------------------------------------------------------------------
// ⚠️ DIVERGENCE ENTRE LES SPECS — SIGNALEE, PAS TRANCHEE
// -----------------------------------------------------------------------------
//   Sur le PLACEMENT du panneau, les trois documents ne disent pas la meme
//   chose :
//     - 02 (Claude) `defaultLayout.json` : `worldOutliner` en colonne DROITE,
//       en HAUT, avec `detailsPanel` en dessous ;
//     - 03 (Banani) §4 : « Panneau droit haut, intitule World Outliner » —
//       d'accord avec 02 ;
//     - 01 (humaine), schema de la vue principale : *Details Panel* en haut a
//       droite, et « World Outliner (dock gauche/droite) » sur la RANGEE DU
//       BAS, a cote du Content Browser. **Contredit 02 et 03.**
//   Retenu ici : `NK_RIGHT`, parce que 2 specs sur 3 concordent et que 02 est
//   le document d'implementation. **C'est un defaut de compilation, pas un
//   arbitrage** : la question revient a Rodolf, et une seule ligne suffit a
//   changer le cote (`SetDefaultSide`).
//   *(A ne pas confondre avec le gros plan §5 de 03, qui montre l'outliner a
//   gauche du panneau Details : c'est une composition d'illustration a deux
//   panneaux, pas la disposition de l'editeur.)*
//
// -----------------------------------------------------------------------------
// ⭐ CE QUI A SURPRIS — et qui change le devis des DEUX portages suivants
// -----------------------------------------------------------------------------
//   NKGui rend une PARTIE du modele neutre inutile sur ce chemin :
//     - l'etat « quels noeuds sont deplies » est tenu par NKGui lui-meme
//       (`ctx.IsNodeOpen` / `ctx.SetNodeOpen`, cles par `NkGuiId`). Les 24
//       lignes `mOpenNodes` / `mOpenCount` / `IsOpen` / `SetOpen` de
//       `NkSceneTreeModel` ne servent QUE au chemin NKUI ;
//     - le renommage inline est NATIF (`TreeNodeEditable` /
//       `SelectableEditable` : double-clic, Entree valide, Echap annule).
//       Les champs `mRenamingEntity` / `mRenameBuffer` et la machine a etats
//       ecrite a la main dans `SceneTreePanel.cpp` ne servent, eux non plus,
//       QUE au chemin NKUI.
//   Restent reellement partages : `mContextMenuEntity` et `mScrollToSelected`.
//   ⚠️ Consequence a ne pas manquer : le modele neutre a bien evite la
//   duplication, mais il a ete dimensionne sur les BESOINS DE NKUI. Le jour ou
//   NKUI sera retire, une partie de ces modeles deviendra du code mort.
//
// CE QUI NE CHANGE PAS : le modele de donnees. On herite de
// `Model/NkSceneTreeModel.h`, en-tete NEUTRE (ni NKUI ni NKGui), partage avec
// `SceneTreePanel`. Il n'y a qu'UNE verite pour l'arbre de scene.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Panels/Model/NkSceneTreeModel.h" // modele PARTAGE avec SceneTreePanel

namespace nkentseu {
	namespace noge {

		class WorldOutlinerPanel final : public editorkit::NkEditorPanel, public NkSceneTreeModel {
			public:
				WorldOutlinerPanel() noexcept
					: editorkit::NkEditorPanel("World Outliner", editorkit::NkEditorDockSide::NK_RIGHT) {
				}

				// Le panneau ne possede pas le monde : le shell le lui prete pour
				// l'image courante. Rien n'est garde entre deux images (les
				// pointeurs peuvent devenir invalides a tout moment).
				void Bind(ecs::NkWorld *world, ecs::NkSceneGraph *scene, NkSelectionManager *sel,
						  CommandHistory *hist) noexcept {
					mWorld = world;
					mScene = scene;
					mSel = sel;
					mHist = hist;
				}

				// ── Rendu : le shell appelle ceci entre Begin/End du dock ─────────
				void OnUI(editorkit::NkEditorFrameContext &ec) override;

				// ── SONDE drag-drop (--dragdrop-test) ─────────────────────────────
				// Rects ECRAN reels des lignes, releves au dessin. Une mesure
				// reproductible ne peut pas cliquer a la main (regle de la sonde
				// d'occultation), et deviner la geometrie du dock reviendrait a
				// mesurer sa supposition au lieu du panneau.
				struct RowProbe {
						ecs::NkEntityId id{};
						nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f};
				};
				static constexpr nk_int32 kProbeRowMax = 16;
				void EnableProbe(bool on) noexcept {
					mProbeEnabled = on;
				}
				const RowProbe *ProbeRow(ecs::NkEntityId id) const noexcept {
					for (nk_int32 i = 0; i < mProbeRowCount; ++i)
						if (mProbeRows[i].id == id)
							return &mProbeRows[i];
					return nullptr;
				}

			private:
				// Rendu recursif d'une entite et de ses enfants.
				void RenderEntity(nkgui::NkGuiContext &ctx, ecs::NkEntityId id, nk_uint32 depth) noexcept;

				// Menu contextuel (clic droit) sur l'entite `mContextMenuEntity`.
				void RenderContextMenu(nkgui::NkGuiContext &ctx) noexcept;

				// Vrai si l'entite (ou l'un de ses descendants) passe le filtre de
				// recherche — sinon la branche entiere est masquee. Un filtre qui
				// masquerait un parent dont un enfant correspond rendrait l'enfant
				// inatteignable.
				bool PassesFilter(ecs::NkEntityId id) const noexcept;

				// ── Etat propre a la CIBLE, absent du chemin NKUI ────────────────
				// Volontairement LOCAL et non remonte dans le modele neutre : le
				// panneau NKUI n'a pas de filtre ni de colonne, et lui ajouter des
				// champs inutilises elargirait un modele deja dimensionne sur ses
				// besoins (cf. « CE QUI A SURPRIS » en tete de fichier).
				char mFilterBuf[64] = {};
				bool mShowLayerColumn = false;

				// §7 : reparentage en attente — livre par AcceptDragPayload PENDANT
				// la recursion, applique par ApplyReparent APRES elle (SetParent
				// modifie les listes NkChildren qu'on parcourt).
				void ApplyReparent(ecs::NkEntityId child, ecs::NkEntityId parent) noexcept;
				ecs::NkEntityId mPendingChild = ecs::NkEntityId::Invalid();
				ecs::NkEntityId mPendingParent = ecs::NkEntityId::Invalid();

				// Sonde drag-drop (cf. bloc public).
				bool mProbeEnabled = false;
				RowProbe mProbeRows[kProbeRowMax];
				nk_int32 mProbeRowCount = 0;

				// Pretes par le shell a chaque image — jamais possedes.
				ecs::NkWorld *mWorld = nullptr;
				ecs::NkSceneGraph *mScene = nullptr;
				NkSelectionManager *mSel = nullptr;
				CommandHistory *mHist = nullptr;
		};

	} // namespace noge
} // namespace nkentseu
