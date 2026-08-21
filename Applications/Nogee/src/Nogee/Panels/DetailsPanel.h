#pragma once
// =============================================================================
// Nogee/Panels/DetailsPanel.h
// =============================================================================
// PORTAGE 3/4 (2026-08-17) — l'inspecteur de proprietes, ecrit sur
// NKGui/NKEditorKit au lieu de NKUI, et vise sur la CIBLE (§8 « Details
// Panel ») et non sur la reproduction de `InspectorPanel`.
//
// Meme raison de nom que `WorldOutlinerPanel` : les trois specifications ne
// contiennent NI « Inspector » NI « Scene Tree ». Le vocabulaire cible est
// *Details Panel*. `InspectorPanel` (NKUI) reste vivant et intact.
//
// -----------------------------------------------------------------------------
// 🔴 CE QU'IL FAUT SAVOIR AVANT DE LIRE LE RESTE : L'INSPECTEUR NKUI NE
//    DESSINE AUCUN COMPOSANT, ET CE DEPUIS TOUJOURS
// -----------------------------------------------------------------------------
//   `InspectorPanel.cpp:56-61` :
//
//       void *ptr = nullptr;  // world.GetRaw(id, meta.typeName);
//       if (!ptr) continue;
//
//   La boucle sur les composants reflechis **ne s'execute jamais** : elle
//   `continue` a chaque iteration. Tout le rendu par reflexion
//   (`RenderComponent`, `RenderField`, et ses 8 cas de types) est du **code
//   mort**. Seuls l'en-tete de nom et le bouton « Ajouter un composant »
//   dessinent quelque chose.
//
//   **La cause n'est pas le panneau : `NkWorld::GetRaw(id, typeId)` N'EXISTE
//   PAS** (verifie dans `NKECS/World/NkWorld.h`, et nulle part ailleurs dans
//   le depot — le controle positif remonte bien des `GetRaw*` dans NKEvent).
//   Sans un acces GENERIQUE a la memoire d'un composant, aucun panneau de
//   proprietes pilote par reflexion ne peut fonctionner, quelle que soit la
//   bibliotheque d'interface.
//
//   ⚠️ **C'est le motif de la journee, une troisieme fois** : du code qui
//   existe, compile, et ne fait rien — que personne ne voit parce que rien ne
//   l'exerce. Porter ce code tel quel aurait produit un panneau NKGui
//   aussi vide, avec l'air d'avoir ete porte.
//
//   👉 **Ce panneau ne reproduit donc PAS la boucle morte.** Il livre ce qui
//   peut reellement fonctionner aujourd'hui (acces TYPE aux composants reels)
//   et **affiche a l'ecran** ce qui est bloque, au lieu de ne rien dessiner en
//   silence. Un panneau qui dit « bloque » est honnete ; un panneau vide ment.
//
// -----------------------------------------------------------------------------
// CE QUE LA CIBLE §8 DEMANDE, ET OU ON EN EST
// -----------------------------------------------------------------------------
//   ✅ en-tete : nom de l'acteur EDITABLE ....... fait (`NkName::value`)
//   ✅ barre de recherche de propriete .......... fait (filtre en direct)
//   ✅ section Transform TOUJOURS en haut ....... fait, et non repliable
//   ✅ Position / Scale : 3 champs X/Y/Z ........ fait
//   ✅ code couleur rouge/vert/bleu sur X/Y/Z ... fait
//   ✅ cadenas de liaison des 3 axes sur Scale .. fait (lie les 3 facteurs)
//   ✅ bouton « Ajouter un composant » .......... fait (menu de recherche)
//   ⛔ ROTATION en 3 champs Euler ............... NON FAIT, et c'est delibere :
//        `NkTransform::localRotation` est un `NkQuatf`, et la conversion
//        Euler passe par `NkEulerAngle`, qui est fait de `NkAngle` et non de
//        `float32`. Un aller-retour quaternion -> Euler -> quaternion ecrit a
//        l'aveugle, sans temoin, produirait une derive de rotation SILENCIEUSE
//        — exactement le genre de faux que cette journee a passe son temps a
//        chasser. La rotation est donc affichee en LECTURE SEULE (composantes
//        du quaternion), et la conversion est un poste nomme.
//   ⛔ sections repliables par composant reflechi  BLOQUE par `GetRaw` (ci-dessus)
//   ⛔ pastille jaune « valeur modifiee vs parent » NON FAIT : suppose une
//        notion de prefab parent et de diff, absente du modele actuel.
//   ⛔ types couleur / enum / reference d'asset / courbe  BLOQUE par `GetRaw`
//        (ils vivent dans `RenderField`, qui est le code mort ci-dessus).
//   ⛔ icone reset au survol si different du defaut  NON FAIT (pas de valeur
//        par defaut connue par champ sans la reflexion).
//
// -----------------------------------------------------------------------------
// ⚠️ DIVERGENCE ENTRE LES SPECS — signalee, PAS tranchee
// -----------------------------------------------------------------------------
//   **01** demande une **liste de composants (« Component hierarchy ») en haut
//   du panneau** quand l'acteur en a plusieurs, en plus des sections. **02**
//   (`§7 DetailsPanel` de la check-list) ne decrit qu'« une liste de sections
//   `<Accordion>` contenant des `<PropertyRow>` » — pas de liste de composants
//   separee. **03** n'en montre aucune. Non tranche ici : de toute facon la
//   question ne se pose qu'une fois `GetRaw` disponible.
//
//   *(Rappel du portage precedent, toujours ouvert : le PLACEMENT du panneau.
//   02 et 03 mettent `detailsPanel` a droite sous le World Outliner ; 01 le met
//   en haut a droite. Retenu `NK_RIGHT`, comme defaut de compilation.)*
//
// CE QUI NE CHANGE PAS : `Model/NkInspectorModel.h`, en-tete NEUTRE, partage
// avec `InspectorPanel`. ⚠️ Mais voir la note de la ROADMAP : sur le chemin
// NKGui, `CollapsingHeader` tient lui-meme l'etat d'ouverture, donc
// `IsSectionOpen`/`SetSectionOpen` ne servent QU'au chemin NKUI — meme constat
// que pour `NkSceneTreeModel`.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKECS/World/NkWorld.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Panels/Model/NkInspectorModel.h" // modele PARTAGE avec InspectorPanel

namespace nkentseu {
	namespace noge {

		class DetailsPanel final : public editorkit::NkEditorPanel, public NkInspectorModel {
			public:
				DetailsPanel() noexcept
					: editorkit::NkEditorPanel("Details", editorkit::NkEditorDockSide::NK_RIGHT) {
				}

				// Meme contrat de pret que WorldOutlinerPanel : le shell prete le
				// monde pour l'image courante, le panneau ne possede rien.
				void Bind(ecs::NkWorld *world, const NkSelectionManager *sel, CommandHistory *hist) noexcept {
					mWorld = world;
					mSel = sel;
					mHist = hist;
				}

				void OnUI(editorkit::NkEditorFrameContext &ec) override;

			private:
				// Section Transform (§8 : toujours en haut, non repliable).
				void RenderTransform(nkgui::NkGuiContext &ctx, ecs::NkEntityId id) noexcept;

				// Une ligne X/Y/Z a labels colores. `lockUniform` : si non nul et
				// vrai, editer un axe applique le meme FACTEUR aux trois (cadenas
				// de §8). Renvoie true si une valeur a change.
				bool RenderVec3Row(nkgui::NkGuiContext &ctx, const char *label, float32 *v,
								   const bool *lockUniform) noexcept;

				// Menu « Ajouter un composant » — fonctionne, lui : il n'a besoin
				// que de `meta.addFn`, pas d'un acces memoire generique.
				void RenderAddComponentMenu(nkgui::NkGuiContext &ctx, ecs::NkEntityId id) noexcept;

				// Vrai si `label` passe le filtre de recherche de propriete.
				bool PassesFilter(const char *label) const noexcept;

				// ── Etat propre a la CIBLE ───────────────────────────────────────
				char mFilterBuf[64] = {};
				bool mLockScale = false; ///< cadenas de §8, sur Scale

				ecs::NkWorld *mWorld = nullptr;
				const NkSelectionManager *mSel = nullptr;
				CommandHistory *mHist = nullptr;
		};

	} // namespace noge
} // namespace nkentseu
