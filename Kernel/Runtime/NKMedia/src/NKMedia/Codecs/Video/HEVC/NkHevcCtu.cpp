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
					bool isCuQpDeltaCoded = false; // par groupe de quantification

					// CU courant.
					bool cuTransquantBypass = false;
					bool intraSplit = false; // part NxN
					int32 curIntraModeY[4] = {1, 1, 1, 1};
					int32 curIntraModeC = 1;
					int32 curCuX = 0, curCuY = 0, curCuLog2 = 3;

					// ---- Reconstruction (brique 6) — active si frame != nullptr ------
					NkHevcFrame *frame = nullptr;
					int32 bitDepth = 8, maxVal = 255;
					int32 qpBdOffsetY = 0, qpBdOffsetC = 0;
					NkVector<uint8> lumaRecon;	 // TU luma reconstruits (grille 4x4 luma)
					NkVector<uint8> chromaRecon; // TU chroma reconstruits (grille 4x4 chroma)
					NkVector<int8> qpMap;		 // QpY par min-CB (voisinage §8.6.1)
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

					// ---- sao() (§7.3.8.3) ---------------------------------------------
					void ParseSao(int32 rx, int32 ry) {
						bool mergeLeft = false, mergeUp = false;
						if (rx > 0)
							mergeLeft = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (!mergeLeft && ry > 0)
							mergeUp = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (mergeLeft || mergeUp)
							return;
						int32 typeChroma = 0;
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
							if (type == 1) { // bande
								for (int32 i = 0; i < 4; ++i)
									if (offsetAbs[i] != 0)
										Bypass(); // sao_offset_sign
								BypassBits(5);	  // sao_band_position
							} else {
								if (cIdx == 0)
									BypassBits(2); // sao_eo_class_luma
								if (cIdx == 1)
									BypassBits(2); // sao_eo_class_chroma (cIdx 2 hérite)
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
						// Disponible seulement dans le MÊME CTB (sinon qPY_PREV).
						if (xQg > 0 && ((xQg - 1) >> ctbLog2) == (xQg >> ctbLog2))
							qpA = qpMap[(usize)((yQg >> minCbLog2) * minCbWidth + ((xQg - 1) >> minCbLog2))];
						if (yQg > 0 && ((yQg - 1) >> ctbLog2) == (yQg >> ctbLog2))
							qpB = qpMap[(usize)(((yQg - 1) >> minCbLog2) * minCbWidth + (xQg >> minCbLog2))];
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
						const int32 maxDepth = maxTrafoDepthIntra + (intraSplit ? 1 : 0);
						if (log2Size <= maxTbLog2 && log2Size > minTbLog2 && depth < maxDepth &&
							!(intraSplit && depth == 0)) {
							split = Bin(kHevcCtxSplitTransform + 5 - log2Size) != 0;
						} else {
							split = (log2Size > maxTbLog2) || (intraSplit && depth == 0);
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
						// Feuille : cbf_luma (toujours signalé en intra) puis transform_unit.
						const bool cbfLuma = Bin(kHevcCtxCbfLuma + (depth == 0 ? 1 : 0)) != 0;
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
						// Mode intra luma du bloc courant (partition couvrante).
						const int32 lumaMode = curIntraModeY[intraSplit ? pbIdx : 0];
						// Reconstruction luma : prédiction TOUJOURS, résidu si cbf.
						if (frame)
							PredictIntra(0, x0, y0, log2Size, lumaMode);
						if (cbfLuma)
							ParseResidual(x0, y0, log2Size, 0, lumaMode);
						if (frame)
							MarkLumaRecon(x0, y0, 1 << log2Size);
						// Chroma (4:2:0).
						if (log2Size > 2) {
							const int32 cx = x0 >> 1, cy = y0 >> 1;
							if (frame)
								PredictIntra(1, cx, cy, log2Size - 1, curIntraModeC);
							if (cbfCb)
								ParseResidual(cx, cy, log2Size - 1, 1, curIntraModeC);
							if (frame)
								PredictIntra(2, cx, cy, log2Size - 1, curIntraModeC);
							if (cbfCr)
								ParseResidual(cx, cy, log2Size - 1, 2, curIntraModeC);
							if (frame)
								MarkChromaRecon(cx, cy, 1 << (log2Size - 1));
						} else if (blkIdx == 3) {
							const int32 cx = xBase >> 1, cy = yBase >> 1;
							if (frame)
								PredictIntra(1, cx, cy, 2, curIntraModeC);
							if (parentCbfCb)
								ParseResidual(cx, cy, 2, 1, curIntraModeC);
							if (frame)
								PredictIntra(2, cx, cy, 2, curIntraModeC);
							if (parentCbfCr)
								ParseResidual(cx, cy, 2, 2, curIntraModeC);
							if (frame)
								MarkChromaRecon(cx, cy, 4);
						}
					}

					// ---- coding_unit intra (§7.3.8.5) ---------------------------------
					void ParseCodingUnit(int32 x0, int32 y0, int32 log2CbSize) {
						if (!okFlag)
							return;
						++stats->cuCount;
						curCuX = x0;
						curCuY = y0;
						curCuLog2 = log2CbSize;
						cuTransquantBypass = false;
						if (pps->transquantBypassEnabled)
							cuTransquantBypass = Bin(kHevcCtxCuTransquantBypass) != 0;
						intraSplit = false;
						if (log2CbSize == minCbLog2) {
							// part_mode intra : 1 bin (1 = 2Nx2N, 0 = NxN).
							if (Bin(kHevcCtxPartMode) == 0)
								intraSplit = true;
						}
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
						ParseTransformTree(x0, y0, x0, y0, log2CbSize, 0, 0, 0, false, false);
						// QP final du CU -> carte de voisinage + qPY_PREV du prochain groupe.
						if (frame) {
							const int32 qpY = CurrentQpY();
							lastCuQpY = qpY;
							const int32 nMin = (1 << log2CbSize) >> minCbLog2;
							const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
							for (int32 j = 0; j < nMin; ++j)
								for (int32 i = 0; i < nMin; ++i) {
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
						ParseCodingUnit(x0, y0, log2CbSize);
					}
			};

			// Boucle commune slice_segment_data() (parse seul OU parse+reconstruction).
			bool RunSliceIntra(const uint8 *nal, usize size, const NkHevcSps &sps, const NkHevcPps &pps,
							   const NkHevcSliceHeader &sh, NkHevcFrame *frame,
							   NkHevcSliceDataStats &out) {
				out = NkHevcSliceDataStats{};
				if (!nal || size < 4 || !sps.valid || !pps.valid || !sh.valid)
					return false;
				if (sh.sliceType != kHevcSliceI || sh.dependentSliceSegment || !sh.firstSliceSegmentInPic)
					return false; // P/B et slices multiples : briques suivantes
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
				const int32 picSizeInCtbs = p.picWidthInCtbs * p.picHeightInCtbs;

				if (frame) {
					p.frame = frame;
					p.bitDepth = sps.bitDepthLuma;
					p.maxVal = (1 << p.bitDepth) - 1;
					p.qpBdOffsetY = 6 * (sps.bitDepthLuma - 8);
					p.qpBdOffsetC = 6 * (sps.bitDepthChroma - 8);
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
				return out.ctusParsed == picSizeInCtbs;
			}

		} // namespace

		bool NkHevcDecoder::ParseSliceDataIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
												const NkHevcPps &pps, const NkHevcSliceHeader &sh,
												NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, nullptr, out);
		}

		bool NkHevcDecoder::DecodeSliceIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
											 const NkHevcPps &pps, const NkHevcSliceHeader &sh,
											 NkHevcFrame &frame, NkHevcSliceDataStats &out) {
			return RunSliceIntra(nal, size, sps, pps, sh, &frame, out);
		}

	} // namespace media
} // namespace nkentseu
