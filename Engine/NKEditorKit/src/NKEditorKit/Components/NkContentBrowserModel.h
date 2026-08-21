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
#include "NKEditorKit/Components/NkComponentInstance.h"
// ⚠️ NKGui N'ENTRE PAS PAR CET INCLUDE, et c'est verifiable : `NkComponentPaint.h`
//    n'inclut que `NKCore/NkTypes.h`. Le banc de neutralite (`ROADMAP.md` §5)
//    continue de passer, et il a ete rejoue apres cet ajout — l'ajouter sans le
//    rejouer aurait laisse l'affirmation « ce fichier compile sans NKGui »
//    debout sans que rien ne la soutienne plus.
#include "NKEditorKit/Components/NkComponentPaint.h"

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

				// ── LA SOURCE DES NOMBRES ───────────────────────────────────────
				// ⚠️ CE CHAMP A REMPLACE TROIS CHAMPS DU DEVIS, et le dire est plus
				//    instructif que le champ lui-meme. Le devis portait ici
				//    `overrideCardGap`, `overrideRowH`, `overrideStrokeW` : trois
				//    fentes d'ecrasement pour SEPT metriques declarees. C'etait le
				//    motif « une liste et son compte separes » — ajouter une
				//    metrique obligeait a ajouter un champ, et les quatre metriques
				//    sans fente n'etaient reglables par personne.
				//
				//    L'instance les remplace toutes les sept, et les suivantes.
				//
				// A `nullptr`, le dessin lit les defauts de la declaration : une
				// application qui n'a rien a regler ne cree pas d'instance et ne
				// paie rien. C'est ce qui garde le PREMIER PAS GRATUIT — la seule
				// chose que la mesure du 18/08 designe comme decisive pour
				// l'adoption d'une brique partagee.
				const NkComponentInstance *values = nullptr;
		};

		/// Lecture d'une metrique du navigateur, avec sa source unique. Le dessin
		/// n'ecrit JAMAIS un nombre : il passe par ici. C'est ce qui rend le temoin
		/// de la tranche possible — changer la valeur dans un fichier change les
		/// commandes de dessin, sans recompiler.
		inline float32 NkBrowserMetric(const NkContentBrowserStyle &s, const char *name);
		inline float32 NkBrowserParam(const NkContentBrowserStyle &s, const char *name);
		/// La variante EFFECTIVE : celle de l'instance si elle en impose une, sinon
		/// celle que l'application a posee dans le style. L'ordre compte — sans lui,
		/// changer la variante dans l'editeur n'aurait aucun effet, et ce serait
		/// exactement « un parametre qui n'est pas honore ».
		inline NkBrowserVariant NkBrowserEffectiveVariant(const NkContentBrowserStyle &s);

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

				// ── LES ECOUTEURS D'EVENEMENTS ──────────────────────────────────
				// ⚠️ CES CINQ-LA NE SONT PAS DES POINTS DE GREFFE, et le devis les
				//    rangeait a tort avec les precedents. Un point de greffe ajoute
				//    du DESSIN ou filtre une donnee ; un evenement signale un FAIT.
				//    Ils sont DECLARES separement (`kEvents` plus bas), avec leur
				//    charge, parce que c'est ce qu'un blueprint devra brancher —
				//    condition posee par Rodolf le 18/08 en tranchant l'issue (2).
				//
				// Le composant ne charge rien, n'ouvre rien, ne supprime rien : il
				// SIGNALE. Aucun de ces cinq n'a d'action par defaut — un
				// double-clic sur lequel personne n'ecoute ne fait rien, et c'est
				// ecrit dans la declaration (`hasDefaultAction = false`) pour que
				// la question ne se repose pas a chaque application.
				//
				// ⚠️ LES SIGNATURES C++ ET LES CHARGES DECLAREES DOIVENT
				//    CORRESPONDRE. Rien dans le compilateur ne le verifie : c'est
				//    le meme angle mort que « le point de verite d'un encodage est
				//    son consommateur ». La sonde de `NKUIDesign` verifie donc les
				//    DEUX bouts — nombre d'evenements declares et emission reelle.
				void (*onSelect)(void *user, int32 index, const char *path) = nullptr;
				void (*onDoubleClick)(void *user, int32 index, const char *path) = nullptr;
				void (*onContextMenu)(void *user, int32 index, float32 x, float32 y) = nullptr;
				void (*onDrop)(void *user, int32 folderIndex, const char *payloadType) = nullptr;
				void (*onNavigate)(void *user, const char *path) = nullptr;
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
		// ⚠️ SIX, PAS CINQ — le devis se trompait, et la raison est ecrite en toutes
		//    lettres dans `NkComponentPaint.h`, au bloc `NkComponentInput` : un
		//    composant a cinq arguments dessine sans jamais rien entendre, et ses
		//    crochets d'evenement ne peuvent alors PAS partir. `ROADMAP.md` §3 est
		//    corrige.
		//
		// ETAT (2026-08-18, seconde passe) : DEFINIE, dans
		//   `NkContentBrowserDraw.cpp`. Elle ne depend PAS du peintre de
		//   NK3DModeler — elle depend de l'INTERFACE `NkComponentPaint`, que ce
		//   peintre satisfera a son arrivee. C'est ce qui a permis de livrer la
		//   tranche sans prendre a l'agent NK3DModeler un travail qui est le sien.
		NkContentBrowserResult NkDrawContentBrowser(NkComponentPaint &p, const NkComponentInput &in,
													const NkPaintRect &rect, NkContentBrowserModel &m,
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
			// ⚠️ TROIS ENTREES ONT QUITTE CETTE TABLE le 18/08 (seconde passe) :
			//    `on_activate`, `on_context_menu`, `on_drop_into` sont des
			//    EVENEMENTS, pas des greffes. Elles vivent dans `kEvents`, avec
			//    leur charge. Ce qui reste ici dessine ou filtre — rien d'autre.
			static const NkHookDecl kHooks[] = {
				{"card_overlay", "(user, peintre, index, x, y, w, h) -> void",
				 "dessin ajoute par l'application par-dessus une carte"},
				{"extra_column", "(user, index, col) -> texte", "colonne supplementaire (cf. props §4.3)"},
				{"accept_entry", "(user, entree) -> bool", "filtre propre a l'application"},
			};

			// ── LES EVENEMENTS ──────────────────────────────────────────────────
			// SECONDE CONDITION DE RODOLF (2026-08-18) : « la declaration porte les
			// EVENEMENTS des maintenant, avec leur charge — sinon le mouvement se
			// refait dans trois mois. »
			//
			// Les charges sont ecrites dans le vocabulaire de types de la spec
			// `.nkgui` v0.2 (§11), sans en inventer un seul. C'est ce qui permet a
			// `NkWriteControllerBlock` d'emettre le bloc `controller` de la spec
			// §10 sans traduction — la convergence est produite, pas affirmee.
			//
			// ⚠️ POURQUOI `path` EN PLUS DE `index`, alors que l'index suffit au
			//    C++ : un blueprint n'a pas le modele sous la main. Une charge qui
			//    n'est interpretable qu'en possedant l'objet emetteur n'est pas une
			//    charge, c'est un pointeur deguise — et le graphe ne pourrait rien
			//    en faire. `index` sert au code, `path` sert au graphe.
			static const NkArgDecl kArgsEntry[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"path", NkArgKind::String, nullptr, 0},
			};
			static const NkArgDecl kArgsMenu[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"at", NkArgKind::Vec2, nullptr, 0},
			};
			static const NkArgDecl kArgsDrop[] = {
				{"folderIndex", NkArgKind::Int, nullptr, 0},
				{"payloadType", NkArgKind::String, nullptr, 0},
			};
			static const NkArgDecl kArgsNav[] = {
				{"path", NkArgKind::String, nullptr, 0},
			};
			static const NkEventDecl kEvents[] = {
				{"onSelect", "Selection", "l'entree active change (clic simple, ou clavier)", kArgsEntry,
				 2, false},
				{"onDoubleClick", "Activation", "double-clic sur une entree — ouvrir, entrer, importer",
				 kArgsEntry, 2, false},
				{"onContextMenu", "Menu contextuel", "clic droit ; `index` vaut -1 sur le fond du panneau",
				 kArgsMenu, 2, false},
				{"onDrop", "Depot", "un glisser-deposer est relache sur un dossier de la vue", kArgsDrop, 2,
				 false},
				{"onNavigate", "Navigation", "le fil d'Ariane ou l'arbre a change de dossier courant",
				 kArgsNav, 1, false},
			};
			// ── L'ARBRE DE SOUS-ELEMENTS ────────────────────────────────────────
			// AJOUT 2 DE RODOLF (2026-08-18, soir), pose ici sur le composant deja
			// livre -- parce qu'un ajout a la forme qui ne serait porte que par le
			// composant NEUF ne prouverait rien : il faut qu'il tienne aussi sur ce
			// qui existe.
			//
			// COMMENT LA LIRE : de haut en bas, c'est l'ordre de l'arbre. Chaque
			// ligne nomme son parent, et un parent est toujours PLUS HAUT qu'un
			// enfant -- ce qui rend un cycle impossible a ecrire et le resolveur non
			// recursif (cf. `NkComponentLayout.h`).
			//
			// ⚠️ AUCUN `x`, AUCUN `y`. Les colonnes du corps se partagent la largeur
			//    par FRACTION et par POIDS : c'est ce qui rend le panneau responsive
			//    sans qu'aucune application n'ait a recalculer quoi que ce soit.
			//
			// ⚠️ CE QUI N'EST PAS ICI, ET C'EST NOMME : les CARTES. Elles sont
			//    repetees par la donnee, leur nombre vient du modele, et l'arbre
			//    statique ne sait pas les dire. Le noeud `grid` s'arrete donc a la
			//    grille, et `NkCheckComponent` le signale par une NOTE
			//    (« agencement_sans_enfant ») plutot que de laisser croire que la
			//    structure est complete. Voir ROADMAP.md §9.3, critere n.4.
			static const NkElementDecl kElements[] = {
				{"browser", "", "le panneau entier", "container", "",
				 NkExpand(), NkExpand(),
				 {NkLayoutKind::Column, "", "card_pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},

				{"header", "browser", "bande d'onglets de panneau", "", "",
				 NkExpand(), NkFixedM("header_h"),
				 {}, 0},

				{"toolbar", "browser", "bande d'outils Creer / Importer / recherche", "container", "",
				 NkExpand(), NkFixedM("toolbar_h"),
				 {NkLayoutKind::Row, "card_pad", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
				{"btn_create", "toolbar", "creer un asset", "button", "",
				 NkContent(72.f), NkExpand(), {}, 0},
				{"btn_import", "toolbar", "importer un fichier", "button", "",
				 NkContent(72.f), NkExpand(), {}, 0},
				{"search", "toolbar", "filtre texte", "text_field", "",
				 NkExpand(120.f), NkExpand(), {}, 0},

				{"body", "browser", "l'arbre de dossiers et la vue d'assets", "container", "",
				 NkExpand(), NkExpand(),
				 {NkLayoutKind::Row, "card_pad", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},

				// ⚠️ LA DEMONSTRATION DE « MEME MECANISME AUX DEUX ECHELLES », et
				//    elle n'est pas theorique : la colonne d'arbre du navigateur EST
				//    le composant `tree_view`. Un seul champ le dit, et c'est le seul
				//    endroit de toute la forme ou l'echelle change.
				//    `tree_width` est un PARAMETRE, pas une metrique -- une fraction
				//    n'est pas une longueur. C'est ce cas qui a impose
				//    `NkComponentDecl::Number` : sans lui, il aurait fallu recopier
				//    0.18 ici, et le curseur de l'editeur n'aurait plus rien deplace.
				{"folder_tree", "body", "arborescence des dossiers", "tree", "tree_view",
				 NkFractionM("tree_width", 120.f, 400.f), NkExpand(), {}, 0},

				{"grid", "body", "la vue d'assets : grille de cartes ou liste dense", "list", "",
				 NkExpand(), NkExpand(),
				 {NkLayoutKind::Grid, "card_gap", "", NkAlign::Start, NkAlign::Stretch, 0,
				  "thumb_size"},
				 0},
			};

			static const NkComponentDecl kDecl = {
				// ⚠️ INITIALISATION POSITIONNELLE COMMENTEE, et ce n'est pas de la
				//    coquetterie : C++17 n'a pas les initialiseurs designes, et une
				//    liste de douze entrees sans reperes est exactement le genre de
				//    table ou l'on decale tout d'un cran en ajoutant un champ. Le
				//    commentaire de gauche est le garde-fou -- il rend l'erreur
				//    visible a la relecture, la ou le compilateur, lui, ne dira rien.
				/* name        */ "content_browser",
				/* title       */ "Navigateur de contenu",
				/* summary     */ "grille ou liste d'assets, arbre de dossiers, fil d'Ariane, recherche",
				/* params      */ kParams,   (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				/* variants    */ kVariants, (uint16)(sizeof(kVariants) / sizeof(kVariants[0])),
				/* tokens      */ kTokens,   (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				/* metrics     */ kMetrics,  (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				/* hooks       */ kHooks,	 (uint16)(sizeof(kHooks) / sizeof(kHooks[0])),
				/* events      */ kEvents,   (uint16)(sizeof(kEvents) / sizeof(kEvents[0])),
				// ⚠️ LA CAPACITE : une COLLECTION. Le navigateur est une apparence de
				//    plus pour un role que l'arbre, l'inspecteur de projet et la
				//    console partagent -- c'est exactement ce que Rodolf voulait dire
				//    par « une poignee de roles sert des milliers d'apparences ».
				/* role        */ "list",
				/* elements    */ kElements, (uint16)(sizeof(kElements) / sizeof(kElements[0])),
				// Ecrite a la main, dans ce depot, jamais rejouee contre une planche.
				// `verified = false` ne dit pas « fausse » : il dit « pas encore
				// passee au juge ».
				/* provenance  */ NkProvenance{NkAuthorKind::Human, false, false},
			};
			return kDecl;
		}

		// ── LES TROIS LECTURES, DEFINIES ────────────────────────────────────────
		// Declarees plus haut (elles precedent la declaration du composant, qu'elles
		// utilisent), definies ici. C'est le SEUL chemin par lequel le dessin obtient
		// un nombre.
		inline float32 NkBrowserMetric(const NkContentBrowserStyle &s, const char *name) {
			return s.values ? s.values->Metric(name) : NkContentBrowserDecl().Metric(name);
		}
		inline float32 NkBrowserParam(const NkContentBrowserStyle &s, const char *name) {
			return s.values ? s.values->Param(name) : NkContentBrowserDecl().Param(name);
		}
		inline NkBrowserVariant NkBrowserEffectiveVariant(const NkContentBrowserStyle &s) {
			if (s.values && s.values->Decl()) {
				const int32 v = s.values->Variant();
				if (v >= 0 && v < (int32)NkBrowserVariant::Count)
					return (NkBrowserVariant)v;
			}
			return s.variant;
		}

	} // namespace editorkit
} // namespace nkentseu
