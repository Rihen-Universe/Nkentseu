#pragma once
// -----------------------------------------------------------------------------
// @File    NkTabStripModel.h
// @Brief   LA BANDE D'ONGLETS — le composant qui manquait a la coquille, et que
//          quatre applications avaient chacune reecrit dans leur coin.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE — la mesure d'abord, la decision ensuite
// =============================================================================
//  Le lot « panneaux de Nogee » a laisse un manque ecrit : **la bande d'onglets
//  n'a aucun equivalent dans la coquille**. Rodolf a repondu « ameliore le
//  system. » — c'est-a-dire : le manque se comble ICI, dans le partage, pas
//  dans Nogee tout seul. Une bande recopiee une cinquieme fois aurait ete
//  exactement ce qu'il a refuse en disant « tous les elements sont deja dans
//  nk3dmodeler donc juste copier et adapter ».
//
//  RECENSEMENT (2026-09-14, perimetre `Applications/*/src`, branche
//  `feat/nogee-panneaux`) — les copies existantes, comptees accolade a accolade :
//
//  | site | fonction | lignes | pile | ce qu'elle a en propre |
//  |---|---|---|---|---|
//  | `NK3DModeler/Shell/NkModelerScreens.h` | `PaintTabsI` l.477-606 | 130 | peintre maison | liseret de NATURE, renommage en place, pastille/croix au MEME endroit |
//  | `NKCode/Shell/Panels.h` | `DrawFileTabs` l.1097-1541 | 445 | NkGuiDrawList | multi-rangees, epinglage, defilement+molette, cycle MRU, homonymes desambigues |
//  | `NKUIDesign/main.cpp` | `DrawProjectTabs` l.7804-7955 | 152 | NKGui + `SetNextItemRect` | double signal actif (fond + lisere BAS), `+` qui ouvre un modal |
//  | `NKCode/Shell/NkAiPanel.h` | `DrawViewTabs` l.1820-1856 | 37 | NkGuiDrawList | onglets de VUE (segmente), pas de document |
//  | **`Nogee`** | — | **0** | — | **rien : c'est le manque signale** |
//  | **`NKEditorKit` (la coquille)** | — | **0** | — | **rien non plus** |
//
//  Soit **764 lignes sur quatre sites, et zero dans le partage**. Le kit porte
//  pourtant `DockBuilderDockTab` — mais c'est l'onglet d'un panneau ANCRE, une
//  autre chose : il designe une fenetre du dock, pas un DOCUMENT ouvert.
//
// =============================================================================
//  CE QUI REND LE PARTAGE POSSIBLE ICI, ET IL FALLAIT LE MESURER
// =============================================================================
//  ⚠️ **NK3DModeler N'UTILISE PAS `NkEditorShell`** (mesure : il n'inclut pas
//     `NkEditorShell.h` ; les consommateurs de la coquille sont NKCode,
//     NKUIDesign, Nogee, UnkenyEditor, ConquerorLab). Poser la bande sur la
//     coquille SEULE l'aurait donc rendue inatteignable a l'application meme
//     que Rodolf a designee comme reference. « Les onglets de Nogee et ceux du
//     modeleur doivent venir du meme code » aurait ete faux des le depart.
//
//  Le denominateur commun n'est pas la coquille : c'est `NkComponentPaint`, et
//  il a DEJA ses deux adaptateurs, ecrits avant ce fichier —
//  `NkGuiComponentPaint` (monde `NkGuiDrawList`) et `NkModelerComponentPaint`
//  (monde `NkModelerPainter`). Un composant ecrit contre cette interface est
//  donc peint par les deux hotes SANS que l'un ait a adopter la coquille de
//  l'autre. C'est la forme deja eprouvee par `tree_view` et `content_browser`.
//
//  ⚠️ ET CE N'EST PAS UN DEMENAGEMENT. La regle du depot — « ce qui est enferme
//     dans un `.cpp` d'application ne s'emprunte pas, il se DEMENAGE » — vise le
//     cas `NkDemo3D.cpp`. Elle ne s'applique pas ici, et il faut dire pourquoi
//     plutot que de la contourner en silence : `PaintTabsI` n'est PAS du dessin
//     enferme qu'on pourrait deplacer tel quel. Sur ses 130 lignes, **74 sont de
//     la logique de DOCUMENTS** propre au modeleur — `NkActivateTab`,
//     `NkCloseSceneTab`, `NkBrowserSyncScenes`, `st.DocAlloc()`, la numerotation
//     « Scene_N », l'archivage d'asset. Deplacer ce bloc dans le kit y ferait
//     entrer la notion de scene, d'asset et de projet du modeleur. Ce qui se
//     partage, c'est la BANDE : sa geometrie, ses etats, ses gestes. Ce qui
//     reste chez l'hote, c'est ce que ces gestes DECLENCHENT.
//
// =============================================================================
//  OU AJOUTER QUELQUE CHOSE — les points d'extension, nommes
// =============================================================================
//   • un REGLAGE (afficher/cacher, bascule)     -> `kParams` dans `NkTabStripDecl()`
//   • une LONGUEUR (hauteur, marge, epaisseur)  -> `kMetrics`, JAMAIS un litteral dans le .cpp
//   • une COULEUR                               -> `kTokens` + un champ de `NkTabStripStyle`
//   • un EVENEMENT                              -> `kEvents` + un champ de `NkTabStripResult` + son depart dans le .cpp
//   • un DESSIN de plus sur un onglet           -> `NkTabStripHooks::tabOverlay`
//   • une REPRESENTATION de plus                -> `NkTabVariant` + `kVariants`
//
// =============================================================================
//  CE QUE CE FICHIER NE FAIT PAS — nomme, pour que personne ne le cherche
// =============================================================================
//  - **Il ne renomme pas.** `NkComponentInput` ne porte aucune entree clavier.
//    Le modeleur renomme sa scene DANS l'onglet ; le composant ne peut donc pas
//    posseder ce champ de saisie. Meme partage que le renommage de `tree_view` :
//    le composant RAPPORTE le rectangle du libelle (`NkTabStripResult::tabs`),
//    l'hote — qui a le clavier — y pose son editeur. C'est un contournement
//    assume, pas un oubli.
//  - **Il ne ferme rien, n'ouvre rien, ne cree rien.** Il SIGNALE. La seule
//    chose qu'il ecrit est `m.active` (declaree `hasDefaultAction = true` sur
//    `onSelect`, comme l'arbre).
//  - **Il ne defile pas, il ne s'enroule pas sur plusieurs rangees, il
//    n'epingle pas.** Ce sont les trois capacites que `DrawFileTabs` (NKCode) a
//    en propre. Elles ne sont PAS declarees ici : declarer un parametre que le
//    dessin n'honore pas serait « un parametre declare qui n'est pas honore »,
//    le defaut que ce depot a paye huit fois. Elles sont ecrites au canal comme
//    le RESTE A FAIRE d'un troisieme consommateur, mesure, pas devine.
//  - **Il ne dessine aucune icone d'atlas.** Le `+` et la croix sont TRACES
//    (le kit n'a pas d'atlas : « tout ce qui doit se voir se TRACE »).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		class NkComponentPaint;

		// ── UN ONGLET ───────────────────────────────────────────────────────────
		// Neutre au sens fort : ni scene, ni fichier, ni projet. Ce que la bande
		// affiche est un TABLEAU PLAT — il n'y a pas de hierarchie d'onglets.
		struct NkTabItem {
				/// L'IDENTITE, OPAQUE au composant. Un indice de document, un
				/// pointeur de fichier, une entite : le composant ne sait pas ce que
				/// c'est, il sait seulement que **ca ne change pas quand le libelle
				/// change**. C'est la lecon commune aux quatre copies : le modeleur
				/// RENOMME dans l'onglet, donc le libelle ne peut pas identifier.
				/// `0` est reserve : « aucun onglet ».
				nk_uint64 id = 0;

				NkString label;

				/// ⚠️ LE POINT « NON ENREGISTRE ». Il vaut pour TOUT onglet, y
				///    compris le DERNIER — et c'est une lecon payee chez le
				///    modeleur : la pastille y etait imbriquee SOUS la condition de
				///    la croix, si bien qu'avec une seule scene — le cas le plus
				///    courant — aucune modification n'etait jamais signalee
				///    (constate par Rihen). Ici les deux conditions sont
				///    INDEPENDANTES, et la structure le rend visible : deux champs.
				bool modified = false;

				/// Cet onglet peut-il etre ferme. Le modeleur interdit de fermer le
				/// dernier (l'application resterait sans document) ; NKCode
				/// l'autorise. C'est donc a l'HOTE de le dire, par onglet.
				bool closable = true;

				/// Role de theme de la NATURE de l'onglet (scene, materiau, texture,
				/// graphe...) : un liseret fin sur l'onglet. `0` = aucun, et c'est le
				/// defaut. Meme decision que `NkTreeNode::kindRole` : une enumeration
				/// commune obligerait chaque nature neuve a modifier le kit.
				uint16 kindRole = 0;

				/// Infobulle de CET onglet. Vide = aucune. **Rapportee, pas peinte**
				/// (meme regle que l'arbre) : seul l'hote sait ou est le bord de la
				/// fenetre et quelle couche est au-dessus.
				NkString infobulle;
		};

		// ── LE MODELE ───────────────────────────────────────────────────────────
		struct NkTabStripModel {
				NkVector<NkTabItem> tabs;

				/// L'onglet ACTIF, par IDENTITE et non par indice. Un indice se
				/// perime des qu'on ferme un onglet a sa gauche — le defaut classique
				/// de cette famille de composants.
				nk_uint64 active = 0;
		};

		/// La representation. `Documents` = la bande du modeleur et de NKCode ;
		/// `Segmente` = les onglets de VUE de `NkAiPanel` (pas de croix, pas de `+`,
		/// largeurs egales). Deux usages REELS mesures dans le depot, devenus deux
		/// variantes plutot que deux composants.
		enum class NkTabVariant : uint8 { Documents = 0, Segmente, Count };

		// ── LE STYLE ────────────────────────────────────────────────────────────
		struct NkTabStripStyle {
				NkTabVariant variant = NkTabVariant::Documents;

				uint16 bandBg = 0;	   ///< fond de la bande
				uint16 border = 0;	   ///< trait de separation bas + filets inter-onglets
				uint16 tabBg = 0;	   ///< onglet au repos
				uint16 tabHoverBg = 0; ///< onglet survole
				uint16 tabActiveBg = 0;
				uint16 text = 0;	  ///< libelle de l'onglet actif
				uint16 textMuted = 0; ///< libelles des inactifs, croix, pastille
				uint16 accent = 0;	  ///< le lisere de l'onglet actif

				/// La source des nombres. A `nullptr`, le dessin lit les defauts de la
				/// declaration : une application qui n'a rien a regler ne cree pas
				/// d'instance et ne paie rien.
				const NkComponentInstance *values = nullptr;
		};

		/// Les trois lectures. Le dessin n'ecrit JAMAIS un nombre : il passe par ici.
		inline float32 NkTabMetric(const NkTabStripStyle &s, const char *name);
		inline float32 NkTabParam(const NkTabStripStyle &s, const char *name);
		inline NkTabVariant NkTabEffectiveVariant(const NkTabStripStyle &s);

		// ── LES POINTS DE GREFFE ────────────────────────────────────────────────
		struct NkTabStripHooks {
				void *user = nullptr;

				/// Dessin SUPPLEMENTAIRE sur un onglet, appele APRES le composant.
				void (*tabOverlay)(void *user, NkComponentPaint &p, int32 index, float32 x, float32 y,
								   float32 w, float32 h) = nullptr;
		};

		/// La geometrie RAPPORTEE d'un onglet : ce dont l'hote a besoin pour poser
		/// son champ de renommage (le composant n'a pas le clavier) ou son propre
		/// dessin. En coordonnees ecran.
		struct NkTabRect {
				nk_uint64 id = 0;
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
				/// La zone du LIBELLE seule — deja amputee de la reserve de droite
				/// (pastille/croix). Un editeur pose sur `x/w` recouvrirait la croix.
				float32 labelX = 0.f, labelW = 0.f;
		};

		// ── LE RESULTAT ─────────────────────────────────────────────────────────
		struct NkTabStripResult {
				/// L'onglet actif a change — `m.active` est DEJA a jour.
				bool selectionChanged = false;

				/// Une fermeture est DEMANDEE. ⚠️ Le composant ne ferme pas : il ne
				/// sait pas ce qu'un onglet represente, ni s'il faut demander quelque
				/// chose avant. L'hote agit APRES le dessin — jamais pendant, sinon il
				/// modifie la liste qu'il est en train de parcourir.
				bool closeRequested = false;
				nk_uint64 closeId = 0;

				/// Le `+` a ete presse.
				bool addRequested = false;

				/// Clic droit ; `contextId` vaut 0 sur le fond de la bande.
				bool contextMenu = false;
				nk_uint64 contextId = 0;

				/// L'onglet SURVOLE et son infobulle — rapportee, pas peinte.
				nk_uint64 hoveredId = 0;
				NkString infobulle;

				/// La geometrie de chaque onglet emis, dans l'ordre du modele.
				NkVector<NkTabRect> tabs;

				/// Largeur REELLEMENT occupee par la bande (onglets + `+`). Permet a
				/// l'hote de savoir s'il deborde — il n'y a pas de defilement ici, et
				/// c'est dit plutot que tu.
				float32 usedW = 0.f;
				/// Vrai si le contenu depasse le rectangle : l'hote peut le signaler.
				bool overflow = false;
		};

		// ── LA SIGNATURE TYPE, A SIX ARGUMENTS ──────────────────────────────────
		//     resultat Dessiner( PEINTRE, ENTREE, RECTANGLE, MODELE, STYLE, GREFFES )
		NkTabStripResult NkDrawTabStrip(NkComponentPaint &p, const NkComponentInput &in,
										const NkPaintRect &rect, NkTabStripModel &m,
										const NkTabStripStyle &s, const NkTabStripHooks &hooks);

		// ── LA DECLARATION ──────────────────────────────────────────────────────
		inline const NkComponentDecl &NkTabStripDecl() {
			static const NkVariantDecl kVariants[] = {
				{"documents", "Documents",
				 "largeur au contenu, croix de fermeture, point « non enregistre », bouton +"},
				{"segmente", "Segmente",
				 "largeurs egales, ni croix ni + — les onglets de VUE de NkAiPanel, devenus une "
				 "representation"},
			};

			static const NkParamDecl kParams[] = {
				{"show_add_button", "Bouton +", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_close", "Croix de fermeture", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				// ⚠️ LA PASTILLE ET LA CROIX PARTAGENT LE MEME EMPLACEMENT, et c'est
				//    la lecon du modeleur : la largeur de l'onglet ne doit PAS bouger
				//    quand un document devient modifie. Des onglets qui changent de
				//    taille a la premiere frappe rendent la bande illisible.
				{"show_modified_dot", "Point « non enregistre »", NkParamKind::Bool, 1.f, 0.f, 0.f,
				 nullptr, 0},
				{"show_separators", "Filets entre onglets", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				// ⚠️ DEUX SIGNAUX POUR L'ACTIF, ET C'EST §6 DU DOCUMENT 3 QUI L'EXIGE :
				//    « fond legerement different PLUS petit lisere accent ». Un fond
				//    seul se perd sur un ecran mal calibre ; un lisere seul disparait
				//    sous une barre. Defaut VRAI : c'est la planche.
				//    ⚠️ Le modeleur n'en a qu'un aujourd'hui (le fond). Il gagne donc
				//       le second en adoptant ce composant — c'est une AMELIORATION
				//       visible, et elle est dite ici pour qu'on ne la prenne pas pour
				//       une regression du recopiage.
				{"active_accent_bar", "Lisere accent sur l'onglet actif", NkParamKind::Bool, 1.f, 0.f,
				 0.f, nullptr, 0},
				// ⚠️ EN BAS, PAS EN HAUT (§6) : un lisere pose en haut se lit comme la
				//    separation d'avec la barre de menu, pas comme l'etat de l'onglet.
				//    Reglable parce que le liseret de NATURE du modeleur, lui, est en
				//    HAUT (choix de Rihen) — les deux coexistent sans se confondre.
				{"accent_bar_bottom", "Le lisere de l'actif est EN BAS", NkParamKind::Bool, 1.f, 0.f,
				 0.f, nullptr, 0},
				{"kind_bar_top", "Le liseret de NATURE est EN HAUT", NkParamKind::Bool, 1.f, 0.f, 0.f,
				 nullptr, 0},
			};

			static const NkTokenDecl kTokens[] = {
				{"band_bg", "panel_bg", "fond de la bande entiere"},
				{"border", "border", "trait bas de la bande et filets entre onglets"},
				{"tab_bg", "input_bg", "onglet au repos"},
				{"tab_hover_bg", "panel_bg", "onglet survole"},
				{"tab_active_bg", "panel_header", "onglet actif"},
				{"text", "text", "libelle de l'onglet actif"},
				{"text_muted", "text_muted", "libelles inactifs, croix, point « non enregistre »"},
				{"accent", "accent_ui", "lisere de l'onglet actif"},
			};

			static const NkMetricDecl kMetrics[] = {
				// ⚠️ 28 ET NON 22 : c'est la hauteur MESUREE chez le modeleur
				//    (`NkLayout::Compute`, `tabsH = S(28.f)`) et celle que le costume
				//    Banani donne au TopHeader de NkUIDesign. Elle est ici pour que la
				//    coquille la LISE au lieu de la reecrire.
				{"band_h", 28.f, "hauteur de la bande — 28 px, la cote du modeleur"},
				{"band_pad_x", 10.f, "marge gauche avant le premier onglet"},
				{"tab_inset_y", 2.f, "retrait haut/bas de l'onglet dans la bande"},
				{"tab_pad_x", 44.f, "largeur ajoutee au libelle pour faire l'onglet"},
				{"label_pad_x", 10.f, "marge gauche du libelle dans l'onglet"},
				{"label_reserve", 32.f, "largeur retranchee a droite du libelle (pastille/croix)"},
				{"tab_gap", 3.f, "ecart entre deux onglets"},
				{"sep_gap", 5.f, "ecart ajoute quand un filet de separation est dessine"},
				{"sep_inset_y", 6.f, "retrait vertical du filet de separation"},
				{"tab_rounding", 3.f, "arrondi des coins de l'onglet"},
				{"kind_bar_h", 2.f, "epaisseur du liseret de NATURE"},
				{"accent_bar_h", 2.f, "epaisseur du lisere de l'onglet actif"},
				{"close_w", 20.f, "largeur de la zone cliquable de la croix"},
				{"close_right", 24.f, "distance du bord droit de l'onglet a cette zone"},
				{"close_mark", 10.f, "taille du trace de la croix"},
				{"dot_d", 7.f, "diametre du point « non enregistre »"},
				{"add_w", 24.f, "largeur du bouton +"},
				{"add_pad", 4.f, "ecart avant le bouton +"},
				{"add_mark", 12.f, "taille du trace du +"},
				{"stroke_w", 1.f, "epaisseur d'un trait"},
				{"seg_min_w", 60.f, "largeur minimale d'un onglet de la variante segmentee"},
			};

			static const NkHookDecl kHooks[] = {
				{"tab_overlay", "(user, peintre, index, x, y, w, h) -> void",
				 "dessin ajoute par l'application par-dessus un onglet"},
			};

			static const char *const kNoNames[] = {nullptr};
			(void)kNoNames;

			static const NkArgDecl kArgsTab[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"id", NkArgKind::Int, nullptr, 0},
			};

			static const NkEventDecl kEvents[] = {
				// ⚠️ `hasDefaultAction = true` : le composant a DEJA ecrit `m.active`
				//    quand l'evenement part. Meme choix que `tree_view`, meme raison :
				//    eviter la question « pourquoi ca a deja bouge ».
				{"onSelect", "Selection", "l'onglet actif a change — le modele est deja a jour",
				 kArgsTab, 2, true},
				{"onClose", "Fermeture demandee",
				 "la croix a ete pressee ; le composant NE ferme PAS, il signale", kArgsTab, 2, false},
				{"onAdd", "Nouvel onglet", "le bouton + a ete presse", nullptr, 0, false},
				{"onContextMenu", "Menu contextuel", "clic droit ; `id` vaut 0 sur le fond de la bande",
				 kArgsTab, 2, false},
			};

			// ⚠️ LES ONGLETS EUX-MEMES NE SONT PAS DANS CET ARBRE, et c'est le MEME
			//    mur que `tree_view` (ses lignes) et `content_browser` (ses cartes) :
			//    un arbre de sous-elements STATIQUE ne sait decrire aucune REPETITION
			//    PAR LA DONNEE. Troisieme composant, troisieme fois. Ce n'est plus une
			//    remarque, c'est un fait sur la forme — signale au canal.
			static const NkElementDecl kElements[] = {
				{"tab_strip", "", "la bande entiere", "container", "", NkExpand(), NkFixedM("band_h"),
				 {NkLayoutKind::Row, "", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
				{"tabs", "tab_strip", "la suite d'onglets — leur nombre vient du modele", "list", "",
				 NkExpand(), NkExpand(), {}, 0},
				{"add", "tab_strip", "le bouton +", "button", "", NkFixedM("add_w"), NkExpand(), {}, 0},
			};

			static const NkComponentDecl kDecl = {
				/* name        */ "tab_strip",
				/* title       */ "Bande d'onglets",
				/* summary     */ "les documents ouverts : selection, fermeture, point « non "
								  "enregistre », liseret de nature, bouton +",
				/* params      */ kParams,   (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				/* variants    */ kVariants, (uint16)(sizeof(kVariants) / sizeof(kVariants[0])),
				/* tokens      */ kTokens,   (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				/* metrics     */ kMetrics,  (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				/* hooks       */ kHooks,	 (uint16)(sizeof(kHooks) / sizeof(kHooks[0])),
				/* events      */ kEvents,   (uint16)(sizeof(kEvents) / sizeof(kEvents[0])),
				/* role        */ "list",
				/* elements    */ kElements, (uint16)(sizeof(kElements) / sizeof(kElements[0])),
				// Ecrite dans ce depot a partir des quatre copies mesurees, JAMAIS
				// rejouee contre une planche. `verified = false` ne dit pas « fausse » :
				// il dit « pas encore passee au juge ».
				/* provenance  */ NkProvenance{NkAuthorKind::Human, false, false},
			};
			return kDecl;
		}

		// ── LES TROIS LECTURES, DEFINIES ────────────────────────────────────────
		inline float32 NkTabMetric(const NkTabStripStyle &s, const char *name) {
			return s.values ? s.values->Metric(name) : NkTabStripDecl().Metric(name);
		}
		inline float32 NkTabParam(const NkTabStripStyle &s, const char *name) {
			return s.values ? s.values->Param(name) : NkTabStripDecl().Param(name);
		}
		inline NkTabVariant NkTabEffectiveVariant(const NkTabStripStyle &s) {
			if (s.values && s.values->Decl()) {
				const int32 v = s.values->Variant();
				if (v >= 0 && v < (int32)NkTabVariant::Count)
					return (NkTabVariant)v;
			}
			return s.variant;
		}

	} // namespace editorkit
} // namespace nkentseu
