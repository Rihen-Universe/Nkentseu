// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcCtu.cpp — briques 5+6 : slice_segment_data()
// INTRA, du CABAC jusqu'aux PIXELS.
// -----------------------------------------------------------------------------
// Brique 5 (parse) : sao() (§7.3.8.3), coding_quadtree (§7.3.8.4), coding_unit
// intra (§7.3.8.5, modes luma via MPM §8.4.2 — le SCAN des résidus dépend du
// mode), transform_tree (§7.3.8.8), cu_qp_delta (§7.3.8.10), residual_coding
// (§7.3.8.11 complet), WPP (§9.3.1). Validation structurelle type tiles VP9.
//
// Brique 6 (reconstruction) : dérivation du QP par groupe de quantification
// (§8.6.1, chroma via table 4:2:0), déquant (§8.6.3), transformées inverses
// (DST 4×4 luma intra + DCT 4-32 en deux passes avec écrêtage 16 bits, §8.6.4),
// prédiction intra 35 modes (§8.4.4.2 : référence + substitution, lissage
// [1 2 1] + lissage fort 32×32, Planar/DC/angulaire + filtres de bord) et
// reconstruction par TU. Validation PIXELS vs ffmpeg sur flux SANS déblocage/
// SAO (les filtres en boucle = briques suivantes).
//
// Dérivations de contexte et ordre des opérations alignés sur la référence
// ffmpeg (libavcodec/hevc/{cabac,dsp_template,pred_template}.c, validée
// bit-exacte) ; tables de scan GÉNÉRÉES par le procédé normatif §6.5.3
// (vérifié identique aux tables ffmpeg par script).
//
// Restrictions (refus propre, briques suivantes) : slices P/B, tuiles, PCM,
// 4:2:2/4:4:4, scaling lists.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h"
#include "NKMedia/Codecs/Video/HEVC/NkHevcCabac.h"

namespace nkentseu {
	namespace media {

		namespace {

			int32 Min32(int32 a, int32 b) {
				return a < b ? a : b;
			}
			int32 Max32(int32 a, int32 b) {
				return a > b ? a : b;
			}
			int32 Clip3i(int32 lo, int32 hi, int32 v) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			int32 ClipInt16(int32 v) {
				return Clip3i(-32768, 32767, v);
			}
			int32 Abs32(int32 v) {
				return v < 0 ? -v : v;
			}

			// ---- Filtres d'interpolation MC (§8.5.4.2.2, Tableaux 8-12/8-13) -------
			// Luma : 1/4-pel (4 positions), 8 taps. Chroma 4:2:0 : 1/8-pel (8 positions,
			// même valeur en x/y — hshift=vshift=1 -> pas de mise à l'échelle
			// supplémentaire, cf. dérivation ff_hevc_hevcdec.c chroma_mc_uni), 4 taps.
			const int8 kHevcQpelFilters[4][8] = {
				{0, 0, 0, 64, 0, 0, 0, 0},
				{-1, 4, -10, 58, 17, -5, 1, 0},
				{-1, 4, -11, 40, 40, -11, 4, -1},
				{0, 1, -5, 17, 58, -10, 4, -1},
			};
			const int8 kHevcEpelFilters[8][4] = {
				{0, 64, 0, 0},
				{-2, 58, 10, -2},
				{-4, 54, 16, -2},
				{-6, 46, 28, -4},
				{-4, 36, 36, -4},
				{-4, 28, 46, -6},
				{-2, 16, 54, -4},
				{-2, 10, 58, -2},
			};

			// Ordre de génération des candidats de fusion bi combinés (§8.5.3.2.4,
			// ffmpeg mvs.c l0_l1_cand_idx) — [comb_idx] -> {idx candidat L0, idx candidat L1}.
			const uint8 kHevcL0L1CandIdx[12][2] = {
				{0, 1}, {1, 0}, {0, 2}, {2, 0}, {1, 2}, {2, 1},
				{0, 3}, {3, 0}, {1, 3}, {3, 1}, {2, 3}, {3, 2},
			};

			// ---- Tables de scan (générées, procédé normatif §6.5.3) ----------------
			struct ScanTables {
					uint8 diag4x4X[16], diag4x4Y[16];
					uint8 diag4x4Inv[4][4];
					uint8 diag2x2X[4], diag2x2Y[4];
					uint8 diag2x2Inv[2][2];
					uint8 diag8x8X[64], diag8x8Y[64];
					uint8 diag8x8Inv[8][8];
					uint8 rasterX[16], rasterY[16]; // scan horizontal 4x4 (raster)
					uint8 horiz2x2X[4], horiz2x2Y[4];
			};

			void BuildDiag(int32 n, uint8 *sx, uint8 *sy, uint8 *inv /*n*n, [y][x]*/) {
				int32 i = 0, x = 0, y = 0;
				while (true) {
					while (y >= 0) {
						if (x < n && y < n) {
							sx[i] = (uint8)x;
							sy[i] = (uint8)y;
							inv[y * n + x] = (uint8)i;
							++i;
						}
						--y;
						++x;
					}
					y = x;
					x = 0;
					if (i >= n * n)
						break;
				}
			}

			const ScanTables &Scans() {
				static ScanTables t;
				static bool built = false;
				if (!built) {
					BuildDiag(4, t.diag4x4X, t.diag4x4Y, &t.diag4x4Inv[0][0]);
					BuildDiag(2, t.diag2x2X, t.diag2x2Y, &t.diag2x2Inv[0][0]);
					BuildDiag(8, t.diag8x8X, t.diag8x8Y, &t.diag8x8Inv[0][0]);
					for (int32 i = 0; i < 16; ++i) {
						t.rasterX[i] = (uint8)(i & 3);
						t.rasterY[i] = (uint8)(i >> 2);
					}
					t.horiz2x2X[0] = 0;
					t.horiz2x2X[1] = 1;
					t.horiz2x2X[2] = 0;
					t.horiz2x2X[3] = 1;
					t.horiz2x2Y[0] = 0;
					t.horiz2x2Y[1] = 0;
					t.horiz2x2Y[2] = 1;
					t.horiz2x2Y[3] = 1;
					built = true;
				}
				return t;
			}

			// Index composé du scan horizontal sur 8x8 (sous-bloc raster puis intérieur
			// raster) — sert à num_coeff pour les scans horizontal/vertical (trafo 4 et 8).
			int32 HorizComposedIdx(int32 y, int32 x) {
				return (((y >> 2) * 2 + (x >> 2)) << 4) + (y & 3) * 4 + (x & 3);
			}

			// Carte de contexte sig_coeff_flag (§9.3.4.2.5, composée avec le scan —
			// disposition identique à la référence ffmpeg). [scanIdx][5*16].
			const uint8 kSigCtxIdxMap[3][5 * 16] = {
				{
					// SCAN_DIAG
					0, 2, 1, 6, 3, 4, 7, 6, 4, 5, 7, 8, 5, 8, 8, 8, // log2TrafoSize == 2
					1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // prevCsbf == 0
					2, 1, 2, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 0, 0, 0, // prevCsbf == 1
					2, 2, 1, 2, 1, 0, 2, 1, 0, 0, 1, 0, 0, 0, 0, 0, // prevCsbf == 2
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // prevCsbf == 3
				},
				{
					// SCAN_HORIZ
					0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8,
					1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
					2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
					2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0,
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				},
				{
					// SCAN_VERT
					0, 2, 6, 7, 1, 3, 6, 7, 4, 4, 8, 8, 5, 5, 8, 8,
					1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
					2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0,
					2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				},
			};

			enum { kScanDiag = 0, kScanHoriz = 1, kScanVert = 2 };

			// part_mode (Table 7-10) — valeurs locales, ordre sans importance (pas de
			// table indexée dessus, juste des comparaisons).
			enum {
				kPart2Nx2N = 0,
				kPart2NxN,
				kPartNx2N,
				kPartNxN,
				kPart2NxnU,
				kPart2NxnD,
				kPartnLx2N,
				kPartnRx2N,
			};

			// ---- Transformées inverses (§8.6.4) -----------------------------------
			// Matrice DCT entière 32×32 normative (lignes = bases ; les tailles 4/8/16
			// sous-échantillonnent les lignes k*(32/N), N premières colonnes). Valeurs
			// identiques à la référence ffmpeg (libavcodec/hevc/dsp.c).
			const int8 kDct32[32][32] = {
				{64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
				{90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13, 4,
				 -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90},
				{90, 87, 80, 70, 57, 43, 25, 9, -9, -25, -43, -57, -70, -80, -87, -90,
				 -90, -87, -80, -70, -57, -43, -25, -9, 9, 25, 43, 57, 70, 80, 87, 90},
				{90, 82, 67, 46, 22, -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13,
				 13, 38, 61, 78, 88, 90, 85, 73, 54, 31, 4, -22, -46, -67, -82, -90},
				{89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89,
				 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89},
				{88, 67, 31, -13, -54, -82, -90, -78, -46, -4, 38, 73, 90, 85, 61, 22,
				 -22, -61, -85, -90, -73, -38, 4, 46, 78, 90, 82, 54, 13, -31, -67, -88},
				{87, 57, 9, -43, -80, -90, -70, -25, 25, 70, 90, 80, 43, -9, -57, -87,
				 -87, -57, -9, 43, 80, 90, 70, 25, -25, -70, -90, -80, -43, 9, 57, 87},
				{85, 46, -13, -67, -90, -73, -22, 38, 82, 88, 54, -4, -61, -90, -78, -31,
				 31, 78, 90, 61, 4, -54, -88, -82, -38, 22, 73, 90, 67, 13, -46, -85},
				{83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83,
				 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83},
				{82, 22, -54, -90, -61, 13, 78, 85, 31, -46, -90, -67, 4, 73, 88, 38,
				 -38, -88, -73, -4, 67, 90, 46, -31, -85, -78, -13, 61, 90, 54, -22, -82},
				{80, 9, -70, -87, -25, 57, 90, 43, -43, -90, -57, 25, 87, 70, -9, -80,
				 -80, -9, 70, 87, 25, -57, -90, -43, 43, 90, 57, -25, -87, -70, 9, 80},
				{78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 31, 90, 54, -38, -90, -46,
				 46, 90, 38, -54, -90, -31, 61, 88, 22, -67, -85, -13, 73, 82, 4, -78},
				{75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75,
				 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75},
				{73, -31, -90, -22, 78, 67, -38, -90, -13, 82, 61, -46, -88, -4, 85, 54,
				 -54, -85, 4, 88, 46, -61, -82, 13, 90, 38, -67, -78, 22, 90, 31, -73},
				{70, -43, -87, 9, 90, 25, -80, -57, 57, 80, -25, -90, -9, 87, 43, -70,
				 -70, 43, 87, -9, -90, -25, 80, 57, -57, -80, 25, 90, 9, -87, -43, 70},
				{67, -54, -78, 38, 85, -22, -90, 4, 90, 13, -88, -31, 82, 46, -73, -61,
				 61, 73, -46, -82, 31, 88, -13, -90, -4, 90, 22, -85, -38, 78, 54, -67},
				{64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64,
				 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64},
				{61, -73, -46, 82, 31, -88, -13, 90, -4, -90, 22, 85, -38, -78, 54, 67,
				 -67, -54, 78, 38, -85, -22, 90, 4, -90, 13, 88, -31, -82, 46, 73, -61},
				{57, -80, -25, 90, -9, -87, 43, 70, -70, -43, 87, 9, -90, 25, 80, -57,
				 -57, 80, 25, -90, 9, 87, -43, -70, 70, 43, -87, -9, 90, -25, -80, 57},
				{54, -85, -4, 88, -46, -61, 82, 13, -90, 38, 67, -78, -22, 90, -31, -73,
				 73, 31, -90, 22, 78, -67, -38, 90, -13, -82, 61, 46, -88, 4, 85, -54},
				{50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50,
				 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50},
				{46, -90, 38, 54, -90, 31, 61, -88, 22, 67, -85, 13, 73, -82, 4, 78,
				 -78, -4, 82, -73, -13, 85, -67, -22, 88, -61, -31, 90, -54, -38, 90, -46},
				{43, -90, 57, 25, -87, 70, 9, -80, 80, -9, -70, 87, -25, -57, 90, -43,
				 -43, 90, -57, -25, 87, -70, -9, 80, -80, 9, 70, -87, 25, 57, -90, 43},
				{38, -88, 73, -4, -67, 90, -46, -31, 85, -78, 13, 61, -90, 54, 22, -82,
				 82, -22, -54, 90, -61, -13, 78, -85, 31, 46, -90, 67, 4, -73, 88, -38},
				{36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36,
				 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36},
				{31, -78, 90, -61, 4, 54, -88, 82, -38, -22, 73, -90, 67, -13, -46, 85,
				 -85, 46, 13, -67, 90, -73, 22, 38, -82, 88, -54, -4, 61, -90, 78, -31},
				{25, -70, 90, -80, 43, 9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -25,
				 -25, 70, -90, 80, -43, -9, 57, -87, 87, -57, 9, 43, -80, 90, -70, 25},
				{22, -61, 85, -90, 73, -38, -4, 46, -78, 90, -82, 54, -13, -31, 67, -88,
				 88, -67, 31, 13, -54, 82, -90, 78, -46, 4, 38, -73, 90, -85, 61, -22},
				{18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18,
				 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18},
				{13, -38, 61, -78, 88, -90, 85, -73, 54, -31, 4, 22, -46, 67, -82, 90,
				 -90, 82, -67, 46, -22, -4, 31, -54, 73, -85, 90, -88, 78, -61, 38, -13},
				{9, -25, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 25, -9,
				 -9, 25, -43, 57, -70, 80, -87, 90, -90, 87, -80, 70, -57, 43, -25, 9},
				{4, -13, 22, -31, 38, -46, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90,
				 90, -90, 88, -85, 82, -78, 73, -67, 61, -54, 46, -38, 31, -22, 13, -4},
			};

			// Matrice DST-VII 4×4 (luma intra 4×4, §8.6.4.1) — r[i] = Σ_k S[k][i]·c[k]
			// (ligne k = contributions du coefficient k ; vérifiée contre les papillons
			// TR_4x4_LUMA de la référence ffmpeg : r0 = 29c0+74c1+84c2+55c3, etc.).
			const int8 kDst4[4][4] = {
				{29, 55, 74, 84},
				{74, 74, 0, -74},
				{84, -29, -74, 55},
				{55, -84, 74, -29},
			};

			const uint8 kLevelScale[6] = {40, 45, 51, 57, 64, 72};
			// Table 8-10 : mapping qPi -> QpC pour 4:2:0 (plage 30..43).
			const uint8 kQpC[14] = {29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37};

			// Déblocage (§8.7.2) : tables normatives tC (Table 8-12, idx 0..53) et
			// β (idx 0..51) — identiques à la référence ffmpeg.
			const uint8 kTcTable[54] = {0, 0, 0, 0, 0, 0, 0, 0,	 0,	 0,	 0,	 0,	 0,	 0,
										0, 0, 0, 0, 1, 1, 1, 1,	 1,	 1,	 1,	 1,	 1,	 2,
										2, 2, 2, 3, 3, 3, 3, 4,	 4,	 4,	 5,	 5,	 6,	 6,
										7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 22, 24};
			const uint8 kBetaTable[52] = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
										  0,  0,  0,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
										  16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38,
										  40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64};

			// Paramètres SAO d'UN CTB (résolus après merge left/up). type : 0=aucun,
			// 1=bande, 2=contour. Offsets SIGNÉS finaux (bande : signe explicite ;
			// contour : catégories 1-2 positives, 3-4 négatives, §7.3.8.3).
			struct SaoCtb {
					uint8 type[3] = {0, 0, 0};
					int16 offset[3][4] = {{0}, {0}, {0}};
					uint8 bandPos[3] = {0, 0, 0};
					uint8 eoClass[3] = {0, 0, 0};
			};

			// Transformée inverse générique deux passes (verticale puis horizontale),
			// écrêtage 16 bits entre passes (§8.6.4.2). dst = résidus.
			void InverseTransform(const int32 *coeff, int32 *res, int32 n, int32 bitDepth, bool dst4) {
				int32 tmp[32 * 32];
				const int32 stride32 = 32 / n;
				// Passe 1 (colonnes) : shift 7.
				for (int32 x = 0; x < n; ++x)
					for (int32 i = 0; i < n; ++i) {
						nk_int64 s = 0;
						for (int32 k = 0; k < n; ++k) {
							const int32 t = dst4 ? kDst4[k][i] : kDct32[k * stride32][i];
							s += (nk_int64)t * coeff[k * n + x];
						}
						tmp[i * n + x] = ClipInt16((int32)((s + 64) >> 7));
					}
				// Passe 2 (lignes) : shift 20 - bitDepth.
				const int32 shift2 = 20 - bitDepth;
				const int32 add2 = 1 << (shift2 - 1);
				for (int32 y = 0; y < n; ++y)
					for (int32 i = 0; i < n; ++i) {
						nk_int64 s = 0;
						for (int32 k = 0; k < n; ++k) {
							const int32 t = dst4 ? kDst4[k][i] : kDct32[k * stride32][i];
							s += (nk_int64)t * tmp[y * n + k];
						}
						res[y * n + i] = ClipInt16((int32)((s + add2) >> shift2));
					}
			}

			// ---- État du parseur/décodeur -----------------------------------------
			struct CtuParser {
					const NkHevcSps *sps = nullptr;
					const NkHevcPps *pps = nullptr;
					const NkHevcSliceHeader *sh = nullptr;
					NkCabacEngine eng;
					NkHevcCabacState st;
					NkHevcCabacState wppSaved; // état après le 2e CTB de la rangée courante
					bool wppSavedValid = false;

					// Géométrie.
					int32 ctbLog2 = 6, minCbLog2 = 3, minTbLog2 = 2, maxTbLog2 = 5;
					int32 picW = 0, picH = 0;
					int32 picWidthInCtbs = 0, picHeightInCtbs = 0;
					int32 minCbWidth = 0, minCbHeight = 0;
					int32 minPuWidth = 0, minPuHeight = 0; // grille 4x4
					int32 maxTrafoDepthIntra = 0;
					int32 log2MinCuQpDeltaSize = 6;

					// Voisinage (parse).
					NkVector<uint8> ctDepth;	   // par min-CB
					NkVector<uint8> intraModeY;	   // par bloc 4x4
					NkVector<uint8> skipFlagMap;   // par min-CB (P/B, brique 8)
					bool isCuQpDeltaCoded = false; // par groupe de quantification

					// CU courant.
					bool cuTransquantBypass = false;
					bool intraSplit = false; // part NxN
					bool cuIsIntra = true;	 // faux = CU inter (brique 8, parse structurel seul)
					int32 curIntraModeY[4] = {1, 1, 1, 1};
					int32 curIntraModeC = 1;
					int32 curCuX = 0, curCuY = 0, curCuLog2 = 3;
					int32 curPartMode = 0; // kPart2Nx2N — nécessaire au split forcé inter (§7.4.9.8)
					int32 maxTrafoDepthInter = 0;

					// ---- Inter P (briques 10-12) — multi-référence L0 + candidat temporel
					// (§8.5.3.2.8/9, brique 12 : champ de MV persistant via NkHevcFrame).
					const NkHevcFrame *const *refsL0 = nullptr; // .poc DOIT être renseigné par l'appelant
					int32 numRefsL0 = 0;
					const NkHevcFrame *const *refsL1 = nullptr; // RefPicList1 (slices B) — nullptr en P
					int32 numRefsL1 = 0;
					NkVector<int16> mvL0x, mvL0y; // MV L0 par bloc 4x4 (grille minPuWidth x minPuHeight)
					NkVector<int16> mvL1x, mvL1y; // MV L1 par bloc 4x4 (bi-prédiction)
					NkVector<int8> refIdxL0;	   // index dans refsL0, par bloc 4x4
					NkVector<int8> refIdxL1;	   // index dans refsL1, par bloc 4x4
					NkVector<uint8> predFlag;	   // bit0=L0, bit1=L1 par bloc 4x4 (0=intra)
					NkVector<uint8> mvValid;	   // 1 si le bloc 4x4 est INTER (predFlag != 0)

					// ---- Reconstruction (brique 6) — active si frame != nullptr ------
					NkHevcFrame *frame = nullptr;
					int32 bitDepth = 8, maxVal = 255;
					int32 qpBdOffsetY = 0, qpBdOffsetC = 0;
					NkVector<uint8> lumaRecon;	 // TU luma reconstruits (grille 4x4 luma)
					NkVector<uint8> chromaRecon; // TU chroma reconstruits (grille 4x4 chroma)
					NkVector<int8> qpMap;		 // QpY par min-CB (voisinage §8.6.1 + déblocage)
					// Filtres en boucle (briques 7/14) : Boundary Strength §8.7.2.4 par
					// segment 4×4 LUMA (0=aucune, 1=résidu/MV, 2=intra) + carte cbf_luma par
					// min-TU 4×4 (dérivation BS inter). Le déblocage CHROMA réutilise ces
					// mêmes cartes luma (filtré uniquement si BS==2, §8.7.2.5.5), comme la
					// référence ffmpeg — pas de cartes chroma distinctes. + paramètres SAO/CTB.
					NkVector<uint8> bsVert, bsHoriz; // BS de l'arête gauche/haute du 4×4 [y4*w4L + x4]
					NkVector<uint8> cbfLuma4;		 // cbf_luma par min-TU 4×4 luma [y4*w4L + x4]
					NkVector<SaoCtb> saoCtb;		 // par CTB (rx + ry*picWidthInCtbs)
					int32 w4L = 0;					 // stride grille 4×4 luma (= picW>>2)
					int32 qpYPrev = 26;			 // qPY_PREV (reset slice/rangée WPP)
					int32 lastCuQpY = 26;		 // QpY du dernier CU décodé
					int32 qgQpYPred = 26;		 // qPY_PRED du groupe de quantification courant
					int32 cuQpDeltaVal = 0;		 // delta signé du QG courant
					int32 tsShiftPend = 0;		 // (réservé transform-skip)
					int32 coeffBuf[32 * 32];	 // coefficients du TU courant (signés)

					NkHevcSliceDataStats *stats = nullptr;
					bool okFlag = true; // passe à false sur incohérence/dépassement

					// ---- Primitives CABAC --------------------------------------------
					uint32 Bin(int32 ctxIdx) {
						return eng.DecodeDecision(st.ctx[ctxIdx]);
					}
					uint32 Bypass() {
						return eng.DecodeBypass();
					}
					uint32 BypassBits(int32 n) {
						uint32 v = 0;
						for (int32 i = 0; i < n; ++i)
							v = (v << 1) | Bypass();
						return v;
					}
					uint32 Terminate() {
						return eng.DecodeTerminate();
					}

					// ---- sao() (§7.3.8.3) — parse ET stocke les paramètres par CTB -----
					void ParseSao(int32 rx, int32 ry) {
						SaoCtb *cur = frame ? &saoCtb[(usize)(ry * picWidthInCtbs + rx)] : nullptr;
						bool mergeLeft = false, mergeUp = false;
						if (rx > 0)
							mergeLeft = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (!mergeLeft && ry > 0)
							mergeUp = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (mergeLeft || mergeUp) {
							if (cur)
								*cur = mergeLeft ? saoCtb[(usize)(ry * picWidthInCtbs + rx - 1)]
												 : saoCtb[(usize)((ry - 1) * picWidthInCtbs + rx)];
							return;
						}
						int32 typeChroma = 0, eoChroma = 0;
						for (int32 cIdx = 0; cIdx < 3; ++cIdx) {
							const bool present = (cIdx == 0) ? sh->saoLuma : sh->saoChroma;
							if (!present)
								continue;
							int32 type;
							if (cIdx == 0 || cIdx == 1) {
								if (Bin(kHevcCtxSaoTypeIdx) == 0)
									type = 0;
								else
									type = Bypass() ? 2 : 1; // 1 = bande, 2 = contour
								if (cIdx == 1)
									typeChroma = type;
							} else {
								type = typeChroma; // cIdx 2 hérite de cIdx 1
							}
							if (cur)
								cur->type[cIdx] = (uint8)type;
							if (type == 0)
								continue;
							const int32 cMax = (1 << (Min32(sps->bitDepthLuma, 10) - 5)) - 1;
							int32 offsetAbs[4];
							for (int32 i = 0; i < 4; ++i) {
								int32 v = 0;
								while (v < cMax && Bypass())
									++v;
								offsetAbs[i] = v;
							}
							if (type == 1) { // bande : signes explicites + position
								for (int32 i = 0; i < 4; ++i) {
									int32 v = offsetAbs[i];
									if (v != 0 && Bypass())
										v = -v; // sao_offset_sign
									if (cur)
										cur->offset[cIdx][i] = (int16)v;
								}
								const int32 pos = (int32)BypassBits(5); // sao_band_position
								if (cur)
									cur->bandPos[cIdx] = (uint8)pos;
							} else { // contour : signes implicites (cat 1-2 : +, cat 3-4 : −)
								if (cur) {
									cur->offset[cIdx][0] = (int16)offsetAbs[0];
									cur->offset[cIdx][1] = (int16)offsetAbs[1];
									cur->offset[cIdx][2] = (int16)(-offsetAbs[2]);
									cur->offset[cIdx][3] = (int16)(-offsetAbs[3]);
								}
								if (cIdx == 0) {
									const int32 eo = (int32)BypassBits(2); // sao_eo_class_luma
									if (cur)
										cur->eoClass[0] = (uint8)eo;
								}
								if (cIdx == 1) {
									eoChroma = (int32)BypassBits(2); // sao_eo_class_chroma
									if (cur)
										cur->eoClass[1] = (uint8)eoChroma;
								}
								if (cIdx == 2 && cur)
									cur->eoClass[2] = (uint8)eoChroma; // hérite
							}
						}
					}

					// ---- Modes intra (§8.4.2) -----------------------------------------
					int32 GetStoredMode(int32 x, int32 y) const {
						return intraModeY[(usize)((y >> 2) * minPuWidth + (x >> 2))];
					}
					void StoreModes(int32 x, int32 y, int32 size, int32 mode) {
						for (int32 j = 0; j < size; j += 4)
							for (int32 i = 0; i < size; i += 4) {
								const int32 px = (x + i) >> 2, py = (y + j) >> 2;
								if (px < minPuWidth && py < minPuHeight)
									intraModeY[(usize)(py * minPuWidth + px)] = (uint8)mode;
							}
					}

					int32 DeriveIntraMode(int32 xPb, int32 yPb, bool prevFlag, int32 mpmIdx, int32 rem) {
						// candA (gauche) / candB (dessus) — DC si indisponible ; le voisin du
						// dessus hors de la rangée de CTB courante est remplacé par DC (§8.4.2).
						int32 candA = 1, candB = 1;
						if (xPb > 0)
							candA = GetStoredMode(xPb - 1, yPb);
						const int32 ctbTop = (yPb >> ctbLog2) << ctbLog2;
						if (yPb > 0 && (yPb - 1) >= ctbTop)
							candB = GetStoredMode(xPb, yPb - 1);
						int32 mpm[3];
						if (candA == candB) {
							if (candA < 2) {
								mpm[0] = 0;
								mpm[1] = 1;
								mpm[2] = 26;
							} else {
								mpm[0] = candA;
								mpm[1] = 2 + ((candA + 29) % 32);
								mpm[2] = 2 + ((candA - 2 + 1) % 32);
							}
						} else {
							mpm[0] = candA;
							mpm[1] = candB;
							if (candA != 0 && candB != 0)
								mpm[2] = 0;
							else
								mpm[2] = (candA + candB) < 2 ? 26 : 1;
						}
						if (prevFlag)
							return mpm[mpmIdx];
						// tri croissant des 3 MPM puis réinsertion du reste.
						if (mpm[0] > mpm[1]) {
							const int32 t = mpm[0];
							mpm[0] = mpm[1];
							mpm[1] = t;
						}
						if (mpm[0] > mpm[2]) {
							const int32 t = mpm[0];
							mpm[0] = mpm[2];
							mpm[2] = t;
						}
						if (mpm[1] > mpm[2]) {
							const int32 t = mpm[1];
							mpm[1] = mpm[2];
							mpm[2] = t;
						}
						int32 mode = rem;
						for (int32 i = 0; i < 3; ++i)
							if (mode >= mpm[i])
								++mode;
						return mode;
					}

					// ---- QP (§8.6.1) --------------------------------------------------
					void StartQuantGroup(int32 xQg, int32 yQg) {
						isCuQpDeltaCoded = false;
						cuQpDeltaVal = 0;
						qpYPrev = lastCuQpY;
						int32 qpA = qpYPrev, qpB = qpYPrev;
						// `qpMap` n'est alloué que si `frame` (reconstruction) : en parse
						// structurel seul (brique 8, P/B), qgQpYPred n'affecte AUCUN bit
						// consommé (seule sa VALEUR servirait à CurrentQpY(), lui-même non
						// lu hors reconstruction) — sans danger de rester à qPY_PREV ici.
						if (frame) {
							// Disponible seulement dans le MÊME CTB (sinon qPY_PREV).
							if (xQg > 0 && ((xQg - 1) >> ctbLog2) == (xQg >> ctbLog2))
								qpA = qpMap[(usize)((yQg >> minCbLog2) * minCbWidth + ((xQg - 1) >> minCbLog2))];
							if (yQg > 0 && ((yQg - 1) >> ctbLog2) == (yQg >> ctbLog2))
								qpB = qpMap[(usize)(((yQg - 1) >> minCbLog2) * minCbWidth + (xQg >> minCbLog2))];
						}
						qgQpYPred = (qpA + qpB + 1) >> 1;
					}
					int32 CurrentQpY() const {
						// §8.6.1 : QpY = ((qPY_PRED + delta + 52 + 2·off) % (52 + off)) − off.
						const int32 m = 52 + qpBdOffsetY;
						return ((qgQpYPred + cuQpDeltaVal + 52 + 2 * qpBdOffsetY) % m) - qpBdOffsetY;
					}
					int32 ChromaQp(int32 qpY, int32 cIdx) const {
						const int32 off = (cIdx == 1) ? (pps->ppsCbQpOffset + sh->sliceCbQpOffset)
													  : (pps->ppsCrQpOffset + sh->sliceCrQpOffset);
						const int32 qPi = Clip3i(-qpBdOffsetC, 57, qpY + off);
						int32 qPc;
						if (qPi < 30)
							qPc = qPi;
						else if (qPi > 43)
							qPc = qPi - 6;
						else
							qPc = kQpC[qPi - 30];
						return qPc + qpBdOffsetC;
					}

					// ---- Prédiction intra (§8.4.4.2) ----------------------------------
					bool SampleAvailLuma(int32 x, int32 y) const {
						if (x < 0 || y < 0 || x >= picW || y >= picH)
							return false;
						return lumaRecon[(usize)((y >> 2) * minPuWidth + (x >> 2))] != 0;
					}
					bool SampleAvailChroma(int32 x, int32 y) const {
						const int32 cw = picW >> 1, ch = picH >> 1;
						if (x < 0 || y < 0 || x >= cw || y >= ch)
							return false;
						return chromaRecon[(usize)((y >> 2) * ((cw + 3) >> 2) + (x >> 2))] != 0;
					}

					void PredictIntra(int32 cIdx, int32 x0, int32 y0, int32 log2Size, int32 mode) {
						const int32 n = 1 << log2Size;
						nk_uint16 *plane;
						int32 stride, w, h;
						if (cIdx == 0) {
							plane = frame->y.Data();
							stride = frame->lumaW;
							w = picW;
							h = picH;
						} else {
							plane = (cIdx == 1) ? frame->cb.Data() : frame->cr.Data();
							stride = frame->chromaW;
							w = picW >> 1;
							h = picH >> 1;
						}
						// Référence : left[-1..2n-1], top[-1..2n-1] (offset +1 dans les buffers).
						int32 leftBuf[2 * 32 + 1], topBuf[2 * 32 + 1];
						bool leftAv[2 * 32 + 1], topAv[2 * 32 + 1];
						int32 *left = leftBuf + 1, *top = topBuf + 1;
						bool *lAv = leftAv + 1, *tAv = topAv + 1;
						const int32 half = 1 << (bitDepth - 1);
						auto avail = [&](int32 px, int32 py) {
							return (cIdx == 0) ? SampleAvailLuma(px, py) : SampleAvailChroma(px, py);
						};
						for (int32 i = -1; i < 2 * n; ++i) {
							const int32 py = y0 + i;
							lAv[i] = avail(x0 - 1, py);
							left[i] = lAv[i] ? (int32)plane[py * stride + (x0 - 1)] : 0;
						}
						for (int32 i = 0; i < 2 * n; ++i) {
							const int32 px = x0 + i;
							tAv[i] = avail(px, y0 - 1);
							top[i] = tAv[i] ? (int32)plane[(y0 - 1) * stride + px] : 0;
						}
						tAv[-1] = lAv[-1];
						top[-1] = left[-1];
						// Substitution (§8.4.4.2.2) : ordre de scan bas-gauche -> coin -> droite.
						bool any = false;
						for (int32 i = -1; i < 2 * n && !any; ++i)
							any = lAv[i];
						for (int32 i = 0; i < 2 * n && !any; ++i)
							any = tAv[i];
						if (!any) {
							for (int32 i = -1; i < 2 * n; ++i) {
								left[i] = half;
								top[i] = half;
							}
						} else {
							// premier échantillon du scan : left[2n-1] ; s'il manque, prendre le
							// premier disponible dans l'ordre du scan.
							if (!lAv[2 * n - 1]) {
								int32 v = half;
								bool found = false;
								for (int32 i = 2 * n - 2; i >= -1 && !found; --i)
									if (lAv[i]) {
										v = left[i];
										found = true;
									}
								for (int32 i = 0; i < 2 * n && !found; ++i)
									if (tAv[i]) {
										v = top[i];
										found = true;
									}
								left[2 * n - 1] = v;
								lAv[2 * n - 1] = true;
							}
							for (int32 i = 2 * n - 2; i >= -1; --i)
								if (!lAv[i]) {
									left[i] = left[i + 1];
									lAv[i] = true;
								}
							top[-1] = left[-1];
							tAv[-1] = true;
							for (int32 i = 0; i < 2 * n; ++i)
								if (!tAv[i]) {
									top[i] = top[i - 1];
									tAv[i] = true;
								}
						}
						// Lissage (§8.4.4.2.3) — luma seulement en 4:2:0.
						int32 fLeft[2 * 32 + 1], fTop[2 * 32 + 1];
						int32 *lf = fLeft + 1, *tf = fTop + 1;
						if (cIdx == 0 && mode != 1 && n != 4) {
							static const int32 kThresh[3] = {7, 1, 0};
							const int32 minDist = Min32(Abs32(mode - 26), Abs32(mode - 10));
							if (minDist > kThresh[log2Size - 3]) {
								const int32 bilinThr = 1 << (bitDepth - 5);
								if (sps->strongIntraSmoothingEnabled && log2Size == 5 &&
									Abs32(top[-1] + top[63] - 2 * top[31]) < bilinThr &&
									Abs32(left[-1] + left[63] - 2 * left[31]) < bilinThr) {
									// Lissage fort bilinéaire (§8.4.4.2.3).
									lf[-1] = left[-1];
									tf[-1] = top[-1];
									for (int32 i = 0; i < 63; ++i) {
										tf[i] = ((63 - i) * top[-1] + (i + 1) * top[63] + 32) >> 6;
										lf[i] = ((63 - i) * left[-1] + (i + 1) * left[63] + 32) >> 6;
									}
									tf[63] = top[63];
									lf[63] = left[63];
								} else {
									lf[-1] = (left[0] + 2 * left[-1] + top[0] + 2) >> 2;
									tf[-1] = lf[-1];
									for (int32 i = 0; i < 2 * n - 1; ++i) {
										lf[i] = (left[i + 1] + 2 * left[i] + left[i - 1] + 2) >> 2;
										tf[i] = (top[i + 1] + 2 * top[i] + top[i - 1] + 2) >> 2;
									}
									lf[2 * n - 1] = left[2 * n - 1];
									tf[2 * n - 1] = top[2 * n - 1];
								}
								left = lf;
								top = tf;
							}
						}
						nk_uint16 *dst = plane + y0 * stride + x0;
						if (mode == 0) { // Planar (§8.4.4.2.4)
							for (int32 y = 0; y < n; ++y)
								for (int32 x = 0; x < n; ++x)
									dst[y * stride + x] = (nk_uint16)(((n - 1 - x) * left[y] +
																	 (x + 1) * top[n] +
																	 (n - 1 - y) * top[x] +
																	 (y + 1) * left[n] + n) >>
																	(log2Size + 1));
						} else if (mode == 1) { // DC (§8.4.4.2.5)
							int32 dc = n;
							for (int32 i = 0; i < n; ++i)
								dc += left[i] + top[i];
							dc >>= (log2Size + 1);
							for (int32 y = 0; y < n; ++y)
								for (int32 x = 0; x < n; ++x)
									dst[y * stride + x] = (nk_uint16)dc;
							if (cIdx == 0 && n < 32) {
								dst[0] = (nk_uint16)((left[0] + 2 * dc + top[0] + 2) >> 2);
								for (int32 x = 1; x < n; ++x)
									dst[x] = (nk_uint16)((top[x] + 3 * dc + 2) >> 2);
								for (int32 y = 1; y < n; ++y)
									dst[y * stride] = (nk_uint16)((left[y] + 3 * dc + 2) >> 2);
							}
						} else { // Angulaire (§8.4.4.2.6) — port direct de la référence.
							static const int32 kAngle[33] = {32, 26, 21, 17, 13, 9, 5, 2, 0,
															 -2, -5, -9, -13, -17, -21, -26, -32,
															 -26, -21, -17, -13, -9, -5, -2, 0,
															 2, 5, 9, 13, 17, 21, 26, 32};
							static const int32 kInvAngle[15] = {-4096, -1638, -910, -630, -482,
																-390, -315, -256, -315, -390,
																-482, -630, -910, -1638, -4096};
							const int32 angle = kAngle[mode - 2];
							int32 refArr[3 * 32 + 4];
							int32 *refTmp = refArr + n + 1;
							const int32 lastIdx = (n * angle) >> 5;
							if (mode >= 18) {
								const int32 *ref = top - 1;
								if (angle < 0 && lastIdx < -1) {
									for (int32 x = 0; x <= n; ++x)
										refTmp[x] = top[x - 1];
									for (int32 x = lastIdx; x <= -1; ++x)
										refTmp[x] = left[-1 + ((x * kInvAngle[mode - 11] + 128) >> 8)];
									ref = refTmp;
								}
								for (int32 y = 0; y < n; ++y) {
									const int32 idx = ((y + 1) * angle) >> 5;
									const int32 fact = ((y + 1) * angle) & 31;
									for (int32 x = 0; x < n; ++x)
										dst[y * stride + x] =
											fact ? (nk_uint16)(((32 - fact) * ref[x + idx + 1] +
															   fact * ref[x + idx + 2] + 16) >>
															  5)
												 : (nk_uint16)ref[x + idx + 1];
								}
								if (mode == 26 && cIdx == 0 && n < 32)
									for (int32 y = 0; y < n; ++y)
										dst[y * stride] = (nk_uint16)Clip3i(
											0, maxVal, top[0] + ((left[y] - left[-1]) >> 1));
							} else {
								const int32 *ref = left - 1;
								if (angle < 0 && lastIdx < -1) {
									for (int32 x = 0; x <= n; ++x)
										refTmp[x] = left[x - 1];
									for (int32 x = lastIdx; x <= -1; ++x)
										refTmp[x] = top[-1 + ((x * kInvAngle[mode - 11] + 128) >> 8)];
									ref = refTmp;
								}
								for (int32 x = 0; x < n; ++x) {
									const int32 idx = ((x + 1) * angle) >> 5;
									const int32 fact = ((x + 1) * angle) & 31;
									for (int32 y = 0; y < n; ++y)
										dst[y * stride + x] =
											fact ? (nk_uint16)(((32 - fact) * ref[y + idx + 1] +
															   fact * ref[y + idx + 2] + 16) >>
															  5)
												 : (nk_uint16)ref[y + idx + 1];
								}
								if (mode == 10 && cIdx == 0 && n < 32)
									for (int32 x = 0; x < n; ++x)
										dst[x] = (nk_uint16)Clip3i(0, maxVal,
																  left[0] + ((top[x] - top[-1]) >> 1));
							}
						}
					}

					// Déquant + transformée inverse + addition dans le plan.
					void ReconstructResidual(int32 cIdx, int32 x0, int32 y0, int32 log2Size,
											 bool transformSkip) {
						const int32 n = 1 << log2Size;
						const int32 qpY = CurrentQpY();
						const int32 qp = (cIdx == 0) ? qpY + qpBdOffsetY : ChromaQp(qpY, cIdx);
						const int32 bd = (cIdx == 0) ? bitDepth : sps->bitDepthChroma;
						const int32 bdShift = bd + log2Size - 5;
						const nk_int64 scale = (nk_int64)kLevelScale[qp % 6] << (qp / 6);
						int32 d[32 * 32];
						if (cuTransquantBypass) {
							for (int32 i = 0; i < n * n; ++i)
								d[i] = coeffBuf[i];
						} else {
							const nk_int64 add = (nk_int64)1 << (bdShift - 1);
							for (int32 i = 0; i < n * n; ++i)
								d[i] = ClipInt16((int32)(((nk_int64)coeffBuf[i] * scale * 16 + add) >>
														 bdShift));
						}
						int32 res[32 * 32];
						if (cuTransquantBypass) {
							for (int32 i = 0; i < n * n; ++i)
								res[i] = d[i];
						} else if (transformSkip) {
							// §8.6.4.2 : pas de transformée — mise à l'échelle directe.
							const int32 s = 15 - bd - log2Size;
							for (int32 i = 0; i < n * n; ++i)
								res[i] = (s > 0) ? ((d[i] << 7) + (1 << (s + 6))) >> (s + 7)
												 : (d[i] << 7) >> 7; // s==0 pour bd=13+ seulement
							// NB : équivalent à ffmpeg (dequant shift 15-bd-log2 après <<7).
							if (s > 0)
								for (int32 i = 0; i < n * n; ++i)
									res[i] = Clip3i(-32768, 32767, res[i]);
						} else {
							const bool dst4 = (cIdx == 0 && log2Size == 2); // luma intra 4x4
							InverseTransform(d, res, n, bd, dst4);
						}
						nk_uint16 *plane;
						int32 stride;
						if (cIdx == 0) {
							plane = frame->y.Data();
							stride = frame->lumaW;
						} else {
							plane = (cIdx == 1) ? frame->cb.Data() : frame->cr.Data();
							stride = frame->chromaW;
						}
						const int32 mv = (cIdx == 0) ? maxVal : (1 << sps->bitDepthChroma) - 1;
						for (int32 y = 0; y < n; ++y)
							for (int32 x = 0; x < n; ++x) {
								nk_uint16 &px = plane[(y0 + y) * stride + (x0 + x)];
								px = (nk_uint16)Clip3i(0, mv, (int32)px + res[y * n + x]);
							}
					}

					// ---- I_PCM (§7.3.8.5 pcm_sample + §8.4.4.1) -----------------------
					// Le bin pcm_flag (terminate) a laissé notre moteur bit-à-bit EXACTEMENT
					// après le flush arithmétique (cf. I_PCM H.264 validé) : on aligne à
					// l'octet (pcm_alignment_zero_bit), on lit les échantillons bruts
					// (pcmBitDepth bits, MSB-first), on reconstruit sample<<(bd-pcmBd), puis on
					// RÉ-INITIALISE CABAC à l'octet suivant (les contextes SURVIVENT, §9.3.1).
					void DecodePcm(int32 x0, int32 y0, int32 log2CbSize) {
						const int32 nY = 1 << log2CbSize;
						const int32 subW =
							(sps->chromaFormatIdc == 1 || sps->chromaFormatIdc == 2) ? 1 : 0;
						const int32 subH = (sps->chromaFormatIdc == 1) ? 1 : 0;
						const int32 nCx = nY >> subW, nCy = nY >> subH;
						const int32 bdY = bitDepth, bdC = sps->bitDepthChroma;
						const int32 pbY = sps->pcmBitDepthLuma, pbC = sps->pcmBitDepthChroma;
						usize pos = eng.bytePos;
						if (eng.bitPos != 0)
							++pos; // pcm_alignment_zero_bit(s) -> octet suivant
						const uint8 *bas = eng.data;
						const usize sz = eng.size;
						usize bit = pos * 8; // curseur de bit ABSOLU dans le RBSP dé-émulé
						auto readBits = [&](int32 nb) -> int32 {
							int32 v = 0;
							for (int32 k = 0; k < nb; ++k) {
								const usize by = bit >> 3;
								const int32 shb = 7 - (int32)(bit & 7);
								const int32 b = (by < sz) ? ((bas[by] >> shb) & 1) : 0;
								v = (v << 1) | b;
								++bit;
							}
							return v;
						};
						// Luma (nY×nY).
						for (int32 y = 0; y < nY; ++y)
							for (int32 x = 0; x < nY; ++x) {
								const int32 s = readBits(pbY);
								if (frame)
									frame->y[(usize)((y0 + y) * frame->lumaW + (x0 + x))] =
										(nk_uint16)(s << (bdY - pbY));
							}
						// Chroma Cb puis Cr (si ChromaArrayType != 0).
						if (sps->chromaFormatIdc != 0 && !sps->separateColourPlane) {
							const int32 cx0 = x0 >> subW, cy0 = y0 >> subH;
							for (int32 pl = 0; pl < 2; ++pl) {
								nk_uint16 *dst =
									frame ? ((pl == 0) ? frame->cb.Data() : frame->cr.Data()) : nullptr;
								const int32 stride = frame ? frame->chromaW : 0;
								for (int32 y = 0; y < nCy; ++y)
									for (int32 x = 0; x < nCx; ++x) {
										const int32 s = readBits(pbC);
										if (frame)
											dst[(usize)((cy0 + y) * stride + (cx0 + x))] =
												(nk_uint16)(s << (bdC - pbC));
									}
							}
						}
						// Ré-init CABAC à l'octet suivant la dernière donnée PCM.
						const usize endByte = (bit + 7) >> 3; // = pos + ceil(totalBits/8)
						eng.InitEngine(eng.data, eng.size, endByte);
						if (frame) {
							MarkLumaRecon(x0, y0, nY);
							if (sps->chromaFormatIdc != 0 && !sps->separateColourPlane)
								MarkChromaRecon(x0 >> subW, y0 >> subH, nCx);
						}
					}

					// Marque cbf_luma=1 sur tous les min-TU 4×4 d'un TU luma (§8.7.2.4,
					// consommé par la dérivation de BS ci-dessous — condition bs=1).
					void MarkCbfLuma(int32 x0, int32 y0, int32 n) {
						for (int32 j = 0; j < n; j += 4)
							for (int32 i = 0; i < n; i += 4) {
								const int32 x4 = (x0 + i) >> 2, y4 = (y0 + j) >> 2;
								if (x4 < w4L && y4 < (picH >> 2))
									cbfLuma4[(usize)(y4 * w4L + x4)] = 1;
							}
					}

					void MarkLumaRecon(int32 x0, int32 y0, int32 n) {
						for (int32 j = 0; j < n; j += 4)
							for (int32 i = 0; i < n; i += 4) {
								const int32 px = (x0 + i) >> 2, py = (y0 + j) >> 2;
								if (px < minPuWidth && py < minPuHeight)
									lumaRecon[(usize)(py * minPuWidth + px)] = 1;
							}
					}
					void MarkChromaRecon(int32 x0, int32 y0, int32 n) {
						const int32 cw4 = ((picW >> 1) + 3) >> 2;
						const int32 ch4 = ((picH >> 1) + 3) >> 2;
						for (int32 j = 0; j < n; j += 4)
							for (int32 i = 0; i < n; i += 4) {
								const int32 px = (x0 + i) >> 2, py = (y0 + j) >> 2;
								if (px < cw4 && py < ch4)
									chromaRecon[(usize)(py * cw4 + px)] = 1;
							}
					}

					// ---- Boundary Strength §8.7.2.4 (transcription ffmpeg boundary_strength
					// + ff_hevc_deblocking_boundary_strengths) ---------------------------
					struct MvField; // défini plus bas (champ de MV par bloc 4×4)
					// POC de l'image référencée par la liste `li` d'un MvField (via les
					// listes résolues de CETTE slice — voisin et courant partagent les mêmes
					// listes en mono-slice, cf. ffmpeg neigh_refPicList == cur refPicList).
					// Valeur distincte "impossible" si l'index est hors-liste (sécurité).
					int32 RefPoc(const MvField &f, int32 li) const {
						const NkHevcFrame *const *refs = (li == 0) ? refsL0 : refsL1;
						const int32 num = (li == 0) ? numRefsL0 : numRefsL1;
						const int32 ri = f.refIdx[li];
						if (ri >= 0 && ri < num && refs && refs[ri])
							return refs[ri]->poc;
						return 0x40000000; // POC impossible -> "références différentes"
					}

					// boundary_strength(curr, neigh) — les DEUX côtés sont INTER (le cas
					// intra bs=2 et le cas cbf bs=1 sont traités par l'appelant). Compare les
					// POC POINTÉS (pas les refIdx) : bit-exact même si un POC est dupliqué
					// dans la liste. mv en 1/4-pel : seuil de différence = 4 (=1 pixel luma).
					int32 BoundaryStrengthMv(const MvField &curr, const MvField &neigh) const {
						if (curr.predFlag == 3 && neigh.predFlag == 3) {
							const int32 cP0 = RefPoc(curr, 0), cP1 = RefPoc(curr, 1);
							const int32 nP0 = RefPoc(neigh, 0), nP1 = RefPoc(neigh, 1);
							if (cP0 == nP0 && cP0 == cP1 && nP0 == nP1) {
								// mêmes L0 et L1 des deux côtés -> double comparaison croisée.
								const bool a = Abs32(neigh.mvx[0] - curr.mvx[0]) >= 4 ||
											   Abs32(neigh.mvy[0] - curr.mvy[0]) >= 4 ||
											   Abs32(neigh.mvx[1] - curr.mvx[1]) >= 4 ||
											   Abs32(neigh.mvy[1] - curr.mvy[1]) >= 4;
								const bool b = Abs32(neigh.mvx[1] - curr.mvx[0]) >= 4 ||
											   Abs32(neigh.mvy[1] - curr.mvy[0]) >= 4 ||
											   Abs32(neigh.mvx[0] - curr.mvx[1]) >= 4 ||
											   Abs32(neigh.mvy[0] - curr.mvy[1]) >= 4;
								return (a && b) ? 1 : 0;
							} else if (nP0 == cP0 && nP1 == cP1) {
								return (Abs32(neigh.mvx[0] - curr.mvx[0]) >= 4 ||
										Abs32(neigh.mvy[0] - curr.mvy[0]) >= 4 ||
										Abs32(neigh.mvx[1] - curr.mvx[1]) >= 4 ||
										Abs32(neigh.mvy[1] - curr.mvy[1]) >= 4)
										   ? 1
										   : 0;
							} else if (nP1 == cP0 && nP0 == cP1) {
								return (Abs32(neigh.mvx[1] - curr.mvx[0]) >= 4 ||
										Abs32(neigh.mvy[1] - curr.mvy[0]) >= 4 ||
										Abs32(neigh.mvx[0] - curr.mvx[1]) >= 4 ||
										Abs32(neigh.mvy[0] - curr.mvy[1]) >= 4)
										   ? 1
										   : 0;
							}
							return 1;
						}
						if (curr.predFlag != 3 && neigh.predFlag != 3) {
							int32 Ax, Ay, refA, Bx, By, refB;
							if (curr.predFlag & 1) {
								Ax = curr.mvx[0];
								Ay = curr.mvy[0];
								refA = RefPoc(curr, 0);
							} else {
								Ax = curr.mvx[1];
								Ay = curr.mvy[1];
								refA = RefPoc(curr, 1);
							}
							if (neigh.predFlag & 1) {
								Bx = neigh.mvx[0];
								By = neigh.mvy[0];
								refB = RefPoc(neigh, 0);
							} else {
								Bx = neigh.mvx[1];
								By = neigh.mvy[1];
								refB = RefPoc(neigh, 1);
							}
							if (refA == refB)
								return (Abs32(Ax - Bx) >= 4 || Abs32(Ay - By) >= 4) ? 1 : 0;
							return 1;
						}
						return 1; // exactement un côté bi
					}

					// Dérive la BS des arêtes GAUCHE et HAUTE (8-alignées) d'un bloc TU/CU +
					// des frontières PU INTERNES (TU inter plus large qu'un PU). Appelée aux
					// feuilles de transform_tree ET sur CU skip / CU inter sans résidu.
					// L'écriture se fait sur la grille 4×4 luma (bsVert/bsHoriz). Un côté
					// INTRA se lit predFlag==0 (jamais écrit par StoreMv) ; cbfLuma4 déjà posé.
					void DeriveDeblockBs(int32 x0, int32 y0, int32 log2Size) {
						const int32 size = 1 << log2Size;
						// log2 de la granularité PU = grille MV 4×4 -> 2.
						const int32 log2MinPu = 2;
						const bool isIntra = GetMvField(x0, y0).predFlag == 0;

						// Arête HAUTE (frontière TU horizontale).
						if (y0 > 0 && !(y0 & 7)) {
							for (int32 i = 0; i < size; i += 4) {
								const MvField top = GetMvField(x0 + i, y0 - 1);
								const MvField cur = GetMvField(x0 + i, y0);
								const int32 xt = (x0 + i) >> 2;
								int32 bs;
								if (cur.predFlag == 0 || top.predFlag == 0)
									bs = 2;
								else if (cbfLuma4[(usize)((y0 >> 2) * w4L + xt)] ||
										 cbfLuma4[(usize)(((y0 - 1) >> 2) * w4L + xt)])
									bs = 1;
								else
									bs = BoundaryStrengthMv(cur, top);
								bsHoriz[(usize)((y0 >> 2) * w4L + xt)] = (uint8)bs;
							}
						}
						// Arête GAUCHE (frontière TU verticale).
						if (x0 > 0 && !(x0 & 7)) {
							for (int32 i = 0; i < size; i += 4) {
								const MvField left = GetMvField(x0 - 1, y0 + i);
								const MvField cur = GetMvField(x0, y0 + i);
								const int32 yt = (y0 + i) >> 2;
								int32 bs;
								if (cur.predFlag == 0 || left.predFlag == 0)
									bs = 2;
								else if (cbfLuma4[(usize)(yt * w4L + (x0 >> 2))] ||
										 cbfLuma4[(usize)(yt * w4L + ((x0 - 1) >> 2))])
									bs = 1;
								else
									bs = BoundaryStrengthMv(cur, left);
								bsVert[(usize)(yt * w4L + (x0 >> 2))] = (uint8)bs;
							}
						}
						// Frontières PU internes (TU inter, taille > min PU) — pas de test cbf.
						if (log2Size > log2MinPu && !isIntra) {
							for (int32 j = 8; j < size; j += 8)
								for (int32 i = 0; i < size; i += 4) {
									const MvField top = GetMvField(x0 + i, y0 + j - 1);
									const MvField cur = GetMvField(x0 + i, y0 + j);
									const int32 bs = BoundaryStrengthMv(cur, top);
									bsHoriz[(usize)(((y0 + j) >> 2) * w4L + ((x0 + i) >> 2))] =
										(uint8)bs;
								}
							for (int32 j = 0; j < size; j += 4)
								for (int32 i = 8; i < size; i += 8) {
									const MvField left = GetMvField(x0 + i - 1, y0 + j);
									const MvField cur = GetMvField(x0 + i, y0 + j);
									const int32 bs = BoundaryStrengthMv(cur, left);
									bsVert[(usize)(((y0 + j) >> 2) * w4L + ((x0 + i) >> 2))] =
										(uint8)bs;
								}
						}
					}

					// ---- Déblocage (§8.7.2) — luma : BS 1 ou 2 ; chroma : BS 2 seulement ---
					int32 QpAt(int32 lx, int32 ly) const {
						return qpMap[(usize)((ly >> minCbLog2) * minCbWidth + (lx >> minCbLog2))];
					}

					// Filtre un segment de 4 lignes/colonnes LUMA. `vert` : arête verticale
					// en x=xE (P à gauche), sinon horizontale en y=yE (P au-dessus). `bs` =
					// Boundary Strength (1 ou 2) : tc = kTcTable[qp + 2*(bs-1) + tcOff]
					// (TC_CALC §8.7.2.5.3). beta indépendant de bs. Décision strong/weak
					// inchangée (par seuils beta/tc, identique intra/inter).
					void LumaDeblockSeg(bool vert, int32 xE, int32 yE, int32 bs) {
						nk_uint16 *Y = frame->y.Data();
						const int32 stride = frame->lumaW;
						const int32 betaOff = sh->sliceBetaOffsetDiv2 * 2;
						const int32 tcOff = sh->sliceTcOffsetDiv2 * 2;
						const int32 qpP = vert ? QpAt(xE - 1, yE) : QpAt(xE, yE - 1);
						const int32 qpQ = QpAt(xE, yE);
						const int32 qp = (qpP + qpQ + 1) >> 1;
						const int32 beta = (int32)kBetaTable[Clip3i(0, 51, qp + betaOff)]
										   << (bitDepth - 8);
						const int32 tc = (int32)kTcTable[Clip3i(0, 53, qp + 2 * (bs - 1) + tcOff)]
										 << (bitDepth - 8);
						if (beta == 0)
							return;
						// Accès : s(i, d) = échantillon à distance i de l'arête (négatif = P),
						// ligne d du segment.
						auto S = [&](int32 i, int32 d) -> nk_uint16 & {
							return vert ? Y[(yE + d) * stride + (xE + i)]
										: Y[(yE + i) * stride + (xE + d)];
						};
						const int32 p0r0 = S(-1, 0), p1r0 = S(-2, 0), p2r0 = S(-3, 0);
						const int32 q0r0 = S(0, 0), q1r0 = S(1, 0), q2r0 = S(2, 0);
						const int32 p0r3 = S(-1, 3), p1r3 = S(-2, 3), p2r3 = S(-3, 3);
						const int32 q0r3 = S(0, 3), q1r3 = S(1, 3), q2r3 = S(2, 3);
						const int32 dp0 = Abs32(p2r0 - 2 * p1r0 + p0r0);
						const int32 dq0 = Abs32(q2r0 - 2 * q1r0 + q0r0);
						const int32 dp3 = Abs32(p2r3 - 2 * p1r3 + p0r3);
						const int32 dq3 = Abs32(q2r3 - 2 * q1r3 + q0r3);
						const int32 d0 = dp0 + dq0, d3 = dp3 + dq3;
						if (d0 + d3 >= beta)
							return;
						const int32 tc25 = (tc * 5 + 1) >> 1;
						const bool strong =
							(Abs32((int32)S(-4, 0) - p0r0) + Abs32(q0r0 - (int32)S(3, 0)) < (beta >> 3)) &&
							Abs32(p0r0 - q0r0) < tc25 &&
							(Abs32((int32)S(-4, 3) - p0r3) + Abs32(q0r3 - (int32)S(3, 3)) < (beta >> 3)) &&
							Abs32(p0r3 - q0r3) < tc25 && (d0 << 1) < (beta >> 2) &&
							(d3 << 1) < (beta >> 2);
						if (strong) {
							const int32 tc2 = tc << 1;
							for (int32 d = 0; d < 4; ++d) {
								const int32 p3 = S(-4, d), p2 = S(-3, d), p1 = S(-2, d), p0 = S(-1, d);
								const int32 q0 = S(0, d), q1 = S(1, d), q2 = S(2, d), q3 = S(3, d);
								S(-1, d) = (nk_uint16)(p0 + Clip3i(-tc2, tc2,
																  ((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3) - p0));
								S(-2, d) = (nk_uint16)(p1 + Clip3i(-tc2, tc2,
																  ((p2 + p1 + p0 + q0 + 2) >> 2) - p1));
								S(-3, d) = (nk_uint16)(p2 + Clip3i(-tc2, tc2,
																  ((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3) - p2));
								S(0, d) = (nk_uint16)(q0 + Clip3i(-tc2, tc2,
																 ((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3) - q0));
								S(1, d) = (nk_uint16)(q1 + Clip3i(-tc2, tc2,
																 ((p0 + q0 + q1 + q2 + 2) >> 2) - q1));
								S(2, d) = (nk_uint16)(q2 + Clip3i(-tc2, tc2,
																 ((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3) - q2));
							}
						} else if (tc > 0) {
							const bool ndP = (dp0 + dp3) < ((beta + (beta >> 1)) >> 3);
							const bool ndQ = (dq0 + dq3) < ((beta + (beta >> 1)) >> 3);
							const int32 tcHalf = tc >> 1;
							for (int32 d = 0; d < 4; ++d) {
								const int32 p2 = S(-3, d), p1 = S(-2, d), p0 = S(-1, d);
								const int32 q0 = S(0, d), q1 = S(1, d), q2 = S(2, d);
								int32 delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
								if (Abs32(delta) >= 10 * tc)
									continue;
								delta = Clip3i(-tc, tc, delta);
								S(-1, d) = (nk_uint16)Clip3i(0, maxVal, p0 + delta);
								S(0, d) = (nk_uint16)Clip3i(0, maxVal, q0 - delta);
								if (ndP) {
									const int32 dp1 = Clip3i(-tcHalf, tcHalf,
															 (((p2 + p0 + 1) >> 1) - p1 + delta) >> 1);
									S(-2, d) = (nk_uint16)Clip3i(0, maxVal, p1 + dp1);
								}
								if (ndQ) {
									const int32 dq1 = Clip3i(-tcHalf, tcHalf,
															 (((q2 + q0 + 1) >> 1) - q1 - delta) >> 1);
									S(1, d) = (nk_uint16)Clip3i(0, maxVal, q1 + dq1);
								}
							}
						}
					}

					// Filtre un segment de 4 lignes/colonnes CHROMA (BS=2, §8.7.2.5.5).
					// Le QP vient des blocs LUMA co-localisés ; offset chroma = PPS seulement
					// (l'offset de slice ne s'applique PAS au déblocage).
					void ChromaDeblockSeg(bool vert, int32 cIdx, int32 xE, int32 yE) {
						nk_uint16 *C = (cIdx == 1) ? frame->cb.Data() : frame->cr.Data();
						const int32 stride = frame->chromaW;
						const int32 tcOff = sh->sliceTcOffsetDiv2 * 2;
						const int32 lx = xE << 1, ly = yE << 1;
						const int32 qpP = vert ? QpAt(lx - 1, ly) : QpAt(lx, ly - 1);
						const int32 qpQ = QpAt(lx, ly);
						const int32 qpAvg = (qpP + qpQ + 1) >> 1;
						const int32 off = (cIdx == 1) ? pps->ppsCbQpOffset : pps->ppsCrQpOffset;
						const int32 qPi = Clip3i(0, 57, qpAvg + off);
						int32 qPc;
						if (qPi < 30)
							qPc = qPi;
						else if (qPi > 43)
							qPc = qPi - 6;
						else
							qPc = kQpC[qPi - 30];
						const int32 tc = (int32)kTcTable[Clip3i(0, 53, qPc + 2 + tcOff)]
										 << (bitDepth - 8);
						if (tc <= 0)
							return;
						auto S = [&](int32 i, int32 d) -> nk_uint16 & {
							return vert ? C[(yE + d) * stride + (xE + i)]
										: C[(yE + i) * stride + (xE + d)];
						};
						const int32 mv = (1 << sps->bitDepthChroma) - 1;
						for (int32 d = 0; d < 4; ++d) {
							const int32 p1 = S(-2, d), p0 = S(-1, d);
							const int32 q0 = S(0, d), q1 = S(1, d);
							const int32 delta =
								Clip3i(-tc, tc, (((q0 - p0) * 4) + p1 - q1 + 4) >> 3);
							S(-1, d) = (nk_uint16)Clip3i(0, mv, p0 + delta);
							S(0, d) = (nk_uint16)Clip3i(0, mv, q0 - delta);
						}
					}

					void DeblockPicture() {
						// Ordre normatif : TOUTES les arêtes verticales de l'image, PUIS
						// toutes les horizontales (qui lisent les échantillons déjà filtrés
						// verticalement). Luma filtré si BS>=1 (tc dépend de BS) ; chroma
						// filtré UNIQUEMENT si BS==2 (§8.7.2.5.5), à partir des MÊMES cartes
						// de BS luma que la référence ffmpeg (grille 8-chroma = 16-luma).
						// Arêtes verticales luma (x 8-aligné, segment de 4 lignes).
						for (int32 x = 8; x < picW; x += 8)
							for (int32 y = 0; y < picH; y += 4) {
								const int32 bs = bsVert[(usize)((y >> 2) * w4L + (x >> 2))];
								if (bs)
									LumaDeblockSeg(true, x, y, bs);
							}
						// Arêtes horizontales luma (y 8-aligné, segment de 4 colonnes).
						for (int32 y = 8; y < picH; y += 8)
							for (int32 x = 0; x < picW; x += 4) {
								const int32 bs = bsHoriz[(usize)((y >> 2) * w4L + (x >> 2))];
								if (bs)
									LumaDeblockSeg(false, x, y, bs);
							}
						// Arêtes verticales chroma (luma x 16-aligné ; 4 lignes chroma = 8 luma).
						for (int32 x = 16; x < picW; x += 16)
							for (int32 y = 0; y < picH; y += 8)
								if (bsVert[(usize)((y >> 2) * w4L + (x >> 2))] == 2) {
									ChromaDeblockSeg(true, 1, x >> 1, y >> 1);
									ChromaDeblockSeg(true, 2, x >> 1, y >> 1);
								}
						// Arêtes horizontales chroma (luma y 16-aligné ; 4 colonnes chroma = 8 luma).
						for (int32 y = 16; y < picH; y += 16)
							for (int32 x = 0; x < picW; x += 8)
								if (bsHoriz[(usize)((y >> 2) * w4L + (x >> 2))] == 2) {
									ChromaDeblockSeg(false, 1, x >> 1, y >> 1);
									ChromaDeblockSeg(false, 2, x >> 1, y >> 1);
								}
					}

					// ---- SAO d'application (§8.7.3) -----------------------------------
					// Entrée = image DÉBLOQUÉE (copie source), sortie écrite dans les plans.
					void ApplySaoComponent(int32 cIdx, const nk_uint16 *src, nk_uint16 *dst,
										   int32 stride, int32 w, int32 h, const SaoCtb &s,
										   int32 x0, int32 y0, int32 nW, int32 nH) {
						const int32 bd = (cIdx == 0) ? bitDepth : sps->bitDepthChroma;
						const int32 mv = (1 << bd) - 1;
						if (s.type[cIdx] == 1) { // bande
							int32 table[32] = {0};
							for (int32 k = 0; k < 4; ++k)
								table[(k + s.bandPos[cIdx]) & 31] = s.offset[cIdx][k];
							for (int32 y = y0; y < y0 + nH && y < h; ++y)
								for (int32 x = x0; x < x0 + nW && x < w; ++x) {
									const int32 v = src[y * stride + x];
									dst[y * stride + x] =
										(nk_uint16)Clip3i(0, mv, v + table[(v >> (bd - 5)) & 31]);
								}
						} else if (s.type[cIdx] == 2) { // contour
							static const int32 kPos[4][2][2] = {
								{{-1, 0}, {1, 0}}, {{0, -1}, {0, 1}}, {{-1, -1}, {1, 1}}, {{1, -1}, {-1, 1}}};
							static const int32 kEdgeIdx[5] = {1, 2, 0, 3, 4};
							const int32 eo = s.eoClass[cIdx];
							const int32 ax = kPos[eo][0][0], ay = kPos[eo][0][1];
							const int32 bx = kPos[eo][1][0], by = kPos[eo][1][1];
							for (int32 y = y0; y < y0 + nH && y < h; ++y)
								for (int32 x = x0; x < x0 + nW && x < w; ++x) {
									// Voisin hors image -> échantillon non modifié (§8.7.3).
									if (x + ax < 0 || x + ax >= w || y + ay < 0 || y + ay >= h ||
										x + bx < 0 || x + bx >= w || y + by < 0 || y + by >= h)
										continue;
									const int32 c = src[y * stride + x];
									const int32 a = src[(y + ay) * stride + (x + ax)];
									const int32 b = src[(y + by) * stride + (x + bx)];
									const int32 sgnA = (c > a) - (c < a);
									const int32 sgnB = (c > b) - (c < b);
									const int32 m = kEdgeIdx[2 + sgnA + sgnB];
									if (m == 0)
										continue;
									dst[y * stride + x] =
										(nk_uint16)Clip3i(0, mv, c + s.offset[cIdx][m - 1]);
								}
						}
					}

					void ApplySao(const NkVector<nk_uint16> &srcY, const NkVector<nk_uint16> &srcCb,
								  const NkVector<nk_uint16> &srcCr) {
						const int32 ctbSize = 1 << ctbLog2;
						for (int32 ry = 0; ry < picHeightInCtbs; ++ry)
							for (int32 rx = 0; rx < picWidthInCtbs; ++rx) {
								const SaoCtb &s = saoCtb[(usize)(ry * picWidthInCtbs + rx)];
								ApplySaoComponent(0, srcY.Data(), frame->y.Data(), frame->lumaW, picW,
												  picH, s, rx * ctbSize, ry * ctbSize, ctbSize, ctbSize);
								const int32 cs = ctbSize >> 1;
								ApplySaoComponent(1, srcCb.Data(), frame->cb.Data(), frame->chromaW,
												  picW >> 1, picH >> 1, s, rx * cs, ry * cs, cs, cs);
								ApplySaoComponent(2, srcCr.Data(), frame->cr.Data(), frame->chromaW,
												  picW >> 1, picH >> 1, s, rx * cs, ry * cs, cs, cs);
							}
					}

					// ---- residual_coding (§7.3.8.11) ----------------------------------
					int32 CoeffAbsLevelRemaining(int32 riceParam) {
						int32 prefix = 0;
						while (prefix < 31 && Bypass())
							++prefix;
						if (prefix < 3)
							return (prefix << riceParam) + (int32)BypassBits(riceParam);
						if (prefix >= 31 || (prefix - 3 + riceParam) > 22) {
							okFlag = false;
							return 0;
						}
						const int32 k = prefix - 3 + riceParam;
						return (((1 << (prefix - 3)) + 3 - 1) << riceParam) + (int32)BypassBits(k);
					}

					// Parse les coefficients du TU dans coeffBuf (signés) puis reconstruit
					// si frame actif. (x0,y0) = coordonnées dans le plan de cIdx.
					void ParseResidual(int32 x0, int32 y0, int32 log2TrafoSize, int32 cIdx,
									   int32 predModeIntra) {
						const ScanTables &sc = Scans();
						++stats->tuCount;
						const int32 nSize = 1 << log2TrafoSize;
						if (frame)
							for (int32 i = 0; i < nSize * nSize; ++i)
								coeffBuf[i] = 0;

						bool transformSkip = false;
						if (pps->transformSkipEnabled && log2TrafoSize == 2 && !cuTransquantBypass)
							transformSkip = Bin(kHevcCtxTransformSkip + (cIdx ? 1 : 0)) != 0;

						// scanIdx (§7.4.9.11) — intra seulement ici.
						int32 scanIdx = kScanDiag;
						if (log2TrafoSize == 2 || (log2TrafoSize == 3 && cIdx == 0)) {
							if (predModeIntra >= 6 && predModeIntra <= 14)
								scanIdx = kScanVert;
							else if (predModeIntra >= 22 && predModeIntra <= 30)
								scanIdx = kScanHoriz;
						}

						// last_sig_coeff prefixes (TR contextés) + suffixes (bypass).
						const int32 maxPre = (log2TrafoSize << 1) - 1;
						int32 ctxOffset, ctxShift;
						if (cIdx == 0) {
							ctxOffset = 3 * (log2TrafoSize - 2) + ((log2TrafoSize - 1) >> 2);
							ctxShift = (log2TrafoSize + 1) >> 2;
						} else {
							ctxOffset = 15;
							ctxShift = log2TrafoSize - 2;
						}
						int32 lastX = 0, lastY = 0;
						while (lastX < maxPre &&
							   Bin(kHevcCtxLastSigCoeffXPrefix + ctxOffset + (lastX >> ctxShift)))
							++lastX;
						while (lastY < maxPre &&
							   Bin(kHevcCtxLastSigCoeffYPrefix + ctxOffset + (lastY >> ctxShift)))
							++lastY;
						if (lastX > 3) {
							const int32 nbits = (lastX >> 1) - 1;
							lastX = (1 << nbits) * (2 + (lastX & 1)) + (int32)BypassBits(nbits);
						}
						if (lastY > 3) {
							const int32 nbits = (lastY >> 1) - 1;
							lastY = (1 << nbits) * (2 + (lastY & 1)) + (int32)BypassBits(nbits);
						}
						if (scanIdx == kScanVert) {
							const int32 t = lastX;
							lastX = lastY;
							lastY = t;
						}

						// Sélection des scans + index composé du dernier coefficient.
						const uint8 *scanXOff, *scanYOff, *scanXCg, *scanYCg;
						int32 numCoeff;
						const int32 xCgLast = lastX >> 2, yCgLast = lastY >> 2;
						if (scanIdx == kScanDiag) {
							scanXOff = sc.diag4x4X;
							scanYOff = sc.diag4x4Y;
							numCoeff = sc.diag4x4Inv[lastY & 3][lastX & 3];
							if (log2TrafoSize == 2) {
								static const uint8 one[1] = {0};
								scanXCg = one;
								scanYCg = one;
							} else if (log2TrafoSize == 3) {
								numCoeff += sc.diag2x2Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag2x2X;
								scanYCg = sc.diag2x2Y;
							} else if (log2TrafoSize == 4) {
								numCoeff += sc.diag4x4Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag4x4X;
								scanYCg = sc.diag4x4Y;
							} else {
								numCoeff += sc.diag8x8Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag8x8X;
								scanYCg = sc.diag8x8Y;
							}
						} else if (scanIdx == kScanHoriz) {
							scanXOff = sc.rasterX;
							scanYOff = sc.rasterY;
							scanXCg = sc.horiz2x2X;
							scanYCg = sc.horiz2x2Y;
							numCoeff = HorizComposedIdx(lastY, lastX);
						} else { // vertical = horizontal transposé
							scanXOff = sc.rasterY;
							scanYOff = sc.rasterX;
							scanXCg = sc.horiz2x2Y;
							scanYCg = sc.horiz2x2X;
							numCoeff = HorizComposedIdx(lastX, lastY);
						}
						++numCoeff;
						const int32 numLastSubset = (numCoeff - 1) >> 4;

						uint8 sbFlag[8][8] = {{0}};
						int32 greater1Ctx = 1;

						for (int32 i = numLastSubset; i >= 0; --i) {
							const int32 xCg = scanXCg[i], yCg = scanYCg[i];
							int32 implicitNonZero = 0;
							uint8 sigIdx[16];
							int32 nbSig = 0;
							const int32 offset = i << 4;

							if (i < numLastSubset && i > 0) {
								int32 ctxCg = 0;
								if (xCg < (1 << (log2TrafoSize - 2)) - 1)
									ctxCg += sbFlag[xCg + 1][yCg];
								if (yCg < (1 << (log2TrafoSize - 2)) - 1)
									ctxCg += sbFlag[xCg][yCg + 1];
								sbFlag[xCg][yCg] = (uint8)Bin(kHevcCtxSigCoeffGroupFlag +
															 Min32(ctxCg, 1) + (cIdx ? 2 : 0));
								implicitNonZero = 1;
							} else {
								sbFlag[xCg][yCg] =
									(uint8)((xCg == xCgLast && yCg == yCgLast) || (xCg == 0 && yCg == 0));
							}

							const int32 lastScanPos = numCoeff - offset - 1;
							int32 nEnd;
							if (i == numLastSubset) {
								nEnd = lastScanPos - 1;
								sigIdx[0] = (uint8)lastScanPos;
								nbSig = 1;
							} else {
								nEnd = 15;
							}

							int32 prevCsbf = 0;
							if (xCg < ((1 << log2TrafoSize) - 1) >> 2)
								prevCsbf = sbFlag[xCg + 1][yCg] ? 1 : 0;
							if (yCg < ((1 << log2TrafoSize) - 1) >> 2)
								prevCsbf += sbFlag[xCg][yCg + 1] ? 2 : 0;

							if (sbFlag[xCg][yCg] && nEnd >= 0) {
								// sig_coeff_flag — même dérivation que la référence ffmpeg.
								const uint8 *map;
								int32 scfOffset = (cIdx != 0) ? 27 : 0;
								if (log2TrafoSize == 2) {
									map = &kSigCtxIdxMap[scanIdx][0];
								} else {
									map = &kSigCtxIdxMap[scanIdx][(prevCsbf + 1) << 4];
									if (cIdx == 0) {
										if (xCg > 0 || yCg > 0)
											scfOffset += 3;
										if (log2TrafoSize == 3)
											scfOffset += (scanIdx == kScanDiag) ? 9 : 15;
										else
											scfOffset += 21;
									} else {
										scfOffset += (log2TrafoSize == 3) ? 9 : 12;
									}
								}
								const int32 nb0 = nbSig;
								for (int32 n = nEnd; n > 0; --n) {
									const int32 sig = (int32)Bin(kHevcCtxSigCoeffFlag + map[n] + scfOffset);
									sigIdx[nbSig] = (uint8)n;
									nbSig += sig;
								}
								if (nbSig != nb0)
									implicitNonZero = 0;
								if (implicitNonZero == 0) {
									int32 dcOffset;
									if (i == 0)
										dcOffset = (cIdx == 0) ? 0 : 27;
									else
										dcOffset = 2 + scfOffset;
									sigIdx[nbSig] = 0;
									nbSig += (int32)Bin(kHevcCtxSigCoeffFlag + dcOffset);
								} else {
									sigIdx[nbSig] = 0;
									++nbSig;
								}
							}

							if (nbSig <= 0)
								continue;
							stats->nonZeroCoeffs += nbSig;

							// greater1 (max 8), greater2 (1er greater1), signes, restes Rice.
							int32 ctxSet = (i > 0 && cIdx == 0) ? 2 : 0;
							if (i != numLastSubset && greater1Ctx == 0)
								++ctxSet;
							greater1Ctx = 1;
							uint8 g1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
							int32 firstG1 = -1;
							const int32 nG1 = Min32(nbSig, 8);
							for (int32 m = 0; m < nG1; ++m) {
								const int32 inc = (ctxSet << 2) + greater1Ctx + (cIdx ? 16 : 0);
								const int32 flag = (int32)Bin(kHevcCtxCoeffAbsGreater1 + inc);
								g1[m] = (uint8)flag;
								if (flag) {
									if (firstG1 < 0)
										firstG1 = m;
									greater1Ctx = 0;
								} else if (greater1Ctx >= 1 && greater1Ctx < 3) {
									++greater1Ctx;
								}
							}
							const int32 lastNzPos = sigIdx[0];
							const int32 firstNzPos = sigIdx[nbSig - 1];
							const bool signHidden = !cuTransquantBypass && (lastNzPos - firstNzPos >= 4);
							if (firstG1 >= 0)
								g1[firstG1] = (uint8)(g1[firstG1] +
													  Bin(kHevcCtxCoeffAbsGreater2 + ctxSet + (cIdx ? 4 : 0)));
							const bool hide = pps->signDataHiding && signHidden;
							const int32 nbSigns = hide ? (nbSig - 1) : nbSig;
							const uint32 signBits = BypassBits(nbSigns);

							int32 riceParam = 0;
							int32 sumAbs = 0;
							for (int32 m = 0; m < nbSig; ++m) {
								nk_int64 level;
								if (m < 8) {
									level = 1 + g1[m];
									const nk_int64 escape = (m == firstG1) ? 3 : 2;
									if (level == escape) {
										level += CoeffAbsLevelRemaining(riceParam);
										if (level > (3 << riceParam))
											riceParam = Min32(riceParam + 1, 4);
									}
								} else {
									level = 1 + CoeffAbsLevelRemaining(riceParam);
									if (level > (3 << riceParam))
										riceParam = Min32(riceParam + 1, 4);
								}
								if (!okFlag)
									return;
								sumAbs += (int32)level;
								nk_int64 signedLevel = level;
								if (m < nbSigns && ((signBits >> (nbSigns - 1 - m)) & 1u))
									signedLevel = -signedLevel;
								if (hide && m == nbSig - 1 && (sumAbs & 1))
									signedLevel = -signedLevel; // signe caché = parité de la somme
								if (frame) {
									const int32 nPos = sigIdx[m];
									const int32 xC = (xCg << 2) + scanXOff[nPos];
									const int32 yC = (yCg << 2) + scanYOff[nPos];
									coeffBuf[yC * nSize + xC] =
										(int32)Clip3i(-32768, 32767, (int32)signedLevel);
								}
							}
						}

						if (frame)
							ReconstructResidual(cIdx, x0, y0, log2TrafoSize, transformSkip);
					}

					// ---- transform_tree / transform_unit (§7.3.8.8/10) ----------------
					void ParseTransformTree(int32 x0, int32 y0, int32 xBase, int32 yBase, int32 log2Size,
											int32 depth, int32 blkIdx, int32 pbIdx, bool parentCbfCb,
											bool parentCbfCr) {
						if (!okFlag)
							return;
						bool split;
						// §7.4.9.8 : max_trafo_depth vient de max_transform_hierarchy_depth_
						// INTRA (+1 si intra_split_flag) OU _INTER selon CuPredMode — PAS
						// toujours la variante intra (bug corrigé : les deux valeurs peuvent
						// différer, même si elles coïncident souvent à 0 chez x265).
						const int32 maxDepth =
							cuIsIntra ? (maxTrafoDepthIntra + (intraSplit ? 1 : 0)) : maxTrafoDepthInter;
						if (log2Size <= maxTbLog2 && log2Size > minTbLog2 && depth < maxDepth &&
							!(intraSplit && depth == 0)) {
							split = Bin(kHevcCtxSplitTransform + 5 - log2Size) != 0;
						} else {
							// inter_split (référence ffmpeg hls_transform_tree) : quand la
							// profondeur inter max est 0 et le CU inter n'est PAS 2Nx2N, le
							// split à la profondeur 0 est FORCÉ (pour aligner l'arbre de
							// transformées sur les limites de PU) — AUCUN bit lu pour ce cas.
							const bool interSplit = !cuIsIntra && maxTrafoDepthInter == 0 &&
													curPartMode != kPart2Nx2N && depth == 0;
							split = (log2Size > maxTbLog2) || (intraSplit && depth == 0) || interSplit;
						}
						bool cbfCb = parentCbfCb, cbfCr = parentCbfCr;
						if (log2Size > 2) { // 4:2:0
							if (depth == 0 || parentCbfCb)
								cbfCb = Bin(kHevcCtxCbfCbCr + depth) != 0;
							else
								cbfCb = false;
							if (depth == 0 || parentCbfCr)
								cbfCr = Bin(kHevcCtxCbfCbCr + depth) != 0;
							else
								cbfCr = false;
						}
						if (split) {
							const int32 h = 1 << (log2Size - 1);
							const bool pbSplitLevel = intraSplit && depth == 0;
							ParseTransformTree(x0, y0, x0, y0, log2Size - 1, depth + 1, 0,
											   pbSplitLevel ? 0 : pbIdx, cbfCb, cbfCr);
							ParseTransformTree(x0 + h, y0, x0, y0, log2Size - 1, depth + 1, 1,
											   pbSplitLevel ? 1 : pbIdx, cbfCb, cbfCr);
							ParseTransformTree(x0, y0 + h, x0, y0, log2Size - 1, depth + 1, 2,
											   pbSplitLevel ? 2 : pbIdx, cbfCb, cbfCr);
							ParseTransformTree(x0 + h, y0 + h, x0, y0, log2Size - 1, depth + 1, 3,
											   pbSplitLevel ? 3 : pbIdx, cbfCb, cbfCr);
							return;
						}
						// Feuille : cbf_luma — INFÉRÉ à 1 (PAS lu) pour un CU INTER à la
						// profondeur 0 SANS cbf chroma (§7.3.8.8 : le résidu luma est alors
						// implicitement présent, cf. hls_transform_unit de la référence).
						bool cbfLuma = true;
						if (cuIsIntra || depth != 0 || cbfCb || cbfCr)
							cbfLuma = Bin(kHevcCtxCbfLuma + (depth == 0 ? 1 : 0)) != 0;
						if ((cbfLuma || cbfCb || cbfCr) && pps->cuQpDeltaEnabled && !isCuQpDeltaCoded) {
							// cu_qp_delta_abs : préfixe TR contexté (bin0 ctx+0, suite ctx+1,
							// cMax 5) + suffixe EG0 bypass, puis signe bypass si non nul.
							int32 prefix = 0, inc = 0;
							while (prefix < 5 && Bin(kHevcCtxCuQpDelta + inc)) {
								++prefix;
								inc = 1;
							}
							int32 v = prefix;
							if (prefix >= 5) {
								int32 k = 0;
								int32 suffix = 0;
								while (k < 7 && Bypass()) {
									suffix += 1 << k;
									++k;
								}
								if (k == 7) {
									okFlag = false;
									return;
								}
								int32 kk = k;
								while (kk--)
									suffix += (int32)Bypass() << kk;
								v += suffix;
							}
							if (v != 0 && Bypass())
								v = -v; // cu_qp_delta_sign_flag
							cuQpDeltaVal = v;
							isCuQpDeltaCoded = true;
							++stats->qpDeltaCount;
						}
						// Mode intra luma du bloc courant (partition couvrante) — -1 pour un
						// CU INTER (brique 8) : force le scan résidus en diagonal (§7.4.9.11,
						// la sélection horiz/vert selon le mode intra ne s'applique QUE si
						// CuPredMode == MODE_INTRA), et interdit toute prédiction intra tant
						// que la reconstruction inter (MC) n'est pas implémentée.
						const int32 lumaMode = cuIsIntra ? curIntraModeY[intraSplit ? pbIdx : 0] : -1;
						const int32 chromaMode = cuIsIntra ? curIntraModeC : -1;
						// Reconstruction luma : prédiction TOUJOURS (intra seulement pour
						// l'instant), résidu si cbf (parse — reconstruit seulement si frame).
						if (frame && cuIsIntra)
							PredictIntra(0, x0, y0, log2Size, lumaMode);
						if (cbfLuma)
							ParseResidual(x0, y0, log2Size, 0, lumaMode);
						if (frame && cuIsIntra)
							MarkLumaRecon(x0, y0, 1 << log2Size);
						// Chroma (4:2:0).
						if (log2Size > 2) {
							const int32 cx = x0 >> 1, cy = y0 >> 1;
							if (frame && cuIsIntra)
								PredictIntra(1, cx, cy, log2Size - 1, chromaMode);
							if (cbfCb)
								ParseResidual(cx, cy, log2Size - 1, 1, chromaMode);
							if (frame && cuIsIntra)
								PredictIntra(2, cx, cy, log2Size - 1, chromaMode);
							if (cbfCr)
								ParseResidual(cx, cy, log2Size - 1, 2, chromaMode);
							if (frame && cuIsIntra)
								MarkChromaRecon(cx, cy, 1 << (log2Size - 1));
						} else if (blkIdx == 3) {
							const int32 cx = xBase >> 1, cy = yBase >> 1;
							if (frame && cuIsIntra)
								PredictIntra(1, cx, cy, 2, chromaMode);
							if (parentCbfCb)
								ParseResidual(cx, cy, 2, 1, chromaMode);
							if (frame && cuIsIntra)
								PredictIntra(2, cx, cy, 2, chromaMode);
							if (parentCbfCr)
								ParseResidual(cx, cy, 2, 2, chromaMode);
							if (frame && cuIsIntra)
								MarkChromaRecon(cx, cy, 4);
						}
						// Déblocage §8.7.2.4 : marque cbf_luma du TU puis dérive la BS de ses
						// arêtes (intra ET inter). L'ordre importe (la BS lit cbf_luma).
						if (frame && !sh->deblockingFilterDisabled) {
							if (cbfLuma)
								MarkCbfLuma(x0, y0, 1 << log2Size);
							DeriveDeblockBs(x0, y0, log2Size);
						}
					}

					// ---- part_mode (§9.3.3.7 — CU inter, cf. ff_hevc_part_mode_decode) -
					int32 DecodePartMode(int32 log2CbSize) {
						if (Bin(kHevcCtxPartMode + 0))
							return kPart2Nx2N;
						if (log2CbSize == minCbLog2) {
							if (cuIsIntra)
								return kPartNxN;
							if (Bin(kHevcCtxPartMode + 1))
								return kPart2NxN;
							if (log2CbSize == 3)
								return kPartNx2N;
							if (Bin(kHevcCtxPartMode + 2))
								return kPartNx2N;
							return kPartNxN;
						}
						if (!sps->ampEnabled) {
							if (Bin(kHevcCtxPartMode + 1))
								return kPart2NxN;
							return kPartNx2N;
						}
						if (Bin(kHevcCtxPartMode + 1)) {
							if (Bin(kHevcCtxPartMode + 3))
								return kPart2NxN;
							return Bypass() ? kPart2NxnD : kPart2NxnU;
						}
						if (Bin(kHevcCtxPartMode + 3))
							return kPartNx2N;
						return Bypass() ? kPartnRx2N : kPartnLx2N;
					}

					// ref_idx_lx (§9.3.3.13) — ffmpeg n'utilise QU'UN SEUL jeu de contextes
					// (REF_IDX_L0_OFFSET) pour L0 ET L1 (vérifié dans la référence :
					// ff_hevc_ref_idx_lx_decode ne reçoit jamais l'offset L1) — reproduit
					// à l'identique pour rester bit-exact (les contextes ref_idx_l1
					// existent dans la table mais ne sont JAMAIS référencés).
					int32 DecodeRefIdx(int32 numRefIdxActive) {
						int32 i = 0;
						const int32 maxV = numRefIdxActive - 1;
						const int32 maxCtx = Min32(maxV, 2);
						while (i < maxCtx && Bin(kHevcCtxRefIdxL0 + i))
							++i;
						if (i == 2)
							while (i < maxV && Bypass())
								++i;
						return i;
					}

					// mvd_coding() (§7.3.8.9/9.3.3.9) — EGk (k=1) : préfixe unaire bypass
					// (k part de 1, incrémenté tant qu'un bit=1 est lu) puis k bits de
					// suffixe puis signe. Valeur = (1<<k) + suffixe (dérivé par
					// concordance de consommation de bits avec la brique 8, qui ne
					// lisait que la structure) — retourne la valeur SIGNÉE complète
					// (brique 10 : nécessaire à la résolution du MV).
					int32 DecodeMvdComponent(int32 gt0, int32 gt1) {
						if (!gt0)
							return 0;
						if (!gt1)
							return Bypass() ? -1 : 1;
						int32 k = 1;
						while (k < 31 && Bypass())
							++k;
						uint32 suffix = 0;
						for (int32 b = 0; b < k; ++b)
							suffix = (suffix << 1) | Bypass();
						const int32 mag = (int32)((1u << k) + suffix);
						return Bypass() ? -mag : mag;
					}
					void DecodeMvd(int32 &dx, int32 &dy) {
						const int32 gt0x = (int32)Bin(kHevcCtxAbsMvdGreater0 + 0);
						const int32 gt0y = (int32)Bin(kHevcCtxAbsMvdGreater0 + 0);
						const int32 gt1x = gt0x ? (int32)Bin(kHevcCtxAbsMvdGreater1 + 1) : 0;
						const int32 gt1y = gt0y ? (int32)Bin(kHevcCtxAbsMvdGreater1 + 1) : 0;
						dx = DecodeMvdComponent(gt0x, gt1x);
						dy = DecodeMvdComponent(gt0y, gt1y);
					}

					// ---- Champ de MV par bloc 4x4 (voisinage merge/AMVP) — bi-liste ----
					// MvField complet (POD, copié par valeur, ffmpeg struct MvField) :
					// `avail` = position/bloc disponible (inter) ; `predFlag` bit0=L0,
					// bit1=L1 (3=BI) ; mv/refIdx par liste.
					struct MvField {
							bool avail = false;
							uint8 predFlag = 0;
							int32 mvx[2] = {0, 0}, mvy[2] = {0, 0};
							int32 refIdx[2] = {0, 0};
					};
					void StoreMv(int32 x0, int32 y0, int32 w, int32 h, const MvField &f) {
						for (int32 j = 0; j < h; j += 4)
							for (int32 i = 0; i < w; i += 4) {
								const int32 px = (x0 + i) >> 2, py = (y0 + j) >> 2;
								const usize idx = (usize)(py * minPuWidth + px);
								mvL0x[idx] = (int16)f.mvx[0];
								mvL0y[idx] = (int16)f.mvy[0];
								mvL1x[idx] = (int16)f.mvx[1];
								mvL1y[idx] = (int16)f.mvy[1];
								refIdxL0[idx] = (int8)f.refIdx[0];
								refIdxL1[idx] = (int8)f.refIdx[1];
								predFlag[idx] = f.predFlag;
								mvValid[idx] = 1;
							}
					}
					MvField GetMvField(int32 x, int32 y) const {
						MvField f;
						const usize idx = (usize)((y >> 2) * minPuWidth + (x >> 2));
						if (!mvValid[idx] || predFlag[idx] == 0)
							return f; // intra / indisponible
						f.avail = true;
						f.predFlag = predFlag[idx];
						f.mvx[0] = mvL0x[idx];
						f.mvy[0] = mvL0y[idx];
						f.refIdx[0] = refIdxL0[idx];
						f.mvx[1] = mvL1x[idx];
						f.mvy[1] = mvL1y[idx];
						f.refIdx[1] = refIdxL1[idx];
						return f;
					}

					// ---- Voisinage inter (§6.4.1, simplifié tuile unique/slice unique) -
					bool CandLeft(int32 x0) const {
						return x0 > 0;
					}
					bool CandUp(int32 y0) const {
						return y0 > 0;
					}
					bool CandUpLeft(int32 x0, int32 y0) const {
						return x0 > 0 && y0 > 0;
					}
					// cand_up_right_sap : dépend de l'alignement CTB (§6.4.1) — EXACT (pas
					// une approximation) pour le cas mono-tuile/mono-slice : quand le PU
					// touche le bord droit de son CTB, le voisin est le CTB diagonal
					// (déjà décodé ssi rangée précédente ET colonne suivante existent, et
					// le PU est en haut de son propre CTB) ; sinon simple cand_up.
					// ⚠️ Le voisin (x0+nPbW, y0-1) doit d'abord être DANS L'IMAGE (§6.4.1,
					// tout candidat hors image est indisponible) — un CU forcé-scindé par
					// la fenêtre d'image (dernière colonne de CTU partielle) peut avoir son
					// bord droit EXACTEMENT sur `picW` sans que x0b+nPbW touche le bord de
					// CTB (le CU est plus petit que le CTB à cause du split forcé, pas d'un
					// split volontaire) : la branche générique retournait alors `y0>0` sans
					// vérifier `x0+nPbW<picW`, un voisin hors image passait pour disponible.
					bool CandUpRight(int32 x0, int32 y0, int32 nPbW) const {
						if (x0 + nPbW >= picW || y0 <= 0)
							return false;
						const int32 ctb = 1 << ctbLog2;
						const int32 x0b = x0 & (ctb - 1), y0b = y0 & (ctb - 1);
						const int32 rx = x0 >> ctbLog2, ry = y0 >> ctbLog2;
						if (x0b + nPbW == ctb) {
							const bool ctbUpRight = (ry > 0) && (rx + 1 < picWidthInCtbs);
							return ctbUpRight && (y0b == 0);
						}
						return true;
					}
					bool CandBottomLeft(int32 x0, int32 y0, int32 nPbH) const {
						if (y0 + nPbH >= picH)
							return false;
						return x0 > 0;
					}
					bool IsDiffMer(int32 x0, int32 y0, int32 xN, int32 yN) const {
						const int32 pl = pps->log2ParallelMergeLevel;
						return ((x0 >> pl) != (xN >> pl)) || ((y0 >> pl) != (yN >> pl));
					}

					MvField GetCand(int32 xN, int32 yN, bool posOk) const {
						if (!posOk)
							return MvField{};
						return GetMvField(xN, yN);
					}
					// compare_mv_ref_idx (§8.5.3.2.3, ffmpeg mvs.c) — élagage merge : deux
					// MvField égaux si même predFlag ET (par liste active) mêmes refIdx+MV.
					// Comparaison sur refIdx BRUT (toutes les cand viennent de la trame
					// courante -> mêmes listes L0/L1, donc refIdx == POC équivalents).
					static bool CompareMvRefIdx(const MvField &a, const MvField &b) {
						if (!a.avail || !b.avail || a.predFlag != b.predFlag)
							return false;
						if (a.predFlag == 3)
							return a.refIdx[0] == b.refIdx[0] && a.mvx[0] == b.mvx[0] &&
								   a.mvy[0] == b.mvy[0] && a.refIdx[1] == b.refIdx[1] &&
								   a.mvx[1] == b.mvx[1] && a.mvy[1] == b.mvy[1];
						if (a.predFlag == 1)
							return a.refIdx[0] == b.refIdx[0] && a.mvx[0] == b.mvx[0] &&
								   a.mvy[0] == b.mvy[0];
						if (a.predFlag == 2)
							return a.refIdx[1] == b.refIdx[1] && a.mvx[1] == b.mvx[1] &&
								   a.mvy[1] == b.mvy[1];
						return false;
					}
					// POC de la référence refPicList[listX].list[refIdx] (§8.3.4).
					int32 RefPocL(int32 listX, int32 refIdx) const {
						if (listX == 0)
							return (refIdx >= 0 && refIdx < numRefsL0 && refsL0[refIdx]) ? refsL0[refIdx]->poc
																						  : 0;
						return (refIdx >= 0 && refIdx < numRefsL1 && refsL1[refIdx]) ? refsL1[refIdx]->poc : 0;
					}
					// Régime de pondération actif (§8.5.3.3.4.2/3) : P -> weighted_pred_flag,
					// B -> weighted_bipred_flag.
					bool WeightedActive() const {
						return (sh->sliceType == kHevcSliceP && pps->weightedPred) ||
							   (sh->sliceType == kHevcSliceB && pps->weightedBipred);
					}
					// mv_scale (ffmpeg mvs.c) — reproduit à l'identique : clip td/tb sur 8
					// bits, scaleFactor sur 12 bits SIGNÉS ([-2048,2047], PAS [-4096,4095]),
					// arrondi par (t+127+(t<0))>>8 (équivalent à l'arrondi signé usuel mais
					// c'est CETTE forme précise qui est normative bit-exacte).
					void ScaleMv(int32 mvx, int32 mvy, int32 td, int32 tb, int32 &outX,
								 int32 &outY) const {
						td = Clip3i(-128, 127, td);
						tb = Clip3i(-128, 127, tb);
						const int32 tx = (16384 + Abs32(td / 2)) / td;
						const int32 scaleFactor = Clip3i(-2048, 2047, (tb * tx + 32) >> 6);
						auto scaleOne = [&](int32 v) -> int32 {
							const int32 t = scaleFactor * v;
							const int32 r = (t + 127 + (t < 0 ? 1 : 0)) >> 8;
							return Clip3i(-32768, 32767, r);
						};
						outX = scaleOne(mvx);
						outY = scaleOne(mvy);
					}

					// ---- Fusion spatiale + temporelle + combinée bi (§8.5.3.2.2, ffmpeg
					// derive_spatial_merge_candidates) — produit MvField[5]. Ordre spatial
					// A1,B1,B0,A0,B2 (exclusions de partition §8.5.3.2.3 + élagage
					// CompareMvRefIdx paires B1~A1,B0~B1,A0~A1,B2~{A1,B1}), puis temporel
					// (deux passes L0/L1), puis candidats combinés bi (B), puis nuls bi.
					void DeriveMergeCandidates(int32 x0, int32 y0, int32 nPbW, int32 nPbH,
											   int32 partIdx, int32 partMode, MvField (&list)[5],
											   int32 &numCand) {
						const bool isB = (sh->sliceType == kHevcSliceB);
						const bool a1Excl = (partIdx == 1) && (partMode == kPartNx2N ||
																partMode == kPartnLx2N ||
																partMode == kPartnRx2N);
						const bool b1Excl = (partIdx == 1) && (partMode == kPart2NxN ||
																partMode == kPart2NxnU ||
																partMode == kPart2NxnD);
						const MvField a1 =
							a1Excl ? MvField{} : GetCand(x0 - 1, y0 + nPbH - 1, CandLeft(x0));
						const MvField b1 =
							b1Excl ? MvField{} : GetCand(x0 + nPbW - 1, y0 - 1, CandUp(y0));
						const MvField b0 = GetCand(x0 + nPbW, y0 - 1,
												   CandUpRight(x0, y0, nPbW) &&
													   IsDiffMer(x0, y0, x0 + nPbW, y0 - 1));
						const MvField a0 = GetCand(x0 - 1, y0 + nPbH,
												   CandBottomLeft(x0, y0, nPbH) &&
													   IsDiffMer(x0, y0, x0 - 1, y0 + nPbH));
						const MvField b2 = GetCand(
							x0 - 1, y0 - 1, CandUpLeft(x0, y0) && IsDiffMer(x0, y0, x0 - 1, y0 - 1));

						numCand = 0;
						if (a1.avail)
							list[numCand++] = a1;
						if (b1.avail && !CompareMvRefIdx(b1, a1))
							list[numCand++] = b1;
						if (b0.avail && !CompareMvRefIdx(b0, b1))
							list[numCand++] = b0;
						if (a0.avail && !CompareMvRefIdx(a0, a1))
							list[numCand++] = a0;
						if (numCand != 4 && b2.avail && !CompareMvRefIdx(b2, a1) &&
							!CompareMvRefIdx(b2, b1))
							list[numCand++] = b2;
						// Candidat temporel (§8.5.3.2.9) — deux passes X=0 puis X=1 (refIdx=0),
						// predFlag = availL0 | (availL1 << 1).
						if (sh->sliceTemporalMvpEnabled && numCand < sh->maxNumMergeCand) {
							int32 l0x = 0, l0y = 0, l1x = 0, l1y = 0;
							const bool aL0 = DeriveTemporalColocatedMv(x0, y0, nPbW, nPbH, 0, 0, l0x, l0y);
							const bool aL1 =
								isB ? DeriveTemporalColocatedMv(x0, y0, nPbW, nPbH, 1, 0, l1x, l1y) : false;
							if (aL0 || aL1) {
								MvField &c = list[numCand];
								c = MvField{};
								c.avail = true;
								c.predFlag = (uint8)((aL0 ? 1 : 0) | (aL1 ? 2 : 0));
								c.mvx[0] = l0x;
								c.mvy[0] = l0y;
								c.mvx[1] = l1x;
								c.mvy[1] = l1y;
								++numCand;
							}
						}
						const int32 numOrig = numCand;
						// Candidats de fusion bi combinés (§8.5.3.2.4, B uniquement).
						if (isB && numOrig > 1 && numOrig < sh->maxNumMergeCand) {
							for (int32 comb = 0; numCand < sh->maxNumMergeCand &&
												 comb < numOrig * (numOrig - 1);
								 ++comb) {
								const int32 l0i = kHevcL0L1CandIdx[comb][0];
								const int32 l1i = kHevcL0L1CandIdx[comb][1];
								const MvField &l0c = list[l0i];
								const MvField &l1c = list[l1i];
								if ((l0c.predFlag & 1) && (l1c.predFlag & 2) &&
									(RefPocL(0, l0c.refIdx[0]) != RefPocL(1, l1c.refIdx[1]) ||
									 l0c.mvx[0] != l1c.mvx[1] || l0c.mvy[0] != l1c.mvy[1])) {
									MvField &c = list[numCand];
									c = MvField{};
									c.avail = true;
									c.predFlag = 3;
									c.refIdx[0] = l0c.refIdx[0];
									c.refIdx[1] = l1c.refIdx[1];
									c.mvx[0] = l0c.mvx[0];
									c.mvy[0] = l0c.mvy[0];
									c.mvx[1] = l1c.mvx[1];
									c.mvy[1] = l1c.mvy[1];
									++numCand;
								}
							}
						}
						// Candidats nuls (§8.5.3.2.5) : predFlag L0 (P) ou BI (B), refIdx cyclé.
						const int32 nbRefs = isB ? Min32(numRefsL0, numRefsL1) : numRefsL0;
						int32 zeroIdx = 0;
						while (numCand < sh->maxNumMergeCand) {
							MvField &c = list[numCand];
							c = MvField{};
							c.avail = true;
							c.predFlag = (uint8)(isB ? 3 : 1);
							c.refIdx[0] = (zeroIdx < nbRefs) ? zeroIdx : 0;
							c.refIdx[1] = (zeroIdx < nbRefs) ? zeroIdx : 0;
							++numCand;
							++zeroIdx;
						}
					}

					// ---- AMVP par liste (§8.5.3.2.6/7, ffmpeg ff_hevc_luma_mv_mvp_mode) :
					// chaque voisin peut fournir le candidat via SA liste L0 OU L1 (test des
					// deux via MpMx/MpMxLt). Ordre exact : A0-exact-l0,A0-exact-l1,A1-exact-
					// l0,A1-exact-l1 ; A0-scaled..A1-scaled ; groupe B exact ; scaled B
					// seulement si !isScaledFlagL0 (+ promotion mxB->mxA). Candidat temporel
					// AMVP (§8.5.3.2.8) si < 2. `listX`/`refIdxLx` = liste et référence cible.
					// MP_MX : voisin dispose de la liste `predIdx` ET son POC == targetPoc.
					bool MpMx(const MvField &c, int32 predIdx, int32 targetPoc, int32 &mx,
							  int32 &my) const {
						if (!c.avail || !(c.predFlag & (1 << predIdx)))
							return false;
						if (RefPocL(predIdx, c.refIdx[predIdx]) != targetPoc)
							return false;
						mx = c.mvx[predIdx];
						my = c.mvy[predIdx];
						return true;
					}
					// MP_MX_LT : voisin dispose de la liste `predIdx` (n'importe quelle réf) ->
					// MV mis à l'échelle vers targetPoc (dist_scale, court-terme uniquement).
					bool MpMxLt(const MvField &c, int32 predIdx, int32 curPoc, int32 targetPoc,
								int32 &mx, int32 &my) const {
						if (!c.avail || !(c.predFlag & (1 << predIdx)))
							return false;
						const int32 nx = c.mvx[predIdx], ny = c.mvy[predIdx];
						const int32 neighborPoc = RefPocL(predIdx, c.refIdx[predIdx]);
						if (neighborPoc == targetPoc) {
							mx = nx;
							my = ny;
							return true;
						}
						int32 td = curPoc - neighborPoc;
						if (td == 0)
							td = 1;
						const int32 tb = curPoc - targetPoc;
						ScaleMv(nx, ny, td, tb, mx, my);
						return true;
					}
					// ---- Candidat temporel bi-liste (§8.5.3.2.8/9, ffmpeg
					// derive_temporal_colocated_mvs + check_mvset + mv_scale). colPic =
					// (collocatedFromL0 ? refsL0 : refsL1)[collocatedRefIdx]. Position bas-
					// droite du PU d'abord (repliée si hors CTB/image), sinon centre. Le champ
					// colocalisé stocke le POC de CHAQUE liste (mvColRefPocL0/L1). `listX` =
					// liste cible, `targetRefIdx` = réf cible vers laquelle mettre à l'échelle.
					struct ColMv {
							uint8 predFlag = 0;
							int32 mvx[2] = {0, 0}, mvy[2] = {0, 0}, refPoc[2] = {0, 0};
					};
					static bool SampleColField(const NkHevcFrame *colPic, int32 xCol, int32 yCol,
											   ColMv &out) {
						const int32 px = xCol >> 2, py = yCol >> 2;
						if (px < 0 || py < 0 || px >= colPic->mvColPuWidth ||
							py >= colPic->mvColPuHeight)
							return false;
						const usize idx = (usize)(py * colPic->mvColPuWidth + px);
						out.predFlag = colPic->mvColPredFlag[idx];
						out.mvx[0] = colPic->mvColX[idx];
						out.mvy[0] = colPic->mvColY[idx];
						out.refPoc[0] = colPic->mvColRefPocL0[idx];
						out.mvx[1] = colPic->mvColL1X[idx];
						out.mvy[1] = colPic->mvColL1Y[idx];
						out.refPoc[1] = colPic->mvColRefPocL1[idx];
						return true;
					}
					// check_mvset + mv_scale (court-terme : cur_lt==col_lt==0).
					bool CheckMvSet(int32 mvx, int32 mvy, int32 colPoc, int32 colRefPoc,
									int32 listX, int32 targetRefIdx, int32 &outX, int32 &outY) const {
						const int32 colPocDiff = colPoc - colRefPoc;
						const int32 targetPoc = RefPocL(listX, targetRefIdx);
						const int32 curPocDiff = frame->poc - targetPoc;
						if (colPocDiff == curPocDiff || colPocDiff == 0) {
							outX = mvx;
							outY = mvy;
						} else {
							ScaleMv(mvx, mvy, colPocDiff, curPocDiff, outX, outY);
						}
						return true;
					}
					// derive_temporal_colocated_mvs : sélectionne la liste du bloc colocalisé
					// selon son predFlag et check_diffpicount (low-delay).
					bool DeriveColMv(const ColMv &tc, int32 colPoc, int32 listX, int32 targetRefIdx,
									 int32 &outX, int32 &outY) const {
						if (tc.predFlag == 0)
							return false; // intra
						int32 useList;
						if (!(tc.predFlag & 1))
							useList = 1; // L1 seul
						else if (tc.predFlag == 1)
							useList = 0; // L0 seul
						else {
							// BI : check_diffpicount — une réf des listes courantes a-t-elle un
							// POC > POC courant ? (sinon low-delay).
							int32 checkDiff = 0;
							for (int32 j = 0; j < 2; ++j) {
								const NkHevcFrame *const *lst = (j == 0) ? refsL0 : refsL1;
								const int32 nn = (j == 0) ? numRefsL0 : numRefsL1;
								for (int32 i = 0; i < nn; ++i)
									if (lst && lst[i] && lst[i]->poc > frame->poc) {
										++checkDiff;
										break;
									}
							}
							if (checkDiff == 0)
								useList = (listX == 0) ? 0 : 1;
							else
								useList = (!sh->collocatedFromL0) ? 0 : 1;
						}
						return CheckMvSet(tc.mvx[useList], tc.mvy[useList], colPoc, tc.refPoc[useList],
										  listX, targetRefIdx, outX, outY);
					}
					bool DeriveTemporalColocatedMv(int32 x0, int32 y0, int32 nPbW, int32 nPbH,
												   int32 listX, int32 targetRefIdx, int32 &outX,
												   int32 &outY) const {
						if (!sh->sliceTemporalMvpEnabled)
							return false;
						const NkHevcFrame *const *colList = sh->collocatedFromL0 ? refsL0 : refsL1;
						const int32 colN = sh->collocatedFromL0 ? numRefsL0 : numRefsL1;
						const int32 ci = sh->collocatedRefIdx;
						if (ci < 0 || ci >= colN || !colList || !colList[ci])
							return false;
						const NkHevcFrame *colPic = colList[ci];
						if (colPic->mvColPredFlag.IsEmpty() || colPic->mvColPuWidth != minPuWidth ||
							colPic->mvColPuHeight != minPuHeight)
							return false; // colPic intra (I) ou résolution différente
						const int32 colPoc = colPic->poc;
						ColMv tc;
						bool have = false;
						const int32 xColBr = x0 + nPbW, yColBr = y0 + nPbH;
						if (((yColBr >> ctbLog2) == (y0 >> ctbLog2)) && yColBr < picH && xColBr < picW) {
							const int32 xColPb = (xColBr >> 4) << 4, yColPb = (yColBr >> 4) << 4;
							if (SampleColField(colPic, xColPb, yColPb, tc))
								have = DeriveColMv(tc, colPoc, listX, targetRefIdx, outX, outY);
						}
						if (!have) {
							const int32 xColCtr = x0 + (nPbW >> 1), yColCtr = y0 + (nPbH >> 1);
							const int32 xColPb = (xColCtr >> 4) << 4, yColPb = (yColCtr >> 4) << 4;
							if (SampleColField(colPic, xColPb, yColPb, tc))
								have = DeriveColMv(tc, colPoc, listX, targetRefIdx, outX, outY);
						}
						return have;
					}

					void DeriveAmvpLx(int32 x0, int32 y0, int32 nPbW, int32 nPbH, int32 listX,
									  int32 refIdxLx, int32 (&mvpX)[2], int32 (&mvpY)[2]) {
						const int32 curPoc = frame->poc;
						const int32 targetPoc = RefPocL(listX, refIdxLx);
						const int32 pL0 = listX, pL1 = 1 - listX;

						const MvField a0 = GetCand(x0 - 1, y0 + nPbH, CandBottomLeft(x0, y0, nPbH));
						const MvField a1 = GetCand(x0 - 1, y0 + nPbH - 1, CandLeft(x0));
						const bool isScaledFlagL0 = a0.avail || a1.avail;

						bool availA = false;
						int32 ax = 0, ay = 0;
						if (a0.avail)
							availA = MpMx(a0, pL0, targetPoc, ax, ay) || MpMx(a0, pL1, targetPoc, ax, ay);
						if (!availA && a1.avail)
							availA = MpMx(a1, pL0, targetPoc, ax, ay) || MpMx(a1, pL1, targetPoc, ax, ay);
						if (!availA && a0.avail)
							availA = MpMxLt(a0, pL0, curPoc, targetPoc, ax, ay) ||
									 MpMxLt(a0, pL1, curPoc, targetPoc, ax, ay);
						if (!availA && a1.avail)
							availA = MpMxLt(a1, pL0, curPoc, targetPoc, ax, ay) ||
									 MpMxLt(a1, pL1, curPoc, targetPoc, ax, ay);

						const MvField b0 = GetCand(x0 + nPbW, y0 - 1, CandUpRight(x0, y0, nPbW));
						const MvField b1 = GetCand(x0 + nPbW - 1, y0 - 1, CandUp(y0));
						const MvField b2 = GetCand(x0 - 1, y0 - 1, CandUpLeft(x0, y0));
						bool availB = false;
						int32 bx = 0, by = 0;
						if (b0.avail)
							availB = MpMx(b0, pL0, targetPoc, bx, by) || MpMx(b0, pL1, targetPoc, bx, by);
						if (!availB && b1.avail)
							availB = MpMx(b1, pL0, targetPoc, bx, by) || MpMx(b1, pL1, targetPoc, bx, by);
						if (!availB && b2.avail)
							availB = MpMx(b2, pL0, targetPoc, bx, by) || MpMx(b2, pL1, targetPoc, bx, by);

						if (!isScaledFlagL0) {
							if (availB) {
								availA = true;
								ax = bx;
								ay = by;
							}
							availB = false;
							if (b0.avail)
								availB = MpMxLt(b0, pL0, curPoc, targetPoc, bx, by) ||
										 MpMxLt(b0, pL1, curPoc, targetPoc, bx, by);
							if (!availB && b1.avail)
								availB = MpMxLt(b1, pL0, curPoc, targetPoc, bx, by) ||
										 MpMxLt(b1, pL1, curPoc, targetPoc, bx, by);
							if (!availB && b2.avail)
								availB = MpMxLt(b2, pL0, curPoc, targetPoc, bx, by) ||
										 MpMxLt(b2, pL1, curPoc, targetPoc, bx, by);
						}

						int32 n = 0;
						if (availA) {
							mvpX[n] = ax;
							mvpY[n] = ay;
							++n;
						}
						if (availB && !(availA && ax == bx && ay == by)) {
							mvpX[n] = bx;
							mvpY[n] = by;
							++n;
						}
						if (n < 2) {
							int32 tx = 0, ty = 0;
							if (DeriveTemporalColocatedMv(x0, y0, nPbW, nPbH, listX, refIdxLx, tx, ty)) {
								mvpX[n] = tx;
								mvpY[n] = ty;
								++n;
							}
						}
						while (n < 2) {
							mvpX[n] = 0;
							mvpY[n] = 0;
							++n;
						}
					}

					// ---- Finalisation d'un échantillon MC UNI (§8.5.3.3.3/8.5.3.3.4.2) —
					// `raw` = pixel source direct (isDirect) OU sortie du dernier filtre
					// (H, V, ou V-du-HV déjà >>6) ; pondération explicite si présente
					// (WeightedActive : P->weightedPred, B->weightedBipred), avec les
					// poids/offsets de la LISTE et du refIdx effectifs.
					int32 FinalizeSample(int32 raw, bool isDirect, bool isLuma, int32 chromaIdx,
										 int32 listX, int32 refIdx) {
						const int32 mx = isLuma ? maxVal : (1 << sps->bitDepthChroma) - 1;
						if (!WeightedActive()) {
							if (isDirect)
								return raw;
							const int32 shift = 14 - bitDepth;
							const int32 offset = shift > 0 ? (1 << (shift - 1)) : 0;
							return Clip3i(0, mx, (raw + offset) >> shift);
						}
						const int32 denom =
							isLuma ? sh->lumaLog2WeightDenom : sh->chromaLog2WeightDenom;
						int32 wx, ox;
						if (listX == 0) {
							wx = isLuma ? sh->lumaWeightL0[refIdx] : sh->chromaWeightL0[refIdx][chromaIdx];
							ox = isLuma ? sh->lumaOffsetL0[refIdx] : sh->chromaOffsetL0[refIdx][chromaIdx];
						} else {
							wx = isLuma ? sh->lumaWeightL1[refIdx] : sh->chromaWeightL1[refIdx][chromaIdx];
							ox = isLuma ? sh->lumaOffsetL1[refIdx] : sh->chromaOffsetL1[refIdx][chromaIdx];
						}
						const int32 shift = denom + 14 - bitDepth;
						const int32 offset = shift > 0 ? (1 << (shift - 1)) : 0;
						const int32 v = isDirect ? (raw << (14 - bitDepth)) : raw;
						return Clip3i(0, mx, ((v * wx + offset) >> shift) + ox);
					}

					// Marque la zone PU comme reconstruite (§8.4.4.2.1 : une slice P/B mélange
					// CU intra et inter ; sans ce marquage, un voisin inter serait vu comme
					// absent par une CU intra adjacente -> substitution/padding erronés).
					void MarkRecon(int32 x0, int32 y0, int32 w, int32 h) {
						for (int32 j = 0; j < h; j += 4)
							for (int32 i = 0; i < w; i += 4) {
								const int32 px = (x0 + i) >> 2, py = (y0 + j) >> 2;
								if (px < minPuWidth && py < minPuHeight)
									lumaRecon[(usize)(py * minPuWidth + px)] = 1;
							}
						const int32 cw4 = ((picW >> 1) + 3) >> 2, ch4 = ((picH >> 1) + 3) >> 2;
						const int32 cx0b = x0 >> 1, cy0b = y0 >> 1, cwb = w >> 1, chb = h >> 1;
						for (int32 j = 0; j < chb; j += 4)
							for (int32 i = 0; i < cwb; i += 4) {
								const int32 px = (cx0b + i) >> 2, py = (cy0b + j) >> 2;
								if (px < cw4 && py < ch4)
									chromaRecon[(usize)(py * cw4 + px)] = 1;
							}
					}

					// ---- Interpolation MC -> intermédiaire 14 bits (§8.5.4.2, DSP put_hevc_*
					// SANS finalisation) : (0,0)->src<<6 ; H->Σhf·src ; V->Σvf·src ;
					// HV->(Σvf·tmp)>>6 (tmp = passe horizontale). Luma qpel 8 taps (mv&3,
					// mv>>2, centre -3) ; chroma epel 4 taps (mv&7, mv>>3, centre -1, 4:2:0).
					// Écrit `out[j*outStride+i]` (int16) pour j∈[0,h), i∈[0,w). Générique
					// bit-depth (§8.5.4.2) : pel <<(14-bd) ; H/V et 1re passe HV >>(bd-8)=shift1
					// (bd=8 -> shifts nuls). shift1=Min(4,bd-8) mais bd∈{8,10,12} -> bd-8.
					void ComputeInterp(const NkVector<nk_uint16> &refPlane, int32 planeW, int32 planeH,
									   int32 baseX, int32 baseY, int32 mvx, int32 mvy, int32 w, int32 h,
									   bool isLuma, int16 *out, int32 outStride) const {
						const int32 mask = isLuma ? 3 : 7;
						const int32 ish = isLuma ? 2 : 3;
						const int32 taps = isLuma ? 8 : 4;
						const int32 ctr = isLuma ? 3 : 1;
						const int32 shift1 = bitDepth - 8;
						const int32 shiftP = 14 - bitDepth;
						const int32 xFrac = mvx & mask, yFrac = mvy & mask;
						const int32 xInt = baseX + (mvx >> ish), yInt = baseY + (mvy >> ish);
						const int8 *hf = isLuma ? kHevcQpelFilters[xFrac] : kHevcEpelFilters[xFrac];
						const int8 *vf = isLuma ? kHevcQpelFilters[yFrac] : kHevcEpelFilters[yFrac];
						const nk_uint16 *srcBase = refPlane.Data();
						// Corps unique (arithmetique BIT-EXACTE) ; `S` fournit l'echantillon.
						// Interieur -> indexation directe sans clamp (vectorisable) ; bord -> clamp.
						auto body = [&](auto S) {
							if (xFrac == 0 && yFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i)
										out[j * outStride + i] = (int16)(S(xInt + i, yInt + j) << shiftP);
							} else if (yFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < taps; ++k)
											sum += hf[k] * S(xInt + i + k - ctr, yInt + j);
										out[j * outStride + i] = (int16)(sum >> shift1);
									}
							} else if (xFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < taps; ++k)
											sum += vf[k] * S(xInt + i, yInt + j + k - ctr);
										out[j * outStride + i] = (int16)(sum >> shift1);
									}
							} else {
								int32 tmp[(64 + 7) * 64];
								const int32 ts = 64;
								for (int32 j = 0; j < h + taps - 1; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < taps; ++k)
											sum += hf[k] * S(xInt + i + k - ctr, yInt + j - ctr);
										tmp[j * ts + i] = sum >> shift1;
									}
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < taps; ++k)
											sum += vf[k] * tmp[(j + k) * ts + i];
										out[j * outStride + i] = (int16)(sum >> 6);
									}
							}
						};
						const bool interior = xInt - ctr >= 0 && yInt - ctr >= 0 &&
											   xInt + w + taps - 1 - ctr <= planeW &&
											   yInt + h + taps - 1 - ctr <= planeH;
						if (interior)
							body([srcBase, planeW](int32 xx, int32 yy) -> int32 {
								return srcBase[(usize)yy * planeW + xx];
							});
						else
							body([&](int32 xx, int32 yy) -> int32 {
								const int32 cx = Clip3i(0, planeW - 1, xx);
								const int32 cy = Clip3i(0, planeH - 1, yy);
								return srcBase[(usize)(cy * planeW + cx)];
							});
					}

					// ---- Compensation de mouvement UNI (§8.5.3.3.3, luma qpel 8 taps + chroma
					// epel 4 taps 4:2:0). `listX`/`refIdx` = liste et référence effectives
					// (L0 ou L1). Échantillonnage hors-image = étendu par bord (clamp,
					// équivalent emulated_edge_mc). Pondération explicite -> FinalizeSample.
					void ApplyMotionCompensation(int32 x0, int32 y0, int32 w, int32 h, int32 mvx,
												  int32 mvy, int32 listX, int32 refIdx) {
						const NkHevcFrame *const *refs = (listX == 0) ? refsL0 : refsL1;
						const int32 nRefs = (listX == 0) ? numRefsL0 : numRefsL1;
						if (refIdx < 0 || refIdx >= nRefs || !refs || !refs[refIdx])
							return;
						const NkHevcFrame *ref0 = refs[refIdx];
						const int32 shift1 = bitDepth - 8; // 1re/seule passe filtre (§8.5.4.2)
						// ---- Luma ----
						{
							const int32 xFrac = mvx & 3, yFrac = mvy & 3;
							const int32 xInt = x0 + (mvx >> 2), yInt = y0 + (mvy >> 2);
							const int8 *hf = kHevcQpelFilters[xFrac];
							const int8 *vf = kHevcQpelFilters[yFrac];
							nk_uint16 *dst = frame->y.Data();
							const int32 stride = frame->lumaW;
							const nk_uint16 *srcBase = ref0->y.Data();
							const int32 rw = ref0->lumaW, rh = ref0->lumaH;
							// Corps unique (arithmetique BIT-EXACTE preservee) ; `fetch` fournit
							// l'echantillon source. Deux instanciations : chemin interieur SANS
							// clamp (indexation directe -> vectorisable) et bord AVEC clamp.
							auto doLuma = [&](auto fetch) {
								if (xFrac == 0 && yFrac == 0) {
									for (int32 j = 0; j < h; ++j)
										for (int32 i = 0; i < w; ++i)
											dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
												fetch(xInt + i, yInt + j), true, true, 0, listX, refIdx);
								} else if (yFrac == 0) {
									for (int32 j = 0; j < h; ++j)
										for (int32 i = 0; i < w; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 8; ++k)
												sum += hf[k] * fetch(xInt + i + k - 3, yInt + j);
											dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
												sum >> shift1, false, true, 0, listX, refIdx);
										}
								} else if (xFrac == 0) {
									for (int32 j = 0; j < h; ++j)
										for (int32 i = 0; i < w; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 8; ++k)
												sum += vf[k] * fetch(xInt + i, yInt + j + k - 3);
											dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
												sum >> shift1, false, true, 0, listX, refIdx);
										}
								} else {
									int32 tmp[(64 + 7) * 64];
									for (int32 j = 0; j < h + 7; ++j)
										for (int32 i = 0; i < w; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 8; ++k)
												sum += hf[k] * fetch(xInt + i + k - 3, yInt + j - 3);
											tmp[j * 64 + i] = sum >> shift1;
										}
									for (int32 j = 0; j < h; ++j)
										for (int32 i = 0; i < w; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 8; ++k)
												sum += vf[k] * tmp[(j + k) * 64 + i];
											dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
												sum >> 6, false, true, 0, listX, refIdx);
										}
								}
							};
							// Fenetre source luma requise (conservatrice, couvre les 4 sous-cas) :
							// x in [xInt-3, xInt+w+3], y in [yInt-3, yInt+h+3].
							const bool interior =
								xInt >= 3 && yInt >= 3 && xInt + w + 4 <= rw && yInt + h + 4 <= rh;
							if (interior)
								doLuma([srcBase, rw](int32 xx, int32 yy) -> int32 {
									return srcBase[(usize)yy * rw + xx];
								});
							else
								doLuma([&](int32 xx, int32 yy) -> int32 {
									const int32 cx = Clip3i(0, rw - 1, xx);
									const int32 cy = Clip3i(0, rh - 1, yy);
									return srcBase[(usize)(cy * rw + cx)];
								});
						}
						// ---- Chroma (Cb=1, Cr=2) ----
						for (int32 cIdx = 1; cIdx <= 2; ++cIdx) {
							const int32 cx0 = x0 >> 1, cy0 = y0 >> 1, cw = w >> 1, ch = h >> 1;
							const int32 xFrac = mvx & 7, yFrac = mvy & 7;
							const int32 xInt = cx0 + (mvx >> 3), yInt = cy0 + (mvy >> 3);
							const int8 *hf = kHevcEpelFilters[xFrac];
							const int8 *vf = kHevcEpelFilters[yFrac];
							const NkVector<nk_uint16> &refPlane = (cIdx == 1) ? ref0->cb : ref0->cr;
							nk_uint16 *dst = (cIdx == 1) ? frame->cb.Data() : frame->cr.Data();
							const int32 stride = frame->chromaW;
							const int32 ci = cIdx - 1;
							const nk_uint16 *srcBase = refPlane.Data();
							const int32 rw = ref0->chromaW, rh = ref0->chromaH;
							auto doChroma = [&](auto fetch) {
								if (xFrac == 0 && yFrac == 0) {
									for (int32 j = 0; j < ch; ++j)
										for (int32 i = 0; i < cw; ++i)
											dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
												fetch(xInt + i, yInt + j), true, false, ci, listX, refIdx);
								} else if (yFrac == 0) {
									for (int32 j = 0; j < ch; ++j)
										for (int32 i = 0; i < cw; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 4; ++k)
												sum += hf[k] * fetch(xInt + i + k - 1, yInt + j);
											dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
												sum >> shift1, false, false, ci, listX, refIdx);
										}
								} else if (xFrac == 0) {
									for (int32 j = 0; j < ch; ++j)
										for (int32 i = 0; i < cw; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 4; ++k)
												sum += vf[k] * fetch(xInt + i, yInt + j + k - 1);
											dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
												sum >> shift1, false, false, ci, listX, refIdx);
										}
								} else {
									int32 tmp[(32 + 3) * 32];
									for (int32 j = 0; j < ch + 3; ++j)
										for (int32 i = 0; i < cw; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 4; ++k)
												sum += hf[k] * fetch(xInt + i + k - 1, yInt + j - 1);
											tmp[j * 32 + i] = sum >> shift1;
										}
									for (int32 j = 0; j < ch; ++j)
										for (int32 i = 0; i < cw; ++i) {
											int32 sum = 0;
											for (int32 k = 0; k < 4; ++k)
												sum += vf[k] * tmp[(j + k) * 32 + i];
											dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
												sum >> 6, false, false, ci, listX, refIdx);
										}
								}
							};
							// Fenetre chroma requise : x in [xInt-1, xInt+cw+1], y in [yInt-1, yInt+ch+1].
							const bool interior =
								xInt >= 1 && yInt >= 1 && xInt + cw + 2 <= rw && yInt + ch + 2 <= rh;
							if (interior)
								doChroma([srcBase, rw](int32 xx, int32 yy) -> int32 {
									return srcBase[(usize)yy * rw + xx];
								});
							else
								doChroma([&](int32 xx, int32 yy) -> int32 {
									const int32 cx = Clip3i(0, rw - 1, xx);
									const int32 cy = Clip3i(0, rh - 1, yy);
									return srcBase[(usize)(cy * rw + cx)];
								});
						}
						MarkRecon(x0, y0, w, h);
					}

					// ---- Compensation de mouvement BI (§8.5.3.3.4) : deux intermédiaires
					// L0/L1 combinés. Non pondéré : (predL0+predL1+64)>>7. Pondéré
					// (weighted_bipred) : (predL0·w0+predL1·w1+((o0+o1+1)<<log2Wd))>>(log2Wd+1),
					// log2Wd = denom+14-bd (8 bits : o<<(bd-8) = o).
					void ApplyMotionCompensationBi(int32 x0, int32 y0, int32 w, int32 h, int32 refL0,
												   int32 mvx0, int32 mvy0, int32 refL1, int32 mvx1,
												   int32 mvy1) {
						if (refL0 < 0 || refL0 >= numRefsL0 || !refsL0 || !refsL0[refL0])
							return;
						if (refL1 < 0 || refL1 >= numRefsL1 || !refsL1 || !refsL1[refL1])
							return;
						const NkHevcFrame *r0 = refsL0[refL0];
						const NkHevcFrame *r1 = refsL1[refL1];
						const bool weighted = WeightedActive();
						// ---- Luma ----
						{
							int16 iL0[64 * 64], iL1[64 * 64];
							ComputeInterp(r0->y, r0->lumaW, r0->lumaH, x0, y0, mvx0, mvy0, w, h, true, iL0, w);
							ComputeInterp(r1->y, r1->lumaW, r1->lumaH, x0, y0, mvx1, mvy1, w, h, true, iL1, w);
							nk_uint16 *dst = frame->y.Data();
							const int32 stride = frame->lumaW;
							if (!weighted) {
								const int32 shift2 = 15 - bitDepth;
								const int32 off2 = 1 << (shift2 - 1);
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i)
										dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)Clip3i(
											0, maxVal, (iL0[j * w + i] + iL1[j * w + i] + off2) >> shift2);
							} else {
								const int32 denom = sh->lumaLog2WeightDenom;
								const int32 log2Wd = denom + 14 - bitDepth;
								const int32 w0 = sh->lumaWeightL0[refL0], w1 = sh->lumaWeightL1[refL1];
								const int32 o0 = sh->lumaOffsetL0[refL0], o1 = sh->lumaOffsetL1[refL1];
								const int32 add = (o0 + o1 + 1) << log2Wd;
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i)
										dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)Clip3i(
											0, maxVal,
											(iL0[j * w + i] * w0 + iL1[j * w + i] * w1 + add) >> (log2Wd + 1));
							}
						}
						// ---- Chroma (Cb=1, Cr=2) ----
						const int32 maxC = (1 << sps->bitDepthChroma) - 1;
						const int32 cx0 = x0 >> 1, cy0 = y0 >> 1, cw = w >> 1, ch = h >> 1;
						for (int32 cIdx = 1; cIdx <= 2; ++cIdx) {
							const int32 ci = cIdx - 1;
							const NkVector<nk_uint16> &p0 = (cIdx == 1) ? r0->cb : r0->cr;
							const NkVector<nk_uint16> &p1 = (cIdx == 1) ? r1->cb : r1->cr;
							int16 iC0[32 * 32], iC1[32 * 32];
							ComputeInterp(p0, r0->chromaW, r0->chromaH, cx0, cy0, mvx0, mvy0, cw, ch, false,
										  iC0, cw);
							ComputeInterp(p1, r1->chromaW, r1->chromaH, cx0, cy0, mvx1, mvy1, cw, ch, false,
										  iC1, cw);
							nk_uint16 *dst = (cIdx == 1) ? frame->cb.Data() : frame->cr.Data();
							const int32 stride = frame->chromaW;
							if (!weighted) {
								const int32 shift2 = 15 - bitDepth;
								const int32 off2 = 1 << (shift2 - 1);
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i)
										dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)Clip3i(
											0, maxC, (iC0[j * cw + i] + iC1[j * cw + i] + off2) >> shift2);
							} else {
								const int32 denom = sh->chromaLog2WeightDenom;
								const int32 log2Wd = denom + 14 - bitDepth;
								const int32 w0 = sh->chromaWeightL0[refL0][ci];
								const int32 w1 = sh->chromaWeightL1[refL1][ci];
								const int32 o0 = sh->chromaOffsetL0[refL0][ci];
								const int32 o1 = sh->chromaOffsetL1[refL1][ci];
								const int32 add = (o0 + o1 + 1) << log2Wd;
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i)
										dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)Clip3i(
											0, maxC,
											(iC0[j * cw + i] * w0 + iC1[j * cw + i] * w1 + add) >> (log2Wd + 1));
							}
						}
						MarkRecon(x0, y0, w, h);
					}

					// Aiguillage MC selon predFlag (§8.5.3.3.2) : L0 -> uni L0, L1 -> uni L1,
					// BI -> bi.
					void ApplyMc(int32 x0, int32 y0, int32 w, int32 h, const MvField &f) {
						if (f.predFlag == 1)
							ApplyMotionCompensation(x0, y0, w, h, f.mvx[0], f.mvy[0], 0, f.refIdx[0]);
						else if (f.predFlag == 2)
							ApplyMotionCompensation(x0, y0, w, h, f.mvx[1], f.mvy[1], 1, f.refIdx[1]);
						else if (f.predFlag == 3)
							ApplyMotionCompensationBi(x0, y0, w, h, f.refIdx[0], f.mvx[0], f.mvy[0],
													  f.refIdx[1], f.mvx[1], f.mvy[1]);
					}

					// prediction_unit() (§7.3.8.6/7) — parse CABAC identique brique 8 +
					// (brique 10, si frame != nullptr donc P mono-référence — jamais vrai
					// pour B, cf. garde-fou appelant) résolution du MV (fusion ou
					// AMVP+mvd) + application immédiate de la MC sur le rectangle PU
					// (le résidu, ajouté ensuite par transform_tree, vient PAR-DESSUS —
					// même mécanisme que PredictIntra+ReconstructResidual). Retourne
					// `merge_flag` (ou true si skip, merge inféré — §7.3.8.6).
					bool ParsePredictionUnit(int32 x0, int32 y0, int32 pw, int32 ph, int32 depth,
											  int32 partIdx, bool isSkip) {
						bool mergeFlag = isSkip;
						if (!isSkip)
							mergeFlag = Bin(kHevcCtxMergeFlag) != 0;
						MvField mvf;
						bool haveMv = false;
						if (mergeFlag) {
							int32 idx = 0;
							if (sh->maxNumMergeCand > 1) {
								idx = (int32)Bin(kHevcCtxMergeIdx);
								if (idx != 0)
									while (idx < sh->maxNumMergeCand - 1 && Bypass())
										++idx;
							}
							if (frame) {
								MvField list[5];
								int32 numCand;
								DeriveMergeCandidates(x0, y0, pw, ph, partIdx, curPartMode, list, numCand);
								const int32 useIdx = Min32(idx, numCand - 1);
								mvf = list[useIdx];
								// Règle 8x4/4x8 (§8.5.3.2.2) : bi interdit -> forcé L0.
								if (mvf.predFlag == 3 && (pw + ph) == 12)
									mvf.predFlag = 1;
								haveMv = true;
							}
							if (frame && haveMv) {
								StoreMv(x0, y0, pw, ph, mvf);
								ApplyMc(x0, y0, pw, ph, mvf);
							}
							return true;
						}
						int32 interPredIdc = 0; // 0=L0, 1=L1, 2=BI (NkHevcSliceType kHevcSliceB)
						if (sh->sliceType == kHevcSliceB) {
							if (pw + ph == 12)
								interPredIdc = (int32)Bin(kHevcCtxInterPredIdc + 4);
							else if (Bin(kHevcCtxInterPredIdc + depth))
								interPredIdc = 2;
							else
								interPredIdc = (int32)Bin(kHevcCtxInterPredIdc + 4);
						}
						int32 refIdxL0Dec = 0, refIdxL1Dec = 0;
						int32 mvdX0 = 0, mvdY0 = 0, mvdX1 = 0, mvdY1 = 0;
						int32 mvpFlag0 = 0, mvpFlag1 = 0;
						if (interPredIdc != 1) { // L0 présent
							refIdxL0Dec = DecodeRefIdx(sh->numRefIdxL0Active);
							DecodeMvd(mvdX0, mvdY0);
							mvpFlag0 = (int32)Bin(kHevcCtxMvpLxFlag);
						}
						if (interPredIdc != 0) { // L1 présent
							refIdxL1Dec = DecodeRefIdx(sh->numRefIdxL1Active);
							if (!(sh->mvdL1Zero && interPredIdc == 2))
								DecodeMvd(mvdX1, mvdY1); // sinon mvd L1 = (0,0)
							mvpFlag1 = (int32)Bin(kHevcCtxMvpLxFlag);
						}
						if (frame) {
							mvf.predFlag =
								(uint8)((interPredIdc == 0) ? 1 : (interPredIdc == 1) ? 2 : 3);
							if (interPredIdc != 1) { // L0
								int32 mvpX[2], mvpY[2];
								DeriveAmvpLx(x0, y0, pw, ph, 0, refIdxL0Dec, mvpX, mvpY);
								mvf.mvx[0] = mvpX[mvpFlag0] + mvdX0;
								mvf.mvy[0] = mvpY[mvpFlag0] + mvdY0;
								mvf.refIdx[0] = refIdxL0Dec;
							}
							if (interPredIdc != 0) { // L1
								int32 mvpX[2], mvpY[2];
								DeriveAmvpLx(x0, y0, pw, ph, 1, refIdxL1Dec, mvpX, mvpY);
								mvf.mvx[1] = mvpX[mvpFlag1] + mvdX1;
								mvf.mvy[1] = mvpY[mvpFlag1] + mvdY1;
								mvf.refIdx[1] = refIdxL1Dec;
							}
							mvf.avail = true;
							haveMv = true;
						}
						if (frame && haveMv) {
							StoreMv(x0, y0, pw, ph, mvf);
							ApplyMc(x0, y0, pw, ph, mvf);
						}
						return false;
					}

					// ---- coding_unit (§7.3.8.5) — commun I/P/B (brique 8 : parse
					// structurel des CU inter — skip/merge/AMVP/mvd — SANS dérivation de
					// MV ni reconstruction ; validé comme la brique 5 CTU intra, via la
					// synchronisation CABAC). ---------------------------------------------
					void ParseCodingUnit(int32 x0, int32 y0, int32 log2CbSize, int32 depth) {
						if (!okFlag)
							return;
						++stats->cuCount;
						curCuX = x0;
						curCuY = y0;
						curCuLog2 = log2CbSize;
						cuTransquantBypass = false;
						if (pps->transquantBypassEnabled)
							cuTransquantBypass = Bin(kHevcCtxCuTransquantBypass) != 0;

						const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
						const int32 nMinCu = (1 << log2CbSize) >> minCbLog2;
						bool skipFlag = false;
						if (sh->sliceType != kHevcSliceI) {
							int32 inc = 0;
							if (x0 > 0)
								inc += skipFlagMap[(usize)(yCb * minCbWidth + xCb - 1)];
							if (y0 > 0)
								inc += skipFlagMap[(usize)((yCb - 1) * minCbWidth + xCb)];
							skipFlag = Bin(kHevcCtxSkipFlag + inc) != 0;
						}
						for (int32 j = 0; j < nMinCu; ++j)
							for (int32 i = 0; i < nMinCu; ++i) {
								const int32 px = xCb + i, py = yCb + j;
								if (px < minCbWidth && py < minCbHeight)
									skipFlagMap[(usize)(py * minCbWidth + px)] = skipFlag ? 1 : 0;
							}

						intraSplit = false;
						cuIsIntra = true;
						int32 partMode = kPart2Nx2N;
						bool rqtRootCbf = true;

						if (skipFlag) {
							cuIsIntra = false;
							curPartMode = partMode; // kPart2Nx2N — cf. note ci-dessous
							const int32 cb = 1 << log2CbSize;
							ParsePredictionUnit(x0, y0, cb, cb, depth, 0, true);
							rqtRootCbf = false; // un CU skip n'a JAMAIS de résidu
						} else {
							if (sh->sliceType != kHevcSliceI)
								cuIsIntra = Bin(kHevcCtxPredModeFlag) != 0; // 1 = MODE_INTRA
							if (!cuIsIntra || log2CbSize == minCbLog2)
								partMode = DecodePartMode(log2CbSize);
							intraSplit = (partMode == kPartNxN && cuIsIntra);
							// ⭐ curPartMode DOIT être à jour AVANT la boucle des PU
							// ci-dessous (DeriveMergeCandidates, via ParsePredictionUnit,
							// lit ce membre pour les exclusions A1/B1 §8.5.3.2.3) — la
							// version précédente l'assignait APRÈS la boucle, lisant donc
							// le partMode du CU PRÉCÉDENT pour toute CU non-2Nx2N.
							curPartMode = partMode;

							// pcm_flag (§7.3.8.5) : CU intra 2Nx2N dans les bornes de taille PCM.
							bool pcmFlag = false;
							if (cuIsIntra && partMode == kPart2Nx2N && sps->pcmEnabled &&
								log2CbSize >= sps->log2MinPcmCbSize &&
								log2CbSize <= sps->log2MaxPcmCbSize) {
								pcmFlag = Terminate() != 0; // décodé via le bin de terminaison
							}
							if (pcmFlag) {
								DecodePcm(x0, y0, log2CbSize);
								rqtRootCbf = false; // aucun transform_tree pour un bloc I_PCM
							} else if (cuIsIntra) {
								const int32 nParts = intraSplit ? 4 : 1;
								const int32 pbSize = (1 << log2CbSize) >> (intraSplit ? 1 : 0);
								bool prevFlag[4] = {false, false, false, false};
								int32 mpmIdx[4] = {0, 0, 0, 0};
								int32 rem[4] = {0, 0, 0, 0};
								for (int32 p = 0; p < nParts; ++p)
									prevFlag[p] = Bin(kHevcCtxPrevIntraLumaPred) != 0;
								for (int32 p = 0; p < nParts; ++p) {
									if (prevFlag[p]) {
										int32 v = 0;
										while (v < 2 && Bypass())
											++v;
										mpmIdx[p] = v;
									} else {
										rem[p] = (int32)BypassBits(5);
									}
								}
								for (int32 p = 0; p < nParts; ++p) {
									const int32 xPb = x0 + (p & 1) * pbSize;
									const int32 yPb = y0 + (p >> 1) * pbSize;
									const int32 mode = DeriveIntraMode(xPb, yPb, prevFlag[p], mpmIdx[p], rem[p]);
									curIntraModeY[p] = mode;
									StoreModes(xPb, yPb, pbSize, mode);
								}
								// intra_chroma_pred_mode (4:2:0 : un seul pour le CU).
								if (Bin(kHevcCtxIntraChromaPredMode) == 0) {
									curIntraModeC = curIntraModeY[0]; // DM
								} else {
									static const int32 kChromaModes[4] = {0, 26, 10, 1};
									const int32 idx = (int32)BypassBits(2);
									curIntraModeC = kChromaModes[idx];
									if (curIntraModeC == curIntraModeY[0])
										curIntraModeC = 34;
								}
							} else {
								// Géométrie des PU selon part_mode (§7.4.9.5) — seule la
								// somme largeur+hauteur importe (contexte inter_pred_idc),
								// et le nombre de PU (1 pour 2Nx2N, sinon 2 ou 4 pour NxN).
								struct PuRect {
										int32 x, y, w, h;
								};
								PuRect pu[4];
								int32 nPu = 0;
								const int32 cb = 1 << log2CbSize;
								switch (partMode) {
									case kPart2Nx2N:
										nPu = 1;
										pu[0] = {0, 0, cb, cb};
										break;
									case kPart2NxN:
										nPu = 2;
										pu[0] = {0, 0, cb, cb / 2};
										pu[1] = {0, cb / 2, cb, cb / 2};
										break;
									case kPartNx2N:
										nPu = 2;
										pu[0] = {0, 0, cb / 2, cb};
										pu[1] = {cb / 2, 0, cb / 2, cb};
										break;
									case kPart2NxnU:
										nPu = 2;
										pu[0] = {0, 0, cb, cb / 4};
										pu[1] = {0, cb / 4, cb, cb * 3 / 4};
										break;
									case kPart2NxnD:
										nPu = 2;
										pu[0] = {0, 0, cb, cb * 3 / 4};
										pu[1] = {0, cb * 3 / 4, cb, cb / 4};
										break;
									case kPartnLx2N:
										nPu = 2;
										pu[0] = {0, 0, cb / 4, cb};
										pu[1] = {cb / 4, 0, cb * 3 / 4, cb};
										break;
									case kPartnRx2N:
										nPu = 2;
										pu[0] = {0, 0, cb * 3 / 4, cb};
										pu[1] = {cb * 3 / 4, 0, cb / 4, cb};
										break;
									case kPartNxN:
										nPu = 4;
										pu[0] = {0, 0, cb / 2, cb / 2};
										pu[1] = {cb / 2, 0, cb / 2, cb / 2};
										pu[2] = {0, cb / 2, cb / 2, cb / 2};
										pu[3] = {cb / 2, cb / 2, cb / 2, cb / 2};
										break;
								}
								bool firstMerge = false;
								for (int32 i = 0; i < nPu; ++i) {
									const bool m = ParsePredictionUnit(x0 + pu[i].x, y0 + pu[i].y,
																	   pu[i].w, pu[i].h, depth, i,
																	   false);
									if (i == 0)
										firstMerge = m;
								}
								if (!(partMode == kPart2Nx2N && firstMerge))
									rqtRootCbf = Bin(kHevcCtxNoResidualData) != 0;
								// (sinon rqtRootCbf reste à sa valeur par défaut TRUE, inférée
								// sans lecture de bit — §7.4.9.8.)
							}
						}

						if (rqtRootCbf) {
							ParseTransformTree(x0, y0, x0, y0, log2CbSize, 0, 0, 0, false, false);
						} else if (frame && !sh->deblockingFilterDisabled) {
							// CU skip / CU inter sans résidu (pas de transform_tree, donc pas
							// de feuille) : la BS des arêtes du CU se dérive ici (§8.7.2.4,
							// cbf_luma implicitement 0 pour tout le CU).
							DeriveDeblockBs(x0, y0, log2CbSize);
						}

						// QP final du CU -> carte de voisinage + qPY_PREV du prochain groupe
						// (calculé même pour un CU skip/inter sans résidu, §8.6.1).
						if (frame) {
							const int32 qpY = CurrentQpY();
							lastCuQpY = qpY;
							for (int32 j = 0; j < nMinCu; ++j)
								for (int32 i = 0; i < nMinCu; ++i) {
									const int32 px = xCb + i, py = yCb + j;
									if (px < minCbWidth && py < minCbHeight)
										qpMap[(usize)(py * minCbWidth + px)] = (int8)qpY;
								}
						}
					}

					// ---- coding_quadtree (§7.3.8.4) -----------------------------------
					void ParseCodingQuadtree(int32 x0, int32 y0, int32 log2CbSize, int32 depth) {
						if (!okFlag)
							return;
						const int32 size = 1 << log2CbSize;
						bool split;
						if (x0 + size <= picW && y0 + size <= picH && log2CbSize > minCbLog2) {
							int32 inc = 0;
							const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
							if (x0 > 0 && ctDepth[(usize)(yCb * minCbWidth + xCb - 1)] > depth)
								++inc;
							if (y0 > 0 && ctDepth[(usize)((yCb - 1) * minCbWidth + xCb)] > depth)
								++inc;
							split = Bin(kHevcCtxSplitCuFlag + inc) != 0;
						} else {
							split = log2CbSize > minCbLog2; // hors image : split forcé si possible
						}
						if (pps->cuQpDeltaEnabled && log2CbSize >= log2MinCuQpDeltaSize)
							StartQuantGroup(x0, y0); // nouveau groupe de quantification
						if (split) {
							const int32 h = size >> 1;
							if (x0 < picW && y0 < picH)
								ParseCodingQuadtree(x0, y0, log2CbSize - 1, depth + 1);
							if (x0 + h < picW && y0 < picH)
								ParseCodingQuadtree(x0 + h, y0, log2CbSize - 1, depth + 1);
							if (x0 < picW && y0 + h < picH)
								ParseCodingQuadtree(x0, y0 + h, log2CbSize - 1, depth + 1);
							if (x0 + h < picW && y0 + h < picH)
								ParseCodingQuadtree(x0 + h, y0 + h, log2CbSize - 1, depth + 1);
							return;
						}
						// Feuille : mémorise la profondeur pour le contexte des voisins.
						const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
						const int32 nMin = size >> minCbLog2;
						for (int32 j = 0; j < nMin; ++j)
							for (int32 i = 0; i < nMin; ++i) {
								const int32 px = xCb + i, py = yCb + j;
								if (px < minCbWidth && py < minCbHeight)
									ctDepth[(usize)(py * minCbWidth + px)] = (uint8)depth;
							}
						ParseCodingUnit(x0, y0, log2CbSize, depth);
					}
			};

			// Boucle commune slice_segment_data() (parse seul OU parse+reconstruction).
			bool RunSliceIntra(const uint8 *nal, usize size, const NkHevcSps &sps, const NkHevcPps &pps,
							   const NkHevcSliceHeader &sh, NkHevcFrame *frame,
							   const NkHevcFrame *const *refsL0, int32 numRefsL0,
							   const NkHevcFrame *const *refsL1, int32 numRefsL1,
							   NkHevcSliceDataStats &out) {
				out = NkHevcSliceDataStats{};
				if (!nal || size < 4 || !sps.valid || !pps.valid || !sh.valid)
					return false;
				if (sh.dependentSliceSegment || !sh.firstSliceSegmentInPic)
					return false; // slices multiples : brique suivante
				if (sh.sliceType != kHevcSliceI && frame) {
					// Reconstruction inter P/B : MC + MV spatiaux merge/AMVP (mise à l'échelle
					// réelle) + candidat temporel bi-liste (§8.5.3.2.8/9) + bi-prédiction (B).
					if (sh.sliceType != kHevcSliceP && sh.sliceType != kHevcSliceB)
						return false;
					if (!refsL0 || numRefsL0 < sh.numRefIdxL0Active)
						return false;
					if (sh.sliceType == kHevcSliceB && (!refsL1 || numRefsL1 < sh.numRefIdxL1Active))
						return false;
					if (pps.log2ParallelMergeLevel > 2)
						return false; // règle CU 8x8 forcé-2Nx2N (§7.3.8.6) non implémentée
				}
				// PCM (I_PCM) : le chemin de décodage EST implémenté (DecodePcm + pcm_flag,
				// calqué sur le I_PCM H.264 validé bit-exact) mais reste GATÉ ici car
				// INVÉRIFIABLE : aucun encodeur disponible (x265/libx265) n'émet de bloc
				// I_PCM, donc impossible de prouver le bit-exact vs ffmpeg. Retirer ce refus
				// dès qu'un flux de conformance I_PCM est disponible.
				if (pps.tilesEnabled || sps.pcmEnabled || sps.chromaFormatIdc != 1 ||
					sps.separateColourPlane)
					return false;

				// RBSP dé-émulé + positions des octets retirés (pour convertir les entry
				// points, exprimés en octets du flux ÉMULÉ, §7.4.7.1).
				NkVector<uint8> rbsp;
				NkVector<uint32> removed; // indices (domaine émulé, après en-tête NAL 2 octets)
				{
					const uint8 *src = nal + 2;
					const usize n = size - 2;
					for (usize i = 0; i < n; ++i) {
						if (i + 2 < n && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
							rbsp.PushBack(0);
							rbsp.PushBack(0);
							removed.PushBack((uint32)(i + 2));
							i += 2;
						} else {
							rbsp.PushBack(src[i]);
						}
					}
				}
				auto deemToEm = [&](usize d) -> usize {
					usize em = d;
					for (uint64 k = 0; k < removed.Size(); ++k)
						if ((usize)removed[k] <= em)
							++em;
						else
							break;
					return em;
				};
				auto emToDeem = [&](usize em) -> usize {
					usize cnt = 0;
					for (uint64 k = 0; k < removed.Size(); ++k)
						if ((usize)removed[k] < em)
							++cnt;
						else
							break;
					return em - cnt;
				};

				CtuParser p;
				p.sps = &sps;
				p.pps = &pps;
				p.sh = &sh;
				p.stats = &out;
				p.ctbLog2 = sps.log2MinCbSizeY + sps.log2DiffMaxMinCbSizeY;
				p.minCbLog2 = sps.log2MinCbSizeY;
				p.minTbLog2 = sps.log2MinTbSizeY;
				p.maxTbLog2 = sps.log2MinTbSizeY + sps.log2DiffMaxMinTbSizeY;
				p.maxTrafoDepthIntra = sps.maxTransformHierarchyDepthIntra;
				p.maxTrafoDepthInter = sps.maxTransformHierarchyDepthInter;
				p.picW = sps.width;
				p.picH = sps.height;
				const int32 ctbSize = 1 << p.ctbLog2;
				p.picWidthInCtbs = (p.picW + ctbSize - 1) >> p.ctbLog2;
				p.picHeightInCtbs = (p.picH + ctbSize - 1) >> p.ctbLog2;
				p.minCbWidth = (p.picW + (1 << p.minCbLog2) - 1) >> p.minCbLog2;
				p.minCbHeight = (p.picH + (1 << p.minCbLog2) - 1) >> p.minCbLog2;
				p.minPuWidth = (p.picW + 3) >> 2;
				p.minPuHeight = (p.picH + 3) >> 2;
				p.log2MinCuQpDeltaSize = p.ctbLog2 - pps.diffCuQpDeltaDepth;
				p.ctDepth.Resize((usize)(p.minCbWidth * p.minCbHeight));
				p.intraModeY.Resize((usize)(p.minPuWidth * p.minPuHeight));
				for (uint64 i = 0; i < p.intraModeY.Size(); ++i)
					p.intraModeY[i] = 1; // DC par défaut
				p.skipFlagMap.Resize((usize)(p.minCbWidth * p.minCbHeight));
				for (uint64 i = 0; i < p.skipFlagMap.Size(); ++i)
					p.skipFlagMap[i] = 0;
				const int32 picSizeInCtbs = p.picWidthInCtbs * p.picHeightInCtbs;

				if (frame) {
					p.frame = frame;
					p.refsL0 = refsL0;
					p.numRefsL0 = numRefsL0;
					p.refsL1 = refsL1;
					p.numRefsL1 = numRefsL1;
					p.bitDepth = sps.bitDepthLuma;
					p.maxVal = (1 << p.bitDepth) - 1;
					p.qpBdOffsetY = 6 * (sps.bitDepthLuma - 8);
					p.qpBdOffsetC = 6 * (sps.bitDepthChroma - 8);
					const usize puCount = (usize)(p.minPuWidth * p.minPuHeight);
					p.mvL0x.Resize(puCount);
					p.mvL0y.Resize(puCount);
					p.mvL1x.Resize(puCount);
					p.mvL1y.Resize(puCount);
					p.refIdxL0.Resize(puCount);
					p.refIdxL1.Resize(puCount);
					p.predFlag.Resize(puCount);
					p.mvValid.Resize(puCount);
					for (uint64 i = 0; i < p.mvValid.Size(); ++i) {
						p.mvValid[i] = 0;
						p.predFlag[i] = 0;
					}
					frame->lumaW = p.picW;
					frame->lumaH = p.picH;
					frame->chromaW = p.picW >> 1;
					frame->chromaH = p.picH >> 1;
					frame->bitDepth = p.bitDepth;
					frame->cropW =
						p.picW - (sps.conformanceWindow ? 2 * (sps.confWinLeft + sps.confWinRight) : 0);
					frame->cropH =
						p.picH - (sps.conformanceWindow ? 2 * (sps.confWinTop + sps.confWinBottom) : 0);
					frame->y.Resize((usize)(frame->lumaW * frame->lumaH));
					frame->cb.Resize((usize)(frame->chromaW * frame->chromaH));
					frame->cr.Resize((usize)(frame->chromaW * frame->chromaH));
					p.lumaRecon.Resize((usize)(p.minPuWidth * p.minPuHeight));
					const int32 cw4 = ((p.picW >> 1) + 3) >> 2, ch4 = ((p.picH >> 1) + 3) >> 2;
					p.chromaRecon.Resize((usize)(cw4 * ch4));
					for (uint64 i = 0; i < p.lumaRecon.Size(); ++i)
						p.lumaRecon[i] = 0;
					for (uint64 i = 0; i < p.chromaRecon.Size(); ++i)
						p.chromaRecon[i] = 0;
					p.qpMap.Resize((usize)(p.minCbWidth * p.minCbHeight));
					p.qpYPrev = sh.sliceQp;
					p.lastCuQpY = sh.sliceQp;
					p.qgQpYPred = sh.sliceQp;
					// Filtres en boucle : cartes de Boundary Strength (grille 4×4 luma)
					// + cbf_luma par min-TU + paramètres SAO par CTB.
					p.w4L = p.picW >> 2;
					const usize bsCount = (usize)(p.w4L * (p.picH >> 2));
					p.bsVert.Resize(bsCount);
					p.bsHoriz.Resize(bsCount);
					p.cbfLuma4.Resize(bsCount);
					for (uint64 i = 0; i < bsCount; ++i) {
						p.bsVert[i] = 0;
						p.bsHoriz[i] = 0;
						p.cbfLuma4[i] = 0;
					}
					p.saoCtb.Resize((usize)(p.picWidthInCtbs * p.picHeightInCtbs));
					for (uint64 i = 0; i < p.saoCtb.Size(); ++i)
						p.saoCtb[i] = SaoCtb{};
				}

				// WPP : rangées attendues et offsets de sous-ensembles (dé-émulés).
				const bool wpp = pps.entropyCodingSyncEnabled;
				if (wpp && sh.numEntryPointOffsets != p.picHeightInCtbs - 1)
					return false;
				const usize emDataStart = deemToEm(sh.dataByteOffset);

				// Init CABAC de la 1re rangée.
				p.st.Init(sh.sliceQp, NkHevcCabacState::InitTypeFor(sh.sliceType, sh.cabacInit));
				p.eng.InitEngine(rbsp.Data(), (usize)rbsp.Size(), sh.dataByteOffset);
				if (p.eng.codIOffset >= p.eng.codIRange)
					return false;

				int32 ctbAddr = 0;
				usize emSubsetStart = emDataStart;
				while (ctbAddr < picSizeInCtbs) {
					const int32 rx = ctbAddr % p.picWidthInCtbs;
					const int32 ry = ctbAddr / p.picWidthInCtbs;

					if (sh.saoLuma || sh.saoChroma)
						p.ParseSao(rx, ry);
					p.ParseCodingQuadtree(rx << p.ctbLog2, ry << p.ctbLog2, p.ctbLog2, 0);
					if (!p.okFlag)
						return false;

					// WPP : sauvegarde de l'état des contextes après le 2e CTB de la rangée.
					if (wpp && rx == Min32(1, p.picWidthInCtbs - 1)) {
						p.wppSaved = p.st;
						p.wppSavedValid = true;
					}

					const uint32 endOfSlice = p.Terminate();
					++out.ctusParsed;
					++ctbAddr;
					const bool lastCtb = (ctbAddr == picSizeInCtbs);
					if (endOfSlice != (lastCtb ? 1u : 0u))
						return false; // terminaison au mauvais endroit = désynchronisation

					if (!lastCtb && wpp && (ctbAddr % p.picWidthInCtbs) == 0) {
						// Fin de rangée : end_of_subset_one_bit (obligatoirement 1), puis
						// nouvelle init moteur au point d'entrée + contextes restaurés.
						if (p.Terminate() != 1)
							return false;
						const int32 row = ctbAddr / p.picWidthInCtbs; // rangée qui COMMENCE
						emSubsetStart += (usize)sh.entryPointOffsets[(usize)(row - 1)];
						const usize deemStart = emToDeem(emSubsetStart);
						nk_int64 dev = (nk_int64)deemStart - (nk_int64)p.eng.bytePos;
						if (dev < 0)
							dev = -dev;
						if ((int32)dev > out.maxSubsetDeviation)
							out.maxSubsetDeviation = (int32)dev;
						if (deemStart >= (usize)rbsp.Size())
							return false;
						if (p.wppSavedValid)
							p.st = p.wppSaved;
						else
							p.st.Init(sh.sliceQp, NkHevcCabacState::InitTypeFor(sh.sliceType, sh.cabacInit));
						p.eng.InitEngine(rbsp.Data(), (usize)rbsp.Size(), deemStart);
						if (p.eng.codIOffset >= p.eng.codIRange)
							return false;
						p.qpYPrev = sh.sliceQp; // reset qPY_PREV en début de rangée WPP (§8.6.1)
						p.lastCuQpY = sh.sliceQp;
						++out.rows;
					}
				}
				out.rows += 1; // la dernière rangée (pas de end_of_subset après elle)
				if (out.ctusParsed != picSizeInCtbs)
					return false;
				// Filtres en boucle (brique 14) : déblocage (2 passes image entière,
				// Boundary Strength §8.7.2.4 dérivée pendant le parse pour I ET P/B via
				// DeriveDeblockBs) PUIS SAO. Appliqué à I, P et B ; gaté UNIQUEMENT par les
				// drapeaux normatifs (deblockingFilterDisabled / saoLuma / saoChroma) — les
				// flux sans filtres portent ces drapeaux à 0, donc rien n''est filtré chez eux.
				if (frame) {
					if (!sh.deblockingFilterDisabled)
						p.DeblockPicture();
					if (sh.saoLuma || sh.saoChroma) {
						// SAO lit les échantillons PRÉ-SAO (déblocés) et écrit `frame`. On en fait
						// un instantané. Le copy-ctor de NkVector copie ÉLÉMENT PAR ÉLÉMENT
						// (PushBack) — ~35 % du temps de décodage sur un flux avec SAO. Ici :
						// tampons SOURCE réutilisés (thread_local) + NkCopy bloc (memcpy AVX2).
						// Sémantique identique : `frame` conserve ses valeurs pré-SAO, ApplySao
						// n'écrase que les CTB effectivement filtrés en lisant la source.
						static thread_local NkVector<nk_uint16> saoSrcY, saoSrcCb, saoSrcCr;
						saoSrcY.Resize(frame->y.Size());
						saoSrcCb.Resize(frame->cb.Size());
						saoSrcCr.Resize(frame->cr.Size());
						memory::NkCopy(saoSrcY.Data(), frame->y.Data(),
									   (usize)frame->y.Size() * sizeof(nk_uint16));
						memory::NkCopy(saoSrcCb.Data(), frame->cb.Data(),
									   (usize)frame->cb.Size() * sizeof(nk_uint16));
						memory::NkCopy(saoSrcCr.Data(), frame->cr.Data(),
									   (usize)frame->cr.Size() * sizeof(nk_uint16));
						p.ApplySao(saoSrcY, saoSrcCb, saoSrcCr);
					}
				}
				// Persistance du champ de MV bi-liste (§8.5.3.2.8/9) — cette trame devient
				// une "collocated picture" possible pour le décodage de la SUIVANTE. Pour
				// une trame I, p.predFlag reste entièrement à 0 (aucun bloc inter) : le
				// candidat temporel y est authentiquement indisponible, sans cas particulier.
				if (frame) {
					frame->mvColPuWidth = p.minPuWidth;
					frame->mvColPuHeight = p.minPuHeight;
					// `p` detruit juste apres : TRANSFERT (move O(1)) au lieu du copy-ctor NkVector
					// (copie element par element). predFlag encore lu par la boucle refPoc
					// ci-dessous -> deplace APRES la boucle.
					frame->mvColX = traits::NkMove(p.mvL0x);
					frame->mvColY = traits::NkMove(p.mvL0y);
					frame->mvColL1X = traits::NkMove(p.mvL1x);
					frame->mvColL1Y = traits::NkMove(p.mvL1y);
					frame->mvColValid = traits::NkMove(p.mvValid);
					frame->mvColRefPocL0.Resize(p.refIdxL0.Size());
					frame->mvColRefPocL1.Resize(p.refIdxL0.Size());
					for (uint64 i = 0; i < p.refIdxL0.Size(); ++i) {
						const uint8 pf = p.predFlag[i];
						int32 rp0 = 0, rp1 = 0;
						if (pf & 1) {
							const int8 ri = p.refIdxL0[i];
							rp0 = (ri >= 0 && ri < p.numRefsL0 && p.refsL0 && p.refsL0[ri])
									  ? p.refsL0[ri]->poc
									  : 0;
						}
						if (pf & 2) {
							const int8 ri = p.refIdxL1[i];
							rp1 = (ri >= 0 && ri < p.numRefsL1 && p.refsL1 && p.refsL1[ri])
									  ? p.refsL1[ri]->poc
									  : 0;
						}
						frame->mvColRefPocL0[i] = rp0;
						frame->mvColRefPocL1[i] = rp1;
					}
					frame->mvColPredFlag = traits::NkMove(p.predFlag);
				}
				return true;
			}

		} // namespace

		bool NkHevcDecoder::ParseSliceDataIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
												const NkHevcPps &pps, const NkHevcSliceHeader &sh,
												NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, nullptr, nullptr, 0, nullptr, 0, out);
		}

		bool NkHevcDecoder::DecodeSliceIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
											 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
											 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, nullptr, 0, nullptr, 0, out);
		}

		bool NkHevcDecoder::DecodeSliceP(const uint8 *nal, usize size, const NkHevcSps &sps,
										 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
										 const NkHevcFrame *const *refsL0, int32 numRefsL0,
										 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, refsL0, numRefsL0, nullptr, 0, out);
		}

		bool NkHevcDecoder::DecodeSliceB(const uint8 *nal, usize size, const NkHevcSps &sps,
										 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
										 const NkHevcFrame *const *refsL0, int32 numRefsL0,
										 const NkHevcFrame *const *refsL1, int32 numRefsL1,
										 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, refsL0, numRefsL0, refsL1, numRefsL1,
								 out);
		}

	} // namespace media
} // namespace nkentseu
