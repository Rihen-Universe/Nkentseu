// =============================================================================
// NKSerialization/NkGui/NkGuiArchive.h
// Le LECTEUR et l'ECRIVAIN `.nkgui`, cote NkArchive.
//
// =============================================================================
//  CE QUE CETTE COUCHE EST, ET CE QU'ELLE N'EST PAS
// =============================================================================
//  Elle est PUREMENT SYNTAXIQUE. Elle ne connait ni `widgets`, ni `Button`, ni
//  `appearance`, ni aucun mot-cle du langage : elle ne connait que la FORME du
//  fichier -- un en-tete de version, puis des blocs `Type "id" { ... }` dont les
//  membres sont des proprietes `cle = valeur` ou d'autres blocs.
//
//  C'est le choix central, et il n'est pas esthetique. `NkGuiFormat.h` (v0.3,
//  4185 lignes) modelise CHAQUE section -- geometry, widgets, behavior,
//  controller, callback, animation, fonts -- et doit donc etre RETOUCHE a chaque
//  ajout du langage. La regle (d) de la compatibilite ascendante (« ce qu'on ne
//  comprend pas, on le preserve tel quel ») y est un mecanisme d'exception
//  (`NkGRaw`) qu'il faut se souvenir de cabler section par section.
//
//  Ici la regle (d) n'est plus un mecanisme : c'est le REGIME NORMAL. Une
//  section de la v0.4 se lit, se represente et se reemet sans qu'une seule ligne
//  de cette couche change, parce que rien ici ne sait ce qu'est une section.
//  Le SENS vit au-dessus : c'est le modele (`NkUIDocument`, la validation par
//  role) qui va chercher dans l'archive les blocs qu'il comprend.
//
//  Consequence assumee : ce qui n'entre PAS dans la forme « bloc / propriete »
//  -- les instructions de `behavior` (`set x = a + b`, `if ... { }`), les fils
//  (`a.b -> c.d`), les signatures de callback -- est conserve en TRANCHE DE
//  SOURCE VERBATIM (T11). Le fichier fait l'aller-retour a l'octet ; le sens de
//  ces constructions, lui, n'est pas modelise ici. Ce n'est pas un repli : c'est
//  la meme regle, appliquee uniformement au lieu d'etre cablee au cas par cas.
//
// =============================================================================
//  LA REPRESENTATION  (elle sort de T9, T10 et T11 -- SandboxNKArchive)
// =============================================================================
//  Rien n'a ete ajoute a `NkArchive` pour ecrire cette couche. Les trois
//  prerequis de representation avaient ete mesures avant :
//
//   - T9  : l'ordre ENTRELACE proprietes/enfants tient, parce que le rang
//           (`SourceOrder`) vit sur le NOEUD -- donc aussi sur chaque element
//           d'un tableau -- et pas sur l'entree ;
//   - T10 : l'identite d'un noeud (`VBox "v"`) n'est PAS une propriete. Elle
//           passe par des CLES RESERVEES `$type` / `$id` que l'ecrivain consomme
//           pour composer l'en-tete du bloc au lieu de les emettre ;
//   - T11 : une tranche de source inconnue se represente par un simple noeud
//           CHAINE -- la forme canonique d'une chaine est la chaine elle-meme.
//           Aucun litteral, aucune trivia, aucun mecanisme.
//
//  ROOT (l'archive rendue par Read) :
//    HeaderTrivia          les lignes AVANT `nkgui X.Y`
//    "$version"            chaine = "0.2", rang 0 ; sa trivia de fin = le reste
//                          de la ligne d'en-tete
//    "$body"               tableau des constructions de premier niveau
//    FooterTrivia          les lignes APRES la derniere construction
//
//  UN BLOC (element OBJET d'un `$body`) :
//    trivia de tete        les lignes qui le precedent
//    trivia de fin         ce qui suit son `{` sur la MEME ligne
//    rang                  sa place parmi les membres de son parent
//    "$type"               le mot qui l'ouvre, rang 0
//    "$id"                 son nom entre guillemets s'il en a un, rang 1
//    <proprietes>          noeuds scalaires, rang >= 2, litteral + trivia
//    "$body"               tableau de ses membres non-scalaires
//    FooterTrivia          les lignes avant son `}`
//
//  UNE TRANCHE BRUTE (element SCALAIRE d'un `$body`) :
//    la chaine EST la tranche de source. Dans un `$body`, un element OBJET est
//    un bloc et un element SCALAIRE est une tranche brute : le discriminant est
//    deja la, il n'y a rien a inventer.
//
// =============================================================================
//  LE JETON NU  --  ET POURQUOI C'EST UNE OPERATION DE PREMIERE CLASSE
// =============================================================================
//  Une valeur `.nkgui` s'ecrit soit ENTRE GUILLEMETS (`label = "Envoyer"`), soit
//  NUE (`enabled = false`, `color = #F79A28`, `size = (12, 4)`, `mode = A | B`).
//  `NkArchive` n'a pas de type pour « symbole nu » : les deux sont des chaines.
//
//  La regle d'impression est donc : **litteral encore valable -> on imprime le
//  litteral tel quel ; sinon on met des guillemets et on echappe.** Elle est
//  complete a une condition -- que poser un jeton nu soit POSSIBLE. C'est
//  `SetToken()`, et ce n'est pas un contournement : c'est la moitie manquante de
//  `SetString()`. Sans elle, un modele qui reecrit une couleur produirait
//  `color = "#F79A28"`, une chaine la ou le langage attend une couleur.
//
//  Le garde-fou anti-perime de `NkArchiveNode::SetLiteral` reste actif : un
//  jeton nu dont on change la valeur SANS repasser par `SetToken()` perd son
//  litteral et repart entre guillemets. C'est visible, et une validation par
//  role le rattrape -- au contraire d'une sortie qui aurait l'air correcte.
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKGUIARCHIVE_H
#define NKENTSEU_SERIALIZATION_NKGUIARCHIVE_H

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/NkSerializationApi.h"

namespace nkentseu {

	// =========================================================================
	// STRUCTURE : NkGuiDiag
	// =========================================================================
	/**
	 * @struct NkGuiDiag
	 * @brief Ce qui a empeche la lecture, avec de quoi le retrouver dans le fichier
	 *
	 * Un refus sans ligne ni colonne oblige a chercher a la main dans un document
	 * de trois mille noeuds. Les codes sont ceux du format : `E-PARSE`,
	 * `E-VERSION-INCOMPATIBLE`.
	 */
	struct NKENTSEU_SERIALIZATION_API NkGuiDiag {
			NkString code;
			NkString message;
			nk_uint32 line = 0;
			nk_uint32 column = 0;
	};

	// =========================================================================
	// STRUCTURE : NkGuiStyle
	// =========================================================================
	/**
	 * @struct NkGuiStyle
	 * @brief La mise en forme LUE dans le fichier source, rendue a l'ecriture
	 *
	 * Ce n'est pas du confort. Sans elle, reecrire un document indente a deux
	 * espaces le rendrait indente a quatre, et le diff porterait sur chaque ligne
	 * du fichier au lieu de porter sur ce qui a change.
	 *
	 * @note `finalNewline` existe parce qu'un fichier peut ne PAS finir par un
	 *       saut de ligne, et qu'en ajouter un serait deja une modification. Il
	 *       vit ici et pas dans l'archive : c'est de la mise en forme, pas du
	 *       contenu.
	 */
	struct NKENTSEU_SERIALIZATION_API NkGuiStyle {
			nk_uint32 indent = 2;			 ///< espaces par cran
			bool crlf = false;				 ///< fins de ligne CRLF
			bool bom = false;				 ///< le fichier commencait par un BOM UTF-8
			bool finalNewline = true;		 ///< le fichier finit par un saut de ligne
			bool inlineEmptyBlock = true;	 ///< `Button "x" { }` sur une ligne
	};

	// =========================================================================
	// CLASSE : NkGuiArchive
	// =========================================================================
	/**
	 * @class NkGuiArchive
	 * @brief Le pont `.nkgui` <-> `NkArchive` : lecteur, ecrivain, et les cles reservees
	 */
	class NKENTSEU_SERIALIZATION_API NkGuiArchive {
		public:
			// -----------------------------------------------------------------
			// LA VERSION QUE CE LECTEUR COMPREND
			// -----------------------------------------------------------------
			/// Regle (c) : au-dela de la MAJEURE on refuse ; au-dela de la MINEURE on
			/// lit et on preserve. Ici la regle (d) ne coute rien : la couche etant
			/// purement syntaxique, une section de la v0.4 se lit deja.
			static const nk_int32 kMajor = 0;
			static const nk_int32 kMinor = 3;

			// -----------------------------------------------------------------
			// LES CLES RESERVEES
			// -----------------------------------------------------------------
			/// Le mot qui ouvre un bloc (`VBox`, `widgets`, `fill`...)
			static const char *KeyType() noexcept {
				return "$type";
			}
			/// Le nom entre guillemets qui suit le type, quand il y en a un
			static const char *KeyId() noexcept {
				return "$id";
			}
			/// Les membres non-scalaires d'un bloc, dans l'ordre du fichier
			static const char *KeyBody() noexcept {
				return "$body";
			}
			/**
			 * @brief La DISPOSITION du bloc, telle que le fichier l'ecrivait
			 *
			 * Quatre etats, et un seul endroit :
			 *  - absente     : un bloc vide s'ecrit `{ }`, un bloc plein sur plusieurs
			 *                  lignes. C'est la forme de tres loin la plus courante,
			 *                  et elle ne coute donc aucune entree ;
			 *  - `block`     : plusieurs lignes MEME VIDE (`{` puis `}`) ;
			 *  - `inline`    : tout sur une ligne, membres separes par une espace ;
			 *  - `inline,`   : tout sur une ligne, membres separes par `, ` -- la
			 *                  forme du document 9 (`shadow { offset = (0, 2), blur = 6 }`).
			 *
			 * ⚠️ POURQUOI UNE CLE ET PAS UN BIT IMPLICITE. La premiere version disait
			 *    « bloc vide sur deux lignes » en POSANT un bloc de trivia vide, et
			 *    lisait la reponse dans `HasTrivia()`. Ca marchait, et c'etait le
			 *    debut d'un empilement : il a suffi qu'un second fait de disposition
			 *    apparaisse (le bloc PLEIN sur une ligne, mesure sur les exemples des
			 *    documents de grammaire) pour qu'il faille un deuxieme bit implicite,
			 *    puis un troisieme pour les virgules. Une cle reservee les porte tous,
			 *    elle se lit, elle se pose, et elle se voit.
			 */
			static const char *KeyLayout() noexcept {
				return "$layout";
			}
			/// La version du fichier, telle qu'ecrite
			static const char *KeyVersion() noexcept {
				return "$version";
			}

			/**
			 * @brief Vrai si la cle porte de la SYNTAXE et non une propriete du modele
			 *
			 * ⚠️ CETTE GARANTIE EST EMPRUNTEE AU LEXEUR, elle n'est pas produite ici.
			 *    Une cle reservee n'est sure que parce qu'un identifiant `.nkgui` est
			 *    `[A-Za-z_][A-Za-z0-9_]*` : `$` en est exclu, donc aucun fichier
			 *    valide ne peut nommer une propriete `$type`. Le jour ou quelqu'un
			 *    ajoutera `$` aux identifiants, cette couche cassera en silence --
			 *    c'est pourquoi le canari vit AVEC le lexeur (controle « $ refuse »
			 *    du banc corpus), pas ici.
			 */
			static bool IsReservedKey(NkStringView key) noexcept;

			// -----------------------------------------------------------------
			// LECTURE ET ECRITURE
			// -----------------------------------------------------------------
			/**
			 * @brief Analyse un texte `.nkgui` et remplit l'archive
			 * @param src     le contenu du fichier, en octets
			 * @param length  sa taille
			 * @param out     l'archive remplie (videe d'abord)
			 * @param err     rempli seulement en cas d'echec
			 * @return false en cas de refus ; `out` est alors sans valeur
			 */
			static bool Read(const char *src, nk_uint32 length, NkArchive &out,
							 NkGuiDiag &err) noexcept;

			/**
			 * @brief Reemet un texte `.nkgui` depuis l'archive
			 * @note L'archive n'est PAS retriee : elle est parcourue par rang, en
			 *       fusionnant les proprietes et le `$body`. Une entree sans rang se
			 *       range apres celles qui en ont un -- une propriete ajoutee par le
			 *       code s'ecrit a la fin, elle ne s'insere pas au hasard.
			 */
			static NkString Write(const NkArchive &doc, const NkGuiStyle &style);

			/// @brief La mise en forme lue dans la source
			static NkGuiStyle DetectStyle(const char *src, nk_uint32 length) noexcept;

			// -----------------------------------------------------------------
			// POSER UNE VALEUR
			// -----------------------------------------------------------------
			/**
			 * @brief Pose un JETON NU (couleur, identifiant, drapeaux, vecteur)
			 * @param ar    le bloc
			 * @param key   le nom de la propriete
			 * @param token le texte a ecrire TEL QUEL, sans guillemets
			 * @return false si la cle est reservee (elle ne se pose pas a la main)
			 * @note La moitie manquante de `SetString` : voir l'en-tete du fichier.
			 */
			static bool SetToken(NkArchive &ar, NkStringView key, NkStringView token) noexcept;

			/// @brief La meme chose sur un noeud deja en place
			static void SetTokenNode(NkArchiveNode &node, NkStringView token) noexcept;

			/// @brief Vrai si ce noeud scalaire s'imprimera NU (sans guillemets)
			static bool IsToken(const NkArchiveNode &node) noexcept;

			// -----------------------------------------------------------------
			// CONSTRUIRE UN DOCUMENT
			// -----------------------------------------------------------------
			/**
			 * @brief Cree un bloc et l'ajoute au `$body` de `parent`
			 * @param parent le bloc (ou la racine) qui accueille
			 * @param type   le mot qui ouvre le bloc
			 * @param id     son nom, ou une vue vide pour un bloc anonyme
			 * @return un pointeur vers le NOEUD ajoute, jamais nul
			 * @note ⚠️ Le pointeur rendu est invalide par le prochain ajout dans le
			 *       MEME `$body` : `NkVector` se réalloue. On s'en sert tout de
			 *       suite, on ne le garde pas.
			 */
			static NkArchiveNode *AddBlock(NkArchive &parent, NkStringView type,
										   NkStringView id) noexcept;

			/// @brief Le `$body` de `ar`, cree s'il n'existe pas
			static NkArchiveNode &EnsureBody(NkArchive &ar) noexcept;

			/// @brief Le type d'un bloc, vue vide si absent
			static NkStringView TypeOf(const NkArchive &block) noexcept;

			/// @brief L'identifiant d'un bloc, vue vide si absent
			static NkStringView IdOf(const NkArchive &block) noexcept;

	}; // class NkGuiArchive

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKGUIARCHIVE_H

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
