// =============================================================================
// NkcCellLabel.h — le nom LISIBLE d'une case : « C4 ».
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Un stagiaire a ecrit : « je ne comprends pas bien le systeme de coordonnees
// axiales [...] l'interpretation du journal des coups est plus compliquee. Ne
// pourrait-on pas proceder comme aux echecs ? »
//
// Il ne demandait pas une fonctionnalite nouvelle. LA FORME LISIBLE EXISTAIT
// DEJA ET ON LA JETAIT : `boards/_generer.py` ecrit les plateaux en
// (colonne, rangee), puis les convertit en axial — son en-tete appelle cette
// conversion « LE piege des plateaux ». On demandait ensuite a l'utilisateur de
// refaire ce calcul dans sa tete.
//
// CE QUI NE CHANGE PAS, ET C'EST L'ESSENTIEL
// ------------------------------------------
// L'axial reste la verite : en memoire, dans l'ABI, et dans les `.json`. Un
// axial (q,r) est excellent pour calculer un voisinage et illisible pour un
// humain ; une etiquette colonne-rangee est exactement l'inverse. Ce ne sont pas
// deux systemes concurrents, ce sont DEUX REPRESENTATIONS DU MEME ETAT.
//
// L'etiquette est donc en AFFICHAGE SEUL, et dans la trace copiee elle est
// AJOUTEE a cote des colonnes q/r, jamais substituee. Consequence : les `.json`
// existants restent lisibles, les traces deja enregistrees restent rejouables,
// l'ABI ne bouge pas — donc les modules deja compiles par des stagiaires
// continuent de fonctionner. Le cout de compatibilite est NUL.
//
// LE SCHEMA RETENU (decision du 2026-08-16) : « SCHEMA A »
// -------------------------------------------------------
//   lettre = colonne offset      chiffre = rangee + 1        ->  "C4"
//
// Mesure sur les 14 plateaux livres, 652 cases : 652 etiquetees, 0 collision,
// 0 erreur d'aller-retour. Parfait (aucun trou de numerotation) sur les 6
// plateaux `hexagone_*`, qui sont ceux du jeu.
//
// Il a ete compare a un « schema B » (lettre = axe axial q, donc de vraies
// diagonales droites) : B est parfait sur les plateaux carres et
// parallelogrammes et troue les hexagones — l'inverse exact. Les deux gardent
// un ecart maximal de 1 entre cases voisines, mesure sur 3 160 paires.
//
// A l'emporte pour UNE raison mesurable : la topologie est `HEX_POINTY`, et le
// rendu pose x = racine(3) * (q + 0,5 r) (`NkcBoardRender.h`, CoordToUnit). Les
// RANGEES sont donc les seules vraies droites a l'ecran. Le chiffre suit une
// ligne que l'oeil voit ; la lettre ne zigzague que d'une demi-case.
//
// TOUT PASSE PAR `NkcFormatCellLabel`. Changer de schema, c'est changer les deux
// fonctions `NkcColumnOf` / `NkcRowOf` — la decision reste reversible pour le
// prix d'une ligne, et c'est ce qui a permis de la prendre vite.
//
// L'ALPHABET N'A NI I NI O : a cote d'un 1 et d'un 0, sur un plateau, ils se
// lisent mal. 24 lettres, le plus grand plateau livre en fait 9.
// =============================================================================
#pragma once

#include "Conqueror/ConquerorRulesABI.h"

namespace nkentseu {
	namespace conqueror {

		/// 24 lettres : l'alphabet SANS I ni O.
		inline const char *NkcLabelAlphabet() noexcept { return "ABCDEFGHJKLMNPQRSTUVWXYZ"; }
		inline constexpr int32 kNkcLabelAlphabetSize = 24;

		/// Division entiere PLANCHER par 2, negatifs compris.
		///
		/// `v >> 1` sur un negatif n'est arithmetiquement garanti que depuis
		/// C++20, et les plateaux centres sur zero (diamant, plus) ont bel et
		/// bien des colonnes negatives : c'est exactement la ou un decalage
		/// approximatif se voit.
		inline int32 NkcFloorHalf(int32 v) noexcept {
			return (v >= 0) ? (v / 2) : -(((-v) + 1) / 2);
		}

		/// La COLONNE d'une case, dans la representation lisible.
		///
		/// C'est ici que vit le schema. `_generer.py` fait la conversion inverse
		/// (`q = col - (row >> 1)`) : les deux doivent rester d'accord.
		inline int32 NkcColumnOf(NkcTopology t, NkcCoord c) noexcept {
			switch (t) {
				case NkcTopology::HexPointy: return c.q + NkcFloorHalf(c.r);	// odd-r
				case NkcTopology::HexFlat:	 return c.q;						// odd-q
				case NkcTopology::Square4:
				case NkcTopology::Square8:	 return c.q;						// deja lisible
			}
			return c.q;
		}

		/// La RANGEE d'une case, dans la representation lisible.
		inline int32 NkcRowOf(NkcTopology t, NkcCoord c) noexcept {
			switch (t) {
				case NkcTopology::HexPointy: return c.r;
				case NkcTopology::HexFlat:	 return c.r + NkcFloorHalf(c.q);
				case NkcTopology::Square4:
				case NkcTopology::Square8:	 return c.r;
			}
			return c.r;
		}

		/// Origine d'etiquetage d'un plateau : DEUX ENTIERS, calcules une fois.
		///
		/// Sans eux, `diamant.json` et `plus.json` — dont le generateur centre la
		/// forme sur zero — sortent en colonnes negatives et perdent 50 cases sur
		/// 652. Avec eux : 652 / 652.
		struct NkcLabelOrigin {
				int32 col = 0;	  ///< colonne minimale du plateau
				int32 row = 0;	  ///< rangee minimale du plateau
		};

		/// Calcule l'origine depuis les coordonnees REELLES du plateau charge.
		///
		/// On la prend de la vue et jamais d'une constante : un plateau defini en
		/// C++ par un module (chapitre « Definir sa grille en C++ ») peut avoir
		/// n'importe quelle forme, et personne ne la connait a l'avance.
		inline NkcLabelOrigin NkcComputeLabelOrigin(const NkcCoord *coords, uint32 count,
													NkcTopology t) noexcept {
			NkcLabelOrigin o;
			if (!coords || count == 0) return o;
			o.col = NkcColumnOf(t, coords[0]);
			o.row = NkcRowOf(t, coords[0]);
			for (uint32 i = 1; i < count; ++i) {
				const int32 c = NkcColumnOf(t, coords[i]);
				const int32 r = NkcRowOf(t, coords[i]);
				if (c < o.col) o.col = c;
				if (r < o.row) o.row = r;
			}
			return o;
		}

		/// Ecrit « C4 » dans `out`. Toujours termine par zero.
		///
		/// Au-dela de 24 colonnes, la lettre double : Z puis AA, AB... Aucun
		/// plateau livre n'y arrive (le plus large en fait 9), mais un plateau
		/// defini en C++ le peut, et une etiquette tronquee en silence serait
		/// exactement le genre de defaut que ce fichier existe pour eviter.
		inline void NkcFormatCellLabel(NkcCoord c, NkcTopology t, const NkcLabelOrigin &o,
									   char *out, uint32 cap) noexcept {
			if (!out || cap == 0) return;
			out[0] = '\0';
			if (cap < 8) return;

			const int32 col = NkcColumnOf(t, c) - o.col;
			const int32 row = NkcRowOf(t, c) - o.row;
			if (col < 0 || row < 0) {	// hors du plateau connu : on le DIT
				out[0] = '?'; out[1] = '\0';
				return;
			}

			const char *alpha = NkcLabelAlphabet();
			uint32		w	  = 0;
			if (col < kNkcLabelAlphabetSize) {
				out[w++] = alpha[col];
			} else {
				const int32 haut = col / kNkcLabelAlphabetSize - 1;
				const int32 bas	 = col % kNkcLabelAlphabetSize;
				if (haut >= kNkcLabelAlphabetSize) { out[0] = '?'; out[1] = '\0'; return; }
				out[w++] = alpha[haut];
				out[w++] = alpha[bas];
			}

			// Le numero de rangee, en base 10, sans dependre d'un formateur.
			int32  n	 = row + 1;	  // l'utilisateur compte a partir de 1
			char   tmp[12];
			uint32 nt = 0;
			while (n > 0 && nt < sizeof(tmp)) { tmp[nt++] = static_cast<char>('0' + (n % 10)); n /= 10; }
			if (nt == 0) tmp[nt++] = '0';
			while (nt > 0 && w + 1 < cap) out[w++] = tmp[--nt];
			out[w] = '\0';
		}

	}	 // namespace conqueror
}	 // namespace nkentseu
