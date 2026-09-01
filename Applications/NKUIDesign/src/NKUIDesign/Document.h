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

#include "NkDocStringPool.h"

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

			/// ⚠️ LA POSITION SUR LA TOILE -- et elle ne vaut QUE quand le PARENT est
			///    en `Free`, exactement comme `anchorEdges` au-dessus ne vaut que sous
			///    un parent `Anchor`. **Ce n'est pas un champ que tout noeud utilise.**
			///
			/// ⚠️ C'EST LE POINT DE CONCEPTION DE L'ETAPE 1, ET IL SE JOUE ICI :
			///    donner des coordonnees a TOUS les noeuds aurait debloque la toile en
			///    une heure **et detruit le modele declaratif** -- celui qui rend
			///    l'apercu depuis le 18 aout, ou la position se calcule et ou une
			///    fenetre redimensionnee replace tout sans qu'on y touche.
			///
			///    Les deux natures cohabitent donc **par le parent** : un noeud sous
			///    `Column` est calcule, un noeud sous `Free` est pose. Le document 3
			///    §14ter.5 dit que les quatre cases du tableau role x composant
			///    existent et servent ; celle de « un bouton dessine sur une page »
			///    est un noeud POSE qui porte un composant. **Une forme qui recoit un
			///    role garde ses coordonnees** : le role est un contrat pose sur elle,
			///    pas un remplacement.
			///
			/// N'est ECRITE dans le fichier que si elle n'est pas nulle -- un document
			/// purement declaratif ne porte donc aucune ligne `position`, et celui du
			/// 18 aout se reenregistre a l'identique.
			float32 posX = 0.f;
			float32 posY = 0.f;
			NkProvenance prov;

			/// ── LA NATURE DESSINÉE (2026-08-30, chaîne du designer) ─────────────
			/// Le vocabulaire est celui de la spécification §4.2 (`ShapeNode.kind`) :
			/// `rect` | `ellipse` | `text` | `image` | `path` | `frame` — rien
			/// d'inventé. **Vide = ce que le nœud était déjà** (cadre d'agencement
			/// ou composant) : un document d'avant cette clé se réenregistre à
			/// l'identique, la clé n'étant écrite que si elle existe — même règle
			/// que `position`. `frame` est l'ARTBOARD de la toile (planche 22.0) ;
			/// les autres natures sont les formes que les outils posent.
			NkString shape;
			/// Le RÔLE de l'élément (Banani §4.3 : « Button », « TextField »… —
			/// la taxonomie de l'écran 6). Vide = pas de rôle. Même règle d'écriture
			/// que `shape` : la clé n'existe dans le fichier que si elle est posée,
			/// un document d'avant se réenregistre à l'identique.
			NkString role;
			/// ── L'APPARENCE POSÉE (vocabulaire §8ter, entamé au remandat 31/08) ──
			/// Chaque champ suit la règle de `shape` : ABSENT du fichier tant qu'il
			/// n'est pas posé, un document d'avant se réenregistre à l'identique.
			/// Vide/0 = « pas posé » : le thème du document (rôles doc_*) prime.
			NkString fill;		 ///< fond, hexa « #rrggbb » (clé `fond`)
			NkString textColor;	 ///< couleur du texte, hexa (clé `couleur_texte`)
			NkString borderColor; ///< couleur du bord, hexa (clé `couleur_bord`)
			NkString alignText;	 ///< `centre` | `droite` (clé `texte_aligne`) — vide = gauche
			/// La CIBLE D'APPAREIL d'un artboard (écran 26 « Menu Cible ») :
			/// texte libre « Mobile 390 x 844 » — l'étiquette de la toile devient
			/// « <nom> — <cible> ». Vide = l'étiquette historique (nom — L × H).
			NkString target; ///< clé `cible`
			/// ── LE MULTILINGUE DU DOCUMENT (mandat 01/09) ────────────────────
			/// `texte` reste LA LANGUE PRINCIPALE (les anciens documents ne
			/// bougent pas d'un octet) ; chaque autre langue est une clé
			/// ADDITIVE `texte_<langue>` (« texte_en = Login »). Deux vecteurs
			/// appariés : la langue (« en ») et son texte.
			NkVector<NkString> texteLangues;
			NkVector<NkString> texteTraduits;
			/// Le texte pour `langue` (« » ou langue inconnue = principal).
			/// `trouve` : faux quand la langue est demandée mais non traduite —
			/// le repli doit se VOIR, pas se déduire.
			const char *TexteEn(const char *langue, bool *trouve = nullptr) const {
				if (trouve)
					*trouve = true;
				if (!langue || !*langue)
					return text.Data();
				for (uint32 i = 0; i < (uint32)texteLangues.Size(); ++i)
					if (StrEq(texteLangues[i].Data(), langue))
						return texteTraduits[i].Data();
				if (trouve)
					*trouve = false;
				return text.Data();
			}
			/// Écrit (ou remplace) la traduction de `langue` ; langue vide =
			/// le texte principal.
			void PoserTexte(const char *langue, const char *t) {
				if (!langue || !*langue) {
					text = NkString(t ? t : "");
					return;
				}
				for (uint32 i = 0; i < (uint32)texteLangues.Size(); ++i)
					if (StrEq(texteLangues[i].Data(), langue)) {
						texteTraduits[i] = NkString(t ? t : "");
						return;
					}
				texteLangues.PushBack(NkString(langue));
				texteTraduits.PushBack(NkString(t ? t : ""));
			}
			/// LA PARENTÉ DE TRANSPOSITION (tranche 1 du chantier Cible, 31/08) :
			/// le LIBELLÉ de la page dont cette page est la version transposée
			/// (« Generate Mobile » de Banani). Additive : absente du fichier
			/// tant qu'elle n'est pas posée. ⚠️ Le lien est PAR NOM (les nœuds
			/// n'ont pas d'identifiant stable) : renommer la source le casse —
			/// le rapport de transposition le dira (« source introuvable »)
			/// plutôt que de le cacher. Clé `transpose_de`.
			NkString transposeDe;
			float32 radius = 0.f;	 ///< rayon des coins, px (clé `rayon`)
			float32 borderW = 0.f;	 ///< épaisseur de bord, px (clé `bordure`)
			float32 fontPx = 0.f;	 ///< corps du texte, px (clé `police_px`) — 0 = défaut
			float32 fontWeight = 0.f; ///< graisse 100..900 (clé `graisse`) — 0 = défaut
			/// Le contenu d'un nœud `shape == "text"`. UNE ligne (le format écrit
			/// une clé par ligne, sans échappement — un retour à la ligne dans ce
			/// champ casserait la relecture, et l'éditeur n'en produit pas).
			NkString text;

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
			/// LES LANGUES SUPPLEMENTAIRES declarees par le document (mandat
			/// multilingue 01/09) : la principale n'a pas de code (c'est `texte`).
			/// Vide = document monolingue — il se reenregistre octet pour octet.
			NkVector<NkString> langues;

			// ── LE PROPRIETAIRE DES NOMS PORTES PAR LES TYPES DU KIT ───────────
			// `NkSizeDecl::valueMetric` est un `const char*` (cf. NkDocStringPool.h).
			// Le document est ce qui possede ces chaines : elles vivent exactement
			// aussi longtemps que lui.
			//
			// ⚠️ NE JAMAIS EXPOSER CE POOL EN ECRITURE A L'EXTERIEUR. Un appelant qui
			//    internerait sans reaffecter le champ ferait grandir le pool sans
			//    raison ; un appelant qui garderait le pointeur au-dela du document
			//    aurait un pendouillant. Les deux seuls chemins legitimes sont
			//    `SetSizeMetric` et la relecture.
			NkDocStringPool pool;

			// ── COPIE : PAR LES VALEURS, ET LE POOL DU DESTINATAIRE RE-INTERNE ──
			// ⚠️ MESURE AVANT CONCEPTION (2026-08-22) : `NkUIDocument` EST copiee par
			//    valeur — cinq fois rien que dans la sonde (`Probe.h`, « arranged =
			//    doc », « grid = doc », « dragged = doc », « dragged2 = doc »,
			//    « changed = reread »). Un pool non copiable rendrait donc la classe
			//    non copiable, et ces cinq lignes cesseraient de compiler. Ce n'est
			//    pas une raison de rendre le pool copiable : c'est la raison pour
			//    laquelle la COPIE DU DOCUMENT doit re-interner.
			//
			//    La copie naive (celle que le compilateur aurait ecrite sans pool)
			//    laisserait les `const char*` de la copie pointer dans le pool de la
			//    SOURCE. Tant que la source vit, tout va bien — et c'est precisement
			//    ce qui rend le defaut invisible : il n'apparait que le jour ou la
			//    source meurt la premiere. D3 le mesure.
			NkUIDocument() = default;
			NkUIDocument(const NkUIDocument &o) {
				CopyValuesFrom(o);
			}
			NkUIDocument &operator=(const NkUIDocument &o) {
				if (this != &o)
					CopyValuesFrom(o);
				return *this;
			}
			// ⚠️ LE DEPLACEMENT, LUI, N'A RIEN A RE-INTERNER — et il faut savoir
			//    pourquoi, sinon on le « corrige » un jour en le rendant couteux.
			//    Le pool est un `NkVector<NkString *>` : deplacer le pool deplace le
			//    tableau de POINTEURS, jamais les `NkString` pointees. Les adresses
			//    de contenu deja distribuees restent donc exactes. C'est le meme
			//    raisonnement qui a impose ce type dans NkDocStringPool.h.
			NkUIDocument(NkUIDocument &&) = default;
			NkUIDocument &operator=(NkUIDocument &&) = default;

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

			/// @brief Nomme la metrique qui donne sa taille a un axe de noeud.
			///
			/// C'est LE point d'entree du pool cote document, et le seul chemin
			/// legitime pour ecrire `NkSizeDecl::valueMetric` : la chaine est copiee
			/// dans le pool, donc possedee par le document, donc valide aussi
			/// longtemps que lui.
			///
			/// ⚠️ `valueMetric` PRIME SUR `value` cote kit (`NkLayoutSolve.h`, l. 131) :
			///    nommer une metrique ici change la taille effective du noeud des le
			///    prochain calcul. Un nom vide efface la designation et rend la main
			///    au nombre — c'est le seul moyen de revenir en arriere.
			///
			/// @param node  index du noeud
			/// @param horizontal `true` pour la largeur, `false` pour la hauteur
			/// @param metricName nom de metrique ; vide = plus de metrique
			/// @return `false` si l'index est invalide (rien n'est ecrit)
			bool SetSizeMetric(int32 node, bool horizontal, const char *metricName) {
				if (!IsValidIndex(node))
					return false;
				NkSizeDecl &a = horizontal ? nodes[(uint32)node].width : nodes[(uint32)node].height;
				a.valueMetric = pool.Intern(metricName);
				return true;
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
				// ⚠️ LES NOEUDS D'ABORD, LE POOL ENSUITE, ET L'ORDRE EST LA REGLE :
				//    vider le pool pendant que des noeuds tiennent encore ses
				//    pointeurs les rendrait pendouillants le temps d'une ligne. Ca
				//    passerait aujourd'hui ; ca cesserait de passer le jour ou
				//    quelque chose lit un noeud entre les deux.
				nodes.Clear();
				pool.Clear();
				title = NkString(docTitle && *docTitle ? docTitle : "Interface sans titre");
				prov = NkProvenance();
				langues.Clear();
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

			// ── LA TRANSPOSITION VERS MOBILE (tranche 1 du chantier Cible) ─────
			/// « Generate Mobile » (Banani, 31/08) : cree un artboard Mobile
			/// portant une COPIE du contenu de `pageSource`, lie par la cle
			/// additive `transpose_de` (le libelle de la source — pas d'id stable
			/// de noeud, le rapport dira « source introuvable » si on renomme).
			/// Ce que la re-disposition NE SAIT PAS traiter est CONSTATE dans
			/// `constats` — ils nourrissent le rapport de transposition (ecran
			/// 27), qui cesse d'etre vide. La synchronisation continue entre les
			/// deux versions n'est PAS cette tranche (points de rupture §8quater).
			/// Rend l'index du nouvel artboard, -1 si refus.
			int32 TransposerVersMobile(int32 pageSource, NkVector<NkString> &constats) {
				if (!IsValidIndex(pageSource) || pageSource == 0)
					return -1;
				// La convention d'AFFICHAGE des artboards Mobile de ce document :
				// 240 x 520 dessines pour une cible reelle 390 x 844 (le ratio de
				// la maquette — demo_ecran_01 fait pareil).
				const float32 wMobile = 240.f, hMobile = 520.f;
				const int32 idx = AddChild(0, "", NkAuthor::Humain);
				if (!IsValidIndex(idx))
					return -1;
				{
					const NkUINode &src = nodes[(uint32)pageSource];
					NkUINode &m = nodes[(uint32)idx];
					m.label = src.label;
					m.label.Append(" Mobile");
					m.shape = NkString("frame");
					m.layout = src.layout;
					m.width.mode = NkSizeMode::Fixed;
					m.width.value = wMobile;
					m.height.mode = NkSizeMode::Fixed;
					m.height.value = hMobile;
					const float32 wSrc =
						src.width.mode == NkSizeMode::Fixed ? src.width.value : wMobile;
					m.posX = src.posX + wSrc + 80.f;
					m.posY = src.posY;
					m.target = NkString("Mobile 390 x 844");
					m.transposeDe = src.label;
				}
				CopierEnfantsTransposes(pageSource, idx, wMobile, constats);
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

			/// LA COPIE COMPLETE D'UN SOUS-ARBRE (Copier/Coller/Dupliquer, 01/09).
			/// `src` peut etre CE document (Dupliquer) ou un AUTRE (le
			/// presse-papiers) : les noms de metrique portes en `const char*` par
			/// les types du kit sont RE-INTERNES dans le pool de CE document — un
			/// pointeur du pool source deviendrait pendouillant a la mort de sa
			/// source (contrat n.1 de NkDocStringPool).
			/// ⚠️ TOUT CHAMP NOUVEAU DE NkUINode S'AJOUTE ICI AUSSI : une copie
			///    qui oublie un champ fabrique un « collage qui perd », silencieux.
			///    (La copie de la TRANSPOSITION, elle, est volontairement
			///    partielle : elle ADAPTE — ne pas les fusionner.)
			/// @return l'index du noeud copie dans CE document, -1 si invalide.
			int32 CopierSousArbre(const NkUIDocument &src, int32 srcNode, int32 dstParent) {
				if (!src.IsValidIndex(srcNode) || !IsValidIndex(dstParent))
					return -1;
				const int32 ni = AddChild(dstParent, src.nodes[(uint32)srcNode].component.Data(),
										  src.nodes[(uint32)srcNode].prov.author);
				if (!IsValidIndex(ni))
					return -1;
				{
					// ⚠️ Références prises APRES AddChild : le NkVector reloge.
					const NkUINode &s = src.nodes[(uint32)srcNode];
					NkUINode &d = nodes[(uint32)ni];
					d.label = s.label;
					d.instance = s.instance;
					d.width = s.width;
					d.height = s.height;
					d.width.valueMetric = pool.Intern(s.width.valueMetric);
					d.height.valueMetric = pool.Intern(s.height.valueMetric);
					d.layout = s.layout;
					d.layout.spacingMetric = pool.Intern(s.layout.spacingMetric);
					d.layout.padMetric = pool.Intern(s.layout.padMetric);
					d.layout.gridCellMetric = pool.Intern(s.layout.gridCellMetric);
					d.anchorEdges = s.anchorEdges;
					d.posX = s.posX;
					d.posY = s.posY;
					d.prov = s.prov;
					d.shape = s.shape;
					d.role = s.role;
					d.fill = s.fill;
					d.textColor = s.textColor;
					d.borderColor = s.borderColor;
					d.alignText = s.alignText;
					d.target = s.target;
					d.texteLangues = s.texteLangues;
					d.texteTraduits = s.texteTraduits;
					d.transposeDe = s.transposeDe;
					d.radius = s.radius;
					d.borderW = s.borderW;
					d.fontPx = s.fontPx;
					d.fontWeight = s.fontWeight;
					d.text = s.text;
					d.spacingName = s.spacingName;
					d.padName = s.padName;
				}
				const NkVector<int32> enfants = src.nodes[(uint32)srcNode].children; // copie
				for (uint32 i = 0; i < (uint32)enfants.Size(); ++i)
					if (src.IsValidIndex(enfants[i]))
						CopierSousArbre(src, enfants[i], ni);
				return ni;
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
				// Les langues SUPPLEMENTAIRES du document (multilingue 01/09) --
				// la ligne n'existe que si le document en declare : un document
				// monolingue se reenregistre octet pour octet.
				if (langues.Size() > 0) {
					out.Append("langues =");
					for (uint32 i = 0; i < (uint32)langues.Size(); ++i) {
						out.Append(' ');
						out.Append(langues[i].Data());
					}
					out.Append('\n');
				}
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
					// La position n'est ecrite que si elle existe : un document
					// declaratif reste octet pour octet ce qu'il etait.
					if (n.posX != 0.f || n.posY != 0.f) {
						out.Append("  position = ");
						WriteNum(out, n.posX);
						out.Append(' ');
						WriteNum(out, n.posY);
						out.Append('\n');
					}
					// La nature dessinée et le texte : mêmes règles que la position —
					// écrits seulement s'ils existent, pour qu'un document d'avant
					// ces clés se réenregistre octet pour octet.
					if (!n.shape.Empty())
						Field(out, "forme", n.shape.Data());
					if (!n.text.Empty())
						Field(out, "texte", n.text.Data());
					// Les traductions ADDITIVES (multilingue 01/09) : une cle
					// `texte_<langue>` par langue posee — absentes d'un document
					// monolingue, qui se reenregistre octet pour octet.
					for (uint32 tl = 0; tl < (uint32)n.texteLangues.Size(); ++tl) {
						out.Append("  texte_");
						out.Append(n.texteLangues[tl].Data());
						out.Append(" = ");
						out.Append(n.texteTraduits[tl].Data());
						out.Append('\n');
					}
					// Le RÔLE (Banani §4.3, écran 5/6) : même règle que la nature —
					// écrit seulement s'il existe, un document d'avant se
					// réenregistre octet pour octet.
					if (!n.role.Empty())
						Field(out, "role", n.role.Data());
					// L'APPARENCE POSÉE (§8ter) : chaque clé n'existe que posée.
					if (!n.fill.Empty())
						Field(out, "fond", n.fill.Data());
					if (!n.textColor.Empty())
						Field(out, "couleur_texte", n.textColor.Data());
					if (!n.borderColor.Empty())
						Field(out, "couleur_bord", n.borderColor.Data());
					if (!n.alignText.Empty())
						Field(out, "texte_aligne", n.alignText.Data());
					if (!n.target.Empty())
						Field(out, "cible", n.target.Data());
					if (!n.transposeDe.Empty())
						Field(out, "transpose_de", n.transposeDe.Data());
					if (n.radius != 0.f) {
						out.Append("  rayon = ");
						WriteNum(out, n.radius);
						out.Append('\n');
					}
					if (n.borderW != 0.f) {
						out.Append("  bordure = ");
						WriteNum(out, n.borderW);
						out.Append('\n');
					}
					if (n.fontPx != 0.f) {
						out.Append("  police_px = ");
						WriteNum(out, n.fontPx);
						out.Append('\n');
					}
					if (n.fontWeight != 0.f) {
						out.Append("  graisse = ");
						WriteNum(out, n.fontWeight);
						out.Append('\n');
					}
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
				pool.Clear(); // apres les noeuds : voir NewDocument
				metrics.Clear();
				title = NkString("");
				prov = NkProvenance();
				langues.Clear();

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
						else if (StrEq(key, "langues")) {
							// la liste des codes de langue, separes par des espaces
							const char *q = val;
							while (*q) {
								while (*q == ' ')
									++q;
								char code[16];
								uint32 ci = 0;
								while (*q && *q != ' ' && ci + 1 < sizeof(code))
									code[ci++] = *q++;
								code[ci] = 0;
								if (ci)
									langues.PushBack(NkString(code));
							}
						} else if (StrEq(key, "metrique")) {
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
						else if (key[0] == 't' && key[1] == 'e' && key[2] == 'x' && key[3] == 't'
								 && key[4] == 'e' && key[5] == '_' && key[6]
								 && !StrEq(key + 6, "aligne")) // texte_aligne = alignement, pas une langue
							// une traduction ADDITIVE `texte_<langue>` (multilingue 01/09)
							n.PoserTexte(key + 6, val);
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
						else if (StrEq(key, "position")) {
							// ⚠️ `Tokenize`, PAS un second lecteur de nombres. `ParseNum`
							//    prend son pointeur PAR VALEUR et n'avance pas : lire deux
							//    nombres a la main relisait le premier deux fois. Le
							//    decoupage en jetons est deja le mecanisme du fichier
							//    (`ParseAxis` en lit quatre ainsi) ; en ajouter un autre,
							//    c'etait un second endroit ou le format des nombres peut
							//    diverger -- ce que l'en-tete de ce fichier interdit.
							char tok[2][32];
							const uint32 t = Tokenize(val, tok, 2);
							if (t > 0)
								n.posX = ParseNum(tok[0]);
							if (t > 1)
								n.posY = ParseNum(tok[1]);
						}
						else if (StrEq(key, "forme"))
							n.shape = NkString(val);
						else if (StrEq(key, "texte"))
							n.text = NkString(val);
						else if (StrEq(key, "role"))
							n.role = NkString(val);
						else if (StrEq(key, "fond"))
							n.fill = NkString(val);
						else if (StrEq(key, "couleur_texte"))
							n.textColor = NkString(val);
						else if (StrEq(key, "couleur_bord"))
							n.borderColor = NkString(val);
						else if (StrEq(key, "texte_aligne"))
							n.alignText = NkString(val);
						else if (StrEq(key, "cible"))
							n.target = NkString(val);
						else if (StrEq(key, "transpose_de"))
							n.transposeDe = NkString(val);
						else if (StrEq(key, "rayon"))
							n.radius = ParseNum(val);
						else if (StrEq(key, "bordure"))
							n.borderW = ParseNum(val);
						else if (StrEq(key, "police_px"))
							n.fontPx = ParseNum(val);
						else if (StrEq(key, "graisse"))
							n.fontWeight = ParseNum(val);
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

			/// La copie recursive de la transposition (TransposerVersMobile).
			/// ⚠️ AUCUNE reference de noeud ne survit a un AddChild (relogement
			/// du NkVector) : tout se relit par INDEX apres chaque ajout.
			/// `wParent` : la largeur du parent COPIE, pour les constats de
			/// depassement (les positions posees sont relatives au parent).
			void CopierEnfantsTransposes(int32 src, int32 dst, float32 wParent,
										 NkVector<NkString> &constats) {
				const NkVector<int32> enfants = nodes[(uint32)src].children; // copie
				for (uint32 i = 0; i < (uint32)enfants.Size(); ++i) {
					const int32 e = enfants[i];
					if (!IsValidIndex(e))
						continue;
					const int32 ne = AddChild(dst, nodes[(uint32)e].component.Data(),
											  nodes[(uint32)e].prov.author);
					if (!IsValidIndex(ne))
						continue;
					{
						const NkUINode &s = nodes[(uint32)e];
						NkUINode &d = nodes[(uint32)ne];
						d.label = s.label;
						d.shape = s.shape;
						d.text = s.text;
						d.role = s.role;
						d.fill = s.fill;
						d.textColor = s.textColor;
						d.borderColor = s.borderColor;
						d.alignText = s.alignText;
						d.radius = s.radius;
						d.borderW = s.borderW;
						d.fontPx = s.fontPx;
						d.fontWeight = s.fontWeight;
						d.layout = s.layout;
						d.anchorEdges = s.anchorEdges;
						d.posX = s.posX;
						d.posY = s.posY;
						// meme document, meme pool : la copie membre a membre des
						// axes est sure ici (cf. CopyAxe pour le cas inter-doc)
						d.width = s.width;
						d.height = s.height;
						d.spacingName = s.spacingName;
						d.padName = s.padName;
						d.instance = s.instance;
					}
					// LES CONSTATS : ce que la cible etroite ne sait pas absorber
					// — ils nourrissent le rapport de transposition (ecran 27).
					{
						const NkUINode &s = nodes[(uint32)e];
						NkString c;
						if (s.width.mode == NkSizeMode::Fixed && s.width.value > wParent) {
							c = NkString("\xC2\xAB ");
							c.Append(s.label);
							c.Append(" \xC2\xBB : largeur fixe ");
							char num[16];
							WriteNumTo(num, sizeof(num), s.width.value);
							c.Append(num);
							c.Append(" > cible ");
							WriteNumTo(num, sizeof(num), wParent);
							c.Append(num);
							c.Append(" \xE2\x80\x94 a redimensionner");
							constats.PushBack(c);
						}
						if (s.posX < 0.f
							|| (s.width.mode == NkSizeMode::Fixed
								&& s.posX + s.width.value > wParent)) {
							c = NkString("\xC2\xAB ");
							c.Append(s.label);
							c.Append(" \xC2\xBB : position posee hors bornes de la cible");
							constats.PushBack(c);
						}
					}
					const float32 wEnfant =
						nodes[(uint32)e].width.mode == NkSizeMode::Fixed
							? nodes[(uint32)e].width.value
							: wParent;
					CopierEnfantsTransposes(e, ne, wEnfant, constats);
				}
			}

			/// Un nombre entier court dans un tampon (les constats ci-dessus —
			/// pas de NkFormat ici : la porte des accolades).
			static void WriteNumTo(char *out, nkentseu::usize cap, float32 v) {
				const int32 n = (int32)v;
				nkentseu::usize k = 0;
				int32 x = n < 0 ? -n : n;
				char tmp[12];
				nkentseu::usize t = 0;
				do {
					tmp[t++] = (char)('0' + (x % 10));
					x /= 10;
				} while (x > 0 && t < 11);
				if (n < 0 && k + 1 < cap)
					out[k++] = '-';
				while (t > 0 && k + 1 < cap)
					out[k++] = tmp[--t];
				out[k] = 0;
			}

			// ── LE POOL, VU DE L'INTERIEUR ──────────────────────────────────
			// Les deux seuls gestes qui font entrer une chaine etrangere dans le pool
			// de CE document. Tout le reste (relecture, `SetSizeMetric`) passe par eux
			// ou par `pool.Intern` directement.

			/// @brief Copie un axe en RE-INTERNANT son nom de metrique dans notre pool.
			///
			/// La copie membre a membre d'un `NkSizeDecl` recopie `valueMetric`, qui est
			/// un pointeur. Entre deux noeuds du MEME document c'est inoffensif (meme
			/// pool) ; depuis un AUTRE document c'est un pointeur qui survit a son
			/// proprietaire. Comme rien dans le type ne distingue les deux cas, on
			/// re-interne toujours : le cout est une chaine de plus dans un pool qui ne
			/// deduplique deja pas, et la regle n'a aucune exception a retenir.
			void AdoptSize(NkSizeDecl &dst, const NkSizeDecl &src) {
				dst = src;
				dst.valueMetric = pool.Intern(src.valueMetric);
			}

			/// @brief Rend un noeud DEJA copie proprietaire de ses chaines.
			/// A appeler juste apres une copie de valeurs venue d'un autre document.
			void AdoptNode(NkUINode &n) {
				n.width.valueMetric = pool.Intern(n.width.valueMetric);
				n.height.valueMetric = pool.Intern(n.height.valueMetric);
			}

			/// @brief Le corps de la copie et de l'affectation par copie.
			///
			/// ⚠️ L'ORDRE EST LA CORRECTION, PAS UN DETAIL. On copie les noeuds PENDANT
			///    que `o` est vivante (leurs `valueMetric` pointent alors chez elle), on
			///    vide NOTRE pool — ce qui n'invalide rien, puisque plus aucun de nos
			///    noeuds ne le lit —, puis on re-interne. Vider le pool en premier
			///    marcherait aussi ; re-interner avant de copier, non. Ecrit ici parce
			///    que les trois ordres se ressemblent a la lecture.
			///
			/// ⚠️ L'AUTO-AFFECTATION EST ECARTEE PAR L'APPELANT (`this != &o`), et il
			///    faut qu'elle le reste : `pool.Clear()` detruirait ici les chaines que
			///    nos propres noeuds designent.
			void CopyValuesFrom(const NkUIDocument &o) {
				title = o.title;
				prov = o.prov;
				metrics = o.metrics;
				// ⚠️ CHAQUE CHAMP NOUVEAU DU DOCUMENT DOIT PASSER ICI — defaut
				//    paye le 01/09 : `langues` oublie, l'annulation d'une langue
				//    declaree laissait la ligne dans le document restaure (la
				//    copie est ECRITE A LA MAIN a cause du pool).
				langues = o.langues;
				nodes = o.nodes;
				pool.Clear();
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					AdoptNode(nodes[i]);
			}
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
			/// `largeur = <mode> <valeur> <min> <max> [<metrique>]`
			///
			/// ⚠️ LE 2026-08-18 CE COMMENTAIRE DISAIT UNE ABSENCE ; ELLE EST LEVEE.
			///    Il disait que `NkSizeDecl::valueMetric` n'etait pas enregistre parce que
			///    c'est un `const char*` sans proprietaire, et que le jour venu il se
			///    traiterait « comme `spacingName` », c'est-a-dire par une `NkString`
			///    posee a cote. Ce n'est PAS ce qui a ete fait, et la difference porte :
			///    une `NkString` a cote aurait fait DEUX verites pour un seul fait — le
			///    champ que le solveur du kit lit (`NkLayoutSolve.h`, l. 131) et la chaine
			///    que le document garde —, avec la charge permanente de les tenir
			///    d'accord. Le pool (`NkDocStringPool.h`) donne un proprietaire au champ
			///    LUI-MEME : le solveur lit la seule verite qui existe, et il n'y a rien
			///    a synchroniser.
			///
			/// ⚠️ LE 5e JETON N'EST ECRIT QUE S'IL EST NON VIDE, et c'est un choix :
			///    un document qui ne nomme aucune taille se reecrit OCTET POUR OCTET comme
			///    avant cette tranche, donc le corpus existant n'a pas bouge — et ce
			///    n'est pas une supposition, D5 le mesure sur un fichier ecrit avant. Le
			///    prix est un champ de longueur variable ; le prix de l'autre choix aurait
			///    ete de reecrire tous les fichiers du depot pour y poser un champ vide.
			///
			/// ⚠️ CE QUI RESTE INTERDIT est inchange : ECRIRE A MOITIE. Un nom sauve et
			///    non relu donnerait un document qui change de taille en le rouvrant.
			///    C'est pourquoi `ParseAxis` le relit DANS LE MEME COMMIT.
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
				if (a.valueMetric && *a.valueMetric) {
					out.Append(' ');
					out.Append(a.valueMetric);
				}
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
			/// ⚠️ N'EST PLUS `static` : relire le 5e jeton veut dire l'INTERNER, donc
			///    toucher au pool, donc au document. C'est toute la raison du changement
			///    de signature.
			///
			/// ⚠️ LA REGLE ASYMETRIQUE DU VERDICT S'APPLIQUE ICI, et elle explique
			///    pourquoi cette fonction ne rend toujours rien. Un 5e jeton ABSENT est
			///    LEGITIME : c'est tout fichier ecrit avant cette tranche, et le refuser
			///    changerait une compatibilite ascendante en panne. Ce qui serait une
			///    faute, c'est un jeton PRESENT et perdu — et il ne peut pas l'etre, il
			///    est pris tel quel, sans grammaire a respecter.
			///
			/// ⚠️ BORNE CONNUE, ET ELLE N'EST PAS PROPRE A CE CHAMP : `Load` lit une
			///    ligne entiere dans `val[256]`. Un nom tres long serait tronque avec le
			///    reste de sa ligne — c'est la borne de TOUT le format, pas une nouvelle.
			///    Le nom ne passe volontairement PAS par `Tokenize`, qui coupe lui a 31
			///    caracteres : la troncature y aurait ete silencieuse ET propre a ce champ.
			void ParseAxis(const char *s, NkSizeDecl &a) {
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
				if (t == 4) {
					// Ce qui reste de la ligne apres les quatre nombres : le nom de
					// metrique, pris entier.
					const char *rest = SkipTokens(s, 4);
					if (*rest)
						a.valueMetric = pool.Intern(rest);
				}
			}
			/// @brief Rend le pointeur juste apres les `n` premiers jetons de `s`, les
			///        espaces de tete manges. Rend la fin de chaine s'il y a moins de
			///        `n` jetons.
			static const char *SkipTokens(const char *s, uint32 n) {
				for (uint32 i = 0; i < n; ++i) {
					while (*s == ' ' || *s == '\t')
						++s;
					while (*s && *s != ' ' && *s != '\t')
						++s;
				}
				while (*s == ' ' || *s == '\t')
					++s;
				return s;
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
					// ⚠️ LES DEUX AXES PASSENT PAR `AdoptSize`, JAMAIS PAR UNE AFFECTATION
					//    DIRECTE. `NkSizeDecl` porte `valueMetric`, un `const char*` qui pointe
					//    dans le pool de `src` — un AUTRE document. Le recopier tel quel ferait
					//    un noeud qui lit chez son voisin, et le defaut n'apparaitrait qu'a la
					//    mort du voisin, c'est-a-dire loin d'ici. D4 le mesure.
					AdoptSize(d.width, s.width);
					AdoptSize(d.height, s.height);
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
