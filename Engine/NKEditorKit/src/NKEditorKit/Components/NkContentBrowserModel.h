#pragma once
// -----------------------------------------------------------------------------
// @File    NkContentBrowserModel.h
// @Brief   LA DEMONSTRATION du devis : le navigateur de contenu, ecrit sous la
//          forme proposee — modele neutre + jetons + variantes + greffes.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ ETAT : DEMONSTRATION DU DEVIS D'ARCHITECTURE (2026-08-18). Inclus par AUCUNE
//    application, dans AUCUNE cible de build. Il montre a quoi ressemble un
//    composant conforme ; il ne remplace rien aujourd'hui. Les trois navigateurs
//    existants (`NK3DModeler/Shell/NkModelerBrowser.h` 1 213 l.,
//    `Nogee/Panels/ContentBrowserPanel.cpp` 309 l., `Nogee/Panels/AssetBrowser.cpp`
//    178 l. en NKUI) restent intacts.
//
// POURQUOI CELUI-LA D'ABORD
//   C'est le composant deja ecrit TROIS fois, et celui dont la maquette est la
//   plus complete : deux captures completes du 18/08 + le contrat de props §4.2
//   de `Applications/Nogee/design/02-specification-claude.md`. La mesure d'ecart
//   de NK3DModeler (18 divergences) conclut que **6 d'entre elles seront ecrites
//   deux fois** si elles ne montent pas. Voir `ROADMAP.md`.
//
// LE TEST QUI GOUVERNE CE FICHIER : « compile-t-il sans NKGui ? »
//   Aucun include d'interface. Le peintre n'est que DECLARE EN AVANT ; les
//   crochets le prennent par reference, ce qui suffit au compilateur et interdit
//   a un type de dessin de fuir dans le modele. La commande du banc est dans
//   `ROADMAP.md`, section « le banc de neutralite ».
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"

namespace nkentseu {
	namespace editorkit {

		// Le peintre du kit : DECLARE, jamais defini ici. C'est lui qui portera
		// `Fill(rect, jeton)`, `Text`, `Icon`, `Outline` — c'est-a-dire ce que
		// `NkModelerPainter` (NkModelerUI.h:168-521) fait deja aujourd'hui pour la
		// seule application NK3DModeler.
		class NkComponentPaint;

		// ── UNE ENTREE ──────────────────────────────────────────────────────────
		// Neutre au sens fort : pas de couleur, pas de rectangle, pas de texture.
		// `thumbnail` est un identifiant OPAQUE — le modele ne sait pas ce qu'il
		// designe, exactement comme `thumbnailHandle` dans
		// `Nogee/Panels/Model/NkAssetBrowserModel.h`, qui a deja fait ce choix et
		// dont c'est la propriete qui a rendu le portage possible.
		struct NkAssetEntry {
				NkString name;
				NkString path;
				bool isFolder = false;
				nk_uint64 thumbnail = 0; ///< 0 = pas encore generee
				bool thumbTried = false; ///< generation deja tentee (reussie ou non)

				// LA NATURE DE L'ASSET N'EST PAS UNE ENUMERATION DU COMPOSANT, et
				// c'est la decision qui rend le composant reutilisable par quatre
				// editeurs. NK3DModeler a « procedural » et « dataset », Nogee a
				// « font », PV3DE aura ses natures medicales : une enumeration
				// commune obligerait chaque nouvelle nature a modifier le kit.
				//
				// On stocke donc un ROLE DE THEME resolu (cf. NkRoleRegistry dans
				// NkTheme.h) : l'application enregistre « nogee.type_font », recoit
				// un identifiant, le pose ici. Le composant sait le PEINDRE sans
				// savoir ce que c'est.
				uint16 kindRole = 0;
				const char *kindLabel = ""; ///< libelle affiche dans le pied de carte
				uint32 userTag = 0;			///< libre a l'application (index, drapeaux)
		};

		// ── LE MODELE ───────────────────────────────────────────────────────────
		// L'application le remplit ; le composant le lit et y ecrit la selection et
		// le defilement. Aucune des trois copies actuelles ne separe ces deux roles :
		// `PaintBrowser` prend `NkModelerState&` en entier et touche **66 champs
		// distincts** de l'etat de l'application (mesure du 18/08) — c'est ce
		// couplage, et non le dessin, qui empeche de le reutiliser ailleurs.
		struct NkContentBrowserModel {
				NkVector<NkAssetEntry> entries;
				NkVector<NkString> breadcrumb; ///< racine -> dossier courant

				// LES DEUX ETATS DE SELECTION, que Rodolf veut voir monter. La carte
				// ACTIVE est celle dont les panneaux montrent les proprietes ; les
				// CHOISIES sont celles qui partiront ensemble. La planche ne montre
				// qu'un etat : elle est donc MUETTE sur le second, pas en
				// contradiction — l'ecart n.3 de la mesure porte sur leur ENCODAGE
				// (aplat contre contour), pas sur leur existence. C'est un arbitrage
				// de Rodolf, et le modele ne le prejuge pas : il porte les deux
				// etats, le style dira comment les peindre.
				int32 active = -1;		 ///< index dans `entries`, -1 = aucun
				NkVector<int32> chosen;	 ///< indices, peut contenir `active`

				char filter[128] = {};
				float32 scroll = 0.f;
				float32 thumbSize = 0.f; ///< 0 = prendre le defaut de la declaration

				bool IsChosen(int32 i) const {
					for (uint32 k = 0; k < (uint32)chosen.Size(); ++k)
						if (chosen[k] == i)
							return true;
					return false;
				}
				void ClearSelection() {
					active = -1;
					chosen.Clear();
				}
		};

		// ── LES VARIANTES ───────────────────────────────────────────────────────
		// Directive de Rodolf du 2026-08-18. UN modele, N rendus. L'index
		// correspond a `NkContentBrowserDecl().variants`.
		//
		// ⚠️ La regle qui rend la separation vraie : ces trois valeurs ne changent
		//    QUE la mise en page et le trait. Elles ne touchent ni `entries`, ni
		//    `active`, ni `chosen`, ni le filtre. Le jour ou une variante a besoin
		//    d'un champ a elle dans le modele, c'est le signe qu'elle n'est pas une
		//    variante mais un second composant — et il faut le dire, pas l'ajouter.
		enum class NkBrowserVariant : uint8 {
			Grid = 0,	///< grille de cartes a vignette (la planche du 18/08)
			DenseList,	///< liste d'une ligne par entree, vignette 16 px
			Columns,	///< colonnes triables (nom / type / taille / date)
			Count
		};

		// ── LE STYLE : QUE DES REFERENCES ───────────────────────────────────────
		// PAS UNE COULEUR, PAS UN PIXEL. Les champs sont des identifiants de role de
		// theme et des noms de metrique. C'est la deuxieme exigence de Rodolf,
		// rendue verifiable par une regle mecanique : si un `uint32` de couleur ou
		// un `float` de pixel apparait ici, la revue le voit.
		//
		// Etat de l'art mesure le 18/08, pour dire ce que ceci corrige :
		//   `NkModelerBrowser.h`    : 10 roles de theme, 2 couleurs en dur, mais
		//                             **249 litteraux flottants nus**
		//   `ContentBrowserPanel.cpp` : **0 role de theme, 4 couleurs en dur**
		struct NkContentBrowserStyle {
				NkBrowserVariant variant = NkBrowserVariant::Grid;

				// Jetons de COULEUR, resolus une fois par l'application depuis la
				// declaration (`NkTokenDecl::defaultRole` -> `NkResolveRole`).
				uint16 panelBg = 0, headerBg = 0, border = 0;
				uint16 text = 0, textMuted = 0;
				uint16 cardBg = 0, cardFooterBg = 0;
				uint16 activeMark = 0, chosenMark = 0;
				uint16 folderTint = 0;

				// Metriques : NOMMEES, pas ecrites. Elles se lisent dans la
				// declaration via `decl.Metric("card_gap")`. Le champ ci-dessous ne
				// sert qu'a l'ecrasement volontaire par l'application ; a zero, le
				// dessin prend le defaut declare.
				float32 overrideCardGap = 0.f;
				float32 overrideRowH = 0.f;
				float32 overrideStrokeW = 0.f;
		};

		// ── LES POINTS DE GREFFE ────────────────────────────────────────────────
		// Troisieme exigence de Rodolf : « en y integrant d'autres graphiques ».
		// Des pointeurs de fonction plutot que des methodes virtuelles : une
		// declaration de composant chargee depuis un fichier pourra les remplir sans
		// qu'une classe existe a la compilation. C'est le meme choix que le depot a
		// deja fait pour `NkEditorAppMenuFn` (NkEditorShell.h) et `ctx.styleFn`
		// (NKGui) — deux crochets qui fonctionnent aujourd'hui.
		struct NkContentBrowserHooks {
				void *user = nullptr;

				// Dessin SUPPLEMENTAIRE par carte, appele APRES le composant : badge
				// « modifie », pastille d'albedo, marque de source de version. Le
				// composant ne sait pas ce qui se dessine, et n'a pas a le savoir.
				void (*cardOverlay)(void *user, NkComponentPaint &p, int32 index, float32 x, float32 y,
									float32 w, float32 h) = nullptr;

				// Colonnes supplementaires — utile surtout en variante `Columns` et
				// `DenseList`. C'est litteralement `extraColumns` du contrat de props
				// §4.3 de la specification de design, porte en C++.
				int32 extraColumnCount = 0;
				const char *(*extraColumnHeader)(void *user, int32 col) = nullptr;
				const char *(*extraColumnText)(void *user, int32 index, int32 col) = nullptr;

				// Filtre PROPRE a l'application, en plus du filtre texte du modele.
				bool (*acceptEntry)(void *user, const NkAssetEntry &e) = nullptr;

				// Decisions rendues a l'application. Le composant ne charge rien,
				// n'ouvre rien, ne supprime rien : il SIGNALE.
				void (*onActivate)(void *user, int32 index) = nullptr;	 ///< double-clic
				void (*onContextMenu)(void *user, int32 index, float32 x, float32 y) = nullptr;
				void (*onDropInto)(void *user, int32 folderIndex) = nullptr;
		};

		// ── CE QUE LE DESSIN REND ───────────────────────────────────────────────
		// Une valeur, pas un effet de bord. L'application decide quoi en faire.
		struct NkContentBrowserResult {
				bool selectionChanged = false;
				bool navigated = false;	 ///< le fil d'Ariane ou un dossier a ete suivi
				int32 activatedIndex = -1;
		};

		// ── LA SIGNATURE TYPE ───────────────────────────────────────────────────
		// C'est la forme proposee pour TOUS les composants de la bibliotheque :
		//
		//     resultat  Dessiner( peintre, rectangle, MODELE, STYLE, GREFFES )
		//
		// Cinq arguments, et chacun a une raison :
		//   - le PEINTRE porte le theme et les primitives (il remplace les 66 champs
		//     d'etat d'application que `PaintBrowser` prend aujourd'hui) ;
		//   - le RECTANGLE : le composant ne decide pas de sa place, l'hote si ;
		//   - le MODELE est neutre et appartient a l'application ;
		//   - le STYLE porte la variante et les jetons — aucune couleur, aucun pixel ;
		//   - les GREFFES portent ce que l'application ajoute sans modifier le kit.
		//
		// ⚠️ DECLARE, PAS DEFINI. La definir exige le peintre partage, qui n'existe
		//    pas encore dans le kit (il vit dans NK3DModeler). C'est le palier 1 de
		//    l'ordre de montee — cf. `ROADMAP.md`.
		NkContentBrowserResult NkDrawContentBrowser(NkComponentPaint &p, float32 x, float32 y, float32 w,
													float32 h, NkContentBrowserModel &m,
													const NkContentBrowserStyle &s,
													const NkContentBrowserHooks &hooks);

		// ── LA DECLARATION ──────────────────────────────────────────────────────
		// Ce que NKUIEditor lira un jour, et ce que le dessin lit DES AUJOURD'HUI
		// pour ses defauts. Definie `inline` dans l'en-tete pour rester verifiable
		// sans cible de build.
		//
		// ⚠️ Ce n'est pas de la documentation : c'est la SOURCE des nombres. Un
		//    dessin conforme ecrit `decl.Metric("card_gap")`, jamais `12.f`.
		inline const NkComponentDecl &NkContentBrowserDecl() {
			static const NkVariantDecl kVariants[] = {
				{"grid", "Grille", "cartes a vignette — la planche du 18/08"},
				{"dense_list", "Liste dense", "une ligne par entree, vignette 16 px"},
				{"columns", "Colonnes", "colonnes triables : nom, type, taille, date"},
			};
			static const NkParamDecl kParams[] = {
				{"thumb_size", "Taille des vignettes", NkParamKind::Float, 96.f, 48.f, 256.f, nullptr, 0},
				{"show_tree", "Afficher l'arbre de dossiers", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_footer", "Pied de carte (nom + type)", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				{"tree_width", "Largeur de l'arbre (fraction)", NkParamKind::Float, 0.18f, 0.10f, 0.45f,
				 nullptr, 0},
			};
			static const NkTokenDecl kTokens[] = {
				{"panel_bg", "PanelBg", "fond du panneau"},
				{"header_bg", "PanelHeader", "bande de tete, barre d'outils, onglets"},
				{"border", "Border", "traits de separation"},
				{"text", "Text", "libelles"},
				{"text_muted", "TextMuted", "type de l'asset, compteurs"},
				{"card_bg", "InputBg", "fond de la vignette"},
				{"card_footer_bg", "InputBg", "pied de carte — la planche lui donne le MEME fond que la "
											  "vignette, l'existant le peint en PanelHeader (ecart n.13)"},
				{"active_mark", "AccentUi", "la carte ACTIVE (ecart n.3 : la planche veut un contour)"},
				{"chosen_mark", "AccentUi", "les cartes CHOISIES — doit rester DISTINCT de active_mark"},
				{"folder_tint", "TypeFolder", "teinte de l'icone de dossier"},
			};
			static const NkMetricDecl kMetrics[] = {
				{"card_gap", 12.f, "gouttiere entre deux cartes"},
				{"card_pad", 8.f, "marge interne d'une carte"},
				{"footer_h", 34.f, "hauteur du pied de carte (2 lignes)"},
				{"row_h", 24.f, "hauteur d'une ligne en variante dense_list / columns"},
				{"stroke_w", 1.f, "epaisseur d'un contour de selection"},
				{"toolbar_h", 36.f, "bande d'outils Creer / Importer"},
				{"header_h", 28.f, "bande d'onglets de panneau"},
			};
			static const NkHookDecl kHooks[] = {
				{"card_overlay", "(user, peintre, index, x, y, w, h) -> void",
				 "dessin ajoute par l'application par-dessus une carte"},
				{"extra_column", "(user, index, col) -> texte", "colonne supplementaire (cf. props §4.3)"},
				{"accept_entry", "(user, entree) -> bool", "filtre propre a l'application"},
				{"on_activate", "(user, index) -> void", "double-clic"},
				{"on_context_menu", "(user, index, x, y) -> void", "clic droit"},
				{"on_drop_into", "(user, index de dossier) -> void", "lacher sur un dossier"},
			};
			static const NkComponentDecl kDecl = {
				"content_browser",
				"Navigateur de contenu",
				"grille ou liste d'assets, arbre de dossiers, fil d'Ariane, recherche",
				kParams,   (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				kVariants, (uint16)(sizeof(kVariants) / sizeof(kVariants[0])),
				kTokens,   (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				kMetrics,  (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				kHooks,	   (uint16)(sizeof(kHooks) / sizeof(kHooks[0])),
			};
			return kDecl;
		}

	} // namespace editorkit
} // namespace nkentseu
