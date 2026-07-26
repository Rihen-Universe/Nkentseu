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
					NkVector<int16> mvL0x, mvL0y; // MV par bloc 4x4 (grille minPuWidth x minPuHeight)
					NkVector<int8> refIdxL0;	   // index dans refsL0, par bloc 4x4
					NkVector<uint8> mvValid;	   // 1 si le bloc 4x4 est INTER avec MV résolu

					// ---- Reconstruction (brique 6) — active si frame != nullptr ------
					NkHevcFrame *frame = nullptr;
					int32 bitDepth = 8, maxVal = 255;
					int32 qpBdOffsetY = 0, qpBdOffsetC = 0;
					NkVector<uint8> lumaRecon;	 // TU luma reconstruits (grille 4x4 luma)
					NkVector<uint8> chromaRecon; // TU chroma reconstruits (grille 4x4 chroma)
					NkVector<int8> qpMap;		 // QpY par min-CB (voisinage §8.6.1 + déblocage)
					// Filtres en boucle (brique 7) : cartes d'arêtes TU (le déblocage ne
					// filtre que les frontières TU/CU sur la grille 8×8 luma / 8×8 chroma)
					// + paramètres SAO par CTB.
					NkVector<uint8> vEdgeL, hEdgeL; // [y4 * w8 + x8] / [y8 * w4 + x4] luma
					NkVector<uint8> vEdgeC, hEdgeC; // idem en coordonnées chroma
					NkVector<SaoCtb> saoCtb;		// par CTB (rx + ry*picWidthInCtbs)
					int32 w8L = 0, h4L = 0, w4L = 0, h8L = 0;
					int32 w8C = 0, h4C = 0, w4C = 0, h8C = 0;
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

					// Arêtes de TU pour le déblocage (grille 8×8 ; les arêtes internes 4×4
					// des TU/PU N×N ne sont jamais filtrées car hors grille).
					void MarkLumaEdges(int32 x0, int32 y0, int32 n) {
						if ((x0 & 7) == 0 && x0 > 0)
							for (int32 j = 0; j < n; j += 4)
								vEdgeL[(usize)(((y0 + j) >> 2) * w8L + (x0 >> 3))] = 1;
						if ((y0 & 7) == 0 && y0 > 0)
							for (int32 i = 0; i < n; i += 4)
								hEdgeL[(usize)((y0 >> 3) * w4L + ((x0 + i) >> 2))] = 1;
					}
					void MarkChromaEdges(int32 cx, int32 cy, int32 n) {
						if ((cx & 7) == 0 && cx > 0)
							for (int32 j = 0; j < n; j += 4)
								vEdgeC[(usize)(((cy + j) >> 2) * w8C + (cx >> 3))] = 1;
						if ((cy & 7) == 0 && cy > 0)
							for (int32 i = 0; i < n; i += 4)
								hEdgeC[(usize)((cy >> 3) * w4C + ((cx + i) >> 2))] = 1;
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

					// ---- Déblocage (§8.7.2) — intra : BS = 2 sur toutes les arêtes ----
					int32 QpAt(int32 lx, int32 ly) const {
						return qpMap[(usize)((ly >> minCbLog2) * minCbWidth + (lx >> minCbLog2))];
					}

					// Filtre un segment de 4 lignes/colonnes LUMA. `vert` : arête verticale
					// en x=xE (P à gauche), sinon horizontale en y=yE (P au-dessus).
					void LumaDeblockSeg(bool vert, int32 xE, int32 yE) {
						nk_uint16 *Y = frame->y.Data();
						const int32 stride = frame->lumaW;
						const int32 betaOff = sh->sliceBetaOffsetDiv2 * 2;
						const int32 tcOff = sh->sliceTcOffsetDiv2 * 2;
						const int32 qpP = vert ? QpAt(xE - 1, yE) : QpAt(xE, yE - 1);
						const int32 qpQ = QpAt(xE, yE);
						const int32 qp = (qpP + qpQ + 1) >> 1;
						const int32 beta = (int32)kBetaTable[Clip3i(0, 51, qp + betaOff)]
										   << (bitDepth - 8);
						const int32 tc = (int32)kTcTable[Clip3i(0, 53, qp + 2 + tcOff)]
										 << (bitDepth - 8); // BS=2 -> +2 (intra)
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
						// verticalement).
						for (int32 x8 = 1; x8 < (picW >> 3); ++x8)
							for (int32 y4 = 0; y4 < (picH >> 2); ++y4)
								if (vEdgeL[(usize)(y4 * w8L + x8)])
									LumaDeblockSeg(true, x8 << 3, y4 << 2);
						for (int32 y8 = 1; y8 < (picH >> 3); ++y8)
							for (int32 x4 = 0; x4 < (picW >> 2); ++x4)
								if (hEdgeL[(usize)(y8 * w4L + x4)])
									LumaDeblockSeg(false, x4 << 2, y8 << 3);
						const int32 cw = picW >> 1, ch = picH >> 1;
						for (int32 x8 = 1; x8 < ((cw + 7) >> 3); ++x8)
							for (int32 y4 = 0; y4 < (ch >> 2); ++y4)
								if (vEdgeC[(usize)(y4 * w8C + x8)]) {
									ChromaDeblockSeg(true, 1, x8 << 3, y4 << 2);
									ChromaDeblockSeg(true, 2, x8 << 3, y4 << 2);
								}
						for (int32 y8 = 1; y8 < ((ch + 7) >> 3); ++y8)
							for (int32 x4 = 0; x4 < (cw >> 2); ++x4)
								if (hEdgeC[(usize)(y8 * w4C + x4)]) {
									ChromaDeblockSeg(false, 1, x4 << 2, y8 << 3);
									ChromaDeblockSeg(false, 2, x4 << 2, y8 << 3);
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
						if (frame && cuIsIntra) {
							PredictIntra(0, x0, y0, log2Size, lumaMode);
							MarkLumaEdges(x0, y0, 1 << log2Size);
						}
						if (cbfLuma)
							ParseResidual(x0, y0, log2Size, 0, lumaMode);
						if (frame && cuIsIntra)
							MarkLumaRecon(x0, y0, 1 << log2Size);
						// Chroma (4:2:0).
						if (log2Size > 2) {
							const int32 cx = x0 >> 1, cy = y0 >> 1;
							if (frame && cuIsIntra) {
								PredictIntra(1, cx, cy, log2Size - 1, chromaMode);
								MarkChromaEdges(cx, cy, 1 << (log2Size - 1));
							}
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
							if (frame && cuIsIntra) {
								PredictIntra(1, cx, cy, 2, chromaMode);
								MarkChromaEdges(cx, cy, 4);
							}
							if (parentCbfCb)
								ParseResidual(cx, cy, 2, 1, chromaMode);
							if (frame && cuIsIntra)
								PredictIntra(2, cx, cy, 2, chromaMode);
							if (parentCbfCr)
								ParseResidual(cx, cy, 2, 2, chromaMode);
							if (frame && cuIsIntra)
								MarkChromaRecon(cx, cy, 4);
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

					// ---- Champ de MV par bloc 4x4 (voisinage merge/AMVP, briques 10+11) ----
					void StoreMv(int32 x0, int32 y0, int32 w, int32 h, int32 mvx, int32 mvy,
								 int32 refIdx) {
						for (int32 j = 0; j < h; j += 4)
							for (int32 i = 0; i < w; i += 4) {
								const int32 px = (x0 + i) >> 2, py = (y0 + j) >> 2;
								const usize idx = (usize)(py * minPuWidth + px);
								mvL0x[idx] = (int16)mvx;
								mvL0y[idx] = (int16)mvy;
								refIdxL0[idx] = (int8)refIdx;
								mvValid[idx] = 1;
							}
					}
					bool GetMv(int32 x, int32 y, int32 &mvx, int32 &mvy, int32 &refIdx) const {
						const usize idx = (usize)((y >> 2) * minPuWidth + (x >> 2));
						if (!mvValid[idx])
							return false;
						mvx = mvL0x[idx];
						mvy = mvL0y[idx];
						refIdx = refIdxL0[idx];
						return true;
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

					struct MvCand {
							bool avail = false;
							int32 mx = 0, my = 0, refIdx = 0;
					};
					MvCand GetCand(int32 xN, int32 yN, bool posOk) const {
						MvCand c;
						if (!posOk)
							return c;
						int32 mx, my, ri;
						if (GetMv(xN, yN, mx, my, ri)) {
							c.avail = true;
							c.mx = mx;
							c.my = my;
							c.refIdx = ri;
						}
						return c;
					}
					// Élagage anti-doublon merge (§8.5.3.2.3, compare_mv_ref_idx restreint à
					// P/L0 seul) : MV et référence identiques.
					static bool SameMv(const MvCand &a, const MvCand &b) {
						return a.avail && b.avail && a.mx == b.mx && a.my == b.my &&
							   a.refIdx == b.refIdx;
					}
					// Dérive td/tb depuis les POC réels (§8.5.3.2.8) : mise à l'échelle
					// AMVP quand le voisin utilise une réf différente de la cible.
					int32 RefPoc(int32 refIdx) const {
						return (refIdx >= 0 && refIdx < numRefsL0) ? refsL0[refIdx]->poc : 0;
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

					// ---- Fusion spatiale + temporelle (§8.5.3.2.2, multi-référence — brique
					// 11 : le candidat spatial garde le refIdx du VOISIN tel quel, AUCUNE
					// mise à l'échelle n'est jamais appliquée à un candidat spatial ; seul
					// le candidat temporel se met à l'échelle, brique 12). Ordre A1,B1,B0,
					// A0,B2 avec exclusions de partition (§8.5.3.2.3) et élagage anti-
					// doublon (paires EXACTES du spec : B1~A1, B0~B1, A0~A1, B2~{A1,B1}),
					// puis temporel (toujours refsL0[0]), puis repli MV nul (refIdx cyclé
					// 0..numRefsL0-1, §8.5.3.2.9).
					void DeriveMergeCandidates(int32 x0, int32 y0, int32 nPbW, int32 nPbH,
											   int32 partIdx, int32 partMode, int32 (&candX)[5],
											   int32 (&candY)[5], int32 (&candRef)[5],
											   int32 &numCand) {
						const bool a1Excl = (partIdx == 1) && (partMode == kPartNx2N ||
																partMode == kPartnLx2N ||
																partMode == kPartnRx2N);
						const bool b1Excl = (partIdx == 1) && (partMode == kPart2NxN ||
																partMode == kPart2NxnU ||
																partMode == kPart2NxnD);
						const MvCand a1 =
							a1Excl ? MvCand{} : GetCand(x0 - 1, y0 + nPbH - 1, CandLeft(x0));
						const MvCand b1 =
							b1Excl ? MvCand{} : GetCand(x0 + nPbW - 1, y0 - 1, CandUp(y0));
						const MvCand b0 = GetCand(x0 + nPbW, y0 - 1,
												   CandUpRight(x0, y0, nPbW) &&
													   IsDiffMer(x0, y0, x0 + nPbW, y0 - 1));
						const MvCand a0 = GetCand(x0 - 1, y0 + nPbH,
												   CandBottomLeft(x0, y0, nPbH) &&
													   IsDiffMer(x0, y0, x0 - 1, y0 + nPbH));
						const MvCand b2 = GetCand(
							x0 - 1, y0 - 1, CandUpLeft(x0, y0) && IsDiffMer(x0, y0, x0 - 1, y0 - 1));

						numCand = 0;
						auto push = [&](const MvCand &c) {
							candX[numCand] = c.mx;
							candY[numCand] = c.my;
							candRef[numCand] = c.refIdx;
							++numCand;
						};
						if (a1.avail)
							push(a1);
						if (b1.avail && !SameMv(b1, a1))
							push(b1);
						if (b0.avail && !SameMv(b0, b1))
							push(b0);
						if (a0.avail && !SameMv(a0, a1))
							push(a0);
						if (numCand < 4 && b2.avail && !SameMv(b2, a1) && !SameMv(b2, b1))
							push(b2);
						// Candidat temporel (§8.5.3.2.9, brique 12) — toujours vers refsL0[0]
						// (refIdxL0Col=0 normatif), ajouté APRÈS les spatiaux, AVANT le repli nul.
						if (numCand < 5) {
							int32 tx = 0, ty = 0;
							if (DeriveTemporalCand(x0, y0, nPbW, nPbH, RefPoc(0), tx, ty)) {
								candX[numCand] = tx;
								candY[numCand] = ty;
								candRef[numCand] = 0;
								++numCand;
							}
						}
						int32 zeroIdx = 0;
						while (numCand < sh->maxNumMergeCand && numCand < 5) {
							candX[numCand] = 0;
							candY[numCand] = 0;
							candRef[numCand] = (zeroIdx < numRefsL0) ? zeroIdx : 0;
							++numCand;
							++zeroIdx;
						}
					}

					// ---- AMVP spatial (§8.5.3.2.6/7, multi-référence — brique 11) : pour
					// CHAQUE groupe (gauche = A0/A1, haut = B0/B1/B2), passe 1 = 1er voisin
					// dont le refIdx pointe EXACTEMENT vers le même POC que la cible (MV
					// utilisé tel quel) ; si aucun match exact dans le groupe gauche, passe
					// 2 = 1er voisin dispo du groupe gauche, MIS À L'ÉCHELLE (§8.5.3.2.8).
					// Le groupe haut ne prend sa propre passe 2 QUE si le groupe gauche est
					// TOTALEMENT indisponible (A0 et A1 absents), auquel cas son résultat de
					// passe 1 est en plus promu en slot A (comportement exact ffmpeg
					// `ff_hevc_luma_mv_mvp_mode`, `isScaledFlag_L0`/promotion mxB->mxA).
					// Dédoublonnage final sur la VALEUR de MV seule (pas le refIdx : à ce
					// stade A et B sont déjà résolus/mis à l'échelle), puis repli nul.
					bool MatchExact(const MvCand &c, int32 targetPoc, int32 &outX, int32 &outY) const {
						if (!c.avail)
							return false;
						if (RefPoc(c.refIdx) != targetPoc)
							return false;
						outX = c.mx;
						outY = c.my;
						return true;
					}
					bool MatchScaled(const MvCand &c, int32 curPoc, int32 targetPoc, int32 &outX,
									 int32 &outY) const {
						if (!c.avail)
							return false;
						const int32 neighborPoc = RefPoc(c.refIdx);
						if (neighborPoc == targetPoc) {
							outX = c.mx;
							outY = c.my;
							return true;
						}
						int32 td = curPoc - neighborPoc;
						if (td == 0)
							td = 1; // garde-fou mv_scale (poc_diff nul -> division par zéro)
						const int32 tb = curPoc - targetPoc;
						ScaleMv(c.mx, c.my, td, tb, outX, outY);
						return true;
					}
					// ---- Candidat temporel (§8.5.3.2.8/9, brique 12) — dérivé du champ de MV
					// PERSISTANT de la trame collocalisée (colPic = refsL0[collocated_ref_idx],
					// toujours L0 : ce décodeur ne traite que des slices P). Position bas-
					// droite du PU d'abord (repliée si hors CTB courant ou hors image), sinon
					// position centrale (toujours dans l'image) ; bloc INTRA côté colPic (ou
					// colPic lui-même intra, donc SANS champ de MV) -> candidat indisponible.
					// `targetRefIdx`/`targetPoc` = référence VERS LAQUELLE mettre le MV à
					// l'échelle (AMVP : le refIdx signalé par le bitstream ; merge : toujours
					// refsL0[0], §8.5.3.2.1 "refIdxL0Col is set equal to 0").
					bool DeriveTemporalCand(int32 x0, int32 y0, int32 nPbW, int32 nPbH,
											 int32 targetPoc, int32 &outX, int32 &outY) const {
						if (!sh->sliceTemporalMvpEnabled)
							return false;
						const int32 colIdx = sh->collocatedRefIdx;
						if (colIdx < 0 || colIdx >= numRefsL0 || !refsL0[colIdx])
							return false;
						const NkHevcFrame *colPic = refsL0[colIdx];
						if (colPic->mvColValid.IsEmpty() || colPic->mvColPuWidth != minPuWidth ||
							colPic->mvColPuHeight != minPuHeight)
							return false; // colPic intra (I), ou résolution différente
						auto sampleCol = [&](int32 xCol, int32 yCol, int32 &mx, int32 &my,
											  int32 &refPoc) -> bool {
							const int32 px = xCol >> 2, py = yCol >> 2;
							if (px < 0 || py < 0 || px >= colPic->mvColPuWidth ||
								py >= colPic->mvColPuHeight)
								return false;
							const usize idx = (usize)(py * colPic->mvColPuWidth + px);
							if (!colPic->mvColValid[idx])
								return false;
							mx = colPic->mvColX[idx];
							my = colPic->mvColY[idx];
							refPoc = colPic->mvColRefPoc[idx];
							return true;
						};
						int32 mvColX = 0, mvColY = 0, colRefPoc = 0;
						bool have = false;
						const int32 xColBr = x0 + nPbW, yColBr = y0 + nPbH;
						if (((yColBr >> ctbLog2) == (y0 >> ctbLog2)) && yColBr < picH && xColBr < picW) {
							const int32 xColPb = (xColBr >> 4) << 4, yColPb = (yColBr >> 4) << 4;
							have = sampleCol(xColPb, yColPb, mvColX, mvColY, colRefPoc);
						}
						if (!have) {
							const int32 xColCtr = x0 + (nPbW >> 1), yColCtr = y0 + (nPbH >> 1);
							const int32 xColPb = (xColCtr >> 4) << 4, yColPb = (yColCtr >> 4) << 4;
							have = sampleCol(xColPb, yColPb, mvColX, mvColY, colRefPoc);
						}
						if (!have)
							return false;
						int32 td = colPic->poc - colRefPoc;
						if (td == 0)
							td = 1; // garde-fou mv_scale (poc_diff nul -> division par zéro)
						const int32 tb = frame->poc - targetPoc;
						ScaleMv(mvColX, mvColY, td, tb, outX, outY);
						return true;
					}

					void DeriveAmvp(int32 x0, int32 y0, int32 nPbW, int32 nPbH, int32 targetRefIdx,
									int32 (&mvpX)[2], int32 (&mvpY)[2]) {
						const int32 curPoc = frame->poc;
						const int32 targetPoc = RefPoc(targetRefIdx);
						const MvCand a0 = GetCand(x0 - 1, y0 + nPbH, CandBottomLeft(x0, y0, nPbH));
						const MvCand a1 = GetCand(x0 - 1, y0 + nPbH - 1, CandLeft(x0));
						const bool isScaledFlagL0 = a0.avail || a1.avail;

						bool availA = false;
						int32 ax = 0, ay = 0;
						availA = MatchExact(a0, targetPoc, ax, ay) || MatchExact(a1, targetPoc, ax, ay) ||
								 MatchScaled(a0, curPoc, targetPoc, ax, ay) ||
								 MatchScaled(a1, curPoc, targetPoc, ax, ay);

						const MvCand b0 = GetCand(x0 + nPbW, y0 - 1, CandUpRight(x0, y0, nPbW));
						const MvCand b1 = GetCand(x0 + nPbW - 1, y0 - 1, CandUp(y0));
						const MvCand b2 = GetCand(x0 - 1, y0 - 1, CandUpLeft(x0, y0));
						bool availB = false;
						int32 bx = 0, by = 0;
						availB = MatchExact(b0, targetPoc, bx, by) || MatchExact(b1, targetPoc, bx, by) ||
								 MatchExact(b2, targetPoc, bx, by);

						if (!isScaledFlagL0) {
							if (availB) {
								availA = true;
								ax = bx;
								ay = by;
							}
							availB = MatchScaled(b0, curPoc, targetPoc, bx, by) ||
									 MatchScaled(b1, curPoc, targetPoc, bx, by) ||
									 MatchScaled(b2, curPoc, targetPoc, bx, by);
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
						// Candidat temporel (§8.5.3.2.6, brique 12) — seulement si les 2
						// spatiaux ne suffisent pas déjà, mis à l'échelle vers targetRefIdx.
						if (n < 2) {
							int32 tx = 0, ty = 0;
							if (DeriveTemporalCand(x0, y0, nPbW, nPbH, targetPoc, tx, ty)) {
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

					// ---- Finalisation d'un échantillon MC (§8.5.4.2.3/8.5.3.3.4.2) —
					// `raw` = pixel source direct (isDirect) OU sortie du dernier filtre
					// (H, V, ou V-du-HV déjà >>6) ; pondération explicite si présente
					// (P uniquement cette brique : gate = pps->weightedPred).
					int32 FinalizeSample(int32 raw, bool isDirect, bool isLuma, int32 chromaIdx) {
						const int32 mx = isLuma ? maxVal : (1 << sps->bitDepthChroma) - 1;
						if (!pps->weightedPred) {
							if (isDirect)
								return raw;
							const int32 shift = 14 - bitDepth;
							const int32 offset = shift > 0 ? (1 << (shift - 1)) : 0;
							return Clip3i(0, mx, (raw + offset) >> shift);
						}
						const int32 denom =
							isLuma ? sh->lumaLog2WeightDenom : sh->chromaLog2WeightDenom;
						const int32 wx = isLuma ? sh->lumaWeightL0[0] : sh->chromaWeightL0[0][chromaIdx];
						const int32 ox = isLuma ? sh->lumaOffsetL0[0] : sh->chromaOffsetL0[0][chromaIdx];
						const int32 shift = denom + 14 - bitDepth;
						const int32 offset = shift > 0 ? (1 << (shift - 1)) : 0;
						const int32 v = isDirect ? (raw << (14 - bitDepth)) : raw;
						return Clip3i(0, mx, ((v * wx + offset) >> shift) + ox);
					}

					// ---- Compensation de mouvement (§8.5.4) — luma qpel 8 taps, chroma
					// epel 4 taps (4:2:0 : mv&7 directement, hshift=vshift=1 -> aucune
					// mise à l'échelle de fraction supplémentaire, cf. chroma_mc_uni
					// ffmpeg). Échantillonnage hors-image = étendu par bord (clamp),
					// équivalent à emulated_edge_mc.
					void ApplyMotionCompensation(int32 x0, int32 y0, int32 w, int32 h, int32 mvx,
												  int32 mvy, int32 refIdx) {
						if (refIdx < 0 || refIdx >= numRefsL0 || !refsL0[refIdx])
							return;
						const NkHevcFrame *ref0 = refsL0[refIdx];
						// ---- Luma ----
						{
							const int32 xFrac = mvx & 3, yFrac = mvy & 3;
							const int32 xInt = x0 + (mvx >> 2), yInt = y0 + (mvy >> 2);
							const int8 *hf = kHevcQpelFilters[xFrac];
							const int8 *vf = kHevcQpelFilters[yFrac];
							auto SrcY = [&](int32 xx, int32 yy) -> int32 {
								const int32 cx = Clip3i(0, ref0->lumaW - 1, xx);
								const int32 cy = Clip3i(0, ref0->lumaH - 1, yy);
								return ref0->y[(usize)(cy * ref0->lumaW + cx)];
							};
							nk_uint16 *dst = frame->y.Data();
							const int32 stride = frame->lumaW;
							if (xFrac == 0 && yFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i)
										dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
											SrcY(xInt + i, yInt + j), true, true, 0);
							} else if (yFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 8; ++k)
											sum += hf[k] * SrcY(xInt + i + k - 3, yInt + j);
										dst[(y0 + j) * stride + (x0 + i)] =
											(nk_uint16)FinalizeSample(sum, false, true, 0);
									}
							} else if (xFrac == 0) {
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 8; ++k)
											sum += vf[k] * SrcY(xInt + i, yInt + j + k - 3);
										dst[(y0 + j) * stride + (x0 + i)] =
											(nk_uint16)FinalizeSample(sum, false, true, 0);
									}
							} else {
								int32 tmp[(64 + 7) * 64];
								for (int32 j = 0; j < h + 7; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 8; ++k)
											sum += hf[k] * SrcY(xInt + i + k - 3, yInt + j - 3);
										tmp[j * 64 + i] = sum;
									}
								for (int32 j = 0; j < h; ++j)
									for (int32 i = 0; i < w; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 8; ++k)
											sum += vf[k] * tmp[(j + k) * 64 + i];
										dst[(y0 + j) * stride + (x0 + i)] = (nk_uint16)FinalizeSample(
											sum >> 6, false, true, 0);
									}
							}
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
							auto SrcC = [&](int32 xx, int32 yy) -> int32 {
								const int32 cx = Clip3i(0, ref0->chromaW - 1, xx);
								const int32 cy = Clip3i(0, ref0->chromaH - 1, yy);
								return refPlane[(usize)(cy * ref0->chromaW + cx)];
							};
							if (xFrac == 0 && yFrac == 0) {
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i)
										dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
											SrcC(xInt + i, yInt + j), true, false, ci);
							} else if (yFrac == 0) {
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 4; ++k)
											sum += hf[k] * SrcC(xInt + i + k - 1, yInt + j);
										dst[(cy0 + j) * stride + (cx0 + i)] =
											(nk_uint16)FinalizeSample(sum, false, false, ci);
									}
							} else if (xFrac == 0) {
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 4; ++k)
											sum += vf[k] * SrcC(xInt + i, yInt + j + k - 1);
										dst[(cy0 + j) * stride + (cx0 + i)] =
											(nk_uint16)FinalizeSample(sum, false, false, ci);
									}
							} else {
								int32 tmp[(32 + 3) * 32];
								for (int32 j = 0; j < ch + 3; ++j)
									for (int32 i = 0; i < cw; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 4; ++k)
											sum += hf[k] * SrcC(xInt + i + k - 1, yInt + j - 1);
										tmp[j * 32 + i] = sum;
									}
								for (int32 j = 0; j < ch; ++j)
									for (int32 i = 0; i < cw; ++i) {
										int32 sum = 0;
										for (int32 k = 0; k < 4; ++k)
											sum += vf[k] * tmp[(j + k) * 32 + i];
										dst[(cy0 + j) * stride + (cx0 + i)] = (nk_uint16)FinalizeSample(
											sum >> 6, false, false, ci);
									}
							}
						}
						// Marque la zone comme reconstruite : une P-slice peut mélanger CU
						// intra et inter, et la disponibilité des échantillons de référence
						// intra (§8.4.4.2.1) restait câblée sur cuIsIntra seul (brique 6) —
						// sans ce marquage, un voisin inter était vu comme absent par une
						// CU intra adjacente (substitution/padding erronés au lieu des
						// pixels MC réellement écrits).
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
						int32 mvx = 0, mvy = 0, mvRef = 0;
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
								int32 candX[5], candY[5], candRef[5], numCand;
								DeriveMergeCandidates(x0, y0, pw, ph, partIdx, curPartMode, candX,
													  candY, candRef, numCand);
								const int32 useIdx = Min32(idx, numCand - 1);
								mvx = candX[useIdx];
								mvy = candY[useIdx];
								mvRef = candRef[useIdx];
								haveMv = true;
							}
							if (frame) {
								StoreMv(x0, y0, pw, ph, mvx, mvy, mvRef);
								ApplyMotionCompensation(x0, y0, pw, ph, mvx, mvy, mvRef);
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
						int32 mvdX = 0, mvdY = 0;
						int32 mvpFlag = 0;
						int32 refIdxL0Dec = 0;
						if (interPredIdc != 1) { // L0 présent
							refIdxL0Dec = DecodeRefIdx(sh->numRefIdxL0Active);
							DecodeMvd(mvdX, mvdY);
							mvpFlag = (int32)Bin(kHevcCtxMvpLxFlag);
						}
						if (interPredIdc != 0) { // L1 présent (B — jamais reconstruit ici)
							DecodeRefIdx(sh->numRefIdxL1Active);
							if (!(sh->mvdL1Zero && interPredIdc == 2)) {
								int32 dx2, dy2;
								DecodeMvd(dx2, dy2);
							}
							Bin(kHevcCtxMvpLxFlag);
						}
						if (frame && interPredIdc != 1) {
							int32 mvpX[2], mvpY[2];
							DeriveAmvp(x0, y0, pw, ph, refIdxL0Dec, mvpX, mvpY);
							mvx = mvpX[mvpFlag] + mvdX;
							mvy = mvpY[mvpFlag] + mvdY;
							mvRef = refIdxL0Dec;
							haveMv = true;
						}
						if (frame && haveMv) {
							StoreMv(x0, y0, pw, ph, mvx, mvy, mvRef);
							ApplyMotionCompensation(x0, y0, pw, ph, mvx, mvy, mvRef);
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

							if (cuIsIntra) {
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

						if (rqtRootCbf)
							ParseTransformTree(x0, y0, x0, y0, log2CbSize, 0, 0, 0, false, false);

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
							   NkHevcSliceDataStats &out) {
				out = NkHevcSliceDataStats{};
				if (!nal || size < 4 || !sps.valid || !pps.valid || !sh.valid)
					return false;
				if (sh.dependentSliceSegment || !sh.firstSliceSegmentInPic)
					return false; // slices multiples : brique suivante
				if (sh.sliceType != kHevcSliceI && frame) {
					// Briques 11+12 : reconstruction P multi-référence (MC + MV spatiaux
					// merge/AMVP avec mise à l'échelle réelle) + candidat temporel
					// (§8.5.3.2.8/9, via le champ de MV persistant de la trame collocalisée).
					// B : brique suivante (bi-prédiction).
					if (sh.sliceType != kHevcSliceP || !refsL0 || numRefsL0 < sh.numRefIdxL0Active)
						return false;
					if (pps.log2ParallelMergeLevel > 2)
						return false; // règle CU 8x8 forcé-2Nx2N (§7.3.8.6) non implémentée
					if (sps.bitDepthLuma != 8)
						return false; // formules MC de cette brique scopées 8 bits
				}
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
					p.bitDepth = sps.bitDepthLuma;
					p.maxVal = (1 << p.bitDepth) - 1;
					p.qpBdOffsetY = 6 * (sps.bitDepthLuma - 8);
					p.qpBdOffsetC = 6 * (sps.bitDepthChroma - 8);
					p.mvL0x.Resize((usize)(p.minPuWidth * p.minPuHeight));
					p.mvL0y.Resize((usize)(p.minPuWidth * p.minPuHeight));
					p.refIdxL0.Resize((usize)(p.minPuWidth * p.minPuHeight));
					p.mvValid.Resize((usize)(p.minPuWidth * p.minPuHeight));
					for (uint64 i = 0; i < p.mvValid.Size(); ++i)
						p.mvValid[i] = 0;
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
					// Filtres en boucle : cartes d'arêtes + paramètres SAO par CTB.
					const int32 cw = p.picW >> 1, ch = p.picH >> 1;
					p.w8L = p.picW >> 3;
					p.w4L = p.picW >> 2;
					p.w8C = (cw + 7) >> 3;
					p.w4C = (cw + 3) >> 2;
					p.vEdgeL.Resize((usize)(p.w8L * (p.picH >> 2)));
					p.hEdgeL.Resize((usize)(p.w4L * ((p.picH + 7) >> 3)));
					p.vEdgeC.Resize((usize)(p.w8C * (ch >> 2)));
					p.hEdgeC.Resize((usize)(p.w4C * ((ch + 7) >> 3)));
					for (uint64 i = 0; i < p.vEdgeL.Size(); ++i)
						p.vEdgeL[i] = 0;
					for (uint64 i = 0; i < p.hEdgeL.Size(); ++i)
						p.hEdgeL[i] = 0;
					for (uint64 i = 0; i < p.vEdgeC.Size(); ++i)
						p.vEdgeC[i] = 0;
					for (uint64 i = 0; i < p.hEdgeC.Size(); ++i)
						p.hEdgeC[i] = 0;
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
				// Filtres en boucle (brique 7, INTRA seul) : déblocage (2 passes image
				// entière) PUIS SAO. Brique 10 (P) : PAS de filtres — le déblocage
				// existant suppose BS=2 partout (règle intra, §8.7.2.4 pas implémenté
				// pour l'inter) ; la reconstruction P est donc validée PURE (MC+résidu),
				// même précédent que la brique 6 intra avant la brique 7.
				if (frame && sh.sliceType == kHevcSliceI) {
					if (!sh.deblockingFilterDisabled)
						p.DeblockPicture();
					if (sh.saoLuma || sh.saoChroma) {
						NkVector<nk_uint16> srcY = frame->y;
						NkVector<nk_uint16> srcCb = frame->cb;
						NkVector<nk_uint16> srcCr = frame->cr;
						p.ApplySao(srcY, srcCb, srcCr);
					}
				}
				// Persistance du champ de MV (brique 12, §8.5.3.2.8/9) — cette trame
				// devient une "collocated picture" possible pour le décodage de la
				// SUIVANTE. Pour une trame I, p.mvValid reste entièrement à 0 (aucun bloc
				// inter) : mvColValid en sortie est donc entièrement à 0 aussi, ce qui
				// rend le candidat temporel authentiquement indisponible via elle — sans
				// cas particulier à coder.
				if (frame) {
					frame->mvColPuWidth = p.minPuWidth;
					frame->mvColPuHeight = p.minPuHeight;
					frame->mvColX = p.mvL0x;
					frame->mvColY = p.mvL0y;
					frame->mvColValid = p.mvValid;
					frame->mvColRefPoc.Resize(p.refIdxL0.Size());
					for (uint64 i = 0; i < p.refIdxL0.Size(); ++i) {
						if (!p.mvValid[i]) {
							frame->mvColRefPoc[i] = 0;
							continue;
						}
						const int8 ri = p.refIdxL0[i];
						frame->mvColRefPoc[i] =
							(ri >= 0 && ri < p.numRefsL0 && p.refsL0[ri]) ? p.refsL0[ri]->poc : 0;
					}
				}
				return true;
			}

		} // namespace

		bool NkHevcDecoder::ParseSliceDataIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
												const NkHevcPps &pps, const NkHevcSliceHeader &sh,
												NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, nullptr, nullptr, 0, out);
		}

		bool NkHevcDecoder::DecodeSliceIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
											 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
											 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, nullptr, 0, out);
		}

		bool NkHevcDecoder::DecodeSliceP(const uint8 *nal, usize size, const NkHevcSps &sps,
										 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
										 const NkHevcFrame *const *refsL0, int32 numRefsL0,
										 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, refsL0, numRefsL0, out);
		}

	} // namespace media
} // namespace nkentseu
