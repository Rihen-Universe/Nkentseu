#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentLayout.h
// @Brief   L'ARBRE de sous-elements d'un composant, et comment il se dimensionne.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (Rodolf, 2026-08-18, soir)
// =============================================================================
//  Deux de ses quatre ajouts a la forme de declaration vivent ici, et ils n'en
//  font qu'un :
//
//  1. **UNE APPARENCE EST UN ARBRE, pas une image plate.** Une interface
//     complete est un composant qui en contient d'autres. **Meme mecanisme aux
//     deux echelles** : le meme `NkElementDecl` decrit le pied d'une carte et le
//     panneau qui contient tout le navigateur. Un noeud qui porte un nom de
//     COMPOSANT (`component`) est le point ou l'echelle change -- et c'est le
//     seul.
//
//  2. **LA TAILLE SE DECLARE, LA POSITION SE CALCULE.** Mot pour mot :
//     « grace a la definition de s'il est extensible ou garde sa taille, on peut
//     definir qu'il soit responsive ou non ; ce sont ces proprietes et celles de
//     ses parents qui definissent tout ca. »
//
//     L'ENFANT declare comment il occupe la place (fige / ajuste au contenu /
//     fraction / a poids / extensible, avec min et max) ; le PARENT declare son
//     agencement (ligne, colonne, grille, ancrage). **La position devient un
//     resultat, plus une donnee.**
//
// =============================================================================
//  LA REGLE QUI TIENT TOUT LE FICHIER, ET LA SEULE A RETENIR
// =============================================================================
//  **AUCUN `x`, AUCUN `y` N'APPARAIT DANS CE FICHIER, ET AUCUN NE DOIT Y
//  ENTRER.** Le jour ou un champ de position est ajoute a `NkElementDecl`,
//  l'ajout de Rodolf est defait : l'outil se remettra a ecrire des coordonnees,
//  et le responsive redeviendra impossible. Si une apparence semble exiger une
//  coordonnee, c'est qu'il manque un mode de taille ou un agencement -- c'est
//  CELA qu'il faut ajouter, jamais un `x`.
//
//  Le resolveur qui transforme cette declaration en rectangles est
//  `NkLayoutSolve.h`. Il fait partie du CONTRAT, pas du dessin : sans lui,
//  « extensible » et « a poids » n'auraient pas de sens defini, et deux agents
//  les implementeraient de deux facons differentes.
//
// =============================================================================
//  OU AJOUTER QUOI -- le point d'extension, nomme et visible
// =============================================================================
//  - un mode de taille nouveau        -> `NkSizeMode`, **A LA FIN** (append-only)
//  - un agencement nouveau            -> `NkLayoutKind`, **A LA FIN** (append-only)
//  - un sous-element dans un composant -> une LIGNE dans la table `kElements` de
//    ce composant, avec le NOM de son parent. Rien d'autre a toucher.
//
//  APPEND-ONLY, meme raison que partout ailleurs dans cette forme : ces valeurs
//  finiront ecrites dans un fichier de description, et un insert au milieu
//  ferait relire le mauvais mode a toutes les descriptions deja enregistrees.
//
// EN-TETE PUR : `NKCore/NkTypes.h` et rien d'autre. Le banc de neutralite
//   (`ROADMAP.md` section 5) le couvre.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ── COMMENT UN ENFANT OCCUPE SA PLACE ───────────────────────────────────
		// Les trois mots de Rodolf -- « fige / extensible / a poids » -- sont les
		// trois premiers ci-dessous ; `Content` et `Fraction` s'y ajoutent parce que
		// deux besoins DEJA ECRITS dans le depot les exigent : un libelle s'ajuste a
		// son texte, et `tree_width` du navigateur de contenu est **deja** declaree
		// comme une fraction (0.18). La forme doit savoir le dire, sinon
		// l'application recalcule ce nombre a la main et on est revenu au depart.
		enum class NkSizeMode : uint8 {
			Fixed = 0, ///< garde sa taille : `value` pixels logiques, ou `valueMetric`
			Content,   ///< s'ajuste a son contenu (mesure fournie par l'hote, cf. NkLayoutSolve.h)
			Fraction,  ///< `value` = fraction de la place du parent (0..1)
			Weight,	   ///< a poids : se partage le RESTE au prorata de `value`
			Expand,	   ///< extensible -- exactement `Weight` de poids 1. Deux orthographes,
					   ///< UN mecanisme : le resolveur traite `Expand` comme `Weight(1)`.
					   ///< Garde parce que c'est le mot de Rodolf, et parce qu'il se lit
					   ///< mieux que « poids 1 » dans une table ecrite a la main.
			Count
		};

		// ── UNE TAILLE SUR UN AXE ───────────────────────────────────────────────
		// POURQUOI `valueMetric` EXISTE, ET POURQUOI `minVal`/`maxVal` SONT DES
		// NOMBRES -- ce n'est pas une inconsequence, c'est une frontiere :
		//
		//   - une TAILLE est du STYLE : elle change avec le theme, la densite, le
		//     gout de l'application. Elle se NOMME (`"row_h"`), exactement comme une
		//     couleur se nomme par un role ;
		//   - une BORNE est une garantie STRUCTURELLE : « en dessous de 24 px le
		//     texte n'est plus lisible ». Elle ne doit PAS dependre du theme, sinon
		//     un theme mal regle casse la lisibilite sans que rien ne le signale.
		//     Elle reste donc un nombre, ecrit ici, une fois.
		//
		// Si `valueMetric` est non vide, il PRIME sur `value` : un seul point de
		// verite, et c'est le nom.
		struct NkSizeDecl {
				NkSizeMode mode = NkSizeMode::Content;
				float32 value = 0.f;
				const char *valueMetric = ""; ///< nom d'une metrique du composant ; prime sur `value`
				float32 minVal = 0.f;
				float32 maxVal = 0.f; ///< `maxVal <= minVal` signifie « non borne »
		};

		// ── LES FABRIQUES DE LECTURE ────────────────────────────────────────────
		// Elles n'ajoutent AUCUNE capacite : elles rendent les tables lisibles.
		// `NkExpand(24.f)` se lit ; `{NkSizeMode::Expand, 1.f, "", 24.f, 0.f}` se
		// dechiffre. La directive « lisible par un humain qui arrive » vaut d'abord
		// pour les tables que quatre mains vont ecrire en parallele.
		constexpr NkSizeDecl NkFixedPx(float32 px) {
			return NkSizeDecl{NkSizeMode::Fixed, px, "", 0.f, 0.f};
		}
		constexpr NkSizeDecl NkFixedM(const char *metric) {
			return NkSizeDecl{NkSizeMode::Fixed, 0.f, metric, 0.f, 0.f};
		}
		constexpr NkSizeDecl NkContent(float32 minV = 0.f, float32 maxV = 0.f) {
			return NkSizeDecl{NkSizeMode::Content, 0.f, "", minV, maxV};
		}
		constexpr NkSizeDecl NkFraction(float32 f, float32 minV = 0.f, float32 maxV = 0.f) {
			return NkSizeDecl{NkSizeMode::Fraction, f, "", minV, maxV};
		}
		constexpr NkSizeDecl NkFractionM(const char *metric, float32 minV = 0.f, float32 maxV = 0.f) {
			return NkSizeDecl{NkSizeMode::Fraction, 0.f, metric, minV, maxV};
		}
		constexpr NkSizeDecl NkWeight(float32 w, float32 minV = 0.f, float32 maxV = 0.f) {
			return NkSizeDecl{NkSizeMode::Weight, w, "", minV, maxV};
		}
		constexpr NkSizeDecl NkExpand(float32 minV = 0.f, float32 maxV = 0.f) {
			return NkSizeDecl{NkSizeMode::Expand, 1.f, "", minV, maxV};
		}

		// ── COMMENT UN PARENT DISPOSE SES ENFANTS ───────────────────────────────
		// Les quatre mots de Rodolf : ligne, colonne, grille, ancrage. `None` est le
		// cinquieme cas, et c'est le plus frequent : une FEUILLE ne dispose rien.
		//
		// CE QUI N'EST PAS LA, ET C'EST DELIBERE : pas de `Stack` (superposition).
		// Rien n'en a besoin aujourd'hui -- le dessin ajoute par l'application passe
		// par un POINT DE GREFFE (`NkHookDecl`), qui peint par-dessus sans que
		// l'agencement ait a le savoir. Le jour ou une apparence en aura vraiment
		// besoin, il s'ajoute A LA FIN de cette enumeration. **Ne pas l'ajouter par
		// anticipation** : une valeur d'enumeration que rien ne produit est une
		// valeur que rien ne teste.
		enum class NkLayoutKind : uint8 {
			None = 0, ///< feuille : le noeud n'a pas d'enfants a disposer
			Row,	  ///< ligne : les enfants se suivent en X
			Column,	  ///< colonne : les enfants se suivent en Y
			Grid,	  ///< grille : `gridColumns` colonnes, ou auto-remplissage
			Anchor,	  ///< ancrage : chaque enfant se colle aux bords qu'il declare
			Count
		};

		enum class NkAlign : uint8 {
			Start = 0,
			Center,
			End,
			Stretch, ///< occupe tout l'axe transverse -- le defaut, c'est ce que veut le responsive
			Count
		};

		/// Bords d'ancrage, quand le PARENT est en `NkLayoutKind::Anchor`.
		/// Deux bords opposes ancres = l'enfant s'etire entre eux ; un seul bord =
		/// il s'y colle en gardant sa taille declaree.
		namespace nkanchor {
			constexpr uint8 Left = 1u << 0;
			constexpr uint8 Top = 1u << 1;
			constexpr uint8 Right = 1u << 2;
			constexpr uint8 Bottom = 1u << 3;
		} // namespace nkanchor

		// ── L'AGENCEMENT D'UN PARENT ────────────────────────────────────────────
		// `spacingMetric` ET `padMetric` SONT DES NOMS, PAS DES NOMBRES, et c'est la
		// meme regle que pour les couleurs : une gouttiere est du style. Le
		// navigateur de contenu declare deja `card_gap` et `card_pad` parmi ses
		// metriques ; son agencement doit les DESIGNER, pas les recopier -- sinon la
		// valeur existe a deux endroits et l'editeur n'en change qu'un. C'est
		// exactement le defaut « une liste et son compte separes », transpose.
		struct NkLayoutDecl {
				NkLayoutKind kind = NkLayoutKind::None;
				const char *spacingMetric = ""; ///< gouttiere entre deux enfants
				const char *padMetric = "";		///< marge interne du parent
				NkAlign mainAlign = NkAlign::Start;
				NkAlign crossAlign = NkAlign::Stretch;

				// Grille seulement.
				uint8 gridColumns = 0;			 ///< 0 = auto-remplissage d'apres `gridCellMetric`
				const char *gridCellMetric = ""; ///< largeur d'une cellule, quand `gridColumns == 0`
		};

		// ── LE GABARIT REPETE ─────────────────────────────────────────
		// LA REPARATION GENERALE ANNONCEE AU §10.4 DE `ROADMAP.md`, ET ELLE EST FAITE
		// ICI PARCE QUE LE MANQUE A ETE MESURE CHEZ **DEUX** COMPOSANTS, DE DEUX MAINS
		// DIFFERENTES, LE MEME JOUR :
		//     `content_browser.grid` -> les CARTES, une par entree
		//     `tree_view.rows`       -> les LIGNES, une par noeud, recursivement
		//
		// La note `agencement_sans_enfant` tombait sur les deux. Ce n'etait donc pas
		// un besoin d'arbres ni un besoin de grilles : c'etait un manque de la FORME.
		// Un arbre statique ne sait pas dire « ici, un enfant PAR ENTREE de donnee ».
		//
		// ⚠️ CE QUE CE CHAMP N'EST PAS : une boucle. La forme ne compte rien et
		//    n'itere rien -- elle ne SAIT PAS combien il y a d'entrees, et c'est
		//    voulu : la donnee appartient a l'application, jamais au kit. Ce champ
		//    DECLARE une intention de repetition ; c'est le dessin, qui tient la
		//    donnee, qui instancie le gabarit autant de fois qu'il le faut.
		//
		// ⚠️ ET LA POSITION RESTE UN RESULTAT. Un gabarit repete declare sa taille
		//    comme tout autre element (fige/extensible/a poids) ; c'est le solveur qui
		//    place les N copies, exactement comme il place N freres ecrits a la main.
		//    Aucun `x`, aucun `y` n'apparait ici -- la regle ne souffre pas d'exception
		//    parce qu'un enfant est repete.
		enum class NkRepeatKind : uint8 {
			Once = 0,     ///< une fois -- le cas ordinaire, et le DEFAUT
			PerEntry,     ///< par entree : une copie par element de la donnee (cartes, lignes)
			PerEntryTree, ///< par entree, RECURSIF : la copie peut contenir des copies (arbre)
			Count
		};

		// ── UN SOUS-ELEMENT ─────────────────────────────────────────────────────
		// LE PARENT SE DESIGNE PAR SON NOM, PAS PAR UN INDEX, et le choix est paye
		// d'avance :
		//   - un index se decale des qu'on INSERE une ligne au milieu de la table,
		//     et rien ne le signale : l'arbre se reconstruit simplement faux. C'est
		//     la famille de defaut la plus couteuse -- celle qui compile ;
		//   - un nom se serialise tel quel dans un fichier de description ;
		//   - un nom se lit : `parent = "toolbar"` se relit sans compter les lignes.
		//
		// CONTRAINTE D'ECRITURE, ET ELLE SUPPRIME LES CYCLES SANS LES CHERCHER :
		// **un parent doit apparaitre PLUS HAUT dans la table que ses enfants.** Un
		// cycle devient alors impossible a ECRIRE, et l'ordre de lecture de la table
		// est l'ordre de l'arbre -- on la lit de haut en bas comme on lirait une
		// interface. `NkCheckComponent` (NkComponentCheck.h) le verifie au lieu de
		// l'esperer. L'ordre des FRERES est leur ordre d'apparition dans la table.
		struct NkElementDecl {
				const char *name = "";	 ///< cle STABLE, unique dans le composant
				const char *parent = ""; ///< NOM du parent ; "" = la racine (il n'y en a qu'une)
				const char *purpose = "";

				/// LA CAPACITE attribuee a cette apparence -- premier ajout de Rodolf.
				/// "" = purement decoratif : ce noeud n'entend rien et n'emet rien.
				/// Le vocabulaire est le catalogue de `NkComponentRole.h`.
				const char *role = "";

				/// LE POINT OU L'ECHELLE CHANGE, ET C'EST LE SEUL. Si ce champ porte
				/// le nom d'un composant du registre, ce noeud n'est PAS dessine par
				/// le composant courant : il DELEGUE. C'est ainsi qu'« une interface
				/// complete est un composant qui en contient d'autres » se dit, sans
				/// qu'aucun mecanisme nouveau apparaisse a la seconde echelle.
				const char *component = "";

				NkSizeDecl width;
				NkSizeDecl height;
				NkLayoutDecl layout;   ///< comment il dispose SES enfants
				uint8 anchorEdges = 0; ///< bits `nkanchor::*`, quand le PARENT est en Anchor

				// ═════════════════════════════════════════════════════════
				//  ⚠️ AJOUTE LE 2026-08-19, DONC **A LA FIN** -- ET C'EST LA REGLE
				//     QUI A ETE PAYEE UNE FOIS, PAS UNE PREFERENCE DE MISE EN PAGE.
				// ═════════════════════════════════════════════════════════
				//  Le meme jour, `role` insere au MILIEU de `NkComponentDecl` a casse
				//  12 initialisations chez un agent qui compilait en parallele. C++17
				//  n'a pas d'initialiseurs designes : toute table d'elements s'ecrit en
				//  liste POSITIONNELLE, et un champ insere au milieu decale tout ce qui
				//  suit, chez tout le monde.
				//
				//  ⚠️ ET LE CAS BENIN EST LE VRAI DANGER : la, les types etaient
				//     incompatibles, donc ca a rougi. Entre deux `const char*` voisins,
				//     le decalage aurait COMPILE et decrit un autre composant.
				//
				//  Ajoute a la fin, une table ecrite avant ce champ reste valide : les
				//  elements manquants prennent leur defaut, et `Once` est precisement le
				//  comportement d'avant. **Cet ajout ne peut casser personne.**
				NkRepeatKind repeat = NkRepeatKind::Once;
		};

		// ── LES MOTS DU FICHIER -- UN SEUL POINT DE VERITE ───────────────────────
		// ⚠️ POURQUOI CES DOUZE FONCTIONS EXISTENT, ET POURQUOI ELLES SONT ICI ET
		//    PAS CHEZ CELUI QUI ECRIT LE FICHIER.
		//
		//    Ces trois enumerations finiront ECRITES dans un fichier de description.
		//    Le jour ou deux endroits savent les orthographier, il y a deux
		//    orthographes -- et un fichier ecrit par l'un devient illisible par
		//    l'autre. Le depot connait deja ce defaut sous une autre forme
		//    (`NkArgTypeName`, `NkAuthorName`) : la table vit a cote du type, une
		//    seule fois.
		//
		//    Ce n'est pas une precaution abstraite : le 19/08, l'application
		//    NkUIDesign ecrivait ses propres `NkCrossAlignName` / `NkParseCrossAlign`
		//    pendant que ce fichier definissait `NkAlign`. Deux vocabulaires pour un
		//    concept, a une heure d'intervalle, dans le meme depot. **Une forme
		//    partagee doit fournir ses mots, sinon chaque main ecrit les siens.**
		//
		// ⚠️ MOTS-CLES EN ANGLAIS ET EN MINUSCULES, et c'est une decision : la cible
		//    est `.nkgui` v0.2, dont tout le vocabulaire l'est (`callback`, `Void`,
		//    `Int`). Les LIBELLES affiches restent francais ; ce sont les CLES de
		//    fichier qui sont ici. Ne pas les traduire -- une cle traduite est une
		//    cle cassee.
		//
		// APPEND-ONLY, comme les enumerations qu'elles servent.
		namespace layoutdetail {
			inline bool KeyEq(const char *a, const char *b) {
				if (!a || !b)
					return false;
				for (; *a && *b; ++a, ++b)
					if (*a != *b)
						return false;
				return *a == *b;
			}
		} // namespace layoutdetail

		inline const char *NkSizeModeName(NkSizeMode m) {
			switch (m) {
				case NkSizeMode::Fixed:
					return "fixed";
				case NkSizeMode::Content:
					return "content";
				case NkSizeMode::Fraction:
					return "fraction";
				case NkSizeMode::Weight:
					return "weight";
				case NkSizeMode::Expand:
					return "expand";
				default:
					return "content";
			}
		}
		/// Rend `false` sur un mot inconnu SANS toucher a `out` -- l'appelant decide
		/// alors s'il compte l'inconnu ou s'il refuse. Meme tolerance que
		/// `NkTheme::Load` : un mot qu'on ne comprend pas se compte, il ne fait pas
		/// echouer un fichier entier.
		inline bool NkParseSizeMode(const char *s, NkSizeMode &out) {
			for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i)
				if (layoutdetail::KeyEq(s, NkSizeModeName((NkSizeMode)i))) {
					out = (NkSizeMode)i;
					return true;
				}
			return false;
		}

		inline const char *NkLayoutKindName(NkLayoutKind k) {
			switch (k) {
				case NkLayoutKind::None:
					return "none";
				case NkLayoutKind::Row:
					return "row";
				case NkLayoutKind::Column:
					return "column";
				case NkLayoutKind::Grid:
					return "grid";
				case NkLayoutKind::Anchor:
					return "anchor";
				default:
					return "none";
			}
		}
		inline bool NkParseLayoutKind(const char *s, NkLayoutKind &out) {
			for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i)
				if (layoutdetail::KeyEq(s, NkLayoutKindName((NkLayoutKind)i))) {
					out = (NkLayoutKind)i;
					return true;
				}
			return false;
		}

		inline const char *NkAlignName(NkAlign a) {
			switch (a) {
				case NkAlign::Start:
					return "start";
				case NkAlign::Center:
					return "center";
				case NkAlign::End:
					return "end";
				case NkAlign::Stretch:
					return "stretch";
				default:
					return "start";
			}
		}
		inline bool NkParseAlign(const char *s, NkAlign &out) {
			for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i)
				if (layoutdetail::KeyEq(s, NkAlignName((NkAlign)i))) {
					out = (NkAlign)i;
					return true;
				}
			return false;
		}

		/// Meme regle que les trois autres : la table vit A COTE du type, une seule
		/// fois. Une seconde orthographe ailleurs = un fichier illisible par l'autre
		/// moitie du code (c'est arrive le 19/08 avec `NkCrossAlignName`).
		inline const char *NkRepeatKindName(NkRepeatKind r) {
			switch (r) {
				case NkRepeatKind::Once:
					return "once";
				case NkRepeatKind::PerEntry:
					return "per_entry";
				case NkRepeatKind::PerEntryTree:
					return "per_entry_tree";
				default:
					return "once";
			}
		}
		inline bool NkParseRepeatKind(const char *s, NkRepeatKind &out) {
			for (uint8 i = 0; i < (uint8)NkRepeatKind::Count; ++i)
				if (layoutdetail::KeyEq(s, NkRepeatKindName((NkRepeatKind)i))) {
					out = (NkRepeatKind)i;
					return true;
				}
			return false;
		}

		/// Les bords d'ancrage s'ecrivent en toutes lettres, un caractere par bord :
		/// « ltrb », « lr », « " " ». Un masque de bits ecrit en nombre serait
		/// illisible dans un fichier et se relirait faux au premier reordonnancement.
		inline void NkAnchorName(uint8 edges, char out[5]) {
			uint8 n = 0;
			if (edges & nkanchor::Left)
				out[n++] = 'l';
			if (edges & nkanchor::Top)
				out[n++] = 't';
			if (edges & nkanchor::Right)
				out[n++] = 'r';
			if (edges & nkanchor::Bottom)
				out[n++] = 'b';
			out[n] = '\0';
		}
		inline uint8 NkParseAnchor(const char *s) {
			uint8 e = 0;
			for (; s && *s; ++s) {
				if (*s == 'l')
					e |= nkanchor::Left;
				else if (*s == 't')
					e |= nkanchor::Top;
				else if (*s == 'r')
					e |= nkanchor::Right;
				else if (*s == 'b')
					e |= nkanchor::Bottom;
			}
			return e;
		}

	} // namespace editorkit
} // namespace nkentseu
