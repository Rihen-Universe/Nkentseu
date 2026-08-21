#pragma once
// -----------------------------------------------------------------------------
// @File    Document.h
// @Brief   LE DOCUMENT : un arbre de composants, et AUCUNE coordonnee dedans.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CE FICHIER EST, EN UNE PHRASE
// =============================================================================
//  Une INTERFACE COMPLETE de NkUIDesign, c'est-a-dire un arbre de noeuds, ou
//  chaque noeud pose un composant declare de la bibliotheque, porte ses ecarts
//  (`NkComponentInstance`), ses proprietes de TAILLE, l'AGENCEMENT de ses
//  enfants, et sa PROVENANCE.
//
//  ⚠️ LA REGLE QUI GOUVERNE TOUT LE FICHIER, ET C'EST CELLE DE RODOLF :
//     **on n'enregistre JAMAIS une coordonnee.** Cherchez `x`, `y`, `position`,
//     `left`, `top` parmi les champs d'un noeud : il n'y en a pas, et il ne doit
//     pas y en avoir. Un noeud declare `fixe / extensible / a poids`, avec `min`
//     et `max` ; son parent declare son agencement (ligne, colonne, grille) ; la
//     POSITION est ce que `Layout.h` en DEDUIT, a chaque image, pour la taille
//     de surface du moment.
//
//     C'est litteralement ce qui separe un OUTIL DE DESIGN d'un CONSTRUCTEUR
//     D'INTERFACES : le second enregistre ou vous avez lache la souris, et son
//     resultat est faux des que la fenetre change de taille, de DPI ou de
//     langue. Le premier enregistre POURQUOI c'etait la, et le resultat se
//     recalcule.
//
//     La souris n'est donc pas interdite — elle est TRADUITE. Tirer un bord
//     ecrit une taille ou un poids (`NkResizeByDrag`, dans `Layout.h`) ;
//     deplacer un noeud ecrit un PARENT et un RANG (`Reparent` / `MoveChild`).
//     Jamais un point.
//
// =============================================================================
//  MEME MECANISME AUX DEUX ECHELLES (Rodolf, 2026-08-18)
// =============================================================================
//  « une apparence est un arbre, pas une image plate ; une interface complete
//  est un composant qui en contient d'autres. »
//
//  D'ou : il n'y a PAS de type « document » distinct d'un type « noeud ». Le
//  document est un noeud (la racine) qui en contient d'autres. Le jour ou l'on
//  voudra enregistrer un document comme un composant reutilisable de la
//  bibliotheque, c'est la meme structure qui partira — pas une conversion.
//
//  ⚠️ UN NOEUD SANS COMPOSANT EST UN **CADRE** : il ne dessine rien, il ARRANGE.
//     C'est ce qui permet a l'arbre d'exister avant qu'un composant conteneur
//     soit declare. Ce n'est pas un composant fantome : `composant` vide se lit
//     dans le fichier, se voit dans l'arbre, et le rendu ne peint rien pour lui.
//
// =============================================================================
//  LA PROVENANCE EST POSEE A LA CREATION, PAS AJOUTEE PLUS TARD
// =============================================================================
//  Regle du corpus (CLAUDE.md, « TOUTE MAIN ALIMENTE LE CORPUS ») : chaque
//  declaration produite ici est de la donnee d'entrainement, quel qu'en soit
//  l'auteur — mais **les sources n'ont pas la meme valeur**. Sans provenance,
//  tout se vaut et le modele apprend LA MOYENNE.
//
//  Trois champs, et ils coutent trois champs aujourd'hui contre une refonte
//  plus tard :
//    - `author`    : humain / IA / import ;
//    - `verified`  : la declaration a ete REJOUEE et a rendu le meme dessin ;
//    - `corrected` : une main est passee APRES la machine — la source la plus
//                    precieuse du corpus, parce qu'elle dit ce qui n'allait pas.
//
//  ⚠️ ET DEUX AUTOMATISMES, sans lesquels ces champs mentiraient au bout d'une
//     seance. Ils sont dans `MarkHumanEdit` :
//       1. editer un noeud d'origine IA met `corrected` a vrai — personne ne
//          pensera a cocher une case, donc la case ne doit pas exister ;
//       2. TOUTE edition remet `verified` a faux. Une verification porte sur ce
//          qui a ete verifie, pas sur ce que c'est devenu. Un `verified` qui
//          survivrait a une modification serait pire qu'absent : il ferait
//          entrer dans le corpus, avec le tampon « verifie », quelque chose que
//          personne n'a rejoue.
//
// =============================================================================
//  LE FORMAT DE FICHIER N'ENGAGE RIEN — MEME AVERTISSEMENT QUE `NkComponentInstance.h`
// =============================================================================
//  Le format des DECLARATIONS reste un arbitrage de Rodolf, et la cible reste
//  `.nkgui` v0.2. Ce qui s'ecrit ici est un format d'ASSEMBLAGE, ligne a ligne,
//  du meme genre que `NkTheme` : un mot-cle, un `=`, une valeur.
//
//  Deux choses le rendent jetable sans douleur :
//    - il ne DECRIT aucun composant (ni type, ni borne, ni evenement) : il ne
//      fait que NOMMER des composants deja declares et poser leur agencement ;
//    - les ecarts d'un noeud sont ecrits par `NkComponentInstance::Save` et
//      relus par `NkComponentInstance::Load` — le meme ecrivain et le meme
//      lecteur que le fichier d'ecarts, prefixes de `reglage `. Il n'y a donc
//      pas de second analyseur de valeurs a maintenir, et pas de second endroit
//      ou le format des nombres pourrait diverger.
//
// OU AJOUTER LA PROCHAINE CHOSE (le contre-test de lisibilite) :
//   - un mode de taille de plus        -> `NkSizeMode` + `NkSizeModeName` +
//                                         `NkParseSizeMode`, puis le repartiteur
//                                         de `Layout.h`
//   - un agencement de plus            -> `NkLayoutKind` + ses deux fonctions de
//                                         nom, puis `Layout.h`
//   - un champ de plus sur un noeud    -> `NkUINode`, puis `Save` et `Load` (les
//                                         deux, dans le meme ordre de champs)
//   - une source de provenance de plus -> `NkAuthor` + `NkAuthorName` +
//                                         `NkParseAuthor`
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentLayout.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::NkString;
	using nkentseu::NkVector;
	using nkentseu::uint16;
	using nkentseu::uint32;
	using nkentseu::uint8;
	using nkentseu::editorkit::NkComponentDecl;
	using nkentseu::editorkit::NkComponentInstance;
	using nkentseu::editorkit::NkComponentRegistry;

	/// Comparaison de chaines nues — la meme que partout ailleurs dans la tranche
	/// (`NkComponentDecl::StrEq`), reexposee sous un nom local pour ne pas obliger
	/// chaque appelant a nommer la declaration.
	inline bool StrEq(const char *a, const char *b) {
		return NkComponentDecl::StrEq(a, b);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LA PROVENANCE
	// ═══════════════════════════════════════════════════════════════════════════

	/// APPEND-ONLY, meme raison que partout ailleurs dans la tranche : ces valeurs
	/// finissent dans un fichier, et un insert au milieu ferait relire la mauvaise
	/// source a tous les documents deja enregistres.
	enum class NkAuthor : uint8 {
		Humain = 0,
		IA,		///< produit par un backend (cf. `DesignAI.h`)
		Import, ///< converti depuis une source exterieure (image, autre outil)
		Count
	};

	inline const char *NkAuthorName(NkAuthor a) {
		switch (a) {
			case NkAuthor::Humain:
				return "humain";
			case NkAuthor::IA:
				return "ia";
			case NkAuthor::Import:
				return "import";
			default:
				return "humain";
		}
	}
	inline NkAuthor NkParseAuthor(const char *s) {
		if (StrEq(s, "ia"))
			return NkAuthor::IA;
		if (StrEq(s, "import"))
			return NkAuthor::Import;
		return NkAuthor::Humain;
	}

	// ── LA PAIRE DE CORRECTION ──────────────────────────────────────────────
	//
	// 🛑 ETAT AU 2026-08-19, A LIRE AVANT DE S'EN SERVIR : **DECLAREE, PAS
	//    BRANCHEE.** Rien ne l'ecrit, rien ne la relit, elle n'est ni serialisee
	//    ni couverte par la sonde. Le chantier a ete mis en pause a cet endroit
	//    precis (moyens concentres sur Ilyana).
	//
	//    Elle est conservee malgre tout **pour une seule raison** : elle porte la
	//    precision de Rodolf ci-dessous, et le raisonnement qui dit pourquoi cette
	//    donnee ne se reconstitue pas apres coup. C'est ca qui serait perdu, pas
	//    les trente lignes de code qui restent a ecrire.
	//
	//    ⚠️ Ne pas la prendre pour une fonctionnalite : tant que les sites
	//    d'edition n'appellent pas `NoteCorrection`, `corrections` reste vide et
	//    `corrected` continue de resumer a lui seul. La suite exacte est ecrite
	//    dans `CARNET.private.md`, section « reprise ».
	// ⚠️ PRECISION DE RODOLF (2026-08-19), ET ELLE CHANGE CE QU'ON ENREGISTRE :
	//
	//    > **L'etape qui perd quelque chose a attendre, c'est la boucle de
	//    > correction.** Chaque fois qu'un humain reprend a la main ce que l'IA a
	//    > produit, la paire *« ce que la machine a propose / ce que l'humain
	//    > voulait »* est **perdue pour toujours si elle n'est pas enregistree au
	//    > moment du geste**. Aucun corpus ne la contient et on ne la reconstitue
	//    > pas apres coup.
	//
	//    Un booleen `corrected` disait qu'une main etait passee. Il ne disait NI
	//    ce qui a change, NI vers quoi. Or c'est exactement l'ecart qui a de la
	//    valeur : « la machine a propose `fixed 260`, l'humain a retenu
	//    `fixed 340`, en tirant le bord droit ». Le booleen jetait les trois
	//    informations et gardait le fait le moins utile.
	//
	// ⚠️ POURQUOI ON NE PEUT PAS LE RECONSTITUER APRES COUP, et c'est la raison
	//    d'etre de cette structure : une fois la valeur ecrasee, **la proposition
	//    de la machine n'existe plus nulle part**. Ni le document, ni le journal,
	//    ni la sortie du backend ne la contiennent — le document ne garde que
	//    l'etat courant. La capture doit donc se faire **au moment du geste**, par
	//    le code qui a les deux valeurs sous la main, ou jamais.
	struct NkCorrection {
			/// CE QUI a change, en chemin stable : « largeur.mode »,
			/// « param.thumb_size », « parent ». Stable parce qu'il sert de cle :
			/// deux corrections du meme champ doivent se reconnaitre.
			NkString what;
			/// Ce que la MACHINE avait pose. Capture a la PREMIERE correction de ce
			/// champ, et **jamais reecrit ensuite** — voir `NoteCorrection`.
			NkString proposed;
			/// Ce que l'humain a retenu. Mis a jour a chaque nouvelle correction du
			/// meme champ : c'est la valeur FINALE qui compte, pas les etapes.
			NkString retained;
			/// LE GESTE, parce qu'il porte l'intention : « glisser-bord-droit » et
			/// « saisie-numerique » donnent la meme valeur et ne disent pas la meme
			/// chose sur ce que l'humain cherchait.
			NkString gesture;
	};

	struct NkProvenance {
			NkAuthor author = NkAuthor::Humain;
			/// REJOUEE et comparee : la declaration a produit le meme dessin apres
			/// un aller-retour par le texte. Ce n'est pas « quelqu'un a regarde ».
			bool verified = false;
			/// Une main est passee APRES une machine. N'a de sens que si `author`
			/// n'est pas `Humain` — un humain qui corrige un humain, c'est du
			/// travail, pas un signal d'apprentissage.
			bool corrected = false;
			/// Qui exactement : nom du backend, du modele, du fichier importe.
			/// Vide pour une pose a la main.
			NkString origin;
			/// ⚠️ LES PAIRES DE CORRECTION. `corrected` reste, mais il n'est plus
			///    que le RESUME de ceci : vrai des qu'il y a au moins une paire.
			///    Le garder evite de casser ce qui le lit ; le remplacer par un
			///    compte aurait fait deux verites a tenir d'accord.
			NkVector<NkCorrection> corrections;
	};

	// ════════════════════════════════════════════════════════════════════════════
	//  TAILLE ET AGENCEMENT : TOUT VIENT DU KIT, Y COMPRIS LES MOTS
	// ════════════════════════════════════════════════════════════════════════════
	//
	// ⚠️ **CE FICHIER NE DEFINIT NI MODE DE TAILLE, NI AGENCEMENT, NI MEME LES
	//    MOTS-CLES QUI LES ECRIVENT.** Tout appartient a
	//    `NKEditorKit/Components/NkComponentLayout.h` (la forme et son vocabulaire)
	//    et a `NkLayoutSolve.h` (sa semantique), tous deux tenus par le gardien de
	//    la forme.
	//
	//    Une application qui en redefinirait un seul creerait une SECONDE
	//    semantique de « extensible » — et « la position est un resultat »
	//    deviendrait faux : elle serait deux resultats. Une application qui
	//    reecrirait seulement les MOTS ferait pire, parce que ca ne se verrait pas
	//    tout de suite : les documents s'ecriraient dans un dialecte que les
	//    composants du kit ne relisent pas.
	//
	//    Une premiere version de ce fichier portait ses propres `NkSizeModeName` /
	//    `NkParseSizeMode` en francais. Le gardien a publie les siens, en anglais
	//    (les cles de fichier visent `.nkgui` v0.2, dont tout le vocabulaire l'est).
	//    **Les miens ont ete supprimes, pas reconcilies** : deux tables de mots ne
	//    se reconcilient pas, l'une des deux se met a mentir.

	using nkentseu::editorkit::NkAlign;
	using nkentseu::editorkit::NkAlignName;
	using nkentseu::editorkit::NkAnchorName;
	using nkentseu::editorkit::NkLayoutDecl;
	using nkentseu::editorkit::NkLayoutKind;
	using nkentseu::editorkit::NkLayoutKindName;
	using nkentseu::editorkit::NkParseAlign;
	using nkentseu::editorkit::NkParseAnchor;
	using nkentseu::editorkit::NkParseLayoutKind;
	using nkentseu::editorkit::NkParseSizeMode;
	using nkentseu::editorkit::NkSizeDecl;
	using nkentseu::editorkit::NkSizeMode;
	using nkentseu::editorkit::NkSizeModeName;

	// ── UNE METRIQUE DE DOCUMENT ───────────────────────────────────────────
	// `NkLayoutDecl` ne porte PAS de nombres : sa gouttiere et sa marge sont des
	// NOMS (`spacingMetric`, `padMetric`), parce qu'un espacement est du STYLE au
	// meme titre qu'une couleur. Cette regle est celle du kit, et elle a une
	// consequence directe ici : **le document doit avoir ses propres metriques**,
	// sinon un cadre — qui n'a aucun composant, donc aucune table de metriques —
	// n'aurait nulle part ou resoudre son espacement.
	//
	// Le benefice se voit tout de suite : changer `espacement` une fois change
	// l'aeration de TOUS les noeuds qui le nomment. C'est ce qu'un nombre ecrit
	// dans chaque noeud n'aurait jamais donne.
	struct NkDocMetric {
			NkString name;
			float32 value = 0.f;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE NOEUD
	// ═══════════════════════════════════════════════════════════════════════════

	struct NkUINode {
			/// Libelle affichable dans l'arbre. Purement humain : rien ne s'y
			/// resout, deux noeuds peuvent porter le meme.
			NkString label;
			/// Cle STABLE d'un composant du registre. **Vide = un CADRE** : le noeud
			/// n'affiche rien et sert a agencer.
			NkString component;
			/// Les ecarts de CE noeud par rapport a la declaration de son composant.
			/// Vide pour un cadre.
			NkComponentInstance instance;

			/// ⚠️ TYPES DU KIT, PAS D'ICI (`NkComponentLayout.h`). Un noeud de document
			///    est, a l'execution et en mutable, ce qu'un `NkElementDecl` est a la
			///    compilation et en constant. Meme vocabulaire, meme semantique, deux
			///    provenances — c'est ce qui permet d'enregistrer un jour un document
			///    comme un composant de la bibliotheque sans rien convertir.
			NkSizeDecl width;
			NkSizeDecl height;
			NkLayoutDecl layout;   ///< s'applique a SES ENFANTS, jamais a lui-meme
			uint8 anchorEdges = 0; ///< bits `nkanchor::*`, quand le PARENT est en `Anchor`
			NkProvenance prov;

			/// Les noms de metrique que ce noeud designe. Ils ne portent aucun nombre :
			/// ils se resolvent dans la table du DOCUMENT (`NkUIDocument::MetricSource`).
			NkString spacingName;
			NkString padName;

			int32 parent = -1; ///< -1 pour la racine ; DERIVE de `children` au chargement
			NkVector<int32> children;

			bool IsFrame() const {
				const char *c = component.Data();
				return !c || !*c;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE DOCUMENT
	// ═══════════════════════════════════════════════════════════════════════════
	//
	// ⚠️ LES NOEUDS VIVENT DANS UN TABLEAU, ET ON SE PARLE EN INDEX, jamais en
	//    pointeurs. Un `NkVector` reloge ses elements quand il grandit : garder un
	//    `NkUINode*` a travers un `AddChild` serait un pointeur pendouillant qui ne
	//    se manifesterait qu'a la 17e pose de composant. Les index survivent au
	//    relogement ; ils ne survivent pas a une SUPPRESSION, et c'est pourquoi
	//    `RemoveSubtree` est la seule operation qui les renumerote — elle le dit, et
	//    elle rend la table de correspondance pour que l'appelant repare la sienne
	//    plutot que de deviner.
	class NkUIDocument {
		public:
			NkString title = NkString("Interface sans titre");
			NkProvenance prov; ///< la provenance du DOCUMENT (qui l'a cree)
			NkVector<NkUINode> nodes;

			// ── LES METRIQUES DU DOCUMENT ──────────────────────────────────────
			// Cf. `NkDocMetric` : un agencement designe sa gouttiere par un NOM, et ce
			// nom se resout ici. Une seule valeur, autant de noeuds qu'on veut.
			NkVector<NkDocMetric> metrics;

			float32 Metric(const char *name, float32 fallback = 0.f) const {
				for (uint32 i = 0; i < (uint32)metrics.Size(); ++i)
					if (StrEq(metrics[i].name.Data(), name))
						return metrics[i].value;
				return fallback;
			}
			void SetMetric(const char *name, float32 v) {
				if (!name || !*name)
					return;
				for (uint32 i = 0; i < (uint32)metrics.Size(); ++i)
					if (StrEq(metrics[i].name.Data(), name)) {
						metrics[i].value = v;
						return;
					}
				NkDocMetric m;
				m.name = NkString(name);
				m.value = v;
				metrics.PushBack(m);
			}
			/// La source que le resolveur du kit sait lire. Le document se presente a
			/// `NkLayoutSolve.h` exactement comme une declaration le ferait — c'est ce
			/// qui evite une seconde facon de resoudre un nom de metrique.
			nkentseu::editorkit::NkMetricSource MetricSource() const {
				nkentseu::editorkit::NkMetricSource s;
				s.user = this;
				s.get = [](const void *u, const char *n, float32 f) -> float32 {
					return ((const NkUIDocument *)u)->Metric(n, f);
				};
				return s;
			}

			/// Un document neuf a toujours une racine : un arbre sans racine
			/// obligerait chaque appelant a traiter le cas « vide », et ce cas se
			/// serait oublie quelque part.
			void NewDocument(const char *docTitle, NkAuthor by) {
				nodes.Clear();
				title = NkString(docTitle && *docTitle ? docTitle : "Interface sans titre");
				prov = NkProvenance();
				prov.author = by;
				// Les deux metriques que tout document possede. Elles existent des la
				// creation parce qu'un agencement les DESIGNE par leur nom : un document
				// sans elles aurait des gouttieres a zero sans que rien l'explique.
				metrics.Clear();
				SetMetric("espacement", 8.f);
				SetMetric("marge", 8.f);

				NkUINode root;
				root.label = NkString("Racine");
				root.parent = -1;
				InitNode(root, by);
				root.layout.kind = NkLayoutKind::Column;
				nodes.PushBack(root);
			}

			// ⚠️ POURQUOI LES NOMS DE METRIQUE NE VIVENT PAS DANS `layout`. Le champ
			//    `NkLayoutDecl::spacingMetric` du kit est un `const char*` — parfait pour
			//    une table compilee, intenable pour un noeud mutable : il pointerait dans
			//    une `NkString` qui se reloge des qu'on la modifie, et le pointeur
			//    survivrait juste assez longtemps pour que le defaut apparaisse ailleurs.
			//    Les noms vivent donc dans `spacingName` / `padName`, et le solveur les y
			//    lit. `layout` ne porte ici que `kind`, les alignements et `gridColumns`.
			static void InitNode(NkUINode &n, NkAuthor by) {
				// ⚠️ TAILLE PAR DEFAUT : EXTENSIBLE SUR LES DEUX AXES, et ce n'est pas un
				//    choix esthetique — c'est un MANQUE nomme. Une `NkComponentDecl` ne
				//    porte aucune taille souhaitee (ni naturelle, ni minimale) : l'outil n'a
				//    rien a lire pour proposer mieux. Poser 200x150 « parce que ca rend
				//    bien » ecrirait un chiffre que personne n'a declare, et ce chiffre
				//    ferait ensuite autorite dans tous les documents. Extensible est le seul
				//    defaut qui n'invente rien. (Manque porte au canal, cote GARDIEN.)
				n.width = nkentseu::editorkit::NkExpand();
				n.height = nkentseu::editorkit::NkExpand();
				n.layout.kind = NkLayoutKind::Column;
				n.layout.crossAlign = NkAlign::Stretch;
				n.layout.mainAlign = NkAlign::Start;
				n.spacingName = NkString("espacement");
				n.padName = NkString("marge");
				n.prov.author = by;
			}

			bool IsValidIndex(int32 i) const {
				return i >= 0 && (uint32)i < (uint32)nodes.Size();
			}
			uint32 NodeCount() const {
				return (uint32)nodes.Size();
			}

			// ── LA PALETTE : POSER UN COMPOSANT ────────────────────────────────
			// ⚠️ LE NOM DU COMPOSANT EST VERIFIE CONTRE LE REGISTRE, ET C'EST LA
			//    MOITIE UTILE DE CETTE FONCTION. Sans cette verification, une faute
			//    de frappe (ou une reponse d'IA approximative) poserait
			//    « contennt_browser » : le document se chargerait, l'arbre
			//    l'afficherait, et le rendu serait simplement vide — un defaut qui
			//    se decouvre a l'ecran, donc tard. Ici, ca rend -1, et l'appelant a
			//    un cas negatif a montrer.
			//
			// Rend l'index du noeud cree, ou -1.
			int32 AddChild(int32 parentIndex, const char *componentName, NkAuthor by,
						   const char *origin = "") {
				if (!IsValidIndex(parentIndex))
					return -1;
				const NkComponentDecl *decl = nullptr;
				if (componentName && *componentName) {
					decl = NkComponentRegistry::Find(componentName);
					if (!decl)
						return -1; // pas dans le registre : on n'invente pas un composant
				}
				NkUINode n;
				n.component = NkString(componentName ? componentName : "");
				n.label =
					NkString(decl ? (decl->title && *decl->title ? decl->title : decl->name) : "Cadre");
				if (decl)
					n.instance.Bind(*decl);
				n.parent = parentIndex;
				InitNode(n, by);
				n.prov.origin = NkString(origin ? origin : "");
				nodes.PushBack(n);
				const int32 idx = (int32)nodes.Size() - 1;
				nodes[(uint32)parentIndex].children.PushBack(idx);
				return idx;
			}

			// ── L'ARBRE DE COMPOSITION ─────────────────────────────────────────

			/// `maybeAncestor` est-il sur le chemin de `node` vers la racine ? Sert
			/// au garde-fou de `Reparent` — sans lui, deplacer un parent dans son
			/// propre enfant fabrique un cycle, et le premier parcours de l'arbre
			/// part en recursion infinie.
			bool IsAncestor(int32 maybeAncestor, int32 node) const {
				int32 c = node;
				uint32 guard = 0;
				while (IsValidIndex(c) && guard++ < (uint32)nodes.Size() + 1) {
					if (c == maybeAncestor)
						return true;
					c = nodes[(uint32)c].parent;
				}
				return false;
			}

			/// Deplacer un noeud SOUS un autre — la version « a la souris » du
			/// glisser d'un noeud vers un autre. Ce qui s'ecrit est un PARENT et un
			/// RANG, pas un point de lachage.
			bool Reparent(int32 node, int32 newParent, int32 insertAt = -1) {
				if (!IsValidIndex(node) || !IsValidIndex(newParent) || node == 0)
					return false; // la racine ne se deplace pas
				if (node == newParent || IsAncestor(node, newParent))
					return false; // cycle
				DetachFromParent(node);
				NkVector<int32> &kids = nodes[(uint32)newParent].children;
				const int32 n = (int32)kids.Size();
				const int32 at = (insertAt < 0 || insertAt > n) ? n : insertAt;
				InsertAt(kids, at, node);
				nodes[(uint32)node].parent = newParent;
				return true;
			}

			/// Changer le RANG d'un noeud dans sa fratrie. Reordonner, c'est ecrire
			/// un rang ; ce n'est pas deplacer un rectangle.
			bool MoveChild(int32 node, int32 newRank) {
				if (!IsValidIndex(node) || node == 0)
					return false;
				const int32 p = nodes[(uint32)node].parent;
				if (!IsValidIndex(p))
					return false;
				NkVector<int32> &kids = nodes[(uint32)p].children;
				int32 cur = -1;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					if (kids[i] == node)
						cur = (int32)i;
				if (cur < 0)
					return false;
				int32 to = newRank;
				const int32 last = (int32)kids.Size() - 1;
				if (to < 0)
					to = 0;
				if (to > last)
					to = last;
				if (to == cur)
					return true;
				kids.RemoveAt((uint32)cur);
				InsertAt(kids, to, node);
				return true;
			}

			/// Supprimer un noeud ET sa descendance.
			/// ⚠️ CETTE OPERATION RENUMEROTE LES INDEX — c'est la seule. Tout index
			///    garde par un appelant (selection courante, resultat de mise en
			///    page) est PERIME apres. `outRemap` rend l'ancien -> nouveau (-1
			///    pour ce qui a disparu).
			bool RemoveSubtree(int32 node, NkVector<int32> *outRemap = nullptr) {
				if (!IsValidIndex(node) || node == 0)
					return false; // on ne supprime pas la racine
				const uint32 n = (uint32)nodes.Size();
				NkVector<uint8> doomed;
				doomed.Resize(n, (uint8)0);
				MarkSubtree(node, doomed);
				DetachFromParent(node);

				NkVector<int32> remap;
				remap.Resize(n, -1);
				NkVector<NkUINode> kept;
				for (uint32 i = 0; i < n; ++i)
					if (!doomed[i]) {
						remap[i] = (int32)kept.Size();
						kept.PushBack(nodes[i]);
					}
				// Reecriture des liens dans la nouvelle numerotation.
				for (uint32 i = 0; i < (uint32)kept.Size(); ++i) {
					NkUINode &k = kept[i];
					k.parent = (k.parent >= 0) ? remap[(uint32)k.parent] : -1;
					NkVector<int32> nk;
					for (uint32 c = 0; c < (uint32)k.children.Size(); ++c) {
						const int32 m = remap[(uint32)k.children[c]];
						if (m >= 0)
							nk.PushBack(m);
					}
					k.children = nk;
				}
				nodes = kept;
				if (outRemap)
					*outRemap = remap;
				return true;
			}

			// ── PROVENANCE : LES DEUX AUTOMATISMES ─────────────────────────────
			// A APPELER APRES TOUTE MODIFICATION D'UN NOEUD PAR LA MAIN. C'est le
			// seul endroit ou `corrected` passe a vrai, et le seul ou `verified`
			// retombe a faux.
			void MarkHumanEdit(int32 node) {
				if (!IsValidIndex(node))
					return;
				NkProvenance &p = nodes[(uint32)node].prov;
				if (p.author != NkAuthor::Humain)
					p.corrected = true;
				p.verified = false;
			}

			/// Poser le tampon « rejouee » sur un sous-arbre. Reserve a ce qui a
			/// REELLEMENT ete rejoue et compare (cf. `DesignAI.h`) : c'est un
			/// constat de mesure, pas une opinion.
			void MarkVerified(int32 node) {
				if (!IsValidIndex(node))
					return;
				nodes[(uint32)node].prov.verified = true;
				const NkVector<int32> kids = nodes[(uint32)node].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					MarkVerified(kids[i]);
			}

			uint32 CountByAuthor(NkAuthor a) const {
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					if (nodes[i].prov.author == a)
						++n;
				return n;
			}
			uint32 CountCorrected() const {
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					if (nodes[i].prov.corrected)
						++n;
				return n;
			}

			// ── GREFFE D'UN SOUS-ARBRE VENU D'AILLEURS ─────────────────────────
			// C'EST LA PORTE UNIQUE, et elle est la raison pour laquelle l'IA n'a pas
			// de chemin a elle. Un document produit par un backend est charge dans un
			// document DE COTE, verifie, puis greffe ICI. La main et la machine
			// passent donc par la meme fonction, et ce qui atterrit est indiscernable
			// — c'est la regle « L'IA EST CO-AUTEUR » rendue mecanique plutot que
			// promise.
			//
			// Rend l'index de la racine greffee, ou -1.
			int32 GraftFrom(const NkUIDocument &src, int32 srcNode, int32 destParent, bool stampAuthor,
							NkAuthor author, const char *origin) {
				if (!src.IsValidIndex(srcNode) || !IsValidIndex(destParent))
					return -1;
				// ⚠️ ON VERIFIE TOUT LE SOUS-ARBRE AVANT DE TOUCHER AU DOCUMENT.
				//    Un composant inconnu au 4e niveau fait echouer la greffe
				//    ENTIERE : une greffe a moitie faite laisserait un document que
				//    personne n'a voulu, et le « rejet laisse le document intact »
				//    deviendrait faux precisement dans le cas ou il compte.
				if (!CanGraft(src, srcNode))
					return -1;
				return GraftRec(src, srcNode, destParent, stampAuthor, author, origin);
			}

			/// Tout le sous-arbre nomme-t-il des composants connus du registre ?
			static bool CanGraft(const NkUIDocument &src, int32 srcNode) {
				if (!src.IsValidIndex(srcNode))
					return false;
				const NkUINode &s = src.nodes[(uint32)srcNode];
				if (!s.IsFrame() && !NkComponentRegistry::Find(s.component.Data()))
					return false;
				for (uint32 i = 0; i < (uint32)s.children.Size(); ++i)
					if (!CanGraft(src, s.children[i]))
						return false;
				return true;
			}

			// ── FICHIER ────────────────────────────────────────────────────────
			// ⚠️ RELISEZ CE QUI EST ECRIT ET CE QUI NE L'EST PAS. Il n'y a ni `x`, ni
			//    `y`, ni `rect` : la position ne s'enregistre pas parce qu'elle
			//    n'existe pas dans le document. C'est verifiable a la lecture du
			//    fichier produit, et la sonde le verifie a chaque passage plutot que
			//    de s'en remettre a la relecture d'un humain.
			void Save(NkString &out) const {
				out = NkString("nkuidoc 1\n");
				out.Append("# Un document NkUIDesign : un ARBRE de composants declares.\n");
				out.Append("# Aucune coordonnee n'est ecrite ici, et c'est le point : chaque\n");
				out.Append("# noeud declare sa taille (fixe/extensible/poids, min, max) et\n");
				out.Append("# l'agencement de ses enfants. La position se CALCULE.\n");
				out.Append("titre = ");
				out.Append(title);
				out.Append('\n');
				out.Append("auteur = ");
				out.Append(NkAuthorName(prov.author));
				out.Append('\n');
				out.Append("verifiee = ");
				out.Append(prov.verified ? "1" : "0");
				out.Append('\n');
				out.Append("corrigee = ");
				out.Append(prov.corrected ? "1" : "0");
				out.Append('\n');
				out.Append("origine = ");
				out.Append(prov.origin);
				out.Append('\n');
				// Les metriques DU DOCUMENT. Elles precedent les noeuds parce que les
				// noeuds les designent par leur nom : un fichier se lit alors de haut
				// en bas sans jamais avoir a revenir en arriere.
				for (uint32 i = 0; i < (uint32)metrics.Size(); ++i) {
					out.Append("metrique ");
					out.Append(metrics[i].name);
					out.Append(" = ");
					WriteNum(out, metrics[i].value);
					out.Append('\n');
				}

				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
					const NkUINode &n = nodes[i];
					out.Append("\nnoeud ");
					WriteNum(out, (float32)i);
					out.Append('\n');
					Field(out, "libelle", n.label.Data());
					Field(out, "composant", n.component.Data());
					// `enfants` est la SEULE verite sur la structure : `parent` s'en
					// deduit au chargement. Ecrire les deux ferait deux verites, et
					// c'est le motif que cette tranche passe son temps a retirer
					// d'ailleurs.
					out.Append("  enfants =");
					for (uint32 c = 0; c < (uint32)n.children.Size(); ++c) {
						out.Append(' ');
						WriteNum(out, (float32)n.children[c]);
					}
					out.Append('\n');
					WriteAxis(out, "largeur", n.width);
					WriteAxis(out, "hauteur", n.height);
					// Une seule ligne pour tout l'agencement, dans l'ordre des champs de
					// `NkLayoutDecl` : agencement, alignement principal, transverse,
					// colonnes de grille.
					out.Append("  agencement = ");
					out.Append(NkLayoutKindName(n.layout.kind));
					out.Append(' ');
					out.Append(NkAlignName(n.layout.mainAlign));
					out.Append(' ');
					out.Append(NkAlignName(n.layout.crossAlign));
					out.Append(' ');
					WriteNum(out, (float32)n.layout.gridColumns);
					out.Append('\n');
					Field(out, "espacement", n.spacingName.Data());
					Field(out, "remplissage", n.padName.Data());
					char edges[5];
					NkAnchorName(n.anchorEdges, edges);
					Field(out, "ancrage", edges);
					Field(out, "auteur", NkAuthorName(n.prov.author));
					Field(out, "verifiee", n.prov.verified ? "1" : "0");
					Field(out, "corrigee", n.prov.corrected ? "1" : "0");
					Field(out, "origine", n.prov.origin.Data());
					WriteOverrides(out, n.instance);
				}
			}

			/// Rend `true` si l'en-tete a ete vu, qu'au moins une racine existe, et
			/// que la structure d'enfants est coherente. `outUnknown` compte les
			/// composants nommes que le registre ne connait pas — MEME ROLE que dans
			/// `NkComponentInstance::Load` : un document a moitie perime se charge,
			/// et il le DIT.
			///
			/// ⚠️ CE QUI EST REFUSE PLUTOT QUE RAFISTOLE : un fichier sans en-tete,
			///    sans aucun noeud, ou dont la structure est incoherente (enfant
			///    inconnu, enfant a deux parents, noeud orphelin). Un document a
			///    moitie reconstruit serait pire qu'un echec franc — l'utilisateur
			///    croirait avoir recupere son travail.
			bool Load(const char *text, uint32 *outUnknown = nullptr) {
				if (outUnknown)
					*outUnknown = 0;
				if (!text)
					return false;
				nodes.Clear();
				metrics.Clear();
				title = NkString("");
				prov = NkProvenance();

				bool sawHeader = false;
				bool inNode = false;
				NkVector<NkVector<int32>> childLists;
				NkString pendingOverrides;
				const char *p = text;
				char key[48], val[256];

				while (*p) {
					while (*p == ' ' || *p == '\t')
						++p;
					if (*p == '#' || *p == '\n' || *p == '\r') {
						SkipLine(p);
						continue;
					}
					uint32 k = 0;
					while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '\n' && *p != '\r' && k < 47)
						key[k++] = *p++;
					key[k] = 0;
					while (*p == ' ' || *p == '\t')
						++p;
					if (*p == '=') {
						++p;
						while (*p == ' ' || *p == '\t')
							++p;
					}
					uint32 v = 0;
					while (*p && *p != '\n' && *p != '\r' && v < 255)
						val[v++] = *p++;
					val[v] = 0;
					TrimEnd(val);

					if (StrEq(key, "nkuidoc")) {
						sawHeader = true;
					} else if (StrEq(key, "noeud")) {
						FlushOverrides(pendingOverrides, inNode);
						nodes.PushBack(NkUINode());
						childLists.PushBack(NkVector<int32>());
						inNode = true;
					} else if (!inNode) {
						// ── En-tete du document ──
						if (StrEq(key, "titre"))
							title = NkString(val);
						else if (StrEq(key, "auteur"))
							prov.author = NkParseAuthor(val);
						else if (StrEq(key, "verifiee"))
							prov.verified = val[0] == '1';
						else if (StrEq(key, "corrigee"))
							prov.corrected = val[0] == '1';
						else if (StrEq(key, "origine"))
							prov.origin = NkString(val);
						else if (StrEq(key, "metrique")) {
							// `metrique <nom> = <valeur>` : le nom est colle a la cle, il
							// faut donc le detacher ici plutot que dans l'analyseur general,
							// qui ne connait qu'un couple cle/valeur.
							char mname[64];
							const char *rest = SplitFirstToken(val, mname, sizeof(mname));
							if (mname[0])
								SetMetric(mname, ParseNum(rest));
						}
					} else {
						NkUINode &n = nodes[(uint32)nodes.Size() - 1];
						if (StrEq(key, "libelle"))
							n.label = NkString(val);
						else if (StrEq(key, "composant")) {
							n.component = NkString(val);
							if (val[0]) {
								const NkComponentDecl *d = NkComponentRegistry::Find(val);
								if (d)
									n.instance.Bind(*d);
								else if (outUnknown)
									(*outUnknown)++;
							}
						} else if (StrEq(key, "enfants"))
							ParseIntList(val, childLists[(uint32)childLists.Size() - 1]);
						else if (StrEq(key, "largeur"))
							ParseAxis(val, n.width);
						else if (StrEq(key, "hauteur"))
							ParseAxis(val, n.height);
						else if (StrEq(key, "agencement"))
							ParseLayout(val, n.layout);
						else if (StrEq(key, "espacement"))
							n.spacingName = NkString(val);
						else if (StrEq(key, "remplissage"))
							n.padName = NkString(val);
						else if (StrEq(key, "ancrage"))
							n.anchorEdges = NkParseAnchor(val);
						else if (StrEq(key, "auteur"))
							n.prov.author = NkParseAuthor(val);
						else if (StrEq(key, "verifiee"))
							n.prov.verified = val[0] == '1';
						else if (StrEq(key, "corrigee"))
							n.prov.corrected = val[0] == '1';
						else if (StrEq(key, "origine"))
							n.prov.origin = NkString(val);
						else if (StrEq(key, "reglage")) {
							pendingOverrides.Append(val);
							pendingOverrides.Append('\n');
						}
					}
					SkipLine(p);
				}
				FlushOverrides(pendingOverrides, inNode);

				if (!sawHeader || nodes.Size() == 0)
					return false;
				// Reconstruction des liens : `enfants` fait foi, `parent` s'en deduit.
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
					nodes[i].children.Clear();
					nodes[i].parent = -1;
				}
				for (uint32 i = 0; i < (uint32)childLists.Size(); ++i)
					for (uint32 c = 0; c < (uint32)childLists[i].Size(); ++c) {
						const int32 kid = childLists[i][c];
						if (!IsValidIndex(kid) || kid == 0 || nodes[(uint32)kid].parent >= 0)
							return false; // enfant inconnu, racine reparentee, ou deux parents
						nodes[i].children.PushBack(kid);
						nodes[(uint32)kid].parent = (int32)i;
					}
				for (uint32 i = 1; i < (uint32)nodes.Size(); ++i)
					if (nodes[i].parent < 0)
						return false; // noeud orphelin : structure incoherente
				return true;
			}

		private:
			// ── Ecriture ───────────────────────────────────────────────────────
			// Les nombres passent par `instdetail::WriteFloat` — le meme ecrivain que
			// le fichier d'ecarts. Un second formateur de flottants serait un second
			// endroit ou la virgule decimale pourrait se mettre a dependre de la
			// machine, et ce genre de divergence ne se voit qu'a l'ouverture chez
			// quelqu'un d'autre.
			static void WriteNum(NkString &out, float32 v) {
				nkentseu::editorkit::instdetail::WriteFloat(v, out);
			}
			static float32 ParseNum(const char *s) {
				return nkentseu::editorkit::instdetail::ParseFloat(s);
			}
			static void Field(NkString &out, const char *k, const char *v) {
				out.Append("  ");
				out.Append(k);
				out.Append(" = ");
				out.Append(v ? v : "");
				out.Append('\n');
			}
			static void NumField(NkString &out, const char *k, float32 v) {
				out.Append("  ");
				out.Append(k);
				out.Append(" = ");
				WriteNum(out, v);
				out.Append('\n');
			}
			/// `largeur = <mode> <valeur> <min> <max>`
			///
			/// ⚠️ `NkSizeDecl::valueMetric` N'EST PAS ENREGISTRE, ET C'EST UNE ABSENCE
			///    NOMMEE. C'est un `const char*` : le relire depuis un fichier voudrait
			///    dire le faire pointer dans une chaine que le document possede et
			///    reloge — le meme pointeur pendouillant que pour `spacingMetric`, et
			///    la solution serait la meme (une `NkString` a cote). Tant qu'aucune
			///    taille de noeud n'a besoin d'etre nommee, on ne paie pas ce champ ;
			///    le jour ou ca arrive, il se traite comme `spacingName`. Ce qui est
			///    interdit, c'est de l'ecrire a moitie : un nom sauve et non relu
			///    donnerait un document qui change de taille en le rouvrant.
			static void WriteAxis(NkString &out, const char *k, const NkSizeDecl &a) {
				out.Append("  ");
				out.Append(k);
				out.Append(" = ");
				out.Append(NkSizeModeName(a.mode));
				out.Append(' ');
				WriteNum(out, a.value);
				out.Append(' ');
				WriteNum(out, a.minVal);
				out.Append(' ');
				WriteNum(out, a.maxVal);
				out.Append('\n');
			}
			/// Les ecarts du composant, ecrits par `NkComponentInstance::Save` puis
			/// prefixes. On ne reformate rien : ce qui sort d'ici est ce que le
			/// fichier d'ecarts aurait contenu, ligne pour ligne.
			static void WriteOverrides(NkString &out, const NkComponentInstance &inst) {
				if (!inst.Decl() || inst.OverrideCount() == 0)
					return;
				NkString block;
				inst.Save(block);
				const char *s = block.Data();
				while (s && *s) {
					const char *e = s;
					while (*e && *e != '\n')
						++e;
					// On saute l'en-tete, les commentaires et la ligne `composant` : le
					// document porte deja le nom du composant, et le repeter ferait deux
					// verites qu'un jour quelqu'un ferait diverger.
					const bool skip = *s == '#' || Starts(s, "nkuicomp") || Starts(s, "composant ");
					if (!skip && e > s) {
						out.Append("  reglage ");
						for (const char *c = s; c < e; ++c)
							out.Append(*c);
						out.Append('\n');
					}
					s = *e ? e + 1 : e;
				}
			}
			static bool Starts(const char *s, const char *pre) {
				for (; *pre; ++pre, ++s)
					if (*s != *pre)
						return false;
				return true;
			}

			// ── Lecture ────────────────────────────────────────────────────────
			void FlushOverrides(NkString &pending, bool inNode) {
				if (!inNode || pending.Length() == 0)
					return;
				NkUINode &n = nodes[(uint32)nodes.Size() - 1];
				if (n.instance.Decl()) {
					// L'en-tete est SYNTHETISE : le lecteur d'ecarts l'exige pour rendre
					// `true`, et le document ne l'a pas ecrit (il a le sien).
					NkString buf("nkuicomp 1\n");
					buf.Append(pending);
					n.instance.Load(buf.Data());
				}
				pending = NkString("");
			}
			static void SkipLine(const char *&p) {
				while (*p && *p != '\n')
					++p;
				if (*p)
					++p;
			}
			static void TrimEnd(char *s) {
				int32 n = 0;
				while (s[n])
					++n;
				while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
					s[--n] = 0;
			}
			static void ParseIntList(const char *s, NkVector<int32> &out) {
				out.Clear();
				while (*s) {
					while (*s == ' ' || *s == '\t')
						++s;
					if (!*s)
						break;
					char tok[24];
					uint32 n = 0;
					while (*s && *s != ' ' && *s != '\t' && n < 23)
						tok[n++] = *s++;
					tok[n] = 0;
					if (n)
						out.PushBack((int32)ParseNum(tok));
				}
			}
			/// Le premier mot de `s`, copie dans `tok` ; rend ce qui suit, un `=`
			/// eventuel etant consomme. Sert aux lignes dont la cle porte un NOM
			/// (`metrique espacement = 8`), que l'analyseur general ne sait pas
			/// decouper puisqu'il ne connait qu'un couple cle/valeur.
			static const char *SplitFirstToken(const char *s, char *tok, uint32 cap) {
				uint32 n = 0;
				while (*s == ' ' || *s == '\t')
					++s;
				while (*s && *s != ' ' && *s != '\t' && *s != '=' && n + 1 < cap)
					tok[n++] = *s++;
				tok[n] = 0;
				while (*s == ' ' || *s == '\t')
					++s;
				if (*s == '=')
					++s;
				return s;
			}
			/// `<agencement> <alignement principal> <alignement transverse> <colonnes>`
			static void ParseLayout(const char *s, NkLayoutDecl &L) {
				char tok[4][32];
				const uint32 t = Tokenize(s, tok, 4);
				// ⚠️ LES ANALYSEURS DU KIT RENDENT `false` SUR UN MOT INCONNU SANS
				//    TOUCHER A LA SORTIE. On garde donc le defaut plutot que d'ecrire
				//    n'importe quoi : un agencement mal orthographie laisse le noeud tel
				//    qu'il etait, au lieu de le remettre silencieusement en colonne.
				if (t > 0)
					NkParseLayoutKind(tok[0], L.kind);
				if (t > 1)
					NkParseAlign(tok[1], L.mainAlign);
				if (t > 2)
					NkParseAlign(tok[2], L.crossAlign);
				if (t > 3)
					L.gridColumns = (uint8)ParseNum(tok[3]);
			}
			static uint32 Tokenize(const char *s, char (*tok)[32], uint32 maxTok) {
				uint32 t = 0;
				while (*s && t < maxTok) {
					while (*s == ' ' || *s == '\t')
						++s;
					if (!*s)
						break;
					uint32 n = 0;
					while (*s && *s != ' ' && *s != '\t' && n < 31)
						tok[t][n++] = *s++;
					tok[t][n] = 0;
					++t;
				}
				return t;
			}
			static void ParseAxis(const char *s, NkSizeDecl &a) {
				char tok[4][32];
				const uint32 t = Tokenize(s, tok, 4);
				if (t > 0)
					NkParseSizeMode(tok[0], a.mode);
				if (t > 1)
					a.value = ParseNum(tok[1]);
				if (t > 2)
					a.minVal = ParseNum(tok[2]);
				if (t > 3)
					a.maxVal = ParseNum(tok[3]);
			}

			// ── Structure ──────────────────────────────────────────────────────
			static void InsertAt(NkVector<int32> &v, int32 at, int32 value) {
				v.PushBack(value);
				for (int32 i = (int32)v.Size() - 1; i > at; --i) {
					const int32 tmp = v[(uint32)i];
					v[(uint32)i] = v[(uint32)(i - 1)];
					v[(uint32)(i - 1)] = tmp;
				}
			}
			void DetachFromParent(int32 node) {
				const int32 p = nodes[(uint32)node].parent;
				if (!IsValidIndex(p))
					return;
				NkVector<int32> &kids = nodes[(uint32)p].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					if (kids[i] == node) {
						kids.RemoveAt(i);
						return;
					}
			}
			void MarkSubtree(int32 node, NkVector<uint8> &doomed) const {
				if (!IsValidIndex(node) || doomed[(uint32)node])
					return;
				doomed[(uint32)node] = 1;
				const NkVector<int32> &kids = nodes[(uint32)node].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					MarkSubtree(kids[i], doomed);
			}
			int32 GraftRec(const NkUIDocument &src, int32 srcNode, int32 destParent, bool stampAuthor,
						   NkAuthor author, const char *origin) {
				const NkUINode &s = src.nodes[(uint32)srcNode];
				const int32 me =
					AddChild(destParent, s.component.Data(), stampAuthor ? author : s.prov.author, origin);
				if (me < 0)
					return -1;
				// ⚠️ LA PORTEE DE `d` S'ARRETE AVANT LA RECURSION, ET CE N'EST PAS
				//    DU ZELE : la recursion appelle `AddChild`, donc `PushBack`, donc
				//    un possible relogement du tableau. Une reference gardee au-dela
				//    de ce bloc serait pendouillante — et seulement a partir du jour
				//    ou l'arbre depasse la capacite courante, c'est-a-dire tard et
				//    ailleurs.
				{
					NkUINode &d = nodes[(uint32)me];
					d.label = s.label;
					d.width = s.width;
					d.height = s.height;
					d.layout = s.layout;
					d.anchorEdges = s.anchorEdges;
					d.spacingName = s.spacingName;
					d.padName = s.padName;
					d.instance = s.instance;
					if (!stampAuthor)
						d.prov = s.prov;
					else {
						d.prov.author = author;
						d.prov.origin = NkString(origin ? origin : "");
						d.prov.verified = false;
						d.prov.corrected = false;
					}
				}
				for (uint32 i = 0; i < (uint32)s.children.Size(); ++i)
					if (GraftRec(src, s.children[i], me, stampAuthor, author, origin) < 0)
						return -1;
				return me;
			}
	};

} // namespace nkuidesign
