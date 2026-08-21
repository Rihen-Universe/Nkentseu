#pragma once
// -----------------------------------------------------------------------------
// @File    NkTreeViewModel.h
// @Brief   L'ARBRE de la bibliotheque — le SECOND composant declare, et par la
//          meme occasion le premier test independant de la forme.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE — deux raisons, et la seconde compte autant
// =============================================================================
//  1. **Le besoin.** Le code d'arbre est ecrit CINQ fois dans ce depot
//     (mesure du 18/08, perimetre `Applications/*/src`, branche
//     `feat/noge-inventaire`) :
//
//     | site | lignes | pile |
//     |---|---|---|
//     | `NK3DModeler/Shell/NkModelerHierarchy.h` | 1 642 | peintre maison |
//     | `Nogee/Panels/WorldOutlinerPanel.{h,cpp}` | 512 | NKGui |
//     | `Nogee/Panels/SceneTreePanel.{h,cpp}` + son modele | 344 | NKUI |
//     | `NKCode/Shell/NkExplorer.h` (arbre de fichiers) | 1 846 | peintre maison |
//     | `NKCode/Editor/NkJsonView.h` (arbre JSON) | 602 | peintre maison |
//
//     ⚠️ La ROADMAP du kit annonce « 3 copies, ~2 200 l. ». Le compte de copies
//        d'ARBRE DE SCENE est juste (3), le total de lignes est sous-estime, et
//        surtout **les deux tiers de la duplication sont dans la meme
//        application** : Nogee porte deux arbres, un par pile d'interface.
//
//  2. **Le test.** La forme de declaration (`NkComponentDecl.h`) n'avait ete
//     eprouvee que sur le composant pour lequel elle a ete ecrite — le
//     navigateur de contenu. Une forme validee sur un seul cas n'est pas
//     validee. Ce fichier est donc aussi une EXPERIENCE, dont les conditions
//     d'echec ont ete ecrites AVANT la premiere ligne de code (canal
//     `echanges/nktree.questions.md`, Q1) :
//
//       C1 modifier `NkComponentDecl.h` · C2 une variante qui exige un champ a
//       elle · C3 le modele qui cesse de compiler sans NKGui · C4 un nombre de
//       pixels dans le dessin · C5 un evenement declare qui ne part jamais.
//
//     Le verdict et les CONTOURNEMENTS sont en Q2 du meme canal. Les deux
//     manques qu'il faut connaitre avant de lire ce fichier sont resumes plus
//     bas, aux blocs « ⚠️ CONTOURNEMENT ».
//
// =============================================================================
//  LE TEST QUI GOUVERNE CE FICHIER : « compile-t-il sans NKGui ? »
// =============================================================================
//  Aucun include d'interface. Le peintre n'est que DECLARE EN AVANT. La commande
//  du banc est dans `NkTreeViewProbe.h`, en tete, avec son temoin NEGATIF — un
//  banc qui ne sait dire que « oui » ne mesure rien.
//
// =============================================================================
//  OU AJOUTER QUELQUE CHOSE — les points d'extension, nommes
// =============================================================================
//  Directive de Rodolf du 18/08 : « n'importe quel humain doit pouvoir lire le
//  fichier et y ajouter un module sans demander a personne. » Donc :
//
//   • une COLONNE de plus (Layer, compte d'enfants, memoire...)  -> `NkTreeViewHooks::extraColumn*`
//   • un DESSIN de plus sur une ligne (badge, pastille)          -> `NkTreeViewHooks::rowOverlay`
//   • une REGLE de deplacement propre a l'application            -> `NkTreeViewHooks::acceptDrop`
//   • un FILTRE propre a l'application                           -> `NkTreeViewHooks::acceptNode`
//   • un REGLAGE (afficher/cacher, seuil, bascule)               -> `kParams` dans `NkTreeViewDecl()`
//   • une LONGUEUR (hauteur, marge, epaisseur)                   -> `kMetrics`, JAMAIS un litteral dans le .cpp
//   • une COULEUR                                                -> `kTokens` + un champ de `NkTreeViewStyle`
//   • un EVENEMENT                                               -> `kEvents` + un pointeur dans `NkTreeViewHooks` + son depart dans le .cpp
//   • une REPRESENTATION de plus                                 -> `NkTreeVariant` + `kVariants` ; si elle reclame un champ du modele, ce n'est PAS une variante (cf. C2)
//
// =============================================================================
//  CE QUE CHAQUE COPIE EXISTANTE AVAIT, ET QUE LES AUTRES N'AVAIENT PAS
// =============================================================================
//  C'est la raison d'etre d'un composant partage : chacune a paye une lecon que
//  les autres n'ont pas. Elles sont TOUTES ici.
//
//  | venu de | quoi |
//  |---|---|
//  | NK3DModeler | la plage **Maj+clic** avec son ancre (absente des deux copies Nogee) |
//  | NK3DModeler | **seul le chevron plie** — le clic de ligne pliait aussi, « trop sensible et genant pour renommer » (Rihen) |
//  | NK3DModeler | zone de chevron **large** : « le pliage doit etre aise » (Rihen) |
//  | NK3DModeler | l'**oeil** et le **cadenas** par ligne, en etat EFFECTIF (herite d'un ancetre) |
//  | NK3DModeler | la recherche bascule en **liste plate** — devenue ici une VARIANTE, pas un mode cache |
//  | Nogee | la **garde anti-cycle** au reparentage (`SetParent` n'en a aucune) |
//  | Nogee | la colonne optionnelle **Layer** — devenue ici un point de greffe |
//  | Nogee | le reparentage **DIFFERE** : appliquer pendant le parcours modifie ce qu'on parcourt |
//  | Nogee | le filtre garde une branche visible si un **descendant** correspond |
//  | les deux | l'identite de ligne est l'**id**, jamais le libelle : il change au renommage |
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentLayout.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		class NkComponentPaint;

		// ── UN NOEUD ────────────────────────────────────────────────────────────
		// Neutre au sens fort : pas une couleur, pas un rectangle, pas un type
		// d'application. Ce que l'arbre affiche est un TABLEAU PLAT de noeuds ; la
		// structure vient de `parent`, un INDICE dans ce meme tableau.
		//
		// ⚠️ POURQUOI PLAT PLUTOT QUE RECURSIF PAR CROCHETS (« donne-moi l'enfant
		//    n de ce noeud ») : les deux copies Nogee parcourent leur ECS PENDANT
		//    le dessin, et les deux ont paye le meme defaut — modifier l'arbre
		//    pendant qu'on le parcourt. `WorldOutlinerPanel.cpp` a du differer son
		//    reparentage apres la boucle, avec un commentaire de dix lignes pour
		//    l'expliquer. Un instantane plat rend ce defaut IMPOSSIBLE au lieu de
		//    demander a chaque appelant de s'en souvenir. Le cout est une copie
		//    par image, sur des listes de l'ordre du millier.
		struct NkTreeNode {
				// L'IDENTITE, et elle est OPAQUE au composant. Une entite ECS, un
				// index de noeud 3D, un pointeur de dossier : le composant ne sait
				// pas ce que c'est, il sait seulement que ca ne change pas quand le
				// libelle change. C'est la lecon commune aux trois copies — le
				// libelle ne peut pas servir d'identifiant, il bouge au renommage.
				// `0` est reserve : il signifie « aucun noeud ».
				nk_uint64 id = 0;

				/// Indice du parent DANS `NkTreeViewModel::nodes`, -1 pour une racine.
				int32 parent = -1;

				NkString label;

				// ⚠️ LE CHEMIN N'EST PAS DECORATIF — c'est la CHARGE que les
				//    evenements portent vers un blueprint. Un graphe n'a pas le
				//    modele sous la main : une charge qui exige de posseder l'objet
				//    emetteur pour etre interpretee n'est pas une charge, c'est un
				//    pointeur deguise. Meme raisonnement que `path` chez le
				//    navigateur de contenu.
				NkString path;

				/// Poignee d'icone OPAQUE (`NkComponentPaint.h`, exigence B). `0` =
				/// aucune. Le composant ne connait l'enumeration d'icones d'aucune
				/// application — c'est ce qui lui permet de servir les quatre.
				uint16 icon = 0;

				/// Role de theme de la NATURE du noeud (lumiere, maillage, dossier,
				/// os...). Meme decision que `NkAssetEntry::kindRole` : une
				/// enumeration commune obligerait chaque nature neuve a modifier le
				/// kit.
				uint16 kindRole = 0;
				const char *kindLabel = ""; ///< affiche si le parametre `show_type` est vrai

				// ── DEUX DRAPEAUX, ET ILS SONT EFFECTIFS ────────────────────────
				// ⚠️ « EFFECTIF » veut dire : deja compose avec les ancetres par
				//    l'application. Un enfant dont le parent est cache est cache.
				//    NK3DModeler a paye cette lecon a l'usage (Rihen : « je ne peux
				//    selectionner ni le parent ni l'enfant ») et affiche l'etat
				//    HERITE en teinte attenuee, faute de quoi le refus parait
				//    inexplicable. Le composant ne compose PAS lui-meme : il ne
				//    connait pas la semantique d'heritage de l'application.
				bool hidden = false; ///< invisible dans la vue (l'oeil)
				bool locked = false; ///< inselectionnable (le cadenas)
				/// Le drapeau vient-il d'un ancetre plutot que du noeud ? Sert a le
				/// peindre attenue. L'application le calcule ; sans lui, l'icone
				/// mentirait sur l'endroit ou l'on peut agir.
				bool flagsInherited = false;

				uint32 userTag = 0; ///< libre a l'application (index, drapeaux)
		};

		// ── LE MODELE ───────────────────────────────────────────────────────────
		// L'application le remplit ; le composant le lit et y ecrit l'ouverture, la
		// selection et le defilement.
		//
		// ⚠️ LA SEULE PRECONDITION, et elle est gratuite : `nodes` est en ordre
		//    PREFIXE (un parent precede toujours ses enfants). C'est l'ordre naturel
		//    d'un parcours en profondeur, donc celui qu'une application produit sans
		//    y penser. `IsWellFormed()` la verifie ; le dessin ne plante pas si elle
		//    est violee, il affiche seulement dans un ordre surprenant.
		struct NkTreeViewModel {
				NkVector<NkTreeNode> nodes;

				// ── L'OUVERTURE : UN ENSEMBLE D'EXCEPTIONS, PAS UN ENSEMBLE D'OUVERTS
				// ⚠️ CE CHOIX RESOUT UNE DIVERGENCE REELLE ENTRE LES DEUX COPIES, et
				//    c'est pour ca qu'il merite dix lignes :
				//      - `NkSceneTreeModel::mOpenNodes` (Nogee) liste les noeuds
				//        OUVERTS -> un arbre neuf est entierement REPLIE ;
				//      - `NkModelerState::hierFold` (NK3DModeler) est un champ de
				//        bits des noeuds PLIES -> un arbre neuf est entierement
				//        DEPLIE.
				//    Les deux ont raison pour leur cas. Trancher pour tout le monde
				//    aurait fait perdre son comportement a l'un des deux.
				//
				//    Ici, `toggled` liste les identites dont l'etat DIFFERE du
				//    defaut, et le defaut est le parametre `default_open`. Les deux
				//    comportements existants deviennent deux valeurs d'un parametre,
				//    reglables depuis l'editeur — et non deux composants.
				//
				//    ⚠️ Et ca supprime au passage un defaut deja paye : `hierFold`
				//       est un tableau de bits BORNE (160 noeuds, avec un commentaire
				//       « bug deja paye » sur le depassement), `mOpenNodes` est borne
				//       a 256 SANS diagnostic — au-dela, le pliage cesse simplement
				//       de fonctionner. Ici rien n'est borne.
				NkVector<nk_uint64> toggled;

				// ── LES DEUX ETATS DE SELECTION ─────────────────────────────────
				// Meme decoupage que le navigateur de contenu, meme raison : la
				// ligne ACTIVE est celle dont les panneaux montrent les proprietes,
				// les CHOISIES sont celles qui partiront ensemble.
				nk_uint64 active = 0; ///< 0 = aucune
				NkVector<nk_uint64> chosen;

				/// Ancre de la plage Maj+clic. Posee par tout clic nu. Venue de
				/// NK3DModeler (`hierAnchor`), absente des deux copies Nogee.
				nk_uint64 anchor = 0;

				// ── LE RENOMMAGE EN PLACE ───────────────────────────────────────
				// ⚠️ CONTOURNEMENT ASSUME, ET IL FAUT LE LIRE AVANT DE S'EN SERVIR.
				//    `NkComponentInput` n'a AUCUNE entree clavier : ni caractere, ni
				//    touche. Le composant ne peut donc pas posseder son champ de
				//    saisie. Le partage retenu, et il est explicite :
				//      - le composant DECIDE quand le renommage commence (double-clic
				//        sur le libelle), pose `renaming` et recopie le libelle dans
				//        `renameBuf` ;
				//      - le composant DESSINE la boite d'edition et son curseur ;
				//      - l'HOTE, qui a le clavier, ecrit dans `renameBuf` et leve
				//        `renameCommit` ou `renameCancel` ;
				//      - le composant EMET `onRename(noeud, ancien, nouveau)` au
				//        commit, et se remet a zero.
				//    Ce n'est pas la bonne forme : de l'ENTREE transite par le
				//    MODELE. La bonne forme est trois champs de plus dans
				//    `NkComponentInput` (caracteres, Entree, Echap) — un fichier qui
				//    n'appartient pas a cet agent. C'est ecrit au canal, en Q2.
				//    ⏳ A RETIRER des que `NkComponentInput` porte le clavier.
				nk_uint64 renaming = 0; ///< 0 = aucun renommage en cours
				char renameBuf[128] = {};
				bool renameCommit = false; ///< leve par l'hote : valider
				bool renameCancel = false; ///< leve par l'hote : abandonner

				char filter[128] = {};
				float32 scroll = 0.f;

				// ── LA SOURCE D'UN GLISSER EN COURS ─────────────────────────────
				// Etat INTERNE au composant, pas une entree : il le pose lui-meme au
				// press sur une ligne et le lit au relachement. Il vit dans le modele
				// pour la meme raison que `scroll` — il doit survivre entre deux
				// images, et le composant n'a pas d'autre memoire.
				//
				// ⚠️ `0` NE VEUT PAS DIRE « pas de depot », il veut dire « le glisser
				//    ne vient pas de cet arbre ». Un lacher avec une source nulle est
				//    RELAYE quand meme, avec un `source` vide : c'est le cas d'un
				//    asset lache depuis le navigateur de contenu sur un noeud de
				//    scene, que NK3DModeler fait deja. Refuser ce cas aurait supprime
				//    une fonction existante sans que rien ne le signale.
				nk_uint64 dragSource = 0;

				// ── LECTURES ────────────────────────────────────────────────────
				int32 IndexOf(nk_uint64 nodeId) const {
					if (nodeId == 0)
						return -1;
					for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
						if (nodes[i].id == nodeId)
							return (int32)i;
					return -1;
				}

				/// `defaultOpen` vient du parametre `default_open`. Il est PASSE
				/// plutot que stocke : le stocker ici ferait exister deux verites,
				/// celle du modele et celle de la declaration, et la seconde est
				/// celle que l'editeur modifie.
				bool IsOpen(nk_uint64 nodeId, bool defaultOpen) const {
					for (uint32 i = 0; i < (uint32)toggled.Size(); ++i)
						if (toggled[i] == nodeId)
							return !defaultOpen;
					return defaultOpen;
				}

				void SetOpen(nk_uint64 nodeId, bool open, bool defaultOpen) {
					const bool isException = (open != defaultOpen);
					for (uint32 i = 0; i < (uint32)toggled.Size(); ++i) {
						if (toggled[i] == nodeId) {
							if (!isException)
								toggled.RemoveAt(i);
							return;
						}
					}
					if (isException)
						toggled.PushBack(nodeId);
				}

				bool IsChosen(nk_uint64 nodeId) const {
					for (uint32 i = 0; i < (uint32)chosen.Size(); ++i)
						if (chosen[i] == nodeId)
							return true;
					return false;
				}

				void ClearSelection() {
					active = 0;
					anchor = 0;
					chosen.Clear();
				}

				/// Profondeur maximale acceptee, ici et dans le dessin. Meme borne
				/// que la garde anti-cycle de `WorldOutlinerPanel` (« une hierarchie
				/// saine fait < 64 niveaux ») : une borne partagee vaut mieux que
				/// deux bornes qui divergeront.
				static const int32 kMaxDepth = 64;

				/// ⚠️ LA PRECONDITION EXACTE, ET ELLE EST PLUS FORTE QUE « le parent
				///    d'abord » : `nodes` est en ordre PREFIXE d'un parcours en
				///    PROFONDEUR. Un ordre en largeur satisferait « le parent
				///    d'abord » et casserait quand meme le dessin.
				///
				///    Ce que la difference achete, et c'est pour ca qu'on l'exige :
				///    en ordre prefixe, **le premier enfant d'un noeud est le noeud
				///    SUIVANT**. « Ce noeud a-t-il des enfants ? » se repond alors en
				///    temps constant, sans tableau annexe et sans allocation par
				///    image. Sans elle il faudrait un parcours complet par ligne.
				///
				///    Si elle est violee, rien ne plante : des chevrons manquent et
				///    l'ordre d'affichage surprend. C'est precisement le genre de
				///    defaut qu'on ne relie pas a sa cause — d'ou cette fonction, a
				///    appeler UNE fois en debogage a l'ecriture d'un nouvel appelant.
				///    Le dessin ne l'appelle pas : elle couterait un parcours de plus
				///    par image pour un defaut qui, lui, ne varie pas d'une image a
				///    l'autre.
				bool IsWellFormed() const {
					int32 chain[kMaxDepth]; // indices de la branche courante
					int32 sp = 0;
					for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
						const int32 p = nodes[i].parent;
						if (p < -1 || p >= (int32)i)
							return false; // parent inconnu, apres son enfant, ou lui-meme
						// Le parent doit etre le noeud precedent, ou l'un de ses
						// ancetres : c'est la definition de l'ordre prefixe.
						while (sp > 0 && chain[sp - 1] != p)
							--sp;
						if (p >= 0 && sp == 0)
							return false; // parent hors de la branche courante : pas prefixe
						if (sp >= kMaxDepth)
							return false; // plus profond que la borne partagee
						chain[sp++] = (int32)i;
					}
					return true;
				}
		};

		// ── LES VARIANTES ───────────────────────────────────────────────────────
		// Directive de Rodolf du 18/08 : un modele, N rendus. L'index correspond a
		// `NkTreeViewDecl().variants`.
		//
		// ⚠️ CE QUE CES DEUX-LA PROUVENT, et c'est l'objet meme de ce composant :
		//    elles ne changent QUE la mise en page et l'ensemble des lignes emises.
		//    Elles ne touchent ni `nodes`, ni `toggled`, ni `active`, ni `chosen`,
		//    ni le filtre, et la logique de selection n'existe qu'a un seul endroit
		//    du dessin. C'est la condition C2 du critere d'echec.
		//
		//    `FlatList` n'est pas une invention : c'est EXACTEMENT ce que
		//    NK3DModeler fait deja quand on tape dans sa recherche (`searching` ->
		//    liste plate, indentation fixe, aucun chevron, `NkModelerHierarchy.h`).
		//    La difference est qu'il en a fait un MODE CACHE, declenche par un autre
		//    champ ; ici c'est une variante que l'application — ou l'utilisateur —
		//    choisit.
		enum class NkTreeVariant : uint8 {
			Tree = 0,  ///< arbre classique : indentation, chevrons, enfants caches si replie
			FlatList,  ///< liste plate indentee : tout ce qui passe le filtre, sans chevron
			Count
		};

		// ── OU UN DEPOT ATTERRIT ────────────────────────────────────────────────
		// Le navigateur de contenu n'avait besoin que de « sur ce dossier ». Un
		// arbre en a trois, et la distinction est la difference entre REPARENTER et
		// REORDONNER. Aucune des trois copies existantes ne sait reordonner ; le
		// tiers haut / tiers bas d'une ligne coute quatre lignes de code, et c'est
		// aussi le seul endroit du composant qui exerce `NkArgKind::Enum` dans une
		// charge d'evenement.
		enum class NkTreeDropPos : uint8 {
			Before = 0, ///< inserer AVANT la cible, comme frere
			Into,		///< devenir enfant de la cible (reparentage)
			After,		///< inserer APRES la cible, comme frere
			Count
		};

		/// Le drapeau qu'une ligne bascule. Meme forme : une valeur d'enumeration
		/// declaree, pas deux evenements presque identiques.
		enum class NkTreeFlag : uint8 {
			Visible = 0, ///< l'oeil
			Locked,		 ///< le cadenas
			Count
		};

		// ── LES ICONES ──────────────────────────────────────────────────────────
		// ⚠️ CONTOURNEMENT ASSUME, ET C'EST LE SECOND. Ce sont des POIGNEES
		//    OPAQUES (`NkComponentPaint.h`, exigence B) : le kit ne connait
		//    l'enumeration d'icones d'aucune application. Elles devraient donc etre
		//    DECLAREES, comme les couleurs le sont par `NkParamKind::RoleRef` et les
		//    longueurs par `MetricRef` — mais **`NkParamKind` n'a pas d'`IconRef`**,
		//    et `NkComponentDecl.h` n'appartient pas a cet agent.
		//
		//    Consequence exacte, a connaitre : ces six poignees sont les SEULES
		//    choses de ce composant qu'un editeur ne peut PAS rebrancher. Tout le
		//    reste — couleurs, longueurs, reglages, variante — passe par la
		//    declaration. C'est ecrit au canal en Q2.
		//    ⏳ A remonter dans `kParams` le jour ou `NkParamKind::IconRef` existe.
		struct NkTreeViewIcons {
				uint16 chevronClosed = 0; ///< noeud replie (pointe a droite)
				uint16 chevronOpen = 0;	  ///< noeud deplie (pointe en bas)
				uint16 eyeOpen = 0;
				uint16 eyeClosed = 0;
				uint16 lockOpen = 0;
				uint16 lockClosed = 0;
		};

		// ── LE STYLE : QUE DES REFERENCES ───────────────────────────────────────
		// PAS UNE COULEUR, PAS UN PIXEL. Des identifiants de role de theme et des
		// poignees. La regle est mecanique donc verifiable a la revue : si un
		// `uint32` de couleur ou un `float32` de pixel apparait ici, c'est un
		// defaut.
		struct NkTreeViewStyle {
				NkTreeVariant variant = NkTreeVariant::Tree;

				uint16 panelBg = 0, headerBg = 0, border = 0;
				uint16 text = 0, textMuted = 0;
				uint16 rowHover = 0;
				uint16 activeMark = 0;	///< la ligne ACTIVE
				uint16 activeText = 0;	///< son libelle (la planche le passe en accent)
				uint16 chosenMark = 0;	///< les lignes CHOISIES — doit rester DISTINCT
				uint16 guide = 0;		///< filets verticaux d'indentation
				uint16 dropMark = 0;	///< trait d'insertion / surlignage de cible
				uint16 iconTint = 0;	///< icones d'oeil, de cadenas, de chevron
				uint16 dimTint = 0;		///< un drapeau HERITE, donc non modifiable ici

				NkTreeViewIcons icons;

				/// La source des nombres. A `nullptr`, le dessin lit les defauts de
				/// la declaration : une application qui n'a rien a regler ne cree pas
				/// d'instance et ne paie rien. Le premier pas reste gratuit.
				const NkComponentInstance *values = nullptr;
		};

		/// Les trois lectures. Le dessin n'ecrit JAMAIS un nombre : il passe par
		/// ici. Definies plus bas (elles utilisent la declaration, qui suit).
		inline float32 NkTreeMetric(const NkTreeViewStyle &s, const char *name);
		inline float32 NkTreeParam(const NkTreeViewStyle &s, const char *name);
		/// La variante EFFECTIVE : celle de l'instance si elle en impose une, sinon
		/// celle du style. L'ordre compte — sans lui, changer la variante dans
		/// l'editeur n'aurait aucun effet, et ce serait « un parametre qui n'est pas
		/// honore ».
		inline NkTreeVariant NkTreeEffectiveVariant(const NkTreeViewStyle &s);

		// ── LES POINTS DE GREFFE ────────────────────────────────────────────────
		// Troisieme exigence de Rodolf : « en y integrant d'autres graphiques ».
		// Des pointeurs de fonction plutot que des methodes virtuelles : une
		// declaration chargee depuis un fichier pourra les remplir sans qu'une
		// classe existe a la compilation.
		struct NkTreeViewHooks {
				void *user = nullptr;

				/// Dessin SUPPLEMENTAIRE par ligne, appele APRES le composant.
				void (*rowOverlay)(void *user, NkComponentPaint &p, int32 index, float32 x, float32 y,
								   float32 w, float32 h) = nullptr;

				/// Colonnes supplementaires. C'est litteralement la colonne « Layer »
				/// de `WorldOutlinerPanel`, portee en point de greffe : Nogee l'a,
				/// NK3DModeler ne l'a pas, et aucune des deux n'a a l'imposer a
				/// l'autre.
				int32 extraColumnCount = 0;
				const char *(*extraColumnHeader)(void *user, int32 col) = nullptr;
				const char *(*extraColumnText)(void *user, int32 index, int32 col) = nullptr;

				/// Filtre PROPRE a l'application, en plus du filtre texte du modele.
				bool (*acceptNode)(void *user, const NkTreeNode &n) = nullptr;

				/// ⚠️ LA REGLE DE DEPLACEMENT PROPRE A L'APPLICATION, et c'est le
				///    point de greffe le plus important du composant.
				///    Le composant refuse DEJA tout seul ce qui est faux quel que
				///    soit le domaine : deposer un noeud sur lui-meme, ou sur l'un de
				///    ses descendants (le cycle). Il peut le faire parce qu'il a la
				///    chaine de parents sous les yeux — Nogee a du l'ecrire a la main
				///    parce que `SetParent` n'a aucune garde interne.
				///    Ce crochet couvre ce que le composant NE PEUT PAS savoir :
				///    « un maillage ne se depose pas sur un maillage » (NK3DModeler,
				///    `NkParentTargetAllowed`), « ce dossier est en lecture seule »...
				///    A `nullptr`, seule la garde generique s'applique.
				bool (*acceptDrop)(void *user, const NkTreeNode &source, const NkTreeNode &target,
								   NkTreeDropPos pos) = nullptr;

				// ── LES ECOUTEURS D'EVENEMENTS ──────────────────────────────────
				// Ce ne sont PAS des points de greffe : un point de greffe ajoute du
				// DESSIN ou filtre une donnee, un evenement signale un FAIT. Ils sont
				// declares separement (`kEvents`), avec leur charge.
				//
				// ⚠️ LES SIGNATURES C++ ET LES CHARGES DECLAREES DOIVENT
				//    CORRESPONDRE. Rien dans le compilateur ne le verifie ; le banc
				//    de `NkTreeViewProbe.h` verifie les deux bouts.
				//
				// ⚠️ ET DEUX D'ENTRE EUX ONT UNE ACTION PAR DEFAUT, contrairement aux
				// cinq du navigateur de contenu qui n'en ont aucune : `onSelect`
				// et `onExpand` sont emis APRES que le composant a deja ecrit dans le
				// modele. C'est declare (`hasDefaultAction = true`) pour que la question
				// « pourquoi ca bouge tout seul » ne se repose pas.
				//
				// ⚠️ LES QUATRE PREMIERS PORTENT LES NOMS ET LES CHARGES DU ROLE `tree`
				//    (`NkComponentRole.h`), au caractere pres — et pas ceux que j'avais
				//    choisis. Le catalogue de roles est arrive PENDANT l'ecriture de ce
				//    fichier ; `onOpen` est devenu `onExpand`, et deux charges ont change
				//    de forme. Le dire vaut mieux que le taire : renommer trois signatures
				//    coute une heure, deux vocabulaires pour un meme fait coutent la
				//    convergence `.nkgui` — et `NkCheckComponent` rougit un role annonce
				//    et non honore.
				//    ⚠️ CE QUE LE ROLE A FAIT PERDRE, ecrit au canal en Q2 : sa charge
				//       `onSelect(index, id)` ne peut PAS dire ce qui est selectionne quand
				//       il y en a plusieurs. Ma premiere ecriture portait un
				//       `selectedCount` ; il a du partir. La selection multiple n'est donc
				//       PAS observable depuis un blueprint, et ce n'est pas un oubli.

				void (*onSelect)(void *user, int32 index, const char *id) = nullptr;
				/// LE FAIT, PAS LE GESTE : un double-clic active, la touche Entree aussi.
				/// Emis seulement si le parametre `activate_on_double_click` est vrai —
				/// sinon le double-clic arme le RENOMMAGE, ce que font les trois copies
				/// d'arbre de SCENE mesurees. Un arbre de FICHIERS veut l'inverse
				/// (`NKCode/Shell/NkExplorer.h` ouvre au double-clic) : d'ou un reglage,
				/// et non un choix impose a l'un des deux.
				void (*onActivate)(void *user, int32 index, const char *id) = nullptr;
				/// `index` vaut -1 sur le fond du panneau — la convention du role.
				void (*onContextMenu)(void *user, int32 index, float32 x, float32 y) = nullptr;
				void (*onExpand)(void *user, int32 index, const char *id, bool open) = nullptr;

				// Les trois suivants sont PROPRES a ce composant : le role `tree` ne les
				// exige pas, et aucun role du catalogue ne les porte.
				void (*onRename)(void *user, int32 index, const char *id, const char *oldName,
								 const char *newName) = nullptr;
				void (*onDrop)(void *user, const char *source, const char *target, uint8 position,
							   const char *payloadType) = nullptr;
				void (*onToggleFlag)(void *user, int32 index, const char *id, uint8 flag,
									 bool value) = nullptr;
		};

		// ── CE QUE LE DESSIN REND ───────────────────────────────────────────────
		// Une valeur, pas un effet de bord.
		struct NkTreeViewResult {
				bool selectionChanged = false;
				bool openChanged = false;
				bool renameStarted = false;
				bool renameCommitted = false;
				/// Un depot a ete ACCEPTE : l'application applique le deplacement.
				/// ⚠️ Elle l'applique APRES son appel de dessin, jamais pendant —
				///    c'est la lecon de `WorldOutlinerPanel`, qui modifiait les
				///    listes d'enfants qu'il etait en train de parcourir.
				bool dropAccepted = false;
				nk_uint64 dropSource = 0;
				nk_uint64 dropTarget = 0;
				NkTreeDropPos dropPos = NkTreeDropPos::Into;
				/// Un depot a ete REFUSE par la garde anti-cycle. Distinct de
				/// « rien ne s'est passe » : sans ce bit, un utilisateur qui glisse un
				/// parent dans son propre enfant voit un geste sans effet et croit a
				/// une panne (Rihen a perdu une seance sur un objet verrouille par
				/// megarde, meme famille de defaut).
				bool dropRefusedCycle = false;

				int32 visibleCount = 0; ///< lignes REELLEMENT emises, pour le pied et la molette
		};

		// ── LA SIGNATURE TYPE ───────────────────────────────────────────────────
		// Six arguments, la meme que le navigateur de contenu :
		//
		//     resultat Dessiner( PEINTRE, ENTREE, RECTANGLE, MODELE, STYLE, GREFFES )
		//
		// ⚠️ Que la forme tienne a l'identique sur un composant d'une autre famille
		//    est precisement ce que ce fichier avait a mesurer. Elle tient : ni un
		//    argument de plus, ni un de moins.
		NkTreeViewResult NkDrawTreeView(NkComponentPaint &p, const NkComponentInput &in,
										const NkPaintRect &rect, NkTreeViewModel &m,
										const NkTreeViewStyle &s, const NkTreeViewHooks &hooks);

		// ── LA DECLARATION ──────────────────────────────────────────────────────
		// Ce que NkUIDesign lit, et ce que le dessin lit DES AUJOURD'HUI pour ses
		// defauts. Ce n'est pas de la documentation : c'est la SOURCE des nombres.
		// Un dessin conforme ecrit `decl.Metric("row_h")`, jamais `24.f`.
		inline const NkComponentDecl &NkTreeViewDecl() {
			static const NkVariantDecl kVariants[] = {
				{"tree", "Arbre", "indentation, chevrons, enfants caches quand le noeud est replie"},
				{"flat_list", "Liste plate",
				 "tout ce qui passe le filtre, indentation fixe, aucun chevron — le mode "
				 "recherche de NK3DModeler, devenu une representation"},
			};

			// ⚠️ TOUS LES PARAMETRES SONT DES BASCULES, ET C'EST UN FAIT SUR LA
			//    FORME, pas un hasard : un arbre n'a pas de « taille de vignette ».
			//    `NkParamKind` porte Float/Int/Bool/Enum/Text/RoleRef/MetricRef, et
			//    ce composant n'en exerce que `Bool`. La forme n'en souffre pas —
			//    mais ca veut dire que le navigateur de contenu et l'arbre, a eux
			//    deux, n'ont toujours exerce ni `Enum`, ni `Text`, ni `RoleRef` en
			//    parametre. Signale au canal plutot que tu.
			static const NkParamDecl kParams[] = {
				{"show_header", "Bande de titre", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_search", "Barre de recherche", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_footer", "Pied de panneau (compteurs)", NkParamKind::Bool, 1.f, 0.f, 0.f,
				 nullptr, 0},
				{"show_visibility", "Colonne oeil", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_lock", "Colonne cadenas", NkParamKind::Bool, 0.f, 0.f, 0.f, nullptr, 0},
				{"show_type", "Colonne type", NkParamKind::Bool, 0.f, 0.f, 0.f, nullptr, 0},
				{"indent_guides", "Filets d'indentation", NkParamKind::Bool, 0.f, 0.f, 0.f, nullptr, 0},
				// La divergence mesuree entre les deux copies, devenue un reglage.
				{"default_open", "Noeuds deplies par defaut", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				// ⚠️ LECON PAYEE PAR NK3DModeler, gardee telle quelle : « le clic de
				//    ligne pliait aussi, trop sensible et genant pour renommer »
				//    (Rihen). C'est un reglage et non une constante parce que
				//    l'inverse se defend pour un arbre de dossiers, ou l'on ne
				//    renomme presque jamais.
				{"chevron_only_fold", "Seul le chevron plie", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				{"multi_select", "Selection multiple (Ctrl)", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				{"range_select", "Selection de plage (Maj)", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr,
				 0},
				// ⚠️ EXIGE PAR LE ROLE `tree`, QUI RECLAME `onActivate`. Aucune des
				//    trois copies d'arbre de SCENE mesurees n'active quoi que ce soit :
				//    chez elles, le double-clic RENOMME. L'arbre de FICHIERS de NKCode,
				//    lui, ouvre. Declarer `onActivate` sans jamais l'emettre aurait ete
				//    « un evenement declare qui ne part jamais » — ma propre condition
				//    d'echec C5. Ce reglage est la reponse : les deux usages reels
				//    existent, le composant les porte tous les deux, et le banc exerce
				//    les deux valeurs.
				{"activate_on_double_click", "Double-clic = activer (sinon : renommer)",
				 NkParamKind::Bool, 0.f, 0.f, 0.f, nullptr, 0},
			};

			static const NkTokenDecl kTokens[] = {
				{"panel_bg", "PanelBg", "fond du panneau"},
				{"header_bg", "PanelHeader", "bande de titre et barre de recherche"},
				{"border", "Border", "traits de separation"},
				{"text", "Text", "libelles"},
				{"text_muted", "TextMuted", "type du noeud, compteurs, chemin en liste plate"},
				{"row_hover", "InputBg", "fond d'une ligne survolee"},
				{"active_mark", "AccentUi", "la ligne ACTIVE — la planche du 18/08 veut un fond teinte "
										   "PLUS une barre a gauche, la capture sombre un aplat pleine "
										   "largeur : l'encodage est un arbitrage de Rodolf"},
				{"active_text", "TextOnAccent", "libelle de la ligne active"},
				{"chosen_mark", "AccentUi", "les lignes CHOISIES — doit rester DISTINCT de active_mark"},
				{"guide", "Border", "filets verticaux d'indentation"},
				{"drop_mark", "AccentUi", "trait d'insertion et surlignage de la cible d'un depot"},
				{"icon_tint", "TextMuted", "chevron, oeil, cadenas"},
				{"dim_tint", "TextMuted", "un drapeau HERITE d'un ancetre — non modifiable sur ce noeud"},
			};

			static const NkMetricDecl kMetrics[] = {
				{"row_h", 24.f, "hauteur d'une ligne"},
				{"indent_step", 14.f, "decalage par niveau de profondeur"},
				{"flat_indent", 6.f, "indentation fixe de la variante liste plate"},
				{"chevron_w", 24.f, "largeur de la zone cliquable du chevron — LARGE : "
									"« le pliage doit etre aise » (Rihen)"},
				{"icon_w", 18.f, "largeur reservee a une icone de colonne"},
				{"row_pad", 6.f, "marge interne gauche d'une ligne"},
				{"header_h", 28.f, "bande de titre"},
				{"search_h", 30.f, "barre de recherche"},
				{"footer_h", 24.f, "pied de panneau"},
				{"stroke_w", 1.f, "epaisseur d'un contour de selection"},
				{"accent_bar_w", 3.f, "barre d'accent a gauche de la ligne active (la planche)"},
				{"drop_line_h", 2.f, "epaisseur du trait d'insertion avant/apres"},
				{"guide_x", 7.f, "position du filet d'indentation dans son cran"},
			};

			static const NkHookDecl kHooks[] = {
				{"row_overlay", "(user, peintre, index, x, y, w, h) -> void",
				 "dessin ajoute par l'application par-dessus une ligne"},
				{"extra_column", "(user, index, col) -> texte",
				 "colonne supplementaire — c'est la colonne « Layer » de Nogee"},
				{"accept_node", "(user, noeud) -> bool", "filtre propre a l'application"},
				{"accept_drop", "(user, source, cible, position) -> bool",
				 "regle de deplacement propre a l'application ; le cycle est deja refuse sans elle"},
			};

			// ── LES EVENEMENTS ────────────────────────────────────────────────
			// Les charges sont ecrites dans le vocabulaire de la spec `.nkgui` v0.2
			// (§11), sans en inventer un seul — c'est ce qui permet a
			// `NkWriteControllerBlock` d'emettre le bloc `controller` de la spec §10
			// sans traduction.
			//
			// ⚠️ LES QUATRE PREMIERS SONT CEUX DU ROLE `tree`, RECOPIES A LA FORME
			//    PRES. `NkSameArgShape` compare le NOMBRE et les TYPES des arguments,
			//    pas leurs noms : un seul argument de plus, et `NkCheckComponent`
			//    rougit en « charge_incompatible ». Ce n'est pas de la bureaucratie —
			//    un blueprint branche sur `onSelect` recevrait autre chose que ce
			//    qu'il attend.
			//
			// ⚠️ TROIS CONTRAINTES DU VOCABULAIRE, TROUVEES ICI ET PAS AVANT — le
			//    navigateur de contenu ne pouvait pas les rencontrer :
			//
			//    1. **L'IDENTITE D'UN NOEUD EST UN `nk_uint64`, et le vocabulaire n'a
			//       pas d'entier 64 bits.** `Int` la tronquerait. Elle voyage donc
			//       comme `String` : le CHEMIN du noeud — ce que le role appelle
			//       `id`. Ce n'est pas un pis-aller : un blueprint ne peut rien faire
			//       d'une poignee opaque, alors qu'un chemin se lit et se compare.
			//       Mais il faut le savoir — **deux noeuds de meme chemin sont
			//       indistinguables pour un blueprint**, la ou le C++ les separe.
			//
			//    2. **`onSelect` NE PEUT PAS DIRE CE QUI EST SELECTIONNE.** Avec la
			//       selection multiple, la reponse est une LISTE, et le vocabulaire
			//       n'a aucun type liste — ni tableau, ni repetition. Ma premiere
			//       ecriture ajoutait un `selectedCount` pour au moins en donner le
			//       NOMBRE ; la charge du role (`index`, `id`) ne le permet pas, et
			//       elle prime. **La selection multiple n'est donc PAS observable
			//       depuis un blueprint** — c'est la limite la plus nette que ce
			//       composant ait trouvee a la forme, et elle est ecrite au canal.
			//
			//    3. **`onDrop` d'un arbre ne rentre dans aucune charge du catalogue.**
			//       `roledetail::ArgsDrop()` porte `(index, payloadType)` : un depot
			//       d'arbre a une SOURCE, une CIBLE et une POSITION. Le role
			//       `drop_target` ne convient donc pas, et un composant n'a qu'un
			//       role. L'evenement reste PROPRE a ce composant, et c'est le seul
			//       endroit ou `NkArgKind::Enum` est exerce dans une charge.
			static const NkArgDecl kArgsEntry[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"id", NkArgKind::String, nullptr, 0},
			};
			static const NkArgDecl kArgsExpand[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"id", NkArgKind::String, nullptr, 0},
				{"open", NkArgKind::Bool, nullptr, 0},
			};
			static const NkArgDecl kArgsMenu[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"at", NkArgKind::Vec2, nullptr, 0},
			};
			static const NkArgDecl kArgsRename[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"id", NkArgKind::String, nullptr, 0},
				{"oldName", NkArgKind::String, nullptr, 0},
				{"newName", NkArgKind::String, nullptr, 0},
			};
			// ⚠️ L'ORDRE DES LIBELLES EST CELUI DE `NkTreeDropPos`, et c'est LA seule
			//    chose qui relie les deux. Rien ne le verifie a la compilation : meme
			//    famille de defaut que « le point de verite d'un encodage est son
			//    consommateur ». Le banc le verifie (essai 24).
			static const char *const kDropPosNames[] = {"before", "into", "after"};
			static const NkArgDecl kArgsDrop[] = {
				{"source", NkArgKind::String, nullptr, 0},
				{"target", NkArgKind::String, nullptr, 0},
				{"position", NkArgKind::Enum, kDropPosNames, 3},
				{"payloadType", NkArgKind::String, nullptr, 0},
			};
			static const char *const kFlagNames[] = {"visible", "locked"};
			static const NkArgDecl kArgsFlag[] = {
				{"index", NkArgKind::Int, nullptr, 0},
				{"id", NkArgKind::String, nullptr, 0},
				{"flag", NkArgKind::Enum, kFlagNames, 2},
				{"value", NkArgKind::Bool, nullptr, 0},
			};

			static const NkEventDecl kEvents[] = {
				// ⚠️ `hasDefaultAction = true` : le composant a DEJA ecrit dans le
				//    modele quand l'evenement part. C'est la difference avec les cinq
				//    du navigateur de contenu, tous a `false`. Le declarer evite la
				//    question inverse de celle du navigateur : non pas « pourquoi ca
				//    ne fait rien », mais « pourquoi ca a deja bouge ».
				{"onSelect", "Selection",
				 "l'entree active a change (clic, Ctrl+clic, Maj+clic) — le modele est deja a jour",
				 kArgsEntry, 2, true},
				{"onActivate", "Activation",
				 "une entree a ete activee — emis SEULEMENT si `activate_on_double_click` est vrai",
				 kArgsEntry, 2, false},
				{"onContextMenu", "Menu contextuel",
				 "clic droit ; `index` vaut -1 sur le fond du panneau", kArgsMenu, 2, false},
				{"onExpand", "Depliage", "un noeud s'ouvre ou se referme — le modele est deja a jour",
				 kArgsExpand, 3, true},
				{"onRename", "Renommage",
				 "l'hote a valide une saisie ; le composant NE renomme PAS le noeud, il signale",
				 kArgsRename, 4, false},
				{"onDrop", "Depot",
				 "un glisser-deposer accepte ; le deplacement est a la charge de l'application, APRES le "
				 "dessin",
				 kArgsDrop, 4, false},
				{"onToggleFlag", "Bascule d'un drapeau",
				 "l'oeil ou le cadenas d'une ligne ; jamais emis si le drapeau est HERITE d'un ancetre",
				 kArgsFlag, 4, false},
			};

			// ── L'ARBRE DE SOUS-ELEMENTS ──────────────────────────────────
			// « Une apparence est un arbre » (Rodolf). Ce que l'editeur pourra
			// selectionner, deplacer et retailler DANS le composant.
			//
			// ⚠️ CE QUI N'EST PAS ICI, ET C'EST NOMME : les LIGNES. Elles sont
			//    repetees par la donnee — leur nombre, leur profondeur et leur
			//    indentation viennent du modele, et un arbre STATIQUE ne sait pas les
			//    dire. Le noeud `rows` s'arrete donc a la zone defilante. C'est
			//    exactement la limite ou butent les CARTES du navigateur de contenu.
			//
			//    ⚠️ ET C'EST UN FAIT SUR LA FORME, pas une coincidence : les DEUX
			//       composants declares butent au meme endroit. Un arbre de
			//       sous-elements statique ne peut decrire aucune REPETITION PAR LA
			//       DONNEE — ni une carte par asset, ni une ligne par noeud. Signale
			//       au canal ; ce n'est pas un manque de ce composant-ci.
			static const NkElementDecl kElements[] = {
				{"tree", "", "le panneau entier", "container", "",
				 NkExpand(), NkExpand(),
				 {NkLayoutKind::Column, "", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},

				{"header", "tree", "bande de titre du panneau", "label", "",
				 NkExpand(), NkFixedM("header_h"), {}, 0},

				{"search", "tree", "filtre texte", "text_field", "",
				 NkExpand(), NkFixedM("search_h"), {}, 0},

				{"rows", "tree", "la zone defilante des lignes — leur nombre vient du modele",
				 "tree", "",
				 NkExpand(), NkExpand(),
				 {NkLayoutKind::Column, "", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},

				{"footer", "tree", "compteurs « N noeud(s), M selectionne(s) »", "label", "",
				 NkExpand(), NkFixedM("footer_h"), {}, 0},
			};

			static const NkComponentDecl kDecl = {
				// ⚠️ INITIALISATION POSITIONNELLE COMMENTEE, et ce n'est pas de la
				//    coquetterie : C++17 n'a pas les initialiseurs designes. Le
				//    commentaire de gauche rend visible a la relecture le decalage d'un
				//    cran qu'un ajout de champ provoque. Ce n'est pas theorique : `role`
				//    est arrive dans la forme PENDANT l'ecriture de ce fichier, et
				//    l'erreur du compilateur portait sur `kParams`, dix lignes plus bas.
				/* name        */ "tree_view",
				/* title       */ "Arbre",
				/* summary     */ "arbre hierarchique : pliage, selection simple et multiple, "
								  "renommage en place, glisser-deposer avec garde anti-cycle",
				/* params      */ kParams,   (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				/* variants    */ kVariants, (uint16)(sizeof(kVariants) / sizeof(kVariants[0])),
				/* tokens      */ kTokens,   (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				/* metrics     */ kMetrics,  (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				/* hooks       */ kHooks,	 (uint16)(sizeof(kHooks) / sizeof(kHooks[0])),
				/* events      */ kEvents,   (uint16)(sizeof(kEvents) / sizeof(kEvents[0])),
				// ⚠️ LES QUATRE CHAMPS SUIVANTS SONT EN FIN DE STRUCTURE, ET C'EST UNE
				//    REGLE ECRITE DANS `NkComponentDecl.h` : tout champ nouveau s'ajoute
				//    A LA FIN. Elle a ete posee le jour meme, apres qu'un `role` insere
				//    apres `summary` a casse les initialisations de CE fichier.
				//
				// ⚠️ LA CAPACITE : une collection HIERARCHIQUE. C'est le role que le
				//    navigateur de contenu designe deja pour sa colonne de dossiers
				//    (`folder_tree` -> component `tree_view`) : ce composant etait donc
				//    attendu par un autre avant meme d'exister.
				/* role        */ "tree",
				/* elements    */ kElements, (uint16)(sizeof(kElements) / sizeof(kElements[0])),
				// Ecrite a la main, dans ce depot, jamais rejouee contre la planche.
				// `verified = false` ne dit pas « fausse » : il dit « pas encore passee
				// au juge ». La planche existe pourtant
				// (`AetherionWorldOutlinerDetailsLight.png`) — c'est le GPU qui manque,
				// pas la reference.
				/* provenance  */ NkProvenance{NkAuthorKind::Human, false, false},
			};
			return kDecl;
		}

		// ── LES TROIS LECTURES, DEFINIES ────────────────────────────────────────
		inline float32 NkTreeMetric(const NkTreeViewStyle &s, const char *name) {
			return s.values ? s.values->Metric(name) : NkTreeViewDecl().Metric(name);
		}
		inline float32 NkTreeParam(const NkTreeViewStyle &s, const char *name) {
			return s.values ? s.values->Param(name) : NkTreeViewDecl().Param(name);
		}
		inline NkTreeVariant NkTreeEffectiveVariant(const NkTreeViewStyle &s) {
			if (s.values && s.values->Decl()) {
				const int32 v = s.values->Variant();
				if (v >= 0 && v < (int32)NkTreeVariant::Count)
					return (NkTreeVariant)v;
			}
			return s.variant;
		}

	} // namespace editorkit
} // namespace nkentseu
