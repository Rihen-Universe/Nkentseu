// =============================================================================
// NKMedia/Codecs/Video/Mpeg2/NkMpeg2Decoder.cpp — décodeur MPEG-2 (ISO 13818-2).
// Voir NkMpeg2Decoder.h. Zero-STL, namespace nkentseu::media.
// =============================================================================
#include "NKMedia/Codecs/Video/Mpeg2/NkMpeg2Decoder.h"
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.h"
#include "NKMedia/Codecs/Video/H264/NkH264BitReader.h"

#include <cstring>

namespace nkentseu {
	namespace media {

		namespace {

			// ---------------------------------------------------------------------
			// Constantes / utilitaires
			// ---------------------------------------------------------------------
			inline int32 Clip255(int32 v) {
				return v < 0 ? 0 : (v > 255 ? 255 : v);
			}

			// Table quantiser_scale non linéaire (q_scale_type=1), 13818-2 Table 7-6.
			const int32 kNonLinearQscale[32] = {0,  1,  2,  3,  4,  5,	6,	7,	8,	10, 12,
												14, 16, 18, 20, 22, 24, 28, 32, 36, 40, 44,
												48, 52, 56, 64, 72, 80, 88, 96, 104, 112};

			// Balayage zig-zag standard (raster index en fonction de la position de scan).
			const uint8 kZigZag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
									   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
									   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
									   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
			// Balayage alternatif MPEG-2 (alternate_scan=1), Figure 7-3.
			const uint8 kAltScan[64] = {0,  8,  16, 24, 1,  9,  2,  10, 17, 25, 32, 40, 48, 56, 57, 49,
										41, 33, 26, 18, 3,  11, 4,  12, 19, 27, 34, 42, 50, 58, 35, 43,
										51, 59, 20, 28, 5,  13, 6,  14, 21, 29, 36, 44, 52, 60, 37, 45,
										53, 61, 22, 30, 7,  15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63};

			// Constantes de l'IDCT « simple » de ffmpeg (bit-exact avec `-idct simple`).
			enum {
				W1 = 22725,
				W2 = 21407,
				W3 = 19266,
				W4 = 16383,
				W5 = 12873,
				W6 = 8867,
				W7 = 4520,
				ROW_SHIFT = 11,
				COL_SHIFT = 20
			};

			void IdctRow(int32 *row) {
				// Cas « DC seul » : une ligne sans coefficient AC vaut exactement dc*8
				// (raccourci du même IDCT de référence — W4/2^11 = 7,9995… ; sans ce cas,
				// les grands |dc| dérivent d'une unité par rapport à l'oracle).
				if ((row[1] | row[2] | row[3] | row[4] | row[5] | row[6] | row[7]) == 0) {
					const int32 dc = row[0] * 8;
					for (int32 i = 0; i < 8; ++i)
						row[i] = dc;
					return;
				}
				int32 a0, a1, a2, a3, b0, b1, b2, b3;
				a0 = W4 * row[0] + (1 << (ROW_SHIFT - 1));
				a1 = a0;
				a2 = a0;
				a3 = a0;
				a0 += W2 * row[2];
				a1 += W6 * row[2];
				a2 -= W6 * row[2];
				a3 -= W2 * row[2];
				a0 += W4 * row[4] + W6 * row[6];
				a1 += -W4 * row[4] - W2 * row[6];
				a2 += -W4 * row[4] + W2 * row[6];
				a3 += W4 * row[4] - W6 * row[6];
				b0 = W1 * row[1];
				b1 = W3 * row[1];
				b2 = W5 * row[1];
				b3 = W7 * row[1];
				b0 += W3 * row[3];
				b1 -= W7 * row[3];
				b2 -= W1 * row[3];
				b3 -= W5 * row[3];
				b0 += W5 * row[5];
				b1 -= W1 * row[5];
				b2 += W7 * row[5];
				b3 += W3 * row[5];
				b0 += W7 * row[7];
				b1 -= W5 * row[7];
				b2 += W3 * row[7];
				b3 -= W1 * row[7];
				row[0] = (a0 + b0) >> ROW_SHIFT;
				row[7] = (a0 - b0) >> ROW_SHIFT;
				row[1] = (a1 + b1) >> ROW_SHIFT;
				row[6] = (a1 - b1) >> ROW_SHIFT;
				row[2] = (a2 + b2) >> ROW_SHIFT;
				row[5] = (a2 - b2) >> ROW_SHIFT;
				row[3] = (a3 + b3) >> ROW_SHIFT;
				row[4] = (a3 - b3) >> ROW_SHIFT;
			}

			// Colonne : produit les résidus (pas de clamp — la reconstruction clampe).
			void IdctCol(int32 *blk, int32 col, int32 *out /*8 valeurs, stride 8*/) {
				int32 a0, a1, a2, a3, b0, b1, b2, b3;
				int32 *c = blk + col;
				// L'arrondi de la passe colonne est REPLIÉ DANS le produit W4 (dc + r) avec
				// r = (1<<19)/W4 — et non ajouté après coup : l'écart de 32 dans
				// l'accumulateur décale sinon certains pixels d'une unité vs l'oracle.
				a0 = W4 * (c[8 * 0] + (1 << (COL_SHIFT - 1)) / W4);
				a1 = a0;
				a2 = a0;
				a3 = a0;
				a0 += W2 * c[8 * 2];
				a1 += W6 * c[8 * 2];
				a2 -= W6 * c[8 * 2];
				a3 -= W2 * c[8 * 2];
				a0 += W4 * c[8 * 4] + W6 * c[8 * 6];
				a1 += -W4 * c[8 * 4] - W2 * c[8 * 6];
				a2 += -W4 * c[8 * 4] + W2 * c[8 * 6];
				a3 += W4 * c[8 * 4] - W6 * c[8 * 6];
				b0 = W1 * c[8 * 1];
				b1 = W3 * c[8 * 1];
				b2 = W5 * c[8 * 1];
				b3 = W7 * c[8 * 1];
				b0 += W3 * c[8 * 3];
				b1 -= W7 * c[8 * 3];
				b2 -= W1 * c[8 * 3];
				b3 -= W5 * c[8 * 3];
				b0 += W5 * c[8 * 5];
				b1 -= W1 * c[8 * 5];
				b2 += W7 * c[8 * 5];
				b3 += W3 * c[8 * 5];
				b0 += W7 * c[8 * 7];
				b1 -= W5 * c[8 * 7];
				b2 += W3 * c[8 * 7];
				b3 -= W1 * c[8 * 7];
				out[8 * 0 + col] = (a0 + b0) >> COL_SHIFT;
				out[8 * 7 + col] = (a0 - b0) >> COL_SHIFT;
				out[8 * 1 + col] = (a1 + b1) >> COL_SHIFT;
				out[8 * 6 + col] = (a1 - b1) >> COL_SHIFT;
				out[8 * 2 + col] = (a2 + b2) >> COL_SHIFT;
				out[8 * 5 + col] = (a2 - b2) >> COL_SHIFT;
				out[8 * 3 + col] = (a3 + b3) >> COL_SHIFT;
				out[8 * 4 + col] = (a3 - b3) >> COL_SHIFT;
			}

			// IDCT 8x8 complet (blk = coefficients F'' en ordre raster, modifié). out = résidus.
			void Idct8x8(int32 *blk, int32 *out) {
				for (int32 r = 0; r < 8; ++r)
					IdctRow(blk + r * 8);
				// ffmpeg stocke les résultats de la passe ligne en int16 entre les deux passes.
				for (int32 i = 0; i < 64; ++i)
					blk[i] = (int32)(int16)blk[i];
				for (int32 c = 0; c < 8; ++c)
					IdctCol(blk, c, out);
			}

			// ---------------------------------------------------------------------
			// Tables VLC de décodage MPEG-2 (Annexe B)
			// ---------------------------------------------------------------------
			struct VlcEntry {
					uint16 code; // motif MSB-first sur `bits` bits
					uint8 bits;
					int32 value;
			};

			// Table B-1 — macroblock_address_increment (valeur 1..33). L'escape (0000 0001 000)
			// et le stuffing (0000 0001 111) sont gérés séparément.
			const VlcEntry kMbAddrInc[] = {
				{0b1, 1, 1},		 {0b011, 3, 2},		  {0b010, 3, 3},		{0b0011, 4, 4},
				{0b0010, 4, 5},		 {0b00011, 5, 6},	  {0b00010, 5, 7},		{0b0000111, 7, 8},
				{0b0000110, 7, 9},	 {0b00001011, 8, 10}, {0b00001010, 8, 11},	{0b00001001, 8, 12},
				{0b00001000, 8, 13}, {0b00000111, 8, 14}, {0b00000110, 8, 15},	{0b0000010111, 10, 16},
				{0b0000010110, 10, 17}, {0b0000010101, 10, 18}, {0b0000010100, 10, 19},
				{0b0000010011, 10, 20}, {0b0000010010, 10, 21}, {0b00000100011, 11, 22},
				{0b00000100010, 11, 23}, {0b00000100001, 11, 24}, {0b00000100000, 11, 25},
				{0b00000011111, 11, 26}, {0b00000011110, 11, 27}, {0b00000011101, 11, 28},
				{0b00000011100, 11, 29}, {0b00000011011, 11, 30}, {0b00000011010, 11, 31},
				{0b00000011001, 11, 32}, {0b00000011000, 11, 33},
			};

			// Masques de flags pour macroblock_type.
			enum { MB_INTRA = 1, MB_FWD = 2, MB_BWD = 4, MB_PAT = 8, MB_QUANT = 16 };

			// Table B-2 — I pictures.
			const VlcEntry kMbTypeI[] = {
				{0b1, 1, MB_INTRA},
				{0b01, 2, MB_INTRA | MB_QUANT},
			};
			// Table B-3 — P pictures.
			const VlcEntry kMbTypeP[] = {
				{0b1, 1, MB_FWD | MB_PAT},
				{0b01, 2, MB_PAT},
				{0b001, 3, MB_FWD},
				{0b00011, 5, MB_INTRA},
				{0b00010, 5, MB_QUANT | MB_FWD | MB_PAT},
				{0b00001, 5, MB_QUANT | MB_PAT},
				{0b000001, 6, MB_QUANT | MB_INTRA},
			};
			// Table B-4 — B pictures.
			const VlcEntry kMbTypeB[] = {
				{0b10, 2, MB_FWD | MB_BWD},
				{0b11, 2, MB_FWD | MB_BWD | MB_PAT},
				{0b010, 3, MB_BWD},
				{0b011, 3, MB_BWD | MB_PAT},
				{0b0010, 4, MB_FWD},
				{0b0011, 4, MB_FWD | MB_PAT},
				{0b00011, 5, MB_INTRA},
				{0b00010, 5, MB_QUANT | MB_FWD | MB_BWD | MB_PAT},
				{0b000011, 6, MB_QUANT | MB_FWD | MB_PAT},
				{0b000010, 6, MB_QUANT | MB_BWD | MB_PAT},
				{0b000001, 6, MB_QUANT | MB_INTRA},
			};

			// Run/level de la Table B-14 (dct_coefficients_0), indexés comme
			// NkMpeg1Tables::AcVlc() (0..110). ⚠️ On NE réutilise PAS NkMpeg1Tables::AcRun/AcLevel :
			// leur `kAcLevel[111]` encodeur n'a que 107 valeurs explicites → les indices 107..110
			// (runs 28..31, niveau 1) y valent 0 (zero-init C++), ce qui décode ces coefficients
			// avec un niveau erroné 0 (bug latent invisible côté encodeur qui ne les émet jamais).
			// Table complète et vérifiée (structure ff_mpeg1_run/level de ffmpeg).
			const int8 kAcRunT[111] = {
				0,  0,  0,  0,  0,  0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
				0,  0,  0,  0,  0,  0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
				1,  1,  1,  1,  1,  1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	2,	2,
				2,  2,  2,  3,  3,  3,	3,	4,	4,	4,	5,	5,	5,	6,	6,	6,	7,	7,	8,	8,
				9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 18, 19, 20,
				21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
			const int8 kAcLevelT[111] = {
				1,  2,	3,	4,	5,	6,	7,	8,	9,	10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
				21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
				1,  2,	3,	4,	5,	6,	7,	8,	9,	10, 11, 12, 13, 14, 15, 16, 17, 18, 1,	2,
				3,  4,	5,	1,	2,	3,	4,	1,	2,	3,	1,	2,	3,	1,	2,	3,	1,	2,	1,	2,
				1,  2,	1,	2,	1,	2,	1,	2,	1,	2,	1,	2,	1,	2,	1,	2,	1,	1,	1,	1,
				1,  1,	1,	1,	1,	1,	1,	1,	1,	1,	1};

			// Table B-15 — DCT coefficients Table one (intra_vlc_format=1). Transcrite du
			// TEXTE d'ISO/IEC 13818-2:1995 (Annexe B, Table B-15, pages 166-169) : chaque
			// entrée = {code MSB-first, nb de bits, run, level} ; le bit de signe « s » est
			// lu séparément. End of Block = « 0110 » (4 bits) et Escape = « 0000 01 »
			// (6 bits), gérés à part dans DecodeAcCoeffB15. Contrairement à B-14, PAS de
			// règle spéciale « premier coefficient » (le code « 10s » vaut toujours
			// run 0/level 1). Utilisée pour les blocs INTRA uniquement (7.2.2.1) ; les
			// blocs non-intra restent sur B-14 quel que soit intra_vlc_format.
			struct AcB15Entry {
					uint16 code;
					uint8 bits;
					uint8 run;
					uint8 level;
			};
			const AcB15Entry kAcB15[111] = {
				// --- page 166 (Table B-15, 1re partie)
				{0b10, 2, 0, 1},
				{0b010, 3, 1, 1},
				{0b110, 3, 0, 2},
				{0b00101, 5, 2, 1},
				{0b0111, 4, 0, 3},
				{0b00111, 5, 3, 1},
				{0b000110, 6, 4, 1},
				{0b00110, 5, 1, 2},
				{0b000111, 6, 5, 1},
				{0b0000110, 7, 6, 1},
				{0b0000100, 7, 7, 1},
				{0b11100, 5, 0, 4},
				{0b0000111, 7, 2, 2},
				{0b0000101, 7, 8, 1},
				{0b1111000, 7, 9, 1},
				{0b11101, 5, 0, 5},
				{0b000101, 6, 0, 6},
				{0b1111001, 7, 1, 3},
				{0b00100110, 8, 3, 2},
				{0b1111010, 7, 10, 1},
				{0b00100001, 8, 11, 1},
				{0b00100101, 8, 12, 1},
				{0b00100100, 8, 13, 1},
				{0b000100, 6, 0, 7},
				{0b00100111, 8, 1, 4},
				{0b11111100, 8, 2, 3},
				{0b11111101, 8, 4, 2},
				{0b000000100, 9, 5, 2},
				{0b000000101, 9, 14, 1},
				{0b000000111, 9, 15, 1},
				{0b0000001101, 10, 16, 1},
				// --- page 167 (continued)
				{0b1111011, 7, 0, 8},
				{0b1111100, 7, 0, 9},
				{0b00100011, 8, 0, 10},
				{0b00100010, 8, 0, 11},
				{0b00100000, 8, 1, 5},
				{0b0000001100, 10, 2, 4},
				{0b000000011100, 12, 3, 3},
				{0b000000010010, 12, 4, 3},
				{0b000000011110, 12, 6, 2},
				{0b000000010101, 12, 7, 2},
				{0b000000010001, 12, 8, 2},
				{0b000000011111, 12, 17, 1},
				{0b000000011010, 12, 18, 1},
				{0b000000011001, 12, 19, 1},
				{0b000000010111, 12, 20, 1},
				{0b000000010110, 12, 21, 1},
				{0b11111010, 8, 0, 12},
				{0b11111011, 8, 0, 13},
				{0b11111110, 8, 0, 14},
				{0b11111111, 8, 0, 15},
				{0b0000000010110, 13, 1, 6},
				{0b0000000010101, 13, 1, 7},
				{0b0000000010100, 13, 2, 5},
				{0b0000000010011, 13, 3, 4},
				{0b0000000010010, 13, 5, 3},
				{0b0000000010001, 13, 9, 2},
				{0b0000000010000, 13, 10, 2},
				{0b0000000011111, 13, 22, 1},
				{0b0000000011110, 13, 23, 1},
				{0b0000000011101, 13, 24, 1},
				{0b0000000011100, 13, 25, 1},
				{0b0000000011011, 13, 26, 1},
				// --- page 168 (continued — identique à B-14 sur ces longueurs)
				{0b00000000011111, 14, 0, 16},
				{0b00000000011110, 14, 0, 17},
				{0b00000000011101, 14, 0, 18},
				{0b00000000011100, 14, 0, 19},
				{0b00000000011011, 14, 0, 20},
				{0b00000000011010, 14, 0, 21},
				{0b00000000011001, 14, 0, 22},
				{0b00000000011000, 14, 0, 23},
				{0b00000000010111, 14, 0, 24},
				{0b00000000010110, 14, 0, 25},
				{0b00000000010101, 14, 0, 26},
				{0b00000000010100, 14, 0, 27},
				{0b00000000010011, 14, 0, 28},
				{0b00000000010010, 14, 0, 29},
				{0b00000000010001, 14, 0, 30},
				{0b00000000010000, 14, 0, 31},
				{0b000000000011000, 15, 0, 32},
				{0b000000000010111, 15, 0, 33},
				{0b000000000010110, 15, 0, 34},
				{0b000000000010101, 15, 0, 35},
				{0b000000000010100, 15, 0, 36},
				{0b000000000010011, 15, 0, 37},
				{0b000000000010010, 15, 0, 38},
				{0b000000000010001, 15, 0, 39},
				{0b000000000010000, 15, 0, 40},
				{0b000000000011111, 15, 1, 8},
				{0b000000000011110, 15, 1, 9},
				{0b000000000011101, 15, 1, 10},
				{0b000000000011100, 15, 1, 11},
				{0b000000000011011, 15, 1, 12},
				{0b000000000011010, 15, 1, 13},
				{0b000000000011001, 15, 1, 14},
				// --- page 169 (concluded — identique à B-14 sur ces longueurs)
				{0b0000000000010011, 16, 1, 15},
				{0b0000000000010010, 16, 1, 16},
				{0b0000000000010001, 16, 1, 17},
				{0b0000000000010000, 16, 1, 18},
				{0b0000000000010100, 16, 6, 3},
				{0b0000000000011010, 16, 11, 2},
				{0b0000000000011001, 16, 12, 2},
				{0b0000000000011000, 16, 13, 2},
				{0b0000000000010111, 16, 14, 2},
				{0b0000000000010110, 16, 15, 2},
				{0b0000000000010101, 16, 16, 2},
				{0b0000000000011111, 16, 27, 1},
				{0b0000000000011110, 16, 28, 1},
				{0b0000000000011101, 16, 29, 1},
				{0b0000000000011100, 16, 30, 1},
				{0b0000000000011011, 16, 31, 1},
			};

			int32 MatchVlc(NkH264BitReader &br, const VlcEntry *tab, int32 n) {
				for (int32 i = 0; i < n; ++i) {
					if (br.Peek((int32)tab[i].bits) == tab[i].code) {
						br.Skip((int32)tab[i].bits);
						return tab[i].value;
					}
				}
				return -0x7fffffff; // non trouvé
			}

			// macroblock_address_increment (gère l'escape +33 et le stuffing).
			int32 DecodeMbAddrIncrement(NkH264BitReader &br) {
				int32 total = 0;
				for (;;) {
					// escape 0000 0001 000 (11 bits) → +33
					if (br.Peek(11) == 0b00000001000) {
						br.Skip(11);
						total += 33;
						continue;
					}
					// stuffing 0000 0001 111 (11 bits) → ignorer
					if (br.Peek(11) == 0b00000001111) {
						br.Skip(11);
						continue;
					}
					int32 v = MatchVlc(br, kMbAddrInc, (int32)(sizeof(kMbAddrInc) / sizeof(kMbAddrInc[0])));
					if (v == -0x7fffffff)
						return -1;
					total += v;
					return total;
				}
			}

			// Table B-9 — coded_block_pattern (réutilise la table encodeur MPEG-1, identique).
			int32 DecodeCbp(NkH264BitReader &br) {
				const uint8(*cbpv)[2] = NkMpeg1Tables::CbpVlc();
				for (int32 cbp = 1; cbp < 64; ++cbp) {
					const int32 bits = cbpv[cbp][1];
					if (bits > 0 && (int32)br.Peek(bits) == (int32)cbpv[cbp][0]) {
						br.Skip(bits);
						return cbp;
					}
				}
				return -1;
			}

			// Table B-12/B-13 — dct_dc_size (réutilise la table encodeur MPEG-1, identique).
			int32 DecodeDcSize(NkH264BitReader &br, bool luma) {
				const uint16 *code = luma ? NkMpeg1Tables::DcLumCode() : NkMpeg1Tables::DcChromaCode();
				const uint8 *bits = luma ? NkMpeg1Tables::DcLumBits() : NkMpeg1Tables::DcChromaBits();
				for (int32 i = 0; i < 12; ++i) {
					if ((int32)br.Peek((int32)bits[i]) == (int32)code[i]) {
						br.Skip((int32)bits[i]);
						return i;
					}
				}
				return -1;
			}

			// Table B-10 — motion_code (magnitude via table encodeur MPEG-1 + bit de signe).
			int32 DecodeMotionCode(NkH264BitReader &br) {
				const uint8(*mv)[2] = NkMpeg1Tables::MotionVlc();
				for (int32 mag = 0; mag <= 16; ++mag) {
					if ((int32)br.Peek((int32)mv[mag][1]) == (int32)mv[mag][0]) {
						br.Skip((int32)mv[mag][1]);
						if (mag == 0)
							return 0;
						const int32 sign = (int32)br.U1();
						return sign ? -mag : mag;
					}
				}
				return 0x7fffffff;
			}

			// Table B-14 — coefficients DCT (run/level). Renvoie true si un coefficient est lu,
			// false sur EOB. `first` = premier coefficient d'un bloc NON-intra.
			bool DecodeAcCoeff(NkH264BitReader &br, bool first, int32 &run, int32 &level) {
				if (first) {
					// Contexte « premier coefficient » : '1s' = (run 0, level ±1).
					if (br.Peek(1) == 1) {
						br.Skip(1);
						const int32 sign = (int32)br.U1();
						run = 0;
						level = sign ? -1 : 1;
						return true;
					}
				}
				const uint16(*ac)[2] = NkMpeg1Tables::AcVlc();
				const int8 *acRun = kAcRunT;
				const int8 *acLevel = kAcLevelT;
				for (int32 i = 0; i < 113; ++i) {
					if ((int32)br.Peek((int32)ac[i][1]) == (int32)ac[i][0]) {
						br.Skip((int32)ac[i][1]);
						if (i == NkMpeg1Tables::kAcEob)
							return false; // EOB
						if (i == NkMpeg1Tables::kAcEscape) {
							// MPEG-2 : run(6) + level(12 signés, complément à deux).
							run = (int32)br.U(6);
							int32 lv = (int32)br.U(12);
							if (lv & 0x800)
								lv -= 0x1000;
							level = lv;
							return true;
						}
						run = (int32)acRun[i];
						const int32 absl = (int32)acLevel[i];
						const int32 sign = (int32)br.U1();
						level = sign ? -absl : absl;
						return true;
					}
				}
				// Rien : forcer EOB pour éviter une boucle infinie.
				return false;
			}

			// Table B-15 — coefficients DCT « Table one » (blocs intra quand
			// intra_vlc_format=1). Renvoie true si un coefficient est lu, false sur EOB.
			// Pas de contexte « premier coefficient » (contrairement à B-14).
			bool DecodeAcCoeffB15(NkH264BitReader &br, int32 &run, int32 &level) {
				// End of Block : « 0110 » (4 bits).
				if (br.Peek(4) == 0b0110) {
					br.Skip(4);
					return false;
				}
				// Escape : « 0000 01 » (6 bits) puis run(6) + level(12 signé, compl. à 2).
				if (br.Peek(6) == 0b000001) {
					br.Skip(6);
					run = (int32)br.U(6);
					int32 lv = (int32)br.U(12);
					if (lv & 0x800)
						lv -= 0x1000;
					level = lv;
					return true;
				}
				for (int32 i = 0; i < 111; ++i) {
					const AcB15Entry &e = kAcB15[i];
					if (br.Peek((int32)e.bits) == (uint32)e.code) {
						br.Skip((int32)e.bits);
						run = (int32)e.run;
						const int32 sign = (int32)br.U1();
						level = sign ? -(int32)e.level : (int32)e.level;
						return true;
					}
				}
				// Rien : forcer EOB pour éviter une boucle infinie (flux malformé).
				return false;
			}

			// ---------------------------------------------------------------------
			// État séquence / image
			// ---------------------------------------------------------------------
			struct SeqState {
					int32 width = 0, height = 0;
					int32 chromaFormat = 1; // 1=4:2:0, 2=4:2:2, 3=4:4:4
					int32 progressiveSeq = 1; // progressive_sequence (sequence extension)
					int32 mbW = 0, mbH = 0;
					int32 intraMatrix[64];		// ordre raster
					int32 nonIntraMatrix[64];	// ordre raster
					bool haveSeq = false;

					// mb_height (6.3.3) : trames progressives = ceil(h/16) ; séquence
					// ENTRELACÉE (frame pictures) = 2*ceil(h/32) (arrondi par paire de champs).
					void UpdateMbSize() {
						mbW = (width + 15) / 16;
						mbH = progressiveSeq ? (height + 15) / 16 : 2 * ((height + 31) / 32);
					}
			};

			struct PicState {
					int32 pictType = 0;
					int32 temporalRef = 0;
					int32 fCode[2][2] = {{15, 15}, {15, 15}};
					int32 intraDcPrecision = 0;
					int32 pictureStructure = 3;
					int32 framePredFrameDct = 1;
					int32 concealmentMv = 0;
					int32 qScaleType = 0;
					int32 intraVlcFormat = 0;
					int32 altScan = 0;
					int32 progressiveFrame = 1;
			};

			void LoadDefaultMatrices(SeqState &s) {
				const uint16 *im = NkMpeg1Tables::IntraMatrix();
				for (int32 i = 0; i < 64; ++i) {
					s.intraMatrix[i] = (int32)im[i];
					s.nonIntraMatrix[i] = 16;
				}
			}

			// ---------------------------------------------------------------------
			// Décodeur (état global d'une passe)
			// ---------------------------------------------------------------------
			struct Decoder {
					SeqState seq;
					PicState pic;
					NkMpeg2Frame cur;					// image en cours de décodage
					NkMpeg2Frame refFwd, refBwd;		// références (forward = ancienne, backward = récente)
					bool haveFwd = false, haveBwd = false;
					NkMpeg2Frame heldAnchor;			// ancre en attente d'affichage
					bool haveHeld = false;
					bool picInProgress = false;
					bool refused = false;
					char err[256];

					NkVector<NkMpeg2Frame> *out = nullptr;

					// Prédicteurs par macrobloc (réinitialisés par tranche).
					int32 dcPred[3];
					// PMV[r][s][t] (7.6.3, Table 7-7) : r = 1er/2e vecteur du MB (champ
					// haut/bas en prédiction de champ), s = forward/backward, t = x/y.
					int32 pmv[2][2][2];
					int32 quantScaleCode = 1;
					// Mémoire du dernier MB (pour le skip en B).
					int32 lastMbFlags = 0;

					void ResetPmv() {
						for (int32 r = 0; r < 2; ++r)
							for (int32 s = 0; s < 2; ++s)
								pmv[r][s][0] = pmv[r][s][1] = 0;
					}

					void SetErr(const char *m) {
						if (!refused) {
							int32 i = 0;
							while (m[i] && i < 255) {
								err[i] = m[i];
								++i;
							}
							err[i] = 0;
							refused = true;
						}
					}

					void AllocFrame(NkMpeg2Frame &f) {
						f.lumaW = seq.mbW * 16;
						f.lumaH = seq.mbH * 16;
						const int32 cx = (seq.chromaFormat == 1) ? 2 : (seq.chromaFormat == 2 ? 2 : 1);
						const int32 cy = (seq.chromaFormat == 1) ? 2 : 1;
						f.chromaW = f.lumaW / cx;
						f.chromaH = f.lumaH / cy;
						f.width = seq.width;
						f.height = seq.height;
						f.dispChromaW = seq.width / cx;
						f.dispChromaH = seq.height / cy;
						f.y.Resize((usize)(f.lumaW * f.lumaH));
						f.cb.Resize((usize)(f.chromaW * f.chromaH));
						f.cr.Resize((usize)(f.chromaW * f.chromaH));
						memset(f.y.Data(), 0, (usize)(f.lumaW * f.lumaH));
						memset(f.cb.Data(), 0, (usize)(f.chromaW * f.chromaH));
						memset(f.cr.Data(), 0, (usize)(f.chromaW * f.chromaH));
					}

					// Échantillonne un plan de référence avec clamp aux bords (demi-pel bilinéaire).
					static int32 SamplePlane(const uint8 *p, int32 w, int32 h, int32 x, int32 y, int32 hx,
											 int32 hy) {
						auto at = [&](int32 xx, int32 yy) -> int32 {
							if (xx < 0)
								xx = 0;
							if (xx > w - 1)
								xx = w - 1;
							if (yy < 0)
								yy = 0;
							if (yy > h - 1)
								yy = h - 1;
							return (int32)p[yy * w + xx];
						};
						if (!hx && !hy)
							return at(x, y);
						if (hx && !hy)
							return (at(x, y) + at(x + 1, y) + 1) >> 1;
						if (!hx && hy)
							return (at(x, y) + at(x, y + 1) + 1) >> 1;
						return (at(x, y) + at(x + 1, y) + at(x, y + 1) + at(x + 1, y + 1) + 2) >> 2;
					}

					// Prédiction d'un bloc rectangulaire depuis une référence (accumulée dans pred[]).
					void PredictBlock(const uint8 *ref, int32 rw, int32 rh, int32 bx, int32 by, int32 bw,
									  int32 bh, int32 mvx, int32 mvy, int32 *pred, int32 stride, bool average) {
						const int32 ix = mvx >> 1, hx = mvx & 1;
						const int32 iy = mvy >> 1, hy = mvy & 1;
						for (int32 yy = 0; yy < bh; ++yy) {
							for (int32 xx = 0; xx < bw; ++xx) {
								const int32 v =
									SamplePlane(ref, rw, rh, bx + xx + ix, by + yy + iy, hx, hy);
								int32 &dst = pred[yy * stride + xx];
								dst = average ? ((dst + v + 1) >> 1) : v;
							}
						}
					}

					// Échantillonne un CHAMP (parité `par` : 0=haut, 1=bas) d'un plan de
					// référence — coordonnées et clamp en espace CHAMP (les lignes du champ
					// sont les lignes par, par+2, par+4… du plan). Demi-pel bilinéaire (7.6.4).
					static int32 SampleFieldPlane(const uint8 *p, int32 w, int32 h, int32 par, int32 x,
												  int32 y, int32 hx, int32 hy) {
						const int32 fh = (h - par + 1) >> 1; // nb de lignes du champ
						auto at = [&](int32 xx, int32 yy) -> int32 {
							if (xx < 0)
								xx = 0;
							if (xx > w - 1)
								xx = w - 1;
							if (yy < 0)
								yy = 0;
							if (yy > fh - 1)
								yy = fh - 1;
							return (int32)p[(yy * 2 + par) * w + xx];
						};
						if (!hx && !hy)
							return at(x, y);
						if (hx && !hy)
							return (at(x, y) + at(x + 1, y) + 1) >> 1;
						if (!hx && hy)
							return (at(x, y) + at(x, y + 1) + 1) >> 1;
						return (at(x, y) + at(x + 1, y) + at(x, y + 1) + at(x + 1, y + 1) + 2) >> 2;
					}

					// Prédiction de CHAMP dans une image FRAME (7.6.2.1) : bloc bw x bhField
					// pris dans le champ `refPar` de la référence (vecteur en coordonnées
					// champ), écrit ENTRELACÉ dans le buffer de prédiction du MB — la ligne
					// champ j va à la ligne (dstPar + 2*j) du bloc (organisation trame).
					void PredictFieldBlock(const uint8 *ref, int32 rw, int32 rh, int32 refPar,
										   int32 dstPar, int32 bx, int32 byField, int32 bw,
										   int32 bhField, int32 mvx, int32 mvy, int32 *pred,
										   int32 stride, bool average) {
						const int32 ix = mvx >> 1, hx = mvx & 1;
						const int32 iy = mvy >> 1, hy = mvy & 1;
						for (int32 j = 0; j < bhField; ++j) {
							for (int32 i = 0; i < bw; ++i) {
								const int32 v = SampleFieldPlane(ref, rw, rh, refPar, bx + i + ix,
																 byField + j + iy, hx, hy);
								int32 &dst = pred[(dstPar + 2 * j) * stride + i];
								dst = average ? ((dst + v + 1) >> 1) : v;
							}
						}
					}

					// Décode un bloc (coefficients F'' déquantifiés, ordre raster) → coef[64].
					void DecodeBlock(NkH264BitReader &br, int32 blockIdx, bool intra, int32 *coef) {
						for (int32 i = 0; i < 64; ++i)
							coef[i] = 0;
						const uint8 *scan = pic.altScan ? kAltScan : kZigZag;
						const bool luma = blockIdx < 4;
						const int32 cc = (blockIdx < 4) ? 0 : (blockIdx == 4 ? 1 : 2);

						int32 pos = 0;
						if (intra) {
							// DC : différentiel prédit.
							const int32 dcSize = DecodeDcSize(br, luma);
							int32 diff = 0;
							if (dcSize > 0) {
								const int32 v = (int32)br.U(dcSize);
								const int32 half = 1 << (dcSize - 1);
								diff = (v < half) ? (v - (1 << dcSize) + 1) : v;
							}
							dcPred[cc] += diff;
							coef[0] = dcPred[cc];
							pos = 1;
						}
						// Coefficients AC (et 1er coeff pour non-intra). Blocs intra avec
						// intra_vlc_format=1 -> Table B-15 ; sinon Table B-14 (7.2.2.1).
						const bool useB15 = intra && pic.intraVlcFormat != 0;
						bool firstNonIntra = !intra;
						for (;;) {
							int32 run, level;
							const bool got = useB15 ? DecodeAcCoeffB15(br, run, level)
													: DecodeAcCoeff(br, firstNonIntra, run, level);
							if (!got)
								break; // EOB
							firstNonIntra = false;
							pos += run;
							if (pos > 63)
								break; // flux malformé : garde-fou contre le dépassement du tableau
							coef[scan[pos]] = level;
							++pos;
							// NE PAS sortir ici quand pos atteint 64 : même un bloc « plein »
							// (64 coefficients) se termine par un code End of block (2 bits) dans la
							// syntaxe MPEG-2 — il faut le consommer via un DecodeAcCoeff supplémentaire
							// (sinon désynchronisation de 2 bits après chaque bloc plein).
						}

						// Déquantification.
						const int32 code = quantScaleCode;
						const int32 qs = pic.qScaleType ? kNonLinearQscale[code] : (code << 1);
						const int32 *W = intra ? seq.intraMatrix : seq.nonIntraMatrix;
						int32 mismatch;
						if (intra) {
							coef[0] = coef[0] * (1 << (3 - pic.intraDcPrecision));
							mismatch = coef[0] ^ 1;
							for (int32 j = 1; j < 64; ++j) {
								int32 lv = coef[j];
								if (lv) {
									int32 a = lv < 0 ? -lv : lv;
									a = (a * qs * W[j]) >> 4;
									lv = lv < 0 ? -a : a;
									coef[j] = lv;
								}
								mismatch ^= coef[j];
							}
						} else {
							mismatch = 1;
							for (int32 j = 0; j < 64; ++j) {
								int32 lv = coef[j];
								if (lv) {
									int32 a = lv < 0 ? -lv : lv;
									a = (((a << 1) + 1) * qs * W[j]) >> 5;
									lv = lv < 0 ? -a : a;
									coef[j] = lv;
								}
								mismatch ^= coef[j];
							}
						}
						coef[63] ^= (mismatch & 1);
					}

					// Reconstruit un bloc 8x8 dans un plan (intra: remplace, inter: ajoute au
					// pred). `rowStep` = 1 (DCT trame) ou 2 (DCT de champ : les 8 lignes du
					// bloc tombent une ligne de trame sur deux, 6.3.17.1 dct_type).
					void ReconBlock(int32 *coef, uint8 *plane, int32 pw, int32 x0, int32 y0,
									int32 rowStep, bool intra, int32 *predBlock /* nullptr si intra */) {
						int32 res[64];
						Idct8x8(coef, res);
						for (int32 yy = 0; yy < 8; ++yy) {
							for (int32 xx = 0; xx < 8; ++xx) {
								int32 v;
								if (intra)
									v = Clip255(res[yy * 8 + xx]);
								else
									v = Clip255(predBlock[yy * 8 + xx] + res[yy * 8 + xx]);
								plane[(y0 + yy * rowStep) * pw + (x0 + xx)] = (uint8)v;
							}
						}
					}

					// Calcule les vecteurs chroma (4:2:0 / 4:2:2) à partir des vecteurs luma.
					void ChromaMv(int32 mvx, int32 mvy, int32 &cmvx, int32 &cmvy) {
						if (seq.chromaFormat == 1) { // 4:2:0
							cmvx = mvx / 2;
							cmvy = mvy / 2;
						} else if (seq.chromaFormat == 2) { // 4:2:2
							cmvx = mvx / 2;
							cmvy = mvy;
						} else { // 4:4:4
							cmvx = mvx;
							cmvy = mvy;
						}
					}

					// Référence « forward » (prédiction avant) selon le type d'image :
					//  P : l'ancre la plus récente (= refBwd) ; B : l'ancre précédente (= refFwd).
					const NkMpeg2Frame *FwdRef() const {
						return (pic.pictType == 2) ? &refBwd : &refFwd;
					}
					// Référence « backward » (uniquement pour les B) : l'ancre suivante = refBwd.
					const NkMpeg2Frame *BwdRef() const {
						return &refBwd;
					}

					// Prédiction complète d'un macrobloc (luma 16x16 + chroma) dans `cur`.
					void PredictMb(int32 mbx, int32 mby, bool useFwd, bool useBwd, int32 fmx, int32 fmy,
								   int32 bmx, int32 bmy, int32 *predY, int32 *predCb, int32 *predCr) {
						const int32 lx = mbx * 16, ly = mby * 16;
						const int32 chBlkW = (seq.chromaFormat == 3) ? 16 : 8;
						const int32 chBlkH = (seq.chromaFormat == 1) ? 8 : 16;
						const int32 cxo = lx * cur.chromaW / cur.lumaW;
						const int32 cyo = ly * cur.chromaH / cur.lumaH;

						int32 fcx, fcy, bcx, bcy;
						ChromaMv(fmx, fmy, fcx, fcy);
						ChromaMv(bmx, bmy, bcx, bcy);

						if (useFwd && FwdRef()->y.Size() > 0) {
							const NkMpeg2Frame *r = FwdRef();
							PredictBlock(r->y.Data(), r->lumaW, r->lumaH, lx, ly, 16, 16, fmx, fmy, predY,
										 16, false);
							PredictBlock(r->cb.Data(), r->chromaW, r->chromaH, cxo, cyo, chBlkW, chBlkH,
										 fcx, fcy, predCb, chBlkW, false);
							PredictBlock(r->cr.Data(), r->chromaW, r->chromaH, cxo, cyo, chBlkW, chBlkH,
										 fcx, fcy, predCr, chBlkW, false);
						}
						if (useBwd && BwdRef()->y.Size() > 0) {
							const bool avg = useFwd;
							const NkMpeg2Frame *r = BwdRef();
							PredictBlock(r->y.Data(), r->lumaW, r->lumaH, lx, ly, 16, 16, bmx, bmy, predY,
										 16, avg);
							PredictBlock(r->cb.Data(), r->chromaW, r->chromaH, cxo, cyo, chBlkW, chBlkH,
										 bcx, bcy, predCb, chBlkW, avg);
							PredictBlock(r->cr.Data(), r->chromaW, r->chromaH, cxo, cyo, chBlkW, chBlkH,
										 bcx, bcy, predCr, chBlkW, avg);
						}
					}

					// Prédiction de champ d'un macrobloc COMPLET dans une image frame
					// (frame_motion_type = « Field-based », Table 6-17) : pour chaque champ
					// destination f (0=haut, 1=bas), le vecteur r=f (16x8 luma + 8x4 chroma
					// 4:2:0, 7.6.7.2) est appliqué au champ de référence choisi par
					// motion_vertical_field_select[f][s]. Vecteurs verticaux en coordonnées
					// CHAMP. 4:2:0 uniquement (gate posé dans DecodeSlice).
					void PredictMbFieldMode(int32 mbx, int32 mby, bool useFwd, bool useBwd,
											const int32 mvs[2][2][2], const int32 fsel[2][2],
											int32 *predY, int32 *predCb, int32 *predCr) {
						const int32 lx = mbx * 16, ly = mby * 16;
						const int32 cxo = lx * cur.chromaW / cur.lumaW;
						const int32 cyo = ly * cur.chromaH / cur.lumaH;
						for (int32 s = 0; s < 2; ++s) {
							if (s == 0 && !useFwd)
								continue;
							if (s == 1 && !useBwd)
								continue;
							const NkMpeg2Frame *r = (s == 0) ? FwdRef() : BwdRef();
							if (r->y.Size() == 0)
								continue;
							const bool avg = (s == 1) && useFwd;
							for (int32 f = 0; f < 2; ++f) {
								const int32 mvx = mvs[f][s][0], mvy = mvs[f][s][1];
								int32 cmvx, cmvy;
								ChromaMv(mvx, mvy, cmvx, cmvy);
								const int32 par = fsel[f][s];
								PredictFieldBlock(r->y.Data(), r->lumaW, r->lumaH, par, f, lx, ly >> 1,
												  16, 8, mvx, mvy, predY, 16, avg);
								PredictFieldBlock(r->cb.Data(), r->chromaW, r->chromaH, par, f, cxo,
												  cyo >> 1, 8, 4, cmvx, cmvy, predCb, 8, avg);
								PredictFieldBlock(r->cr.Data(), r->chromaW, r->chromaH, par, f, cxo,
												  cyo >> 1, 8, 4, cmvx, cmvy, predCr, 8, avg);
							}
						}
					}

					// Écrit un macrobloc prédit (sans résidu — pour les blocs non codés / skip).
					void WritePredMb(int32 mbx, int32 mby, const int32 *predY, const int32 *predCb,
									 const int32 *predCr) {
						const int32 lx = mbx * 16, ly = mby * 16;
						for (int32 yy = 0; yy < 16; ++yy)
							for (int32 xx = 0; xx < 16; ++xx)
								cur.y.Data()[(ly + yy) * cur.lumaW + (lx + xx)] =
									(uint8)Clip255(predY[yy * 16 + xx]);
						const int32 chBlkW = (seq.chromaFormat == 3) ? 16 : 8;
						const int32 chBlkH = (seq.chromaFormat == 1) ? 8 : 16;
						const int32 cxo = lx * cur.chromaW / cur.lumaW;
						const int32 cyo = ly * cur.chromaH / cur.lumaH;
						for (int32 yy = 0; yy < chBlkH; ++yy)
							for (int32 xx = 0; xx < chBlkW; ++xx) {
								cur.cb.Data()[(cyo + yy) * cur.chromaW + (cxo + xx)] =
									(uint8)Clip255(predCb[yy * chBlkW + xx]);
								cur.cr.Data()[(cyo + yy) * cur.chromaW + (cxo + xx)] =
									(uint8)Clip255(predCr[yy * chBlkW + xx]);
							}
					}

					// -------------------------------------------------------------
					// Décodage d'une tranche (slice)
					// -------------------------------------------------------------
					bool DecodeSlice(const uint8 *data, usize len, int32 sliceRow) {
						// Périmètre : images FRAME uniquement (progressives, ou entrelacées avec
						// DCT de champ / prédiction de champ / alternate_scan — 7.6.1/7.6.2.1).
						// Les FIELD PICTURES (picture_structure 1/2 : deux passes de décodage par
						// trame, sélection de champs de référence inter-trame) restent refusées
						// proprement — non émises par l'encodeur mpeg2video de ffmpeg.
						if (pic.pictureStructure != 3) {
							SetErr("field pictures (picture_structure != frame) non supporte "
								   "- images frame uniquement (progressives ou entrelacees)");
							return false;
						}
						// Le chemin entrelacé (DCT de champ + prédiction de champ) n'est câblé
						// que pour le 4:2:0 (organisation des blocs chroma 6.3.17.1).
						if (pic.framePredFrameDct == 0 && seq.chromaFormat != 1) {
							SetErr("frame_pred_frame_dct=0 avec chroma != 4:2:0 non supporte");
							return false;
						}
						NkH264BitReader br(data, len);
						quantScaleCode = (int32)br.U(5);
						if (quantScaleCode == 0)
							quantScaleCode = 1;
						// slice extension (intra_slice_flag etc.).
						if (br.Peek(1) == 1) {
							br.Skip(1); // intra_slice_flag=1 marker
							br.Skip(1); // intra_slice
							br.Skip(7); // reserved
							while (br.Peek(1) == 1) {
								br.Skip(1);
								br.Skip(8);
							}
						}
						br.Skip(1); // extra_bit_slice (=0)

						// Prédicteurs réinitialisés en début de tranche.
						const int32 dcReset = 1 << (7 + pic.intraDcPrecision);
						dcPred[0] = dcPred[1] = dcPred[2] = dcReset;
						ResetPmv();
						lastMbFlags = 0;

						int32 mbAddr = sliceRow * seq.mbW - 1;
						bool firstMb = true;

						while (br.BitsLeft() > 0 && br.Peek(23) != 0) {
							int32 inc = DecodeMbAddrIncrement(br);
							if (inc < 1)
								return false;

							if (firstMb) {
								// Positionne le premier MB codé (pas de MB sauté avant lui).
								mbAddr += inc;
								firstMb = false;
							} else {
								// (inc-1) macroblocs sautés, puis le MB codé.
								for (int32 k = 1; k < inc; ++k) {
									++mbAddr;
									if (mbAddr < 0 || mbAddr >= seq.mbW * seq.mbH)
										return false;
									if (!HandleSkippedMb(mbAddr))
										return false;
								}
								++mbAddr;
							}

							if (mbAddr < 0 || mbAddr >= seq.mbW * seq.mbH)
								return false;
							const int32 mbx = mbAddr % seq.mbW;
							const int32 mby = mbAddr / seq.mbW;

							if (!DecodeMacroblock(br, mbx, mby))
								return false;
						}
						return true;
					}

					// Gestion d'un MB sauté (P: copie forward mv0 ; B: reprend le mode précédent).
					bool HandleSkippedMb(int32 mbAddr) {
						const int32 mbx = mbAddr % seq.mbW;
						const int32 mby = mbAddr / seq.mbW;
						int32 predY[256], predCb[256], predCr[256];
						for (int32 i = 0; i < 256; ++i) {
							predY[i] = 0;
							predCb[i] = 0;
							predCr[i] = 0;
						}
						if (pic.pictType == 2) { // P : frame-based, mv 0, PMV remis à zéro (7.6.6.2)
							if (!haveBwd)
								return false;
							ResetPmv();
							PredictMb(mbx, mby, true, false, 0, 0, 0, 0, predY, predCb, predCr);
						} else if (pic.pictType == 3) { // B : frame-based, MVs = PMV, direction du MB précédent (7.6.6.4)
							const bool uf = (lastMbFlags & MB_FWD) != 0;
							const bool ub = (lastMbFlags & MB_BWD) != 0;
							if (!uf && !ub)
								return false;
							if ((uf && !haveFwd) || (ub && !haveBwd))
								return false;
							PredictMb(mbx, mby, uf, ub, pmv[0][0][0], pmv[0][0][1], pmv[0][1][0],
									  pmv[0][1][1], predY, predCb, predCr);
						} else {
							return false;
						}
						WritePredMb(mbx, mby, predY, predCb, predCr);
						return true;
					}

					// Lit un vecteur de mouvement motion_vector(r, s) et met à jour PMV[r][s]
					// (7.6.3.1). `fieldInFrame` = vecteur de CHAMP dans une image frame
					// (mv_format == field) : le prédicteur vertical est PMV DIV 2 (division
					// entière vers -inf, réalisée par >> 1 arithmétique) et le PMV mis à jour
					// vaut vector*2 — le vecteur vertical décodé est en coordonnées champ.
					void DecodeMotionVector(NkH264BitReader &br, int32 r, int32 s, bool fieldInFrame,
											int32 &mvx, int32 &mvy) {
						for (int32 t = 0; t < 2; ++t) {
							const int32 code = DecodeMotionCode(br);
							const int32 rSize = pic.fCode[s][t] - 1;
							int32 residual = 0;
							if (rSize != 0 && code != 0)
								residual = (int32)br.U(rSize);
							int32 delta;
							const int32 f = 1 << rSize;
							if (f == 1 || code == 0) {
								delta = code;
							} else {
								delta = ((code < 0 ? -code : code) - 1) * f + residual + 1;
								if (code < 0)
									delta = -delta;
							}
							const int32 high = (16 * f) - 1;
							const int32 low = -16 * f;
							const int32 range = 32 * f;
							const bool halfPred = fieldInFrame && t == 1;
							int32 pred = halfPred ? (pmv[r][s][t] >> 1) : pmv[r][s][t];
							pred += delta;
							if (pred < low)
								pred += range;
							if (pred > high)
								pred -= range;
							pmv[r][s][t] = halfPred ? pred * 2 : pred;
							if (t == 0)
								mvx = pred;
							else
								mvy = pred;
						}
					}

					bool DecodeMacroblock(NkH264BitReader &br, int32 mbx, int32 mby) {
						const VlcEntry *tab;
						int32 n;
						if (pic.pictType == 1) {
							tab = kMbTypeI;
							n = 2;
						} else if (pic.pictType == 2) {
							tab = kMbTypeP;
							n = 7;
						} else {
							tab = kMbTypeB;
							n = 11;
						}
						int32 flags = MatchVlc(br, tab, n);
						if (flags == -0x7fffffff)
							return false;

						const bool intra = (flags & MB_INTRA) != 0;
						const bool motionFwd = (flags & MB_FWD) != 0;
						const bool motionBwd = (flags & MB_BWD) != 0;
						const bool hasPattern = (flags & MB_PAT) != 0;
						const bool quant = (flags & MB_QUANT) != 0;

						// frame_motion_type (6.2.5.1, Table 6-17) — présent si mouvement et
						// frame_pred_frame_dct == 0. 10 = frame-based (1 vecteur/direction),
						// 01 = field-based (2 vecteurs/direction, un par champ),
						// 11 = dual-prime (refusé : jamais émis par l'encodeur ffmpeg, et
						// l'oracle manque pour le valider), 00 = réservé.
						int32 motionType = 2; // frame-based implicite (6.3.17.1)
						if (!intra && (motionFwd || motionBwd) && pic.framePredFrameDct == 0) {
							motionType = (int32)br.U(2);
							if (motionType == 0)
								return false; // réservé -> flux malformé
							if (motionType == 3) {
								SetErr("MPEG-2 dual-prime motion non supporte");
								return false;
							}
						}
						const bool fieldPred = !intra && (motionFwd || motionBwd) && motionType == 1;

						// dct_type (6.2.5.1) : DCT de champ possible seulement si
						// frame_pred_frame_dct == 0 et que le MB porte des coefficients.
						int32 dctType = 0;
						if (pic.framePredFrameDct == 0 && (intra || hasPattern))
							dctType = (int32)br.U(1);

						if (quant) {
							quantScaleCode = (int32)br.U(5);
							if (quantScaleCode == 0)
								quantScaleCode = 1;
						}

						// Vecteurs du MB : mvs[r][s][x/y] + sélection de champ fsel[r][s]
						// (motion_vertical_field_select, 6.2.5.2 : 0=champ haut, 1=champ bas).
						int32 mvs[2][2][2] = {{{0}}};
						int32 fsel[2][2] = {{0}};
						if (motionFwd || (intra && pic.concealmentMv)) {
							if (fieldPred) {
								for (int32 r = 0; r < 2; ++r) {
									fsel[r][0] = (int32)br.U1();
									DecodeMotionVector(br, r, 0, true, mvs[r][0][0], mvs[r][0][1]);
								}
							} else {
								DecodeMotionVector(br, 0, 0, false, mvs[0][0][0], mvs[0][0][1]);
								// Table 7-9 (frame-based) : PMV[1][0] = PMV[0][0].
								pmv[1][0][0] = pmv[0][0][0];
								pmv[1][0][1] = pmv[0][0][1];
							}
						}
						if (motionBwd) {
							if (fieldPred) {
								for (int32 r = 0; r < 2; ++r) {
									fsel[r][1] = (int32)br.U1();
									DecodeMotionVector(br, r, 1, true, mvs[r][1][0], mvs[r][1][1]);
								}
							} else {
								DecodeMotionVector(br, 0, 1, false, mvs[0][1][0], mvs[0][1][1]);
								// Table 7-9 (frame-based) : PMV[1][1] = PMV[0][1].
								pmv[1][1][0] = pmv[0][1][0];
								pmv[1][1][1] = pmv[0][1][1];
							}
						}
						if (intra && pic.concealmentMv)
							br.Skip(1); // marker_bit

						// Réinitialisation des prédicteurs de mouvement (7.6.3.4).
						if (intra && !pic.concealmentMv)
							ResetPmv();
						if (pic.pictType == 2 && !motionFwd && !intra) {
							// P : MB « pattern only » → mv 0 + reset PMV.
							ResetPmv();
						}

						// Prédicteurs DC réinitialisés si non-intra.
						if (!intra) {
							const int32 dcReset = 1 << (7 + pic.intraDcPrecision);
							dcPred[0] = dcPred[1] = dcPred[2] = dcReset;
						}

						int32 cbp = 0;
						if (intra)
							cbp = 0x3f; // tous les blocs codés
						else if (hasPattern) {
							cbp = DecodeCbp(br);
							if (cbp < 0)
								return false;
						} else
							cbp = 0;

						const int32 blockCount = (seq.chromaFormat == 1) ? 6 : (seq.chromaFormat == 2 ? 8 : 12);

						// Prédiction (inter).
						int32 predY[256], predCb[256], predCr[256];
						for (int32 i = 0; i < 256; ++i) {
							predY[i] = 0;
							predCb[i] = 0;
							predCr[i] = 0;
						}
						bool useFwd = false, useBwd = false;
						if (!intra) {
							if (pic.pictType == 2) {
								useFwd = true; // P : toujours forward (mv0 si pattern-only, 7.6.3.5)
							} else if (pic.pictType == 3) {
								useFwd = motionFwd;
								useBwd = motionBwd;
							}
							if (fieldPred)
								PredictMbFieldMode(mbx, mby, useFwd, useBwd, mvs, fsel, predY, predCb,
												   predCr);
							else
								PredictMb(mbx, mby, useFwd, useBwd, mvs[0][0][0], mvs[0][0][1],
										  mvs[0][1][0], mvs[0][1][1], predY, predCb, predCr);
						}

						// Blocs. Avec dct_type=1 (DCT de champ, 6.3.17.1), les 4 blocs luma
						// sont organisés par champ : blocs 0/1 = champ haut (lignes paires),
						// blocs 2/3 = champ bas (lignes impaires), chacun gauche/droite —
						// la ligne yy du bloc tombe sur la ligne de trame (b>>1) + 2*yy.
						// Les blocs chroma 4:2:0 restent organisés trame.
						const int32 lx = mbx * 16, ly = mby * 16;
						const int32 cxo = lx * cur.chromaW / cur.lumaW;
						const int32 cyo = ly * cur.chromaH / cur.lumaH;
						int32 coef[64];
						for (int32 b = 0; b < blockCount; ++b) {
							const bool coded = (cbp & (1 << (blockCount - 1 - b))) != 0;
							// position du bloc + organisation trame/champ
							int32 px, py, pw, rowStep;
							uint8 *plane;
							int32 *predPtr;
							int32 predStride, predRow0, predCol0;
							if (b < 4) {
								plane = cur.y.Data();
								pw = cur.lumaW;
								px = lx + (b & 1) * 8;
								predPtr = predY;
								predStride = 16;
								predCol0 = (b & 1) * 8;
								if (dctType) {
									py = ly + (b >> 1); // parité du champ
									rowStep = 2;
									predRow0 = (b >> 1);
								} else {
									py = ly + (b >> 1) * 8;
									rowStep = 1;
									predRow0 = (b >> 1) * 8;
								}
							} else {
								// chroma : pour 4:2:0 un seul bloc 8x8 par composante.
								const int32 comp = b - 4; // 0=Cb,1=Cr (4:2:0)
								plane = (comp == 0) ? cur.cb.Data() : cur.cr.Data();
								pw = cur.chromaW;
								px = cxo;
								py = cyo;
								rowStep = 1;
								predPtr = (comp == 0) ? predCb : predCr;
								predRow0 = 0;
								predCol0 = 0;
								predStride = (seq.chromaFormat == 3) ? 16 : 8;
							}

							if (intra) {
								DecodeBlock(br, b, true, coef);
								ReconBlock(coef, plane, pw, px, py, rowStep, true, nullptr);
							} else if (coded) {
								DecodeBlock(br, b, false, coef);
								// extrait le pred 8x8 correspondant (même organisation que le bloc)
								int32 pblk[64];
								for (int32 yy = 0; yy < 8; ++yy)
									for (int32 xx = 0; xx < 8; ++xx)
										pblk[yy * 8 + xx] =
											predPtr[(predRow0 + yy * rowStep) * predStride + predCol0 + xx];
								ReconBlock(coef, plane, pw, px, py, rowStep, false, pblk);
							} else {
								// non codé : écrire la prédiction seule
								for (int32 yy = 0; yy < 8; ++yy)
									for (int32 xx = 0; xx < 8; ++xx)
										plane[(py + yy * rowStep) * pw + (px + xx)] = (uint8)Clip255(
											predPtr[(predRow0 + yy * rowStep) * predStride + predCol0 + xx]);
							}
						}

						lastMbFlags = flags;
						return true;
					}

					// -------------------------------------------------------------
					// Réordonnancement / sortie
					// -------------------------------------------------------------
					void FinalizePicture() {
						if (!picInProgress)
							return;
						picInProgress = false;
						cur.poc = cur.temporalRef;

						if (pic.pictType == 1 || pic.pictType == 2) {
							// Ancre : décale les références.
							refFwd = refBwd;
							haveFwd = haveBwd;
							refBwd = cur;
							haveBwd = true;
							if (haveHeld)
								out->PushBack(heldAnchor);
							heldAnchor = cur;
							haveHeld = true;
						} else {
							// B : affichée immédiatement.
							out->PushBack(cur);
						}
					}

					void Flush() {
						if (haveHeld) {
							out->PushBack(heldAnchor);
							haveHeld = false;
						}
					}
			};

			// -------------------------------------------------------------------------
			// Parsing des en-têtes
			// -------------------------------------------------------------------------
			void ParseSequenceHeader(const uint8 *d, usize len, SeqState &s) {
				NkH264BitReader br(d, len);
				s.width = (int32)br.U(12);
				s.height = (int32)br.U(12);
				br.Skip(4);	 // aspect_ratio
				br.Skip(4);	 // frame_rate_code
				br.Skip(18); // bit_rate
				br.Skip(1);	 // marker
				br.Skip(10); // vbv_buffer_size
				br.Skip(1);	 // constrained_parameters
				LoadDefaultMatrices(s);
				if (br.U1()) { // load_intra_quantiser_matrix
					for (int32 i = 0; i < 64; ++i) {
						const int32 v = (int32)br.U(8);
						s.intraMatrix[kZigZag[i]] = v;
					}
				}
				if (br.U1()) { // load_non_intra_quantiser_matrix
					for (int32 i = 0; i < 64; ++i) {
						const int32 v = (int32)br.U(8);
						s.nonIntraMatrix[kZigZag[i]] = v;
					}
				}
				s.UpdateMbSize();
				s.haveSeq = true;
			}

			void ParseSequenceExtension(const uint8 *d, usize len, SeqState &s) {
				NkH264BitReader br(d, len);
				br.Skip(4); // extension id (=1)
				br.Skip(8); // profile_and_level
				s.progressiveSeq = (int32)br.U(1);
				s.chromaFormat = (int32)br.U(2);
				const int32 hExt = (int32)br.U(2);
				const int32 vExt = (int32)br.U(2);
				s.width |= (hExt << 12);
				s.height |= (vExt << 12);
				s.UpdateMbSize();
			}

			void ParsePictureHeader(const uint8 *d, usize len, PicState &p) {
				NkH264BitReader br(d, len);
				p.temporalRef = (int32)br.U(10);
				p.pictType = (int32)br.U(3);
				br.Skip(16); // vbv_delay
				if (p.pictType == 2 || p.pictType == 3) {
					br.Skip(1); // full_pel_forward
					br.Skip(3); // forward_f_code (MPEG-1, remplacé par picture_coding_extension)
				}
				if (p.pictType == 3) {
					br.Skip(1); // full_pel_backward
					br.Skip(3); // backward_f_code
				}
				// extra_information_picture (ignoré)
				// valeurs par défaut MPEG-2 si pas de picture_coding_extension.
				p.fCode[0][0] = p.fCode[0][1] = p.fCode[1][0] = p.fCode[1][1] = 15;
				p.intraDcPrecision = 0;
				p.pictureStructure = 3;
				p.framePredFrameDct = 1;
				p.concealmentMv = 0;
				p.qScaleType = 0;
				p.intraVlcFormat = 0;
				p.altScan = 0;
				p.progressiveFrame = 1;
			}

			void ParsePictureCodingExtension(const uint8 *d, usize len, PicState &p) {
				NkH264BitReader br(d, len);
				br.Skip(4); // extension id (=8)
				p.fCode[0][0] = (int32)br.U(4);
				p.fCode[0][1] = (int32)br.U(4);
				p.fCode[1][0] = (int32)br.U(4);
				p.fCode[1][1] = (int32)br.U(4);
				p.intraDcPrecision = (int32)br.U(2);
				p.pictureStructure = (int32)br.U(2);
				br.Skip(1); // top_field_first
				p.framePredFrameDct = (int32)br.U(1);
				p.concealmentMv = (int32)br.U(1);
				p.qScaleType = (int32)br.U(1);
				p.intraVlcFormat = (int32)br.U(1);
				p.altScan = (int32)br.U(1);
				br.Skip(1); // repeat_first_field
				br.Skip(1); // chroma_420_type
				p.progressiveFrame = (int32)br.U(1);
			}

		} // namespace

		// =========================================================================
		// API publique
		// =========================================================================
		bool NkMpeg2Decoder::DecodeAll(const uint8 *data, usize size, NkVector<NkMpeg2Frame> &out,
									   char *errmsg, usize errcap) {
			out.Clear();
			if (errmsg && errcap)
				errmsg[0] = 0;

			// 1) Localiser tous les start codes (00 00 01 xx).
			NkVector<usize> scOff;
			NkVector<uint8> scVal;
			{
				usize i = 0;
				while (i + 3 < size) {
					if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
						scOff.PushBack(i);
						scVal.PushBack(data[i + 3]);
						i += 4;
					} else {
						++i;
					}
				}
			}
			const usize nsc = scOff.Size();
			if (nsc == 0)
				return false;

			Decoder dec;
			dec.out = &out;

			auto payload = [&](usize idx, const uint8 *&d, usize &len) {
				const usize start = scOff[idx] + 4;
				const usize end = (idx + 1 < nsc) ? scOff[idx + 1] : size;
				d = data + start;
				len = (end > start) ? (end - start) : 0;
			};

			for (usize idx = 0; idx < nsc; ++idx) {
				const uint8 code = scVal[idx];
				const uint8 *d;
				usize len;
				payload(idx, d, len);

				if (code == 0xB3) {
					ParseSequenceHeader(d, len, dec.seq);
				} else if (code == 0xB5) {
					if (len > 0) {
						const int32 eid = (d[0] >> 4) & 0xF;
						if (eid == 1) {
							ParseSequenceExtension(d, len, dec.seq);
						} else if (eid == 8) {
							ParsePictureCodingExtension(d, len, dec.pic);
						}
					}
				} else if (code == 0xB8) {
					// GOP header — rien à faire (temporal_reference gère l'affichage).
				} else if (code == 0x00) {
					// Nouvelle image : finaliser la précédente.
					dec.FinalizePicture();
					if (!dec.seq.haveSeq)
						return false;
					ParsePictureHeader(d, len, dec.pic);
					dec.AllocFrame(dec.cur);
					dec.cur.pictType = dec.pic.pictType;
					dec.cur.temporalRef = dec.pic.temporalRef;
					dec.picInProgress = true;
				} else if (code >= 0x01 && code <= 0xAF) {
					if (dec.picInProgress) {
						const int32 sliceRow = (int32)code - 1;
						if (!dec.DecodeSlice(d, len, sliceRow)) {
							if (dec.refused) {
								if (errmsg && errcap) {
									usize k = 0;
									while (dec.err[k] && k + 1 < errcap) {
										errmsg[k] = dec.err[k];
										++k;
									}
									errmsg[k] = 0;
								}
								return false;
							}
							// Desync non fatal : on finalise ce qui a été décodé et on s'arrête.
							dec.FinalizePicture();
							dec.Flush();
							return out.Size() > 0;
						}
					}
				} else if (code == 0xB7) {
					// sequence_end_code
					dec.FinalizePicture();
				}
			}
			dec.FinalizePicture();
			dec.Flush();
			return out.Size() > 0;
		}

		bool NkMpeg2Decoder::SelfTest() {
			// Vérifie la cohérence des tables de base (pas d'ambiguïté triviale).
			// mb_addr_increment valeur 1 = '1'.
			if (kMbAddrInc[0].value != 1 || kMbAddrInc[0].bits != 1)
				return false;
			// IDCT d'un bloc plat (DC seul) doit donner une valeur constante.
			int32 blk[64];
			for (int32 i = 0; i < 64; ++i)
				blk[i] = 0;
			blk[0] = 1024; // DC correspondant à une valeur ~128
			int32 res[64];
			Idct8x8(blk, res);
			const int32 v0 = res[0];
			for (int32 i = 1; i < 64; ++i)
				if (res[i] != v0)
					return false;
			return v0 >= 120 && v0 <= 136;
		}

	} // namespace nkentseu
} // namespace nkentseu
