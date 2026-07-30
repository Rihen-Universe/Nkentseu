// =============================================================================
// NKMedia/Codecs/Video/Theora/NkTheoraTables.h
// -----------------------------------------------------------------------------
// Tables NORMATIVES du décodeur Theora, transcrites À LA MAIN depuis la
// spécification Theora I (Xiph.Org). Aucune table n'a été copiée depuis du
// code tiers : chacune provient d'une figure ou d'un tableau de la spec, avec
// la référence de section indiquée. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {
		namespace theora {

			// --- Figure 2.8 : ordre zig-zag ------------------------------------------
			// NAT_TO_ZZ[ci] : pour l'indice ci en ordre NATUREL (row-major, ci=r*8+c),
			// donne la position correspondante en ordre ZIG-ZAG. Utilisé par la
			// déquantification (§7.9.2) : DQC[ci] = COEFFS[NAT_TO_ZZ[ci]] * QMAT[ci].
			static const uint8 kNatToZz[64] = {
				0,	1,	5,	6,	14, 15, 27, 28,
				2,	4,	7,	13, 16, 26, 29, 42,
				3,	8,	12, 17, 25, 30, 41, 43,
				9,	11, 18, 24, 31, 40, 44, 53,
				10, 19, 23, 32, 39, 45, 52, 54,
				20, 22, 33, 38, 46, 51, 55, 60,
				21, 34, 37, 47, 50, 56, 59, 61,
				35, 36, 48, 49, 57, 58, 62, 63,
			};

			// --- Figure 2.4 : courbe de Hilbert des blocs dans un super bloc ----------
			// Pour l'index de Hilbert h (0..15), donne la position locale (lx,ly) dans
			// le super bloc 4x4, ly=0 = bas. Dérivé du tableau de la figure 2.4.
			static const uint8 kHilbertLx[16] = {0, 1, 1, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 2, 2, 3};
			static const uint8 kHilbertLy[16] = {0, 0, 1, 1, 2, 3, 3, 2, 2, 3, 3, 2, 1, 1, 0, 0};

			// --- Figure 2.6 : courbe de Hilbert des macroblocs dans un super bloc -----
			// Ordre local (mlx,mly), mly=0 = bas : 0=(0,0),1=(0,1),2=(1,1),3=(1,0).
			static const uint8 kMbHilbertLx[4] = {0, 0, 1, 1};
			static const uint8 kMbHilbertLy[4] = {0, 1, 1, 0};

			// --- Table 7.65 : approximations 16 bits des cosinus/sinus ----------------
			// Ci = cos(i*pi/16)*65536, Sj = sin(j*pi/16)*65536. Sj = C(8-j).
			static const int32 kC1 = 64277;
			static const int32 kC2 = 60547;
			static const int32 kC3 = 54491;
			static const int32 kC4 = 46341;
			static const int32 kC5 = 36410;
			static const int32 kC6 = 25080;
			static const int32 kC7 = 12785;

			// --- Table 6.18 : valeurs minimales de quantification ---------------------
			// [qti][ci==0 ? 0 : 1] : intra DC=16 AC=8, inter DC=32 AC=16.
			static inline int32 QMin(int32 qti, int32 ci) {
				if (qti == 0)
					return ci == 0 ? 16 : 8;
				return ci == 0 ? 32 : 16;
			}

			// --- Table 7.46 : indice de trame de référence par mode de codage ---------
			// 0=INTER NOMV→Prev(1), 1=INTRA→None(0), 2=INTER MV→1, 3=MV LAST→1,
			// 4=MV LAST2→1, 5=GOLDEN NOMV→Golden(2), 6=GOLDEN MV→2, 7=MV FOUR→1.
			static const uint8 kModeRefFrame[8] = {1, 0, 1, 1, 1, 2, 2, 1};

			// --- Table 7.19 : schémas de codage des modes de macrobloc (schémas 1..6) -
			// kModeScheme[scheme-1][mi] = mode.
			static const uint8 kModeScheme[6][8] = {
				{3, 4, 2, 0, 1, 5, 6, 7}, // schéma 1
				{3, 4, 0, 2, 1, 5, 6, 7}, // schéma 2
				{3, 2, 4, 0, 1, 5, 6, 7}, // schéma 3
				{3, 2, 0, 4, 1, 5, 6, 7}, // schéma 4
				{0, 3, 4, 2, 1, 5, 6, 7}, // schéma 5
				{0, 5, 3, 4, 2, 1, 6, 7}, // schéma 6
			};

			// --- Table 7.47 : poids et diviseurs de la prédiction DC ------------------
			// Indexé par le masque (P0|P1<<1|P2<<2|P3<<3), P0=L,P1=DL,P2=D,P3=DR.
			// Chaque entrée : {W0,W1,W2,W3, PDIV}. Les lignes non listées (0,2,...
			// sans voisin) ne sont jamais utilisées car il faut au moins un P non nul.
			struct DcWeights {
					int32 w[4];
					int32 pdiv;
			};
			// Masque = P0 + 2*P1 + 4*P2 + 8*P3 (P0=L,P1=DL,P2=D,P3=DR).
			static const DcWeights kDcPredWeights[16] = {
				{{0, 0, 0, 0}, 1},		 // 0000 (inutilisé)
				{{1, 0, 0, 0}, 1},		 // 0001 L
				{{0, 1, 0, 0}, 1},		 // 0010 DL
				{{1, 0, 0, 0}, 1},		 // 0011 L+DL
				{{0, 0, 1, 0}, 1},		 // 0100 D
				{{1, 0, 1, 0}, 2},		 // 0101 L+D
				{{0, 0, 1, 0}, 1},		 // 0110 DL+D
				{{29, -26, 29, 0}, 32},	 // 0111 L+DL+D
				{{0, 0, 0, 1}, 1},		 // 1000 DR
				{{75, 0, 0, 53}, 128},	 // 1001 L+DR
				{{0, 1, 0, 1}, 2},		 // 1010 DL+DR
				{{75, 0, 0, 53}, 128},	 // 1011 L+DL+DR
				{{0, 0, 1, 0}, 1},		 // 1100 D+DR
				{{75, 0, 0, 53}, 128},	 // 1101 L+D+DR
				{{0, 3, 10, 3}, 16},	 // 1110 DL+D+DR
				{{29, -26, 29, 0}, 32},	 // 1111 L+DL+D+DR
			};

			// --- Table 7.42 : groupe de table de Huffman selon l'index de token ti ----
			static inline int32 HuffGroup(int32 ti) {
				if (ti == 0)
					return 0;
				if (ti <= 5)
					return 1;
				if (ti <= 14)
					return 2;
				if (ti <= 27)
					return 3;
				return 4;
			}

		} // namespace theora
	} // namespace media
} // namespace nkentseu
