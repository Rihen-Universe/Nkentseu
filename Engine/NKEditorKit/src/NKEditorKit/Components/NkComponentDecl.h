#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentDecl.h
// @Brief   LA FORME DE DECLARATION d'un composant de la bibliotheque.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ETAT (2026-08-18, seconde passe) : ce fichier est SORTI de l'etat de devis.
//    Rodolf a tranche l'issue (2) du rectificatif §4 de `ROADMAP.md`. Il est
//    desormais consomme par `NkContentBrowserDraw.cpp` (le dessin lit la
//    declaration) et par l'application `NKUIDesign` (l'editeur lit et ecrase).
//
// ETAT (2026-08-19, troisieme passe) : les QUATRE AJOUTS de Rodolf du 18/08 au
//    soir sont integres, et la forme est mise a l'epreuve d'un SECOND COMPOSANT
//    d'une autre famille (`NkTreeViewDecl.h`) -- une forme validee sur le seul
//    composant pour lequel elle a ete ecrite n'est pas validee.
//
//        1. LE ROLE           -> `NkComponentRole.h`   (+ champ `role` ci-dessous)
//        2. L'ARBRE           -> `NkComponentLayout.h` (+ `elements` ci-dessous)
//        3. TAILLE/AGENCEMENT -> `NkComponentLayout.h` (+ `NkLayoutSolve.h`)
//        4. LA PROVENANCE     -> `NkProvenance` ci-dessous, trois champs
//
//    LES CINQ FICHIERS DE LA FORME, ET DANS QUEL ORDRE LES LIRE :
//        NkComponentLayout.h   l'arbre, la taille, l'agencement  (aucune dependance)
//        NkComponentDecl.h     CE FICHIER : ce qu'un composant declare
//        NkComponentRole.h     le catalogue des capacites
//        NkComponentCheck.h    les verifications -- une seule porte d'entree
//        NkLayoutSolve.h       la position, calculee : la SEMANTIQUE de la forme
//    Puis, autour : `NkComponentInstance.h` (les ecarts sauves, et la provenance
//    persistee) et `NkComponentPaint.h` (le peintre vu par un composant).
//
// =============================================================================
//  LA FRONTIERE AVEC `NKReflection` — ECRITE NOIR SUR BLANC (Rodolf, 2026-08-18)
// =============================================================================
//  Premiere condition posee par Rodolf en tranchant l'issue (2). Elle existe
//  parce que le depot porte deja trois systemes de description sans utilisateur
//  et qu'un quatrieme, non delimite, serait une faute et non un ajout.
//
//  ⚠️ `NKReflection` N'EST NI DEPRECIEE NI CONCURRENCEE. Rodolf : « elle va etre
//     utilisee dans d'autres choses dans le futur. » Elle a un AUTRE DOMAINE.
//     Ce fichier ne la remplace pas, ne la deconseille pas, et n'en depend pas.
//
//  | | `NKReflection` | `NkComponentDecl` (ce fichier) |
//  |---|---|---|
//  | ce qui est decrit | un OBJET DE DONNEES (materiau, entite, reglage) | un COMPOSANT D'INTERFACE (navigateur, arbre, ligne de propriete) |
//  | quand | a l'EXECUTION : enregistrement, allocation, `CreateInstance` par nom | a la COMPILATION : tableaux statiques `const char*`, zero allocation, zero enregistrement |
//  | qui possede l'instance | la reflexion (elle sait construire) | personne — une declaration ne s'instancie pas, elle se LIT |
//  | ce qu'on peut en faire sans rien lier | rien : il faut le module | tout : voir le banc de neutralite de `ROADMAP.md` §5 |
//  | ce qu'elle sait decrire en plus | conteneurs, heritage, sous-objets, 25 drapeaux de metadonnees | VARIANTES, JETONS de theme, JETONS de metrique, POINTS DE GREFFE, EVENEMENTS |
//  | ce qu'elle NE sait PAS decrire | variantes, jetons, greffes, evenements | conteneurs, heritage, pointeurs |
//
//  🎯 LA REGLE OPERATOIRE, en une phrase, pour l'agent qui hesitera :
//     **si la chose decrite peut etre INSTANCIEE, c'est `NKReflection` ; si elle
//     peut etre DESSINEE, c'est `NkComponentDecl`.** Un materiau s'instancie et
//     ne se dessine pas ; un navigateur de contenu se dessine et ne s'instancie
//     pas (l'application possede son modele, pas le kit).
//
//  ⚠️ CE QUI EST INTERDIT DES DEUX COTES, et c'est la seule interdiction :
//     - ajouter ici de quoi decrire un objet de donnees (heritage, conteneurs,
//       fabrique par nom) — ce serait reecrire `NKReflection` en moins bien ;
//     - demander a `NKReflection` de porter une variante ou un jeton de theme —
//       ce serait lui faire payer le cout d'un domaine qui n'est pas le sien.
//
//  LE POINT DE CONTACT PREVU, ET IL EST UNIQUE : un panneau de PROPRIETES
//  affiche les proprietes d'un objet reflechi (`NkEditorInspector.h`) DANS un
//  composant declare ici. Les deux se rencontrent a cet endroit-la, et nulle
//  part ailleurs. Le composant ne lit pas la reflexion ; l'hote passe l'un a
//  l'autre.
//
// =============================================================================
//  LA CIBLE RESTE `.nkgui` v0.2 — CE FICHIER EST LE CHEMIN, PAS LA DESTINATION
// =============================================================================
//  Rodolf, 2026-08-18 : la cible est la specification
//  `Applications/NKUIDesign/2_NkUIDesign_Langage_Description_NodeBlueprint.md`
//  (342 l.), seule des trois a decrire le COMPORTEMENT, donc seule a pouvoir
//  porter les blueprints. L'issue (2) est un chemin vers elle.
//
//  CE QUI, DANS CETTE FORME, PREPARE LA CONVERGENCE — et ce n'est pas une
//  intention, c'est mecanique et verifiable :
//   1. `NkArgKind` reprend EXACTEMENT le vocabulaire de types de la spec §11
//      (`Void|Bool|Int|Float|String|Color|Vec2|Enum[...]`) — pas un type de
//      plus, pas un type de moins ;
//   2. `NkEventDecl` a la forme d'un `callback_sig` de la spec §10
//      (`callback Nom(arg: Type, ...) -> Void`) ;
//   3. `NkWriteControllerBlock()` EMET le bloc `controller "..."` de la spec a
//      partir d'une declaration. Ce n'est donc pas « compatible en principe » :
//      le texte `.nkgui` se produit, et la sonde de `NKUIDesign` l'imprime.
//   4. Le manque structurel mesure de la spec (sa table §8 ne mappe que des
//      PRIMITIVES, aucune entree composite) est exactement ce que le REGISTRE
//      ci-dessous comble : il nomme les composants de l'etage 2. Le jour ou la
//      spec gagne une entree composite, elle la lira ici.
//
// POURQUOI UNE DECLARATION, ET PAS SEULEMENT UNE FONCTION
//   Directive de Rodolf du 2026-08-18 (NKUIEditor) : « un editeur ne peut
//   composer que ce qui est DECRIT PAR DES DONNEES ». Un composant qui n'existe
//   qu'en C++ compile ne peut etre ni assemble, ni parametre, ni sauve par un
//   editeur d'interfaces. La declaration ci-dessous est le minimum qui rende un
//   composant DESCRIPTIBLE, et elle coute quelques dizaines de lignes par
//   composant AUJOURD'HUI contre la reecriture de sa surface de dessin APRES.
//
// ⚠️ CE QUI N'EST PAS TRANCHE ICI, ET NE DOIT PAS L'ETRE
//   Le FORMAT DE FICHIER (texte a la NkTheme, JSON, binaire) est une decision de
//   Rodolf. Ce fichier ne decrit que la STRUCTURE en memoire. Elle se serialise
//   dans n'importe lequel de ces formats -- c'est justement pourquoi on peut
//   l'ecrire avant que le format soit choisi.
//
// EN-TETE PUR, ZERO DEPENDANCE D'INTERFACE — meme regle que NkTheme.h et
//   NkShortcutTable.h. Test de conformite, et il est EXECUTABLE (voir ROADMAP,
//   section « le banc de neutralite ») : une compilation en -fsyntax-only avec
//   les seuls chemins NKCore/NKContainers. S'il cesse de passer, c'est qu'un
//   type d'interface s'est invite : c'est exactement le defaut que ce fichier
//   existe pour empecher.
//
// LE PATRON SUIVI EST DEJA DANS LE DEPOT, TROIS FOIS
//   1. `NkTheme.h`      — roles = ENUM (acces du code) + NOM (acces du fichier),
//                         plus un REGISTRE d'extension pour les roles propres a
//                         une application. La structure ci-dessous copie ce
//                         couple, deliberement.
//   2. `NkDirBrowser.h` — `NkDirBrowserState` : coeur neutre reutilisable, dont
//                         l'application DERIVE (NkOpenWsState dans NKCode).
//   3. `Nogee/Panels/Model/*.h` — modeles neutres dont le panneau herite.
//   Ce n'est donc pas une invention : c'est le motif que ce depot a deja ecrit
//   trois fois separement, nomme une bonne fois.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"
// L'ARBRE, LA TAILLE ET L'AGENCEMENT (ajouts 2 et 3 de Rodolf). Cet en-tete
// n'inclut lui-meme que `NKCore/NkTypes.h` : le banc de neutralite tient.
#include "NKEditorKit/Components/NkComponentLayout.h"

namespace nkentseu {
	namespace editorkit {

		// ── LA PROVENANCE ───────────────────────────────────────────────────────
		// QUATRIEME AJOUT DE RODOLF (2026-08-19), et il l'a chiffre lui-meme :
		// « trois champs aujourd'hui contre une refonte plus tard ».
		//
		// POURQUOI CA VIT DANS LA FORME ET PAS A COTE : chaque declaration produite
		// dans NkUIDesign est de la DONNEE D'ENTRAINEMENT, quelle qu'en soit la
		// main. Sans provenance, tout se vaut et le modele apprend la MOYENNE --
		// c'est-a-dire le niveau median de ce qui passe, pas la cible. Avec elle, on
		// pondere, on filtre et on compare **sans rien avoir a refaire**.
		//
		//    les designs de Rodolf          -> la reference, le style vise
		//    une paire synthetique verifiee -> du volume sain
		//    une declaration CORRIGEE apres l'IA -> la plus precieuse : elle dit ce
		//                                           qui n'allait pas
		//    un premier essai d'etudiant    -> du volume, a ponderer, pas a jeter
		//
		// ⚠️ EXACTEMENT TROIS CHAMPS, et en ajouter un quatrieme est une decision,
		//    pas un detail. La tentation immediate est un `note` en clair ; elle se
		//    refuse tant que personne n'a de lecteur pour ce texte. Un champ que
		//    rien ne lit se remplit mal, puis ment.
		enum class NkAuthorKind : uint8 {
			Human = 0, ///< une main -- Rodolf, un agent humain, un etudiant
			AI,		   ///< produite par un modele
			Imported,  ///< convertie depuis une source externe (image, autre outil)
			Count
		};

		struct NkProvenance {
				NkAuthorKind author = NkAuthorKind::Human;
				/// REJOUEE et comparee a sa source. C'est l'avantage de Rihen : la
				/// declaration se rend, donc la paire se verifie automatiquement --
				/// le moteur est le juge du corpus. `false` ne veut pas dire
				/// « fausse », il veut dire « pas encore passee au juge ».
				bool verified = false;
				/// REPRISE A LA MAIN APRES LA MACHINE. Le signal le plus cher du
				/// corpus : ce que Rodolf a change apres l'IA vaut plus que ce
				/// qu'elle avait produit.
				bool corrected = false;
		};

		/// Le nom TEL QU'IL S'ECRIT DANS UN FICHIER. Un seul point de verite, meme
		/// raison que `NkArgTypeName` plus bas : deux tables donneraient deux
		/// orthographes et un fichier illisible par l'autre moitie du code.
		inline const char *NkAuthorName(NkAuthorKind a) {
			switch (a) {
				case NkAuthorKind::Human:
					return "humain";
				case NkAuthorKind::AI:
					return "ia";
				case NkAuthorKind::Imported:
					return "importe";
				default:
					return "humain";
			}
		}

		// ── LES LIBELLES SONT DES CLES, PAS DU TEXTE ────────────────────────────
		// REGLE DE RODOLF DU 2026-08-18, « LE MULTILINGUE VIT DANS NKGui », point 5 :
		// *« Les interfaces produites par NkUIDesign sont traduisibles aussi : les
		// libelles d'un composant declare sont des CLES, pas du texte. Sinon l'outil
		// produit des interfaces monolingues. »*
		//
		// CE QUE CA CHANGE, ET C'EST UN SEUL CHANGEMENT : les champs `label` et
		// `title` de ce fichier ne portent plus la chaine A AFFICHER, ils portent la
		// chaine A RESOUDRE. Aucun champ n'est ajoute -- et c'est deliberement :
		//
		// ⚠️ LA TENTATION ETAIT D'AJOUTER UN `labelKey` A COTE DE `label`. Elle se
		//    refuse. Deux champs pour une meme chose, c'est deux sources de verite,
		//    donc une divergence garantie -- exactement le defaut que cette forme
		//    existe pour supprimer, et qu'elle a deja paye deux fois ce mois-ci
		//    (`NkCrossAlignName` en double, le solveur en double). Le libelle EST la
		//    cle. Il n'y a rien a synchroniser parce qu'il n'y a qu'un champ.
		//
		// ⚠️ LA FORME NE TRADUIT RIEN, ET NE LE PEUT PAS. Le banc de neutralite exige
		//    qu'elle compile sans le moindre chemin NKGui ; or le catalogue de langues
		//    vit dans NKGui, qui en change A CHAUD. La forme DECLARE des cles ; NKGui
		//    les RESOUT au moment du dessin. C'est la seule repartition qui laisse la
		//    langue changer sans redemarrer, puisque rien n'est fige a la declaration.
		//
		// LE REPLI, ET IL EST VOULU VISIBLE (regle 3 du meme bloc de Rodolf : « une
		// cle manquante se voit, jamais un vide silencieux ») : une cle non resolue
		// s'affiche TELLE QUELLE. Consequence heureuse pour la migration -- une
		// declaration ecrite avant cette regle, qui porte « Grille » en clair,
		// continue d'afficher « Grille ». Elle n'est pas cassee, elle est seulement
		// MONOLINGUE, et `NkCheckComponent` le signale en NOTE (jamais en erreur).
		//
		// ⚠️ POURQUOI UNE NOTE ET PAS UNE ERREUR -- c'est une decision, pas une
		//    mollesse : au moment ou cette regle est ecrite, DEUX composants portent
		//    des libelles en clair, dont un ecrit par une autre main qui compile en
		//    ce moment meme sur ce fichier. Une erreur les rougirait tous les deux
		//    sans que leur auteur ait rien casse. La note garde l'information, la
		//    COMPTE, et laisse chaque main migrer a son rythme.
		//
		// LA CONVENTION : minuscules ASCII, chiffres, `_` et `.` comme separateur de
		// niveau. Ni espace, ni accent, ni majuscule -- une cle traverse un fichier,
		// un catalogue et peut-etre un tableur de traduction ; tout ce qui s'encode
		// mal quelque part est banni d'avance.
		//     `content_browser.title`
		//     `content_browser.variant.grid`
		//     `content_browser.param.thumb_size`
		//     `content_browser.event.on_select`
		//
		/// VRAI si `s` a la FORME d'une cle. Ne dit PAS si la cle existe dans un
		/// catalogue -- la forme n'en connait aucun (voir le banc de neutralite).
		/// Une chaine vide n'est pas une cle : elle veut dire « pas de libelle ».
		inline bool NkIsLabelKey(const char *s) {
			if (!s || !*s)
				return false;
			bool prevDot = true; // interdit un point en tete
			for (const char *p = s; *p; ++p) {
				const char c = *p;
				if (c == '.') {
					if (prevDot) // ni « .. » ni un point initial
						return false;
					prevDot = true;
					continue;
				}
				const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
				if (!ok)
					return false;
				prevDot = false;
			}
			return !prevDot; // interdit un point final
		}

		// ── NATURE D'UN PARAMETRE ───────────────────────────────────────────────
		// APPEND-ONLY, meme raison que NkRole : la valeur est destinee a etre
		// ecrite dans un fichier de description, un insert au milieu ferait relire
		// la mauvaise nature a toutes les descriptions deja enregistrees.
		enum class NkParamKind : uint8 {
			Float = 0,
			Int,
			Bool,
			Enum,	   ///< choix parmi `enumNames`
			Text,	   ///< chaine libre (libelle, gabarit de format)
			RoleRef,   ///< REFERENCE a un role de theme — jamais une couleur
			MetricRef, ///< REFERENCE a une metrique de theme — jamais un nombre de pixels
			Count
		};

		// ── UN PARAMETRE ────────────────────────────────────────────────────────
		// Ce que l'editeur d'interfaces pourra exposer dans son inspecteur, et ce
		// que le code de dessin lit pour ses defauts. UNE SEULE SOURCE DE VERITE :
		// si le dessin relit `defVal` ici au lieu d'ecrire 96.f dans son corps, il
		// devient impossible que la description et le dessin divergent.
		struct NkParamDecl {
				const char *name = "";	///< cle STABLE, telle qu'elle apparaitra dans un fichier
				const char *label = ""; ///< CLE de traduction (cf. « LES LIBELLES SONT DES CLES »)
				NkParamKind kind = NkParamKind::Float;
				float32 defVal = 0.f; ///< defaut ; pour Bool : 0 ou 1 ; pour Enum : l'index
				float32 minVal = 0.f; ///< borne basse (Float/Int) — 0 si sans objet
				float32 maxVal = 0.f; ///< borne haute — maxVal <= minVal signifie « non borne »
				const char *const *enumNames = nullptr; ///< Enum seulement
				uint8 enumCount = 0;
		};

		// ── UNE VARIANTE ────────────────────────────────────────────────────────
		// Directive de Rodolf du 2026-08-18 : « on peut avoir plusieurs
		// representations de la meme chose, et chaque application utilise celle qui
		// lui plait ». La variante est donc DECLAREE, et le dessin la recoit en
		// PARAMETRE.
		//
		// ⚠️ LA REGLE QUI REND LA SEPARATION VRAIE : ajouter une variante ne doit
		//    dupliquer NI le modele NI la logique de selection. Si elle les
		//    duplique, la variante n'en est pas une -- c'est un second composant
		//    qui se fait passer pour une option, et la duplication qu'on cherche a
		//    supprimer vient de rentrer par la fenetre.
		struct NkVariantDecl {
				const char *name = ""; ///< cle stable : « grid », « dense_list », « columns »
				const char *label = "";   ///< CLE de traduction, ex. « content_browser.variant.grid »
				const char *summary = ""; ///< a quoi elle sert, pour l'editeur et pour l'humain
		};

		// ── UN JETON DE THEME ───────────────────────────────────────────────────
		// Le composant ne nomme JAMAIS une couleur : il nomme un USAGE, et dit de
		// quel role du theme cet usage herite par defaut. L'application (ou
		// l'editeur) peut reaffecter le jeton a un autre role sans toucher au
		// dessin -- c'est la deuxieme exigence de Rodolf (« en changeant le
		// theme »), rendue verifiable.
		//
		// `defaultRole` est un NOM, pas une valeur d'enumeration, parce que le role
		// peut etre un role d'APPLICATION enregistre a l'execution
		// (`NkRoleRegistry::Register`, cf. NkTheme.h) : « nk3d.anneau_brosse » est
		// une cle legitime ici. La resolution passe par `NkResolveRole`.
		struct NkTokenDecl {
				const char *name = "";		  ///< usage : « card_bg », « active_outline »
				const char *defaultRole = ""; ///< role du theme dont il herite
				const char *purpose = "";	  ///< ce que ce jeton peint, en une ligne
		};

		// ── UNE METRIQUE ────────────────────────────────────────────────────────
		// LE MANQUE MESURE LE 18/08 : `NkTheme` porte 30 roles de COULEUR et
		// seulement 4 rayons ; `NkGuiTheme` porte 16 couleurs et 3 metriques. Ni
		// l'un ni l'autre n'a de jeton d'espacement, de hauteur de ligne ou
		// d'epaisseur de trait. Consequence chiffree : `NkModelerBrowser.h` porte
		// 249 litteraux flottants nus. Tant que les metriques ne sont pas des
		// jetons, « changer le theme » ne change que les couleurs -- la moitie de
		// l'exigence.
		struct NkMetricDecl {
				const char *name = ""; ///< « card_gap », « row_h », « stroke_w »
				float32 defVal = 0.f;  ///< en pixels LOGIQUES (avant l'echelle d'ecran)
				const char *purpose = "";
		};

		// ── UN POINT DE GREFFE ──────────────────────────────────────────────────
		// Troisieme exigence de Rodolf : « en y integrant d'autres graphiques ».
		// L'application greffe son propre dessin SANS MODIFIER le composant.
		//
		// Le point de greffe est DECLARE (nom + signature en clair) pour que
		// l'editeur d'interfaces sache qu'il existe et puisse y raccrocher quelque
		// chose. `signature` est du TEXTE : c'est une description lisible, pas un
		// type C++ -- le type vit dans la structure de crochets du composant, qui,
		// elle, peut connaitre le peintre.
		struct NkHookDecl {
				const char *name = "";		///< « card_overlay », « extra_column »
				const char *signature = ""; ///< « (user, rect carte, index) -> void »
				const char *purpose = "";
		};

		// ── LE VOCABULAIRE DE TYPES ─────────────────────────────────────────────
		// ⚠️ CE N'EST PAS UNE ENUMERATION DE PLUS : c'est EXACTEMENT la table §11
		//    de la spec `.nkgui` v0.2, recopiee sans en ajouter ni en retirer un
		//    seul. C'est ce qui rend `NkWriteControllerBlock` possible, et c'est la
		//    piece qui prepare la convergence demandee par Rodolf.
		//
		//    `Void|Bool|Int|Float|String|Color|Vec2|Enum[...]`
		//
		// APPEND-ONLY, meme raison que partout ailleurs ici : ces valeurs finiront
		// ecrites dans un fichier de description.
		//
		// ⚠️ POURQUOI PAS `NkParamKind` ? Parce que `NkParamKind` porte `RoleRef` et
		//    `MetricRef`, qui sont propres a NOTRE systeme de theme et n'ont AUCUN
		//    equivalent dans la spec. Les fusionner rendrait la sortie `.nkgui`
		//    impossible a produire sans invention. Deux vocabulaires, deux
		//    domaines : les parametres sont a nous, les charges d'evenement sont a
		//    la spec.
		enum class NkArgKind : uint8 {
			Void = 0,
			Bool,
			Int,
			Float,
			String,
			Color,
			Vec2,
			Enum, ///< liste de libelles fixee a la declaration (`enumNames`)
			Count
		};

		/// Le nom du type TEL QUE LA SPEC L'ECRIT. Un seul point de verite : si
		/// quelqu'un renomme une valeur ici sans toucher cette fonction, la sortie
		/// `.nkgui` devient fausse en silence — d'ou la table locale plutot qu'un
		/// `switch` disperse chez les appelants.
		inline const char *NkArgTypeName(NkArgKind k) {
			switch (k) {
				case NkArgKind::Void:
					return "Void";
				case NkArgKind::Bool:
					return "Bool";
				case NkArgKind::Int:
					return "Int";
				case NkArgKind::Float:
					return "Float";
				case NkArgKind::String:
					return "String";
				case NkArgKind::Color:
					return "Color";
				case NkArgKind::Vec2:
					return "Vec2";
				case NkArgKind::Enum:
					return "Enum";
				default:
					return "Void";
			}
		}

		// ── UN ARGUMENT DE CHARGE ───────────────────────────────────────────────
		// « avec quelle charge » (Rodolf, 2026-08-18). Un evenement sans charge
		// declaree est inutilisable par un blueprint : le graphe ne saurait pas
		// quoi brancher sur sa sortie.
		struct NkArgDecl {
				const char *name = ""; ///< nom du parametre, tel qu'il apparaitra dans le graphe
				NkArgKind kind = NkArgKind::Void;
				const char *const *enumNames = nullptr; ///< `Enum` seulement
				uint8 enumCount = 0;
		};

		// ── UN EVENEMENT ────────────────────────────────────────────────────────
		// SECONDE CONDITION POSEE PAR RODOLF en tranchant l'issue (2), et la raison
		// est ecrite dans sa consigne : « sinon le mouvement se refait dans trois
		// mois ». Un composant declare CE QU'IL EMET, avec la charge, DES
		// MAINTENANT — meme si rien ne s'y branche encore.
		//
		// ⚠️ CE QUE CE BLOC A CHANGE DANS LA FORME DU DEVIS, et il faut le dire :
		//    le devis rangeait `on_activate`, `on_context_menu` et `on_drop_into`
		//    parmi les POINTS DE GREFFE. C'etait un melange : un point de greffe
		//    ajoute du DESSIN, un evenement signale un FAIT. Les trois ont donc
		//    demenage ici. `NkHookDecl` ne garde que ce qui dessine ou filtre.
		//    Le code C++ ne change pas (les crochets restent des pointeurs de
		//    fonction dans la structure de crochets du composant) ; c'est la
		//    DESCRIPTION qui cesse de mentir sur la nature de ce qu'elle nomme.
		//
		// Correspondance avec la spec `.nkgui` §10, exacte :
		//     callback OnActivate(index: Int, path: String) -> Void
		struct NkEventDecl {
				const char *name = "";	  ///< `onSelect`, `onDoubleClick`, `onDrop` — cle STABLE
				const char *label = "";	  ///< CLE de traduction, affichee dans l'editeur
				const char *purpose = ""; ///< quand il part, en une ligne
				const NkArgDecl *args = nullptr;
				uint8 argCount = 0;
				/// Le composant a-t-il un comportement PAR DEFAUT si personne n'ecoute ?
				/// `false` = il ne fait rien (le composant SIGNALE, il n'agit pas).
				/// Le distinguer evite la question « pourquoi le double-clic ne fait
				/// rien » posee a chaque nouvelle application.
				bool hasDefaultAction = false;
		};

		// ── LA DECLARATION D'UN COMPOSANT ───────────────────────────────────────
		// Tout est en `const char*` et en tableaux statiques : une declaration est
		// une CONSTANTE de compilation, elle ne s'alloue pas, elle ne se detruit
		// pas, et elle peut vivre en donnees en lecture seule. Le jour ou NKUIEditor
		// chargera des declarations depuis un fichier, il construira les memes
		// structures a la main -- la forme ne change pas, seule la provenance.
		struct NkComponentDecl {
				const char *name = "";	///< cle stable : « content_browser »
				const char *title = ""; ///< CLE de traduction, ex. « content_browser.title »
				const char *summary = "";

				const NkParamDecl *params = nullptr;
				uint16 paramCount = 0;
				const NkVariantDecl *variants = nullptr;
				uint16 variantCount = 0;
				const NkTokenDecl *tokens = nullptr;
				uint16 tokenCount = 0;
				const NkMetricDecl *metrics = nullptr;
				uint16 metricCount = 0;
				const NkHookDecl *hooks = nullptr;
				uint16 hookCount = 0;
				const NkEventDecl *events = nullptr;
				uint16 eventCount = 0;

				// ═══════════════════════════════════════════════════════════════
				//  ⚠️ LES CHAMPS AJOUTES LE 2026-08-19 SONT ICI, A LA FIN, ET C'EST
				//     UNE REGLE -- PAS UNE PARESSE DE MISE EN PAGE
				// ═══════════════════════════════════════════════════════════════
				//  `role` appartient logiquement en haut, avec `name` et `title`. Il
				//  est en bas quand meme, et la raison a ete MESUREE le jour meme :
				//  insere apres `summary`, il a casse **12 initialisations** dans
				//  `NkTreeViewModel.h` -- le second composant, ecrit en parallele par
				//  un autre agent, dans la meme heure.
				//
				//  C++17 n'a pas d'initialiseurs designes : toute declaration de
				//  composant s'ecrit en liste POSITIONNELLE. Un champ insere au
				//  milieu decale donc tout ce qui suit, chez tout le monde, y compris
				//  chez qui n'a pas encore commite.
				//
				//  > **TOUT CHAMP NOUVEAU S'AJOUTE A LA FIN DE CETTE STRUCTURE.**
				//  > C'est la meme discipline append-only que les enumerations de
				//  > cette forme, pour une raison voisine : ici c'est la POSITION qui
				//  > est le contrat, la-bas c'est la VALEUR.
				//
				//  Et le cas benin est le vrai danger : si les types avaient ete
				//  compatibles, le decalage aurait COMPILE et decrit un autre
				//  composant. Il n'a rougi que par chance de typage.

				/// ── LA CAPACITE DU COMPOSANT ENTIER (ajout 1 de Rodolf) ──────────
				/// « On dessine une apparence, puis on lui attribue une capacite. »
				/// Un NOM du catalogue de `NkComponentRole.h` -- jamais un pointeur :
				/// une declaration chargee depuis un fichier porte des chaines, et
				/// c'est le meme mot qui s'ecrit dans le fichier et se lit ici.
				/// "" = le composant n'endosse aucune capacite du catalogue ; ses
				/// evenements sont alors entierement les siens.
				/// `NkCheckComponent` verifie que ce que le role EXIGE est bien
				/// declare -- un role annonce et non honore est le pire des deux
				/// mondes : l'application branche un ecouteur et attend.
				const char *role = "";

				/// ── L'ARBRE DE SOUS-ELEMENTS (ajout 2 de Rodolf) ─────────────────
				/// « Une apparence est un arbre ; une interface complete est un
				/// composant qui en contient d'autres. MEME MECANISME AUX DEUX
				/// ECHELLES. » La table est PLATE et chaque ligne nomme son parent ;
				/// un parent apparait toujours plus haut que ses enfants, ce qui rend
				/// un cycle impossible a ecrire (cf. `NkComponentLayout.h`).
				///
				/// Vide = le composant est une boite noire qui se dessine d'un bloc.
				/// C'est un etat legitime, pas un manque : la console n'a pas de
				/// structure interne a exposer a un editeur.
				const NkElementDecl *elements = nullptr;
				uint16 elementCount = 0;

				/// ── D'OU VIENT CETTE DECLARATION (ajout 4 de Rodolf) ─────────────
				/// Le defaut -- main humaine, non verifiee, non corrigee -- decrit
				/// exactement une declaration ecrite en C++ dans ce depot. Une
				/// declaration chargee depuis un fichier porte la sienne.
				NkProvenance provenance;

				static bool StrEq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					for (; *a && *b; ++a, ++b)
						if (*a != *b)
							return false;
					return *a == *b;
				}

				const NkParamDecl *FindParam(const char *n) const {
					for (uint16 i = 0; i < paramCount; ++i)
						if (StrEq(params[i].name, n))
							return &params[i];
					return nullptr;
				}
				const NkTokenDecl *FindToken(const char *n) const {
					for (uint16 i = 0; i < tokenCount; ++i)
						if (StrEq(tokens[i].name, n))
							return &tokens[i];
					return nullptr;
				}
				const NkMetricDecl *FindMetric(const char *n) const {
					for (uint16 i = 0; i < metricCount; ++i)
						if (StrEq(metrics[i].name, n))
							return &metrics[i];
					return nullptr;
				}

				// C'EST CETTE FONCTION QUE LE DESSIN APPELLE, au lieu d'ecrire un
				// nombre : le litteral n'existe alors qu'a UN endroit, dans la
				// declaration, ou un editeur peut le lire et le changer.
				float32 Metric(const char *n, float32 fallback = 0.f) const {
					const NkMetricDecl *m = FindMetric(n);
					return m ? m->defVal : fallback;
				}
				float32 Param(const char *n, float32 fallback = 0.f) const {
					const NkParamDecl *p = FindParam(n);
					return p ? p->defVal : fallback;
				}

				/// UN NOMBRE NOMME DU COMPOSANT -- metrique d'abord, parametre
				/// ensuite. C'est ce que lit la DISPOSITION, et il a fallu le
				/// decouvrir plutot que le prevoir : `tree_width` (la largeur de la
				/// colonne d'arbre) est declaree comme PARAMETRE, pas comme metrique,
				/// et c'est juste -- une fraction n'est pas une longueur en pixels.
				/// Interdire aux tailles de la nommer aurait force a recopier `0.18`
				/// dans l'agencement : deux verites pour un nombre, exactement le
				/// defaut que cette forme existe pour supprimer.
				///
				/// ⚠️ LES DEUX TABLES PARTAGENT DONC UN ESPACE DE NOMS DU POINT DE VUE
				///    DE LA DISPOSITION. Un meme nom dans les deux serait ambigu :
				///    `NkCheckComponent` le refuse, plutot que de laisser l'ordre de
				///    lecture trancher en silence.
				float32 Number(const char *n, float32 fallback = 0.f) const {
					const NkMetricDecl *m = FindMetric(n);
					if (m)
						return m->defVal;
					const NkParamDecl *p = FindParam(n);
					return p ? p->defVal : fallback;
				}
				int32 VariantIndex(const char *n) const {
					for (uint16 i = 0; i < variantCount; ++i)
						if (StrEq(variants[i].name, n))
							return (int32)i;
					return -1;
				}
				const NkEventDecl *FindEvent(const char *n) const {
					for (uint16 i = 0; i < eventCount; ++i)
						if (StrEq(events[i].name, n))
							return &events[i];
					return nullptr;
				}

				// ── LECTURE DE L'ARBRE ──────────────────────────────────────────
				// Trois fonctions, et elles suffisent a tout parcourir. Elles sont
				// LINEAIRES : un composant a une dizaine de sous-elements, pas mille.
				// Le jour ou l'un en aurait mille, c'est l'arbre qu'il faudrait
				// regarder, pas la recherche.
				int32 ElementIndex(const char *n) const {
					for (uint16 i = 0; i < elementCount; ++i)
						if (StrEq(elements[i].name, n))
							return (int32)i;
					return -1;
				}
				const NkElementDecl *FindElement(const char *n) const {
					const int32 i = ElementIndex(n);
					return i >= 0 ? &elements[i] : nullptr;
				}
				/// La racine : le seul element sans parent. `NkCheckComponent`
				/// verifie qu'il n'y en a qu'une -- deux racines ne sont pas un arbre,
				/// et le resolveur en dessinerait une seule sans rien dire.
				const NkElementDecl *RootElement() const {
					for (uint16 i = 0; i < elementCount; ++i)
						if (!elements[i].parent || !*elements[i].parent)
							return &elements[i];
					return nullptr;
				}
				/// Les enfants directs de `n`, dans l'ordre de la table -- qui est
				/// l'ordre d'affichage. `from` est l'index de reprise : appeler avec
				/// `from = resultat + 1` pour obtenir le suivant.
				int32 NextChildOf(const char *n, uint16 from) const {
					for (uint16 i = from; i < elementCount; ++i)
						if (StrEq(elements[i].parent, n))
							return (int32)i;
					return -1;
				}
		};

		// ── L'EMETTEUR `.nkgui` ─────────────────────────────────────────────────
		// LA CONVERGENCE, RENDUE EXECUTABLE. Rodolf : « la cible reste `.nkgui`
		// v0.2 ; l'issue (2) est le chemin, pas la destination. » Une affirmation
		// de compatibilite qui ne produit rien ne se verifie pas — celle-ci
		// produit le bloc `controller` de la spec §10, tel qu'il s'ecrit :
		//
		//     controller "content_browser" {
		//         callback onSelect(index: Int, path: String) -> Void
		//         callback onDoubleClick(index: Int, path: String) -> Void
		//     }
		//
		// ⚠️ PORTEE, et elle est etroite — a dire avec le resultat : ceci emet le
		//    CONTRAT DE CALLBACKS (§10) d'un composant, PAS un fichier `.nkgui`
		//    complet. Les sections `geometry` / `widgets` / `behavior` ne sont pas
		//    produites et ne peuvent pas l'etre : elles decrivent un ASSEMBLAGE,
		//    dont ce fichier ne sait rien. Ce qui est prouve ici, c'est que le
		//    vocabulaire de types et la forme de signature se traduisent SANS
		//    INVENTION — pas que la convergence est faite.
		//
		// Zero allocation : l'appelant fournit le tampon, la fonction rend le
		// nombre d'octets ecrits (hors zero terminal), ou 0 si le tampon est trop
		// petit — jamais de troncature silencieuse.
		inline uint32 NkWriteControllerBlock(const NkComponentDecl &d, char *out, uint32 cap) {
			if (!out || cap == 0)
				return 0;
			uint32 n = 0;
			bool overflow = false;
			auto put = [&](const char *s) {
				if (!s)
					return;
				for (; *s; ++s) {
					if (n + 1 >= cap) {
						overflow = true;
						return;
					}
					out[n++] = *s;
				}
			};
			put("controller \"");
			put(d.name);
			put("\" {\n");
			for (uint16 i = 0; i < d.eventCount; ++i) {
				const NkEventDecl &e = d.events[i];
				put("    callback ");
				put(e.name);
				put("(");
				for (uint8 a = 0; a < e.argCount; ++a) {
					if (a)
						put(", ");
					put(e.args[a].name);
					put(": ");
					put(NkArgTypeName(e.args[a].kind));
					if (e.args[a].kind == NkArgKind::Enum) {
						put("[");
						for (uint8 v = 0; v < e.args[a].enumCount; ++v) {
							if (v)
								put(",");
							put(e.args[a].enumNames[v]);
						}
						put("]");
					}
				}
				put(") -> Void\n");
			}
			put("}\n");
			if (overflow)
				return 0;
			out[n] = '\0';
			return n;
		}

		// ── LE REGISTRE ─────────────────────────────────────────────────────────
		// Meme forme que `NkRoleRegistry` (NkTheme.h), et pour la meme raison :
		// l'editeur d'interfaces a besoin d'ENUMERER ce qui existe, ce qu'une liste
		// ecrite en dur dans son code ne lui donnerait pas.
		//
		// ⚠️ ETAT (2026-08-18, seconde passe) : DEFINI, dans
		//    `NkComponentRegistry.cpp`. Il l'a fallu : `NKUIDesign` doit LISTER les
		//    composants disponibles, et une liste ecrite en dur dans son code
		//    n'aurait rien prouve — elle aurait « marche » sans qu'aucune
		//    declaration soit lue.
		//
		// ⚠️ IL NE STOCKE QUE DES POINTEURS, jamais des copies. Une declaration
		//    est une constante de compilation a duree de vie statique : la copier
		//    ferait exister deux verites, et la frontiere ci-dessus deviendrait
		//    fausse (« zero allocation »). Le registre, lui, alloue son TABLEAU —
		//    c'est un objet d'execution, ce que la declaration n'est pas.
		//
		// PLAFOND FIXE, et il est volontaire : pas de croissance dynamique, pas
		// d'allocation, pas d'ordre d'initialisation statique a redouter. La
		// bibliotheque vise ~8 composants (les paliers 3-8 de `ROADMAP.md` §6).
		class NkComponentRegistry {
			public:
				static const uint16 kMaxComponents = 64;

				static void Register(const NkComponentDecl &d); ///< idempotent sur `name`
				static uint16 Count();
				static const NkComponentDecl *At(uint16 i);
				static const NkComponentDecl *Find(const char *name);
		};

	} // namespace editorkit
} // namespace nkentseu
