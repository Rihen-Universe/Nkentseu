#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiEcrire.h
// @Brief   LE MAILLON QUI MANQUAIT : NKUIDesign ECRIT un `.nkgui`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EXISTAIT DEJA, ET QUE CE FICHIER N'ECRIT DONC PAS
// =============================================================================
//  Mesure du 2026-09-14, par ouverture des fichiers et non par comptage :
//
//    l'ecrivain de TEXTE `.nkgui`   `NkGuiArchive::Write`   declare NkGuiArchive.h:312,
//                                   implemente NkGuiArchive.cpp:2021 -- il EXISTE
//    le lecteur                     `NkGuiArchive::Read`    2 030 lignes
//    le banc d'aller-retour         `NkGuiRoundTrip.h`      2 414 lignes, et il lit
//                                   deja par `ReadAllBytes` (l.297)
//    le monteur                     `NKGui/Doc/NkGuiMonteur.h`
//
//  Ce qui manquait n'etait donc PAS « l'ecrivain » : c'etait le PONT entre le
//  document de l'editeur (`NkUIDocument`, un arbre de DESSINS) et l'archive que
//  cet ecrivain sait deja mettre en texte. **Aucune fonction du depot ne
//  construisait un `NkArchive` a partir d'un `NkUIDocument`.** Ce fichier est ce
//  pont, et rien d'autre.
//
// =============================================================================
//  TROIS ECARTS DE NATURE ENTRE LES DEUX FORMATS -- C'EST TOUT LE TRAVAIL
// =============================================================================
//    .nkuidoc                              .nkgui 0.3
//    ----------------------------------    ---------------------------------
//    PLAT : `noeud 3` + `enfants = 4 5`    IMBRIQUE : le bloc contient ses fils
//    `forme = rect|text|frame` (DESSIN)    un TYPE DE WIDGET (`Panel`, `Text`...)
//    `largeur = fixed 240 0 0`             des membres nommes
//
//  La deuxieme ligne est la seule ou il faut DECIDER, et la decision est ecrite
//  dans `NkRoleNkguiDuNoeud` -- une table, lisible, a un seul endroit.
//
// =============================================================================
//  CE QUE CE PONT NE TRANSPORTE PAS, ET IL FAUT LE LIRE AVANT DE S'EN SERVIR
// =============================================================================
//  Le `.nkgui` porte huit sections ; ce pont n'ecrit QUE `widgets`. Ne sont pas
//  ecrits, et aucun n'est un oubli :
//   - `behavior` / `animation` / `controller` / `callback` : le document de
//     l'editeur ne porte AUCUN comportement. On ne peut pas ecrire ce qui
//     n'existe pas en amont -- c'est le chantier suivant, cote EDITEUR ;
//   - `geometry` : le monteur ne la monte pas (il ecarte toute section qui n'est
//     ni `widgets`, ni `behavior`, ni `animation`). L'ecrire serait produire une
//     section que rien ne relit ;
//   - les COULEURS du document (`fond`, `couleur_texte`, remplissages, bordures,
//     effets). Elles sont ecrites en `appearance`, et **le monteur les ecarte
//     aujourd'hui** (`NkGuiMonteur.h:144`) : elles voyagent dans le fichier,
//     elles ne se peignent pas encore. Ecrites quand meme -- un fichier qui perd
//     la couleur est un fichier faux, meme si le consommateur du jour l'ignore ;
//   - les TRANSFORMATIONS (rotation, miroirs, inclinaison, perspective,
//     opacite, fusion) : le format `.nkgui` n'a pas de vocabulaire pour elles.
//     Elles sont COMPTEES et nommees dans le rapport, jamais devinees.
//
//  ⚠️ ET LE PLACEMENT. Le monteur place au CURSEUR (flux). Le seul role auquel
//     le FORMAT accorde `pos`/`size` est `Window` (pWindow, NkGuiValidate.h:248 ;
//     `pPanel` ne declare que `title`, l.251) -- et c'est aussi ce que le monteur
//     honore (`RegionCourante`, NkGuiMonteur.h:518/802, ou `Window` et `Panel`
//     partagent la meme branche). Ce pont ecrit donc `Window` pour tout conteneur
//     dont la place compte, et les FEUILLES retombent dans le flux. **Les deux
//     images ne coincideront donc pas sur les feuilles**, et ce n'est pas un
//     defaut de l'ecrivain : c'est la limite du montage, ecrite ici pour qu'on ne
//     la redecouvre pas devant une capture.
//
//  ⚠️ L'ECRITURE PASSE PAR `WriteAllBytes`, JAMAIS PAR `WriteAllText`.
//     `WriteAllText` ecrit en CRLF et `ReadAllText` renormalise : l'ecart serait
//     masque des DEUX cotes a la fois, et un aller-retour mesure ainsi serait
//     vert sans rien prouver.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIECRIRE_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIECRIRE_H__

#include "NKFileSystem/NkFile.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

// ⚠️ IL UTILISE `NkGFindRole` / `NkGFindAlias`, DONC IL LES INCLUT. Mesure du
//    17/09 : ce fichier compilait chez son unique consommateur (NKUIDesign)
//    seulement parce que `main.cpp` avait inclus le validateur plus tot. Le
//    premier consommateur a ne pas l'avoir eu par hasard (NKDesignIABanc) a
//    recolte deux « undeclared identifier ». Un en-tete partage inclut ce
//    qu'il utilise.
#include "NkGuiValidate.h"

#include "Document.h"
#include "Layout.h"

// ⚠️ IL UTILISE `std::getenv` (la mutation `NK_PONT_SANS_COMPOSANT`), DONC IL
//    L'INCLUT. Meme regle que l'include de `NkGuiValidate.h` ci-dessus : un
//    en-tete partage inclut ce qu'il utilise, sinon il compile par accident chez
//    celui qui l'a eu par un autre chemin et casse chez le suivant.
#include <cstdlib>

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkFile;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkGuiStyle;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;

		// =====================================================================
		//  LE RAPPORT -- un ecrivain muet ne se distingue pas d'un ecrivain vide
		// =====================================================================
		/// CE QUE L'ECRIVAIN A MIS DANS LE FICHIER, widget par widget.
		///
		/// ⚠️ IL EXISTE POUR QUE (e3) SE MESURE EN COORDONNEES, PAS EN PIXELS. Le
		///    monteur produit deja `NkGuiMonteItem{id, role, rect}` ; en produire
		///    le pendant du cote ecrivain permet de comparer les deux releves PAR
		///    IDENTIFIANT. Un compteur de pixels dit « 12 000 pixels different » ;
		///    ces deux releves disent QUEL widget est ou -- et c'est la liste qui
		///    vaut, pas le chiffre.
		struct NkEcritItem {
				NkString id;
				NkString role;
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
				bool aRect = false; ///< faux quand la disposition n'a rien pour ce noeud
				int32 noeud = -1;	///< l'indice dans le document, pour remonter a la source
		};

		struct NkEcritRapport {
				uint32 noeudsVus = 0;	  ///< noeuds du document parcourus (racine exclue)
				uint32 widgetsEcrits = 0; ///< blocs `widgets` produits
				uint32 conteneurs = 0;	  ///< ceux qui portent des fils
				uint32 feuilles = 0;
				uint32 rolesHorsCatalogue = 0; ///< `role = ...` hors vocabulaire du doc 7
				uint32 transfoAbandonnees = 0; ///< rotation / miroir / inclinaison / fusion
				uint32 apparencesEcrites = 0;  ///< blocs `appearance` produits
				/// Les noeuds texte dont le mot est parti dans le `label` du parent
				/// au lieu de devenir un widget. Ils ne sont PAS perdus : ils sont
				/// dans le fichier, en propriete.
				uint32 textesAbsorbes = 0;
				/// Les fils ecrits sous un role que le monteur ne traverse pas.
				/// Non nul = le fichier porte plus que ce que le montage rend.
				uint32 filsSousUneFeuille = 0;
				/// Les couleurs de TEXTE qu'un noeud a fond n'a pas pu ecrire : une
				/// `appearance` n'a qu'un `fill`. Comptees, jamais tues en silence.
				uint32 encresPerdues = 0;
				/// Les noeuds dont la place n'a PAS pu etre ecrite parce que leur parent
				/// porte un role dont le NOM dit deja l'agencement (`VBox`, `Grid`...).
				/// Le format interdit `placement` sur ces roles-la -- a juste titre -- donc
				/// leurs enfants retombent dans le flux. Comptes, jamais tus.
				uint32 placementsPerdus = 0;
				/// Les conteneurs auxquels `placement = absolute` a ete ecrit.
				uint32 conteneursAbsolus = 0;
				/// Les noeuds dont `pos` a ete ecrit.
				uint32 posesEcrits = 0;
				/// Les noms hors catalogue, SANS doublon. **Signaler, jamais migrer** :
				/// la graphie des roles touche des fichiers deja enregistres, et cette
				/// decision appartient a Rodolf.
				NkVector<NkString> nomsHorsCatalogue;
				/// Les noeuds qui portent un `composant` du registre AUQUEL LE
				/// VOCABULAIRE `.nkgui` N'A PAS D'EQUIVALENT (les composites du kit :
				/// un navigateur de contenu n'est pas un `ListBox`, il porte un arbre,
				/// un fil d'Ariane et une recherche). **Comptes, jamais devines** :
				/// leur donner un role approchant produirait un fichier qui se monte
				/// en montrant autre chose que ce qui a ete demande -- la pire des
				/// trois issues, parce qu'elle est VERTE.
				uint32 composantsSansRole = 0;
				NkVector<NkString> nomsComposantsSansRole;
				/// Les noeuds dont le role `.nkgui` a ete derive de leur `composant`.
				/// Non nul = ce fichier vient de la palette ou de l'IA, pas du menu.
				uint32 rolesDuComposant = 0;
				/// Le releve de ce qui a ete ecrit -- voir `NkEcritItem`.
				NkVector<NkEcritItem> items;

				void SignalerHorsCatalogue(const NkString &n) {
					for (uint32 i = 0; i < (uint32)nomsHorsCatalogue.Size(); ++i) {
						if (nomsHorsCatalogue[i].Compare(n) == 0) {
							return;
						}
					}
					nomsHorsCatalogue.PushBack(n);
					++rolesHorsCatalogue;
				}

				void SignalerComposantSansRole(const NkString &n) {
					for (uint32 i = 0; i < (uint32)nomsComposantsSansRole.Size(); ++i) {
						if (nomsComposantsSansRole[i].Compare(n) == 0) {
							return;
						}
					}
					nomsComposantsSansRole.PushBack(n);
					++composantsSansRole;
				}
		};

		// =====================================================================
		//  LA DECISION : QUEL ROLE `.nkgui` POUR CE NOEUD
		// =====================================================================
		//
		// ⚠️ LE `role` D'UN `.nkuidoc` EST UNE CHAINE D'AFFICHAGE FRANCAISE.
		//    Mesure du 12/09 : le menu ecrit dans le document le LIBELLE qu'il
		//    affiche (`bouton a repetition`, `case a trois etats`). Les fichiers
		//    livres, eux, portent `role = Button` -- une graphie qu'aucun geste
		//    d'interface ne produit. Les deux vocabulaires existent donc VRAIMENT
		//    sur le disque de Rodolf, et un pont qui n'en connaitrait qu'un
		//    perdrait la moitie des roles sans un mot.
		//
		//    Ce pont accepte les DEUX, ne convertit RIEN dans le document, et
		//    SIGNALE ce qu'il ne reconnait pas. Il ne choisit pas a la place de
		//    Rodolf : unifier la graphie toucherait des fichiers enregistres.

		/// La table francais -> vocabulaire du document 7. Les libelles viennent de
		/// `MenuRole.h` (ceux que le menu ECRIT reellement), pas d'une invention.
		struct NkRoleFr {
				const char *libelle;
				const char *nkgui;
		};

		inline const NkRoleFr *NkTableRolesFr(uint32 &n) {
			static const NkRoleFr k[] = {
				// Actions
				{"bouton", "Button"},
				{"bouton \xC3\xA0 r\xC3\xA9p\xC3\xA9tition", "RepeatButton"},
				{"bouton image", "ImageButton"},
				{"bouton de couleur", "ColorField"},
				{"\xC3\xA9l\xC3\xA9ment de menu", "MenuItem"},
				// Saisie
				{"champ de saisie", "TextField"},
				{"champ multiligne", "TextField"},
				{"champ entier", "NumberField"},
				{"champ d\xC3\xA9""cimal", "NumberField"},
				{"curseur", "Slider"},
				{"glisseur", "Drag"},
				{"s\xC3\xA9lecteur de couleur", "ColorField"},
				// Selection
				{"case \xC3\xA0 cocher", "Checkbox"},
				{"case \xC3\xA0 trois \xC3\xA9tats", "Checkbox"},
				{"liste d\xC3\xA9roulante", "Dropdown"},
				{"liste", "ListBox"},
				{"\xC3\xA9l\xC3\xA9ment s\xC3\xA9lectionnable", "Item"},
				// Navigation
				{"barre de menu", "MenuBar"},
				{"menu", "Menu"},
				{"menu contextuel", "ContextMenu"},
				{"en-t\xC3\xAAte repliable", "Expander"},
				// Conteneurs
				{"fen\xC3\xAAtre", "Window"},
				{"panneau", "Panel"},
				{"groupe", "Group"},
				{"bo\xC3\xAEte verticale", "VBox"},
				{"bo\xC3\xAEte horizontale", "HBox"},
				{"grille", "Grid"},
				{"tableau", "Table"},
				{"zone d\xC3\xA9""filante", "Scroll"},
				// Affichage
				{"texte", "Text"},
				{"image", "Image"},
				{"barre de progression", "Progress"},
				{"courbe", "Chart"},
				{"s\xC3\xA9parateur", "Separator"},
				// Roles de projet : ils DERIVENT, et la derivation est ecrite dans
				// le menu lui-meme (« derive de case a cocher »). On suit la meme.
				{"interrupteur", "Switch"},
				{"bouton \xC3\xA0 bascule", "Checkbox"},
				{"carte cliquable", "Button"},
				{"champ recherche", "TextField"},
			};
			n = (uint32)(sizeof(k) / sizeof(k[0]));
			return k;
		}

		// =====================================================================
		//  LE `composant` -- LE TROISIEME VOCABULAIRE, ET LE SEUL QUE L'IA ECRIT
		// =====================================================================
		//
		// 🔴 MESURE DU 18/09, ET ELLE OUVRE CE BLOC. Le dorsal TEMOIN (une reponse
		//    fixe et JUSTE) a passe les trois niveaux du jeu d'epreuve -- 12/12,
		//    12/12, 12/12 -- et le fichier produit etait :
		//
		//        Group "Ecran_1" { Group "Titre_2" { } Group "Valider_3" { } }
		//
		//    La reponse disait `composant = etiquette` et `composant = bouton`.
		//    **Les deux sont sortis en `Group`.** Trois taux verts, et un document
		//    qui ne montre rien : c'est le compteur `roles utiles` de
		//    `NKGuiMonteTest --monter=` qui l'a attrape, a sa premiere course.
		//
		// ⚠️ LA CAUSE EST UNE CONFUSION DE CHAMPS, PAS UN OUBLI DE TABLE.
		//    `NkUINode` porte DEUX cles distinctes (`Document.h`) :
		//      `component` -- la cle du REGISTRE, ce que `composant = ...` ecrit ;
		//      `role`      -- la taxonomie d'ecran, ce que le MENU ecrit.
		//    Les trois regles de `NkRoleNkguiDuNoeud` ne regardaient que `role` et
		//    `shape`. Or l'invite envoyee au modele (`DesignAI.h`) ne propose QUE
		//    `composant = <nom du catalogue>` : le mot `role` n'y figure meme pas.
		//    **Le pont etait donc aveugle, par construction, a tout ce qu'un modele
		//    peut produire.** Ce n'etait pas une regression : ce chemin n'avait
		//    jamais eu de premier producteur.
		//
		// ⚠️ LE RISQUE DE REGRESSION EST NUL, ET COMPTE AVANT D'ECRIRE : sur les
		//    NEUF `.nkuidoc` versionnes, **zero** noeud porte un `composant` non
		//    vide. La regle ci-dessous ne peut donc changer aucun fichier existant ;
		//    elle ne sert qu'aux documents que l'IA et la palette produisent.
		//
		// ⚠️ ET ELLE NE DEVINE PAS LES COMPOSITES. Huit des dix composants du
		//    catalogue ont un equivalent evident. `content_browser` et `tree_view`
		//    n'en ont pas : un navigateur de contenu n'est pas un `ListBox`. Ils
		//    sont SIGNALES (`composantsSansRole`) et le pont retombe sur la regle
		//    suivante. Un refus nomme est un succes ; un repli muet est le defaut
		//    que ce depot passe son temps a retirer.
		//
		//    CONDITION DE RETRAIT DE CETTE TABLE : le jour ou `NkComponentDecl`
		//    portera lui-meme son role `.nkgui`, elle disparait -- c'est la porte
		//    propre, et elle vit dans NKEditorKit, que cinq applications partagent.

		struct NkComposantNkgui {
				const char *composant;
				const char *nkgui;
		};

		inline const NkComposantNkgui *NkTableComposantsNkgui(uint32 &n) {
			static const NkComposantNkgui k[] = {
				{"bouton", "Button"},		  {"champ_texte", "TextField"},
				{"case_a_cocher", "Checkbox"}, {"interrupteur", "Switch"},
				{"barre_progression", "Progress"}, {"etiquette", "Text"},
				{"separateur", "Separator"},  {"carte", "Panel"},
			};
			n = (uint32)(sizeof(k) / sizeof(k[0]));
			return k;
		}

		/// Le role `.nkgui` d'une cle de registre. Rend `nullptr` quand le
		/// composant existe mais n'a pas d'equivalent -- l'appelant SIGNALE alors.
		///
		/// ⚠️ LA MUTATION VIT DANS L'INSTRUMENT, pas dans une modification
		///    temporaire du code : `NK_PONT_SANS_COMPOSANT=1` eteint la table. Elle
		///    reste dans le depot, donc n'importe qui peut reprouver que le critere
		///    mord -- un negatif qu'il faut reecrire est un negatif qu'on ne refait
		///    jamais.
		inline const char *NkRoleNkguiDuComposant(const char *comp) {
			if (!comp || !*comp) {
				return nullptr;
			}
			if (const char *m = std::getenv("NK_PONT_SANS_COMPOSANT")) {
				if (m[0] == '1') {
					return nullptr;
				}
			}
			uint32 nb = 0;
			const NkComposantNkgui *t = NkTableComposantsNkgui(nb);
			for (uint32 i = 0; i < nb; ++i) {
				if (NkComponentDecl::StrEq(t[i].composant, comp)) {
					return t[i].nkgui;
				}
			}
			return nullptr;
		}

		/// Vrai si `nom` est du vocabulaire `.nkgui`.
		///
		/// ⚠️ LA RECHERCHE N'EST PAS REECRITE ICI. `NkGFindRole` et `NkGFindAlias`
		///    existent dans `NkGuiValidate.h` et interrogent LA table du validateur.
		///    En recopier une seconde boucle donnerait deux vocabulaires qui
		///    derivent -- exactement ce que ce chantier constate deja sur les roles.
		///    Les alias du document 7 §5 comptent : un fichier qui les emploie est
		///    valide, donc un pont qui les refuserait serait plus severe que le format.
		inline bool NkRoleConnuNkgui(const char *nom) {
			if (!nom || !*nom) {
				return false;
			}
			const NkString r(nom);
			return NkGFindRole(r) != nullptr || NkGFindAlias(r) != nullptr;
		}

		/// LE ROLE `.nkgui` D'UN NOEUD, et la regle se lit en trois lignes :
		///   1. un `role` deja ecrit dans le vocabulaire du format -> il passe tel quel ;
		///   2. un `role` en libelle francais -> sa traduction de la table ;
		///   3. pas de role exploitable -> on retombe sur la NATURE DESSINEE.
		/// `horsCatalogue` est pose quand le noeud portait un role que ni (1) ni (2)
		/// ne reconnaissent : la valeur est alors SIGNALEE et le pont retombe sur (3).
		inline const char *NkRoleNkguiDuNoeud(const NkUINode &n, bool &horsCatalogue,
											  bool &composantSansRole, bool &roleDuComposant) {
			horsCatalogue = false;
			composantSansRole = false;
			roleDuComposant = false;
			const char *r = n.role.Data();
			if (r && *r) {
				if (NkRoleConnuNkgui(r)) {
					return r;
				}
				uint32 nb = 0;
				const NkRoleFr *t = NkTableRolesFr(nb);
				for (uint32 i = 0; i < nb; ++i) {
					if (NkComponentDecl::StrEq(t[i].libelle, r)) {
						return t[i].nkgui;
					}
				}
				horsCatalogue = true; // on le DIT, puis on retombe sur la forme
			}
			// (2bis) LE `composant` DU REGISTRE -- la seule cle que l'IA ecrive.
			//        Voir le bloc « LE TROISIEME VOCABULAIRE » plus haut.
			const char *c = n.component.Data();
			if (c && *c) {
				if (const char *rc = NkRoleNkguiDuComposant(c)) {
					roleDuComposant = true;
					return rc;
				}
				// Le composant existe et n'a pas d'equivalent : on le DIT, et on
				// retombe sur la forme plutot que d'inventer un role approchant.
				composantSansRole = true;
			}
			// (3) LA NATURE DESSINEE. Le vocabulaire est celui de `NkUINode::shape`
			//     (specification §4.2), et la correspondance est celle qui perd le
			//     moins : un cadre porte des fils -> `Panel` ; un texte -> `Text` ;
			//     une image -> `Image` ; une ligne -> `Separator` ; toute autre forme
			//     est un APLAT, et le seul role qui peint un aplat en portant des
			//     fils est `Panel`.
			const char *s = n.shape.Data();
			if (!s || !*s) {
				// Ni role ni forme : un cadre d'agencement pur. Il ne peint rien et
				// porte des fils -> `Group` (le monteur l'ouvre sans peindre de fond).
				return "Group";
			}
			if (NkComponentDecl::StrEq(s, "text")) {
				return "Text";
			}
			if (NkComponentDecl::StrEq(s, "image") || NkComponentDecl::StrEq(s, "avatar")) {
				return "Image";
			}
			if (NkComponentDecl::StrEq(s, "line")) {
				return "Separator";
			}
			// 🔴 UN APLAT DEVENAIT UNE `Window`, ET C'ETAIT UN CONTOURNEMENT.
			//    Ce matin, le seul role auquel le format accordait `pos`/`size` etait
			//    `Window` : tout rectangle du document devait donc en devenir une, et
			//    le fichier produit portait VINGT-DEUX `Window` pour une maquette qui
			//    n'a qu'une fenetre. La graphie disait le format, pas le document.
			//
			//    Depuis que le placement est une propriete du CONTENEUR, la contrainte
			//    a disparu : n'importe quel role se pose sous un parent absolu. On
			//    peut donc ecrire ce que le document DIT :
			//      `frame`  = l'ARTBOARD de la toile (c'est le mot de `NkUINode::shape`
			//                 lui-meme) -> `Window`, le conteneur de premier plan ;
			//      tout autre aplat -> `Panel`, qui peint un fond et porte des fils.
			if (NkComponentDecl::StrEq(s, "frame")) {
				return "Window";
			}
			return "Panel";
		}

		// =====================================================================
		//  L'IDENTIFIANT -- stable, unique, et qui ne ment pas sur sa provenance
		// =====================================================================
		//
		// ⚠️ L'ID D'UN BLOC `.nkgui` EST LA CLE D'ETAT DU MONTEUR (`CleEtat` :
		//    le `bind` s'il existe, sinon l'id). Deux blocs qui portent le meme id
		//    partagent donc leur valeur. Le libelle d'un document N'EST PAS unique
		//    (`NkUINode::label` : « purement humain : deux noeuds peuvent porter le
		//    meme »). L'index du noeud, lui, l'est -- il est donc SUFFIXE au
		//    libelle, jamais remplace par lui : un id `n7` seul serait illisible
		//    dans le fichier produit.
		inline NkString NkIdDuNoeud(const NkUINode &n, int32 index) {
			NkString id;
			const char *l = n.label.Data();
			// On garde les caracteres lisibles du libelle ; le reste devient `_`.
			// L'id est ECRIT ENTRE GUILLEMETS dans le format, donc rien n'oblige a
			// le reduire a un identifiant C -- mais un id sans saut de ligne ni
			// guillemet se relit a l'oeil dans un diff.
			for (uint32 i = 0; l && l[i] && i < 48u; ++i) {
				const char c = l[i];
				if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
					id.Append('_');
				} else {
					id.Append(c);
				}
			}
			if (id.Empty()) {
				id.Append("noeud");
			}
			id.Append('_');
			id.Append(NkString::Fmtf("%d", (int)index));
			return id;
		}

		// =====================================================================
		//  ECRIRE UN NOEUD, PUIS SES FILS
		// =====================================================================

		/// `pos = (x, y)` / `size = (w, h)` : des JETONS NUS a deux nombres, la
		/// forme exacte que `NkGuiMonteur::LireVec2` sait relire.
		inline void NkPoserVec2(NkArchive &bloc, const char *cle, float32 a, float32 b) {
			NkString t;
			t.Append('(');
			t.Append(NkString::Fmtf("%g", (double)a));
			t.Append(", ");
			t.Append(NkString::Fmtf("%g", (double)b));
			t.Append(')');
			NkGuiArchive::SetToken(bloc, NkStringView(cle), NkStringView(t));
		}

		/// Vrai si ce role est un CONTENEUR au sens du monteur : c'est la liste du
		/// `switch` de `MonterCorps`, pas une devinette.
		inline bool NkRoleEstConteneur(const char *r) {
			return NkComponentDecl::StrEq(r, "Window") || NkComponentDecl::StrEq(r, "Panel")
				   || NkComponentDecl::StrEq(r, "Group") || NkComponentDecl::StrEq(r, "VBox")
				   || NkComponentDecl::StrEq(r, "HBox");
		}

		// =====================================================================
		//  LE SEUL CONTENEUR POSITIONNABLE DU FORMAT EST `Window`
		// =====================================================================
		//
		// 🔴 MESURE DU 2026-09-14 : ma premiere version ecrivait `pos`/`size` sur des
		//    `Panel`. Le MONTEUR les honorait (`RegionCourante` traite `Window` et
		//    `Panel` dans la meme branche) -- et le VALIDATEUR du depot rendait
		//    **81 erreurs** sur le fichier produit. En relisant la table :
		//
		//        pWindow = { title, pos, size, flags, modal }   NkGuiValidate.h:248
		//        pPanel  = { title }                            NkGuiValidate.h:251
		//
		//    Le format ne declare `pos`/`size` QUE sur `Window`. Le monteur est plus
		//    permissif que le format ; ecrire pour le monteur seul aurait produit un
		//    fichier qui se monte et que l'outil du depot refuse. **Un fichier n'est
		//    pas juste parce qu'un consommateur l'avale.**
		//
		// ⚠️ CONSEQUENCE ASSUMEE, ET NOMMEE : un conteneur dont la PLACE compte est
		//    ecrit `Window`, y compris imbrique. Le corpus ecrit a la main n'atteste
		//    aucun `Window` dans un `Window` -- c'est une lecture de la table, pas un
		//    usage constate, et la question de fond (« le format veut-il un second
		//    conteneur positionnable ? ») appartient a Rodolf, pas a ce pont.
		// =====================================================================
		//  LE PLACEMENT EST UNE PROPRIETE DU CONTENEUR
		// =====================================================================
		//
		// 🔴 CE BLOC REMPLACE `NkRoleHonorePos`, ET IL FAUT DIRE POURQUOI.
		//    Ma version du 14/09 au matin ecrivait `pos`/`size` sur le seul role
		//    auquel le format les accordait -- `Window` -- et forcait donc tout
		//    rectangle du document a devenir une `Window`. Vingt-deux `Window`
		//    dans un fichier qui n'a qu'une fenetre : la graphie etait un
		//    CONTOURNEMENT du format, pas une lecture du document.
		//
		//    Rodolf a tranche depuis : « au vu de son parent, un conteneur ne
		//    pourra jamais porter les deux ». Le placement est donc une propriete
		//    du CONTENEUR (`placement = absolute`), et ses enfants portent alors
		//    `pos` QUEL QUE SOIT LEUR ROLE. Le contournement disparait.
		//
		// ⚠️ ET LES COORDONNEES DEVIENNENT RELATIVES AU PARENT. La disposition
		//    calculee (`NkLayoutResult`) est en coordonnees de TOILE ; le format,
		//    lui, place un enfant DANS son conteneur -- sans quoi deplacer un
		//    dialogue laisserait son contenu sur place. On retranche donc
		//    l'origine du parent, et c'est la seule arithmetique de ce pont.
		
		/// Les trois conteneurs NEUTRES, seuls a pouvoir declarer leur placement.
		/// Les autres NOMMENT deja leur agencement (`VBox` empile, `Grid`
		/// quadrille) : le format leur refuse `placement`, et ce pont ne peut donc
		/// pas poser leurs enfants -- il les COMPTE (`placementsPerdus`).
		inline bool NkRolePorteUnPlacement(const char *r) {
			return NkComponentDecl::StrEq(r, "Window") || NkComponentDecl::StrEq(r, "Panel")
					   || NkComponentDecl::StrEq(r, "Group");
		}

		/// Un role qui porte son mot en PROPRIETE (`label`) et n'a pas d'enfants.
		inline bool NkRolePorteUnLibelle(const char *r) {
			return NkComponentDecl::StrEq(r, "Button") || NkComponentDecl::StrEq(r, "RepeatButton")
				   || NkComponentDecl::StrEq(r, "Checkbox") || NkComponentDecl::StrEq(r, "Switch")
				   || NkComponentDecl::StrEq(r, "MenuItem") || NkComponentDecl::StrEq(r, "Item")
				   || NkComponentDecl::StrEq(r, "ImageButton");
		}

		// =====================================================================
		//  LE TEXTE D'UN BOUTON VIT DANS UN ENFANT -- ET LE FORMAT LE VEUT DEDANS
		// =====================================================================
		//
		// 🔴 MESURE AVANT CORRECTIF (2026-09-14) : 41 widgets ecrits, 40 montes.
		//    Le manquant etait `Text "Texte du bouton_10"`, ENFANT de
		//    `Button "Bouton_Connexion_9"`. Ce n'etait un defaut ni de l'ecrivain
		//    ni du monteur : c'est un ecart de MODELE, et il se voit dans le
		//    document source --
		//        noeud 9  : forme = rect, role = Button, enfants = 10
		//        noeud 10 : forme = text, texte = Se connecter
		//    Dans l'editeur, un bouton est un RECTANGLE avec un TEXTE POSE DESSUS.
		//    Dans `.nkgui`, un bouton porte son mot en propriete et n'a pas de
		//    fils : `Button "ok" { label = "Enregistrer" }` -- c'est ce qu'ecrit
		//    tout le corpus ecrit a la main.
		//
		//    Le monteur ne descend pas dans un `Button` (`MonterCorps` n'est
		//    appele que par les conteneurs). Ecrire l'enfant comme un widget
		//    separe, c'etait donc ecrire un widget que rien ne montrerait
		//    JAMAIS -- une perte silencieuse.
		//
		// ⚠️ L'ABSORPTION EST ETROITE, ET C'EST VOULU : uniquement le PREMIER
		//    enfant de nature `text` d'un role qui porte un libelle, et seulement
		//    quand le noeud n'a pas de texte a lui. Un `Panel` garde ses enfants
		//    textes comme des widgets a part entiere : ce sont de vrais textes,
		//    pas le mot d'un bouton.

		/// L'enfant dont le texte devient le `label`, ou -1. `rap` n'est pas
		/// touche ici : la fonction REPOND, elle ne compte pas.
		inline int32 NkEnfantLibelle(const NkUIDocument &doc, const NkUINode &n, const char *role) {
			if (!NkRolePorteUnLibelle(role) || !n.text.Empty()) {
				return -1;
			}
			for (uint32 i = 0; i < (uint32)n.children.Size(); ++i) {
				const int32 c = n.children[i];
				if (!doc.IsValidIndex(c)) {
					continue;
				}
				const NkUINode &e = doc.nodes[(uint32)c];
				if (NkComponentDecl::StrEq(e.shape.Data(), "text") && !e.text.Empty()
					&& e.children.Empty()) {
					return c;
				}
			}
			return -1;
		}

		/// Une couleur du document (`#rrggbb`) telle que le format l'ecrit : un
		/// JETON NU. Une reference de variable (`@fond`) est RESOLUE avant, par le
		/// document -- une reference non resolue n'est pas ecrite du tout, plutot
		/// qu'ecrite fausse.
		inline void NkPoserCouleur(NkArchive &bloc, const char *cle, const NkUIDocument &doc,
								   const char *valeur) {
			if (!valeur || !*valeur) {
				return;
			}
			const char *c = doc.ResoudreCouleur(valeur);
			if (!c || !*c) {
				return;
			}
			NkGuiArchive::SetToken(bloc, NkStringView(cle), NkStringView(c));
		}

		inline void NkEcrireNoeud(const NkUIDocument &doc, const NkLayoutResult &lay, int32 index,
						  NkArchive &parent, NkEcritRapport &rap, float32 parentX,
						  float32 parentY, bool parentAbsolu) {
			if (!doc.IsValidIndex(index)) {
				return;
			}
			const NkUINode &n = doc.nodes[(uint32)index];
			++rap.noeudsVus;

			bool hors = false;
			bool compSansRole = false;
			bool roleDuComposant = false;
			const char *role = NkRoleNkguiDuNoeud(n, hors, compSansRole, roleDuComposant);
			if (hors) {
				rap.SignalerHorsCatalogue(n.role);
			}
			if (compSansRole) {
				rap.SignalerComposantSansRole(n.component);
			}
			if (roleDuComposant) {
				++rap.rolesDuComposant;
			}
			const NkString id = NkIdDuNoeud(n, index);

			NkArchiveNode *bn = NkGuiArchive::AddBlock(parent, NkStringView(role), NkStringView(id));
			if (!bn || !bn->object) {
				return;
			}
			// ⚠️ LE POINTEUR NE SE GARDE PAS : `AddBlock` le dit, le prochain ajout
			//    dans le meme `$body` relogerait le vecteur. On s'en sert ici, tout
			//    de suite, et on le reprend par index pour descendre dans les fils.
			NkArchive &bloc = *bn->object;
			++rap.widgetsEcrits;

			// L'enfant qui donne son mot au bouton -- il ne sera PAS ecrit comme
			// widget : le format le veut en propriete. Voir le bloc ci-dessus.
			const int32 enfantLibelle = NkEnfantLibelle(doc, n, role);

			// ── ce que le role sait porter ───────────────────────────────────
			if (NkComponentDecl::StrEq(role, "Text")) {
				// 🔴 LE REPLI SUR LE LIBELLE, ET SEULEMENT QUAND LE ROLE VIENT DU
				//    COMPOSANT. Le modele n'ecrit JAMAIS `texte` : l'invite ne lui
				//    propose que `libelle` (`DesignAI.h`). Sans ce repli, une
				//    `etiquette` sortirait avec le bon role et une chaine VIDE --
				//    c'est-a-dire un vert qui n'affiche rien, exactement le defaut
				//    qu'on repare. La condition `roleDuComposant` garde les fichiers
				//    deja enregistres intacts PAR CONSTRUCTION : aucun d'eux ne porte
				//    de composant (compte : 0 noeud sur les 9 `.nkuidoc` versionnes),
				//    donc aucun ne peut prendre ce chemin. `NkRolePorteUnLibelle`
				//    fait le meme repli depuis le 14/09, et pour la meme raison.
				const NkString &t = (!n.text.Empty() || !roleDuComposant) ? n.text : n.label;
				bloc.SetString(NkStringView("text"), NkStringView(t));
			} else if (NkRolePorteUnLibelle(role)) {
				// Le libelle : son `texte` s'il en a un ; sinon celui de l'enfant
				// absorbe ; sinon, en dernier recours seulement, son libelle
				// d'arbre -- un bouton sans mot afficherait son id.
				const NkString &l =
					!n.text.Empty()
						? n.text
						: (enfantLibelle >= 0 ? doc.nodes[(uint32)enfantLibelle].text : n.label);
				bloc.SetString(NkStringView("label"), NkStringView(l));
			} else if (NkComponentDecl::StrEq(role, "TextField")
					   || NkComponentDecl::StrEq(role, "NumberField")) {
				// Meme repli, meme raison, meme garde que pour `Text` ci-dessus : un
				// `champ_texte` produit par le modele ne porte que son `libelle`.
				const NkString &t = (!n.text.Empty() || !roleDuComposant) ? n.text : n.label;
				bloc.SetString(NkStringView("placeholder"), NkStringView(t));
			} else if (NkComponentDecl::StrEq(role, "Panel") && !n.text.Empty()) {
				bloc.SetString(NkStringView("title"), NkStringView(n.text));
			}

				// ── la geometrie, telle que le format sait la dire ──────────────
				{
					NkEcritItem it;
					it.id = id;
					it.role = NkString(role);
					it.noeud = index;
					if (lay.Has(index)) {
						const NkPaintRect &r = lay.At(index);
						it.x = r.x;
						it.y = r.y;
						it.w = r.w;
						it.h = r.h;
						it.aRect = true;
						if (parentAbsolu) {
							// RELATIVES au conteneur : voir le bloc de tete de cette section.
							NkPoserVec2(bloc, "pos", r.x - parentX, r.y - parentY);
							NkPoserVec2(bloc, "size", r.w, r.h);
							++rap.posesEcrits;
						}
					}
					rap.items.PushBack(it);
				}

			// ── l'apparence AU REPOS ─────────────────────────────────────────
			// Ecrite meme si le monteur l'ecarte aujourd'hui : un fichier qui perd
			// la couleur est un fichier faux. Le bloc `appearance` sans etat est
			// l'etat de repos -- c'est ce que le format appelle le nu.
			{
				const char *fond = n.FondEffectif();
				// 🔴 LA TYPOGRAPHIE DE L'ENFANT ABSORBE SUIT SON MOT. Mesure du
				//    2026-09-14 : le document porte 18 noeuds a corps et 11 a graisse,
				//    le fichier n'en ecrivait que 17 et 10. Le manquant etait le
				//    `Texte du bouton` -- absorbe en `label`, il emportait avec lui son
				//    `police_px = 12` et sa `graisse = 600`. En `.nkgui` c'est
				//    l'apparence du BOUTON qui regit son libelle : la typographie de
				//    l'enfant remonte donc au parent, sinon elle disparait sans un mot.
				const NkUINode *typo = &n;
				if (enfantLibelle >= 0) {
					const NkUINode &e = doc.nodes[(uint32)enfantLibelle];
					if (n.fontPx == 0.f && n.fontWeight == 0.f && n.textColor.Empty()) {
						typo = &e;
					}
				}
				const char *encre = typo->textColor.Empty() ? nullptr : typo->textColor.Data();
				// ⚠️ LA CONDITION D'EXISTENCE DU BLOC PORTE SUR TOUT CE QU'IL PEUT
				//    CONTENIR, pas sur la couleur seule. Elle ne portait que sur la
				//    couleur : un noeud n'ayant qu'un corps ou qu'un rayon perdait les
				//    deux. Aucun document du depot n'est dans ce cas AUJOURD'HUI -- la
				//    garde vaut pour le premier qui le sera.
				if ((fond && *fond) || encre || n.radius != 0.f || typo->fontPx != 0.f
					|| typo->fontWeight != 0.f) {
					NkArchiveNode *an =
						NkGuiArchive::AddBlock(bloc, NkStringView("appearance"), NkStringView(""));
					if (an && an->object) {
						NkArchive &ap = *an->object;
						// ⚠️ `fill` EST UN BLOC, PAS UNE PROPRIETE. Document 9 §3.1 : les
						//    quatre effets sont `fill`, `stroke`, `shadow`, `blur`, et
						//    `pFill = { color, gradient, from, to, angle, image, fit }`.
						//    Le corpus ecrit a la main le dit aussi : `fill { color = #2F6F7A }`
						//    (07_apparence.nkgui). J'ecrivais `fill = #...` -- une propriete
						//    que j'avais inventee, et que la validation a refusee.
						// ⚠️ ET IL N'Y A PAS DE `textColor` : l'encre d'un texte est SON
						//    remplissage. Le meme bloc sert donc aux deux, et c'est le seul
						//    vocabulaire que le format offre.
						// 🔴 UN NOEUD PEUT AVOIR UN FOND *ET* UNE ENCRE ; UNE `appearance`
						//    N'A QU'UN `fill`. Le bouton de la maquette est exactement ce
						//    cas : fond #0969da, libelle #ffffff. Le format ne sait pas dire
						//    les deux -- `pApparence` n'a ni `textColor` ni `inkColor`, et
						//    les quatre effets sont `fill`, `stroke`, `shadow`, `blur`.
						//    On garde le FOND (c'est lui qui porte la forme) et **on compte
						//    l'encre perdue** : un champ qui disparait sans un mot est la
						//    pire forme d'echec, et celui-ci disparaissait sans un mot.
						const char *couleur = (fond && *fond) ? fond : encre;
						if ((fond && *fond) && encre && !NkComponentDecl::StrEq(fond, encre)) {
							++rap.encresPerdues;
						}
						NkArchiveNode *fn =
							NkGuiArchive::AddBlock(ap, NkStringView("fill"), NkStringView(""));
						if (fn && fn->object) {
							NkPoserCouleur(*fn->object, "color", doc, couleur);
						}
						if (n.radius != 0.f) {
							ap.SetFloat32(NkStringView("radius"), n.radius);
						}
						// `size` et `weight` sont declares par le format (pApparence,
						// NkGuiValidate.h:590) et le document les porte : les laisser de cote
						// aurait perdu le corps et la graisse sans qu'aucun banc ne le dise.
						if (typo->fontPx != 0.f) {
							ap.SetFloat32(NkStringView("size"), typo->fontPx);
						}
						if (typo->fontWeight != 0.f) {
							ap.SetFloat32(NkStringView("weight"), typo->fontWeight);
						}
						++rap.apparencesEcrites;
					}
				}
			}

			// ── ce qui N'A PAS DE VOCABULAIRE dans le format : on le COMPTE ───
			if (n.rotation != 0.f || n.miroirH || n.miroirV || n.inclinaisonX != 0.f
				|| n.inclinaisonY != 0.f || !n.fusion.Empty() || n.opacite != 100.f) {
				++rap.transfoAbandonnees;
			}

			if (n.children.Empty()) {
				++rap.feuilles;
				return;
			}
			++rap.conteneurs;
			// ── LE MODE DE PLACEMENT DE CE CONTENEUR ────────────────────
			// Le document de l'editeur place en ABSOLU (`NkLayoutKind` None/Free/
			// Anchor) ; c'est mesure, pas suppose -- 42 noeuds sur 42, et 226 sur 226
			// sur les huit `.nkuidoc` du depot. On le DERIVE quand meme du noeud
			// plutot que de l'ecrire en dur : le jour ou une maquette emploiera un
			// flux, ce pont le suivra sans qu'on y touche.
			const NkLayoutKind k = n.layout.kind;
			const bool docAbsolu = (k != NkLayoutKind::Row && k != NkLayoutKind::Column
						   && k != NkLayoutKind::Grid);
			bool enfantsAbsolus = false;
			if (docAbsolu) {
				if (NkRolePorteUnPlacement(role)) {
					NkGuiArchive::SetToken(bloc, NkStringView("placement"), NkStringView("absolute"));
					enfantsAbsolus = true;
					++rap.conteneursAbsolus;
				} else {
					// ⚠️ LE FORMAT REFUSE `placement` SUR UNE `VBox` -- a juste titre, son
					//    nom dit deja son agencement. Les enfants retombent donc dans le
					//    flux, et leur place est PERDUE. On le compte plutot que de la
					//    perdre en silence, et le releve le dira a chaque execution.
					++rap.placementsPerdus;
				}
			}
			const float32 ox = lay.Has(index) ? lay.At(index).x : parentX;
			const float32 oy = lay.Has(index) ? lay.At(index).y : parentY;
			for (uint32 i = 0; i < (uint32)n.children.Size(); ++i) {
				const int32 c = n.children[i];
				if (c == enfantLibelle) {
					++rap.textesAbsorbes; // son mot est parti dans `label`
					continue;
				}
				if (!NkRoleEstConteneur(role)) {
					// 🔴 UNE FEUILLE QUI PORTE DES FILS, et ce n'est pas rien : le
					//    monteur n'appelle `MonterCorps` QUE depuis les conteneurs.
					//    Ecrire ce fils produirait un widget que rien ne montrerait
					//    jamais -- il serait « dans le fichier » et invisible. On
					//    l'ecrit quand meme (perdre la donnee serait pire), mais on
					//    le COMPTE : c'est le seul moyen de savoir que le fichier
					//    porte plus que ce que le montage rend.
					++rap.filsSousUneFeuille;
				}
				NkEcrireNoeud(doc, lay, c, bloc, rap, ox, oy, enfantsAbsolus);
			}
		}

		// =====================================================================
		//  LE DOCUMENT ENTIER -> UNE ARCHIVE
		// =====================================================================
		/// Construit l'archive `.nkgui` d'un document. `lay` est la disposition
		/// CALCULEE : les rectangles que l'editeur a peints. Elle est un PARAMETRE
		/// et non une donnee recalculee ici, parce qu'un pont qui recalculerait la
		/// disposition pourrait ecrire autre chose que ce que l'ecran a montre.
		inline bool NkDocumentVersArchive(const NkUIDocument &doc, const NkLayoutResult &lay,
										  NkArchive &out, NkEcritRapport &rap) {
			out.Clear();
			// La version vient des constantes de la COUCHE, jamais d'un « 0.3 »
			// recopie : un attendu en dur se perime sans prevenir le jour ou le
			// format avance.
			nkentseu::NkSchemaVersion v;
			v.major = (nkentseu::nk_uint16)NkGuiArchive::kMajor;
			v.minor = (nkentseu::nk_uint16)NkGuiArchive::kMinor;
			v.patch = 0u;
			NkGuiArchive::SetVersion(out, v);
			NkArchiveNode *wn =
				NkGuiArchive::AddBlock(out, NkStringView("widgets"), NkStringView(""));
			if (!wn || !wn->object) {
				return false;
			}
			// La racine du document (indice 0) est LA TOILE : elle n'est pas un
			// widget, ses enfants sont les pages. L'ecrire produirait un conteneur
			// de plus que le document n'a.
			if (!doc.IsValidIndex(0)) {
				return true; // document vide : un fichier valide A ZERO WIDGET
			}
			const NkUINode &racine = doc.nodes[0];
			for (uint32 i = 0; i < (uint32)racine.children.Size(); ++i) {
			// La racine de `widgets` est ABSOLUE par nature : rien ne la contient.
			// Ses enfants portent donc des coordonnees d'ECRAN (origine (0, 0)).
			NkEcrireNoeud(doc, lay, racine.children[i], *wn->object, rap, 0.f, 0.f,
						  /*parentAbsolu=*/true);
			}
			return true;
		}

		/// Le TEXTE `.nkgui` d'un document. La mise en forme est celle du format
		/// (2 espaces, LF), pas celle d'un fichier source : ce document-ci n'a pas
		/// de fichier source `.nkgui` dont on reprendrait le style.
		inline NkString NkDocumentVersTexte(const NkUIDocument &doc, const NkLayoutResult &lay,
											NkEcritRapport &rap) {
			NkArchive ar;
			if (!NkDocumentVersArchive(doc, lay, ar, rap)) {
				return NkString();
			}
			NkGuiStyle style; // 2 espaces, LF, saut de ligne final
			return NkGuiArchive::Write(ar, style);
		}

		/// LE GESTE COMPLET : le document, en `.nkgui`, sur le disque.
		///
		/// ⚠️ `WriteAllBytes`, PAS `WriteAllText` : voir l'en-tete du fichier.
		inline bool NkEcrireNkgui(const NkUIDocument &doc, const NkLayoutResult &lay,
								  const char *chemin, NkEcritRapport &rap) {
			if (!chemin || !*chemin) {
				return false;
			}
			const NkString texte = NkDocumentVersTexte(doc, lay, rap);
			if (texte.Empty()) {
				return false;
			}
			NkVector<nkentseu::uint8> octets;
			octets.Resize((nkentseu::usize)texte.Size());
			for (uint32 i = 0; i < (uint32)texte.Size(); ++i) {
				octets[i] = (nkentseu::uint8)texte.Data()[i];
			}
			return NkFile::WriteAllBytes(chemin, octets);
		}

	} // namespace guifmt
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_NKGUIECRIRE_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
