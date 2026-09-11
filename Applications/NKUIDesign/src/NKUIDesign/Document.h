#pragma once
// -----------------------------------------------------------------------------
// @File    Document.h
// @Brief   LE DOCUMENT : un arbre de composants, et AUCUNE coordonnee dedans.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
	//  UN REMPLISSAGE (Lunacy « FILLS »)
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ TROIS CHAMPS, ET PAS UN DE PLUS POUR L'INSTANT. La capture de
	///    reference (`lunacy_props_12`) montre exactement ceci sur une ligne :
	///    une pastille de couleur, l'hexa, un pourcentage, un oeil, une
	///    poubelle. Le TYPE de remplissage (degrade, image, bruit) et le mode
	///    de fusion PAR remplissage existent chez Lunacy et **ne sont pas ici**
	///    : ils sont nommes, pas ebauches. Un champ `type` pose maintenant, que
	///    ni le peintre ni l'inspecteur n'honorent, serait le « parametre
	///    declare non honore » que ce depot compte depuis huit fois.
	// ════════════════════════════════════════════════════════════════════════════
	//  UN DEGRADE (Lunacy « Gradient »)
	// ════════════════════════════════════════════════════════════════════════════
	/// UN ARRET DE COULEUR : une position sur l'axe [0..1] et sa couleur.
	/// 🔑 C'EST L'UNITE DU GESTE, pas un detail de format : un degrade se regle
	///    en DEPLACANT des arrets, jamais en tapant une chaine. Le modele porte
	///    donc la position comme une VALEUR MANIPULABLE, pas comme un rang.
	inline bool StrStartsWith(const char *s, const char *prefixe);
	// ════════════════════════════════════════════════════════════════════════
	//  LA VARIABLE : un nom pour UNE valeur (§15.14, 04/09)
	// ════════════════════════════════════════════════════════════════════════
	/// Une valeur par MODE : la place reservee pour Dark Pro / Light Pro.
	struct NkValeurMode {
			NkString mode;	 ///< « clair », « sombre »... texte libre
			NkString valeur; ///< la valeur dans ce mode
	};
	/// `cle` : sans espace, c'est elle qu'une reference nomme (« @primaire »).
	/// `nom` : libre, celui que la ligne affiche (« Dark Primary »). `valeur` :
	/// la valeur par defaut ; `parMode` : les valeurs par mode ; `inconnus` :
	/// les jetons non compris de la ligne `variable`, reemis tels quels.
	struct NkVariable {
			NkString cle;
			NkString nom;
			NkString valeur;
			NkVector<NkValeurMode> parMode;
			NkString inconnus;
			/// La valeur pour ce mode -- ou la valeur par defaut si le mode n'en a pas.
			const char *ValeurPour(const char *mode) const {
				if (mode && *mode)
					for (uint32 i = 0; i < (uint32)parMode.Size(); ++i)
						if (NkComponentDecl::StrEq(parMode[i].mode.Data(), mode))
							return parMode[i].valeur.Data();
				return valeur.Data();
			}
			/// POSER une valeur : dans le mode s'il est declare, sinon la valeur par
			/// defaut -- c'est celle que l'oeil voit dans ce mode (`ValeurPour`), donc
			/// c'est celle que le selecteur edite. Ecrire et lire par la meme regle.
			void PoserValeur(const char *mode, const char *v) {
				if (mode && *mode)
					for (uint32 i = 0; i < (uint32)parMode.Size(); ++i)
						if (NkComponentDecl::StrEq(parMode[i].mode.Data(), mode)) {
							parMode[i].valeur = NkString(v ? v : "");
							return;
						}
				valeur = NkString(v ? v : "");
			}
	};
	/// Une couleur est-elle une REFERENCE (« @cle ») plutot qu'une valeur ?
	inline bool NkEstReference(const char *c) {
		return c && c[0] == '@' && c[1];
	}

	struct NkArretDegrade {
			float32 position = 0.f; ///< 0..1 le long de l'axe
			NkString couleur;		///< hexa « #rrggbb »
			/// L'OPACITE DE CET ARRET (0..100), comme la liste d'arrets de Lunacy
			/// (capture 04/09 : `0% · pastille · FFFFFF · 100% · poubelle`).
			/// Additive : absente du fichier tant qu'elle vaut 100.
			float32 opacite = 100.f;
	};

	/// UN DEGRADE porte par un remplissage.
	///
	/// ⚠️ `type` EST DU TEXTE LIBRE, ET C'EST VOULU : le format doit se relire
	///    **sans que l'application connaisse tous les types**. Un `enum` aurait
	///    force le lecteur a choisir entre refuser un fichier qu'il ne comprend
	///    pas et le degrader en silence -- les deux sont pires que garder le mot.
	///    Un `conique` ecrit par une version future revient intact d'un
	///    aller-retour, meme si cette version-ci ne sait pas le PEINDRE.
	///    C'est la meme regle que la cle `unite` des cibles et que les etats.
	///
	/// ⚠️ DEUX ARRETS AU MINIMUM POUR PEINDRE : en dessous, ce n'est pas un
	///    degrade, c'est une couleur -- et le remplissage sait deja la dire.
	struct NkDegrade {
			NkString type;	   ///< « lineaire », « radial »... texte libre, PRESERVE
			float32 angle = 0.f; ///< degres, pour les types qui en ont un
			/// ① RADIAL / ANGULAIRE / LOSANGE (captures de Rodolf, 04/09) : l'ORIGINE et les
			///    RAYONS, en fractions de la boite. 0,5 / 0,5 = le centre ; rayons 0,5 = t vaut
			///    1 sur le bord. Fichier : `o=<x>,<y>` et `r=<x>,<y>`, ecrits hors defaut seulement.
			float32 origineX = 0.5f, origineY = 0.5f;
			float32 rayonX = 0.5f, rayonY = 0.5f;
			NkString inconnus; ///< jetons `cle=valeur` non compris de la ligne `degrade_`, reemis tels quels
			NkVector<NkArretDegrade> arrets;

			bool Actif() const {
				return arrets.Size() >= 2;
			}
	};

	struct NkRemplissage {
			NkString couleur;		///< hexa « #rrggbb », vide = rien a peindre
			float32 opacite = 100.f; ///< 0..100 (le « 100% » de la reference)
			bool visible = true;	///< l'oeil de la reference
			/// LE DEGRADE de ce remplissage. Sans arrets = remplissage uni, et le
			/// fichier ne gagne AUCUNE cle : un document d'avant se reenregistre
			/// octet pour octet.
			NkDegrade degrade;
			// ── LE REMPLISSAGE IMAGE (capture Lunacy du 04/09) ─────────────────
			/// `genre` : vide = uni ou degrade selon les arrets ; « image ». Texte
			/// libre, inconnu preserve. `cadrage` : Fill (vide) · Fit · Stretch ·
			/// Tile · Crop -- les cinq mots de son menu, tels quels. `image` : la
			/// source (chemin), que le peintre ne charge pas encore -- il peint le
			/// damier et le dit. Tous additifs : rien au fichier tant que vides.
			NkString genre;
			NkString image;
			NkString cadrage;
			float32 rotationImage = 0.f;
			/// LA FENETRE DU CADRAGE `crop` (fractions de l'image, 0..1) : `crop=x,y,w,h`,
			/// additif (rien tant que la fenetre est l'image entiere). Les autres
			/// cadrages l'ignorent -- un champ qui n'agit pas ne s'ecrit pas.
			float32 cropX = 0.f, cropY = 0.f, cropW = 1.f, cropH = 1.f;
			bool CropEntier() const {
				return cropX == 0.f && cropY == 0.f && cropW == 1.f && cropH == 1.f;
			}
			/// ②-1 LE MODE DE FUSION (la goutte, 18 modes) : la cle CSS `mix-blend-mode`
			///    (« multiply », « screen »...) telle que Lunacy l'exporte ; vide = normal.
			///    Une propriete du document, ecrite et relue, montree sur la ligne --
			///    peinte quand le peintre le sait, dite sinon. Texte libre, inconnu preserve.
			NkString fusion;
			NkString inconnus; ///< jetons non compris de la ligne `fond_`, reemis tels quels
			bool EstImage() const {
				return NkComponentDecl::StrEq(genre.Data(), "image");
			}
	};

	// ════════════════════════════════════════════════════════════════════════════
	//  UNE SURCHARGE D'APPARENCE PAR ETAT (bloc `appearance(Etat)` du format)
	// ════════════════════════════════════════════════════════════════════════════
	/// 🔑 SURCHARGE **PAR PROPRIETE**, jamais une copie d'apparence complete --
	///    la meme forme que le masque d'ecarts d'une instance (§15.3) : chaque
	///    champ vaut « heritee » tant qu'il n'est pas pose. Une copie complete
	///    obligerait a rejouer TOUT l'etat Normal dans chaque bloc, et le jour
	///    ou le fond du Normal change, les cinq autres mentiraient.
	///
	/// ⚠️ `etat` PORTE UN NOM DE LA TABLE FERMEE (`guifmt::NkGEtats`), et rien
	///    d'autre. La liste est tranchee par Rodolf (27/08) et son ORDRE EST LA
	///    PRIORITE -- deja tenu par un controle (26h). Aucun code ne reecrit
	///    cette liste : il l'ITERE.
	struct NkApparenceEtat {
			NkString etat;			///< « Hover », « Pressed »... (table fermee)
			NkString fond;			///< hexa, vide = HERITE de l'etat Normal
			float32 radius = -1.f;	///< < 0 = HERITE
			/// L'OPACITE DU NŒUD (0..100), < 0 = HERITE. 🔴 SENS TRANCHE PAR RODOLF LE
			///   11/09 : c'est celle de CALQUE, qui s'applique aux enfants -- un bouton
			///   Disabled a 50 % s'estompe EN ENTIER. Le lot 652632e6e l'avait branchee
			///   sur l'opacite du FOND ; aucun document n'en portait, la cle
			///   `apparence_<Etat>` garde sa forme (troisieme jeton), son sens est dit.
			float32 opacite = -1.f;
			// ── CE QU'UN ETAT PEUT AUSSI VOULOIR DIRE (11/09, recensement Q143) ──
			// Sur quinze sections, QUATRE ont un sens par etat : la bordure, l'ombre,
			// la couleur du texte, l'opacite du nœud. Les quatre sont ici desormais :
			// l'ombre et la couleur du texte (lot ①), la bordure (lot ② b, par sa
			// porte `BorduresEffectives`), l'opacite du nœud (lot ③, ci-dessus).
			// ⚠️ PAR CHAMP, PAS PAR BLOC : un etat qui ne pose que l'ombre laisse tout
			//    le reste a la base. C'est ce qui rend « Hover = un peu plus d'ombre »
			//    exprimable sans recopier le nœud entier.
			NkString couleurTexte;		///< hexa, vide = HERITE
			float32 ombreFlou = -1.f;	///< px, < 0 = HERITE (le flou de CHAQUE ombre)
			float32 ombreOpacite = -1.f; ///< 0..100, < 0 = HERITE
			bool OmbrePosee() const {
				return ombreFlou >= 0.f || ombreOpacite >= 0.f;
			}
			/// LA BORDURE PAR ETAT (11/09, lot ② b) -- par champ, comme l'ombre : la
			/// couleur et l'epaisseur de l'etat remplacent celles de CHAQUE bordure
			/// qui se peint (par la porte `BorduresEffectives`) ; position, jointure,
			/// cotes restent ceux du noeud. Epaisseur 0 posee = AUCUNE bordure dans
			/// cet etat (un bouton Pressed qui perd son trait). < 0 / vide = herite.
			NkString bordureCouleur;		 ///< hexa, vide = HERITE
			float32 bordureEpaisseur = -1.f; ///< px, < 0 = HERITE, 0 = retiree
			bool BordurePosee() const {
				return !bordureCouleur.Empty() || bordureEpaisseur >= 0.f;
			}

			/// Vrai si ce bloc ne pose RIEN — il n'a alors pas a etre ecrit.
			bool Vide() const {
				return fond.Empty() && radius < 0.f && opacite < 0.f && couleurTexte.Empty()
					   && !OmbrePosee() && !BordurePosee();
			}
	};

	/// LA PORTE DES ETATS : le bloc de `n` pour `etat`, cree s'il manque.
	/// ⚠️ UNE SEULE ECRITURE, pour que le panneau, la recette et un futur
	///    « reinitialiser » cherchent le bloc de la MEME facon. Deux boucles de
	///    recherche divergeraient au premier etat renomme.
	template <class N>
	inline NkApparenceEtat &NkBlocEtat(N &n, const char *etat) {
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.apparences.Size(); ++i)
			if (NkComponentDecl::StrEq(n.apparences[i].etat.Data(), etat))
				return n.apparences[i];
		NkApparenceEtat a;
		a.etat = NkString(etat);
		n.apparences.PushBack(a);
		return n.apparences[(nkentseu::uint32)n.apparences.Size() - 1];
	}

	/// RETIRE le bloc de `etat` s'il existe. ⚠️ Vider le dernier champ d'un
	/// etat doit le faire DISPARAITRE : un bloc vide laisse dans la liste
	/// ecrirait une cle `apparence_<Etat>` fantome... ou plutot NE l'ecrirait
	/// pas (l'ecriture saute les blocs vides) mais garderait un etat « pose »
	/// a l'ecran sans rien derriere -- l'interface et le fichier se
	/// contrediraient. *Ce que l'ecran montre comme absent doit etre absent.*
	template <class N>
	inline void NkRetirerBlocEtat(N &n, const char *etat) {
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.apparences.Size(); ++i)
			if (NkComponentDecl::StrEq(n.apparences[i].etat.Data(), etat)) {
				n.apparences.RemoveAt(i);
				return;
			}
	}

	/// Le bloc de `etat` s'il EXISTE, sinon nul — pour lire sans creer.
	template <class N>
	inline const NkApparenceEtat *NkBlocEtatSi(const N &n, const char *etat) {
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.apparences.Size(); ++i)
			if (NkComponentDecl::StrEq(n.apparences[i].etat.Data(), etat))
				return &n.apparences[i];
		return nullptr;
	}

	// ════════════════════════════════════════════════════════════════════════════
	//  UNE BORDURE (Lunacy « BORDERS »)
	// ════════════════════════════════════════════════════════════════════════════
	/// Le remplissage, PLUS l'epaisseur et la POSITION -- les deux champs que
	/// `lunacy_props_12` montre sur la seconde ligne de sa section BORDERS
	/// (« 1 » et « Outside ⌄ »).
	///
	/// ⚠️ LA POSITION EST UN CHAMP DU MODELE, PAS UN REGLAGE DE PEINTRE. Une
	///    bordure « interieure », « centree » ou « exterieure » ne change pas la
	///    couleur : elle change la GEOMETRIE PEINTE, donc l'encombrement visuel
	///    de la forme. La ranger cote peintre l'aurait rendue invisible au
	///    fichier -- et deux documents identiques auraient rendu differemment
	///    selon le peintre qui les ouvre.
	enum class NkBordurePos : uint8 {
		Interieur = 0, ///< le trait mord VERS L'INTERIEUR du rectangle
		Centre = 1,	   ///< a cheval sur le bord (le comportement historique)
		Exterieur = 2  ///< le trait deborde VERS L'EXTERIEUR
	};
	struct NkBordure {
			NkString couleur;		 ///< hexa « #rrggbb », vide = rien a peindre
			float32 opacite = 100.f; ///< 0..100
			bool visible = true;	 ///< l'oeil
			float32 epaisseur = 1.f; ///< px, le « 1 » de la reference
			NkBordurePos position = NkBordurePos::Centre;
			// ── PAR COTE, JOINTURE, EXTREMITE (03/09) ────────────────────────
			/// Quatre epaisseurs : haut, droite, bas, gauche ; < 0 = `epaisseur`.
			float32 cotes[4] = {-1.f, -1.f, -1.f, -1.f};
			/// Texte libre, preserve : « onglet » (defaut, vide), « rond », « biseau ».
			NkString jointure;
			/// Texte libre, preserve : « plate » (defaut, vide), « ronde », « carree ».
			/// Ne vaut que pour une forme OUVERTE (ligne) : l'inspecteur la grise ailleurs.
			NkString extremite;
			/// Jetons non compris sur la ligne `bord_`, reemis tels quels.
			NkString inconnus;
			float32 Cote(uint32 i) const {
				const float32 v = cotes[i < 4u ? i : 0u];
				return v >= 0.f ? v : (epaisseur > 0.f ? epaisseur : 1.f);
			}
			bool CotesEgaux() const {
				return cotes[0] < 0.f && cotes[1] < 0.f && cotes[2] < 0.f && cotes[3] < 0.f;
			}
	};
	inline const char *NkBordurePosNom(NkBordurePos p) {
		switch (p) {
			case NkBordurePos::Interieur: return "interieur";
			case NkBordurePos::Exterieur: return "exterieur";
			default: return "centre";
		}
	}
	// ═══════════════════════════════════════════════════════════════════════════
	//  UN EFFET (Lunacy « EFFECTS »)
	// ═══════════════════════════════════════════════════════════════════════════
	/// Le gabarit EXACT de `lunacy_props_11`, relu à la source : un type
	/// (« Shadow ⌄ »), puis **X, Y, flou, étendue**, puis **couleur + opacité**,
	/// avec l'œil et la poubelle sur la ligne du type.
	///
	/// ⚠️ QUATRE NOMBRES, PAS DEUX. Une ombre n'est pas « un décalage » : elle a
	///    un déport (X, Y), un FLOU (l'adoucissement du bord) et une ÉTENDUE
	///    (le grossissement avant flou). Les confondre donnerait un contrôle qui
	///    répond à deux gestes sur quatre — et l'utilisateur croirait que le
	///    modèle est cassé alors qu'il serait seulement incomplet.
	///
	/// ⚠️ LE TYPE EST UN ENUM, ET IL N'EN PORTE QUE DEUX. Lunacy en a plus
	///    (flou de calque, flou d'arrière-plan…). Deux sont posés parce que deux
	///    sont PEINTS ; les autres seront ajoutés avec leur peintre, pas avant.
	enum class NkEffetType : uint8 {
		OmbrePortee = 0, ///< « Drop shadow » : l'ombre TOMBE hors de la forme
		OmbreInterne = 1 ///< « Inner shadow » : elle se creuse à l'intérieur
	};
	struct NkEffet {
			NkEffetType type = NkEffetType::OmbrePortee;
			float32 x = 0.f;		 ///< déport horizontal, px
			float32 y = 4.f;		 ///< déport vertical, px (le « Y 4 » de la référence)
			float32 flou = 4.f;		 ///< rayon d'adoucissement, px
			float32 etendue = 0.f;	 ///< grossissement avant flou, px
			NkString couleur;		 ///< hexa « #rrggbb »
			float32 opacite = 25.f;	 ///< 0..100 (le « 25% » de la référence)
			bool visible = true;	 ///< l'œil
	};
	inline const char *NkEffetTypeNom(NkEffetType t) {
		return t == NkEffetType::OmbreInterne ? "ombre_interne" : "ombre_portee";
	}
	inline NkEffetType NkParseEffetType(const char *s) {
		// « ombre_interne » et « ombre_portee » partagent leurs six premiers
		// caractères : on discrimine sur le SEPTIEME, pas sur le premier.
		return (s && s[6] == 'i') ? NkEffetType::OmbreInterne : NkEffetType::OmbrePortee;
	}

	inline NkBordurePos NkParseBordurePos(const char *s) {
		if (s && s[0] == 'i')
			return NkBordurePos::Interieur;
		if (s && s[0] == 'e')
			return NkBordurePos::Exterieur;
		return NkBordurePos::Centre;
	}

	/// UN SOMMET, en coordonnées UNITAIRES (-1..1) dans la boîte de la forme.
	/// ⚠️ UNITAIRE, ET PAS EN PIXELS. Un sommet en pixels absolus se décrocherait
	///    de sa forme dès qu'on la redimensionne — la position d'un point est un
	///    RÉSULTAT de la boîte, exactement comme la position d'un nœud est un
	///    résultat de sa disposition. C'est la même règle, appliquée un cran
	///    plus bas.
	struct NkPoint2 {
			float32 x = 0.f;
			float32 y = 0.f;
			/// ── L'ARRONDI DE CE SOMMET (Lunacy, retour de Rodolf du 01/09) ───
			/// « si on double-clique sur une poignée sombre on peut l'arrondir. »
			/// C'est un rayon PAR SOMMET, en pixels, distinct du `radius` global
			/// du nœud : `radius` arrondit les quatre coins d'une boîte, celui-ci
			/// arrondit UN sommet d'un tracé, et le doc 3 §8bis les sépare
			/// nommément (*« pas seulement un rayon global de rectangle »*).
			///
			/// ⚠️ ET IL EST HONORÉ AU DESSIN, sinon ce serait un paramètre déclaré
			///    qui n'agit pas : `NkContourDe` (Sommets.h) remplace le coin par
			///    un arc échantillonné. Le peintre n'a pas de primitive « polygone
			///    à coins ronds » — l'arc est donc dans la LISTE DE POINTS, ce qui
			///    revient au même à l'écran et n'invente aucune primitive.
			///
			/// 0 = coin vif. N'est écrit dans le fichier que s'il est non nul —
			/// même discipline additive que `position`, `shape` et les trois
			/// listes : un document d'avant se réenregistre OCTET POUR OCTET.
			float32 rayon = 0.f;

			// ── LES DEUX POIGNÉES DE COURBE (retour de Rodolf, 01/09 nuit) ───
			/// *« pour l'arrondi on doit avoir le manipulateur de courbe, avec la
			/// possibilité d'avoir le manipulateur de chaque côté indépendant ou
			/// dépendant en fonction de l'utilisateur. »*
			///
			/// Deux vecteurs de contrôle, **relatifs au sommet** et dans la MÊME
			/// unité que `x`/`y` (fraction de la boîte, −1..1). L'unité n'est pas
			/// un détail : en unitaire, redimensionner la forme **emmène ses
			/// courbes** ; en pixels, une forme étirée verrait ses tangentes
			/// rester courtes et ses courbes s'aplatir toutes seules.
			///
			/// `entrant` est la tangente du côté du sommet PRÉCÉDENT, `sortant`
			/// celle du côté du SUIVANT — le segment `i → i+1` est donc la
			/// cubique `(P_i, P_i + sortant_i, P_{i+1} + entrant_{i+1}, P_{i+1})`.
			float32 ex = 0.f, ey = 0.f; ///< tangente ENTRANTE (vers le précédent)
			float32 sx = 0.f, sy = 0.f; ///< tangente SORTANTE (vers le suivant)

			/// Comment les deux poignées sont LIÉES. C'est le choix que Rodolf
			/// demande (« indépendant ou dépendant en fonction de l'utilisateur »).
			///
			/// ⚠️ QUATRE VALEURS, ET CE SONT LES QUATRE QUE LA DOCUMENTATION LUNACY
			///    NOMME — pas six, et pas des noms de mon invention. La page
			///    `lunacy.docs.icons8.com/tools/#types-of-points` dit, mot pour
			///    mot : *« Points can be either straight or curved. Curved points
			///    […] have three subtypes: **Mirrored** points come with identical
			///    handles that mirror each other […]. **Disconnected** points have
			///    totally independent handles. **Asymmetric** points come with
			///    handles that share the same angle but can have different
			///    lengths. »* Les captures officielles du panneau `Edit shape` de
			///    cette page montrent **quatre** icônes.
			///
			/// ⚠️ ET LE PIÈGE DE VOCABULAIRE EST RÉEL : chez Lunacy,
			///    **« Asymmetric » = MÊME ANGLE, longueurs différentes** — ce que
			///    Sketch appelle aujourd'hui « Mirror angle ». Prendre le mot dans
			///    son sens courant (« les deux font ce qu'elles veulent ») aurait
			///    donné à `Asymetrique` le comportement de `Deconnecte`, et deux
			///    boutons sur quatre auraient fait la même chose.
			///
			/// 📌 CE QUI EXISTE AILLEURS ET N'EST PAS LIVRÉ, NOMMÉ PLUTÔT QUE TU LE
			///    DÉCOUVRES : la spécification du FORMAT de fichier Lunacy
			///    (`free-format`, enum `CurveMode`) liste deux modes de plus,
			///    `OnlyFrom` et `OnlyTo` — des points à UNE SEULE poignée, ce qui
			///    explique probablement la rangée de six icônes. **Aucune page de
			///    documentation d'interface ne les décrit.** On ne les ébauche donc
			///    pas : implémenter un comportement dont on n'a que le nom, c'est
			///    inventer la moitié qui compte.
			enum : nkentseu::uint8 {
				LiaisonDroit = 0,	   ///< « Straight » : aucune poignée, coin vif (défaut)
				LiaisonMiroir = 1,	   ///< « Mirrored » : liées en DIRECTION **et** LONGUEUR
				LiaisonAsymetrique = 2, ///< « Asymmetric » : MÊME ANGLE, longueurs libres
				LiaisonDeconnecte = 3  ///< « Disconnected » : totalement indépendantes
			};
			nkentseu::uint8 liaison = LiaisonDroit;

			/// LE TYPE PAR DÉFAUT QUAND UN SOMMET NAÎT COURBE — **Miroir**.
			///
			/// Décision de Rodolf, 02/09 : *« par défaut je veux Miroir. »* Et
			/// elle **coïncide avec la source**, ce qui est la meilleure raison de
			/// la prendre : `editing_shapes` dit, mot pour mot, *« hover the
			/// cursor over the path, then click it to place a straight point or
			/// double-click to place a **mirrored** point »*. Le seul geste de
			/// Lunacy qui fabrique un point courbe d'emblée fabrique un point
			/// **miroir**.
			///
			/// ⚠️ ÉCRIT UNE FOIS, ICI, ET PAS RÉPÉTÉ AUX SITES D'APPEL. Trois
			///    endroits doivent choisir un type sans que la main le nomme (le
			///    double-clic sur le tracé, la mise en scène `--courber`, et
			///    demain la plume). Trois `LiaisonMiroir` écrits à la main, ce
			///    sont trois endroits à retrouver le jour où Rodolf change d'avis
			///    — et c'est le genre de valeur qu'on découvre incohérente six
			///    mois plus tard, sur un seul des trois.
			///
			/// ⚠️ CE QU'IL NE CHANGE PAS : un sommet **ajouté au simple clic**
			///    reste **droit**, et ce n'est pas une exception oubliée — c'est
			///    la même phrase de la source (*« click it to place a straight
			///    point »*). Le défaut ne vaut que pour les sommets qui naissent
			///    COURBES, pas pour tous les sommets neufs.
			static constexpr nkentseu::uint8 LiaisonParDefaut = LiaisonMiroir;

			/// Ce sommet porte-t-il une courbe ?
			bool Courbe() const {
				return liaison != LiaisonDroit && (ex != 0.f || ey != 0.f || sx != 0.f || sy != 0.f);
			}

			/// ⚠️ `rayon` ET LES TANGENTES NE SONT PAS DEUX VÉRITÉS POUR LA MÊME
			///    CHOSE, ET LA RÈGLE EST ÉCRITE ICI PLUTÔT QUE SUBIE AU DESSIN :
			///    **`rayon` n'a de sens que sur un sommet `LiaisonDroit`.** Dès
			///    qu'un sommet porte des tangentes, son `rayon` est ignoré — par
			///    le peintre comme par le geste — et l'arrondi au double-clic le
			///    REFUSE au lieu d'écraser en silence la courbe que la main vient
			///    de tirer.
			///
			///    L'autre voie était tentante : faire de `rayon` une simple paire
			///    de tangentes symétriques, donc une seule représentation. Elle est
			///    **écartée pour une raison mesurable** : elle réécrirait les
			///    documents existants (un `rayon = 12` deviendrait quatre nombres
			///    de tangente), et le round-trip octet pour octet des fichiers
			///    d'avant tomberait. *On ne paie pas la conservation d'hier pour
			///    l'élégance d'aujourd'hui.* Une seule vérité PAR SOMMET, choisie
			///    par sa liaison : c'est ce que la règle ci-dessus garantit.
			bool RayonActif() const {
				return liaison == LiaisonDroit && rayon > 0.f;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE NOEUD
	// ═══════════════════════════════════════════════════════════════════════════

	/// Une echelle nulle ferait une matrice non inversible (le pointage
	/// mourrait) : on borne loin de zero, signe conserve, et on plafonne.
	inline bool StrStartsWith(const char *s, const char *prefixe) {
		if (!s || !prefixe)
			return false;
		while (*prefixe) {
			if (*s != *prefixe)
				return false;
			++s;
			++prefixe;
		}
		return true;
	}
	inline float32 NkEchelleSaine(float32 v) {
		if (v != v)
			return 1.f;
		const float32 signe = v < 0.f ? -1.f : 1.f;
		float32 m = v < 0.f ? -v : v;
		if (m < 0.001f)
			m = 0.001f;
		if (m > 1000.f)
			m = 1000.f;
		return signe * m;
	}

	/// DEUX LISTES DE REMPLISSAGES SONT-ELLES LA MEME CHOSE ? Couleur, opacite,
	/// oeil, et le degrade entier (type, angle, arrets avec leur opacite). Sert
	/// a la propagation vers les instances : comparer les TAILLES laissait
	/// passer un changement de couleur, c'est-a-dire le cas courant.
	inline bool NkMemesRemplissages(const NkVector<NkRemplissage> &a,
									const NkVector<NkRemplissage> &b) {
		if (a.Size() != b.Size())
			return false;
		for (uint32 i = 0; i < (uint32)a.Size(); ++i) {
			const NkRemplissage &x = a[i], &y = b[i];
			if (!NkComponentDecl::StrEq(x.couleur.Data(), y.couleur.Data())
				|| x.opacite != y.opacite || x.visible != y.visible
				|| !NkComponentDecl::StrEq(x.genre.Data(), y.genre.Data())
				|| !NkComponentDecl::StrEq(x.image.Data(), y.image.Data())
				|| !NkComponentDecl::StrEq(x.cadrage.Data(), y.cadrage.Data())
				|| x.rotationImage != y.rotationImage
				|| !NkComponentDecl::StrEq(x.fusion.Data(), y.fusion.Data()))
				return false;
			if (!NkComponentDecl::StrEq(x.degrade.type.Data(), y.degrade.type.Data())
				|| x.degrade.angle != y.degrade.angle
				|| x.degrade.origineX != y.degrade.origineX || x.degrade.origineY != y.degrade.origineY
				|| x.degrade.rayonX != y.degrade.rayonX || x.degrade.rayonY != y.degrade.rayonY
				|| x.degrade.arrets.Size() != y.degrade.arrets.Size())
				return false;
			for (uint32 k = 0; k < (uint32)x.degrade.arrets.Size(); ++k) {
				const NkArretDegrade &p = x.degrade.arrets[k], &q = y.degrade.arrets[k];
				if (p.position != q.position || p.opacite != q.opacite
					|| !NkComponentDecl::StrEq(p.couleur.Data(), q.couleur.Data()))
					return false;
			}
		}
		return true;
	}
	struct NkUINode;
	/// FEUILLE ou GROUPE : une planche (`frame`) est toujours un groupe ; un
	/// genre declare fait un groupe (meme vide) ; sinon, des enfants font un
	/// groupe. Tout le reste est une feuille -- et une feuille ne contient
	/// rien, en aucun cas. UN predicat, lu par la pose, le depot, la creation,
	/// le reparentage et la hierarchie.
	inline bool NkEstGroupe(const NkUINode &n);
	/// Une forme OUVERTE (ligne) : ses extremites se voient ; une forme fermee n'en a pas.
	inline bool NkFormeOuverte(const NkUINode &n);
	inline const char *NkRefusFeuille() {
		return "Une feuille ne contient rien, en aucun cas — déposez avant ou après elle, "
			   "ou dans un groupe.";
	}

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
			/// ── LA LISTE DE REMPLISSAGES (Lunacy « FILLS », 01/09) ───────────
			/// ⚠️ ADDITIVE, ET LA CLÉ SIMPLE RESTE L'AUTORITÉ QUAND LA LISTE EST
			///    VIDE. `fond` n'est pas déprécié : il EST « la liste à un
			///    élément », opaque et visible. Tant que personne n'ouvre la
			///    liste, un document d'avant ne gagne pas une clé — il se
			///    réenregistre OCTET POUR OCTET, et c'est la PREUVE D'ENTRÉE de
			///    ce chantier, pas sa vérification de sortie.
			///
			/// ⚠️ ET LA FORME ÉCRITE EST DÉCIDÉE PAR LA PRÉSENCE DE LA LISTE,
			///    JAMAIS PAR SON CONTENU. Un document qui porte `fond_1` se
			///    réécrit en `fond_1`, même si son unique remplissage est opaque
			///    et visible. On pourrait le « rétrograder » en `fond` pour faire
			///    plus court : ce serait un aller-retour qui CHANGE LES OCTETS.
			///    Le round-trip passe avant la concision.
			///
			/// L'ORDRE EST CELUI DE LUNACY : le DERNIER se peint PAR-DESSUS.
			NkVector<NkRemplissage> fills;
			/// LES SURCHARGES D'APPARENCE PAR ETAT (§9, chantier composants).
			/// Vide = le nœud n'a qu'un etat Normal, et son fichier ne gagne
			/// AUCUNE cle : un document d'avant se reenregistre octet pour
			/// octet. Cle additive `apparence_<Etat>`.
			NkVector<NkApparenceEtat> apparences;
			/// ── LA LISTE DE BORDURES (Lunacy « BORDERS ») ────────────────
			/// EXACTEMENT la meme discipline que `fills`, et ce n'est pas une
			/// coincidence : c'est la regle qui a marche, donc on la reprend sans
			/// l'amenager. `couleur_bord` + `bordure` sont « la liste a un
			/// element » ; la forme ecrite est decidee par la PRESENCE de la
			/// liste, jamais par son contenu.
			NkVector<NkBordure> borders;
			/// ── LA LISTE D'EFFETS (Lunacy « EFFECTS ») ───────────────────────
			/// Troisième et dernier étage du mandat listes, MÊME discipline.
			/// ⚠️ ET ELLE N'A PAS DE CLÉ SIMPLE DONT ELLE SERAIT « la liste à un
			///    élément » : le modèle n'a JAMAIS porté d'ombre. La règle
			///    d'additivité tient quand même, et plus simplement : la clé
			///    `effet_<i>` n'existe que si la liste existe, donc tout document
			///    d'avant se réenregistre octet pour octet — il n'a rien à
			///    convertir. Pas de `MaterialiserEffets` : il n'y a rien à
			///    préserver.
			NkVector<NkEffet> effets;
			/// ── LES SOMMETS DÉPLACÉS (mode points, §8bis restreint) ──────────
			/// VIDE = le polygone RÉGULIER de la table (`Sommets.h`) ; non vide =
			/// les sommets que la main a bougés. Même discipline additive que les
			/// trois listes : la clé `sommet_<i>` n'existe que si la liste existe,
			/// donc un document d'avant se réenregistre octet pour octet.
			/// ⚠️ NE VAUT QUE POUR LES POLYGONES. Une ligne exprime ses bouts par
			///    sa boîte, un rectangle par sa taille : leur donner une liste de
			///    sommets créerait deux façons de dire la même chose, et le
			///    lecteur devrait choisir.
			NkVector<NkPoint2> sommets;
			NkString alignText;	 ///< `centre` | `droite` (clé `texte_aligne`) — vide = gauche
			/// La CIBLE D'APPAREIL d'un artboard (écran 26 « Menu Cible ») :
			/// texte libre « Mobile 390 x 844 » — l'étiquette de la toile devient
			/// « <nom> — <cible> ». Vide = l'étiquette historique (nom — L × H).
			NkString target; ///< clé `cible`
			/// L'UNITÉ de la cible — clé `unite`, RÉSERVÉE le 2026-09-02 sur
			/// mandat VR/AR/XR (ROADMAP_PRODUITS.md §5 au parent) : une CIBLE
			/// portera un jour une projection en mètres et en degrés (« panneau
			/// à 2 m, 60° ») — jamais la page.
			/// ⚠️ ABSENTE = PIXELS, et c'est tout ce qu'elle fait aujourd'hui :
			///    ni exploitée, ni affichée — c'est la PLACE qui compte (même
			///    logique que `deriveDe` pour la filiation). Son absence serait
			///    le seul irréversible. Clé additive : un document d'avant se
			///    relit octet pour octet.
			/// 🔴 ET LA RÈGLE QUI VA AVEC : aucune hypothèse « écran plat » ne se
			///    grave dans les propriétés d'une PAGE. L'entrée (rayon, regard,
			///    mains) ne va nulle part dans le document — une propriété
			///    « rayon » serait le `si (mobile)` de la VR.
			NkString targetUnit; ///< clé `unite`
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
			/// ── LE FOND QUI SE PEINT — LE SEUL POINT DE VÉRITÉ DES PEINTRES ──
			/// ⚠️ SIX SITES DE `Renderers.h` LISAIENT `fill` DIRECTEMENT. S'ils
			///    avaient continué, la liste serait un champ que le fichier porte
			///    et que l'écran ignore : le paramètre déclaré non honoré, encore.
			/// Rend nullptr quand rien n'est posé — le thème du document prime
			/// alors, exactement comme avant la liste.
			/// ⚠️ UNE LISTE DONT TOUT EST MASQUÉ REND nullptr, ELLE NE RETOMBE PAS
			///    SUR `fond` : masquer le dernier œil doit se VOIR. Retomber sur
			///    la clé simple rendrait l'œil sans effet sur un nœud matérialisé.
			const char *FondEffectif() const {
				for (uint32 i = (uint32)fills.Size(); i > 0; --i) {
					const NkRemplissage &f = fills[i - 1];
					if (f.visible && !f.couleur.Empty())
						return f.couleur.Data();
				}
				if (!fills.Empty())
					return nullptr;
				return fill.Empty() ? nullptr : fill.Data();
			}
			/// LES BORDURES QUI SE PEIGNENT -- LA PORTE UNIQUE (11/09, Q143 -> Q145).
			///
			/// 🔴 AVANT ELLE, QUATRE LECTEURS DECIDAIENT CHACUN : les deux boucles du
			///    rectangle (anneau d'un trace edite, cadre), la ligne, et l'export SVG
			///    -- trois recopiaient la cle historique `borderColor` en une bordure,
			///    la ligne l'ignorait. Et `BordureEffective()` (le DERNIER visible)
			///    existait ici SANS AUCUN APPELANT : une porte declaree que personne
			///    n'empruntait. Un etat de bordure branche sur l'un des quatre aurait
			///    fui sur les trois autres. La voici, et les quatre passent par elle.
			///
			/// LE CONTRAT : TOUTES les bordures qui se peignent, dans l'ordre de la
			/// liste (le dernier se peint par-dessus) ; une bordure masquee, sans
			/// couleur ou d'epaisseur nulle n'y est pas ; une liste toute masquee ne
			/// rend RIEN (masquer le dernier oeil doit se voir) ; SANS liste, la cle
			/// simple `borderColor` devient UNE bordure interieure d'epaisseur
			/// `borderW` (ou 1) -- le geste historique, ecrit une fois.
			/// ⚠️ DES POINTEURS, PAS DES COPIES : le peintre passe ici a chaque image.
			///    `legacy` est le logement de la bordure synthetisee, fourni par
			///    l'appelant -- la porte ne possede rien.
			/// Rend le nombre ecrit dans `out` (au plus `cap`).
			static constexpr uint32 kMaxBorduresPeintes = 16u;
			uint32 BorduresEffectives(const NkBordure **out, uint32 cap, NkBordure &legacy) const {
				uint32 n = 0u;
				if (!borders.Empty()) {
					for (uint32 i = 0; i < (uint32)borders.Size(); ++i) {
						const NkBordure &b = borders[i];
						if (!b.visible || b.couleur.Empty() || b.epaisseur <= 0.f)
							continue;
						if (n < cap)
							out[n] = &b;
						++n;
					}
					return n > cap ? cap : n;
				}
				if (borderColor.Empty() || cap == 0u)
					return 0u;
				legacy = NkBordure();
				legacy.couleur = borderColor;
				legacy.epaisseur = borderW > 0.f ? borderW : 1.f;
				legacy.position = NkBordurePos::Interieur;
				out[0] = &legacy;
				return 1u;
			}
			/// MATERIALISER la liste depuis les cles simples. Meme regle que pour
			/// les remplissages, y compris le « il ne vide pas la cle simple ».
			void MaterialiserBorders() {
				if (!borders.Empty())
					return;
				NkBordure b;
				b.couleur = borderColor.Empty() ? NkString("#000000") : borderColor;
				b.epaisseur = borderW > 0.f ? borderW : 1.f;
				borders.PushBack(b);
			}
			/// L'opacité (0..100) du remplissage que rend `FondEffectif`.
			float32 FondOpacite() const {
				for (uint32 i = (uint32)fills.Size(); i > 0; --i) {
					const NkRemplissage &f = fills[i - 1];
					if (f.visible && !f.couleur.Empty())
						return f.opacite;
				}
				return 100.f;
			}
			/// MATÉRIALISER la liste depuis la clé simple — le geste qui fait
			/// basculer le nœud dans la forme « liste ». Appelé par le premier
			/// geste de l'Inspecteur qui a besoin de ce que `fond` ne sait pas
			/// dire : une opacité, un œil, un second remplissage. Idempotent.
			/// ⚠️ IL NE VIDE PAS `fill` : le champ reste ce qu'il était, il cesse
			///    seulement d'être lu. Le vider ferait perdre la valeur d'origine
			///    si l'on revenait en arrière, et n'apporterait rien.
			void MaterialiserFills() {
				if (!fills.Empty())
					return;
				NkRemplissage f;
				f.couleur = fill.Empty() ? NkString("#ffffff") : fill;
				fills.PushBack(f);
			}
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
			/// ── L'ARRONDI PAR COIN (Rodolf, 02/09 : *« pourquoi je n'ai pas
			///    des arrondis par coin comme sur Lunacy ? »*) ────────────────
			/// L'ordre est celui du sens horaire depuis le haut-gauche :
			/// **haut-gauche, haut-droit, bas-droit, bas-gauche** — celui de CSS
			/// et de Lunacy. Un autre ordre aurait été un piège permanent.
			///
			/// 🔑 `rayonsDelies` EST L'ÉTAT DU LIEN, pas une redondance : sans
			///    lui, « quatre coins à 8 » et « un rayon de 8 » seraient
			///    indiscernables, et le panneau ne saurait pas s'il doit afficher
			///    un champ ou quatre. C'est aussi ce qui garde la clé simple
			///    quand elle suffit — *la clé simple EST la liste à un élément*,
			///    la règle que le format tient déjà pour les remplissages.
			///
			/// ⚠️ NE PAS CONFONDRE AVEC LE RAYON PAR SOMMET (`NkPoint2::rayon`) :
			///    celui-ci arrondit les COINS D'UNE BOÎTE, celui-là arrondit un
			///    SOMMET DE TRACÉ en édition vectorielle. Deux notions voisines,
			///    deux champs — la même discipline que `component` contre
			///    `instanceDe`.
			bool rayonsDelies = false;
			float32 rayonsCoins[4] = {0.f, 0.f, 0.f, 0.f};

			/// LE RAYON EFFECTIF d'un coin — la SEULE lecture autorisée.
			/// ⚠️ Tout site qui lirait `radius` directement afficherait un coin
			///    faux dès que les rayons sont déliés. Une porte, pas quatre
			///    lectures dispersées.
			float32 RayonCoin(nkentseu::uint32 i) const {
				if (!rayonsDelies)
					return radius;
				return rayonsCoins[i < 4u ? i : 0u];
			}

			// ⚠️ `RayonsUniformes()` A ETE RETIRE LE 07/09, ET SON ABSENCE EST LA
			//    MOITIE DU CORRECTIF. Son commentaire disait « donc si la clé simple
			//    suffit à les écrire » : c'était faux, et c'est ce mot — *suffit* —
			//    qui a coûté l'arrondi par coin (cas 50c). Le prédicat n'avait qu'un
			//    seul appelant, la condition de sérialisation, et il n'y encodait
			//    qu'une règle fausse.
			//    *Un prédicat dont l'unique métier était une règle fausse ne doit pas
			//    survivre à la règle* : laissé là, il aurait été réutilisé de bonne
			//    foi par la personne suivante, et le piège se serait reconstruit
			//    ailleurs. Le mode (`rayonsDelies`) décide seul, désormais.
			/// ── LA ROTATION ET LES DEUX MIROIRS (Lunacy, bandeau du haut) ────
			/// Retour de Rodolf, 01/09 : *« dans propriétés il n'y a pas miroir,
			/// rotation etc., ni autour de l'objet sélectionné. »* Les trois
			/// champs qui manquaient à l'inventaire des 9 manques de Q42.
			///
			/// ⚠️ POURQUOI LES TROIS ENSEMBLE, ET PAS LA ROTATION SEULE. Ce sont
			///    les trois faces d'une même chose : une **transformation autour
			///    du centre de la boîte**. Elles se composent (`NkTransfoDe`),
			///    elles se propagent aux enfants ensemble, et le picking les
			///    inverse ensemble. Livrées séparément, chacune aurait eu son
			///    propre inverseur de picking — trois occasions de diverger sur
			///    la question la plus délicate du lot.
			///
			/// ⚠️ ET LE MIROIR N'EST PAS « UN SIGNE SUR LA TAILLE ». Passer par
			///    une largeur négative aurait contamine la disposition, les
			///    contraintes min/max et l'englobant de sélection, qui supposent
			///    tous des tailles positives. Le miroir est une propriété du
			///    DESSIN et du POINTAGE, pas de la boîte.
			///
			/// `rotation` en DEGRÉS, sens horaire (celui de l'écran, où l'axe Y
			/// descend — c'est la convention de Lunacy et celle du champ « ° »).
			/// Tous trois additifs : absents du fichier tant qu'ils valent leur
			/// défaut, donc un document d'avant se réenregistre octet pour octet.
			float32 rotation = 0.f;	 ///< degrés horaires (clé `rotation`)
			bool miroirH = false;	 ///< retourné gauche/droite (clé `miroir_h`)
			bool miroirV = false;	 ///< retourné haut/bas (clé `miroir_v`)
			/// L'ECHELLE, PORTEE PAR LE NOEUD (decision de Rodolf : le texte subit
			/// la mise a l'echelle). Composee en descendant avec les ancetres
			/// (NkMatEffective), lue par toutes les tailles a travers le peintre.
			/// Additive comme les miroirs : absente du fichier tant qu'elle vaut 1.
			float32 echelleX = 1.f; ///< cle `echelle_x`
			float32 echelleY = 1.f; ///< cle `echelle_y`
			/// ── L'INCLINAISON, EN DEGRES (cles `inclinaison_x`, `inclinaison_y`) ──
			/// Deux angles : l'inclinaison AUTOUR de l'axe X, et autour de l'axe Y.
			/// ⚠️ AFFINE, ET LA LIMITE EST ECRITE PLUTOT QUE COMPENSEE : le sommet du
			///    dessinateur est `pos, uv, col` -- **sans composante de profondeur**.
			///    L'interpolation reste donc affine : une texture ou un degrade sur un
			///    quadrilatere fortement incline SE PLIERA le long de la diagonale. Aux
			///    angles moderes sur un aplat, invisible. La vraie perspective exige un
			///    `w` au sommet ET dans les cinq dorsaux -- chantier de socle, pas ici.
			float32 inclinaisonX = 0.f;
			float32 inclinaisonY = 0.f;

			// ── VERROUILLER / MASQUER (vague 2, source `/layers`) ────────────
			/// ⚠️ DEUX BOOLÉENS, DEUX EFFETS DIFFÉRENTS, ET LA DIFFÉRENCE EST TOUT
			///    LE SUJET :
			///      - `masque` retire le nœud **du dessin ET du pointage** ;
			///      - `verrouille` le laisse **visible** et le retire **du seul
			///        pointage**.
			///    Les confondre donnerait soit un objet verrouillé invisible, soit
			///    un objet masqué qu'on attrape encore.
			///
			/// 🔴 *Un nœud masqué qu'on peut encore attraper est pire que pas de
			///    masquage du tout* : on croit l'objet parti, on clique « dans le
			///    vide », et on déplace ce qu'on ne voit pas. Les deux drapeaux se
			///    lisent donc par `NkNoeudVisible` / `NkNoeudAttrapable`
			///    (`Selection.h`) et **jamais directement** — ces portes remontent
			///    aussi les ANCÊTRES.
			///
			/// ⚠️ ILS S'HÉRITENT, ET CE N'EST PAS UN CHOIX DE STYLE. Masquer un
			///    groupe doit masquer son contenu, sinon on obtient un groupe
			///    « invisible » dont les enfants continuent de se peindre. Même
			///    chose pour le verrou. L'héritage vit dans les deux portes, pas
			///    dans une recopie du drapeau sur chaque descendant : *un drapeau
			///    recopié est un drapeau qui dérive au premier nœud déplacé.*
			///
			/// Additifs comme `miroir_h` : absents du fichier tant qu'ils valent
			/// leur défaut, donc un document d'avant se réenregistre **octet pour
			/// octet**.
			bool verrouille = false; ///< non attrapable, mais toujours peint (clé `verrouille`)
			bool masque = false;	 ///< ni peint ni attrapable (clé `masque`)
			// ── LE REFUS PAR AXE (decision de Rodolf) ───────────────────────
			/// Un enfant peut refuser un axe de transformation : il ne le subit
			/// ni de ses ancetres ni de lui-meme. Lu par NkMatEffective (une
			/// seule matrice pour le dessin et le pointage), montre dans la
			/// hierarchie et dans l'inspecteur. Additifs, comme `verrouille`.
			bool refusPosition = false; ///< cle `refus_position`
			bool refusRotation = false; ///< cle `refus_rotation`
			bool refusEchelle = false;  ///< cle `refus_echelle`
			// ── LA NATURE : FEUILLE ou GROUPE (Rodolf, 03/09) ───────────────
			/// « Chaque rectangle est un graphique et ne peut en aucun cas
			/// contenir d'autres graphiques ou groupes. » Deux familles : les
			/// FEUILLES (graphiques, images, texte) et les GROUPES (contiennent
			/// groupes et feuilles). Le GENRE du groupe est libre : `simple`,
			/// `booleen` (une operation booleenne CREE un groupe -- chapitre 3),
			/// `composant`, d'autres plus tard ; un genre inconnu se relit et se
			/// reemet intact. Cle `groupe = <genre>`, additive : absente =
			/// inferee (voir NkEstGroupe) ; ecrite des qu'un groupe est cree,
			/// pour qu'un groupe VIDE reste un groupe.
			NkString genre; ///< vide = pas declare (infere) ; cle `groupe`

			// ── CE NŒUD EST-IL UNE INSTANCE ? (composants de document, 02/09) ──
			/// La **clé** de la déclaration dont ce nœud est une instance
			/// (`auteur/nom@version`). Vide = nœud ordinaire.
			/// ⚠️ CE N'EST PAS `component`, et la distinction est écrite au long
			///    au-dessus de `NkDeclarationComposant` : `component` désigne un
			///    composant **de code** (C++, statique, absent du fichier).
			NkString instanceDe;
			/// LES STYLES LIES (§15.15) : la cle d'un style de calque (remplissages,
			/// bordures, effets) et d'un style de texte (taille, graisse, couleur).
			/// Additifs (`style_calque`, `style_texte`) ; les bits d'ecart sont CEUX des
			/// instances -- une propriete surchargee tient, quel que soit celui qui pousse.
			NkString styleCalque;
			NkString styleTexte;

			/// ── LES ÉCARTS, PROPRIÉTÉ PAR PROPRIÉTÉ ──────────────────────────
			/// Un masque de bits : chaque bit dit « cette propriété est À MOI,
			/// n'hérite plus de la déclaration ».
			///
			/// 🔴 LA RÈGLE DE FOND, ET C'EST ELLE QUI REND LE RESTE ACCEPTABLE :
			///    **une surcharge gagne sur la mise à jour.** Le composant se
			///    propage à ses instances *sauf* là où un bit est posé. Sans ça,
			///    mettre à jour une déclaration écraserait le travail fait sur
			///    chaque instance — et personne n'oserait plus toucher à un
			///    composant.
			///
			/// ⚠️ UN MASQUE PLUTÔT QU'UNE COPIE DES VALEURS : le nœud porte DÉJÀ
			///    tous les champs. Le bit dit seulement lequel fait foi. Une
			///    seconde table de valeurs aurait été une deuxième vérité, et
			///    c'est le motif que ce dépôt passe son temps à retirer.
			///
			/// 📌 CE QUI NE FIGURE PAS ICI EST AUSSI UNE DÉCISION (`15_…` §15.3) :
			///    la **géométrie du tracé** ne se surcharge jamais (c'est
			///    *l'identité* de la forme), et **position / rotation / miroirs**
			///    n'ont pas de bit parce qu'ils ne s'héritent pas du tout — ils
			///    sont propres à l'instance par nature.
			enum : nkentseu::uint32 {
				EcartRemplissages = 1u << 0,
				EcartBordures = 1u << 1,
				EcartEffets = 1u << 2,
				EcartTexte = 1u << 3,  ///< **toujours** légitime : deux boutons ne disent pas la même chose
				EcartApparence = 1u << 4, ///< rayon, opacité, corps de police
				EcartTaille = 1u << 5	  ///< ⚠️ voir §15.7 : la 1re tranche est à taille FIXE
			};
			nkentseu::uint32 ecarts = 0;

			bool ADesEcarts() const {
				return ecarts != 0u;
			}
			bool Surcharge(nkentseu::uint32 bit) const {
				return (ecarts & bit) != 0u;
			}
			float32 borderW = 0.f;	 ///< épaisseur de bord, px (clé `bordure`)
			float32 fontPx = 0.f;	 ///< corps du texte, px (clé `police_px`) — 0 = défaut
			float32 fontWeight = 0.f; ///< graisse 100..900 (clé `graisse`) — 0 = défaut
			/// Le contenu d'un nœud `shape == "text"`. UNE ligne (le format écrit
			/// une clé par ligne, sans échappement — un retour à la ligne dans ce
			/// champ casserait la relecture, et l'éditeur n'en produit pas).
			NkString text;

			/// Les noms de metrique que ce noeud designe. Ils ne portent aucun nombre :
			/// ils se resolvent dans la table du DOCUMENT (`NkUIDocument::MetricSource`).
			/// ⑤ (07/09) L'OPACITÉ DU NŒUD (Lunacy « LAYER »), clé `opacite`, 0..100.
			/// Le panneau CALQUE l'affichait en dur à « 100 », grisée, avec la raison :
			/// *« le modèle ne la porte pas encore »*. Il la porte.
			/// ⚠️ ADDITIVE : rien au fichier tant qu'elle vaut 100, donc un document
			///    d'avant se réenregistre octet pour octet.
			/// ⚠️ ELLE SE TRANSMET AUX DESCENDANTS, mais ce n'est PAS un calque
			///    composité une seule fois : le peintre multiplie l'alpha de chaque
			///    élément. Deux enfants qui se recouvrent se voient donc l'un l'autre à
			///    travers, là où Lunacy composite le groupe puis l'atténue. Le vrai
			///    compositing demande une cible hors écran -- c'est un chantier de
			///    rendu, nommé ici pour qu'on ne le découvre pas à l'usage.
			float32 opacite = 100.f;
			/// ⑥ (07/09) LE MODE DE FUSION DU NŒUD (Lunacy « LAYER »), clé `fusion`.
			/// La clé CSS (`multiply`, `screen`…) ; vide = normal. Texte libre, un mode
			/// inconnu est PRÉSERVÉ et montré tel quel -- même règle que la fusion par
			/// remplissage, dont ce champ est le jumeau à l'échelle du nœud.
			/// ⚠️ ADDITIVE : rien au fichier tant qu'elle est vide.
			/// ⚠️ CINQ MODES SUR DIX-HUIT sont peints exactement (au GPU) ; les treize
			///    autres lisent la DESTINATION et restent « enregistrés, pas peints ».
			///    Les approcher donnerait une image plausible et fausse, qui ne se
			///    découvrirait que sur un document réel -- *un repli qui reste plausible
			///    est pire qu'un refus.*
			/// ⚠️ ET CE N'EST PAS UN CALQUE COMPOSITÉ UNE SEULE FOIS, exactement comme
			///    `opacite` : le nœud se fond avec ce qui est DÉJÀ sur la toile, pas
			///    « le groupe composité PUIS fondu ». Même écart avec Lunacy, même
			///    phrase : qui a compris la limite de l'opacité comprend celle-ci.
			NkString fusion;
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
	// ═══════════════════════════════════════════════════════════════════════════
	//  LES COMPOSANTS **DE DOCUMENT** (chantier du 02/09, modele `15_…`)
	// ═══════════════════════════════════════════════════════════════════════════
	//  🔴 DEUX NOTIONS PORTENT LE MEME MOT, ET LES CONFONDRE COUTERAIT TOUT.
	//     Le champ `NkUINode::component` designe un composant **DE CODE** :
	//     declare en C++, `NkComponentRegistry`, duree de vie STATIQUE, INEXISTANT
	//     dans le fichier. Ce qui suit est un composant **DE DOCUMENT** : dessine
	//     dans l'editeur, il VIT dans le fichier et doit voyager avec lui.
	//
	//     Leur donner le meme champ aurait ete tentant -- un seul nom, une seule
	//     resolution. Ce serait un document qui **cesse d'etre ouvrable des que le
	//     binaire change de version** : sa moitie « composants » pointerait des
	//     declarations C++ qui n'existent plus. *Deux natures voisines, deux
	//     champs*, et c'est vu avant d'ecrire plutot que paye apres.

	/// L'IDENTITE D'UNE DECLARATION, et elle existe pour la REGLE DE FORK.
	///
	/// Decision de Rodolf : un composant **tiers** se **duplique a la
	/// modification** au lieu de se modifier en place. Elle vient de ce que
	/// l'atelier alimente -- *« des millions d'utilisateurs peuvent creer des
	/// composants, les commercialiser ou les partager »*. Modifier en place la
	/// declaration de quelqu'un d'autre casserait l'identite sous laquelle elle a
	/// ete partagee.
	///
	/// ⚠️ ET C'EST POURQUOI L'IDENTITE EST DANS LE MODELE DES MAINTENANT.
	///    `NkProvenance::author` dit le *genre* d'auteur (`Human`, `AI`,
	///    `Imported`), pas **qui**. Ajoutee apres coup, cette identite demanderait
	///    de reecrire tous les documents deja produits pour leur inventer un
	///    auteur qu'ils n'ont pas -- ce qu'on ne peut pas faire. *C'est de la
	///    structure, pas du comportement.*
	struct NkIdentiteComposant {
			NkString auteur;  ///< QUI (pas le genre d'auteur : le nom)
			NkString nom;	  ///< le nom lisible, celui que la palette affiche
			NkString version; ///< libre ; « 1 » par defaut
			/// De quelle identite ce composant DERIVE, quand il est un fork. Vide
			/// pour une creation.
			/// ⚠️ SANS CE LIEN, UN FORK EST INDISCERNABLE D'UNE CREATION, et on
			///    perd la seule information qui permettra un jour de proposer
			///    « l'original a change, veux-tu rejouer ta modification ? ».
			NkString deriveDe;

			/// La cle stable `auteur/nom@version` -- c'est elle que les instances
			/// referencent, et elle survit au partage.
			NkString Cle() const {
				NkString c = auteur.Empty() ? NkString("anonyme") : auteur;
				c.Append("/");
				c.Append(nom.Empty() ? "sans_nom" : nom.Data());
				c.Append("@");
				c.Append(version.Empty() ? "1" : version.Data());
				return c;
			}
	};

	/// UNE DECLARATION : son identite, et son ARBRE.
	/// ⚠️ L'ARBRE EST AUTONOME (son propre vecteur de nœuds, indices internes) et
	///    ce n'est pas un detail d'implementation : une declaration doit pouvoir
	///    **voyager** -- etre partagee, vendue, importee. Un sous-arbre qui vivrait
	///    dans le vecteur du document ne se detacherait jamais proprement.
	// ════════════════════════════════════════════════════════════════════════
	//  LE STYLE : un nom pour UN ENSEMBLE (§15.15, 05/09)
	// ════════════════════════════════════════════════════════════════════════
	/// `genre` : « calque » (remplissages + bordures + effets) ou « texte » (taille,
	/// graisse, couleur) -- texte libre, inconnu preserve. L'ensemble vit dans
	/// `apparence`, un noeud SANS geometrie : c'est ce qui permet au fichier de
	/// relire `fond_i` / `bord_i` / `effet_i` / `police_px` / `graisse` /
	/// `couleur_texte` d'un style AVEC LES LECTEURS DU NOEUD, et de les ecrire avec
	/// ses ecrivains. Pas un second parseur, pas un second ecrivain.
	/// ⚠️ La police (famille) n'est pas dans le modele du noeud : le style de texte
	///    ne la porte pas non plus -- un parametre declare non honore serait pire
	///    qu'absent.
	struct NkStyle {
			NkString cle;
			NkString nom;
			NkString genre;
			NkString inconnus;
			NkUINode apparence;
			bool EstTexte() const {
				return NkComponentDecl::StrEq(genre.Data(), "texte");
			}
	};

	struct NkDeclarationComposant {
			NkIdentiteComposant identite;
			NkVector<NkUINode> arbre; ///< racine = indice 0
	};

	/// LA TABLE DES ECARTS : un bit, un nom lisible.
	///
	/// 🔴 ELLE EXISTE POUR QU'UN BIT NE PUISSE PAS DEVENIR UNE SURCHARGE
	///    INVISIBLE. Le jour ou quelqu'un ajoute un `EcartRotation` a
	///    l'enumeration sans l'ajouter ici, l'utilisateur aurait une propriete
	///    surchargee que l'interface ne sait pas nommer -- donc qu'il ne peut ni
	///    voir ni reinitialiser, et qui expliquerait sans raison visible pourquoi
	///    son instance ne suit plus sa declaration. Meme famille que « une entree
	///    qui n'agit pas porte sa raison » : *ce que le modele porte, l'interface
	///    doit pouvoir le nommer.*
	/// ⚠️ ET UN CAS DE RECETTE EXIGE QUE LA TABLE COUVRE TOUTE L'ENUMERATION.
	///    Sans lui, cette regle ne serait qu'un commentaire.
	struct NkEcartNomme {
			nkentseu::uint32 bit;
			const char *nom;
	};

	inline const NkEcartNomme *NkTousLesEcarts(nkentseu::uint32 &nb) {
		static const NkEcartNomme kTable[] = {
			{NkUINode::EcartRemplissages, "Remplissages"},
			{NkUINode::EcartBordures, "Bordures"},
			{NkUINode::EcartEffets, "Effets"},
			{NkUINode::EcartTexte, "Texte"},
			{NkUINode::EcartApparence, "Apparence"},
			{NkUINode::EcartTaille, "Taille"},
		};
		nb = (nkentseu::uint32)(sizeof(kTable) / sizeof(kTable[0]));
		return kTable;
	}

	/// LA PORTE DE LA REGLE DE FORK — consultee **avant toute ecriture** sur une
	/// declaration, jamais dispersee aux sites d'appel (meme discipline que
	/// `NkNoeudAttrapable`).
	/// @return vrai si `auteurCourant` peut modifier CETTE declaration en place ;
	///         faux s'il doit la **forker**.
	inline bool NkPeutModifierDeclaration(const NkIdentiteComposant &id,
										  const char *auteurCourant) {
		const char *a = id.auteur.Data();
		// une declaration sans auteur est locale au document : elle nous appartient.
		if (!a || !a[0])
			return true;
		if (!auteurCourant || !auteurCourant[0])
			return false; // on ne sait pas qui on est : on ne touche pas au bien d'autrui
		return NkComponentDecl::StrEq(a, auteurCourant);
	}

	/// L'ORIGINE d'un composant — les quatre vues de la palette (Rodolf, 02/09 :
	/// *« lister tous les composants, ou seulement les composants systeme, ou
	/// externes, ou nos propres composants »*).
	///
	/// 🔑 CE N'EST PAS UNE NOUVELLE NOTION : c'est la frontiere de Q51 (la
	///    PROPRIETE) rendue visible. Un filtre qui inventerait son propre critere
	///    divergerait de la regle de fork au premier composant partage.
	///
	/// ⚠️ MAIS LE PREDICAT SEUL NE SUFFIT PAS, ET C'EST UN FAIT DE MODELE :
	///    `NkPeutModifierDeclaration` repond « a moi / pas a moi ». Or *systeme*
	///    et *tiers* sont TOUS DEUX « pas a moi » — il les confond. La
	///    distinction ne se devine donc pas du predicat : elle vient de la
	///    SOURCE de la declaration. Le kit vit dans `NkComponentRegistry` (des
	///    composants de CODE), le tiers dans `doc.declarations` avec un auteur
	///    qui n'est pas le notre. *Le filtre traverse deux listes, et c'est le
	///    modele qui le dit, pas le panneau.*
	enum class NkOrigineComposant {
		Systeme, ///< le kit — composants de CODE, jamais modifiables (copie seule)
		Tiers,	 ///< declare par quelqu'un d'autre : copie seule aussi
		Mien	 ///< cree par moi, ou fork que j'ai fait — modifiable en place
	};

	/// L'origine d'une declaration de DOCUMENT (le systeme ne passe pas ici : il
	/// n'a pas de declaration, il a une entree de registre).
	/// ⚠️ ELLE S'APPUIE SUR LE MEME PREDICAT que la regle de fork — une seule
	///    ecriture du critere, jamais deux qui se repondraient differemment.
	inline NkOrigineComposant NkOrigineDe(const NkIdentiteComposant &id,
										  const char *auteurCourant) {
		return NkPeutModifierDeclaration(id, auteurCourant) ? NkOrigineComposant::Mien
															: NkOrigineComposant::Tiers;
	}

	/// Le nom lisible d'une origine — pour le filtre ET pour la pilule.
	inline const char *NkNomOrigine(NkOrigineComposant o) {
		switch (o) {
			case NkOrigineComposant::Systeme: return "système";
			case NkOrigineComposant::Tiers: return "externe";
			default: return "à moi";
		}
	}


	class NkUIDocument {
		public:
			NkString title = NkString("Interface sans titre");
			NkProvenance prov; ///< la provenance du DOCUMENT (qui l'a cree)
			NkVector<NkUINode> nodes;
			/// LES DECLARATIONS DE COMPOSANTS **DE DOCUMENT** portees par ce
			/// fichier. Vide = aucun composant, et le fichier ne gagne aucune cle :
			/// un document d'avant se reenregistre OCTET POUR OCTET.
			/// 📌 Elles vivent DANS le document qui les a creees, et rien d'autre
			///    pour l'instant (`12_…` §12.3(a)) : la bibliotheque partagee est un
			///    chantier de RESOLUTION DE DEPENDANCES, pas d'editeur. Ca ne la
			///    ferme pas -- une declaration locale se **promeut** plus tard.
			NkVector<NkDeclarationComposant> declarations;
			/// LES LANGUES SUPPLEMENTAIRES declarees par le document (mandat
			/// multilingue 01/09) : la principale n'a pas de code (c'est `texte`).
			/// Vide = document monolingue — il se reenregistre octet pour octet.
			NkVector<NkString> langues;
			/// LES VARIABLES du document, et son mode courant (vide = le defaut).
			NkVector<NkVariable> variables;
			NkString modeCourant;

			/// ③ (05/09) LES DOSSIERS RECEMMENT OUVERTS DEPUIS CE DOCUMENT. Ils sont
			/// ecrits dans le `.nkuidoc` et voyagent avec lui — contrairement aux recents
			/// de la SESSION, qui vivent dans l'application et meurent avec elle.
			/// Le plus recent EN TETE.
			nkentseu::NkVector<nkentseu::NkString> dossiersRecents;
			const NkVariable *TrouverVariable(const char *cle) const {
				if (!cle)
					return nullptr;
				if (*cle == '@')
					++cle;
				for (uint32 i = 0; i < (uint32)variables.Size(); ++i)
					if (NkComponentDecl::StrEq(variables[i].cle.Data(), cle))
						return &variables[i];
				return nullptr;
			}
			/// RESOUDRE une couleur : un litteral revient tel quel ; une reference
			/// donne la valeur de la variable dans le mode courant ; une reference
			/// vers une variable ABSENTE rend nullptr -- l'appelant le DIT.
			const char *ResoudreCouleur(const char *c) const {
				if (!NkEstReference(c))
					return c;
				const NkVariable *v = TrouverVariable(c);
				return v ? v->ValeurPour(modeCourant.Data()) : nullptr;
			}
			NkVariable *TrouverVariableMut(const char *cle) {
				return const_cast<NkVariable *>(TrouverVariable(cle));
			}
			// ── LES STYLES (§15.15, 05/09) : le MEME mecanisme que les instances ──
			// Un style se PROPAGE par copie a la modification, sous les bits d'ecart
			// des instances (`NkTousLesEcarts`) ; un ecart se DETECTE a l'edition
			// humaine (MarkHumanEdit) -- une porte, pas seize sites. L'EMPREINTE d'un
			// ensemble est ce que le fichier ecrirait : une seule verite pour comparer.
			NkVector<NkStyle> styles;
			const NkStyle *TrouverStyle(const char *cle) const {
				if (!cle)
					return nullptr;
				if (*cle == '@')
					++cle;
				for (uint32 i = 0; i < (uint32)styles.Size(); ++i)
					if (NkComponentDecl::StrEq(styles[i].cle.Data(), cle))
						return &styles[i];
				return nullptr;
			}
			NkStyle *TrouverStyleMut(const char *cle) {
				return const_cast<NkStyle *>(TrouverStyle(cle));
			}
			/// Les bits d'ecart que couvre un genre de style.
			static uint32 BitsDuGenre(bool texte) {
				return texte ? NkUINode::EcartTexte
							 : (NkUINode::EcartRemplissages | NkUINode::EcartBordures | NkUINode::EcartEffets);
			}
			/// L'EMPREINTE d'une propriete : ce que le fichier ecrirait pour elle.
			static NkString Empreinte(const NkUINode &n, uint32 bit) {
				NkString s;
				if (bit == NkUINode::EcartRemplissages) {
					s.Append(n.fill);
					s.Append('|');
					EcrireFonds(s, n.fills);
				} else if (bit == NkUINode::EcartBordures) {
					s.Append(n.borderColor);
					s.Append('|');
					WriteNum(s, n.borderW);
					s.Append('|');
					EcrireBords(s, n.borders);
				} else if (bit == NkUINode::EcartEffets)
					EcrireEffets(s, n.effets);
				else if (bit == NkUINode::EcartTexte) {
					WriteNum(s, n.fontPx);
					s.Append('|');
					WriteNum(s, n.fontWeight);
					s.Append('|');
					s.Append(n.textColor);
				}
				return s;
			}
			static bool MemeEmpreinte(const NkUINode &a, const NkUINode &b, uint32 bit) {
				const NkString ea = Empreinte(a, bit), eb = Empreinte(b, bit);
				return NkComponentDecl::StrEq(ea.Data() ? ea.Data() : "", eb.Data() ? eb.Data() : "");
			}
			/// COPIER une propriete d'un noeud a l'autre (le style vers le noeud, ou l'inverse).
			static void CopierPropriete(NkUINode &vers, const NkUINode &de, uint32 bit) {
				if (bit == NkUINode::EcartRemplissages) {
					vers.fill = de.fill;
					vers.fills = de.fills;
				} else if (bit == NkUINode::EcartBordures) {
					vers.borderColor = de.borderColor;
					vers.borderW = de.borderW;
					vers.borders = de.borders;
				} else if (bit == NkUINode::EcartEffets)
					vers.effets = de.effets;
				else if (bit == NkUINode::EcartTexte) {
					vers.fontPx = de.fontPx;
					vers.fontWeight = de.fontWeight;
					vers.textColor = de.textColor;
				}
			}
			static const NkString &RefStyle(const NkUINode &n, bool texte) {
				return texte ? n.styleTexte : n.styleCalque;
			}
			static NkString &RefStyleMut(NkUINode &n, bool texte) {
				return texte ? n.styleTexte : n.styleCalque;
			}
			/// PROPAGER un style a tout ce qui le lie, sauf les proprietes surchargees --
			/// le patron de `PropagerVersInstances`, les memes bits. Rend le nombre de
			/// noeuds touches.
			int32 PropagerStyle(const char *cle) {
				const NkStyle *st = TrouverStyle(cle);
				if (!st)
					return 0;
				const bool texte = st->EstTexte();
				const uint32 bits = BitsDuGenre(texte);
				int32 touches = 0;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
					NkUINode &n = nodes[i];
					if (!NkComponentDecl::StrEq(RefStyle(n, texte).Data(), st->cle.Data()))
						continue;
					bool bouge = false;
					for (uint32 bit = 1u; bit <= NkUINode::EcartTexte; bit <<= 1u) {
						if (!(bits & bit) || n.Surcharge(bit))
							continue;
						if (!MemeEmpreinte(n, st->apparence, bit)) {
							CopierPropriete(n, st->apparence, bit);
							bouge = true;
						}
					}
					if (bouge)
						++touches;
				}
				return touches;
			}
			/// LA DETECTION D'ECART a l'edition humaine : le noeud lie un style et
			/// l'ensemble differe -> c'est la main qui vient de l'ecrire, le bit se leve.
			void DetecterEcartsStyle(int32 node) {
				if (!IsValidIndex(node))
					return;
				NkUINode &n = nodes[(uint32)node];
				for (int32 g = 0; g < 2; ++g) {
					const bool texte = g == 1;
					const NkStyle *st = RefStyle(n, texte).Empty() ? nullptr : TrouverStyle(RefStyle(n, texte).Data());
					if (!st)
						continue;
					const uint32 bits = BitsDuGenre(texte);
					for (uint32 bit = 1u; bit <= NkUINode::EcartTexte; bit <<= 1u)
						if ((bits & bit) && !n.Surcharge(bit) && !MemeEmpreinte(n, st->apparence, bit))
							n.ecarts |= bit;
				}
			}
			/// LIER : le noeud prend le style ENTIER (ses ecarts du genre s'effacent --
			/// lier, c'est choisir de suivre), puis la propagation le sert.
			bool LierStyle(int32 node, const char *cle) {
				const NkStyle *st = TrouverStyle(cle);
				if (!st || !IsValidIndex(node))
					return false;
				NkUINode &n = nodes[(uint32)node];
				RefStyleMut(n, st->EstTexte()) = st->cle;
				n.ecarts &= ~BitsDuGenre(st->EstTexte());
				PropagerStyle(st->cle.Data());
				return true;
			}
			/// DETACHER : la reference s'efface, les valeurs restent -- locales desormais.
			bool DetacherStyle(int32 node, bool texte) {
				if (!IsValidIndex(node) || RefStyle(nodes[(uint32)node], texte).Empty())
					return false;
				RefStyleMut(nodes[(uint32)node], texte) = NkString("");
				nodes[(uint32)node].ecarts &= ~BitsDuGenre(texte);
				return true;
			}
			/// REINITIALISER une surcharge : le bit tombe, le style reprend la main.
			bool ReinitialiserEcartStyle(int32 node, uint32 bit) {
				if (!IsValidIndex(node))
					return false;
				NkUINode &n = nodes[(uint32)node];
				const bool texte = bit == NkUINode::EcartTexte;
				if (RefStyle(n, texte).Empty())
					return false;
				n.ecarts &= ~bit;
				PropagerStyle(RefStyle(n, texte).Data());
				return true;
			}
			/// APPLIQUER AU STYLE : l'ensemble du noeud devient celui du style (la branche
			/// « appliquer a l'original » de Q51), ses ecarts tombent, la propagation part.
			int32 AppliquerAuStyle(int32 node, bool texte) {
				if (!IsValidIndex(node))
					return -1;
				NkUINode &n = nodes[(uint32)node];
				NkStyle *st = RefStyle(n, texte).Empty() ? nullptr : TrouverStyleMut(RefStyle(n, texte).Data());
				if (!st)
					return -1;
				const uint32 bits = BitsDuGenre(texte);
				for (uint32 bit = 1u; bit <= NkUINode::EcartTexte; bit <<= 1u)
					if (bits & bit)
						CopierPropriete(st->apparence, n, bit);
				n.ecarts &= ~bits;
				return PropagerStyle(st->cle.Data());
			}
			/// CREER UN STYLE DEPUIS UN NOEUD (le geste de Lunacy, sur la section) :
			/// l'ensemble du noeud devient le style, et le noeud le lie. Cle unique
			/// « calque_N » / « texte_N », nom « Style N » / « Texte N ». Rend l'indice.
			int32 CreerStyleDepuis(int32 node, bool texte, const char *nom) {
				if (!IsValidIndex(node))
					return -1;
				NkString cle, nomDef;
				for (uint32 k = 1u; k < 100000u; ++k) {
					char num[16];
					uint32 l = 0u, t = k;
					char tmp[16];
					do {
						tmp[l++] = (char)('0' + t % 10u);
						t /= 10u;
					} while (t && l < 15u);
					uint32 z = 0u;
					while (l)
						num[z++] = tmp[--l];
					num[z] = '\0';
					cle = NkString(texte ? "texte_" : "calque_");
					cle.Append(num);
					nomDef = NkString(texte ? "Texte " : "Style ");
					nomDef.Append(num);
					if (!TrouverStyle(cle.Data()))
						break;
				}
				NkStyle st;
				st.cle = cle;
				st.nom = nom && *nom ? NkString(nom) : nomDef;
				st.genre = NkString(texte ? "texte" : "calque");
				const uint32 bits = BitsDuGenre(texte);
				for (uint32 bit = 1u; bit <= NkUINode::EcartTexte; bit <<= 1u)
					if (bits & bit)
						CopierPropriete(st.apparence, nodes[(uint32)node], bit);
				styles.PushBack(st);
				LierStyle(node, cle.Data());
				return (int32)styles.Size() - 1;
			}
			/// Combien de noeuds (document et declarations) lient ce style.
			uint32 CompterUsagesStyle(const char *cle) const {
				const NkStyle *st = TrouverStyle(cle);
				if (!st)
					return 0u;
				const bool texte = st->EstTexte();
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					if (NkComponentDecl::StrEq(RefStyle(nodes[i], texte).Data(), st->cle.Data()))
						++n;
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					for (uint32 j = 0; j < (uint32)declarations[d].arbre.Size(); ++j)
						if (NkComponentDecl::StrEq(RefStyle(declarations[d].arbre[j], texte).Data(), st->cle.Data()))
							++n;
				return n;
			}
			/// DETACHER TOUT ce qui lie ce style (les valeurs restent). Rend le nombre.
			uint32 DetacherTousStyle(const char *cle) {
				const NkStyle *st = TrouverStyle(cle);
				if (!st)
					return 0u;
				const bool texte = st->EstTexte();
				const NkString c = st->cle;
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					if (NkComponentDecl::StrEq(RefStyle(nodes[i], texte).Data(), c.Data())) {
						RefStyleMut(nodes[i], texte) = NkString("");
						nodes[i].ecarts &= ~BitsDuGenre(texte);
						++n;
					}
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					for (uint32 j = 0; j < (uint32)declarations[d].arbre.Size(); ++j)
						if (NkComponentDecl::StrEq(RefStyle(declarations[d].arbre[j], texte).Data(), c.Data())) {
							RefStyleMut(declarations[d].arbre[j], texte) = NkString("");
							++n;
						}
				return n;
			}
			/// SUPPRIMER un style : REFUSE tant qu'il est utilise (le nombre dit).
			bool SupprimerStyle(const char *cle, uint32 *usages = nullptr) {
				const NkStyle *st = TrouverStyle(cle);
				if (!st)
					return false;
				const uint32 u = CompterUsagesStyle(cle);
				if (usages)
					*usages = u;
				if (u > 0u)
					return false;
				for (uint32 i = 0; i < (uint32)styles.Size(); ++i)
					if (&styles[i] == st) {
						styles.RemoveAt(i);
						return true;
					}
				return false;
			}
			// ── LA VARIABLE DANS L'INTERFACE (§15.14, lot du 05/09) ─────────────
			/// UN SEUL VISITEUR de toutes les couleurs d'un noeud : cle simple,
			/// texte, bord, listes de remplissages et leurs arrets, bordures, effets,
			/// etats. Trois lecteurs (compter, detacher, supprimer) et UNE table : un
			/// champ de couleur ajoute ici est compte, detache et garde partout ;
			/// ajoute ailleurs, il serait un usage INVISIBLE -- et une variable dite
			/// « inutilisee » se supprimerait sous lui (reference orpheline, magenta).
			template <class N, class F>
			static void VisiterCouleursNoeud(N &n, F &&f) {
				f(n.fill);
				f(n.textColor);
				f(n.borderColor);
				for (uint32 i = 0; i < (uint32)n.fills.Size(); ++i) {
					f(n.fills[i].couleur);
					for (uint32 a = 0; a < (uint32)n.fills[i].degrade.arrets.Size(); ++a)
						f(n.fills[i].degrade.arrets[a].couleur);
				}
				for (uint32 i = 0; i < (uint32)n.borders.Size(); ++i)
					f(n.borders[i].couleur);
				for (uint32 i = 0; i < (uint32)n.effets.Size(); ++i)
					f(n.effets[i].couleur);
				// 🔴 (11/09) LES DEUX CHAMPS D'ETAT AJOUTES CE JOUR-LA MANQUAIENT ICI :
				//    `couleurTexte` (lot ①) et `bordureCouleur` (lot ② b). `NkPorteHex`
				//    accepte « @cle » : un etat pouvait donc REFERENCER une variable que ce
				//    visiteur ne voyait pas -- comptee zero, supprimable sous lui, magenta.
				//    C'est exactement le defaut que l'avertissement ci-dessus annoncait,
				//    et je l'ai commis en ajoutant les champs AILLEURS qu'ici.
				for (uint32 i = 0; i < (uint32)n.apparences.Size(); ++i) {
					f(n.apparences[i].fond);
					f(n.apparences[i].couleurTexte);
					f(n.apparences[i].bordureCouleur);
				}
			}
			/// ... et de tout le document : les noeuds ET les arbres des declarations
			/// (un composant dont le fond reference une variable EST un usage).
			template <class F>
			void VisiterCouleurs(F &&f) {
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					VisiterCouleursNoeud(nodes[i], f);
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					for (uint32 j = 0; j < (uint32)declarations[d].arbre.Size(); ++j)
						VisiterCouleursNoeud(declarations[d].arbre[j], f);
				for (uint32 si = 0; si < (uint32)styles.Size(); ++si) // un style qui reference une variable EST un usage
					VisiterCouleursNoeud(styles[si].apparence, f);
			}
			template <class F>
			void VisiterCouleurs(F &&f) const {
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					VisiterCouleursNoeud(nodes[i], f);
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					for (uint32 j = 0; j < (uint32)declarations[d].arbre.Size(); ++j)
						VisiterCouleursNoeud(declarations[d].arbre[j], f);
				for (uint32 si = 0; si < (uint32)styles.Size(); ++si)
					VisiterCouleursNoeud(styles[si].apparence, f);
			}
			/// Combien de couleurs du document referencent cette variable.
			uint32 CompterUsagesVariable(const char *cle) const {
				if (cle && *cle == '@')
					++cle;
				if (!cle || !*cle)
					return 0u;
				uint32 n = 0u;
				VisiterCouleurs([&](const NkString &c) {
					if (NkEstReference(c.Data()) && NkComponentDecl::StrEq(c.Data() + 1, cle))
						++n;
				});
				return n;
			}
			/// CREER une variable de couleur depuis une valeur -- le geste du
			/// selecteur (Lunacy : « Create Color Variable » sous la rangee du
			/// modele). Cle unique « couleur_N », nom « Couleur N » (le rail le
			/// renomme). Rend l'indice de la variable.
			int32 CreerVariableCouleur(const char *valeur, const char *nom) {
				NkString cle, nomDef;
				for (uint32 k = 1u; k < 100000u; ++k) {
					char num[16];
					uint32 l = 0u, t = k;
					char tmp[16];
					do {
						tmp[l++] = (char)('0' + t % 10u);
						t /= 10u;
					} while (t && l < 15u);
					uint32 z = 0u;
					while (l)
						num[z++] = tmp[--l];
					num[z] = '\0';
					cle = NkString("couleur_");
					cle.Append(num);
					nomDef = NkString("Couleur ");
					nomDef.Append(num);
					if (!TrouverVariable(cle.Data()))
						break;
				}
				NkVariable v;
				v.cle = cle;
				v.nom = nom && *nom ? NkString(nom) : nomDef;
				v.valeur = NkString(valeur ? valeur : "");
				variables.PushBack(v);
				return (int32)variables.Size() - 1;
			}
			/// DETACHER : chaque reference vers `cle` devient le LITTERAL que l'oeil
			/// voyait (la valeur du mode courant). Rend le nombre de couleurs
			/// detachees. Une variable ABSENTE se detache en magenta -- la couleur
			/// qu'elle montrait deja ; l'appelant le dit.
			uint32 DetacherVariable(const char *cle) {
				if (cle && *cle == '@')
					++cle;
				if (!cle || !*cle)
					return 0u;
				const NkVariable *v = TrouverVariable(cle);
				const NkString litteral(v ? v->ValeurPour(modeCourant.Data()) : "#ff00ff");
				uint32 n = 0u;
				VisiterCouleurs([&](NkString &c) {
					if (NkEstReference(c.Data()) && NkComponentDecl::StrEq(c.Data() + 1, cle)) {
						c = litteral;
						++n;
					}
				});
				return n;
			}
			/// SUPPRIMER une variable : REFUSE tant qu'elle est utilisee (le nombre
			/// est rendu dans `usages`). Une reference orpheline serait dite en
			/// magenta, mais on ne la fabrique pas soi-meme : detacher d'abord.
			bool SupprimerVariable(const char *cle, uint32 *usages = nullptr) {
				if (cle && *cle == '@')
					++cle;
				const uint32 u = CompterUsagesVariable(cle);
				if (usages)
					*usages = u;
				if (u > 0u || !cle)
					return false;
				for (uint32 i = 0; i < (uint32)variables.Size(); ++i)
					if (NkComponentDecl::StrEq(variables[i].cle.Data(), cle)) {
						variables.RemoveAt(i);
						return true;
					}
				return false;
			}

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
				variables.Clear(); // 05/09 : aucun des deux ne les vidait -- un chargement gardait les variables du document d'avant
				modeCourant = NkString();
				styles.Clear();
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
				// UNE TOILE DE DESIGN EST UNE TOILE, PAS UNE COLONNE : la racine pose
				// ses enfants librement, sinon une forme creee hors page serait
				// empilee par un agencement. (Le document de Rodolf dit deja
				// `agencement = free` : c'est le defaut de `NewDocument` qui divergeait.)
				root.layout.kind = NkLayoutKind::Free;
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
					d.fills = s.fills; // la LISTE suit la copie, comme tout le reste
					d.borders = s.borders;
					d.effets = s.effets;
					d.sommets = s.sommets;
					d.textColor = s.textColor;
					d.borderColor = s.borderColor;
					d.alignText = s.alignText;
					d.target = s.target;
					d.texteLangues = s.texteLangues;
					d.texteTraduits = s.texteTraduits;
					d.transposeDe = s.transposeDe;
					d.radius = s.radius;
					// ⚠️ LES TROIS CHAMPS DE TRANSFORMATION VOYAGENT AVEC LE NOEUD.
					//    Oubliés ici, un copier-coller aurait « redressé » l'objet
					//    en silence : la copie aurait eu la même forme et pas la
					//    même orientation, et le cas « la copie ne perd aucun
					//    champ » (recette gestes) l'aurait vu — c'est lui qui
					//    protège cette ligne, pas ma vigilance.
					d.rotation = s.rotation;
					d.inclinaisonX = s.inclinaisonX;
					d.inclinaisonY = s.inclinaisonY;
					d.echelleX = s.echelleX;
					d.echelleY = s.echelleY;
					d.refusPosition = s.refusPosition;
					d.refusRotation = s.refusRotation;
					d.refusEchelle = s.refusEchelle;
					d.genre = s.genre;
					d.miroirH = s.miroirH;
					d.miroirV = s.miroirV;
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

			// ═══════════════════════════════════════════════════════════════════
			//  EXTRAIRE / DETACHER — ET ILS ARRIVENT ENSEMBLE (modele §15.4)
			// ═══════════════════════════════════════════════════════════════════
			//  🔴 NON NEGOCIABLE, ET LA RAISON N'EST PAS TECHNIQUE : *« creer un
			//     composant » sans « detacher » enferme l'utilisateur dans une
			//     decision qu'il ne peut pas defaire.* Le premier qui a besoin
			//     d'une variante et ne peut pas la faire CESSERA de creer des
			//     composants -- et l'atelier aura produit l'inverse de son but.
			//
			//  ⚠️ LES DEUX PASSENT PAR `CopierSousArbre`, VIA UN DOCUMENT
			//     TEMPORAIRE, plutot que de recopier sa liste de trente champs.
			//     Cette liste porte deja une cicatrice (« les trois champs de
			//     transformation voyagent avec le noeud », oubliés une fois) : en
			//     ecrire une seconde copie, c'est garantir qu'un champ ajoute
			//     demain sera oublie dans l'une des deux. *Le seul cout est un
			//     decalage d'indices, et il est mecanique.*

			/// Le sous-arbre `tmp` (racine 0 ignoree) devient un arbre AUTONOME.
			static void ArbreDepuisDocument(const NkUIDocument &tmp, NkVector<NkUINode> &out) {
				out.Clear();
				for (uint32 i = 1; i < (uint32)tmp.nodes.Size(); ++i) {
					NkUINode n = tmp.nodes[i];
					n.parent = (tmp.nodes[i].parent <= 0) ? -1 : tmp.nodes[i].parent - 1;
					n.children.Clear();
					for (uint32 c = 0; c < (uint32)tmp.nodes[i].children.Size(); ++c)
						n.children.PushBack(tmp.nodes[i].children[c] - 1);
					out.PushBack(n);
				}
			}

			/// L'inverse : un arbre autonome redevient un document temporaire dont
			/// la racine 0 est un porteur, et l'arbre commence a l'indice 1.
			static void DocumentDepuisArbre(const NkVector<NkUINode> &arbre, NkUIDocument &tmp) {
				tmp.NewDocument("temporaire", NkAuthor::Humain);
				for (uint32 i = 0; i < (uint32)arbre.Size(); ++i) {
					NkUINode n = arbre[i];
					n.parent = (arbre[i].parent < 0) ? 0 : arbre[i].parent + 1;
					n.children.Clear();
					for (uint32 c = 0; c < (uint32)arbre[i].children.Size(); ++c)
						n.children.PushBack(arbre[i].children[c] + 1);
					tmp.nodes.PushBack(n);
				}
				if (arbre.Size() > 0)
					tmp.nodes[0].children.PushBack(1);
			}

			/// EXTRAIRE : `node` et sa descendance deviennent une DECLARATION, et
			/// `node` reste en place comme INSTANCE.
			/// @return l'indice de la declaration creee, -1 en cas de refus.
			int32 ExtraireComposant(int32 node, const char *auteur, const char *nom) {
				if (!IsValidIndex(node) || node == 0)
					return -1;
				NkUIDocument tmp;
				tmp.NewDocument("extraction", NkAuthor::Humain);
				if (tmp.CopierSousArbre(*this, node, 0) < 0)
					return -1;
				NkDeclarationComposant dc;
				dc.identite.auteur = NkString(auteur ? auteur : "");
				dc.identite.nom = NkString(nom && *nom ? nom : "composant");
				dc.identite.version = NkString("1");
				ArbreDepuisDocument(tmp, dc.arbre);
				if (dc.arbre.Empty())
					return -1;
				declarations.PushBack(dc);
				// 🔴 LA DESCENDANCE **RESTE DANS LE DOCUMENT**, ET C'EST UNE MESURE
				//    QUI L'A DECIDE, PAS UNE PREFERENCE.
				//
				//    Ma premiere version la RETIRAIT : la declaration devenait
				//    seule detentrice du sous-arbre, l'instance n'etait qu'une
				//    reference. Defendable sur le papier, FAUX a l'ecran. Le cas
				//    « une instance peint le contenu de sa declaration » l'a
				//    mesure : **3 commandes de peintre avant l'extraction, 2
				//    apres** -- autrement dit, extraire un bouton lui faisait
				//    PERDRE SON LIBELLE.
				//
				//    La cause est structurelle, pas un oubli de dessin : les nœuds
				//    d'une declaration ne passent pas par `NkComputeLayout`, donc
				//    ils n'ont aucun rectangle, donc rien ne peut les peindre. Les
				//    resoudre au peintre aurait demande de les resoudre AUSSI a la
				//    disposition -- deux mecanismes de plus pour retrouver ce que
				//    le document savait deja faire.
				//
				// ⚠️ L'INSTANCE GARDE DONC SON SOUS-ARBRE MATERIALISE, la
				//    declaration en detenant la copie de reference. C'est ce que
				//    fait Figma, et ca rend trois choses gratuites : le dessin, la
				//    disposition, et le detachement -- qui n'a plus rien a
				//    rematerialiser. *Extraire devient un geste de STRUCTURE qui ne
				//    change RIEN a l'image, ce que le cas exige desormais.*
				//
				// 📌 Le prix est une duplication (la declaration + chaque
				//    instance), et il est assume : c'est ce qui permettra a la
				//    propagation (Q51) de reecrire les instances sans que le
				//    document ait a resoudre une reference a chaque image.
				nodes[(uint32)node].instanceDe = declarations[(uint32)declarations.Size() - 1]
													 .identite.Cle();
				nodes[(uint32)node].ecarts = 0;
				return (int32)declarations.Size() - 1;
			}

			/// L'indice de la declaration portant cette cle, -1 si absente.
			/// ⑤ LA DECLARATION PAR SON NOM LISIBLE (2026-09-05) -- la cle est
			/// `auteur/nom@version`, et personne ne la tape ; l'utilisateur, lui, connait le
			/// NOM (« Bouton_Connexion »). Sert a « Voir le composant » sur un noeud qui n'est
			/// pas une instance mais porte le nom d'une declaration.
			/// ⚠️ LE PREMIER QUI REPOND GAGNE : deux declarations peuvent partager un nom (deux
			///    auteurs, deux versions) -- la cle, elle, est unique. Dit ici plutot que
			///    decouvert : ce chemin est une commodite d'interface, jamais l'identite.
			int32 TrouverDeclarationParNom(const char *nom) const {
				if (!nom || !*nom)
					return -1;
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					if (NkComponentDecl::StrEq(declarations[d].identite.nom.Data(), nom))
						return (int32)d;
				return -1;
			}

			int32 TrouverDeclaration(const char *cle) const {
				if (!cle || !*cle)
					return -1;
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d)
					if (NkComponentDecl::StrEq(declarations[d].identite.Cle().Data(), cle))
						return (int32)d;
				return -1;
			}

			/// DETACHER : l'instance redevient un sous-arbre ordinaire.
			/// ⚠️ LES ECARTS SONT **FUSIONNES**, PAS JETES. Une instance dont le
			///    texte a ete surcharge garde SON texte en se detachant : les
			///    champs propres du nœud ne sont jamais ecrases ici, seule la
			///    DESCENDANCE est rematerialisee. Jeter les ecarts serait une
			///    perte de travail silencieuse -- la pire espece.
			bool DetacherInstance(int32 node) {
				if (!IsValidIndex(node) || nodes[(uint32)node].instanceDe.Empty())
					return false;
				const int32 d = TrouverDeclaration(nodes[(uint32)node].instanceDe.Data());
				if (d < 0)
					return false;
				// 🔴 IL N'Y A PLUS RIEN A REMATERIALISER, ET C'EST LE BENEFICE DE
				//    LA CORRECTION D'A COTE : depuis que l'instance GARDE son
				//    sous-arbre, detacher ne recopie rien -- il retire seulement le
				//    lien. Ma version precedente recopiait les enfants de la
				//    declaration ; laissee ici, elle les aurait AJOUTES A CEUX QUI
				//    SONT DEJA LA, et un bouton detache se serait retrouve avec
				//    deux libelles superposes.
				//
				// ⚠️ ET LES ECARTS SONT AINSI FUSIONNES SANS EFFORT : les champs
				//    propres du nœud n'ont jamais ete touches, donc une instance
				//    dont le texte etait surcharge garde SON texte. La regle du
				//    §15.4 (« fusionnes, pas jetes ») devient une propriete de la
				//    structure au lieu d'une precaution a tenir.
				nodes[(uint32)node].instanceDe = NkString("");
				nodes[(uint32)node].ecarts = 0;
				return true;
			}

			// ── ② LA PROPAGATION (Q51 R1′, révisée et validée le 02/09) ──────
			//
			// 🔑 LA RÈGLE, DANS SES MOTS : *« si je modifie l'arrondi du composant,
			//    ça modifie pour tous les boutons qui en héritent »* — et les
			//    surcharges TIENNENT : *« la couleur n'est qu'une propriété »*,
			//    deux boutons du même composant gardent chacun la leur.
			//
			// ⚠️ CE QUI EST COPIÉ EST CE QUI N'EST PAS SURCHARGÉ, propriété par
			//    propriété — jamais le nœud entier. Écraser l'instance en bloc
			//    aurait fait disparaître le travail de la main *en silence*, la
			//    pire espèce de perte, celle que §15.4 interdit déjà au
			//    détachement.
			//
			// ⚠️ ET LA DESCENDANCE N'EST PAS RETOUCHÉE ICI. Une instance porte son
			//    sous-arbre matérialisé (voir ExtraireComposant) ; propager dans
			//    les enfants demande une correspondance nœud-à-nœud que le modèle
			//    ne porte pas encore. *Propager à moitié en le taisant serait pire
			//    que ne pas propager* : la racine suit, et cette limite est écrite
			//    ici comme dans le rapport.
			/// @return le nombre d'instances RÉELLEMENT modifiées.
			int32 PropagerVersInstances(int32 decl) {
				if (decl < 0 || decl >= (int32)declarations.Size())
					return 0;
				const NkDeclarationComposant &dc = declarations[(uint32)decl];
				if (dc.arbre.Empty())
					return 0;
				const NkUINode &ref = dc.arbre[0];
				const NkString cle = dc.identite.Cle();
				int32 touchees = 0;
				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
					NkUINode &n = nodes[i];
					if (n.instanceDe.Empty()
						|| !NkComponentDecl::StrEq(n.instanceDe.Data(), cle.Data()))
						continue;
					bool bouge = false;
					// LE FOND — sauf si l'instance l'a surchargé.
					if (!n.Surcharge(NkUINode::EcartRemplissages)) {
						if (!NkComponentDecl::StrEq(n.fill.Data(), ref.fill.Data())) {
							n.fill = ref.fill;
							bouge = true;
						}
						// ⚠️ LE CONTENU, PAS LE NOMBRE. Comparer les tailles laissait
						//    passer le cas le plus courant : changer la COULEUR du
						//    composant sans changer le nombre de remplissages. Aucune
						//    instance ne suivait (mesure : sonde 58b, « 0 touchee »).
						if (!NkMemesRemplissages(n.fills, ref.fills)) {
							n.fills = ref.fills;
							bouge = true;
						}
					}
					// L'APPARENCE (rayon, opacité) — même règle, autre bit.
					if (!n.Surcharge(NkUINode::EcartApparence)) {
						if (n.radius != ref.radius) {
							n.radius = ref.radius;
							bouge = true;
						}
					}
					if (bouge)
						++touchees;
				}
				return touchees;
			}

			/// APPLIQUER AU COMPOSANT : les propriétés de l'instance `node`
			/// deviennent celles de sa DÉCLARATION, puis la propagation part.
			///
			/// 🔑 C'est le geste qui rend la règle JOUABLE À LA MAIN : sans lui,
			///    Rodolf n'aurait aucun moyen de modifier une déclaration — elle
			///    n'est pas un nœud qu'on sélectionne. C'est la branche
			///    « appliquer à l'original » du dialogue à trois branches (R2′) ;
			///    les deux autres branches attendent le dialogue lui-même.
			///
			/// ⚠️ LA PORTE DE PROPRIÉTÉ EST CONSULTÉE ICI, ET NULLE PART AILLEURS
			///    (`NkPeutModifierDeclaration`, R3) : on ne modifie pas en place
			///    la déclaration d'autrui — elle se forke. Recalculer ce critère
			///    au site d'appel, c'est le voir diverger au premier appelant qui
			///    l'oublie.
			/// @return le nombre d'instances suivies, -1 si le geste est REFUSÉ.
			int32 AppliquerAuComposant(int32 node, const char *auteurCourant) {
				if (!IsValidIndex(node) || nodes[(uint32)node].instanceDe.Empty())
					return -1;
				const int32 d = TrouverDeclaration(nodes[(uint32)node].instanceDe.Data());
				if (d < 0)
					return -1;
				if (!NkPeutModifierDeclaration(declarations[(uint32)d].identite, auteurCourant))
					return -1;
				if (declarations[(uint32)d].arbre.Empty())
					return -1;
				NkUINode &ref = declarations[(uint32)d].arbre[0];
				const NkUINode &src = nodes[(uint32)node];
				ref.fill = src.fill;
				ref.fills = src.fills;
				ref.radius = src.radius;
				// ⚠️ L'INSTANCE SOURCE CESSE D'ÊTRE « SURCHARGÉE » sur ce qu'elle
				//    vient de DONNER : sa valeur EST désormais la référence. Lui
				//    laisser ses bits en ferait une instance éternellement en
				//    écart d'elle-même — et le prochain changement du composant
				//    la sauterait sans raison visible.
				nodes[(uint32)node].ecarts &=
					~(uint32)(NkUINode::EcartRemplissages | NkUINode::EcartApparence);
				return PropagerVersInstances(d);
			}

			/// INSTANCIER : une NOUVELLE instance de la déclaration `decl` naît
			/// sous `parent`. C'est la moitié « réutiliser » du chantier — sans
			/// elle, extraire un composant ne servait qu'à le détacher.
			///
			/// ⚠️ L'INSTANCE NAÎT **MATÉRIALISÉE**, comme celles d'ExtraireComposant :
			///    le sous-arbre de la déclaration est recopié dans le document
			///    (mêmes raisons, mesurées là-bas : disposition, dessin et
			///    détachement gratuits). La recopie passe par `CopierSousArbre` via
			///    un document temporaire — jamais par une liste de champs écrite à
			///    la main, qui porte déjà une cicatrice.
			/// @return l'indice du nouveau nœud, -1 en cas de refus.
			int32 InstancierComposant(int32 decl, int32 parent) {
				if (decl < 0 || decl >= (int32)declarations.Size() || !IsValidIndex(parent))
					return -1;
				NkUIDocument tmp;
				DocumentDepuisArbre(declarations[(uint32)decl].arbre, tmp);
				// La racine de l'arbre vit à l'indice 1 du temporaire (0 = porteur).
				if (tmp.nodes.Size() < 2)
					return -1;
				const int32 neuf = CopierSousArbre(tmp, 1, parent);
				if (neuf < 0)
					return -1;
				nodes[(uint32)neuf].instanceDe = declarations[(uint32)decl].identite.Cle();
				nodes[(uint32)neuf].ecarts = 0;
				return neuf;
			}

			// ── PROVENANCE : LES DEUX AUTOMATISMES ─────────────────────────────
			// A APPELER APRES TOUTE MODIFICATION D'UN NOEUD PAR LA MAIN. C'est le
			// seul endroit ou `corrected` passe a vrai, et le seul ou `verified`
			// retombe a faux.
			void MarkHumanEdit(int32 node) {
				if (!IsValidIndex(node))
					return;
				DetecterEcartsStyle(node); // §15.15 : la main vient d'ecrire -- ce qui differe du style est un ecart
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
			/// ECRIRE UN NOEUD. Extrait de la boucle pour que les arbres des
			/// DECLARATIONS de composants passent par le MEME ecrivain -- une
			/// seconde copie aurait diverge au premier champ ajoute, et c'est le
			/// motif que ce depot passe son temps a retirer (« le peintre a ete
			/// ecrit deux fois »).
			/// @param motCle `noeud` pour le document, `dnoeud` dans une declaration.
			// ── LES TROIS LISTES S'ECRIVENT ICI, ET NULLE PART AILLEURS ────────
			// Extraites d'`EcrireNoeud` le 05/09 pour que le STYLE (§15.15) ecrive ses
			// listes avec les memes ecrivains -- et pour que l'EMPREINTE d'un ensemble
			// (ce que le fichier ecrirait) serve de comparateur : une seule verite.
			static void EcrireFonds(NkString &out, const NkVector<NkRemplissage> &fills) {
				// La LISTE (Lunacy FILLS) : une ligne par remplissage,
				// `fond_<i>` a partir de 1, « couleur opacite visible ».
				// Meme patron additif que `texte_<langue>` : la cle
				// n'existe que si la liste existe.
				for (uint32 fi = 0; fi < (uint32)fills.Size(); ++fi) {
					const NkRemplissage &f = fills[fi];
					out.Append("  fond_");
					WriteNum(out, (float32)(fi + 1));
					out.Append(" = ");
					out.Append(f.couleur.Empty() ? "-" : f.couleur.Data());
					out.Append(' ');
					WriteNum(out, f.opacite);
					out.Append(' ');
					out.Append(f.visible ? "1" : "0");
					// additifs : rien tant que tout vaut son defaut
					if (!f.genre.Empty()) {
						out.Append(" genre=");
						out.Append(f.genre);
					}
					if (!f.cadrage.Empty()) {
						out.Append(" cadrage=");
						out.Append(f.cadrage);
					}
					if (!f.image.Empty()) {
						out.Append(" image=");
						out.Append(f.image);
					}
					if (f.rotationImage != 0.f) {
						out.Append(" rotation_image=");
						WriteNum(out, f.rotationImage);
					}
					if (!f.CropEntier()) {
						out.Append(" crop=");
						WriteNum(out, f.cropX);
						out.Append(',');
						WriteNum(out, f.cropY);
						out.Append(',');
						WriteNum(out, f.cropW);
						out.Append(',');
						WriteNum(out, f.cropH);
					}
					if (!f.fusion.Empty()) {
						out.Append(" fusion=");
						out.Append(f.fusion);
					}
					if (!f.inconnus.Empty()) {
						out.Append(' ');
						out.Append(f.inconnus);
					}
					out.Append('\n');
					// `degrade_<i> = <type> <angle> <pos>:<coul> ...`
					// ⚠️ ADDITIVE : pas d'arrets, pas de cle. Un remplissage
					//    uni ne gagne pas une ligne parce qu'un degrade
					//    existe ailleurs dans le fichier.
					if (f.degrade.Actif()) {
						out.Append("  degrade_");
						WriteNum(out, (float32)(fi + 1));
						out.Append(" = ");
						out.Append(f.degrade.type.Empty() ? "lineaire"
														  : f.degrade.type.Data());
						out.Append(' ');
						WriteNum(out, f.degrade.angle);
						// ① origine et rayons : ADDITIFS, hors defaut seulement
						if (f.degrade.origineX != 0.5f || f.degrade.origineY != 0.5f) {
							out.Append(" o=");
							WriteNum(out, f.degrade.origineX);
							out.Append(',');
							WriteNum(out, f.degrade.origineY);
						}
						if (f.degrade.rayonX != 0.5f || f.degrade.rayonY != 0.5f) {
							out.Append(" r=");
							WriteNum(out, f.degrade.rayonX);
							out.Append(',');
							WriteNum(out, f.degrade.rayonY);
						}
						if (!f.degrade.inconnus.Empty()) {
							out.Append(' ');
							out.Append(f.degrade.inconnus);
						}
						for (uint32 ai = 0; ai < (uint32)f.degrade.arrets.Size(); ++ai) {
							out.Append(' ');
							WriteNum(out, f.degrade.arrets[ai].position);
							out.Append(':');
							out.Append(f.degrade.arrets[ai].couleur.Empty()
										   ? "-"
										   : f.degrade.arrets[ai].couleur.Data());
							if (f.degrade.arrets[ai].opacite != 100.f) {
								out.Append(':');
								WriteNum(out, f.degrade.arrets[ai].opacite);
							}
						}
						out.Append('\n');
					}
				}
			}
			static void EcrireBords(NkString &out, const NkVector<NkBordure> &borders) {
				for (uint32 bi = 0; bi < (uint32)borders.Size(); ++bi) {
					const NkBordure &b = borders[bi];
					out.Append("  bord_");
					WriteNum(out, (float32)(bi + 1));
					out.Append(" = ");
					out.Append(b.couleur.Empty() ? "-" : b.couleur.Data());
					out.Append(' ');
					WriteNum(out, b.opacite);
					out.Append(' ');
					out.Append(b.visible ? "1" : "0");
					out.Append(' ');
					WriteNum(out, b.epaisseur);
					out.Append(' ');
					out.Append(NkBordurePosNom(b.position));
					// additifs : rien tant que tout vaut son defaut
					if (!b.CotesEgaux()) {
						out.Append(" cotes=");
						for (uint32 k = 0; k < 4u; ++k) {
							if (k)
								out.Append(',');
							WriteNum(out, b.Cote(k));
						}
					}
					if (!b.jointure.Empty()) {
						out.Append(" jointure=");
						out.Append(b.jointure);
					}
					if (!b.extremite.Empty()) {
						out.Append(" extremite=");
						out.Append(b.extremite);
					}
					if (!b.inconnus.Empty()) {
						out.Append(' ');
						out.Append(b.inconnus);
					}
					out.Append('\n');
				}
			}
			static void EcrireEffets(NkString &out, const NkVector<NkEffet> &effets) {
			for (uint32 ei = 0; ei < (uint32)effets.Size(); ++ei) {
				const NkEffet &e = effets[ei];
				out.Append("  effet_");
				WriteNum(out, (float32)(ei + 1));
				out.Append(" = ");
				out.Append(NkEffetTypeNom(e.type));
				out.Append(' ');
				WriteNum(out, e.x);
				out.Append(' ');
				WriteNum(out, e.y);
				out.Append(' ');
				WriteNum(out, e.flou);
				out.Append(' ');
				WriteNum(out, e.etendue);
				out.Append(' ');
				out.Append(e.couleur.Empty() ? "-" : e.couleur.Data());
				out.Append(' ');
				WriteNum(out, e.opacite);
				out.Append(' ');
				out.Append(e.visible ? "1" : "0");
				out.Append('\n');
			}
			}

			void EcrireNoeud(NkString &out, const NkUINode &n, uint32 i,
							 const char *motCle) const {
				out.Append('\n');
				out.Append(motCle);
				out.Append(' ');
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
				// ⚠️ L'UNE OU L'AUTRE, JAMAIS LES DEUX. Ecrire `fond` EN PLUS
				//    de la liste donnerait un fichier a deux verites, et le
				//    lecteur devrait choisir -- c'est-a-dire deviner.
				if (!n.fills.Empty()) {
					EcrireFonds(out, n.fills);
				} else if (!n.fill.Empty())
					Field(out, "fond", n.fill.Data());
				if (!n.textColor.Empty())
					Field(out, "couleur_texte", n.textColor.Data());
				// ⚠️ L'UNE OU L'AUTRE, JAMAIS LES DEUX — comme pour `fond`.
				//    `bord_<i> = couleur opacite visible epaisseur position`.
				//    Et quand la liste existe, la clé `bordure` ne s'écrit pas
				//    non plus : elle appartient à la même notion, et un fichier
				//    qui porte les deux ferait choisir le lecteur.
				if (!n.borders.Empty()) {
					EcrireBords(out, n.borders);
				} else if (!n.borderColor.Empty())
					Field(out, "couleur_bord", n.borderColor.Data());
				// LES SOMMETS DEPLACES : `sommet_<i> = x y [rayon]` (unitaire -1..1).
				// ⚠️ LE TROISIEME CHAMP N'EST ECRIT QUE S'IL EST NON NUL, et c'est
				//    ce qui garde l'aller-retour OCTET POUR OCTET pour tout
				//    document ecrit avant l'arrondi par sommet : un tracé a coins
				//    vifs rend exactement les deux memes nombres qu'hier. Meme
				//    discipline additive que `position`, `shape` et les trois
				//    listes -- reprise sans etre amenagee.
				for (uint32 si = 0; si < (uint32)n.sommets.Size(); ++si) {
					out.Append("  sommet_");
					WriteNum(out, (float32)(si + 1));
					out.Append(" = ");
					WriteNum(out, n.sommets[si].x);
					out.Append(' ');
					WriteNum(out, n.sommets[si].y);
					// ⚠️ LES CHAMPS SONT POSITIONNELS, DONC UN CHAMP TARDIF FORCE
					//    CEUX D'AVANT. Un sommet qui porte des tangentes mais pas
					//    de rayon doit quand même écrire son rayon (0) pour tenir
					//    la place -- sinon le lecteur prendrait la tangente pour
					//    un rayon. C'est le prix d'un format positionnel, et il
					//    se paie ICI plutôt que par un lecteur qui devine.
					// ⚠️ ET RIEN NE CHANGE POUR LES DOCUMENTS D'AVANT : un tracé
					//    sans tangente ni rayon rend exactement les DEUX MEMES
					//    nombres qu'hier, octet pour octet. Même discipline
					//    additive que `position`, `shape` et les trois listes --
					//    reprise sans être aménagée, pour la deuxième fois.
					const NkPoint2 &sp = n.sommets[si];
					const bool aTangente = sp.liaison != NkPoint2::LiaisonDroit
										   || sp.ex != 0.f || sp.ey != 0.f || sp.sx != 0.f
										   || sp.sy != 0.f;
					if (sp.rayon != 0.f || aTangente) {
						out.Append(' ');
						WriteNum(out, sp.rayon);
					}
					if (aTangente) {
						out.Append(' ');
						WriteNum(out, sp.ex);
						out.Append(' ');
						WriteNum(out, sp.ey);
						out.Append(' ');
						WriteNum(out, sp.sx);
						out.Append(' ');
						WriteNum(out, sp.sy);
						out.Append(' ');
						WriteNum(out, (float32)sp.liaison);
					}
					out.Append('\n');
				}
				// LES EFFETS : `effet_<i> = type x y flou etendue couleur opacite visible`.
				EcrireEffets(out, n.effets);
				if (!n.alignText.Empty())
					Field(out, "texte_aligne", n.alignText.Data());
				if (!n.target.Empty())
					Field(out, "cible", n.target.Data());
				if (!n.targetUnit.Empty())
					Field(out, "unite", n.targetUnit.Data());
				// `apparence_<Etat> = fond radius opacite`, « - » = HERITE.
				// ⚠️ `opacite` EST CELLE DU NŒUD (CALQUE, enfants compris), pas celle du
				//    fond -- sens tranche le 11/09 ; le jeton garde sa place.
				// ⚠️ Un bloc VIDE ne s'ecrit pas : ouvrir la section d'un etat
				//    sans rien y poser ne doit pas alourdir le fichier -- sinon
				//    le simple fait de REGARDER un etat le ferait exister.
				for (uint32 ai = 0; ai < (uint32)n.apparences.Size(); ++ai) {
					const NkApparenceEtat &a = n.apparences[ai];
					if (a.etat.Empty() || a.Vide())
						continue;
					out.Append("  apparence_");
					out.Append(a.etat.Data());
					out.Append(" = ");
					out.Append(a.fond.Empty() ? "-" : a.fond.Data());
					out.Append(' ');
					if (a.radius < 0.f)
						out.Append('-');
					else
						WriteNum(out, a.radius);
					out.Append(' ');
					if (a.opacite < 0.f)
						out.Append('-');
					else
						WriteNum(out, a.opacite);
					// LES JETONS SUIVANTS SONT NOMMES ET OPTIONNELS : un bloc qui n'en pose
					// aucun s'ecrit EXACTEMENT comme avant -- un document d'avant se
					// reenregistre octet pour octet. Et un lecteur d'avant, qui lit trois
					// jetons, ne casse pas sur les suivants : il les ignore.
					if (!a.couleurTexte.Empty()) {
						out.Append(" texte=");
						out.Append(a.couleurTexte.Data());
					}
					if (a.OmbrePosee()) {
						out.Append(" ombre=");
						if (a.ombreFlou < 0.f)
							out.Append('-');
						else
							WriteNum(out, a.ombreFlou);
						out.Append(',');
						if (a.ombreOpacite < 0.f)
							out.Append('-');
						else
							WriteNum(out, a.ombreOpacite);
					}
					if (a.BordurePosee()) {
						out.Append(" bordure=");
						out.Append(a.bordureCouleur.Empty() ? "-" : a.bordureCouleur.Data());
						out.Append(',');
						if (a.bordureEpaisseur < 0.f)
							out.Append('-');
						else
							WriteNum(out, a.bordureEpaisseur);
					}
					out.Append('\n');
				}
				if (!n.transposeDe.Empty())
					Field(out, "transpose_de", n.transposeDe.Data());
				// ⚠️ C'EST LE MODE QUI DECIDE, PLUS LA VALEUR (correctif du 07/09).
				//    L'ancienne condition etait `rayonsDelies && !RayonsUniformes()`
				//    -- « la cle simple tant qu'elle suffit ». Elle ne suffisait pas :
				//    l'inspecteur delie ecrit `rayonsCoins` et JAMAIS `radius`, donc
				//    delier puis poser 12 aux quatre coins donnait quatre valeurs
				//    UNIFORMES avec un `radius` reste a 0. La condition retombait sur
				//    `else if (radius != 0.f)`, faux lui aussi : RIEN n'etait ecrit, et
				//    l'arrondi valait zero au rechargement.
				//
				// 🔴 TROIS SITES CONCORDANTS, CHACUN JUSTE ISOLEMENT -- c'est ce qui l'a
				//    fait tenir : la rangee de l'inspecteur, le predicat d'uniformite, et
				//    cette condition. Aucun des trois n'a l'air faux tout seul.
				//
				// ⚠️ ET AUCUN ALLER-RETOUR NE POUVAIT LE VOIR : le document ampute se
				//    reenregistre A L'IDENTIQUE. Un invariant de STABILITE ne detecte pas
				//    une perte STABLE -- le cas 50c exige donc la CONSERVATION en plus.
				//
				// ⚠️ RIEN NE CHANGE POUR LES DOCUMENTS EXISTANTS : `rayonsDelies` ne
				//    pouvait etre vrai, sur un fichier, que via une ligne `rayons` -- que
				//    l'ancienne regle n'ecrivait que non uniforme. Tout fichier ecrit
				//    avant ce jour se reenregistre donc octet pour octet.
				if (n.rayonsDelies) {
					out.Append("  rayons = ");
					for (uint32 ci = 0; ci < 4u; ++ci) {
						if (ci)
							out.Append(' ');
						WriteNum(out, n.rayonsCoins[ci]);
					}
					out.Append('\n');
				} else if (n.radius != 0.f) {
					out.Append("  rayon = ");
					WriteNum(out, n.radius);
					out.Append('\n');
				}
				// LA ROTATION ET LES DEUX MIROIRS : ecrits SEULEMENT s'ils ne
				// valent pas leur defaut -- meme discipline additive que
				// `position`, `shape` et les trois listes.
				if (n.echelleX != 1.f) {
					out.Append("  echelle_x = ");
					WriteNum(out, n.echelleX);
					out.Append("\n");
				}
				if (n.echelleY != 1.f) {
					out.Append("  echelle_y = ");
					WriteNum(out, n.echelleY);
					out.Append("\n");
				}
				if (n.rotation != 0.f) {
					out.Append("  rotation = ");
					WriteNum(out, n.rotation);
					out.Append('\n');
				}
				// L'INCLINAISON : meme discipline additive -- rien au fichier tant
				// qu'elle est nulle, donc un document d'avant se reenregistre octet
				// pour octet.
				if (n.inclinaisonX != 0.f) {
					out.Append("  inclinaison_x = ");
					WriteNum(out, n.inclinaisonX);
					out.Append('\n');
				}
				if (n.inclinaisonY != 0.f) {
					out.Append("  inclinaison_y = ");
					WriteNum(out, n.inclinaisonY);
					out.Append('\n');
				}
				// ⑤ L'OPACITÉ DU NŒUD : même discipline additive que la rotation et les
				//    miroirs -- rien tant qu'elle vaut son défaut.
				if (n.opacite != 100.f) {
					out.Append("  opacite = ");
					WriteNum(out, n.opacite);
					out.Append('\n');
				}
				if (!n.fusion.Empty())
					Field(out, "fusion", n.fusion.Data());
				if (n.miroirH)
					out.Append("  miroir_h = 1\n");
				if (n.miroirV)
					out.Append("  miroir_v = 1\n");
				// Additifs : rien n'est écrit tant qu'ils valent leur défaut,
				// donc un document d'avant se réenregistre OCTET POUR OCTET.
				if (n.verrouille)
					out.Append("  verrouille = 1\n");
				if (n.masque)
					out.Append("  masque = 1\n");
				if (n.refusPosition)
					out.Append("  refus_position = 1\n");
				if (n.refusRotation)
					out.Append("  refus_rotation = 1\n");
				if (n.refusEchelle)
					out.Append("  refus_echelle = 1\n");
				if (!n.genre.Empty()) {
					out.Append("  groupe = ");
					out.Append(n.genre);
					out.Append("\n");
				}
				// ── L'INSTANCE ET SES ÉCARTS (composants de document) ────
				// Mêmes règles additives : un nœud ordinaire n'écrit rien.
				// LES STYLES LIES (§15.15), additifs
				if (!n.styleCalque.Empty())
					Field(out, "style_calque", n.styleCalque.Data());
				if (!n.styleTexte.Empty())
					Field(out, "style_texte", n.styleTexte.Data());
				if (!n.instanceDe.Empty()) {
					out.Append("  instance = ");
					out.Append(n.instanceDe);
					out.Append('\n');
					if (n.ecarts != 0u) {
						out.Append("  ecarts = ");
						WriteNum(out, (float32)n.ecarts);
						out.Append('\n');
					}
				}
				if (n.borderW != 0.f && n.borders.Empty()) {
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
				// ③ LES DOSSIERS RECEMMENT OUVERTS DEPUIS CE DOCUMENT (05/09). Additifs :
				// rien tant qu'il n'y en a pas, et un lecteur qui ignore la cle lit le
				// document sans perdre un noeud. Ils VOYAGENT avec le fichier -- c'est la
				// demande de Rodolf : « dans cette session OU ce document ».
				for (uint32 ri = 0; ri < (uint32)dossiersRecents.Size(); ++ri) {
					out.Append("dossier_recent = ");
					out.Append(dossiersRecents[ri]);
					out.Append('\n');
				}
				// LES VARIABLES, additives : rien tant qu'il n'y en a pas
				if (!modeCourant.Empty()) {
					out.Append("mode = ");
					out.Append(modeCourant);
					out.Append('\n');
				}
				for (uint32 vi = 0; vi < (uint32)variables.Size(); ++vi) {
					const NkVariable &v = variables[vi];
					out.Append("variable = ");
					out.Append(v.cle);
					out.Append(' ');
					out.Append(v.valeur.Empty() ? "-" : v.valeur.Data());
					if (!v.nom.Empty()) {
						out.Append(" nom=\"");
						out.Append(v.nom);
						out.Append('\"');
					}
					for (uint32 mi = 0; mi < (uint32)v.parMode.Size(); ++mi) {
						out.Append(" @");
						out.Append(v.parMode[mi].mode);
						out.Append('=');
						out.Append(v.parMode[mi].valeur);
					}
					if (!v.inconnus.Empty()) {
						out.Append(' ');
						out.Append(v.inconnus);
					}
					out.Append('\n');
				}
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

				// ── LES DECLARATIONS DE COMPOSANTS, AVANT LES NOEUDS ────────────
				// ⚠️ AVANT, POUR LA MEME RAISON QUE LES METRIQUES : les nœuds les
				//    designent par leur cle, donc un fichier se lit de haut en bas
				//    sans jamais revenir en arriere.
				// ⚠️ ET RIEN N'EST ECRIT QUAND IL N'Y EN A PAS : un document sans
				//    composant ne gagne AUCUNE cle et se reenregistre octet pour
				//    octet. Meme discipline additive que `position`, `masque` et
				//    les trois listes.
				for (uint32 d = 0; d < (uint32)declarations.Size(); ++d) {
					const NkDeclarationComposant &dc = declarations[d];
					out.Append("\ncomposant ");
					WriteNum(out, (float32)d);
					out.Append('\n');
					Field(out, "auteur", dc.identite.auteur.Data());
					Field(out, "nom", dc.identite.nom.Data());
					Field(out, "version", dc.identite.version.Data());
					// `derive_de` n'existe QUE sur un fork : c'est le lien sans
					// lequel un fork serait indiscernable d'une creation.
					if (!dc.identite.deriveDe.Empty())
						Field(out, "derive_de", dc.identite.deriveDe.Data());
					for (uint32 k = 0; k < (uint32)dc.arbre.Size(); ++k)
						EcrireNoeud(out, dc.arbre[k], k, "dnoeud");
				}

				for (uint32 i = 0; i < (uint32)nodes.Size(); ++i)
					EcrireNoeud(out, nodes[i], i, "noeud");
				// LES STYLES (§15.15), en fin de fichier, additifs : rien tant qu'il n'y en
				// a pas. `style = <cle> genre=<g> nom="..."`, puis les listes de l'ensemble
				// par LES MEMES ecrivains que le noeud.
				for (uint32 si = 0; si < (uint32)styles.Size(); ++si) {
					const NkStyle &st = styles[si];
					out.Append("\nstyle = ");
					out.Append(st.cle);
					out.Append(" genre=");
					out.Append(st.genre.Empty() ? "calque" : st.genre.Data());
					if (!st.nom.Empty()) {
						out.Append(" nom=\"");
						out.Append(st.nom);
						out.Append('\"');
					}
					if (!st.inconnus.Empty()) {
						out.Append(' ');
						out.Append(st.inconnus);
					}
					out.Append('\n');
					const NkUINode &a = st.apparence;
					if (st.EstTexte()) {
						if (a.fontPx != 0.f) {
							out.Append("  police_px = ");
							WriteNum(out, a.fontPx);
							out.Append('\n');
						}
						if (a.fontWeight != 0.f) {
							out.Append("  graisse = ");
							WriteNum(out, a.fontWeight);
							out.Append('\n');
						}
						if (!a.textColor.Empty())
							Field(out, "couleur_texte", a.textColor.Data());
					} else {
						if (!a.fills.Empty())
							EcrireFonds(out, a.fills);
						else if (!a.fill.Empty())
							Field(out, "fond", a.fill.Data());
						if (!a.borders.Empty())
							EcrireBords(out, a.borders);
						else if (!a.borderColor.Empty())
							Field(out, "couleur_bord", a.borderColor.Data());
						EcrireEffets(out, a.effets);
					}
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
				variables.Clear(); // 05/09 : aucun des deux ne les vidait -- un chargement gardait les variables du document d'avant
				modeCourant = NkString();
				styles.Clear();

				bool sawHeader = false;
				bool inNode = false;
				NkVector<NkVector<int32>> childLists;
				/// Vrai entre `composant N` et le prochain `noeud` : les `dnoeud`
				/// et les cles d'identite appartiennent alors a la declaration.
				bool dansDecl = false;
				/// Vrai entre `style = ...` et le prochain bloc : les cles de listes vont
				/// a l'APPARENCE du style, par les memes lecteurs que le noeud (§15.15).
				bool dansStyle = false;
				/// Les listes d'enfants des nœuds de DECLARATION, dans l'ordre de
				/// lecture toutes declarations confondues. Meme role que
				/// `childLists` : `enfants` est la seule verite sur la structure,
				/// `parent` s'en deduit.
				NkVector<NkVector<int32>> declChildLists;
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
						FlushOverrides(pendingOverrides, inNode && !dansStyle);
						nodes.PushBack(NkUINode());
						childLists.PushBack(NkVector<int32>());
						inNode = true;
						dansDecl = false;
						dansStyle = false;
					} else if (StrEq(key, "composant") && !inNode) {
						// ── UNE DECLARATION S'OUVRE ──────────────────────────
						// ⚠️ `&& !inNode` EST LA MOITIE QUI COMPTE : `composant`
						//    est DEJA une cle de nœud (le composant DE CODE).
						//    Sans cette garde, chaque nœud du document ouvrirait
						//    une declaration vide -- les deux notions se
						//    marcheraient dessus au premier fichier lu, ce qui
						//    est exactement ce que le modele §15.1 interdit.
						declarations.PushBack(NkDeclarationComposant());
						dansDecl = true;
						dansStyle = false;
					} else if (StrEq(key, "dnoeud") && dansDecl) {
						FlushOverrides(pendingOverrides, inNode && !dansStyle);
						declarations[(uint32)declarations.Size() - 1].arbre.PushBack(NkUINode());
						declChildLists.PushBack(NkVector<int32>());
						inNode = true;
						dansStyle = false;
					} else if (StrEq(key, "style")) {
						// ── UN STYLE S'OUVRE (§15.15) : `<cle> genre=<g> nom="..." [inconnu]` ;
						//    les lignes qui suivent (fond_i, bord_i, effet_i, police_px,
						//    graisse, couleur_texte) vont a son apparence par les lecteurs du noeud
						FlushOverrides(pendingOverrides, inNode && !dansStyle);
						NkStyle st;
						const char *q = val;
						char mot[256];
						bool premier = true;
						while (*q) {
							while (*q == ' ')
								++q;
							if (!*q)
								break;
							uint32 z = 0;
							if (StrStartsWith(q, "nom=\"")) {
								q += 5;
								while (*q && *q != '\"' && z + 1 < (uint32)sizeof(mot))
									mot[z++] = *q++;
								if (*q == '\"')
									++q;
								mot[z] = '\0';
								st.nom = NkString(mot);
								continue;
							}
							while (*q && *q != ' ' && z + 1 < (uint32)sizeof(mot))
								mot[z++] = *q++;
							mot[z] = '\0';
							if (premier) {
								st.cle = NkString(mot);
								premier = false;
							} else if (StrStartsWith(mot, "genre="))
								st.genre = NkString(mot + 6);
							else {
								if (!st.inconnus.Empty())
									st.inconnus.Append(' ');
								st.inconnus.Append(mot);
							}
						}
						styles.PushBack(st);
						inNode = true;
						dansDecl = false;
						dansStyle = true;
					} else if (dansDecl && !inNode) {
						// ── L'IDENTITE DE LA DECLARATION EN COURS ────────────
						NkIdentiteComposant &id =
							declarations[(uint32)declarations.Size() - 1].identite;
						if (StrEq(key, "auteur"))
							id.auteur = NkString(val);
						else if (StrEq(key, "nom"))
							id.nom = NkString(val);
						else if (StrEq(key, "version"))
							id.version = NkString(val);
						else if (StrEq(key, "derive_de"))
							id.deriveDe = NkString(val);
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
						else if (StrEq(key, "mode"))
							modeCourant = NkString(val);
						else if (StrEq(key, "dossier_recent")) {
							// ③ sans doublon, et l'ORDRE DU FICHIER est celui de la liste
							bool vu = false;
							for (uint32 ri = 0; ri < (uint32)dossiersRecents.Size() && !vu; ++ri)
								vu = StrEq(dossiersRecents[ri].Data(), val);
							if (!vu)
								dossiersRecents.PushBack(NkString(val));
						}
						else if (StrEq(key, "variable")) {
							// `<cle> <valeur> [nom="..."] [@mode=valeur]... [inconnu]`
							NkVariable v;
							const char *q = val;
							char mot[256];
							auto suivant = [&q, &mot]() -> uint32 {
								while (*q == ' ')
									++q;
								uint32 z = 0;
								if (StrStartsWith(q, "nom=\"")) { // un nom entre guillemets, espaces compris
									while (*q && *q != '\"' && z + 1 < (uint32)sizeof(mot))
										mot[z++] = *q++;
									if (*q == '\"')
										mot[z++] = *q++;
									while (*q && *q != '\"' && z + 1 < (uint32)sizeof(mot))
										mot[z++] = *q++;
									if (*q == '\"')
										++q;
								} else
									while (*q && *q != ' ' && z + 1 < (uint32)sizeof(mot))
										mot[z++] = *q++;
								mot[z] = '\0';
								return z;
							};
							if (suivant() > 0)
								v.cle = NkString(mot);
							if (suivant() > 0 && !(mot[0] == '-' && mot[1] == '\0'))
								v.valeur = NkString(mot);
							while (suivant() > 0) {
								if (StrStartsWith(mot, "nom=\""))
									v.nom = NkString(mot + 5); // la guillemet fermante a ete consommee
								else if (mot[0] == '@') {
									NkValeurMode vm;
									uint32 e = 1;
									while (mot[e] && mot[e] != '=')
										++e;
									if (mot[e] == '=') {
										mot[e] = '\0';
										vm.mode = NkString(mot + 1);
										vm.valeur = NkString(mot + e + 1);
										v.parMode.PushBack(vm);
									}
								} else {
									if (!v.inconnus.Empty())
										v.inconnus.Append(' ');
									v.inconnus.Append(mot);
								}
							}
							if (!v.cle.Empty())
								variables.PushBack(v);
						}
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
						// ⚠️ LE MEME ANALYSEUR SERT LES DEUX ARBRES, et c'est le
						//    point : les nœuds d'une declaration se lisent avec le
						//    lecteur du document, pas avec une copie. Une seconde
						//    copie aurait diverge au premier champ ajoute.
						NkUINode &n =
							dansStyle ? styles[(uint32)styles.Size() - 1].apparence :
							dansDecl
								? declarations[(uint32)declarations.Size() - 1]
									  .arbre[(uint32)declarations[(uint32)declarations.Size() - 1]
												 .arbre.Size()
											 - 1]
								: nodes[(uint32)nodes.Size() - 1];
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
							ParseIntList(val, dansDecl
											 ? declChildLists[(uint32)declChildLists.Size() - 1]
											 : childLists[(uint32)childLists.Size() - 1]);
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
						else if (StrEq(key, "style_calque"))
							n.styleCalque = NkString(val);
						else if (StrEq(key, "style_texte"))
							n.styleTexte = NkString(val);
						else if (key[0] == 'f' && key[1] == 'o' && key[2] == 'n'
								 && key[3] == 'd' && key[4] == '_') {
							// `fond_<i> = couleur opacite visible`. ⚠️ L'INDICE DU
							//    NOM NE SERT PAS A RANGER : les lignes arrivent
							//    dans l'ordre du fichier, et on empile dans cet
							//    ordre. Se fier a l'indice obligerait a gerer les
							//    trous (`fond_1` puis `fond_3`) — un fichier ecrit
							//    a la main pourrait en avoir, et l'ordre du
							//    fichier reste la seule chose qu'on sache vraie.
							NkRemplissage f;
							const char *q = val;
							char coul[64];
							uint32 k = 0;
							while (*q && *q != ' ' && k + 1 < (uint32)sizeof(coul))
								coul[k++] = *q++;
							coul[k] = '\0';
							if (k > 0 && !(k == 1 && coul[0] == '-'))
								f.couleur = NkString(coul);
							while (*q == ' ')
								++q;
							if (*q) {
								f.opacite = ParseNum(q);
								while (*q && *q != ' ')
									++q;
								while (*q == ' ')
									++q;
								if (*q)
									f.visible = (*q == '1');
								while (*q && *q != ' ')
									++q;
								// les jetons additifs, dans n'importe quel ordre ; l'inconnu est garde
								char mot[256];
								for (;;) {
									while (*q == ' ')
										++q;
									if (!*q)
										break;
									uint32 z = 0;
									while (*q && *q != ' ' && z + 1 < (uint32)sizeof(mot))
										mot[z++] = *q++;
									mot[z] = '\0';
									if (StrStartsWith(mot, "genre="))
										f.genre = NkString(mot + 6);
									else if (StrStartsWith(mot, "cadrage="))
										f.cadrage = NkString(mot + 8);
									else if (StrStartsWith(mot, "image="))
										f.image = NkString(mot + 6);
									else if (StrStartsWith(mot, "rotation_image="))
										f.rotationImage = ParseNum(mot + 15);
									else if (StrStartsWith(mot, "crop=")) {
										// quatre nombres separes par des virgules ; un jeton tronque
										// garde ce qu'il a lu, le reste vaut son defaut
										float32 v[4] = {0.f, 0.f, 1.f, 1.f};
										const char *w = mot + 5;
										for (uint32 k = 0; k < 4u && *w; ++k) {
											v[k] = ParseNum(w);
											while (*w && *w != ',')
												++w;
											if (*w == ',')
												++w;
										}
										f.cropX = v[0];
										f.cropY = v[1];
										f.cropW = v[2];
										f.cropH = v[3];
									}
									else if (StrStartsWith(mot, "fusion="))
										f.fusion = NkString(mot + 7);
									else {
										if (!f.inconnus.Empty())
											f.inconnus.Append(' ');
										f.inconnus.Append(mot);
									}
								}
							}
							n.fills.PushBack(f);
						}
						else if (StrEq(key, "couleur_texte"))
							n.textColor = NkString(val);
						else if (StrEq(key, "couleur_bord"))
							n.borderColor = NkString(val);
						else if (key[0] == 'b' && key[1] == 'o' && key[2] == 'r'
								 && key[3] == 'd' && key[4] == '_') {
							// `bord_<i> = couleur opacite visible epaisseur position`.
							// Meme regle que `fond_<i>` : on empile dans L'ORDRE DU
							// FICHIER, l'indice du nom ne sert pas a ranger.
							NkBordure b;
							const char *q = val;
							auto motSuivant = [&q](char *dst, uint32 cap) {
								uint32 k = 0;
								while (*q && *q != ' ' && k + 1 < cap)
									dst[k++] = *q++;
								dst[k] = '\0';
								while (*q == ' ')
									++q;
								return k;
							};
							char mot[64];
							if (motSuivant(mot, (uint32)sizeof(mot)) > 0
								&& !(mot[0] == '-' && mot[1] == '\0'))
								b.couleur = NkString(mot);
							if (motSuivant(mot, (uint32)sizeof(mot)) > 0)
								b.opacite = ParseNum(mot);
							if (motSuivant(mot, (uint32)sizeof(mot)) > 0)
								b.visible = (mot[0] == '1');
							if (motSuivant(mot, (uint32)sizeof(mot)) > 0)
								b.epaisseur = ParseNum(mot);
							if (motSuivant(mot, (uint32)sizeof(mot)) > 0)
								b.position = NkParseBordurePos(mot);
							// les jetons additifs, dans n'importe quel ordre ; l'inconnu est garde
							while (motSuivant(mot, (uint32)sizeof(mot)) > 0) {
								if (StrStartsWith(mot, "cotes=")) {
									const char *q2 = mot + 6;
									for (uint32 k = 0; k < 4u; ++k) {
										b.cotes[k] = ParseNum(q2);
										while (*q2 && *q2 != ',')
											++q2;
										if (*q2 == ',')
											++q2;
									}
								} else if (StrStartsWith(mot, "jointure="))
									b.jointure = NkString(mot + 9);
								else if (StrStartsWith(mot, "extremite="))
									b.extremite = NkString(mot + 10);
								else {
									if (!b.inconnus.Empty())
										b.inconnus.Append(' ');
									b.inconnus.Append(mot);
								}
							}
							n.borders.PushBack(b);
						}
						else if (key[0] == 's' && key[1] == 'o' && key[2] == 'm' && key[3] == 'm'
								 && key[4] == 'e' && key[5] == 't' && key[6] == '_') {
							// `sommet_<i> = x y [rayon]`, unitaire. Empile dans l'ordre
							// du fichier, comme les trois listes.
							// ⚠️ LE TROISIEME CHAMP EST FACULTATIF : un fichier ecrit
							//    avant l'arrondi par sommet n'en a pas, et il doit se
							//    relire sans que le rayon prenne une valeur inventee.
							//    Le defaut du champ (0 = coin vif) fait le travail --
							//    ce qui n'est pas ecrit n'est pas devine.
							NkPoint2 pt;
							const char *q = val;
							auto motSuiv = [&q]() {
								while (*q && *q != ' ')
									++q;
								while (*q == ' ')
									++q;
							};
							pt.x = ParseNum(q);
							motSuiv();
							if (*q) {
								pt.y = ParseNum(q);
								motSuiv();
								if (*q) {
									pt.rayon = ParseNum(q);
									motSuiv();
									// ⚠️ LES CINQ CHAMPS DE TANGENTE SE LISENT ENSEMBLE OU
									//    PAS DU TOUT. Les lire un par un laisserait un
									//    sommet à moitié courbe si la ligne est tronquée --
									//    une forme dont une tangente sur deux existe est
									//    pire qu'une forme sans tangente : elle a l'air
									//    d'une courbe et se comporte comme un angle.
									if (*q) {
										pt.ex = ParseNum(q);
										motSuiv();
										pt.ey = ParseNum(q);
										motSuiv();
										pt.sx = ParseNum(q);
										motSuiv();
										pt.sy = ParseNum(q);
										motSuiv();
										const float32 li = ParseNum(q);
										// une liaison hors des trois connues retombe sur
										// l'angle : ce qui n'est pas compris ne s'invente pas.
										pt.liaison = (li >= 1.f && li <= 3.f)
														 ? (nkentseu::uint8)li
														 : (nkentseu::uint8)NkPoint2::LiaisonDroit;
									}
								}
							}
							n.sommets.PushBack(pt);
						}
						else if (key[0] == 'e' && key[1] == 'f' && key[2] == 'f' && key[3] == 'e'
								 && key[4] == 't' && key[5] == '_') {
							// Meme patron que `fond_` et `bord_` : on empile dans
							// l'ordre du fichier, l'indice du nom ne range rien.
							NkEffet e;
							const char *q = val;
							auto mot = [&q](char *dst, uint32 cap) {
								uint32 k = 0;
								while (*q && *q != ' ' && k + 1 < cap)
									dst[k++] = *q++;
								dst[k] = '\0';
								while (*q == ' ')
									++q;
								return k;
							};
							char m[64];
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.type = NkParseEffetType(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.x = ParseNum(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.y = ParseNum(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.flou = ParseNum(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.etendue = ParseNum(m);
							if (mot(m, (uint32)sizeof(m)) > 0 && !(m[0] == '-' && m[1] == '\0'))
								e.couleur = NkString(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.opacite = ParseNum(m);
							if (mot(m, (uint32)sizeof(m)) > 0)
								e.visible = (m[0] == '1');
							n.effets.PushBack(e);
						}
						else if (StrEq(key, "texte_aligne"))
							n.alignText = NkString(val);
						else if (StrEq(key, "cible"))
							n.target = NkString(val);
						// `degrade_<i> = <type> <angle> <pos>:<coul> ...`
						//
						// ⚠️ LE DEGRADE SE RATTACHE AU DERNIER REMPLISSAGE LU, et
						//    non a l'indice de son nom : les lignes arrivent dans
						//    l'ordre du fichier, `fond_<i>` empile, `degrade_<i>`
						//    suit. Se fier a l'indice obligerait a gerer les trous
						//    -- et c'est deja la regle ecrite pour `fond_<i>`.
						//    *Deux regles differentes pour deux cles jumelles,
						//    c'est une divergence programmee.*
						//
						// ⚠️ UN TYPE INCONNU EST CONSERVE TEL QUEL. On ne le
						//    traduit pas, on ne le refuse pas : un « conique »
						//    ecrit par une version future revient intact d'un
						//    aller-retour, meme si celle-ci ne sait pas le
						//    peindre. Refuser aurait perdu le fichier ; traduire
						//    aurait menti sur son contenu.
						else if (NkString(key).StartsWith("degrade_")) {
							if (n.fills.Empty())
								continue;
							NkDegrade g;
							const char *q = val;
							char mot[64];
							uint32 k = 0;
							while (*q && *q != ' ' && k + 1 < (uint32)sizeof(mot))
								mot[k++] = *q++;
							mot[k] = '\0';
							if (k > 0)
								g.type = NkString(mot);
							while (*q == ' ')
								++q;
							if (*q) {
								g.angle = ParseNum(q);
								while (*q && *q != ' ')
									++q;
							}
							while (*q) {
								while (*q == ' ')
									++q;
								if (!*q)
									break;
								if ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z')) {
									// ① `cle=valeur` : o= (origine), r= (rayons), sinon PRESERVE tel quel
									k = 0;
									while (*q && *q != ' ' && k + 1 < (uint32)sizeof(mot))
										mot[k++] = *q++;
									mot[k] = '\0';
									if ((mot[0] == 'o' || mot[0] == 'r') && mot[1] == '=') {
										float32 a = 0.5f, b = 0.5f;
										a = ParseNum(mot + 2);
										for (uint32 z = 2; z < k; ++z)
											if (mot[z] == ',') {
												b = ParseNum(mot + z + 1);
												break;
											}
										if (mot[0] == 'o') {
											g.origineX = a;
											g.origineY = b;
										} else {
											g.rayonX = a;
											g.rayonY = b;
										}
									} else {
										if (!g.inconnus.Empty())
											g.inconnus.Append(' ');
										g.inconnus.Append(mot);
									}
									continue;
								}
								NkArretDegrade ar;
								ar.position = ParseNum(q);
								while (*q && *q != ':' && *q != ' ')
									++q;
								if (*q == ':') {
									++q;
									k = 0;
									while (*q && *q != ' ' && k + 1 < (uint32)sizeof(mot))
										mot[k++] = *q++;
									mot[k] = '\0';
									// `<coul>` ou `<coul>:<opacite>` : additive
									for (uint32 z = 0; z < k; ++z)
										if (mot[z] == ':') {
											mot[z] = '\0';
											ar.opacite = ParseNum(mot + z + 1);
											k = z;
											break;
										}
									if (k > 0 && !(k == 1 && mot[0] == '-'))
										ar.couleur = NkString(mot);
								}
								g.arrets.PushBack(ar);
							}
							n.fills[(uint32)n.fills.Size() - 1].degrade = g;
						}
						else if (StrEq(key, "unite"))
							n.targetUnit = NkString(val);
						// `apparence_<Etat> = fond radius opacite` ; « - » = herite ;
						// `opacite` = celle du NŒUD (CALQUE), sens tranche le 11/09.
						// ⚠️ ON N'EXIGE PAS QUE L'ETAT SOIT CONNU A LA LECTURE :
						//    un document ecrit par une version qui aurait un
						//    SEPTIEME etat doit se relire sans perdre sa ligne.
						//    C'est l'interface qui n'affiche que la table fermee.
						else if (NkString(key).StartsWith("apparence_")) {
							NkApparenceEtat a;
							a.etat = NkString(key + 10);
							const char *q = val;
							char champ[64];
							for (int32 c = 0; c < 3; ++c) {
								uint32 k = 0;
								while (*q && *q != ' ' && k + 1 < (uint32)sizeof(champ))
									champ[k++] = *q++;
								champ[k] = '\0';
								const bool herite = (k == 0) || (k == 1 && champ[0] == '-');
								if (!herite) {
									if (c == 0)
										a.fond = NkString(champ);
									else if (c == 1)
										a.radius = ParseNum(champ);
									else
										a.opacite = ParseNum(champ);
								}
								while (*q == ' ')
									++q;
							}
							// LES JETONS NOMMES, dans n'importe quel ordre, inconnus ignores :
							// `texte=#rrggbb`, `ombre=<flou>,<opacite>` (« - » = herite).
							while (*q) {
								uint32 k = 0;
								while (*q && *q != ' ' && k + 1 < (uint32)sizeof(champ))
									champ[k++] = *q++;
								champ[k] = '\0';
								while (*q == ' ')
									++q;
								if (NkString(champ).StartsWith("texte="))
									a.couleurTexte = NkString(champ + 6);
								else if (NkString(champ).StartsWith("ombre=")) {
									const char *v = champ + 6;
									char part[32];
									uint32 m = 0;
									while (*v && *v != ',' && m + 1 < (uint32)sizeof(part))
										part[m++] = *v++;
									part[m] = '\0';
									if (!(m == 0 || (m == 1 && part[0] == '-')))
										a.ombreFlou = ParseNum(part);
									if (*v == ',')
										++v;
									if (*v && !(*v == '-' && v[1] == '\0'))
										a.ombreOpacite = ParseNum(v);
								} else if (NkString(champ).StartsWith("bordure=")) {
									// `bordure=<hex|->,<epaisseur|->`
									const char *v = champ + 8;
									char part[32];
									uint32 m = 0;
									while (*v && *v != ',' && m + 1 < (uint32)sizeof(part))
										part[m++] = *v++;
									part[m] = '\0';
									if (!(m == 0 || (m == 1 && part[0] == '-')))
										a.bordureCouleur = NkString(part);
									if (*v == ',')
										++v;
									if (*v && !(*v == '-' && v[1] == '\0'))
										a.bordureEpaisseur = ParseNum(v);
								}
							}
							if (!a.etat.Empty())
								n.apparences.PushBack(a);
						}
						else if (StrEq(key, "transpose_de"))
							n.transposeDe = NkString(val);
						// `rayons = hg hd bd bg` — l'ordre horaire depuis le
						// haut-gauche, celui de CSS et de Lunacy.
						else if (StrEq(key, "rayons")) {
							n.rayonsDelies = true;
							const char *q = val;
							for (uint32 ci = 0; ci < 4u; ++ci) {
								while (*q == ' ')
									++q;
								if (!*q)
									break;
								n.rayonsCoins[ci] = ParseNum(q);
								while (*q && *q != ' ')
									++q;
							}
						}
						else if (StrEq(key, "rayon"))
							n.radius = ParseNum(val);
						else if (StrEq(key, "rotation"))
							n.rotation = ParseNum(val);
						else if (StrEq(key, "fusion"))
							n.fusion = NkString(val);
						else if (StrEq(key, "opacite"))
							n.opacite = ParseNum(val);
						else if (StrEq(key, "miroir_h"))
							n.miroirH = (val[0] == '1');
						else if (StrEq(key, "miroir_v"))
							n.miroirV = (val[0] == '1');
						else if (StrEq(key, "inclinaison_x"))
							n.inclinaisonX = ParseNum(val);
						else if (StrEq(key, "inclinaison_y"))
							n.inclinaisonY = ParseNum(val);
						else if (StrEq(key, "echelle_x"))
							n.echelleX = NkEchelleSaine(ParseNum(val));
						else if (StrEq(key, "echelle_y"))
							n.echelleY = NkEchelleSaine(ParseNum(val));
						else if (StrEq(key, "verrouille"))
							n.verrouille = (val[0] == '1');
						else if (StrEq(key, "masque"))
							n.masque = (val[0] == '1');
						else if (StrEq(key, "refus_position"))
							n.refusPosition = (val[0] == '1');
						else if (StrEq(key, "refus_rotation"))
							n.refusRotation = (val[0] == '1');
						else if (StrEq(key, "refus_echelle"))
							n.refusEchelle = (val[0] == '1');
						else if (StrEq(key, "groupe"))
							n.genre = NkString(val); // genre libre, inconnu preserve
						else if (StrEq(key, "instance"))
							n.instanceDe = NkString(val);
						else if (StrEq(key, "ecarts"))
							n.ecarts = (nkentseu::uint32)ParseNum(val);
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
				FlushOverrides(pendingOverrides, inNode && !dansStyle);

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
				// ── LES ARBRES DES DECLARATIONS, MEME REGLE ─────────────────
				// `declChildLists` court sur TOUTES les declarations dans l'ordre
				// de lecture : on le reparcourt declaration par declaration, avec
				// un decalage. Les indices d'`enfants` sont INTERNES a l'arbre.
				{
					uint32 base = 0;
					for (uint32 d = 0; d < (uint32)declarations.Size(); ++d) {
						NkVector<NkUINode> &a = declarations[d].arbre;
						for (uint32 i = 0; i < (uint32)a.Size(); ++i) {
							const uint32 li = base + i;
							if (li >= (uint32)declChildLists.Size())
								break;
							for (uint32 c = 0; c < (uint32)declChildLists[li].Size(); ++c) {
								const int32 kid = declChildLists[li][c];
								if (kid <= 0 || (uint32)kid >= (uint32)a.Size()
									|| a[(uint32)kid].parent >= 0)
									return false; // enfant inconnu, racine reparentee, ou deux parents
								a[i].children.PushBack(kid);
								a[(uint32)kid].parent = (int32)i;
							}
						}
						for (uint32 i = 1; i < (uint32)a.Size(); ++i)
							if (a[i].parent < 0)
								return false; // nœud orphelin dans une declaration
						base += (uint32)a.Size();
					}
				}
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
						// ⚠️ LE CHEMIN FRERE DE `CopierSousArbre`, traité au même
						//    moment : une page transposée en mobile garde
						//    l'orientation de ses éléments. Écrit là-bas
						//    seulement, une flèche retournée serait revenue à
						//    l'endroit dans la version mobile — sans un mot.
						d.rotation = s.rotation;
					d.inclinaisonX = s.inclinaisonX;
					d.inclinaisonY = s.inclinaisonY;
					d.echelleX = s.echelleX;
					d.echelleY = s.echelleY;
					d.refusPosition = s.refusPosition;
					d.refusRotation = s.refusRotation;
					d.refusEchelle = s.refusEchelle;
					d.genre = s.genre;
						d.miroirH = s.miroirH;
						d.miroirV = s.miroirV;
						// 📌 CONSTAT, NON CORRIGÉ ET NON ÉLARGI (2026-09-01) :
						//    cette copie-ci ne transporte NI `fills`, NI `borders`,
						//    NI `effets`, NI `sommets` — quatre listes que
						//    `CopierSousArbre`, lui, transporte. C'est antérieur à
						//    ce chantier et ça se voit en comparant les deux blocs.
						//    Je le NOTE plutôt que de le prendre au vol : le
						//    corriger change ce que produit « Generate Mobile », ça
						//    demande sa propre mesure et son propre cas de recette.
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

	// ═══════════════════════════════════════════════════════════════════════════
	//  VERROUILLER / MASQUER — LES DEUX PORTES, ET ELLES REMONTENT LES ANCETRES
	// ═══════════════════════════════════════════════════════════════════════════
	//  Vague 2, source `/layers`. `masque` retire du DESSIN et du POINTAGE ;
	//  `verrouille` retire du SEUL POINTAGE.
	//
	//  ⚠️ ECRITES ICI, DANS `Document.h`, ET PAS DANS `Selection.h` OU JE LES
	//     AVAIS MISES D'ABORD. Le compilateur a tranche : `Selection.h` inclut
	//     `Transfo.h`, donc le pointage de base ne pouvait pas les voir. Elles ne
	//     parlent que du DOCUMENT -- ni disposition, ni ecran, ni souris -- donc
	//     leur place est la couche du dessous, celle que TOUS leurs appelants
	//     incluent deja. *L'erreur d'etage s'est vue en une compilation ; elle
	//     aurait pu se voir en se demandant de quoi ces fonctions parlent.*
	//
	//  ⚠️ LUES PARTOUT, JAMAIS `n.masque` EN DIRECT. Deux raisons, et la seconde
	//     est celle que ce depot a deja payee :
	//       1. l'HERITAGE. Masquer un groupe doit masquer son contenu ; le
	//          drapeau ne vit que sur l'ancetre, donc un test direct sur l'enfant
	//          repondrait « visible » et le peindrait quand meme ;
	//       2. il y a DEUX sites de pointage plus le peintre. Trois copies de la
	//          meme regle, c'est trois occasions de diverger -- exactement ce que
	//          ce chantier a paye avec « le peintre a ete ecrit deux fois ».
	//
	//  ⚠️ ET LA BORNE DE PROFONDEUR N'EST PAS DECORATIVE : `parent` est DERIVE au
	//     chargement, et un document mal forme (cycle) figerait l'application
	//     dans la boucle de rendu. On borne a 64 plutot que de supposer l'arbre
	//     sain.

	/// Ce nœud se PEINT-il ? Faux si lui ou un de ses ancetres est masque.
	inline bool NkNoeudVisible(const NkUIDocument &doc, nkentseu::int32 i) {
		nkentseu::int32 garde = 0;
		for (nkentseu::int32 k = i; k >= 0 && ++garde < 64;
			 k = doc.nodes[(nkentseu::uint32)k].parent) {
			if (!doc.IsValidIndex(k))
				return false;
			if (doc.nodes[(nkentseu::uint32)k].masque)
				return false;
		}
		return true;
	}

	/// Ce nœud s'ATTRAPE-t-il a la souris ? Faux s'il est invisible, ou si lui ou
	/// un de ses ancetres est verrouille.
	/// ⚠️ L'INVISIBLE EST INCLUS, ET C'EST TOUT LE POINT : *un nœud masque qu'on
	///    peut encore attraper est pire que pas de masquage du tout* -- on croit
	///    l'objet parti, on clique « dans le vide », et on deplace ce qu'on ne
	///    voit pas.
	inline bool NkNoeudAttrapable(const NkUIDocument &doc, nkentseu::int32 i) {
		nkentseu::int32 garde = 0;
		for (nkentseu::int32 k = i; k >= 0 && ++garde < 64;
			 k = doc.nodes[(nkentseu::uint32)k].parent) {
			if (!doc.IsValidIndex(k))
				return false;
			const NkUINode &n = doc.nodes[(nkentseu::uint32)k];
			if (n.masque || n.verrouille)
				return false;
		}
		return true;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  CE QUE LA HIERARCHIE MONTRE, ET CE QU'UN CLIC SUR L'ICONE ECRIT
	// ═══════════════════════════════════════════════════════════════════════════
	//  Deux fonctions minuscules, extraites pour UNE raison : ce sont les deux
	//  seuls endroits ou l'oeil et le cadenas peuvent se tromper, et aucun des
	//  deux ne se voit a la relecture.

	/// LES DRAPEAUX **EFFECTIFS** D'UNE RANGEE, plus « viennent-ils d'un
	/// ancetre ? ». Le composant d'arbre exige des drapeaux deja composes avec
	/// les ancetres (il ne connait pas notre semantique d'heritage), et il a
	/// besoin de `herite` pour peindre l'icone ATTENUEE et REFUSER le clic --
	/// sans quoi le refus parait inexplicable. NK3DModeler a paye cette lecon a
	/// l'usage.
	inline void NkDrapeauxArbre(const NkUIDocument &doc, nkentseu::int32 i, bool &cache,
								bool &verrouille, bool &herite) {
		cache = !NkNoeudVisible(doc, i);
		verrouille = !NkNoeudAttrapable(doc, i);
		herite = false;
		if (!doc.IsValidIndex(i))
			return;
		const NkUINode &n = doc.nodes[(nkentseu::uint32)i];
		// herite = l'etat effectif est vrai SANS que le noeud porte lui-meme le
		// drapeau qui l'expliquerait.
		herite = (cache && !n.masque) || (verrouille && !n.verrouille && !n.masque);
	}

	/// CE QU'UN CLIC SUR L'ICONE ECRIT DANS LE MODELE.
	///
	/// 🔴 LES DEUX ICONES N'ONT PAS LA MEME POLARITE, ET C'EST LE PIEGE ENTIER DE
	///    CE BRANCHEMENT. L'oeil du composant dit « **VISIBLE** » ; notre champ
	///    dit « **MASQUE** ». La valeur s'INVERSE donc pour l'oeil, et pas pour le
	///    cadenas. Ecrit au site d'appel, ce genre de detail passe la relecture
	///    puis fait exactement le contraire a l'ecran -- et le defaut se lit comme
	///    « l'icone ne marche pas », jamais comme « le sens est inverse ». Ici, un
	///    cas de recette le tient.
	///
	/// @param oeil vrai pour l'oeil (`valeur` = VISIBLE), faux pour le cadenas
	///             (`valeur` = VERROUILLE).
	inline void NkPoserDrapeauArbre(NkUINode &n, bool oeil, bool valeur) {
		if (oeil)
			n.masque = !valeur;
		else
			n.verrouille = valeur;
	}

	inline bool NkEstGroupe(const NkUINode &n) {
		return NkComponentDecl::StrEq(n.shape.Data(), "frame") || !n.genre.Empty()
			   || !n.children.Empty();
	}
	inline bool NkFormeOuverte(const NkUINode &n) {
		return NkComponentDecl::StrEq(n.shape.Data(), "line")
			   || NkComponentDecl::StrEq(n.shape.Data(), "line_up");
	}

	/// LA MIGRATION (Rodolf, 03/09 : ses six rect a enfants sont illegaux).
	/// Un ENVELOPPEMENT, pas un re-etiquetage : un graphique qui porte des
	/// enfants garde son dessin (fond, bordure) et devient la PREMIERE feuille
	/// d'un groupe neuf qui prend sa place, sa boite, son etiquette, son role,
	/// sa rotation, ses miroirs, son echelle ; ses anciens enfants suivent dans
	/// le groupe, aux memes positions relatives (la boite du groupe est celle du
	/// graphique, a l'origine). Rien ne bouge a l'ecran : sonde 48 (peintre
	/// enregistreur, commande pour commande). Rend le nombre d'enveloppements ;
	/// `enveloppes` recoit les indices des groupes crees.
	inline uint32 NkEnvelopperGraphiques(NkUIDocument &doc, NkVector<int32> *enveloppes) {
		uint32 nb = 0;
		const uint32 n0 = (uint32)doc.nodes.Size(); // les groupes crees s'ajoutent apres
		for (uint32 i = 1; i < n0; ++i) {
			NkUINode &g = doc.nodes[i];
			if (g.children.Empty() || !g.genre.Empty() || g.shape.Empty()
				|| NkComponentDecl::StrEq(g.shape.Data(), "frame"))
				continue; // un groupe, une planche, ou un noeud sans forme : rien a faire
			const int32 parent = g.parent;
			if (!doc.IsValidIndex(parent))
				continue;
			// le rang du graphique chez son parent : le groupe prend cette place
			int32 rang = -1;
			const NkVector<int32> &freres = doc.nodes[(uint32)parent].children;
			for (uint32 k = 0; k < (uint32)freres.Size(); ++k)
				if (freres[k] == (int32)i)
					rang = (int32)k;
			const int32 gi = doc.AddChild(parent, "", NkAuthor::Humain);
			if (!doc.IsValidIndex(gi))
				continue;
			NkVector<int32> anciens = doc.nodes[i].children; // copie : on va les deplacer
			{
				NkUINode &G = doc.nodes[(uint32)gi];
				const NkUINode &f = doc.nodes[i];
				G.label = f.label;
				G.genre = NkString("simple");
				G.layout = f.layout;
				G.posX = f.posX;
				G.posY = f.posY;
				G.width = f.width;
				G.height = f.height;
				G.rotation = f.rotation;
				G.miroirH = f.miroirH;
				G.miroirV = f.miroirV;
				G.echelleX = f.echelleX;
				G.echelleY = f.echelleY;
				G.role = f.role;
				G.verrouille = f.verrouille;
				G.masque = f.masque;
			}
			if (rang >= 0)
				doc.MoveChild(gi, rang);
			// le graphique devient la premiere feuille du groupe, a l'origine, droit
			doc.Reparent((int32)i, gi, 0);
			{
				NkUINode &f = doc.nodes[i];
				f.posX = 0.f;
				f.posY = 0.f;
				f.rotation = 0.f;
				f.miroirH = false;
				f.miroirV = false;
				f.echelleX = 1.f;
				f.echelleY = 1.f;
				f.role = NkString();
				f.verrouille = false;
				f.masque = false;
				if (!f.label.Empty())
					f.label.Append(" (fond)");
			}
			// ses anciens enfants suivent, dans l'ordre, aux memes positions relatives
			for (uint32 k = 0; k < (uint32)anciens.Size(); ++k)
				doc.Reparent(anciens[k], gi, -1);
			if (enveloppes)
				enveloppes->PushBack(gi);
			++nb;
		}
		return nb;
	}
	// ── ⑥ (07/09) LE CYCLE DES MÉTRIQUES NOMMÉES ────────────────────────
	/// La métrique SUIVANTE dans le cycle « aucune → m0 → m1 → … → aucune ».
	///
	/// 🔴 ELLE VIT ICI, A COTE DE LA TABLE QU'ELLE PARCOURT, et pas dans
	///    l'inspecteur -- pour la même raison que `NkAlignementLuParLeSolveur` vit a
	///    coté du solveur : le fait appartient au document. Et pour une raison de
	///    plus, payee le jour meme : ecrite dans le panneau, elle etait hors de
	///    portee du banc, qui en avait donc REIMPLANTE une copie -- et mesurait sa
	///    copie. *Une sonde qui reproduit la strategie du code mesure autre chose
	///    que ce code.* Un seul site, appele par le dessin ET par la sonde.
	///
	/// ⚠️ LA LISTE EST CELLE DU DOCUMENT, jamais une table ecrite ailleurs : une
	///    liste en dur aurait propose des noms que `metrics` ne connait pas, et
	///    `Metric()` aurait rendu zero **sans le dire**.
	/// ⚠️ « AUCUNE » EST DANS LE CYCLE, et ce n'est pas une commodite : sans elle,
	///    poser un nom serait irreversible depuis la rangee. Un geste qui ne se
	///    defait pas par le meme chemin n'est pas un reglage.
	inline const char *NkMetriqueSuivante(const NkUIDocument &doc, const char *courant) {
		const uint32 n = (uint32)doc.metrics.Size();
		if (n == 0u)
			return ""; // aucun nom disponible : le cycle ne peut que rester vide
		if (!courant || !*courant)
			return doc.metrics[0].name.Data();
		for (uint32 i = 0; i < n; ++i)
			if (NkComponentDecl::StrEq(doc.metrics[i].name.Data(), courant))
				return (i + 1u < n) ? doc.metrics[i + 1u].name.Data() : "";
		// Un nom que la table ne porte plus (metrique retiree) : on repart du premier
		// plutot que de rester coince sur un nom mort.
		return doc.metrics[0].name.Data();
	}

} // namespace nkuidesign
