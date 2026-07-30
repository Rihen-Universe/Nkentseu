// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264IntraDecoder.cpp
// -----------------------------------------------------------------------------
// DÉCODEUR H.264 INTRA (bricks 4-6) : macroblocs I_4x4 / I_16x16 + chroma ->
// prédiction intra + déquant + transformée inverse -> reconstruction YUV 4:2:0.
// MIROIR EXACT de NkH264Encoder (mêmes tables kZigZag/LumaBlk/kCbpIntra, même
// prédiction Predict4x4, mêmes transformées NkH264Transform déjà implémentées).
// Baseline, CAVLC, sans déblocage (suffit pour valider bit-exact un flux no-deblock).
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
#include "NKMedia/Codecs/Video/H264/NkH264BitReader.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"
#include "NKMedia/Codecs/Video/H264/NkH264Transform.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cabac.h"

// SIMD SSE2 (baseline garantie sur x86-64 ; pas de -march requis). Utilisé pour la
// compensation de mouvement (chemins intérieurs sans écrêtage). Fallback scalaire conservé.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
#define NKH264_HAS_SSE2 1
#else
#define NKH264_HAS_SSE2 0
#endif

namespace nkentseu {
	namespace media {

		namespace {

			inline int32 Clamp(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			inline uint8 ClampU8(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}

			// Balayage zig-zag 4x4 (position raster pour chaque indice de scan) — même que l'encodeur.
			const int32 kZigZag[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};

			// Balayage zig-zag 8x8 (profil High) — position raster (row-major) pour chaque indice de scan.
			const int32 kZigZag8x8[64] = {
				0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
				12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
				35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
				58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

			inline void LumaBlk(int32 blkIdx, int32 &x4, int32 &y4) {
				const int32 b8 = blkIdx >> 2, b4 = blkIdx & 3;
				x4 = (b8 & 1) * 8 + (b4 & 1) * 4;
				y4 = (b8 >> 1) * 8 + (b4 >> 1) * 4;
			}

			inline int32 PredictNc(int32 nA, int32 nB) {
				if (nA >= 0 && nB >= 0)
					return (nA + nB + 1) >> 1;
				if (nA >= 0)
					return nA;
				if (nB >= 0)
					return nB;
				return 0;
			}

			const int32 kTrAvail[16] = {1, 1, 2, 0, 1, 3, 2, 0, 2, 2, 2, 0, 2, 0, 2, 0};

			const int32 kCbpIntra[48] = {47, 31, 15, 0,	 23, 27, 29, 30, 7,	 11, 13, 14, 39, 43, 45, 46,
										 16, 3,	 5,	 10, 12, 19, 21, 26, 28, 35, 37, 42, 44, 1,	 2,	 4,
										 8,	 17, 18, 20, 24, 6,	 9,	 22, 25, 32, 33, 34, 36, 40, 38, 41};

			const int32 kCbpInter[48] = {0,	 16, 1,	 2,	 4,	 8,	 32, 3,	 5,	 10, 12, 15, 47, 7,	 11, 13,
										 14, 6,	 9,	 31, 35, 37, 42, 44, 33, 34, 36, 40, 39, 43, 45, 46,
										 17, 18, 20, 24, 19, 21, 26, 28, 23, 27, 29, 30, 22, 25, 38, 41};

			// Inverse-zigzag d'un bloc complet (16 coeffs, scan -> raster).
			inline void InvScan16(const int32 scan[16], int32 raster[16]) {
				for (int32 i = 0; i < 16; ++i)
					raster[kZigZag[i]] = scan[i];
			}

			// --- Prédiction Intra_4x4 (§8.3.1.2), 9 modes — copie exacte de l'encodeur ---
			void Predict4x4(int32 mode, const int32 t[8], const int32 l[4], int32 tl, bool avt, bool avl, uint8 pred[16]) {
				auto T = [&](int32 i) -> int32 { return i < 0 ? tl : t[i]; };
				auto L = [&](int32 j) -> int32 { return j < 0 ? tl : l[j]; };
				auto P = [&](int32 y, int32 x, int32 v) { pred[y * 4 + x] = ClampU8(v); };
				switch (mode) {
					case 0:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x)
								P(y, x, t[x]);
						break;
					case 1:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x)
								P(y, x, l[y]);
						break;
					case 2: {
						int32 dc;
						if (avt && avl)
							dc = (t[0] + t[1] + t[2] + t[3] + l[0] + l[1] + l[2] + l[3] + 4) >> 3;
						else if (avt)
							dc = (t[0] + t[1] + t[2] + t[3] + 2) >> 2;
						else if (avl)
							dc = (l[0] + l[1] + l[2] + l[3] + 2) >> 2;
						else
							dc = 128;
						for (int32 i = 0; i < 16; ++i)
							pred[i] = ClampU8(dc);
						break;
					}
					case 3:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 i = x + y;
								P(y, x, (x == 3 && y == 3) ? (t[6] + 3 * t[7] + 2) >> 2
														  : (t[i] + 2 * t[i + 1] + t[i + 2] + 2) >> 2);
							}
						break;
					case 4:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								if (x > y)
									P(y, x, (T(x - y - 2) + 2 * T(x - y - 1) + T(x - y) + 2) >> 2);
								else if (x < y)
									P(y, x, (L(y - x - 2) + 2 * L(y - x - 1) + L(y - x) + 2) >> 2);
								else
									P(y, x, (t[0] + 2 * tl + l[0] + 2) >> 2);
							}
						break;
					case 5:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 z = 2 * x - y, h = x - (y >> 1);
								if (z >= 0 && (z & 1) == 0)
									P(y, x, (T(h - 1) + T(h) + 1) >> 1);
								else if (z >= 0)
									P(y, x, (T(h - 2) + 2 * T(h - 1) + T(h) + 2) >> 2);
								else if (z == -1)
									P(y, x, (L(0) + 2 * tl + T(0) + 2) >> 2);
								else
									P(y, x, (L(y - 1) + 2 * L(y - 2) + L(y - 3) + 2) >> 2);
							}
						break;
					case 6:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 z = 2 * y - x, h = y - (x >> 1);
								if (z >= 0 && (z & 1) == 0)
									P(y, x, (L(h - 1) + L(h) + 1) >> 1);
								else if (z >= 0)
									P(y, x, (L(h - 2) + 2 * L(h - 1) + L(h) + 2) >> 2);
								else if (z == -1)
									P(y, x, (L(0) + 2 * tl + T(0) + 2) >> 2);
								else
									P(y, x, (T(x - 1) + 2 * T(x - 2) + T(x - 3) + 2) >> 2);
							}
						break;
					case 7:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 h = x + (y >> 1);
								if ((y & 1) == 0)
									P(y, x, (t[h] + t[h + 1] + 1) >> 1);
								else
									P(y, x, (t[h] + 2 * t[h + 1] + t[h + 2] + 2) >> 2);
							}
						break;
					default:
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 z = x + 2 * y, h = y + (x >> 1);
								if (z < 5 && (z & 1) == 0)
									P(y, x, (L(h) + L(h + 1) + 1) >> 1);
								else if (z < 5)
									P(y, x, (L(h) + 2 * L(h + 1) + L(h + 2) + 2) >> 2);
								else if (z == 5)
									P(y, x, (l[2] + 3 * l[3] + 2) >> 2);
								else
									P(y, x, l[3]);
							}
						break;
				}
			}

			// --- Prédiction Intra_16x16 (§8.3.3), 4 modes -> pred[256] ---
			void Predict16x16(int32 mode, const int32 top[16], const int32 left[16], int32 tl, bool avt, bool avl,
							  uint8 pred[256]) {
				if (mode == 0) { // Vertical
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x)
							pred[y * 16 + x] = (uint8)top[x];
				} else if (mode == 1) { // Horizontal
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x)
							pred[y * 16 + x] = (uint8)left[y];
				} else if (mode == 2) { // DC
					int32 s = 0, dc;
					if (avt && avl) {
						for (int32 i = 0; i < 16; ++i)
							s += top[i] + left[i];
						dc = (s + 16) >> 5;
					} else if (avt) {
						for (int32 i = 0; i < 16; ++i)
							s += top[i];
						dc = (s + 8) >> 4;
					} else if (avl) {
						for (int32 i = 0; i < 16; ++i)
							s += left[i];
						dc = (s + 8) >> 4;
					} else
						dc = 128;
					for (int32 i = 0; i < 256; ++i)
						pred[i] = ClampU8(dc);
				} else { // Plane (top[-1] = tl : pour x=7 -> top[6-7] = top[-1] = tl)
					int32 H = 0, V = 0;
					for (int32 x = 0; x < 8; ++x) {
						const int32 a = top[8 + x];
						const int32 b = (6 - x >= 0) ? top[6 - x] : tl;
						H += (x + 1) * (a - b);
					}
					for (int32 y = 0; y < 8; ++y) {
						const int32 a = left[8 + y];
						const int32 b = (6 - y >= 0) ? left[6 - y] : tl;
						V += (y + 1) * (a - b);
					}
					const int32 a = 16 * (top[15] + left[15]);
					const int32 b = (5 * H + 32) >> 6;
					const int32 c = (5 * V + 32) >> 6;
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x)
							pred[y * 16 + x] = ClampU8((a + b * (x - 7) + c * (y - 7) + 16) >> 5);
				}
			}

			// --- Prédiction chroma 8x8 (§8.3.4), 4 modes -> pred[64] ---
			void PredictChroma8x8(int32 mode, const int32 top[8], const int32 left[8], int32 tl, bool avt, bool avl,
								  uint8 pred[64]) {
				if (mode == 1) { // Horizontal
					for (int32 y = 0; y < 8; ++y)
						for (int32 x = 0; x < 8; ++x)
							pred[y * 8 + x] = (uint8)left[y];
					return;
				}
				if (mode == 2) { // Vertical
					for (int32 y = 0; y < 8; ++y)
						for (int32 x = 0; x < 8; ++x)
							pred[y * 8 + x] = (uint8)top[x];
					return;
				}
				if (mode == 3) { // Plane
					int32 H = 0, V = 0;
					for (int32 x = 0; x < 4; ++x) {
						const int32 a = top[4 + x];
						const int32 b = (2 - x >= 0) ? top[2 - x] : tl;
						H += (x + 1) * (a - b);
					}
					for (int32 y = 0; y < 4; ++y) {
						const int32 a = left[4 + y];
						const int32 b = (2 - y >= 0) ? left[2 - y] : tl;
						V += (y + 1) * (a - b);
					}
					const int32 a = 16 * (top[7] + left[7]);
					const int32 b = (17 * H + 16) >> 5;
					const int32 c = (17 * V + 16) >> 5;
					for (int32 y = 0; y < 8; ++y)
						for (int32 x = 0; x < 8; ++x)
							pred[y * 8 + x] = ClampU8((a + b * (x - 3) + c * (y - 3) + 16) >> 5);
					return;
				}
				// mode 0 : DC par quadrant (4x4) — même formule que l'encodeur.
				const int32 sTL = top[0] + top[1] + top[2] + top[3], sTR = top[4] + top[5] + top[6] + top[7];
				const int32 sLT = left[0] + left[1] + left[2] + left[3], sLB = left[4] + left[5] + left[6] + left[7];
				int32 q[4];
				q[0] = (avt && avl) ? (sTL + sLT + 4) >> 3 : avt ? (sTL + 2) >> 2 : avl ? (sLT + 2) >> 2 : 128;
				q[1] = avt ? (sTR + 2) >> 2 : avl ? (sLT + 2) >> 2 : 128;
				q[2] = avl ? (sLB + 2) >> 2 : avt ? (sTL + 2) >> 2 : 128;
				q[3] = (avt && avl) ? (sTR + sLB + 4) >> 3 : avt ? (sTR + 2) >> 2 : avl ? (sLB + 2) >> 2 : 128;
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						pred[y * 8 + x] = ClampU8(q[(x < 4 ? 0 : 1) + (y < 4 ? 0 : 2)]);
			}

			// --- Prediction Intra_8x8 (8.3.2), 9 modes + filtrage des references (8.3.2.2.1) ---
			// Entree : top[16] (p[0..15][-1]), tl (coin), left[8]. avtr = top-right dispo (top[8..15]).
			void Predict8x8(int32 mode, const int32 top0[16], int32 tlIn, const int32 left0[8], bool avt, bool avl,
				bool avtr, uint8 pred[64]) {
				// Filtrage low-pass des references (8.3.2.2.1). Si top-right absent, on etend top[7].
				int32 tp[16], lf[8], tl;
				int32 rawT[16];
				for (int32 x = 0; x < 16; ++x)
					rawT[x] = (x < 8) ? top0[x] : (avtr ? top0[x] : top0[7]);
				// Coin filtre.
				if (avt && avl) tl = (rawT[0] + 2 * tlIn + left0[0] + 2) >> 2;
				else if (avt) tl = (3 * tlIn + rawT[0] + 2) >> 2;
				else if (avl) tl = (3 * tlIn + left0[0] + 2) >> 2;
				else tl = tlIn;
				// Top filtre.
				if (avt) {
					tp[0] = avl ? ((tlIn + 2 * rawT[0] + rawT[1] + 2) >> 2) : ((3 * rawT[0] + rawT[1] + 2) >> 2);
					for (int32 x = 1; x < 15; ++x)
						tp[x] = (rawT[x - 1] + 2 * rawT[x] + rawT[x + 1] + 2) >> 2;
					tp[15] = (rawT[14] + 3 * rawT[15] + 2) >> 2;
				}
				// Left filtre.
				if (avl) {
					lf[0] = avt ? ((tlIn + 2 * left0[0] + left0[1] + 2) >> 2) : ((3 * left0[0] + left0[1] + 2) >> 2);
					for (int32 y = 1; y < 7; ++y)
						lf[y] = (left0[y - 1] + 2 * left0[y] + left0[y + 1] + 2) >> 2;
					lf[7] = (left0[6] + 3 * left0[7] + 2) >> 2;
				}
				auto TP = [&](int32 i) -> int32 { return i < 0 ? tl : tp[i]; };
				auto LF = [&](int32 j) -> int32 { return j < 0 ? tl : lf[j]; };
				auto P = [&](int32 y, int32 x, int32 v) { pred[y * 8 + x] = ClampU8(v); };
				switch (mode) {
					case 0: // Vertical
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) P(y, x, tp[x]);
						break;
					case 1: // Horizontal
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) P(y, x, lf[y]);
						break;
					case 2: { // DC
						int32 dc;
						if (avt && avl) { int32 s = 0; for (int32 k = 0; k < 8; ++k) s += tp[k] + lf[k]; dc = (s + 8) >> 4; }
						else if (avt) { int32 s = 0; for (int32 k = 0; k < 8; ++k) s += tp[k]; dc = (s + 4) >> 3; }
						else if (avl) { int32 s = 0; for (int32 k = 0; k < 8; ++k) s += lf[k]; dc = (s + 4) >> 3; }
						else dc = 128;
						for (int32 i = 0; i < 64; ++i) pred[i] = ClampU8(dc);
						break;
					}
					case 3: // Diagonal Down-Left
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							const int32 i = x + y;
							P(y, x, (x == 7 && y == 7) ? (tp[14] + 3 * tp[15] + 2) >> 2
								: (tp[i] + 2 * tp[i + 1] + tp[i + 2] + 2) >> 2);
						}
						break;
					case 4: // Diagonal Down-Right
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							if (x > y) P(y, x, (TP(x - y - 2) + 2 * TP(x - y - 1) + TP(x - y) + 2) >> 2);
							else if (x < y) P(y, x, (LF(y - x - 2) + 2 * LF(y - x - 1) + LF(y - x) + 2) >> 2);
							else P(y, x, (tp[0] + 2 * tl + lf[0] + 2) >> 2);
						}
						break;
					case 5: // Vertical-Right
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							const int32 z = 2 * x - y, h = x - (y >> 1);
							if (z >= 0 && (z & 1) == 0) P(y, x, (TP(h - 1) + TP(h) + 1) >> 1);
							else if (z >= 0) P(y, x, (TP(h - 2) + 2 * TP(h - 1) + TP(h) + 2) >> 2);
							else if (z == -1) P(y, x, (lf[0] + 2 * tl + tp[0] + 2) >> 2);
							else P(y, x, (LF(y - 2 * x - 1) + 2 * LF(y - 2 * x - 2) + LF(y - 2 * x - 3) + 2) >> 2);
						}
						break;
					case 6: // Horizontal-Down
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							const int32 z = 2 * y - x, h = y - (x >> 1);
							if (z >= 0 && (z & 1) == 0) P(y, x, (LF(h - 1) + LF(h) + 1) >> 1);
							else if (z >= 0) P(y, x, (LF(h - 2) + 2 * LF(h - 1) + LF(h) + 2) >> 2);
							else if (z == -1) P(y, x, (lf[0] + 2 * tl + tp[0] + 2) >> 2);
							else P(y, x, (TP(x - 2 * y - 1) + 2 * TP(x - 2 * y - 2) + TP(x - 2 * y - 3) + 2) >> 2);
						}
						break;
					case 7: // Vertical-Left
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							const int32 h = x + (y >> 1);
							if ((y & 1) == 0) P(y, x, (tp[h] + tp[h + 1] + 1) >> 1);
							else P(y, x, (tp[h] + 2 * tp[h + 1] + tp[h + 2] + 2) >> 2);
						}
						break;
					default: // 8 Horizontal-Up
						for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 8; ++x) {
							const int32 z = x + 2 * y, h = y + (x >> 1);
							if (z < 13 && (z & 1) == 0) P(y, x, (LF(h) + LF(h + 1) + 1) >> 1);
							else if (z < 13) P(y, x, (LF(h) + 2 * LF(h + 1) + LF(h + 2) + 2) >> 2);
							else if (z == 13) P(y, x, (lf[6] + 3 * lf[7] + 2) >> 2);
							else P(y, x, lf[7]);
						}
						break;
				}
			}

			// Contexte de décodage d'une image (plans + grilles de voisinage).
			// UNE liste de references (RefPicList0 ou RefPicList1) : les plans, les poids explicites
			// associes a chaque index, et l'etat de mouvement decode CONTRE cette liste.
			// Les P n'utilisent que L[0] ; les B utilisent les deux et moyennent les deux predictions.
			struct RefList {
					const uint8 *y[16] = {nullptr};
					const uint8 *cb[16] = {nullptr};
					const uint8 *cr[16] = {nullptr};
					int32 numRefs = 0;		// taille de la liste
					int32 numRefActive = 1; // num_ref_idx_lX_active (PPS default / override slice)
					int32 poc[16] = {0};	// POC de chaque entree (poids IMPLICITES des B)
					// Grilles PAR BLOC 4x4. ref4 : -2 non decode, -1 intra/non utilise, >=0 = index dans
					// cette liste. mvd4 : |mvd| borne a 70 (CABAC, ctxIdxInc de mvd §9.3.3.1.1.7).
					int32 *mvx4 = nullptr, *mvy4 = nullptr, *ref4 = nullptr;
					int32 *mvdx4 = nullptr, *mvdy4 = nullptr;
					// Poids EXPLICITES (§8.4.2.3) par index de reference. Neutres par defaut.
					int32 lumaWeight[16], lumaOffset[16];
					int32 chromaWeight[16][2], chromaOffset[16][2];

					RefList() {
						for (int32 i = 0; i < 16; ++i) {
							lumaWeight[i] = 1;
							lumaOffset[i] = 0;
							for (int32 j = 0; j < 2; ++j) {
								chromaWeight[i][j] = 1;
								chromaOffset[i][j] = 0;
							}
						}
					}
			};

			struct DecCtx {
					uint8 *Y = nullptr, *Cb = nullptr, *Cr = nullptr;
					int32 lumaW = 0, lumaH = 0, chromaW = 0, chromaH = 0;
					int32 mbW = 0, mbH = 0;
					int32 nzW = 0, cnzW = 0;
					int32 *lumaNz = nullptr, *chromaNz0 = nullptr, *chromaNz1 = nullptr, *i4mode = nullptr;
					int32 qp = 26;
					int32 chromaQpOffset = 0;
					int32 transform8x8Mode = 0; // PPS transform_8x8_mode_flag (profil High)
					int32 directInference = 1;	// SPS direct_8x8_inference_flag (condition transform 8x8 B)
					// Scaling matrices EFFECTIVES (PPS prioritaire, sinon SPS), en raster ; nullptr = plates.
					// 4x4 : [0]=IntraY [1]=IntraCb [2]=IntraCr [3]=InterY [4]=InterCb [5]=InterCr.
					const nk_uint8 *wsl4[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
					const nk_uint8 *wsl8[2] = {nullptr, nullptr}; // 8x8 : [0]=IntraY [1]=InterY
					int32 W00(int32 i) const {
						return wsl4[i] ? (int32)wsl4[i][0] : 16; // weightScale(0,0) pour les DC
					}
					int32 qpprimeBypass = 0; // SPS qpprime_y_zero_transform_bypass_flag
					// Transform bypass §8.5.15 : lossless quand le flag SPS est armé ET QP du MB == 0.
					bool TsBypass() const {
						return qpprimeBypass != 0 && qp == 0;
					}
					// Par MB : 1 si 16x16 ou intra (sauvegarde dans la frame pour le Direct spatial des
					// B ULTERIEURES, qui liront cette image comme co-localisee).
					uint8 *mb16x16OrIntra = nullptr;
					// Par MB : 1 si le MB utilise la transformée 8x8 (profil High). Le déblocage luma NE
					// filtre PAS les arêtes internes 4x4 d'un MB 8x8 (seulement les bords à 8 échantillons).
					uint8 *transform8x8Mb = nullptr;
					// B-slices : 1 si le bloc 4x4 appartient a une partition Direct. Le contexte de
					// ref_idx (§9.3.3.1.1.6) EXCLUT un voisin Direct meme si sa reference est > 0.
					int32 *direct4 = nullptr;
					// Image CO-LOCALISEE (= RefPicList1[0]) : son champ de mouvement + ses formes.
					// Le Direct spatial (§8.4.1.2.2) y teste si le bloc est "immobile".
					const int32 *colL0x = nullptr, *colL0y = nullptr, *colL0Ref = nullptr;
					const int32 *colL1x = nullptr, *colL1y = nullptr, *colL1Ref = nullptr;
					const uint8 *col16x16OrIntra = nullptr;
					// L[0] = RefPicList0 (P et B), L[1] = RefPicList1 (B seulement).
					RefList L[2];
					// Ponderation EXPLICITE : active + denominateurs (communs aux deux listes).
					// ⚠️ Ne s'applique JAMAIS a une partition bi-predite : la bi-prediction combine les
					// DEUX poids en une seule formule (§8.4.2.3.2), elle ne pondere pas chaque cote.
					int32 weightedPred = 0;
					int32 lumaLog2Denom = 0, chromaLog2Denom = 0;
					// Ponderation IMPLICITE des B (weighted_bipred_idc == 2, le DEFAUT de x264) : les
					// poids se derivent des distances POC, aucune table n'est transmise. implicitW0
					// = poids de L0 pour le couple (refIdx L0, refIdx L1) ; w1 = 64 - w0 ; denom = 5.
					// ⚠️ N'affecte QUE les partitions bi-predites (une partition mono-liste reste brute).
					int32 biPredMode = 0; // 0 = aucune ponderation, 1 = explicite, 2 = implicite
					int32 implicitW0[16][16];
			};

			// §8.4.2.3.2 : applique la ponderation explicite a un echantillon predit.
			//   logWD >= 1 : Clip1(((pred * w + 2^(logWD-1)) >> logWD) + o)
			//   logWD == 0 : Clip1(pred * w + o)
			int32 ApplyWeight(int32 pred, int32 w, int32 o, int32 logWD) {
				const int32 v = (logWD >= 1) ? (((pred * w + (1 << (logWD - 1))) >> logWD) + o) : (pred * w + o);
				return v < 0 ? 0 : (v > 255 ? 255 : v);
			}

			// Sature a un int8 (av_clip_int8 de la reference) : les distances POC des poids implicites.
			int32 ClampI8(int32 v) {
				return v < -128 ? -128 : (v > 127 ? 127 : v);
			}

			// §8.4.2.3.2 (cas bi-predit) : combine les predictions L0 et L1 en UNE formule.
			//   logWD >= 1 : Clip1(((p0*w0 + p1*w1 + 2^logWD) >> (logWD+1)) + ((o0+o1+1) >> 1))
			//   logWD == 0 : Clip1(((p0*w0 + p1*w1 + 1) >> 1) + ((o0+o1+1) >> 1))
			int32 ApplyBiWeight(int32 p0, int32 p1, int32 w0, int32 w1, int32 o0, int32 o1, int32 logWD) {
				const int32 s = p0 * w0 + p1 * w1;
				const int32 v = ((logWD >= 1) ? ((s + (1 << logWD)) >> (logWD + 1)) : ((s + 1) >> 1)) +
								((o0 + o1 + 1) >> 1);
				return v < 0 ? 0 : (v > 255 ? 255 : v);
			}

			// Cœur MC luma quart-pel (§8.4.2.2.1). CLAMP=true : accès bornés (bloc au bord de l'image) ;
			// CLAMP=false : chemin rapide intérieur (l'appelant a prouvé que tout le support 6-tap est
			// dans l'image), l'écrêtage devient un no-op → éliminé à la compilation. Arithmétique
			// STRICTEMENT identique dans les deux cas (bit-exact).
			template <bool CLAMP>
			static void McLumaImpl(const uint8 *ref, int32 W, int32 H, int32 bx, int32 by, int32 fx, int32 fy, int32 w,
								   int32 h, int32 ox, int32 oy, uint8 predY[256], bool doWeight, int32 lw, int32 lo,
								   int32 logWD) {
				auto R = [&](int32 x, int32 y) -> int32 {
					if (CLAMP) {
						x = x < 0 ? 0 : (x >= W ? W - 1 : x);
						y = y < 0 ? 0 : (y >= H ? H - 1 : y);
					}
					return ref[(usize)y * W + x];
				};
				auto Hor = [&](int32 x, int32 y) -> int32 {
					return R(x - 2, y) - 5 * R(x - 1, y) + 20 * R(x, y) + 20 * R(x + 1, y) - 5 * R(x + 2, y) + R(x + 3, y);
				};
				auto Ver = [&](int32 x, int32 y) -> int32 {
					return R(x, y - 2) - 5 * R(x, y - 1) + 20 * R(x, y) + 20 * R(x, y + 1) - 5 * R(x, y + 2) + R(x, y + 3);
				};
				int32 horBuf[21 * 16];
				int32 rowLo = 0;
				if (fx != 0) {
					rowLo = -2;
					const int32 rowHi = h + 2;
					for (int32 r = rowLo; r <= rowHi; ++r)
						for (int32 xx = 0; xx < w; ++xx)
							horBuf[(r - rowLo) * w + xx] = Hor(bx + xx, by + r);
				}
				auto HorC = [&](int32 x, int32 y) -> int32 { return horBuf[(y - by - rowLo) * w + (x - bx)]; };
				auto Bh = [&](int32 x, int32 y) -> int32 { return ClampU8((HorC(x, y) + 16) >> 5); };
				auto Hh = [&](int32 x, int32 y) -> int32 { return ClampU8((Ver(x, y) + 16) >> 5); };
				auto Jj = [&](int32 x, int32 y) -> int32 {
					const int32 j1 = HorC(x, y - 2) - 5 * HorC(x, y - 1) + 20 * HorC(x, y) + 20 * HorC(x, y + 1) -
									 5 * HorC(x, y + 2) + HorC(x, y + 3);
					return ClampU8((j1 + 512) >> 10);
				};
				// La sélection de position quart-pel est INVARIANTE sur tout le rectangle : on la hisse
				// hors de la boucle par pixel (un seul cas par appel, boucles serrées vectorisables).
				const int32 pos = fy * 4 + fx;
				for (int32 y = 0; y < h; ++y) {
					uint8 *out = &predY[(oy + y) * 16 + ox];
					const int32 iy = by + y;
					for (int32 x = 0; x < w; ++x) {
						const int32 ix = bx + x;
						int32 v;
						switch (pos) {
							case 0: v = R(ix, iy); break;
							case 1: v = (R(ix, iy) + Bh(ix, iy) + 1) >> 1; break;
							case 2: v = Bh(ix, iy); break;
							case 3: v = (Bh(ix, iy) + R(ix + 1, iy) + 1) >> 1; break;
							case 4: v = (R(ix, iy) + Hh(ix, iy) + 1) >> 1; break;
							case 5: v = (Bh(ix, iy) + Hh(ix, iy) + 1) >> 1; break;
							case 6: v = (Bh(ix, iy) + Jj(ix, iy) + 1) >> 1; break;
							case 7: v = (Bh(ix, iy) + Hh(ix + 1, iy) + 1) >> 1; break;
							case 8: v = Hh(ix, iy); break;
							case 9: v = (Hh(ix, iy) + Jj(ix, iy) + 1) >> 1; break;
							case 10: v = Jj(ix, iy); break;
							case 11: v = (Jj(ix, iy) + Hh(ix + 1, iy) + 1) >> 1; break;
							case 12: v = (Hh(ix, iy) + R(ix, iy + 1) + 1) >> 1; break;
							case 13: v = (Hh(ix, iy) + Bh(ix, iy + 1) + 1) >> 1; break;
							case 14: v = (Jj(ix, iy) + Bh(ix, iy + 1) + 1) >> 1; break;
							default: v = (Hh(ix + 1, iy) + Bh(ix, iy + 1) + 1) >> 1; break;
						}
						if (doWeight)
							v = ApplyWeight(v, lw, lo, logWD);
						out[x] = (uint8)v;
					}
				}
			}

			// MC luma quart-pel (§8.4.2.2.1) d'un rectangle w×h -> écrit dans predY[16*16] à (ox,oy).
			void McLumaRect(const DecCtx &c, const RefList &L, int32 dx, int32 dy, int32 ox, int32 oy, int32 w, int32 h, int32 mvx,
							int32 mvy, uint8 predY[256], int32 refIdx = 0,
							bool applyWeight = true) {
				const int32 W = c.lumaW, H = c.lumaH;
				if (refIdx < 0)
					refIdx = 0;
				if (refIdx >= L.numRefs)
					refIdx = L.numRefs > 0 ? L.numRefs - 1 : 0;
				const uint8 *ref = L.y[refIdx];
				const int32 fx = mvx & 3, fy = mvy & 3;
				const int32 bx = dx + (mvx >> 2), by = dy + (mvy >> 2);
				const bool doWeight = c.weightedPred && applyWeight;
				const int32 lw = L.lumaWeight[refIdx], lo = L.lumaOffset[refIdx], logWD = c.lumaLog2Denom;
				// Support du filtre 6-tap : x ∈ [bx-2, bx+w+2], y ∈ [by-2, by+h+2]. S'il est
				// entièrement dans l'image, l'écrêtage est inutile → chemin rapide sans clamp.
				const bool inside = (bx >= 2 && by >= 2 && bx + w + 2 < W && by + h + 2 < H);
				if (inside)
					McLumaImpl<false>(ref, W, H, bx, by, fx, fy, w, h, ox, oy, predY, doWeight, lw, lo, logWD);
				else
					McLumaImpl<true>(ref, W, H, bx, by, fx, fy, w, h, ox, oy, predY, doWeight, lw, lo, logWD);
			}

			// Cœur MC chroma bilinéaire (§8.4.2.2.2). CLAMP=false : chemin rapide intérieur.
			// Les 4 poids bilinéaires sont invariants sur le rectangle → hissés hors des boucles.
			template <bool CLAMP>
			static void McChromaImpl(const uint8 *ref, int32 W, int32 H, int32 bx0, int32 by0, int32 w00, int32 w10,
									 int32 w01, int32 w11, int32 cw, int32 ch, int32 ox, int32 oy, uint8 cPred[64],
									 bool doWeight, int32 cwgt, int32 coff, int32 logWD) {
#if NKH264_HAS_SSE2
				// Chemin SIMD : intérieur (pas de clamp), sans pondération, largeur 8 (cas dominant :
				// bloc chroma 8×8). Les produits w·octet tiennent dans 16 bits (Σw=64, octet≤255 →
				// Σ≤16320+32<32768) : arithmétique STRICTEMENT identique au scalaire (bit-exact).
				if (!CLAMP && !doWeight && cw == 8 && ox == 0) {
					const __m128i vw00 = _mm_set1_epi16((short)w00), vw10 = _mm_set1_epi16((short)w10);
					const __m128i vw01 = _mm_set1_epi16((short)w01), vw11 = _mm_set1_epi16((short)w11);
					const __m128i v32 = _mm_set1_epi16(32), z = _mm_setzero_si128();
					for (int32 y = 0; y < ch; ++y) {
						const uint8 *r0 = &ref[(usize)(by0 + y) * W + bx0];
						const uint8 *r1 = r0 + W;
						__m128i a = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)r0), z);
						__m128i b = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(r0 + 1)), z);
						__m128i cc = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)r1), z);
						__m128i d = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(r1 + 1)), z);
						__m128i acc = _mm_add_epi16(_mm_mullo_epi16(a, vw00), _mm_mullo_epi16(b, vw10));
						acc = _mm_add_epi16(acc, _mm_mullo_epi16(cc, vw01));
						acc = _mm_add_epi16(acc, _mm_mullo_epi16(d, vw11));
						acc = _mm_srli_epi16(_mm_add_epi16(acc, v32), 6);
						_mm_storel_epi64((__m128i *)&cPred[(oy + y) * 8], _mm_packus_epi16(acc, acc));
					}
					return;
				}
#endif
				auto C = [&](int32 x, int32 y) -> int32 {
					if (CLAMP) {
						x = x < 0 ? 0 : (x >= W ? W - 1 : x);
						y = y < 0 ? 0 : (y >= H ? H - 1 : y);
					}
					return ref[(usize)y * W + x];
				};
				for (int32 y = 0; y < ch; ++y) {
					uint8 *out = &cPred[(oy + y) * 8 + ox];
					const int32 ry = by0 + y;
					for (int32 x = 0; x < cw; ++x) {
						const int32 rx = bx0 + x;
						const int32 A = C(rx, ry), Bb = C(rx + 1, ry), Cc = C(rx, ry + 1), D = C(rx + 1, ry + 1);
						int32 v = (w00 * A + w10 * Bb + w01 * Cc + w11 * D + 32) >> 6;
						if (doWeight)
							v = ApplyWeight(v, cwgt, coff, logWD);
						out[x] = (uint8)v;
					}
				}
			}

			// MC chroma 1/8-pel bilinéaire (§8.4.2.2.2) d'un rectangle cw×ch -> cPred[64] à (ox,oy).
			void McChromaRect(const DecCtx &c, const RefList &L, int32 comp, int32 dx, int32 dy, int32 ox, int32 oy, int32 cw, int32 ch,
							  int32 mvx, int32 mvy, uint8 cPred[64], int32 refIdx = 0,
							  bool applyWeight = true) {
				if (refIdx < 0)
					refIdx = 0;
				if (refIdx >= L.numRefs)
					refIdx = L.numRefs > 0 ? L.numRefs - 1 : 0;
				const uint8 *ref = (comp == 0) ? L.cb[refIdx] : L.cr[refIdx];
				const int32 W = c.chromaW, H = c.chromaH;
				const int32 fx = mvx & 7, fy = mvy & 7, oxx = mvx >> 3, oyy = mvy >> 3;
				const int32 bx0 = dx + oxx, by0 = dy + oyy;
				const int32 w00 = (8 - fx) * (8 - fy), w10 = fx * (8 - fy), w01 = (8 - fx) * fy, w11 = fx * fy;
				const bool doWeight = c.weightedPred && applyWeight;
				const int32 cwgt = L.chromaWeight[refIdx][comp], coff = L.chromaOffset[refIdx][comp];
				const int32 logWD = c.chromaLog2Denom;
				// Support bilinéaire : x ∈ [bx0, bx0+cw], y ∈ [by0, by0+ch]. Intérieur → sans clamp.
				const bool inside = (bx0 >= 0 && by0 >= 0 && bx0 + cw < W && by0 + ch < H);
				if (inside)
					McChromaImpl<false>(ref, W, H, bx0, by0, w00, w10, w01, w11, cw, ch, ox, oy, cPred, doWeight, cwgt,
										coff, logWD);
				else
					McChromaImpl<true>(ref, W, H, bx0, by0, w00, w10, w01, w11, cw, ch, ox, oy, cPred, doWeight, cwgt,
									   coff, logWD);
			}

			// Voisin 4x4 pour la prédiction de MV : dispo + ref (-1 = intra/hors) + MV.
			void GetMv4(const DecCtx &c, const RefList &L, int32 x4, int32 y4, bool &avail, int32 &ref, int32 &mx, int32 &my) {
				const int32 w4 = c.mbW * 4, h4 = c.mbH * 4;
				if (x4 < 0 || y4 < 0 || x4 >= w4 || y4 >= h4 || L.ref4[y4 * c.nzW + x4] == -2) {
					avail = false;
					ref = -1;
					mx = my = 0;
					return;
				}
				avail = true;
				ref = L.ref4[y4 * c.nzW + x4];
				// Voisin INTRA (ref==-1) : sa MV vaut 0 pour la prediction (H.264 §8.4.1.3.2),
				// PAS la valeur périmée de mvx4/mvy4 (StoreMv4 n'est appelé que pour l'inter).
				if (ref < 0) {
					mx = my = 0;
				} else {
					mx = L.mvx4[y4 * c.nzW + x4];
					my = L.mvy4[y4 * c.nzW + x4];
				}
			}

			// Stocke la MV + l'index de référence d'une partition (grille 4x4). ref4 = refIdx (>=0 inter).
			void StoreMv4(const DecCtx &c, const RefList &L, int32 bx, int32 by, int32 pw, int32 ph, int32 mvx, int32 mvy,
						  int32 refIdx) {
				for (int32 y = by; y < by + ph; ++y)
					for (int32 x = bx; x < bx + pw; ++x) {
						L.mvx4[y * c.nzW + x] = mvx;
						L.mvy4[y * c.nzW + x] = mvy;
						L.ref4[y * c.nzW + x] = refIdx;
					}
			}

			// Stocke SEULEMENT l'index de reference d'une partition (CABAC : la reference remplit la grille
			// ref des la lecture du ref_idx, AVANT les mvd, car les partitions se voisinent entre elles
			// pour le ctxIdxInc du ref_idx suivant).
			void StoreRef4(const DecCtx &c, const RefList &L, int32 bx, int32 by, int32 pw, int32 ph, int32 refIdx) {
				for (int32 y = by; y < by + ph; ++y)
					for (int32 x = bx; x < bx + pw; ++x)
						L.ref4[y * c.nzW + x] = refIdx;
			}

			int32 Med3(int32 a, int32 b, int32 cc) {
				const int32 mn = a < b ? (a < cc ? a : cc) : (b < cc ? b : cc);
				const int32 mx = a > b ? (a > cc ? a : cc) : (b > cc ? b : cc);
				return a + b + cc - mn - mx;
			}

			// Prédiction de MV d'une partition (§8.4.1.3). (bx,by) = coin 4x4, pw/ph = taille 4x4,
			// mbType (1=16x8, 2=8x16 -> cas directionnels), part = index. refIdx=0 (baseline ref=1).
			void PredMvPart(const DecCtx &c, const RefList &L, int32 bx, int32 by, int32 pw, int32 mbType, int32 part,
							int32 refIdx,
							int32 &pmx, int32 &pmy) {
				bool avA, avB, avC;
				int32 rA, rB, rC, ax, ay, bxx, byy, cx, cy;
				GetMv4(c, L, bx - 1, by, avA, rA, ax, ay);
				GetMv4(c, L, bx, by - 1, avB, rB, bxx, byy);
				GetMv4(c, L, bx + pw, by - 1, avC, rC, cx, cy);
				if (!avC)
					GetMv4(c, L, bx - 1, by - 1, avC, rC, cx, cy); // repli D
				if (mbType == 1) { // P_16x8
					if (part == 0 && avB && rB == refIdx) {
						pmx = bxx;
						pmy = byy;
						return;
					}
					if (part == 1 && avA && rA == refIdx) {
						pmx = ax;
						pmy = ay;
						return;
					}
				} else if (mbType == 2) { // P_8x16
					if (part == 0 && avA && rA == refIdx) {
						pmx = ax;
						pmy = ay;
						return;
					}
					if (part == 1 && avC && rC == refIdx) {
						pmx = cx;
						pmy = cy;
						return;
					}
				}
				if (!avB && !avC && avA) { // B et C indispo, A dispo -> B=C=A
					avB = avC = true;
					rB = rC = rA;
					bxx = cx = ax;
					byy = cy = ay;
				}
				const int32 eqA = (avA && rA == refIdx) ? 1 : 0;
				const int32 eqB = (avB && rB == refIdx) ? 1 : 0;
				const int32 eqC = (avC && rC == refIdx) ? 1 : 0;
				if (eqA + eqB + eqC == 1) {
					if (eqA) {
						pmx = ax;
						pmy = ay;
					} else if (eqB) {
						pmx = bxx;
						pmy = byy;
					} else {
						pmx = cx;
						pmy = cy;
					}
					return;
				}
				pmx = Med3(ax, bxx, cx);
				pmy = Med3(ay, byy, cy);
			}

			// P_Skip (§8.4.1.1) : MV nul si A/B manque ou est inter à MV nulle ; sinon médian 16x16.
			void SkipMv(const DecCtx &c, int32 mbX, int32 mbY, int32 &smx, int32 &smy) {
				const RefList &L = c.L[0]; // P_Skip : toujours la reference 0 de la liste 0
				const int32 bx = mbX * 4, by = mbY * 4;
				bool avA, avB;
				int32 rA, rB, ax, ay, bxx, byy;
				GetMv4(c, L, bx - 1, by, avA, rA, ax, ay);
				GetMv4(c, L, bx, by - 1, avB, rB, bxx, byy);
				if (!avA || !avB || (rA == 0 && ax == 0 && ay == 0) || (rB == 0 && bxx == 0 && byy == 0)) {
					smx = smy = 0;
					return;
				}
				PredMvPart(c, L, bx, by, 4, 0, 0, 0, smx, smy); // P_Skip -> référence 0
			}

			// De-emulation locale (00 00 03 -> 00 00).
			void Deemul(const uint8 *src, usize n, NkVector<nk_uint8> &out) {
				out.Clear();
				for (usize i = 0; i < n; ++i) {
					if (i + 2 < n && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
						out.PushBack(0);
						out.PushBack(0);
						i += 2;
					} else
						out.PushBack(src[i]);
				}
			}

			// Décode et reconstruit le résidu chroma (DC + AC) pour une prédiction cPred donnée (intra OU inter).
			// `intra` sélectionne la scaling list chroma (1/2 = intra Cb/Cr, 4/5 = inter Cb/Cr).
			void DecodeChromaResidual(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 cbpChroma,
									  const uint8 cPred[2][64], bool intra) {
				const int32 qpC = NkH264Transform::ChromaQp(Clamp(c.qp + c.chromaQpOffset, 0, 51));
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				const int32 wCb = intra ? 1 : 4, wCr = intra ? 2 : 5;

				// DC chroma (4 coeffs raster 2x2 par composante) si cbp&3.
				int32 cDcRec[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
				if (cbpChroma & 3) {
					for (int32 comp = 0; comp < 2; ++comp) {
						int32 dcLvl[4];
						NkH264Cavlc::DecodeResidual(br, dcLvl, 4, -1);
						int32 gdc[4];
						NkH264Transform::Hadamard2x2(dcLvl, gdc);
						NkH264Transform::DequantChromaDC(gdc, cDcRec[comp], qpC, c.W00(comp == 0 ? wCb : wCr));
					}
				}

				// AC chroma + reconstruction.
				const int32 cbx0 = mbX * 2, cby0 = mbY * 2;
				for (int32 comp = 0; comp < 2; ++comp) {
					int32 *cnz = (comp == 0) ? c.chromaNz0 : c.chromaNz1;
					uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
					for (int32 blk = 0; blk < 4; ++blk) {
						const int32 bx4 = (blk & 1) * 4, by4 = (blk >> 1) * 4;
						const int32 bx = cbx0 + (blk & 1), by = cby0 + (blk >> 1);
						int32 acRaster[16] = {0};
						int32 tc = 0;
						if (cbpChroma & 2) {
							const int32 nA = (bx > 0) ? cnz[by * c.cnzW + (bx - 1)] : -1;
							const int32 nB = (by > 0) ? cnz[(by - 1) * c.cnzW + bx] : -1;
							const int32 nC = PredictNc(nA, nB);
							int32 acScan[15];
							tc = NkH264Cavlc::DecodeResidual(br, acScan, 15, nC);
							for (int32 k = 1; k < 16; ++k)
								acRaster[kZigZag[k]] = acScan[k - 1];
						}
						cnz[by * c.cnzW + bx] = tc;

						int32 deq[16];
						NkH264Transform::Dequant4x4(acRaster, deq, qpC, c.wsl4[comp == 0 ? wCb : wCr]);
						if (!(cbpChroma & 2))
							for (int32 k = 1; k < 16; ++k)
								deq[k] = 0;
						deq[0] = (cbpChroma & 3) ? cDcRec[comp][blk] : 0;
						int32 resRec[16];
						NkH264Transform::Inverse4x4(deq, resRec);
						for (int32 r = 0; r < 4; ++r)
							for (int32 col = 0; col < 4; ++col) {
								const int32 p = cPred[comp][(by4 + r) * 8 + (bx4 + col)];
								rec[(usize)(cpy + by4 + r) * c.chromaW + cpx + bx4 + col] =
									ClampU8(p + resRec[r * 4 + col]);
							}
					}
				}
			}

			// Chroma INTRA : calcule la prédiction (4 modes) puis décode le résidu.
			void DecodeChroma(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 chromaMode, int32 cbpChroma) {
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				const bool avt = mbY > 0, avl = mbX > 0;
				uint8 cPred[2][64];
				for (int32 comp = 0; comp < 2; ++comp) {
					const uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
					int32 ct[8], cl[8], tl = 128;
					for (int32 i = 0; i < 8; ++i) {
						ct[i] = avt ? rec[(usize)(cpy - 1) * c.chromaW + cpx + i] : 0;
						cl[i] = avl ? rec[(usize)(cpy + i) * c.chromaW + cpx - 1] : 0;
					}
					if (avt && avl)
						tl = rec[(usize)(cpy - 1) * c.chromaW + cpx - 1];
					else if (avt)
						tl = ct[0];
					else if (avl)
						tl = cl[0];
					PredictChroma8x8(chromaMode, ct, cl, tl, avt, avl, cPred[comp]);
				}
				DecodeChromaResidual(c, br, mbX, mbY, cbpChroma, cPred, true);
			}

			// --- Macrobloc I_4x4 ---
			bool DecodeMbI4x4(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY) {
				const int32 px = mbX * 16, py = mbY * 16;
				const bool availTop = mbY > 0, availLeft = mbX > 0;
				const bool availTrMb = (mbY > 0) && (mbX < c.mbW - 1);

				// mb_pred : 16 modes intra4x4.
				int32 mode4[16];
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
					int32 predMode;
					if (bx == 0 || by == 0)
						predMode = 2;
					else {
						const int32 a = c.i4mode[by * c.nzW + (bx - 1)];
						const int32 b = c.i4mode[(by - 1) * c.nzW + bx];
						predMode = a < b ? a : b;
					}
					const uint32 prevFlag = br.U1();
					int32 mode;
					if (prevFlag)
						mode = predMode;
					else {
						const int32 rem = (int32)br.U(3);
						mode = (rem < predMode) ? rem : rem + 1;
					}
					mode4[blk] = mode;
					c.i4mode[by * c.nzW + bx] = mode;
				}

				const int32 chromaMode = (int32)br.UE();
				const int32 codeNum = (int32)br.UE();
				if (codeNum >= 48)
					return false;
				const int32 cbp = kCbpIntra[codeNum];
				const int32 lumaCbp = cbp & 15, cbpChroma = cbp >> 4;
				if (cbp > 0)
					c.qp = (c.qp + br.SE() + 52) % 52;

				// Reconstruction bloc par bloc (prédiction en cascade sur les pixels déjà reconstruits).
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 X = px + x4, Y = py + y4;
					const bool avt = (y4 > 0) || availTop;
					const bool avl = (x4 > 0) || availLeft;
					const bool avtl = avt && avl;
					bool avtr;
					switch (kTrAvail[blk]) {
						case 2: avtr = true; break;
						case 1: avtr = availTop; break;
						case 3: avtr = availTrMb; break;
						default: avtr = false; break;
					}
					int32 t[8], l[4], tl;
					for (int32 i = 0; i < 4; ++i)
						t[i] = avt ? c.Y[(usize)(Y - 1) * c.lumaW + X + i] : 0;
					for (int32 i = 0; i < 4; ++i)
						t[4 + i] = (avt && avtr) ? c.Y[(usize)(Y - 1) * c.lumaW + X + 4 + i] : t[3];
					for (int32 j = 0; j < 4; ++j)
						l[j] = avl ? c.Y[(usize)(Y + j) * c.lumaW + X - 1] : 0;
					tl = avtl ? c.Y[(usize)(Y - 1) * c.lumaW + X - 1] : 0;

					uint8 pred[16];
					Predict4x4(mode4[blk], t, l, tl, avt, avl, pred);

					const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
					int32 raster[16] = {0};
					int32 tc = 0;
					if (lumaCbp & (1 << (blk >> 2))) {
						const int32 nA = (bx > 0) ? c.lumaNz[by * c.nzW + (bx - 1)] : -1;
						const int32 nB = (by > 0) ? c.lumaNz[(by - 1) * c.nzW + bx] : -1;
						const int32 nC = PredictNc(nA, nB);
						int32 scan[16];
						tc = NkH264Cavlc::DecodeResidual(br, scan, 16, nC);
						InvScan16(scan, raster);
					}
					c.lumaNz[by * c.nzW + bx] = tc;

					int32 deq[16], resRec[16];
					NkH264Transform::Dequant4x4(raster, deq, c.qp, c.wsl4[0]);
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(Y + r) * c.lumaW + X + col] = ClampU8(pred[r * 4 + col] + resRec[r * 4 + col]);
				}

				DecodeChroma(c, br, mbX, mbY, chromaMode, cbpChroma);
				return true;
			}

			// --- Macrobloc I_16x16 ---
			bool DecodeMbI16x16(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 mbType) {
				const int32 px = mbX * 16, py = mbY * 16;
				const int32 predMode = (mbType - 1) % 4;
				const int32 cbpChroma = ((mbType - 1) / 4) % 3;
				const int32 cbpLuma = ((mbType - 1) >= 12) ? 15 : 0;
				const bool availTop = mbY > 0, availLeft = mbX > 0;

				// Prédiction 16x16.
				int32 top[16], left[16], tl = 128;
				for (int32 i = 0; i < 16; ++i) {
					top[i] = availTop ? c.Y[(usize)(py - 1) * c.lumaW + px + i] : 0;
					left[i] = availLeft ? c.Y[(usize)(py + i) * c.lumaW + px - 1] : 0;
				}
				if (availTop && availLeft)
					tl = c.Y[(usize)(py - 1) * c.lumaW + px - 1];
				uint8 pred[256];
				Predict16x16(predMode, top, left, tl, availTop, availLeft, pred);

				const int32 chromaMode = (int32)br.UE();
				c.qp = (c.qp + br.SE() + 52) % 52; // mb_qp_delta (toujours présent en I_16x16)

				const int32 bx0 = mbX * 4, by0 = mbY * 4;

				// DC luma (16 coeffs) -> Hadamard inverse -> dequant DC.
				int32 dcRec[16] = {0};
				{
					const int32 nA = availLeft ? c.lumaNz[by0 * c.nzW + (bx0 - 1)] : -1;
					const int32 nB = availTop ? c.lumaNz[(by0 - 1) * c.nzW + bx0] : -1;
					const int32 nC = PredictNc(nA, nB);
					int32 dcScan[16], dcLvl[16];
					NkH264Cavlc::DecodeResidual(br, dcScan, 16, nC);
					InvScan16(dcScan, dcLvl);
					int32 gDc[16];
					NkH264Transform::HadamardInverse4x4(dcLvl, gDc);
					NkH264Transform::DequantDC(gDc, dcRec, 16, c.qp, c.W00(0));
				}

				// AC luma + reconstruction.
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
					int32 acRaster[16] = {0};
					int32 tc = 0;
					if (cbpLuma) {
						const int32 nA = (bx > 0) ? c.lumaNz[by * c.nzW + (bx - 1)] : -1;
						const int32 nB = (by > 0) ? c.lumaNz[(by - 1) * c.nzW + bx] : -1;
						const int32 nC = PredictNc(nA, nB);
						int32 acScan[15];
						tc = NkH264Cavlc::DecodeResidual(br, acScan, 15, nC);
						for (int32 k = 1; k < 16; ++k)
							acRaster[kZigZag[k]] = acScan[k - 1];
					}
					c.lumaNz[by * c.nzW + bx] = tc;

					int32 deq[16];
					NkH264Transform::Dequant4x4(acRaster, deq, c.qp, c.wsl4[0]);
					if (!cbpLuma)
						for (int32 k = 1; k < 16; ++k)
							deq[k] = 0;
					deq[0] = dcRec[(y4 / 4) * 4 + (x4 / 4)];
					int32 resRec[16];
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
								ClampU8(pred[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
				}

				// Le mode intra4x4 des voisins = DC(2) pour un MB I_16x16.
				for (int32 by = by0; by < by0 + 4; ++by)
					for (int32 bx = bx0; bx < bx0 + 4; ++bx)
						c.i4mode[by * c.nzW + bx] = 2;

				DecodeChroma(c, br, mbX, mbY, chromaMode, cbpChroma);
				return true;
			}

			// Grilles nz=0 + i4mode=DC d'un MB (skip / inter).
			void ClearMbNz(DecCtx &c, int32 mbX, int32 mbY) {
				const int32 bx0 = mbX * 4, by0 = mbY * 4;
				for (int32 by = by0; by < by0 + 4; ++by)
					for (int32 bx = bx0; bx < bx0 + 4; ++bx) {
						c.lumaNz[by * c.nzW + bx] = 0;
						c.i4mode[by * c.nzW + bx] = 2;
					}
				const int32 cbx0 = mbX * 2, cby0 = mbY * 2;
				for (int32 by = cby0; by < cby0 + 2; ++by)
					for (int32 bx = cbx0; bx < cbx0 + 2; ++bx) {
						c.chromaNz0[by * c.cnzW + bx] = 0;
						c.chromaNz1[by * c.cnzW + bx] = 0;
					}
			}

			// --- P_Skip : compensation au MV de skip, aucun résidu ---
			void DecodeMbSkip(DecCtx &c, int32 mbX, int32 mbY) {
				const RefList &L = c.L[0]; // P : une seule liste de references

				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				int32 smx, smy;
				SkipMv(c, mbX, mbY, smx, smy);
				uint8 predY[256], cp[64];
				McLumaRect(c, L, px, py, 0, 0, 16, 16, smx, smy, predY);
				for (int32 y = 0; y < 16; ++y)
					for (int32 x = 0; x < 16; ++x)
						c.Y[(usize)(py + y) * c.lumaW + px + x] = predY[y * 16 + x];
				McChromaRect(c, L, 0, cpx, cpy, 0, 0, 8, 8, smx, smy, cp);
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cb[(usize)(cpy + y) * c.chromaW + cpx + x] = cp[y * 8 + x];
				McChromaRect(c, L, 1, cpx, cpy, 0, 0, 8, 8, smx, smy, cp);
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cr[(usize)(cpy + y) * c.chromaW + cpx + x] = cp[y * 8 + x];
				ClearMbNz(c, mbX, mbY);
				StoreMv4(c, L, mbX * 4, mbY * 4, 4, 4, smx, smy, 0); // P_Skip -> référence 0
			}

			// --- Macrobloc inter P : partitions 16x16 / 16x8 / 8x16 / 8x8(+sous-mb) ---
			bool DecodeMbInterP(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 mbType) {
				const RefList &L = c.L[0]; // P : une seule liste de references

				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				const int32 gbx = mbX * 4, gby = mbY * 4;
				uint8 predY[256], cPred[2][64];

				// Compense une partition (bx4,by4 = coin 4x4 ; pw4/ph4 = taille 4x4) + stocke MV/refIdx.
				auto doPart = [&](int32 bx4, int32 by4, int32 pw4, int32 ph4, int32 mvx, int32 mvy, int32 ri) {
					McLumaRect(c, L, px + bx4 * 4, py + by4 * 4, bx4 * 4, by4 * 4, pw4 * 4, ph4 * 4, mvx, mvy, predY, ri);
					McChromaRect(c, L, 0, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[0], ri);
					McChromaRect(c, L, 1, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[1], ri);
					StoreMv4(c, L, gbx + bx4, gby + by4, pw4, ph4, mvx, mvy, ri);
				};

				// Lit un ref_idx_l0 (te(v)) : range = numRefActive-1. range==0 -> 0 (non codé) ;
				// range==1 -> 1 bit inversé ; range>1 -> ue(v). (H.264 §9.1.1)
				const bool readRef = (L.numRefActive > 1);
				auto readRefIdx = [&]() -> int32 {
					if (!readRef)
						return 0;
					if (L.numRefActive == 2)
						return 1 - (int32)br.U1();
					return (int32)br.UE();
				};

				if (mbType == 0) { // P_L0_16x16 : 1 ref_idx puis 1 mvd
					const int32 ri = readRefIdx();
					int32 pmx, pmy;
					PredMvPart(c, L, gbx, gby, 4, 0, 0, ri, pmx, pmy);
					doPart(0, 0, 4, 4, pmx + br.SE(), pmy + br.SE(), ri);
				} else if (mbType == 1) { // P_L0_L0_16x8 : ref_idx[0..1] puis mvd[0..1]
					const int32 ri0 = readRefIdx(), ri1 = readRefIdx();
					const int32 ri[2] = {ri0, ri1};
					for (int32 part = 0; part < 2; ++part) {
						const int32 by4 = part * 2;
						int32 pmx, pmy;
						PredMvPart(c, L, gbx, gby + by4, 4, 1, part, ri[part], pmx, pmy);
						doPart(0, by4, 4, 2, pmx + br.SE(), pmy + br.SE(), ri[part]);
					}
				} else if (mbType == 2) { // P_L0_L0_8x16 : ref_idx[0..1] puis mvd[0..1]
					const int32 ri0 = readRefIdx(), ri1 = readRefIdx();
					const int32 ri[2] = {ri0, ri1};
					for (int32 part = 0; part < 2; ++part) {
						const int32 bx4 = part * 2;
						int32 pmx, pmy;
						PredMvPart(c, L, gbx + bx4, gby, 2, 2, part, ri[part], pmx, pmy);
						doPart(bx4, 0, 2, 4, pmx + br.SE(), pmy + br.SE(), ri[part]);
					}
				} else { // P_8x8 (3) / P_8x8ref0 (4)
					int32 subType[4];
					for (int32 b = 0; b < 4; ++b)
						subType[b] = (int32)br.UE();
					// ref_idx_l0 : UN par partition 8x8 (ordre : tous avant les MVs). P_8x8ref0 -> toujours 0.
					int32 ri[4] = {0, 0, 0, 0};
					if (mbType == 3)
						for (int32 b = 0; b < 4; ++b)
							ri[b] = readRefIdx();
					for (int32 b = 0; b < 4; ++b) {
						const int32 sbx = (b & 1) * 2, sby = (b >> 1) * 2;
						int32 pmx, pmy;
						if (subType[b] == 0) { // 8x8
							PredMvPart(c, L, gbx + sbx, gby + sby, 2, 3, 0, ri[b], pmx, pmy);
							doPart(sbx, sby, 2, 2, pmx + br.SE(), pmy + br.SE(), ri[b]);
						} else if (subType[b] == 1) { // 8x4
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, L, gbx + sbx, gby + sby + sp, 2, 3, 0, ri[b], pmx, pmy);
								doPart(sbx, sby + sp, 2, 1, pmx + br.SE(), pmy + br.SE(), ri[b]);
							}
						} else if (subType[b] == 2) { // 4x8
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, L, gbx + sbx + sp, gby + sby, 1, 3, 0, ri[b], pmx, pmy);
								doPart(sbx + sp, sby, 1, 2, pmx + br.SE(), pmy + br.SE(), ri[b]);
							}
						} else { // 4x4
							for (int32 sp = 0; sp < 4; ++sp) {
								const int32 pbx = sbx + (sp & 1), pby = sby + (sp >> 1);
								PredMvPart(c, L, gbx + pbx, gby + pby, 1, 3, 0, ri[b], pmx, pmy);
								doPart(pbx, pby, 1, 1, pmx + br.SE(), pmy + br.SE(), ri[b]);
							}
						}
					}
				}

				const int32 codeNum = (int32)br.UE();
				if (codeNum >= 48)
					return false;
				const int32 cbp = kCbpInter[codeNum];
				const int32 lumaCbp = cbp & 15, cbpChroma = cbp >> 4;
				if (cbp > 0)
					c.qp = (c.qp + br.SE() + 52) % 52;
				// ⚠️ Une B pourrait lire cette image comme co-localisee (forme = granularite du test
				// "bloc immobile" de son Direct spatial). P_L0_16x16 -> 16x16 ; les autres -> non.
				if (c.mb16x16OrIntra)
					c.mb16x16OrIntra[mbY * c.mbW + mbX] = (mbType == 0) ? 1 : 0;

				const int32 bx0 = mbX * 4, by0 = mbY * 4;
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
					int32 raster[16] = {0};
					int32 tc = 0;
					if (lumaCbp & (1 << (blk >> 2))) {
						const int32 nA = (bx > 0) ? c.lumaNz[by * c.nzW + (bx - 1)] : -1;
						const int32 nB = (by > 0) ? c.lumaNz[(by - 1) * c.nzW + bx] : -1;
						const int32 nC = PredictNc(nA, nB);
						int32 scan[16];
						tc = NkH264Cavlc::DecodeResidual(br, scan, 16, nC);
						InvScan16(scan, raster);
					}
					c.lumaNz[by * c.nzW + bx] = tc;
					int32 deq[16], resRec[16];
					NkH264Transform::Dequant4x4(raster, deq, c.qp, c.wsl4[3]);
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
								ClampU8(predY[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
				}

				DecodeChromaResidual(c, br, mbX, mbY, cbpChroma, cPred, false);
				for (int32 by = by0; by < by0 + 4; ++by)
					for (int32 bx = bx0; bx < bx0 + 4; ++bx)
						c.i4mode[by * c.nzW + bx] = 2;
				return true;
			}

			// --- Déblocage (§8.7) : tables + filtres (copie de l'encodeur) ---
			const int32 kAlpha[52] = {0,   0,	0,	 0,	  0,   0,	0,	 0,	  0,   0,	0,	 0,	  0,	0,	 0,	  0,
									  4,   4,	5,	 6,	  7,   8,	9,	 10,  12,  13,	15,	 17,  20,	22,	 25,  28,
									  32,  36,	40,	 45,  50,  56,	63,	 71,  80,  90,	101, 113, 127, 144, 162, 182,
									  203, 226, 255, 255};
			const int32 kBeta[52] = {0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 2,	 2,
									 2,	 3,	 3,	 3,	 3,	 4,	 4,	 4,	 6,	 6,	 7,	 7,	 8,	 8,	 9,	 9,	 10, 10,
									 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18};
			// Table 8-18 (tc0 par indexA x bS 1..3) — EXTRAITE PROGRAMMATIQUEMENT de la référence
			// (l'ancienne transcription manuelle était décalée d'un cran à partir de indexA~21 :
			// ±1 de tc -> ±1 de delta au clamp -> résidu chroma/luma fin au déblocage).
			const int32 kTc0[52][3] = {
				{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
				{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
				{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
				{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
				{0, 0, 0}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1},
				{0, 0, 1}, {0, 1, 1}, {0, 1, 1}, {1, 1, 1},
				{1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 2},
				{1, 1, 2}, {1, 1, 2}, {1, 1, 2}, {1, 2, 3},
				{1, 2, 3}, {2, 2, 3}, {2, 2, 4}, {2, 3, 4},
				{2, 3, 4}, {3, 3, 5}, {3, 4, 6}, {3, 4, 6},
				{4, 5, 7}, {4, 5, 8}, {4, 6, 9}, {5, 7, 10},
				{6, 8, 11}, {6, 8, 13}, {7, 10, 14}, {8, 11, 16},
				{9, 12, 18}, {10, 13, 20}, {11, 15, 23}, {13, 17, 25}};

			void FiltLuma(uint8 *q, int32 d, int32 alpha, int32 beta, int32 bS, int32 tc0) {
				const int32 p0 = q[-d], p1 = q[-2 * d], p2 = q[-3 * d], p3 = q[-4 * d];
				const int32 Q0 = q[0], Q1 = q[d], Q2 = q[2 * d], Q3 = q[3 * d];
				const int32 dpq = p0 - Q0 < 0 ? Q0 - p0 : p0 - Q0;
				if (dpq >= alpha || (p1 - p0 < 0 ? p0 - p1 : p1 - p0) >= beta ||
					(Q1 - Q0 < 0 ? Q0 - Q1 : Q1 - Q0) >= beta)
					return;
				const int32 ap = p2 - p0 < 0 ? p0 - p2 : p2 - p0;
				const int32 aq = Q2 - Q0 < 0 ? Q0 - Q2 : Q2 - Q0;
				if (bS < 4) {
					const int32 tc = tc0 + (ap < beta ? 1 : 0) + (aq < beta ? 1 : 0);
					const int32 delta = Clamp(((Q0 - p0) * 4 + (p1 - Q1) + 4) >> 3, -tc, tc);
					q[-d] = ClampU8(p0 + delta);
					q[0] = ClampU8(Q0 - delta);
					if (ap < beta)
						q[-2 * d] = (uint8)(p1 + Clamp((p2 + ((p0 + Q0 + 1) >> 1) - 2 * p1) >> 1, -tc0, tc0));
					if (aq < beta)
						q[d] = (uint8)(Q1 + Clamp((Q2 + ((p0 + Q0 + 1) >> 1) - 2 * Q1) >> 1, -tc0, tc0));
				} else {
					const int32 thr = (alpha >> 2) + 2;
					if (ap < beta && dpq < thr) {
						q[-d] = (uint8)((p2 + 2 * p1 + 2 * p0 + 2 * Q0 + Q1 + 4) >> 3);
						q[-2 * d] = (uint8)((p2 + p1 + p0 + Q0 + 2) >> 2);
						q[-3 * d] = (uint8)((2 * p3 + 3 * p2 + p1 + p0 + Q0 + 4) >> 3);
					} else {
						q[-d] = (uint8)((2 * p1 + p0 + Q1 + 2) >> 2);
					}
					if (aq < beta && dpq < thr) {
						q[0] = (uint8)((Q2 + 2 * Q1 + 2 * Q0 + 2 * p0 + p1 + 4) >> 3);
						q[d] = (uint8)((Q2 + Q1 + Q0 + p0 + 2) >> 2);
						q[2 * d] = (uint8)((2 * Q3 + 3 * Q2 + Q1 + Q0 + p0 + 4) >> 3);
					} else {
						q[0] = (uint8)((2 * Q1 + Q0 + p1 + 2) >> 2);
					}
				}
			}

			void FiltChroma(uint8 *q, int32 d, int32 alpha, int32 beta, int32 bS, int32 tc0) {
				const int32 p1 = q[-2 * d], p0 = q[-d], Q0 = q[0], Q1 = q[d];
				if ((p0 - Q0 < 0 ? Q0 - p0 : p0 - Q0) >= alpha || (p1 - p0 < 0 ? p0 - p1 : p1 - p0) >= beta ||
					(Q1 - Q0 < 0 ? Q0 - Q1 : Q1 - Q0) >= beta)
					return;
				if (bS < 4) {
					const int32 tc = tc0 + 1;
					const int32 delta = Clamp(((Q0 - p0) * 4 + (p1 - Q1) + 4) >> 3, -tc, tc);
					q[-d] = ClampU8(p0 + delta);
					q[0] = ClampU8(Q0 - delta);
				} else {
					q[-d] = (uint8)((2 * p1 + p0 + Q1 + 2) >> 2);
					q[0] = (uint8)((2 * Q1 + Q0 + p1 + 2) >> 2);
				}
			}

			// Déblocage §8.7 (intra ET inter) : bS calculé par segment 4x4.
			//   intra impliqué -> 4 (bord MB) / 3 (interne) ; inter -> 2 (coeff nz) / 1 (Δmv≥1pel) / 0.
			void Deblock(DecCtx &c, const int32 *mbQp, int32 alphaOff, int32 betaOff, bool isB) {
				const int32 mbW = c.mbW, nzW = c.nzW;
				int32 alpha, beta, tc0;
				auto lumaEdge = [&](int32 qPav, int32 bS) {
					const int32 ia = Clamp(qPav + alphaOff, 0, 51), ib = Clamp(qPav + betaOff, 0, 51);
					alpha = kAlpha[ia];
					beta = kBeta[ib];
					tc0 = (bS < 4) ? kTc0[ia][bS - 1] : 0;
				};
				auto chromaQpOf = [&](int32 lumaQp) {
					return NkH264Transform::ChromaQp(Clamp(lumaQp + c.chromaQpOffset, 0, 51));
				};
				// Identifiant d'IMAGE (POC) de la liste l d'un bloc 4x4, ou -1 si la liste est inutilisee.
				// Deux blocs qui referencent la MEME image ont le meme POC, meme via des listes differentes.
				auto pocOf = [&](int32 l, int32 x, int32 y) -> int32 {
					const int32 r = c.L[l].ref4[y * nzW + x];
					return (r >= 0) ? c.L[l].poc[r] : -1;
				};
				// |Δmv| >= 4 (1 pel en quart-pel) entre deux blocs, pour un couple de listes (lq, lp).
				auto mvFar = [&](int32 lq, int32 qx, int32 qy, int32 lp, int32 px, int32 py) -> bool {
					const int32 dx = c.L[lq].mvx4[qy * nzW + qx] - c.L[lp].mvx4[py * nzW + px];
					const int32 dy = c.L[lq].mvy4[qy * nzW + qx] - c.L[lp].mvy4[py * nzW + px];
					return (dx <= -4 || dx >= 4) || (dy <= -4 || dy >= 4);
				};
				// Force de bord entre les blocs 4x4 q(qx,qy) et p(px,py) ; mbB = bord de MB. (§8.7.2.1)
				auto bsOf = [&](int32 qx, int32 qy, int32 px, int32 py, bool mbB) -> int32 {
					// Intra impliqué (une ref < 0 sur les DEUX listes = bloc intra/non decode).
					const bool qInter = c.L[0].ref4[qy * nzW + qx] >= 0 || c.L[1].ref4[qy * nzW + qx] >= 0;
					const bool pInter = c.L[0].ref4[py * nzW + px] >= 0 || c.L[1].ref4[py * nzW + px] >= 0;
					if (!qInter || !pInter)
						return mbB ? 4 : 3;
					if (c.lumaNz[qy * nzW + qx] > 0 || c.lumaNz[py * nzW + px] > 0)
						return 2;
					// Blocs inter, aucun coefficient : comparer refs (par IMAGE) + MV. (miroir de check_mv)
					const int32 q0 = pocOf(0, qx, qy), q1 = pocOf(1, qx, qy);
					const int32 p0 = pocOf(0, px, py), p1 = pocOf(1, px, py);
					// Appariement direct L0<->L0, L1<->L1.
					bool v = (q0 != p0);
					if (!v && q0 != -1)
						v = mvFar(0, qx, qy, 0, px, py);
					if (isB) {
						if (!v)
							v = (q1 != p1) || (q1 != -1 && mvFar(1, qx, qy, 1, px, py));
						if (v) {
							// L'appariement direct diffère : essayer l'appariement CROISE L0<->L1.
							if (q0 != p1 || q1 != p0)
								return 1;
							return (mvFar(0, qx, qy, 1, px, py) || mvFar(1, qx, qy, 0, px, py)) ? 1 : 0;
						}
					}
					return v ? 1 : 0;
				};

				for (int32 mbY = 0; mbY < c.mbH; ++mbY)
					for (int32 mbX = 0; mbX < mbW; ++mbX) {
						const int32 cur = mbY * mbW + mbX;
						const int32 qpQ = mbQp[cur];
						// MB en transformée 8x8 (profil High) : les arêtes internes 4x4 (e=1,3) ne sont PAS
						// filtrées — il n'y a pas de bord de transformée à x=4/12 ni y=4/12 (§8.7.1).
						const bool c8x8 = c.transform8x8Mb && c.transform8x8Mb[cur];
						// Luma bords verticaux (par segment de 4 lignes).
						for (int32 e = 0; e < 4; ++e) {
							if (e == 0 && mbX == 0)
								continue;
							if (c8x8 && (e & 1))
								continue;
							const int32 x = mbX * 16 + e * 4;
							const int32 qPav = (e == 0) ? ((mbQp[cur - 1] + qpQ + 1) >> 1) : qpQ;
							for (int32 seg = 0; seg < 4; ++seg) {
								const int32 qx = mbX * 4 + e, qy = mbY * 4 + seg;
								const int32 pxx = (e == 0) ? (mbX * 4 - 1) : (mbX * 4 + e - 1);
								const int32 bS = bsOf(qx, qy, pxx, qy, e == 0);
								if (!bS)
									continue;
								lumaEdge(qPav, bS);
								for (int32 r = 0; r < 4; ++r)
									FiltLuma(&c.Y[(usize)(mbY * 16 + seg * 4 + r) * c.lumaW + x], 1, alpha, beta, bS, tc0);
							}
						}
						// Luma bords horizontaux.
						for (int32 e = 0; e < 4; ++e) {
							if (e == 0 && mbY == 0)
								continue;
							if (c8x8 && (e & 1))
								continue;
							const int32 y = mbY * 16 + e * 4;
							const int32 qPav = (e == 0) ? ((mbQp[cur - mbW] + qpQ + 1) >> 1) : qpQ;
							for (int32 seg = 0; seg < 4; ++seg) {
								const int32 qx = mbX * 4 + seg, qy = mbY * 4 + e;
								const int32 pyy = (e == 0) ? (mbY * 4 - 1) : (mbY * 4 + e - 1);
								const int32 bS = bsOf(qx, qy, qx, pyy, e == 0);
								if (!bS)
									continue;
								lumaEdge(qPav, bS);
								for (int32 col = 0; col < 4; ++col)
									FiltLuma(&c.Y[(usize)y * c.lumaW + mbX * 16 + seg * 4 + col], c.lumaW, alpha, beta, bS,
											 tc0);
							}
						}
						// Chroma 4:2:0 : bords ec=0/1 (↔ luma e=0/2) ; bS repris du segment luma cr/2.
						// bS/α/β/tc0 sont dérivés de la grille LUMA → IDENTIQUES pour Cb et Cr, et constants
						// sur les 2 lignes chroma d'un segment. Calculés UNE fois par arête (au lieu de
						// 4×), puis appliqués aux deux plans (plans indépendants → même ordre par plan).
						const int32 qpcQ = chromaQpOf(qpQ);
						// Bords verticaux chroma.
						for (int32 ec = 0; ec < 2; ++ec) {
							if (ec == 0 && mbX == 0)
								continue;
							const int32 e = ec * 2, cx = mbX * 8 + ec * 4;
							const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - 1]) + qpcQ + 1) >> 1) : qpcQ;
							const int32 pxx = (e == 0) ? (mbX * 4 - 1) : (mbX * 4 + e - 1);
							for (int32 seg = 0; seg < 4; ++seg) {
								const int32 qx = mbX * 4 + e, qy = mbY * 4 + seg;
								const int32 bS = bsOf(qx, qy, pxx, qy, e == 0);
								if (!bS)
									continue;
								lumaEdge(qPavC, bS);
								for (int32 sub = 0; sub < 2; ++sub) {
									const int32 cr = seg * 2 + sub;
									FiltChroma(&c.Cb[(usize)(mbY * 8 + cr) * c.chromaW + cx], 1, alpha, beta, bS, tc0);
									FiltChroma(&c.Cr[(usize)(mbY * 8 + cr) * c.chromaW + cx], 1, alpha, beta, bS, tc0);
								}
							}
						}
						// Bords horizontaux chroma.
						for (int32 ec = 0; ec < 2; ++ec) {
							if (ec == 0 && mbY == 0)
								continue;
							const int32 e = ec * 2, cy = mbY * 8 + ec * 4;
							const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - mbW]) + qpcQ + 1) >> 1) : qpcQ;
							const int32 pyy = (e == 0) ? (mbY * 4 - 1) : (mbY * 4 + e - 1);
							for (int32 seg = 0; seg < 4; ++seg) {
								const int32 qx = mbX * 4 + seg, qy = mbY * 4 + e;
								const int32 bS = bsOf(qx, qy, qx, pyy, e == 0);
								if (!bS)
									continue;
								lumaEdge(qPavC, bS);
								for (int32 sub = 0; sub < 2; ++sub) {
									const int32 cc = seg * 2 + sub;
									FiltChroma(&c.Cb[(usize)cy * c.chromaW + mbX * 8 + cc], c.chromaW, alpha, beta, bS, tc0);
									FiltChroma(&c.Cr[(usize)cy * c.chromaW + mbX * 8 + cc], c.chromaW, alpha, beta, bS, tc0);
								}
							}
						}
					}
			}

			// ═══════════════════════════════════════════════════════════════════════
			// CABAC (profils Main/High) — brique C : couche macrobloc.
			// Réutilise Predict*/Inverse*/DecCtx ; ne remplace QUE l'entropie CAVLC.
			// Offsets ctxIdx = Table 9-11 du standard ; init = kCabacInitI/PB (brique B).
			// ═══════════════════════════════════════════════════════════════════════

			// Table 9-11 : ctxIdxOffset par élément de syntaxe (les indices utiles).
			enum : int32 {
				kCtxMbTypeSI = 0,		 // mb_type (SI) 0..2
				kCtxMbTypeI = 3,		 // mb_type (I) 3..10
				kCtxMbSkipP = 11,		 // mb_skip_flag (P/SP) 11..13
				kCtxMbTypePpre = 14,	 // mb_type (P) préfixe 14..16
				kCtxMbTypePsuf = 17,	 // mb_type (P) suffixe 17..20
				kCtxSubMbTypeP = 21,	 // sub_mb_type (P) 21..23
				kCtxMbSkipB = 24,		 // mb_skip_flag (B) 24..26 (= 11 + 13)
				kCtxMbTypeB = 27,		 // mb_type (B) prefixe 27..35
				kCtxMbTypeBIntra = 32,	 // mb_type (B) : echappement intra (intraSlice=0)
				kCtxSubMbTypeB = 36,	 // sub_mb_type (B) 36..39
				kCtxMvd0 = 40,			 // mvd_l0[][][0] (x) 40..46
				kCtxMvd1 = 47,			 // mvd_l0[][][1] (y) 47..53
				kCtxRefIdx = 54,		 // ref_idx_l0 54..59
				kCtxMbQpDelta = 60,		 // mb_qp_delta 60..63
				kCtxIntraChroma = 64,	 // intra_chroma_pred_mode 64..67
				kCtxPrevIntra4 = 68,	 // prev_intra4x4_pred_mode_flag 68
				kCtxRemIntra4 = 69,		 // rem_intra4x4_pred_mode 69
				kCtxCbpLuma = 73,		 // coded_block_pattern (luma) 73..76
				kCtxCbpChroma = 77,		 // coded_block_pattern (chroma) 77..84
				kCtxCbf = 85,			 // coded_block_flag 85..104 (+1012.. High)
				kCtxSig = 105,			 // significant_coeff_flag 105..165
				kCtxLast = 166,			 // last_significant_coeff_flag 166..226
				kCtxCoeffAbs = 227,		 // coeff_abs_level_minus1 227..275
				kCtxEndOfSlice = 276,	 // end_of_slice_flag (terminaison)
				// ── Profil High (transformée 8x8) ──
				kCtxTransform8x8 = 399,	 // transform_size_8x8_flag 399..401
				kCtxSig8x8 = 402,		 // significant_coeff_flag 8x8 (frame) 402..416 (15 ctx)
				kCtxLast8x8 = 417,		 // last_significant_coeff_flag 8x8 (frame) 417..425 (9 ctx)
				kCtxCoeffAbs8x8 = 426,	 // coeff_abs_level_minus1 8x8 426..435 (10 ctx)
			};

			// Décodeur CABAC au niveau macrobloc : moteur + 1024 contextes + grilles voisines.
			struct CabacMb {
					NkCabacEngine e;
					NkCabacCtx ctx[1024];
					// Grilles voisines (par MB) pour les ctxIdxInc dépendants du voisinage.
					NkVector<int32> mbTypeClass; // -1 indispo, 0 I_NxN, 1 I_16x16/inter (par MB)
					NkVector<int32> cbpLumaMb;	   // CodedBlockPatternLuma par MB (4 bits)
					NkVector<int32> cbpChromaMb;   // CodedBlockPatternChroma par MB
					NkVector<int32> chromaModeMb;  // intra_chroma_pred_mode par MB
					NkVector<int32> lumaDcCodedMb; // cbf luma DC (I_16x16) par MB, pour le contexte cbf DC
					NkVector<int32> chromaDcCodedMb; // cbf chroma DC par MB (bit0=Cb, bit1=Cr)
					NkVector<int32> mbSkipMb;	// 1 si le MB est P_Skip/B_Skip (ctx de mb_skip_flag)
					NkVector<int32> mbDirectMb; // 1 si le MB est B_Direct/B_Skip (ctx du 1er bin mb_type B)
					NkVector<int32> transform8x8Mb; // 1 si transform_size_8x8_flag (ctx du flag, §9.3.3.1.1.10)
					int32 mbW = 0;
					int32 prevQpDeltaNonZero = 0;

					void InitSlice(int32 nbMbW, int32 nbMbH, int32 sliceQp, bool isI, int32 cabacInitIdc) {
						mbW = nbMbW;
						const int32 n = nbMbW * nbMbH;
						mbTypeClass.Resize((uint64)n);
						cbpLumaMb.Resize((uint64)n);
						cbpChromaMb.Resize((uint64)n);
						chromaModeMb.Resize((uint64)n);
						lumaDcCodedMb.Resize((uint64)n);
						chromaDcCodedMb.Resize((uint64)n);
						mbSkipMb.Resize((uint64)n);
						mbDirectMb.Resize((uint64)n);
						transform8x8Mb.Resize((uint64)n);
						for (int32 i = 0; i < n; ++i) {
							mbTypeClass[(uint64)i] = -1;
							cbpLumaMb[(uint64)i] = 0;
							cbpChromaMb[(uint64)i] = 0;
							chromaModeMb[(uint64)i] = 0;
							lumaDcCodedMb[(uint64)i] = 0;
							chromaDcCodedMb[(uint64)i] = 0;
							mbSkipMb[(uint64)i] = 0;
							mbDirectMb[(uint64)i] = 0;
							transform8x8Mb[(uint64)i] = 0;
						}
						prevQpDeltaNonZero = 0;
						// Init des 1024 contextes depuis la table normative (I ou variante P/B).
						if (isI)
							NkCabacInitContexts(ctx, kCabacInitI, 1024, sliceQp);
						else
							NkCabacInitContexts(ctx, kCabacInitPB[cabacInitIdc], 1024, sliceQp);
					}

					// Décode un bin régulier au contexte ctxIdxOffset+ctxIdxInc.
					uint32 Bin(int32 ctxIdx) {
						return e.DecodeDecision(ctx[ctxIdx]);
					}
					uint32 Bypass() {
						return e.DecodeBypass();
					}
					uint32 Terminate() {
						return e.DecodeTerminate();
					}

					// Binarisation Unary tronquée (TU) de max cMax, contextes ctxIdxOffset+inc(bin).
					// incFn(binIdx) -> ctxIdxInc. Renvoie la valeur.
					template <typename F>
					int32 DecodeTU(int32 cMax, F incFn) {
						int32 v = 0;
						while (v < cMax) {
							if (Bin(incFn(v)) == 0)
								break;
							++v;
						}
						return v;
					}

					// Exp-Golomb d'ordre k EN CONTOURNEMENT (suffixe UEGk, §9.3.2.3).
					int32 DecodeBypassEGk(int32 k) {
						int32 value = 0;
						// Préfixe unaire (bits à 1) tant que le seuil est atteint.
						while (Bypass() == 1) {
							value += (1 << k);
							++k;
							if (k > 30)
								break;
						}
						// Suffixe : k bits.
						while (k-- > 0)
							value += (int32)(Bypass() << k);
						return value;
					}

					// Signe en contournement (§9.3.3.2.3) : renvoie +val ou -val selon le bit.
					int32 BypassSign(int32 val) {
						return Bypass() ? -val : val;
					}
			};

			// ── Decodeurs de syntaxe CABAC (miroir exact de la reference H.264) ───────

			// mb_type intra generique. `ctxBase` = 3 (I-slice, intraSlice=1) ou 17 (suffixe intra d'une
			// P-slice, intraSlice=0). ⚠️ Quand intraSlice=0, la reference REUTILISE certains contextes
			// (state[2] deux fois, state[3] deux fois) : ce n'est PAS un simple decalage d'offset.
			int32 DecodeMbTypeIntraCabac(CabacMb &cab, int32 ctxBase, int32 intraSlice, int32 mbX, int32 mbY) {
				int32 base = ctxBase;
				if (intraSlice) {
					const int32 idx = mbY * cab.mbW + mbX;
					int32 ctx = 0;
					const int32 clsA = (mbX > 0) ? cab.mbTypeClass[(uint64)(idx - 1)] : -1;
					const int32 clsB = (mbY > 0) ? cab.mbTypeClass[(uint64)(idx - cab.mbW)] : -1;
					if (clsA > 0)
						++ctx; // voisin gauche I_16x16/PCM (pas I_NxN, dispo)
					if (clsB > 0)
						++ctx;
					if (cab.Bin(base + ctx) == 0)
						return 0; // I_NxN
					base += 2;
				} else {
					if (cab.Bin(base) == 0)
						return 0; // I_NxN
				}
				if (cab.Terminate())
					return 25; // I_PCM
				int32 mbType = 1;
				mbType += 12 * (int32)cab.Bin(base + 1);					 // cbp_luma != 0
				if (cab.Bin(base + 2))										 // cbp_chroma present
					mbType += 4 + 4 * (int32)cab.Bin(base + 2 + intraSlice); // cbp_chroma == 2 ? 2 : 1
				mbType += 2 * (int32)cab.Bin(base + 3 + intraSlice);			 // pred mode (bit haut)
				mbType += (int32)cab.Bin(base + 3 + 2 * intraSlice);			 // pred mode (bit bas)
				return mbType;
			}

			// mb_type (I-slice) : 0=I_NxN(I_4x4) ; 1..24=I_16x16 ; 25=I_PCM. (ctxIdxOffset 3)
			int32 DecodeMbTypeICabac(CabacMb &cab, int32 mbX, int32 mbY) {
				return DecodeMbTypeIntraCabac(cab, kCtxMbTypeI, 1, mbX, mbY);
			}

			// prev_intra4x4_pred_mode_flag + rem_intra4x4_pred_mode -> mode intra 4x4 final.
			int32 DecodeIntra4x4ModeCabac(CabacMb &cab, int32 predMode) {
				if (cab.Bin(kCtxPrevIntra4)) // flag : utilise la prediction
					return predMode;
				int32 mode = (int32)cab.Bin(kCtxRemIntra4);
				mode += 2 * (int32)cab.Bin(kCtxRemIntra4);
				mode += 4 * (int32)cab.Bin(kCtxRemIntra4);
				return mode + (mode >= predMode ? 1 : 0);
			}

			// intra_chroma_pred_mode (0..3). (ctxIdxOffset 64)
			int32 DecodeChromaModeCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				int32 ctx = 0;
				if (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0 && cab.chromaModeMb[(uint64)(idx - 1)] != 0)
					++ctx;
				if (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0 &&
					cab.chromaModeMb[(uint64)(idx - cab.mbW)] != 0)
					++ctx;
				if (cab.Bin(kCtxIntraChroma + ctx) == 0)
					return 0;
				if (cab.Bin(kCtxIntraChroma + 3) == 0)
					return 1;
				if (cab.Bin(kCtxIntraChroma + 3) == 0)
					return 2;
				return 3;
			}

			// coded_block_pattern luma (4 bits, un par 8x8). (ctxIdxOffset 73)
			int32 DecodeCbpLumaCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				const int32 cbpA = (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0)
									   ? cab.cbpLumaMb[(uint64)(idx - 1)]
									   : 0x0F;
				const int32 cbpB = (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0)
									   ? cab.cbpLumaMb[(uint64)(idx - cab.mbW)]
									   : 0x0F;
				int32 cbp = 0, ctx;
				ctx = !(cbpA & 0x02) + 2 * !(cbpB & 0x04);
				cbp += (int32)cab.Bin(kCtxCbpLuma + ctx);
				ctx = !(cbp & 0x01) + 2 * !(cbpB & 0x08);
				cbp += (int32)cab.Bin(kCtxCbpLuma + ctx) << 1;
				ctx = !(cbpA & 0x08) + 2 * !(cbp & 0x01);
				cbp += (int32)cab.Bin(kCtxCbpLuma + ctx) << 2;
				ctx = !(cbp & 0x04) + 2 * !(cbp & 0x02);
				cbp += (int32)cab.Bin(kCtxCbpLuma + ctx) << 3;
				return cbp;
			}

			// coded_block_pattern chroma (0/1/2). (ctxIdxOffset 77)
			int32 DecodeCbpChromaCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				const int32 cbpA = (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0)
									   ? cab.cbpChromaMb[(uint64)(idx - 1)]
									   : 0;
				const int32 cbpB = (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0)
									   ? cab.cbpChromaMb[(uint64)(idx - cab.mbW)]
									   : 0;
				int32 ctx = 0;
				if (cbpA > 0)
					++ctx;
				if (cbpB > 0)
					ctx += 2;
				if (cab.Bin(kCtxCbpChroma + ctx) == 0)
					return 0;
				ctx = 4;
				if (cbpA == 2)
					++ctx;
				if (cbpB == 2)
					ctx += 2;
				return 1 + (int32)cab.Bin(kCtxCbpChroma + ctx);
			}

			// mb_qp_delta -> valeur signee. (ctxIdxOffset 60)
			int32 DecodeMbQpDeltaCabac(CabacMb &cab) {
				int32 ctx = cab.prevQpDeltaNonZero ? 1 : 0;
				if (cab.Bin(kCtxMbQpDelta + ctx) == 0) {
					cab.prevQpDeltaNonZero = 0;
					return 0;
				}
				int32 val = 1;
				ctx = 2;
				while (cab.Bin(kCtxMbQpDelta + ctx) && val < 128) {
					++val;
					ctx = 3;
				}
				cab.prevQpDeltaNonZero = 1;
				// Mapping unaire -> signe : 1,2,3,... -> +1,-1,+2,-2,...
				return (val & 1) ? ((val + 1) >> 1) : -(val >> 1);
			}

			// ── Syntaxe P-slice ───────────────────────────────────────────────────────

			// mb_skip_flag (P). ctxIdxInc = (A dispo && non-skip) + (B dispo && non-skip). (offset 11)
			int32 DecodeMbSkipCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				int32 ctx = 0;
				if (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0 && !cab.mbSkipMb[(uint64)(idx - 1)])
					++ctx;
				if (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0 &&
					!cab.mbSkipMb[(uint64)(idx - cab.mbW)])
					++ctx;
				return (int32)cab.Bin(kCtxMbSkipP + ctx);
			}

			// mb_type (P). Renvoie le type P 0..3 (0=P_L0_16x16, 1=P_16x8, 2=P_8x16, 3=P_8x8) ; si le MB
			// est en fait intra, pose isIntra=true et renvoie le mb_type intra (0..25). (offsets 14..17)
			int32 DecodeMbTypePCabac(CabacMb &cab, bool &isIntra) {
				isIntra = false;
				if (cab.Bin(kCtxMbTypePpre) == 0) {
					if (cab.Bin(kCtxMbTypePpre + 1) == 0)
						return 3 * (int32)cab.Bin(kCtxMbTypePpre + 2); // P_L0_16x16 (0) ou P_8x8 (3)
					return 2 - (int32)cab.Bin(kCtxMbTypePsuf);		  // P_8x16 (2) ou P_16x8 (1)
				}
				isIntra = true;
				return DecodeMbTypeIntraCabac(cab, kCtxMbTypePsuf, 0, 0, 0);
			}

			// ── Description d'un mb_type / sub_mb_type de B-slice ─────────────────────
			// Les B ont 23 mb_type et 13 sub_mb_type : chaque entree dit la GEOMETRIE des partitions
			// et, pour CHAQUE partition, de quelle(s) liste(s) elle predit (L0, L1 ou les deux = Bi).
			struct BPartInfo {
					int32 pw = 4, ph = 4; // taille de partition en blocs 4x4 (4x4=16x16, 4x2=16x8, 2x4=8x16)
					int32 nParts = 1;	  // 1, 2 ou 4
					bool direct = false;  // B_Direct (le mouvement est DEDUIT, rien n'est code)
					bool l0[2] = {false, false}; // la partition p predit-elle depuis L0 ?
					bool l1[2] = {false, false}; // ... depuis L1 ? (les deux = bi-prediction)
			};

			// Table des 23 mb_type B (miroir de ff_h264_b_mb_type_info). Indices 0..22.
			BPartInfo BMbTypeInfo(int32 t) {
				BPartInfo b;
				if (t == 0) { // B_Direct_16x16
					b.direct = true;
					b.l0[0] = b.l1[0] = true;
					return b;
				}
				if (t <= 3) { // 1=B_L0_16x16, 2=B_L1_16x16, 3=B_Bi_16x16
					b.l0[0] = (t == 1 || t == 3);
					b.l1[0] = (t == 2 || t == 3);
					return b;
				}
				if (t == 22) { // B_8x8 : les 4 sous-blocs portent leur propre sub_mb_type
					b.pw = b.ph = 2;
					b.nParts = 4;
					return b;
				}
				// 4..21 : deux partitions 16x8 (t pair) ou 8x16 (t impair), avec les 9 combinaisons
				// de directions (L0/L1/Bi) x (L0/L1/Bi) — cf. ff_h264_b_mb_type_info.
				static const int32 kDir[18][2] = {
					{0, 0}, {0, 0}, // 4,5   : L0_L0
					{1, 1}, {1, 1}, // 6,7   : L1_L1
					{0, 1}, {0, 1}, // 8,9   : L0_L1
					{1, 0}, {1, 0}, // 10,11 : L1_L0
					{0, 2}, {0, 2}, // 12,13 : L0_Bi
					{1, 2}, {1, 2}, // 14,15 : L1_Bi
					{2, 0}, {2, 0}, // 16,17 : Bi_L0
					{2, 1}, {2, 1}, // 18,19 : Bi_L1
					{2, 2}, {2, 2}, // 20,21 : Bi_Bi
				};
				const int32 k = t - 4;
				const bool horiz = ((t & 1) == 0); // pair -> 16x8, impair -> 8x16
				b.pw = horiz ? 4 : 2;
				b.ph = horiz ? 2 : 4;
				b.nParts = 2;
				for (int32 p = 0; p < 2; ++p) {
					const int32 d = kDir[k][p];
					b.l0[p] = (d == 0 || d == 2);
					b.l1[p] = (d == 1 || d == 2);
				}
				return b;
			}

			// Table des 13 sub_mb_type B (miroir de ff_h264_b_sub_mb_type_info). Indices 0..12.
			BPartInfo BSubMbTypeInfo(int32 t) {
				BPartInfo b;
				static const int32 kGeo[13][3] = {
					// {pw, ph, nParts} en blocs 4x4, dans un 8x8
					{2, 2, 1}, // 0  B_Direct_8x8
					{2, 2, 1}, // 1  B_L0_8x8
					{2, 2, 1}, // 2  B_L1_8x8
					{2, 2, 1}, // 3  B_Bi_8x8
					{2, 1, 2}, // 4  B_L0_8x4
					{1, 2, 2}, // 5  B_L0_4x8
					{2, 1, 2}, // 6  B_L1_8x4
					{1, 2, 2}, // 7  B_L1_4x8
					{2, 1, 2}, // 8  B_Bi_8x4
					{1, 2, 2}, // 9  B_Bi_4x8
					{1, 1, 4}, // 10 B_L0_4x4
					{1, 1, 4}, // 11 B_L1_4x4
					{1, 1, 4}, // 12 B_Bi_4x4
				};
				static const int32 kDir[13] = {2, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2}; // 0=L0,1=L1,2=Bi
				b.pw = kGeo[t][0];
				b.ph = kGeo[t][1];
				b.nParts = kGeo[t][2];
				b.direct = (t == 0);
				const int32 d = kDir[t];
				b.l0[0] = (d == 0 || d == 2);
				b.l1[0] = (d == 1 || d == 2);
				b.l0[1] = b.l0[0];
				b.l1[1] = b.l1[0];
				return b;
			}

			// mb_skip_flag (B) : meme derivation qu'en P mais offset 24 (= 11 + 13).
			int32 DecodeMbSkipBCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				int32 ctx = 0;
				if (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0 && !cab.mbSkipMb[(uint64)(idx - 1)])
					++ctx;
				if (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0 &&
					!cab.mbSkipMb[(uint64)(idx - cab.mbW)])
					++ctx;
				return (int32)cab.Bin(kCtxMbSkipB + ctx);
			}

			// mb_type (B). Renvoie 0..22 ; si le MB est intra, pose isIntra et renvoie le mb_type intra.
			// ctxIdxInc du 1er bin = nombre de voisins dispo qui ne sont PAS Direct. (offsets 27..35)
			int32 DecodeMbTypeBCabac(CabacMb &cab, int32 mbX, int32 mbY, bool &isIntra) {
				isIntra = false;
				const int32 idx = mbY * cab.mbW + mbX;
				int32 ctx = 0;
				if (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0 && !cab.mbDirectMb[(uint64)(idx - 1)])
					++ctx;
				if (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0 &&
					!cab.mbDirectMb[(uint64)(idx - cab.mbW)])
					++ctx;
				if (cab.Bin(kCtxMbTypeB + ctx) == 0)
					return 0; // B_Direct_16x16
				if (cab.Bin(kCtxMbTypeB + 3) == 0)
					return 1 + (int32)cab.Bin(kCtxMbTypeB + 5); // B_L0_16x16 / B_L1_16x16
				int32 bits = (int32)cab.Bin(kCtxMbTypeB + 4) << 3;
				bits += (int32)cab.Bin(kCtxMbTypeB + 5) << 2;
				bits += (int32)cab.Bin(kCtxMbTypeB + 5) << 1;
				bits += (int32)cab.Bin(kCtxMbTypeB + 5);
				if (bits < 8)
					return bits + 3; // B_Bi_16x16 .. B_L1_L0_16x8
				if (bits == 13) {
					isIntra = true;
					return DecodeMbTypeIntraCabac(cab, kCtxMbTypeBIntra, 0, 0, 0);
				}
				if (bits == 14)
					return 11; // B_L1_L0_8x16
				if (bits == 15)
					return 22; // B_8x8
				bits = (bits << 1) + (int32)cab.Bin(kCtxMbTypeB + 5);
				return bits - 4; // B_L0_Bi_* .. B_Bi_Bi_*
			}

			// sub_mb_type (B) : 0..12. (offsets 36..39)
			int32 DecodeSubMbTypeBCabac(CabacMb &cab) {
				if (cab.Bin(kCtxSubMbTypeB) == 0)
					return 0; // B_Direct_8x8
				if (cab.Bin(kCtxSubMbTypeB + 1) == 0)
					return 1 + (int32)cab.Bin(kCtxSubMbTypeB + 3); // B_L0_8x8 / B_L1_8x8
				int32 type = 3;
				if (cab.Bin(kCtxSubMbTypeB + 2)) {
					if (cab.Bin(kCtxSubMbTypeB + 3))
						return 11 + (int32)cab.Bin(kCtxSubMbTypeB + 3); // B_L1_4x4 / B_Bi_4x4
					type += 4;
				}
				type += 2 * (int32)cab.Bin(kCtxSubMbTypeB + 3);
				type += (int32)cab.Bin(kCtxSubMbTypeB + 3);
				return type;
			}

			// sub_mb_type (P) : 0=8x8, 1=8x4, 2=4x8, 3=4x4. (offset 21)
			// ⚠️ La polarite des bins n'est pas uniforme : bin(21)==1 -> 8x8 ; bin(22)==0 -> 8x4.
			int32 DecodeSubMbTypePCabac(CabacMb &cab) {
				if (cab.Bin(kCtxSubMbTypeP))
					return 0; // 8x8
				if (cab.Bin(kCtxSubMbTypeP + 1) == 0)
					return 1; // 8x4
				if (cab.Bin(kCtxSubMbTypeP + 2))
					return 2; // 4x8
				return 3;	  // 4x4
			}

			// ref_idx_l0. ctxIdxInc = (refA > 0) + 2*(refB > 0) ; unaire, puis ctx = (ctx>>2)+4. (offset 54)
			int32 DecodeRefIdxCabac(CabacMb &cab, const DecCtx &c, const RefList &L, int32 bx, int32 by,
									bool isB = false) {
				const int32 refA = (bx > 0) ? L.ref4[by * c.nzW + (bx - 1)] : -1;
				const int32 refB = (by > 0) ? L.ref4[(by - 1) * c.nzW + bx] : -1;
				// ⚠️ B-slices : un voisin DIRECT ne contribue PAS au contexte, meme si sa reference
				// est > 0 (§9.3.3.1.1.6, condition direct_cache de la reference). En P, direct4 est nul.
				const bool dirA = isB && c.direct4 && bx > 0 && c.direct4[by * c.nzW + (bx - 1)] != 0;
				const bool dirB = isB && c.direct4 && by > 0 && c.direct4[(by - 1) * c.nzW + bx] != 0;
				int32 ctx = 0;
				if (refA > 0 && !dirA)
					++ctx;
				if (refB > 0 && !dirB)
					ctx += 2;
				int32 ref = 0;
				while (cab.Bin(kCtxRefIdx + ctx)) {
					++ref;
					ctx = (ctx >> 2) + 4;
					if (ref >= 32)
						return -1;
				}
				return ref;
			}

			// Sentinelle de debordement de mvd (flux corrompu) — hors de toute plage de MV legale.
			const int32 kMvdOverflow = 0x7FFFFFFF;

			// mvd (une composante). `ctxBase` = 40 (x) ou 47 (y). amvd = |mvd_A| + |mvd_B| (grille 4x4).
			// Prefixe TU(9) puis echappement UEG3, signe en contournement. `absOut` = |mvd| borne a 70
			// (valeur a stocker dans la grille pour les voisins). (§9.3.3.1.1.7)
			int32 DecodeMvdCabac(CabacMb &cab, int32 ctxBase, int32 amvd, int32 &absOut) {
				if (cab.Bin(ctxBase + (amvd > 2 ? 1 : 0) + (amvd > 32 ? 1 : 0)) == 0) {
					absOut = 0;
					return 0;
				}
				int32 mvd = 1;
				int32 ctx = ctxBase + 3;
				while (mvd < 9 && cab.Bin(ctx)) {
					if (mvd < 4)
						++ctx;
					++mvd;
				}
				if (mvd >= 9) {
					int32 k = 3;
					while (cab.Bypass()) {
						mvd += 1 << k;
						++k;
						if (k > 24) {
							absOut = 70;
							return kMvdOverflow; // debordement -> flux invalide
						}
					}
					while (k--)
						mvd += (int32)cab.Bypass() << k;
					absOut = mvd < 70 ? mvd : 70;
				} else
					absOut = mvd;
				return cab.Bypass() ? -mvd : mvd;
			}

			// ── Brique D : residus CABAC (coded_block_flag + significativite + niveaux) ──
			// cat = ctxBlockCat (0=I16DC,1=I16AC,2=Luma4x4,3=ChromaDC,4=ChromaAC).
			// cbfA/cbfB = indicateurs "code" des blocs voisins (pour le contexte cbf).
			// scanMode : 0 = 16 coeffs (Luma4x4 / I16DC) ; 1 = 15 AC ; 2 = 4 chroma DC.
			// out[] (pre-mis a zero) recoit les niveaux BRUTS en ordre RASTER (dequant ensuite).
			// Renvoie le nombre de coefficients non nuls (0 si cbf=0).
			int32 DecodeResidualCabac(CabacMb &cab, int32 cat, int32 cbfA, int32 cbfB, int32 scanMode, int32 *out) {
				static const int32 cbfBase[5] = {85, 89, 93, 97, 101};
				static const int32 sigBase[5] = {105, 120, 134, 149, 152};
				static const int32 lastBase[5] = {166, 181, 195, 210, 213};
				static const int32 absBase[5] = {227, 237, 247, 257, 266};
				// coded_block_flag.
				const int32 cbfCtx = cbfBase[cat] + (cbfA > 0 ? 1 : 0) + (cbfB > 0 ? 2 : 0);
				if (cab.Bin(cbfCtx) == 0)
					return 0;
				const int32 maxCoeff = (scanMode == 2) ? 4 : (scanMode == 1) ? 15 : 16;
				const int32 sb = sigBase[cat], lb = lastBase[cat], ab = absBase[cat];
				// Carte de significativite (§9.3.3.1.3).
				int32 index[16];
				int32 coeffCount = 0;
				int32 last;
				for (last = 0; last < maxCoeff - 1; ++last) {
					if (cab.Bin(sb + last)) {
						index[coeffCount++] = last;
						if (cab.Bin(lb + last)) {
							last = maxCoeff;
							break;
						}
					}
				}
				if (last == maxCoeff - 1)
					index[coeffCount++] = maxCoeff - 1;
				// Niveaux (machine a etat node_ctx, §9.3.3.1.1.7 + UEGk k=0 pour l'echappement).
				static const int32 level1ctx[8] = {1, 2, 3, 4, 0, 0, 0, 0};
				static const int32 gt1ctx[8] = {5, 5, 5, 5, 6, 7, 8, 9};
				static const int32 trans0[8] = {1, 2, 3, 3, 4, 5, 6, 7};
				static const int32 trans1[8] = {4, 4, 4, 4, 5, 6, 7, 7};
				int32 nodeCtx = 0;
				for (int32 k = coeffCount - 1; k >= 0; --k) {
					const int32 sp = index[k];
					const int32 rp = (scanMode == 2) ? sp : (scanMode == 1) ? kZigZag[sp + 1] : kZigZag[sp];
					int32 level;
					if (cab.Bin(ab + level1ctx[nodeCtx]) == 0) {
						nodeCtx = trans0[nodeCtx];
						level = 1;
					} else {
						int32 coeffAbs = 2;
						const int32 gctx = ab + gt1ctx[nodeCtx];
						nodeCtx = trans1[nodeCtx];
						while (coeffAbs < 15 && cab.Bin(gctx))
							++coeffAbs;
						if (coeffAbs >= 15) { // echappement Exp-Golomb ordre 0
							int32 j = 0;
							while (cab.Bypass() && j < 23)
								++j;
							coeffAbs = 1;
							while (j-- > 0)
								coeffAbs += coeffAbs + (int32)cab.Bypass();
							coeffAbs += 14;
						}
						level = coeffAbs;
					}
					out[rp] = cab.BypassSign(level);
				}
				return coeffCount;
			}

			// transform_size_8x8_flag (§9.3.3.1.1.10). ctxIdxInc = condTermA + condTermB, où
			// condTermN = 1 si le MB voisin N est dispo ET a transform_size_8x8_flag == 1. (offset 399)
			int32 DecodeTransform8x8FlagCabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				int32 ctx = 0;
				if (mbX > 0 && cab.mbTypeClass[(uint64)(idx - 1)] >= 0 && cab.transform8x8Mb[(uint64)(idx - 1)])
					++ctx;
				if (mbY > 0 && cab.mbTypeClass[(uint64)(idx - cab.mbW)] >= 0 &&
					cab.transform8x8Mb[(uint64)(idx - cab.mbW)])
					++ctx;
				return (int32)cab.Bin(kCtxTransform8x8 + ctx);
			}

			// Residu LUMA 8x8 CABAC (ctxBlockCat 5, profil High). En 4:2:0 (ChromaArrayType != 3) il n'y a
			// PAS de coded_block_flag pour le bloc 8x8 (il est inféré à 1) : l'appelant n'entre ici que si le
			// bit CBP du 8x8 est armé. La carte de significativité 8x8 utilise un MAPPING NON-LINÉAIRE des
			// contextes (Table 9-43, seulement 15 contextes sig / 9 last pour 63 positions). Les niveaux
			// réutilisent la même machine node_ctx que le 4x4 (offset 426). out[64] pré-mis à zéro (raster).
			int32 DecodeResidualCabac8x8(CabacMb &cab, int32 *out) {
				// ctxIdxMap significant_coeff_flag 8x8 (frame), levelListIdx 0..62 (Table 9-43).
				static const int32 sig8[63] = {
					0, 1,	2,	3,	4,	5,	5,	4,	4,	3,	3,	4,	4,	4,	5,	5,	4,	4,	4,	4,	3,
					3, 6,	7,	7,	7,	8,	9,	10, 9,	8,	7,	7,	6,	11, 12, 13, 11, 6,	7,	8,	9,
					14, 10, 9,	8,	6,	11, 12, 13, 11, 6,	9,	14, 10, 9,	11, 12, 13, 11, 14, 10, 12};
				// ctxIdxMap last_significant_coeff_flag 8x8 (frame), levelListIdx 0..62 (9 contextes : 0..8).
				// ⚠️ Table EXACTE de ffmpeg (cabac.c) : 15 uns puis 16 DEUX (idx 16..31), pas le motif
				// "régulier" du texte de la spec — le décalage à idx 16 est la clé de la bit-exactness.
				static const int32 last8[63] = {
					0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4,
					4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8};
				const int32 sb = kCtxSig8x8, lb = kCtxLast8x8, ab = kCtxCoeffAbs8x8;
				int32 index[64];
				int32 coeffCount = 0;
				int32 last;
				for (last = 0; last < 63; ++last) {
					if (cab.Bin(sb + sig8[last])) {
						index[coeffCount++] = last;
						if (cab.Bin(lb + last8[last])) {
							last = 64;
							break;
						}
					}
				}
				if (last == 63)
					index[coeffCount++] = 63;
				static const int32 level1ctx[8] = {1, 2, 3, 4, 0, 0, 0, 0};
				static const int32 gt1ctx[8] = {5, 5, 5, 5, 6, 7, 8, 9};
				static const int32 trans0[8] = {1, 2, 3, 3, 4, 5, 6, 7};
				static const int32 trans1[8] = {4, 4, 4, 4, 5, 6, 7, 7};
				int32 nodeCtx = 0;
				for (int32 k = coeffCount - 1; k >= 0; --k) {
					const int32 rp = kZigZag8x8[index[k]];
					int32 level;
					if (cab.Bin(ab + level1ctx[nodeCtx]) == 0) {
						nodeCtx = trans0[nodeCtx];
						level = 1;
					} else {
						int32 coeffAbs = 2;
						const int32 gctx = ab + gt1ctx[nodeCtx];
						nodeCtx = trans1[nodeCtx];
						while (coeffAbs < 15 && cab.Bin(gctx))
							++coeffAbs;
						if (coeffAbs >= 15) { // echappement Exp-Golomb ordre 0
							int32 j = 0;
							while (cab.Bypass() && j < 23)
								++j;
							coeffAbs = 1;
							while (j-- > 0)
								coeffAbs += coeffAbs + (int32)cab.Bypass();
							coeffAbs += 14;
						}
						level = coeffAbs;
					}
					out[rp] = cab.BypassSign(level);
				}
				return coeffCount;
			}

			// Chroma CABAC : prediction (4 modes) + residus DC(cat3)/AC(cat4).
			// Residu chroma CABAC (DC + AC) pour une prediction cPred donnee (intra OU inter).
			// `cbfDef` = valeur cbf a prendre pour un voisin INDISPONIBLE : 1 si le MB courant est intra,
			// 0 s'il est inter (la reference derive ce defaut du TYPE DU MB COURANT, pas du voisin).
			// `chromaMode` : mode de prédiction chroma du MB (0..3) pour le cumul DPCM du transform
			// bypass §8.5.15 (1=H, 2=V) ; -1 pour un MB inter (jamais de cumul).
			void DecodeChromaResidualCabac(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY, int32 cbpChroma,
										   const uint8 cPred[2][64], int32 cbfDef, int32 chromaMode = -1) {
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				const int32 mbIdx = mbY * cab.mbW + mbX;
				const int32 qpC = NkH264Transform::ChromaQp(Clamp(c.qp + c.chromaQpOffset, 0, 51));
				// Scaling lists chroma : cbfDef == 1 <=> MB courant intra (listes 1/2), sinon inter (4/5).
				const int32 wCb = cbfDef ? 1 : 4, wCr = cbfDef ? 2 : 5;
				// DC chroma (cat 3, 4 coeffs 2x2 par composante).
				int32 cDcRec[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
				if (cbpChroma & 3) {
					for (int32 comp = 0; comp < 2; ++comp) {
						const int32 cbfA =
							(mbX > 0) ? ((cab.chromaDcCodedMb[(uint64)(mbIdx - 1)] >> comp) & 1) : cbfDef;
						const int32 cbfB =
							(mbY > 0) ? ((cab.chromaDcCodedMb[(uint64)(mbIdx - cab.mbW)] >> comp) & 1) : cbfDef;
						int32 dcRaster[4] = {0, 0, 0, 0};
						const int32 nz = DecodeResidualCabac(cab, 3, cbfA, cbfB, 2, dcRaster);
						if (nz > 0) {
							cab.chromaDcCodedMb[(uint64)mbIdx] |= (1 << comp);
							if (c.TsBypass()) { // lossless : les niveaux SONT les DC (ni Hadamard ni scale)
								for (int32 i = 0; i < 4; ++i)
									cDcRec[comp][i] = dcRaster[i];
							} else {
								int32 gdc[4];
								NkH264Transform::Hadamard2x2(dcRaster, gdc);
								NkH264Transform::DequantChromaDC(gdc, cDcRec[comp], qpC,
																 c.W00(comp == 0 ? wCb : wCr));
							}
						}
					}
				}
				// AC chroma (cat 4) + reconstruction.
				const int32 cbx0 = mbX * 2, cby0 = mbY * 2;
				const bool byp = c.TsBypass();
				for (int32 comp = 0; comp < 2; ++comp) {
					int32 *cnz = (comp == 0) ? c.chromaNz0 : c.chromaNz1;
					uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
					int32 resC[64]; // bypass : résidu du MB chroma ENTIER (le cumul DPCM traverse le 8x8)
					for (int32 blk = 0; blk < 4; ++blk) {
						const int32 bx4 = (blk & 1) * 4, by4 = (blk >> 1) * 4;
						const int32 bx = cbx0 + (blk & 1), by = cby0 + (blk >> 1);
						int32 acRaster[16] = {0};
						int32 tc = 0;
						if (cbpChroma & 2) {
							const int32 cbfA = (bx > 0) ? (cnz[by * c.cnzW + (bx - 1)] > 0 ? 1 : 0) : cbfDef;
							const int32 cbfB = (by > 0) ? (cnz[(by - 1) * c.cnzW + bx] > 0 ? 1 : 0) : cbfDef;
							tc = DecodeResidualCabac(cab, 4, cbfA, cbfB, 1, acRaster);
						}
						cnz[by * c.cnzW + bx] = tc;
						if (byp) { // lossless : niveaux directs (DC en 0), assemblés pour le cumul 8x8
							for (int32 k = 0; k < 16; ++k)
								resC[(by4 + k / 4) * 8 + bx4 + (k & 3)] = (cbpChroma & 2) ? acRaster[k] : 0;
							resC[by4 * 8 + bx4] = (cbpChroma & 3) ? cDcRec[comp][blk] : 0;
							continue;
						}
						int32 deq[16];
						int32 resRec[16];
						NkH264Transform::Dequant4x4(acRaster, deq, qpC, c.wsl4[comp == 0 ? wCb : wCr]);
						if (!(cbpChroma & 2))
							for (int32 k = 1; k < 16; ++k)
								deq[k] = 0;
						deq[0] = (cbpChroma & 3) ? cDcRec[comp][blk] : 0;
						NkH264Transform::Inverse4x4(deq, resRec);
						for (int32 r = 0; r < 4; ++r)
							for (int32 col = 0; col < 4; ++col) {
								const int32 p = cPred[comp][(by4 + r) * 8 + (bx4 + col)];
								rec[(usize)(cpy + by4 + r) * c.chromaW + cpx + bx4 + col] =
									ClampU8(p + resRec[r * 4 + col]);
							}
					}
					if (byp) {
						if (chromaMode == 2) { // vertical : cumul par colonne sur les 8 lignes
							for (int32 col = 0; col < 8; ++col)
								for (int32 r = 1; r < 8; ++r)
									resC[r * 8 + col] += resC[(r - 1) * 8 + col];
						} else if (chromaMode == 1) { // horizontal : cumul par ligne
							for (int32 r = 0; r < 8; ++r)
								for (int32 col = 1; col < 8; ++col)
									resC[r * 8 + col] += resC[r * 8 + col - 1];
						}
						for (int32 r = 0; r < 8; ++r)
							for (int32 col = 0; col < 8; ++col)
								rec[(usize)(cpy + r) * c.chromaW + cpx + col] =
									ClampU8((int32)cPred[comp][r * 8 + col] + resC[r * 8 + col]);
					}
				}
			}

			// Chroma d'un MB INTRA en CABAC : prediction 8x8 (4 modes) puis residu (defaut cbf voisin = 1).
			void DecodeChromaCabac(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY, int32 chromaMode, int32 cbpChroma) {
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				const bool avt = mbY > 0, avl = mbX > 0;
				uint8 cPred[2][64];
				for (int32 comp = 0; comp < 2; ++comp) {
					const uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
					int32 ct[8], cl[8], tl = 128;
					for (int32 i = 0; i < 8; ++i) {
						ct[i] = avt ? rec[(usize)(cpy - 1) * c.chromaW + cpx + i] : 0;
						cl[i] = avl ? rec[(usize)(cpy + i) * c.chromaW + cpx - 1] : 0;
					}
					if (avt && avl)
						tl = rec[(usize)(cpy - 1) * c.chromaW + cpx - 1];
					else if (avt)
						tl = ct[0];
					else if (avl)
						tl = cl[0];
					PredictChroma8x8(chromaMode, ct, cl, tl, avt, avl, cPred[comp]);
				}
				DecodeChromaResidualCabac(c, cab, mbX, mbY, cbpChroma, cPred, 1, chromaMode);
			}

			// Macrobloc I_4x4 en CABAC (miroir de DecodeMbI4x4 ; entropie CABAC).
			bool DecodeMbCabacI4x4(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 px = mbX * 16, py = mbY * 16;
				const bool availTop = mbY > 0, availLeft = mbX > 0;
				const bool availTrMb = (mbY > 0) && (mbX < c.mbW - 1);
				const int32 mbIdx = mbY * cab.mbW + mbX;

				int32 mode4[16];
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
					int32 predMode;
					if (bx == 0 || by == 0)
						predMode = 2;
					else {
						const int32 a = c.i4mode[by * c.nzW + (bx - 1)];
						const int32 b = c.i4mode[(by - 1) * c.nzW + bx];
						predMode = a < b ? a : b;
					}
					mode4[blk] = DecodeIntra4x4ModeCabac(cab, predMode);
					c.i4mode[by * c.nzW + bx] = mode4[blk];
				}

				const int32 chromaMode = DecodeChromaModeCabac(cab, mbX, mbY);
				const int32 lumaCbp = DecodeCbpLumaCabac(cab, mbX, mbY);
				const int32 cbpChroma = DecodeCbpChromaCabac(cab, mbX, mbY);
				// Grilles voisines (I_NxN).
				cab.mbTypeClass[(uint64)mbIdx] = 0;
				cab.cbpLumaMb[(uint64)mbIdx] = lumaCbp;
				cab.cbpChromaMb[(uint64)mbIdx] = cbpChroma;
				cab.chromaModeMb[(uint64)mbIdx] = chromaMode;
				if (lumaCbp || cbpChroma)
					c.qp = (c.qp + DecodeMbQpDeltaCabac(cab) + 52) % 52;
				else
					cab.prevQpDeltaNonZero = 0;

				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 X = px + x4, Y = py + y4;
					const bool avt = (y4 > 0) || availTop;
					const bool avl = (x4 > 0) || availLeft;
					const bool avtl = avt && avl;
					bool avtr;
					switch (kTrAvail[blk]) {
						case 2: avtr = true; break;
						case 1: avtr = availTop; break;
						case 3: avtr = availTrMb; break;
						default: avtr = false; break;
					}
					int32 t[8], l[4], tl;
					for (int32 i = 0; i < 4; ++i)
						t[i] = avt ? c.Y[(usize)(Y - 1) * c.lumaW + X + i] : 0;
					for (int32 i = 0; i < 4; ++i)
						t[4 + i] = (avt && avtr) ? c.Y[(usize)(Y - 1) * c.lumaW + X + 4 + i] : t[3];
					for (int32 j = 0; j < 4; ++j)
						l[j] = avl ? c.Y[(usize)(Y + j) * c.lumaW + X - 1] : 0;
					tl = avtl ? c.Y[(usize)(Y - 1) * c.lumaW + X - 1] : 0;

					uint8 pred[16];
					Predict4x4(mode4[blk], t, l, tl, avt, avl, pred);

					const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
					int32 raster[16] = {0};
					int32 tc = 0;
					if (lumaCbp & (1 << (blk >> 2))) {
						const int32 cbfA = (bx > 0) ? (c.lumaNz[by * c.nzW + (bx - 1)] > 0 ? 1 : 0) : 1;
						const int32 cbfB = (by > 0) ? (c.lumaNz[(by - 1) * c.nzW + bx] > 0 ? 1 : 0) : 1;
						tc = DecodeResidualCabac(cab, 2, cbfA, cbfB, 0, raster);
					}
					c.lumaNz[by * c.nzW + bx] = tc;
					int32 deq[16], resRec[16];
					if (c.TsBypass()) {
						// Lossless §8.5.15 : les niveaux SONT les résidus (ni déquant ni IDCT) ; en mode
						// vertical (0) / horizontal (1), le résidu se CUMULE dans la direction de prédiction
						// (chaque ligne/colonne est prédite de la précédente RECONSTRUITE — DPCM).
						for (int32 i = 0; i < 16; ++i)
							resRec[i] = raster[i];
						if (mode4[blk] == 0) {
							for (int32 col = 0; col < 4; ++col)
								for (int32 r = 1; r < 4; ++r)
									resRec[r * 4 + col] += resRec[(r - 1) * 4 + col];
						} else if (mode4[blk] == 1) {
							for (int32 r = 0; r < 4; ++r)
								for (int32 col = 1; col < 4; ++col)
									resRec[r * 4 + col] += resRec[r * 4 + col - 1];
						}
					} else {
						NkH264Transform::Dequant4x4(raster, deq, c.qp, c.wsl4[0]);
						NkH264Transform::Inverse4x4(deq, resRec);
					}
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(Y + r) * c.lumaW + X + col] = ClampU8(pred[r * 4 + col] + resRec[r * 4 + col]);
				}

				DecodeChromaCabac(c, cab, mbX, mbY, chromaMode, cbpChroma);
				return true;
			}

			// Macrobloc I_8x8 en CABAC (profil High). Comme I_4x4 mais 4 blocs 8x8 : prédiction Intra_8x8
			// (9 modes + filtrage des références), résidu 8x8 (ctxBlockCat 5), transformée inverse 8x8.
			// transform_size_8x8_flag a DÉJÀ été décodé et vaut 1 (par l'appelant DecodeMbCabacINxN).
			bool DecodeMbCabacI8x8(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY) {
				if (c.TsBypass())
					return false; // lossless 8x8 (cumul DPCM 8x8) non géré — échec propre
				const int32 px = mbX * 16, py = mbY * 16;
				const bool availTop = mbY > 0, availLeft = mbX > 0;
				const bool availTrMb = (mbY > 0) && (mbX < c.mbW - 1);
				const int32 mbIdx = mbY * cab.mbW + mbX;

				// Modes des 4 blocs 8x8 (prédits depuis les voisins, grille i4mode répliquée par 2x2).
				int32 mode8[4];
				for (int32 i8 = 0; i8 < 4; ++i8) {
					const int32 bx = mbX * 4 + (i8 & 1) * 2, by = mbY * 4 + (i8 >> 1) * 2;
					int32 predMode;
					if (bx == 0 || by == 0)
						predMode = 2;
					else {
						const int32 a = c.i4mode[by * c.nzW + (bx - 1)];
						const int32 b = c.i4mode[(by - 1) * c.nzW + bx];
						predMode = a < b ? a : b;
					}
					mode8[i8] = DecodeIntra4x4ModeCabac(cab, predMode); // prev/rem partagent les ctx 68/69
					// Réplique le mode sur les 4 cellules 4x4 couvertes (pour la prédiction des voisins).
					for (int32 dy = 0; dy < 2; ++dy)
						for (int32 dx = 0; dx < 2; ++dx)
							c.i4mode[(by + dy) * c.nzW + (bx + dx)] = mode8[i8];
				}

				const int32 chromaMode = DecodeChromaModeCabac(cab, mbX, mbY);
				const int32 lumaCbp = DecodeCbpLumaCabac(cab, mbX, mbY);
				const int32 cbpChroma = DecodeCbpChromaCabac(cab, mbX, mbY);
				cab.mbTypeClass[(uint64)mbIdx] = 0;
				cab.cbpLumaMb[(uint64)mbIdx] = lumaCbp;
				cab.cbpChromaMb[(uint64)mbIdx] = cbpChroma;
				cab.chromaModeMb[(uint64)mbIdx] = chromaMode;
				if (lumaCbp || cbpChroma)
					c.qp = (c.qp + DecodeMbQpDeltaCabac(cab) + 52) % 52;
				else
					cab.prevQpDeltaNonZero = 0;

				for (int32 i8 = 0; i8 < 4; ++i8) {
					const int32 bx8 = (i8 & 1) * 8, by8 = (i8 >> 1) * 8;
					const int32 X = px + bx8, Y = py + by8;
					const bool avt = (by8 > 0) || availTop;
					const bool avl = (bx8 > 0) || availLeft;
					const bool avtl = avt && avl;
					// Disponibilité du haut-droit de ce bloc 8x8 (Predict8x8 étend top[7] sinon).
					bool avtr;
					switch (i8) {
						case 0: avtr = availTop; break;   // rangée au-dessus du MB
						case 1: avtr = availTrMb; break;  // MB en haut à droite
						case 2: avtr = true; break;		  // bloc 1 (déjà reconstruit)
						default: avtr = false; break;	  // MB de droite, non décodé
					}
					int32 top0[16], left0[8], tl;
					for (int32 i = 0; i < 8; ++i)
						top0[i] = avt ? (int32)c.Y[(usize)(Y - 1) * c.lumaW + X + i] : 0;
					for (int32 i = 8; i < 16; ++i)
						top0[i] = (avt && avtr) ? (int32)c.Y[(usize)(Y - 1) * c.lumaW + X + i] : top0[7];
					for (int32 j = 0; j < 8; ++j)
						left0[j] = avl ? (int32)c.Y[(usize)(Y + j) * c.lumaW + X - 1] : 0;
					tl = avtl ? (int32)c.Y[(usize)(Y - 1) * c.lumaW + X - 1] : 0;

					uint8 pred[64];
					Predict8x8(mode8[i8], top0, tl, left0, avt, avl, avtr, pred);

					const int32 bx4 = mbX * 4 + (i8 & 1) * 2, by4 = mbY * 4 + (i8 >> 1) * 2;
					int32 raster[64] = {0};
					int32 tc = 0;
					if (lumaCbp & (1 << i8)) // pas de coded_block_flag en 4:2:0 (inféré à 1)
						tc = DecodeResidualCabac8x8(cab, raster);
					// Réplique le nnz du 8x8 sur les 4 cellules 4x4 (contexte cbf des voisins + déblocage).
					for (int32 dy = 0; dy < 2; ++dy)
						for (int32 dx = 0; dx < 2; ++dx)
							c.lumaNz[(by4 + dy) * c.nzW + (bx4 + dx)] = tc;
					int32 deq[64], resRec[64];
					NkH264Transform::Dequant8x8(raster, deq, c.qp, c.wsl8[0]);
					NkH264Transform::Inverse8x8(deq, resRec);
					for (int32 r = 0; r < 8; ++r)
						for (int32 col = 0; col < 8; ++col)
							c.Y[(usize)(Y + r) * c.lumaW + X + col] =
								ClampU8((int32)pred[r * 8 + col] + resRec[r * 8 + col]);
				}

				DecodeChromaCabac(c, cab, mbX, mbY, chromaMode, cbpChroma);
				return true;
			}

			// Macrobloc I_PCM en CABAC (§7.3.5 + §9.3.1) : échantillons BRUTS non compressés.
			// Le bin terminate (déjà décodé par DecodeMbTypeIntraCabac) laisse bytePos/bitPos de notre
			// moteur bit-à-bit EXACTEMENT après le flush arithmétique de l'encodeur (NOTE §9.3.3.2.4 :
			// le dernier bit inséré dans codIOffset est le dernier bit du codeword — pas de lookahead à
			// compenser, contrairement à ffmpeg qui recule son pointeur selon l'état de `low`).
			// On aligne à l'octet (pcm_alignment_zero_bit), on copie 256+64+64 octets, puis on
			// RÉ-INITIALISE le moteur CABAC à l'octet suivant (§9.3.1.2) — les contextes, eux, survivent.
			bool DecodeMbCabacIPcm(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY) {
				NkCabacEngine &e = cab.e;
				usize pos = e.bytePos;
				if (e.bitPos != 0)
					++pos; // pcm_alignment_zero_bit(s) jusqu'à l'octet suivant
				if (pos + 384 > e.size)
					return false; // flux tronqué
				const uint8 *p = e.data + pos;
				const int32 px = mbX * 16, py = mbY * 16;
				for (int32 y = 0; y < 16; ++y)
					for (int32 x = 0; x < 16; ++x)
						c.Y[(usize)(py + y) * c.lumaW + px + x] = *p++;
				const int32 cx = mbX * 8, cy = mbY * 8;
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cb[(usize)(cy + y) * c.chromaW + cx + x] = *p++;
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cr[(usize)(cy + y) * c.chromaW + cx + x] = *p++;
				pos += 384;
				e.InitEngine(e.data, e.size, pos);
				// Grilles voisines — conventions de la référence : TOUT est "codé" (nnz=16, cbp plein),
				// pas I_NxN (classe 1), mode chroma 0, transformée 4x4.
				const int32 mbIdx = mbY * cab.mbW + mbX;
				cab.mbTypeClass[(uint64)mbIdx] = 1;
				cab.cbpLumaMb[(uint64)mbIdx] = 0xF;
				cab.cbpChromaMb[(uint64)mbIdx] = 2;
				cab.chromaModeMb[(uint64)mbIdx] = 0;
				cab.lumaDcCodedMb[(uint64)mbIdx] = 1;
				cab.chromaDcCodedMb[(uint64)mbIdx] = 3;
				cab.mbSkipMb[(uint64)mbIdx] = 0;
				cab.mbDirectMb[(uint64)mbIdx] = 0;
				cab.transform8x8Mb[(uint64)mbIdx] = 0;
				cab.prevQpDeltaNonZero = 0;
				if (c.transform8x8Mb)
					c.transform8x8Mb[mbIdx] = 0;
				for (int32 y = 0; y < 4; ++y)
					for (int32 x = 0; x < 4; ++x) {
						c.lumaNz[(mbY * 4 + y) * c.nzW + mbX * 4 + x] = 16;
						c.i4mode[(mbY * 4 + y) * c.nzW + mbX * 4 + x] = 2; // DC pour la prédiction voisine
					}
				for (int32 y = 0; y < 2; ++y)
					for (int32 x = 0; x < 2; ++x) {
						c.chromaNz0[(mbY * 2 + y) * c.cnzW + mbX * 2 + x] = 16;
						c.chromaNz1[(mbY * 2 + y) * c.cnzW + mbX * 2 + x] = 16;
					}
				return true;
			}

			// Dispatch I_NxN CABAC : décode transform_size_8x8_flag (si activé au PPS) puis route vers
			// le chemin 4x4 (transformée entière) ou 8x8 (profil High).
			bool DecodeMbCabacINxN(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 mbIdx = mbY * cab.mbW + mbX;
				int32 t8 = 0;
				if (c.transform8x8Mode)
					t8 = DecodeTransform8x8FlagCabac(cab, mbX, mbY);
				cab.transform8x8Mb[(uint64)mbIdx] = t8;
				if (c.transform8x8Mb)
					c.transform8x8Mb[mbIdx] = (uint8)t8; // règle de déblocage High (arêtes internes 4x4)
				return t8 ? DecodeMbCabacI8x8(c, cab, mbX, mbY) : DecodeMbCabacI4x4(c, cab, mbX, mbY);
			}

			// Macrobloc I_16x16 en CABAC (miroir de DecodeMbI16x16).
			bool DecodeMbCabacI16x16(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY, int32 mbType) {
				const int32 px = mbX * 16, py = mbY * 16;
				const int32 predMode = (mbType - 1) % 4;
				const int32 cbpChroma = ((mbType - 1) / 4) % 3;
				const int32 cbpLuma = ((mbType - 1) >= 12) ? 15 : 0;
				const bool availTop = mbY > 0, availLeft = mbX > 0;
				const int32 mbIdx = mbY * cab.mbW + mbX;

				int32 top[16], left[16], tl = 128;
				for (int32 i = 0; i < 16; ++i) {
					top[i] = availTop ? c.Y[(usize)(py - 1) * c.lumaW + px + i] : 0;
					left[i] = availLeft ? c.Y[(usize)(py + i) * c.lumaW + px - 1] : 0;
				}
				if (availTop && availLeft)
					tl = c.Y[(usize)(py - 1) * c.lumaW + px - 1];
				uint8 pred[256];
				Predict16x16(predMode, top, left, tl, availTop, availLeft, pred);

				const int32 chromaMode = DecodeChromaModeCabac(cab, mbX, mbY);
				c.qp = (c.qp + DecodeMbQpDeltaCabac(cab) + 52) % 52; // toujours present en I_16x16
				// Grilles voisines (classe 1 = I_16x16).
				cab.mbTypeClass[(uint64)mbIdx] = 1;
				cab.cbpLumaMb[(uint64)mbIdx] = cbpLuma;
				cab.cbpChromaMb[(uint64)mbIdx] = cbpChroma;
				cab.chromaModeMb[(uint64)mbIdx] = chromaMode;

				const int32 bx0 = mbX * 4, by0 = mbY * 4;
				// DC luma (cat 0).
				int32 dcRec[16] = {0};
				{
					const int32 cbfA = (mbX > 0) ? cab.lumaDcCodedMb[(uint64)(mbIdx - 1)] : 1;
					const int32 cbfB = (mbY > 0) ? cab.lumaDcCodedMb[(uint64)(mbIdx - cab.mbW)] : 1;
					int32 dcLvl[16] = {0};
					const int32 nz = DecodeResidualCabac(cab, 0, cbfA, cbfB, 0, dcLvl);
					if (nz > 0)
						cab.lumaDcCodedMb[(uint64)mbIdx] = 1;
					if (c.TsBypass()) { // lossless : les niveaux SONT les DC (ni Hadamard ni scale)
						for (int32 i = 0; i < 16; ++i)
							dcRec[i] = dcLvl[i];
					} else {
						int32 gDc[16];
						NkH264Transform::HadamardInverse4x4(dcLvl, gDc);
						NkH264Transform::DequantDC(gDc, dcRec, 16, c.qp, c.W00(0));
					}
				}

				// AC luma (cat 1) + reconstruction. En bypass (§8.5.15) le cumul DPCM V/H opère sur le
				// résidu du MB ENTIER (chaque ligne/colonne prédite de la précédente reconstruite) :
				// on assemble d'abord les 256 résidus, on cumule, puis on ajoute la prédiction.
				const bool byp = c.TsBypass();
				int32 res16[256];
				for (int32 blk = 0; blk < 16; ++blk) {
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
					int32 acRaster[16] = {0};
					int32 tc = 0;
					if (cbpLuma) {
						const int32 cbfA = (bx > 0) ? (c.lumaNz[by * c.nzW + (bx - 1)] > 0 ? 1 : 0) : 1;
						const int32 cbfB = (by > 0) ? (c.lumaNz[(by - 1) * c.nzW + bx] > 0 ? 1 : 0) : 1;
						tc = DecodeResidualCabac(cab, 1, cbfA, cbfB, 1, acRaster);
					}
					c.lumaNz[by * c.nzW + bx] = tc;
					if (byp) { // résidu direct : AC (jamais de coeff en 0) + DC en position 0 du bloc
						for (int32 k = 0; k < 16; ++k)
							res16[(y4 + k / 4) * 16 + x4 + (k & 3)] = cbpLuma ? acRaster[k] : 0;
						res16[y4 * 16 + x4] = dcRec[(y4 / 4) * 4 + (x4 / 4)];
						continue;
					}
					int32 deq[16];
					NkH264Transform::Dequant4x4(acRaster, deq, c.qp, c.wsl4[0]);
					if (!cbpLuma)
						for (int32 k = 1; k < 16; ++k)
							deq[k] = 0;
					deq[0] = dcRec[(y4 / 4) * 4 + (x4 / 4)];
					int32 resRec[16];
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
								ClampU8(pred[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
				}
				if (byp) {
					if (predMode == 0) { // vertical : cumul par colonne sur les 16 lignes
						for (int32 col = 0; col < 16; ++col)
							for (int32 r = 1; r < 16; ++r)
								res16[r * 16 + col] += res16[(r - 1) * 16 + col];
					} else if (predMode == 1) { // horizontal : cumul par ligne
						for (int32 r = 0; r < 16; ++r)
							for (int32 col = 1; col < 16; ++col)
								res16[r * 16 + col] += res16[r * 16 + col - 1];
					}
					for (int32 r = 0; r < 16; ++r)
						for (int32 col = 0; col < 16; ++col)
							c.Y[(usize)(py + r) * c.lumaW + px + col] =
								ClampU8((int32)pred[r * 16 + col] + res16[r * 16 + col]);
				}

				DecodeChromaCabac(c, cab, mbX, mbY, chromaMode, cbpChroma);
				return true;
			}

			// ── B_Direct SPATIAL (§8.4.1.2.2) ─────────────────────────────────────────
			// Rien n'est code : le mouvement se DEDUIT. Deux ingredients :
			//  1) refs/MV pris des VOISINS du MB courant (comme une prediction de MV classique) ;
			//  2) l'image CO-LOCALISEE (RefPicList1[0]) : si le bloc y est "immobile" (reference 0 et
			//     |MV| <= 1), on FORCE la MV a 0 pour la liste dont la reference vaut 0 ("col_zero").
			// Sorties : ref[2] (-1 = liste inutilisee), mv[2][2], zero8[4] = col_zero par 8x8.
			void PredDirectSpatial(const DecCtx &c, int32 mbX, int32 mbY, int32 ref[2], int32 mv[2][2],
								   bool zero8[4]) {
				const int32 bx = mbX * 4, by = mbY * 4;
				// 1) ref = min des voisins A (gauche), B (haut), C (haut-droite, sinon haut-gauche).
				for (int32 l = 0; l < 2; ++l) {
					const RefList &L = c.L[l];
					bool avA, avB, avC;
					int32 rA, rB, rC, ax, ay, bxx, byy, cx, cy;
					GetMv4(c, L, bx - 1, by, avA, rA, ax, ay);
					GetMv4(c, L, bx, by - 1, avB, rB, bxx, byy);
					GetMv4(c, L, bx + 4, by - 1, avC, rC, cx, cy);
					if (!avC) // C indisponible -> D (haut-gauche)
						GetMv4(c, L, bx - 1, by - 1, avC, rC, cx, cy);
					// Le minimum se prend en NON SIGNE (astuce de la reference) : toute reference >= 0
					// l'emporte sur les indisponibles/intra (negatives), qui deviennent enormes.
					const uint32 uA = (uint32)rA, uB = (uint32)rB, uC = (uint32)rC;
					uint32 um = uA < uB ? uA : uB;
					um = um < uC ? um : uC;
					const int32 r = (int32)um;
					if (r >= 0) {
						ref[l] = r;
						const int32 match = (rA == r ? 1 : 0) + (rB == r ? 1 : 0) + (rC == r ? 1 : 0);
						if (match > 1) { // cas courant : mediane des trois
							mv[l][0] = Med3(ax, bxx, cx);
							mv[l][1] = Med3(ay, byy, cy);
						} else if (rA == r) {
							mv[l][0] = ax;
							mv[l][1] = ay;
						} else if (rB == r) {
							mv[l][0] = bxx;
							mv[l][1] = byy;
						} else {
							mv[l][0] = cx;
							mv[l][1] = cy;
						}
					} else {
						ref[l] = -1;
						mv[l][0] = mv[l][1] = 0;
					}
				}
				// Aucun voisin utilisable sur AUCUNE liste -> bi-prediction sur la reference 0, MV nulle.
				if (ref[0] < 0 && ref[1] < 0) {
					ref[0] = ref[1] = 0;
					mv[0][0] = mv[0][1] = mv[1][0] = mv[1][1] = 0;
				}
				// 2) col_zero : le bloc co-localise est-il "immobile" ? Granularite = tout le MB si la
				//    co-localisee est 16x16/intra, sinon par 8x8.
				for (int32 i = 0; i < 4; ++i)
					zero8[i] = false;
				if (!c.colL0Ref)
					return; // pas de co-localisee exploitable
				const int32 mbIdx = mbY * c.mbW + mbX;
				const bool whole = c.col16x16OrIntra && c.col16x16OrIntra[mbIdx] != 0;
				for (int32 i8 = 0; i8 < 4; ++i8) {
					// direct_8x8_inference : la MV de la co-localisee s'echantillonne au COIN du 8x8.
					const int32 sx = whole ? bx : (bx + (i8 & 1) * 3);
					const int32 sy = whole ? by : (by + (i8 >> 1) * 3);
					const int32 ci = sy * c.nzW + sx;
					const int32 r0 = c.colL0Ref[ci], r1 = c.colL1Ref[ci];
					if (!(r0 < 0 && r1 < 0)) { // co-localisee INTRA -> jamais "immobile"
						int32 cmx = 0, cmy = 0;
						bool cand = false;
						if (r0 == 0) {
							cmx = c.colL0x[ci];
							cmy = c.colL0y[ci];
							cand = true;
						} else if (r0 < 0 && r1 == 0) {
							// N'utilise pas L0 mais L1 avec la reference 0.
							cmx = c.colL1x[ci];
							cmy = c.colL1y[ci];
							cand = true;
						}
						if (cand && cmx >= -1 && cmx <= 1 && cmy >= -1 && cmy <= 1)
							zero8[i8] = true;
					}
					if (whole) { // un seul test : il vaut pour tout le MB
						zero8[1] = zero8[2] = zero8[3] = zero8[0];
						break;
					}
				}
			}

			// Predit UNE partition d'un MB B dans predY/cPred : depuis L0 seule, L1 seule, ou les DEUX
			// (bi-prediction). (bx4,by4) = coin en blocs 4x4 dans le MB ; pw4/ph4 = taille en 4x4.
			// Regles de ponderation (§8.4.2.3, verifiees sur la reference) :
			//   - mono-liste : ponderation EXPLICITE seulement (l'implicite ne s'applique pas) ;
			//   - bi-predite : UNE formule combinant les deux poids — jamais deux ponderations
			//     successives — d'ou le passage de applyWeight=false a la MC.
			void McPartB(const DecCtx &c, int32 mbX, int32 mbY, int32 bx4, int32 by4, int32 pw4, int32 ph4,
						 bool useL0, bool useL1, int32 ri0, int32 ri1, int32 mvx0, int32 mvy0, int32 mvx1,
						 int32 mvy1, uint8 predY[256], uint8 cPred[2][64]) {
				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				const int32 dx = px + bx4 * 4, dy = py + by4 * 4;
				const int32 ox = bx4 * 4, oy = by4 * 4, w = pw4 * 4, h = ph4 * 4;
				const int32 cdx = cpx + bx4 * 2, cdy = cpy + by4 * 2;
				const int32 cox = bx4 * 2, coy = by4 * 2, cw = pw4 * 2, ch = ph4 * 2;

				if (useL0 && !useL1) {
					McLumaRect(c, c.L[0], dx, dy, ox, oy, w, h, mvx0, mvy0, predY, ri0);
					McChromaRect(c, c.L[0], 0, cdx, cdy, cox, coy, cw, ch, mvx0, mvy0, cPred[0], ri0);
					McChromaRect(c, c.L[0], 1, cdx, cdy, cox, coy, cw, ch, mvx0, mvy0, cPred[1], ri0);
					return;
				}
				if (useL1 && !useL0) {
					McLumaRect(c, c.L[1], dx, dy, ox, oy, w, h, mvx1, mvy1, predY, ri1);
					McChromaRect(c, c.L[1], 0, cdx, cdy, cox, coy, cw, ch, mvx1, mvy1, cPred[0], ri1);
					McChromaRect(c, c.L[1], 1, cdx, cdy, cox, coy, cw, ch, mvx1, mvy1, cPred[1], ri1);
					return;
				}
				// ── Bi-prediction : les deux predictions BRUTES puis une seule combinaison ──
				uint8 p0Y[256], p1Y[256], p0C[2][64], p1C[2][64];
				McLumaRect(c, c.L[0], dx, dy, ox, oy, w, h, mvx0, mvy0, p0Y, ri0, false);
				McLumaRect(c, c.L[1], dx, dy, ox, oy, w, h, mvx1, mvy1, p1Y, ri1, false);
				for (int32 comp = 0; comp < 2; ++comp) {
					McChromaRect(c, c.L[0], comp, cdx, cdy, cox, coy, cw, ch, mvx0, mvy0, p0C[comp], ri0, false);
					McChromaRect(c, c.L[1], comp, cdx, cdy, cox, coy, cw, ch, mvx1, mvy1, p1C[comp], ri1, false);
				}
				int32 w0 = 1, w1 = 1, o0 = 0, o1 = 0, logWD = 0;
				bool weighted = false;
				if (c.biPredMode == 1) { // explicite
					weighted = true;
				} else if (c.biPredMode == 2) { // implicite
					w0 = c.implicitW0[ri0][ri1];
					w1 = 64 - w0;
					logWD = 5;
					weighted = (w0 != 32); // 32/32 == moyenne simple : inutile de ponderer
				}
				for (int32 y = 0; y < h; ++y)
					for (int32 x = 0; x < w; ++x) {
						const int32 i = (oy + y) * 16 + (ox + x);
						const int32 a = p0Y[i], b = p1Y[i];
						if (!weighted)
							predY[i] = (uint8)((a + b + 1) >> 1);
						else if (c.biPredMode == 1)
							predY[i] = (uint8)ApplyBiWeight(a, b, c.L[0].lumaWeight[ri0], c.L[1].lumaWeight[ri1],
															c.L[0].lumaOffset[ri0], c.L[1].lumaOffset[ri1],
															c.lumaLog2Denom);
						else
							predY[i] = (uint8)ApplyBiWeight(a, b, w0, w1, o0, o1, logWD);
					}
				for (int32 comp = 0; comp < 2; ++comp)
					for (int32 y = 0; y < ch; ++y)
						for (int32 x = 0; x < cw; ++x) {
							const int32 i = (coy + y) * 8 + (cox + x);
							const int32 a = p0C[comp][i], b = p1C[comp][i];
							if (!weighted)
								cPred[comp][i] = (uint8)((a + b + 1) >> 1);
							else if (c.biPredMode == 1)
								cPred[comp][i] = (uint8)ApplyBiWeight(
									a, b, c.L[0].chromaWeight[ri0][comp], c.L[1].chromaWeight[ri1][comp],
									c.L[0].chromaOffset[ri0][comp], c.L[1].chromaOffset[ri1][comp],
									c.chromaLog2Denom);
							else
								cPred[comp][i] = (uint8)ApplyBiWeight(a, b, w0, w1, o0, o1, logWD);
						}
			}

			// Applique un mouvement DIRECT (deja deduit) sur un rectangle 8x8 du MB et compense.
			// `zero` = col_zero : la MV est forcee a 0 pour la liste dont la reference vaut 0.
			void ApplyDirect8x8(const DecCtx &c, int32 mbX, int32 mbY, int32 i8, const int32 ref[2],
								const int32 mv[2][2], bool zero, uint8 predY[256], uint8 cPred[2][64]) {
				const int32 sbx = (i8 & 1) * 2, sby = (i8 >> 1) * 2;
				int32 m[2][2] = {{mv[0][0], mv[0][1]}, {mv[1][0], mv[1][1]}};
				if (zero) {
					for (int32 l = 0; l < 2; ++l)
						if (ref[l] == 0) // la reference 0 "colle" a la co-localisee immobile
							m[l][0] = m[l][1] = 0;
				}
				const bool u0 = (ref[0] >= 0), u1 = (ref[1] >= 0);
				const int32 r0 = u0 ? ref[0] : 0, r1 = u1 ? ref[1] : 0;
				McPartB(c, mbX, mbY, sbx, sby, 2, 2, u0, u1, r0, r1, m[0][0], m[0][1], m[1][0], m[1][1],
						predY, cPred);
				StoreMv4(c, c.L[0], mbX * 4 + sbx, mbY * 4 + sby, 2, 2, m[0][0], m[0][1], u0 ? ref[0] : -1);
				StoreMv4(c, c.L[1], mbX * 4 + sbx, mbY * 4 + sby, 2, 2, m[1][0], m[1][1], u1 ? ref[1] : -1);
			}

			// Remet a zero la grille |mvd| d'un MB (skip ou intra : la reference y met 0 pour les voisins).
			void ClearMvdGrid(const DecCtx &c, const RefList &L, int32 mbX, int32 mbY) {
				for (int32 y = 0; y < 4; ++y)
					for (int32 x = 0; x < 4; ++x) {
						const int32 i = (mbY * 4 + y) * c.nzW + (mbX * 4 + x);
						L.mvdx4[i] = 0;
						L.mvdy4[i] = 0;
					}
			}

			// Ecrit |mvd| sur le rectangle d'une partition (grille 4x4).
			void StoreMvd4(const DecCtx &c, const RefList &L, int32 bx, int32 by, int32 pw, int32 ph, int32 ax, int32 ay) {
				for (int32 y = by; y < by + ph; ++y)
					for (int32 x = bx; x < bx + pw; ++x) {
						L.mvdx4[y * c.nzW + x] = ax;
						L.mvdy4[y * c.nzW + x] = ay;
					}
			}

			// Macrobloc INTER P en CABAC (miroir de DecodeMbInterP ; entropie CABAC).
			// mbType : 0=P_L0_16x16, 1=P_16x8, 2=P_8x16, 3=P_8x8.
			bool DecodeMbCabacP(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY, int32 mbType) {
				const RefList &L = c.L[0]; // P : une seule liste de references

				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				const int32 gbx = mbX * 4, gby = mbY * 4;
				const int32 mbIdx = mbY * cab.mbW + mbX;
				uint8 predY[256], cPred[2][64];

				// Compense une partition (bx4,by4 = coin 4x4 ; pw4/ph4 = taille 4x4) + stocke MV/refIdx/|mvd|.
				auto doPart = [&](int32 bx4, int32 by4, int32 pw4, int32 ph4, int32 mvx, int32 mvy, int32 ri,
								  int32 ax, int32 ay) {
					McLumaRect(c, L, px + bx4 * 4, py + by4 * 4, bx4 * 4, by4 * 4, pw4 * 4, ph4 * 4, mvx, mvy, predY, ri);
					McChromaRect(c, L, 0, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[0], ri);
					McChromaRect(c, L, 1, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[1], ri);
					StoreMv4(c, L, gbx + bx4, gby + by4, pw4, ph4, mvx, mvy, ri);
					StoreMvd4(c, L, gbx + bx4, gby + by4, pw4, ph4, ax, ay);
				};

				// ref_idx_l0 : non code (donc 0) si une seule reference active.
				const bool readRef = (L.numRefActive > 1);

				// Lit les 2 composantes du mvd. amvd = |mvd_A| + |mvd_B| lus dans la grille 4x4 AVANT
				// d'ecrire cette partition (les sous-blocs d'un meme MB se voisinent entre eux).
				bool bad = false;
				auto readMvd = [&](int32 bx, int32 by, int32 &dx, int32 &dy, int32 &ax, int32 &ay) {
					const int32 ax0 = (bx > 0) ? L.mvdx4[by * c.nzW + (bx - 1)] : 0;
					const int32 ax1 = (by > 0) ? L.mvdx4[(by - 1) * c.nzW + bx] : 0;
					const int32 ay0 = (bx > 0) ? L.mvdy4[by * c.nzW + (bx - 1)] : 0;
					const int32 ay1 = (by > 0) ? L.mvdy4[(by - 1) * c.nzW + bx] : 0;
					dx = DecodeMvdCabac(cab, kCtxMvd0, ax0 + ax1, ax);
					dy = DecodeMvdCabac(cab, kCtxMvd1, ay0 + ay1, ay);
					if (dx == kMvdOverflow || dy == kMvdOverflow)
						bad = true;
				};

				// noSubMbPartSizeLessThan8x8Flag : autorise la transformée 8x8 (profil High) seulement si
				// aucune sous-partition n'est < 8x8. Vrai sauf en P_8x8 avec au moins un sous-type != 8x8.
				bool noSub8 = true;
				if (mbType == 0) { // P_L0_16x16 : 1 ref_idx puis 1 mvd
					const int32 ri = readRef ? DecodeRefIdxCabac(cab, c, L, gbx, gby) : 0;
					if (ri < 0)
						return false;
					StoreRef4(c, L, gbx, gby, 4, 4, ri);
					int32 pmx, pmy;
					PredMvPart(c, L, gbx, gby, 4, 0, 0, ri, pmx, pmy);
					int32 dx, dy, ax, ay;
					readMvd(gbx, gby, dx, dy, ax, ay);
					if (bad)
						return false;
					doPart(0, 0, 4, 4, pmx + dx, pmy + dy, ri, ax, ay);
				} else if (mbType == 1 || mbType == 2) { // P_16x8 / P_8x16 : les 2 ref_idx AVANT les 2 mvd
					const bool horiz = (mbType == 1);
					int32 ri[2];
					for (int32 part = 0; part < 2; ++part) {
						const int32 bx4 = horiz ? 0 : part * 2, by4 = horiz ? part * 2 : 0;
						// ⚠️ La grille ref DOIT etre remplie entre les deux lectures : la partition 1 voit
						// la partition 0 comme voisine pour son ctxIdxInc.
						ri[part] = readRef ? DecodeRefIdxCabac(cab, c, L, gbx + bx4, gby + by4) : 0;
						if (ri[part] < 0)
							return false;
						StoreRef4(c, L, gbx + bx4, gby + by4, horiz ? 4 : 2, horiz ? 2 : 4, ri[part]);
					}
					for (int32 part = 0; part < 2; ++part) {
						const int32 bx4 = horiz ? 0 : part * 2, by4 = horiz ? part * 2 : 0;
						int32 pmx, pmy;
						PredMvPart(c, L, gbx + bx4, gby + by4, horiz ? 4 : 2, horiz ? 1 : 2, part, ri[part], pmx, pmy);
						int32 dx, dy, ax, ay;
						readMvd(gbx + bx4, gby + by4, dx, dy, ax, ay);
						if (bad)
							return false;
						doPart(bx4, by4, horiz ? 4 : 2, horiz ? 2 : 4, pmx + dx, pmy + dy, ri[part], ax, ay);
					}
				} else { // P_8x8
					int32 subType[4];
					for (int32 b = 0; b < 4; ++b) {
						subType[b] = DecodeSubMbTypePCabac(cab);
						if (subType[b] != 0) // 8x4/4x8/4x4 -> sous-partition < 8x8
							noSub8 = false;
					}
					int32 ri[4] = {0, 0, 0, 0};
					for (int32 b = 0; b < 4; ++b) {
						const int32 sbx = (b & 1) * 2, sby = (b >> 1) * 2;
						ri[b] = readRef ? DecodeRefIdxCabac(cab, c, L, gbx + sbx, gby + sby) : 0;
						if (ri[b] < 0)
							return false;
						StoreRef4(c, L, gbx + sbx, gby + sby, 2, 2, ri[b]);
					}
					for (int32 b = 0; b < 4; ++b) {
						const int32 sbx = (b & 1) * 2, sby = (b >> 1) * 2;
						int32 pmx, pmy, dx, dy, ax, ay;
						if (subType[b] == 0) { // 8x8
							PredMvPart(c, L, gbx + sbx, gby + sby, 2, 3, 0, ri[b], pmx, pmy);
							readMvd(gbx + sbx, gby + sby, dx, dy, ax, ay);
							if (bad)
								return false;
							doPart(sbx, sby, 2, 2, pmx + dx, pmy + dy, ri[b], ax, ay);
						} else if (subType[b] == 1) { // 8x4
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, L, gbx + sbx, gby + sby + sp, 2, 3, 0, ri[b], pmx, pmy);
								readMvd(gbx + sbx, gby + sby + sp, dx, dy, ax, ay);
								if (bad)
									return false;
								doPart(sbx, sby + sp, 2, 1, pmx + dx, pmy + dy, ri[b], ax, ay);
							}
						} else if (subType[b] == 2) { // 4x8
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, L, gbx + sbx + sp, gby + sby, 1, 3, 0, ri[b], pmx, pmy);
								readMvd(gbx + sbx + sp, gby + sby, dx, dy, ax, ay);
								if (bad)
									return false;
								doPart(sbx + sp, sby, 1, 2, pmx + dx, pmy + dy, ri[b], ax, ay);
							}
						} else { // 4x4
							for (int32 sp = 0; sp < 4; ++sp) {
								const int32 pbx = sbx + (sp & 1), pby = sby + (sp >> 1);
								PredMvPart(c, L, gbx + pbx, gby + pby, 1, 3, 0, ri[b], pmx, pmy);
								readMvd(gbx + pbx, gby + pby, dx, dy, ax, ay);
								if (bad)
									return false;
								doPart(pbx, pby, 1, 1, pmx + dx, pmy + dy, ri[b], ax, ay);
							}
						}
					}
				}

				// cbp puis mb_qp_delta.
				const int32 lumaCbp = DecodeCbpLumaCabac(cab, mbX, mbY);
				const int32 cbpChroma = DecodeCbpChromaCabac(cab, mbX, mbY);
				cab.mbTypeClass[(uint64)mbIdx] = 1; // inter -> classe 1 (comme I_16x16 pour le ctx mb_type)
				cab.cbpLumaMb[(uint64)mbIdx] = lumaCbp;
				cab.cbpChromaMb[(uint64)mbIdx] = cbpChroma;
				cab.chromaModeMb[(uint64)mbIdx] = 0;
				cab.mbSkipMb[(uint64)mbIdx] = 0;
				// ⚠️ Une B lira CETTE image comme co-localisee : la forme conditionne la granularite de
				// son test "bloc immobile". P_L0_16x16 -> 16x16 ; les autres partitions -> pas 16x16.
				c.mb16x16OrIntra[mbIdx] = (mbType == 0) ? 1 : 0;
				// transform_size_8x8_flag (inter, profil High) : APRES le CBP, si CBPLuma>0 et aucune
				// sous-partition < 8x8. (§7.3.5 : lu entre coded_block_pattern et mb_qp_delta.)
				int32 t8 = 0;
				if (c.transform8x8Mode && lumaCbp > 0 && noSub8)
					t8 = DecodeTransform8x8FlagCabac(cab, mbX, mbY);
				cab.transform8x8Mb[(uint64)mbIdx] = t8;
				if (c.transform8x8Mb)
					c.transform8x8Mb[mbIdx] = (uint8)t8;
				if (lumaCbp || cbpChroma)
					c.qp = (c.qp + DecodeMbQpDeltaCabac(cab) + 52) % 52;
				else
					cab.prevQpDeltaNonZero = 0;

				// Residu luma (cat 2). ⚠️ MB inter -> defaut cbf d'un voisin INDISPONIBLE = 0 (pas 1).
				const int32 bx0 = mbX * 4, by0 = mbY * 4;
				if (t8) {
					// Transformée 8x8 : 4 blocs 8x8, résidu ajouté à la prédiction compensée.
					for (int32 i8 = 0; i8 < 4; ++i8) {
						const int32 bx8 = (i8 & 1) * 8, by8 = (i8 >> 1) * 8;
						const int32 bx4 = bx0 + (i8 & 1) * 2, by4 = by0 + (i8 >> 1) * 2;
						int32 raster[64] = {0};
						int32 tc = 0;
						if (lumaCbp & (1 << i8)) // pas de coded_block_flag en 4:2:0 (inféré à 1)
							tc = DecodeResidualCabac8x8(cab, raster);
						for (int32 dy = 0; dy < 2; ++dy)
							for (int32 dx = 0; dx < 2; ++dx)
								c.lumaNz[(by4 + dy) * c.nzW + (bx4 + dx)] = tc;
						int32 deq[64], resRec[64];
						if (c.TsBypass()) { // lossless inter : résidu direct (pas de cumul en inter)
							for (int32 i = 0; i < 64; ++i)
								resRec[i] = raster[i];
						} else {
							NkH264Transform::Dequant8x8(raster, deq, c.qp, c.wsl8[1]);
							NkH264Transform::Inverse8x8(deq, resRec);
						}
						for (int32 r = 0; r < 8; ++r)
							for (int32 col = 0; col < 8; ++col)
								c.Y[(usize)(py + by8 + r) * c.lumaW + px + bx8 + col] =
									ClampU8((int32)predY[(by8 + r) * 16 + (bx8 + col)] + resRec[r * 8 + col]);
					}
				} else {
					for (int32 blk = 0; blk < 16; ++blk) {
						int32 x4, y4;
						LumaBlk(blk, x4, y4);
						const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
						int32 raster[16] = {0};
						int32 tc = 0;
						if (lumaCbp & (1 << (blk >> 2))) {
							const int32 cbfA = (bx > 0) ? (c.lumaNz[by * c.nzW + (bx - 1)] > 0 ? 1 : 0) : 0;
							const int32 cbfB = (by > 0) ? (c.lumaNz[(by - 1) * c.nzW + bx] > 0 ? 1 : 0) : 0;
							tc = DecodeResidualCabac(cab, 2, cbfA, cbfB, 0, raster);
						}
						c.lumaNz[by * c.nzW + bx] = tc;
						int32 deq[16], resRec[16];
						if (c.TsBypass()) { // lossless inter : résidu direct (pas de cumul en inter)
							for (int32 i = 0; i < 16; ++i)
								resRec[i] = raster[i];
						} else {
							NkH264Transform::Dequant4x4(raster, deq, c.qp, c.wsl4[3]);
							NkH264Transform::Inverse4x4(deq, resRec);
						}
						for (int32 r = 0; r < 4; ++r)
							for (int32 col = 0; col < 4; ++col)
								c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
									ClampU8(predY[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
					}
				}

				DecodeChromaResidualCabac(c, cab, mbX, mbY, cbpChroma, cPred, 0); // inter -> defaut cbf = 0
				return true;
			}

			// Macrobloc INTER B en CABAC. mbType 0..22 (0 = B_Direct_16x16, 22 = B_8x8).
			// `skip` : B_Skip = B_Direct_16x16 sans aucun residu.
			bool DecodeMbCabacB(DecCtx &c, CabacMb &cab, int32 mbX, int32 mbY, int32 mbType, bool skip) {
				const int32 px = mbX * 16, py = mbY * 16;
				const int32 gbx = mbX * 4, gby = mbY * 4;
				const int32 mbIdx = mbY * cab.mbW + mbX;
				uint8 predY[256], cPred[2][64];
				const BPartInfo info = skip ? BMbTypeInfo(0) : BMbTypeInfo(mbType);
				// noSubMbPartSizeLessThan8x8Flag (autorise la transformée 8x8 inter, profil High).
				bool noSub8 = true;

				// Les grilles |mvd| ne servent qu'aux partitions codees : le Direct n'en code aucune.
				ClearMvdGrid(c, c.L[0], mbX, mbY);
				ClearMvdGrid(c, c.L[1], mbX, mbY);
				// ⚠️ Init du champ de mouvement du MB courant a "non utilise" (ref=-1, mv=0) pour LES
				// DEUX listes. Sinon, dans un B_8x8, un sous-bloc qui n'utilise pas une liste garde le
				// ref4/mv4 PERIME du MB precedent a cette position -> la prediction MV d'un sous-bloc
				// VOISIN (meme MB) qui interroge cette liste lit une valeur fausse -> MV finale fausse.
				for (int32 l = 0; l < 2; ++l)
					for (int32 y = 0; y < 4; ++y)
						for (int32 x = 0; x < 4; ++x) {
							const int32 gi = (gby + y) * c.nzW + (gbx + x);
							c.L[l].ref4[gi] = -1;
							c.L[l].mvx4[gi] = 0;
							c.L[l].mvy4[gi] = 0;
						}

				// direct4 : marque les blocs Direct AVANT de lire les ref_idx (leur contexte exclut un
				// voisin Direct). Tout le MB si B_Skip/Direct_16x16 ; sinon rien (les sous-blocs Direct
				// d'un B_8x8 sont marques plus bas, une fois les sub_mb_type connus).
				if (c.direct4)
					for (int32 y = 0; y < 4; ++y)
						for (int32 x = 0; x < 4; ++x)
							c.direct4[(gby + y) * c.nzW + (gbx + x)] = info.direct ? 1 : 0;

				if (info.direct) { // B_Skip / B_Direct_16x16 : tout le MB est deduit
					int32 dref[2], dmv[2][2];
					bool zero8[4];
					PredDirectSpatial(c, mbX, mbY, dref, dmv, zero8);
					for (int32 i8 = 0; i8 < 4; ++i8)
						ApplyDirect8x8(c, mbX, mbY, i8, dref, dmv, zero8[i8], predY, cPred);
				} else if (mbType == 22) { // B_8x8 : chaque 8x8 a son sub_mb_type
					int32 sub[4];
					for (int32 b = 0; b < 4; ++b)
						sub[b] = DecodeSubMbTypeBCabac(cab);
					BPartInfo si[4];
					for (int32 b = 0; b < 4; ++b) {
						si[b] = BSubMbTypeInfo(sub[b]);
						// §7.4.5.2 : un sous-bloc non-Direct de plus d'une partition -> < 8x8 ; un
						// B_Direct_8x8 impose < 8x8 SAUF si direct_8x8_inference_flag.
						if (si[b].direct) {
							if (!c.directInference)
								noSub8 = false;
						} else if (si[b].nParts > 1)
							noSub8 = false;
					}
						// Marque les sous-blocs Direct AVANT les ref_idx (contexte des voisins internes).
						if (c.direct4)
							for (int32 b = 0; b < 4; ++b) {
								const int32 sbx = (b & 1) * 2, sby = (b >> 1) * 2;
								for (int32 y = 0; y < 2; ++y)
									for (int32 x = 0; x < 2; ++x)
										c.direct4[(gby + sby + y) * c.nzW + (gbx + sbx + x)] = si[b].direct ? 1 : 0;
							}
					// Tous les ref_idx (L0 puis L1) AVANT tous les mvd — ordre de la reference.
					// Sous-blocs Direct calcules + stockes + compenses ICI, AVANT ref/mvd (comme la reference
					// ff_h264_pred_direct_motion qui precede les ref_idx). Sinon un sous-bloc CODE ayant un
					// voisin Direct dans le meme MB predirait sa MV sur un champ de mouvement encore vide.
					{
						bool anyDirect = false;
						for (int32 b = 0; b < 4; ++b)
							anyDirect = anyDirect || si[b].direct;
						if (anyDirect) {
							int32 dref[2], dmv[2][2];
							bool zero8[4];
							PredDirectSpatial(c, mbX, mbY, dref, dmv, zero8);
							for (int32 b = 0; b < 4; ++b)
								if (si[b].direct)
									ApplyDirect8x8(c, mbX, mbY, b, dref, dmv, zero8[b], predY, cPred);
						}
					}
					int32 ri[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
					for (int32 l = 0; l < 2; ++l)
						for (int32 b = 0; b < 4; ++b) {
							const bool use = (l == 0) ? si[b].l0[0] : si[b].l1[0];
							const int32 sbx = (b & 1) * 2, sby = (b >> 1) * 2;
							if (si[b].direct || !use) {
								ri[l][b] = -1;
								continue;
							}
							ri[l][b] = (c.L[l].numRefActive > 1)
										   ? DecodeRefIdxCabac(cab, c, c.L[l], gbx + sbx, gby + sby, true)
										   : 0;
							if (ri[l][b] < 0)
								return false;
							StoreRef4(c, c.L[l], gbx + sbx, gby + sby, 2, 2, ri[l][b]);
						}
						// Position (en blocs 4x4, dans le MB) de la sous-partition sp d'un 8x8.
						auto subOff = [&](int32 b, int32 sp, const BPartInfo &sinf, int32 &ox, int32 &oy) {
							ox = (b & 1) * 2;
							oy = (b >> 1) * 2;
							if (sinf.nParts == 2) {
								if (sinf.ph == 1)
									oy += sp; // 8x4
								else
									ox += sp; // 4x8
							} else if (sinf.nParts == 4) {
								ox += (sp & 1);
								oy += (sp >> 1);
							}
						};
						// mvd LISTE-EXTERNE (toute la L0 puis toute la L1), block puis sous-partition, en
						// SAUTANT les blocs Direct -- exactement l'ordre de la reference (sinon desync).
						int32 mvSub[2][4][4][2] = {{{{0}}}}; // [list][block][subpart][xy]
						for (int32 l = 0; l < 2; ++l)
							for (int32 b = 0; b < 4; ++b) {
								if (si[b].direct)
									continue;
								const bool use = (l == 0) ? si[b].l0[0] : si[b].l1[0];
								if (!use)
									continue;
								for (int32 sp = 0; sp < si[b].nParts; ++sp) {
									int32 ox, oy;
									subOff(b, sp, si[b], ox, oy);
									int32 pmx, pmy;
									PredMvPart(c, c.L[l], gbx + ox, gby + oy, si[b].pw, 3, 0, ri[l][b], pmx, pmy);
									const int32 ax0 = (gbx + ox > 0) ? c.L[l].mvdx4[(gby + oy) * c.nzW + gbx + ox - 1] : 0;
									const int32 ax1 = (gby + oy > 0) ? c.L[l].mvdx4[(gby + oy - 1) * c.nzW + gbx + ox] : 0;
									const int32 ay0 = (gbx + ox > 0) ? c.L[l].mvdy4[(gby + oy) * c.nzW + gbx + ox - 1] : 0;
									const int32 ay1 = (gby + oy > 0) ? c.L[l].mvdy4[(gby + oy - 1) * c.nzW + gbx + ox] : 0;
									int32 ax, ay;
									const int32 dx = DecodeMvdCabac(cab, kCtxMvd0, ax0 + ax1, ax);
									const int32 dy = DecodeMvdCabac(cab, kCtxMvd1, ay0 + ay1, ay);
									if (dx == kMvdOverflow || dy == kMvdOverflow)
										return false;
									mvSub[l][b][sp][0] = pmx + dx;
									mvSub[l][b][sp][1] = pmy + dy;
									StoreMvd4(c, c.L[l], gbx + ox, gby + oy, si[b].pw, si[b].ph, ax, ay);
									StoreMv4(c, c.L[l], gbx + ox, gby + oy, si[b].pw, si[b].ph, mvSub[l][b][sp][0],
											 mvSub[l][b][sp][1], ri[l][b]);
								}
							}
						// Compensation (2e passage) : SEULEMENT les sous-blocs codes (les Direct ont deja
						// ete compenses plus haut, avant ref/mvd).
						for (int32 b = 0; b < 4; ++b) {
							if (si[b].direct)
								continue;
							for (int32 sp = 0; sp < si[b].nParts; ++sp) {
								int32 ox, oy;
								subOff(b, sp, si[b], ox, oy);
								McPartB(c, mbX, mbY, ox, oy, si[b].pw, si[b].ph, si[b].l0[0], si[b].l1[0],
										si[b].l0[0] ? ri[0][b] : 0, si[b].l1[0] ? ri[1][b] : 0, mvSub[0][b][sp][0],
										mvSub[0][b][sp][1], mvSub[1][b][sp][0], mvSub[1][b][sp][1], predY, cPred);
							}
						}
				} else { // 16x16 / 16x8 / 8x16, avec une direction par partition
					const int32 nP = info.nParts;
					const int32 shape = (nP == 1) ? 0 : (info.ph == 2 ? 1 : 2); // 0=16x16,1=16x8,2=8x16
					int32 ri[2][2] = {{0, 0}, {0, 0}};
					for (int32 l = 0; l < 2; ++l)
						for (int32 p = 0; p < nP; ++p) {
							const bool use = (l == 0) ? info.l0[p] : info.l1[p];
							const int32 ox = (shape == 2) ? p * 2 : 0, oy = (shape == 1) ? p * 2 : 0;
							if (!use) {
								ri[l][p] = -1;
								StoreRef4(c, c.L[l], gbx + ox, gby + oy, info.pw, info.ph, -1);
								continue;
							}
							ri[l][p] = (c.L[l].numRefActive > 1)
										   ? DecodeRefIdxCabac(cab, c, c.L[l], gbx + ox, gby + oy, true)
										   : 0;
							if (ri[l][p] < 0)
								return false;
							StoreRef4(c, c.L[l], gbx + ox, gby + oy, info.pw, info.ph, ri[l][p]);
						}
					// ⚠️ ORDRE : les mvd se lisent LISTE-EXTERNE (toute la L0 puis toute la L1), comme la
					// reference — PAS partition-externe. Sinon desync des que 2 partitions/listes ont un
					// mvd non nul. La MV de chaque partition est stockee des sa lecture (prediction des
					// suivantes de la MEME liste), la compensation se fait dans un 2e passage.
					int32 mvPart[2][2][2] = {{{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}}; // [list][part][xy]
					for (int32 l = 0; l < 2; ++l)
						for (int32 p = 0; p < nP; ++p) {
							const bool use = (l == 0) ? info.l0[p] : info.l1[p];
							if (!use)
								continue;
							const int32 ox = (shape == 2) ? p * 2 : 0, oy = (shape == 1) ? p * 2 : 0;
							int32 pmx, pmy;
							PredMvPart(c, c.L[l], gbx + ox, gby + oy, info.pw, shape, p, ri[l][p], pmx, pmy);
							const int32 ax0 = (gbx + ox > 0) ? c.L[l].mvdx4[(gby + oy) * c.nzW + gbx + ox - 1] : 0;
							const int32 ax1 = (gby + oy > 0) ? c.L[l].mvdx4[(gby + oy - 1) * c.nzW + gbx + ox] : 0;
							const int32 ay0 = (gbx + ox > 0) ? c.L[l].mvdy4[(gby + oy) * c.nzW + gbx + ox - 1] : 0;
							const int32 ay1 = (gby + oy > 0) ? c.L[l].mvdy4[(gby + oy - 1) * c.nzW + gbx + ox] : 0;
							int32 ax, ay;
							const int32 dx = DecodeMvdCabac(cab, kCtxMvd0, ax0 + ax1, ax);
							const int32 dy = DecodeMvdCabac(cab, kCtxMvd1, ay0 + ay1, ay);
							if (dx == kMvdOverflow || dy == kMvdOverflow)
								return false;
							mvPart[l][p][0] = pmx + dx;
							mvPart[l][p][1] = pmy + dy;
							StoreMvd4(c, c.L[l], gbx + ox, gby + oy, info.pw, info.ph, ax, ay);
							StoreMv4(c, c.L[l], gbx + ox, gby + oy, info.pw, info.ph, mvPart[l][p][0],
									 mvPart[l][p][1], ri[l][p]);
						}
					for (int32 p = 0; p < nP; ++p) {
						const int32 ox = (shape == 2) ? p * 2 : 0, oy = (shape == 1) ? p * 2 : 0;
						McPartB(c, mbX, mbY, ox, oy, info.pw, info.ph, info.l0[p], info.l1[p],
								info.l0[p] ? ri[0][p] : 0, info.l1[p] ? ri[1][p] : 0, mvPart[0][p][0],
								mvPart[0][p][1], mvPart[1][p][0], mvPart[1][p][1], predY, cPred);
					}
				}

				// Grilles voisines. Un B_Direct/B_Skip compte comme "Direct" pour le ctx de mb_type.
				cab.mbTypeClass[(uint64)mbIdx] = 1;
				cab.chromaModeMb[(uint64)mbIdx] = 0;
				cab.mbSkipMb[(uint64)mbIdx] = skip ? 1 : 0;
				cab.mbDirectMb[(uint64)mbIdx] = info.direct ? 1 : 0;
				c.mb16x16OrIntra[mbIdx] = (info.nParts == 1) ? 1 : 0;

				if (skip) { // B_Skip : aucun residu, aucun cbp, aucun mb_qp_delta
					cab.cbpLumaMb[(uint64)mbIdx] = 0;
					cab.cbpChromaMb[(uint64)mbIdx] = 0;
					cab.lumaDcCodedMb[(uint64)mbIdx] = 0;
					cab.chromaDcCodedMb[(uint64)mbIdx] = 0;
					cab.prevQpDeltaNonZero = 0;
					for (int32 y = 0; y < 4; ++y)
						for (int32 x = 0; x < 4; ++x)
							c.lumaNz[(mbY * 4 + y) * c.nzW + (mbX * 4 + x)] = 0;
					for (int32 y = 0; y < 2; ++y)
						for (int32 x = 0; x < 2; ++x) {
							c.chromaNz0[(mbY * 2 + y) * c.cnzW + (mbX * 2 + x)] = 0;
							c.chromaNz1[(mbY * 2 + y) * c.cnzW + (mbX * 2 + x)] = 0;
						}
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x)
							c.Y[(usize)(py + y) * c.lumaW + px + x] = predY[y * 16 + x];
					for (int32 comp = 0; comp < 2; ++comp) {
						uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
						for (int32 y = 0; y < 8; ++y)
							for (int32 x = 0; x < 8; ++x)
								rec[(usize)(mbY * 8 + y) * c.chromaW + mbX * 8 + x] = cPred[comp][y * 8 + x];
					}
					return true;
				}

				const int32 lumaCbp = DecodeCbpLumaCabac(cab, mbX, mbY);
				const int32 cbpChroma = DecodeCbpChromaCabac(cab, mbX, mbY);
				cab.cbpLumaMb[(uint64)mbIdx] = lumaCbp;
				cab.cbpChromaMb[(uint64)mbIdx] = cbpChroma;
				// transform_size_8x8_flag (inter B, profil High) : après le CBP. Condition B-specifique
				// (§7.3.5) : mb_type != B_Direct_16x16 OU direct_8x8_inference_flag.
				int32 t8 = 0;
				if (c.transform8x8Mode && lumaCbp > 0 && noSub8 && (!info.direct || c.directInference))
					t8 = DecodeTransform8x8FlagCabac(cab, mbX, mbY);
				cab.transform8x8Mb[(uint64)mbIdx] = t8;
				if (c.transform8x8Mb)
					c.transform8x8Mb[mbIdx] = (uint8)t8;
				if (lumaCbp || cbpChroma)
					c.qp = (c.qp + DecodeMbQpDeltaCabac(cab) + 52) % 52;
				else
					cab.prevQpDeltaNonZero = 0;

				// Residu luma (cat 2). MB inter -> defaut cbf d'un voisin INDISPONIBLE = 0.
				const int32 bx0 = mbX * 4, by0 = mbY * 4;
				if (t8) {
					for (int32 i8 = 0; i8 < 4; ++i8) {
						const int32 bx8 = (i8 & 1) * 8, by8 = (i8 >> 1) * 8;
						const int32 bx4 = bx0 + (i8 & 1) * 2, by4 = by0 + (i8 >> 1) * 2;
						int32 raster[64] = {0};
						int32 tc = 0;
						if (lumaCbp & (1 << i8)) // pas de coded_block_flag en 4:2:0 (inféré à 1)
							tc = DecodeResidualCabac8x8(cab, raster);
						for (int32 dy = 0; dy < 2; ++dy)
							for (int32 dx = 0; dx < 2; ++dx)
								c.lumaNz[(by4 + dy) * c.nzW + (bx4 + dx)] = tc;
						int32 deq[64], resRec[64];
						if (c.TsBypass()) { // lossless inter : résidu direct (pas de cumul en inter)
							for (int32 i = 0; i < 64; ++i)
								resRec[i] = raster[i];
						} else {
							NkH264Transform::Dequant8x8(raster, deq, c.qp, c.wsl8[1]);
							NkH264Transform::Inverse8x8(deq, resRec);
						}
						for (int32 r = 0; r < 8; ++r)
							for (int32 col = 0; col < 8; ++col)
								c.Y[(usize)(py + by8 + r) * c.lumaW + px + bx8 + col] =
									ClampU8((int32)predY[(by8 + r) * 16 + (bx8 + col)] + resRec[r * 8 + col]);
					}
				} else {
					for (int32 blk = 0; blk < 16; ++blk) {
						int32 x4, y4;
						LumaBlk(blk, x4, y4);
						const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
						int32 raster[16] = {0};
						int32 tc = 0;
						if (lumaCbp & (1 << (blk >> 2))) {
							const int32 cbfA = (bx > 0) ? (c.lumaNz[by * c.nzW + (bx - 1)] > 0 ? 1 : 0) : 0;
							const int32 cbfB = (by > 0) ? (c.lumaNz[(by - 1) * c.nzW + bx] > 0 ? 1 : 0) : 0;
							tc = DecodeResidualCabac(cab, 2, cbfA, cbfB, 0, raster);
						}
						c.lumaNz[by * c.nzW + bx] = tc;
						int32 deq[16], resRec[16];
						if (c.TsBypass()) { // lossless inter : résidu direct (pas de cumul en inter)
							for (int32 i = 0; i < 16; ++i)
								resRec[i] = raster[i];
						} else {
							NkH264Transform::Dequant4x4(raster, deq, c.qp, c.wsl4[3]);
							NkH264Transform::Inverse4x4(deq, resRec);
						}
						for (int32 r = 0; r < 4; ++r)
							for (int32 col = 0; col < 4; ++col)
								c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
									ClampU8(predY[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
					}
				}
				DecodeChromaResidualCabac(c, cab, mbX, mbY, cbpChroma, cPred, 0);
				return true;
			}

		} // namespace

		bool NkH264Decoder::DecodeIdrFrame(const uint8 *annexB, usize size, NkH264Frame &out) {
			return DecodeFrame(annexB, size, nullptr, out);
		}

		bool NkH264Decoder::DecodeFrame(const uint8 *annexB, usize size, const NkH264Frame *ref, NkH264Frame &out) {
			// Wrapper mono-référence : liste de 0 ou 1 élément.
			const NkH264Frame *refs[1] = {ref};
			return DecodeFrame(annexB, size, ref ? refs : nullptr, ref ? 1 : 0, out);
		}

		bool NkH264Decoder::DecodeFrame(const uint8 *annexB, usize size, const NkH264Frame *const *refs, int32 numRefs,
										NkH264Frame &out) {
			NkVector<NkH264Nal> nals;
			SplitNalsAnnexB(annexB, size, nals);
			const uint8 *spsN = nullptr, *ppsN = nullptr, *idrN = nullptr;
			usize spsS = 0, ppsS = 0, idrS = 0;
			for (uint64 i = 0; i < nals.Size(); ++i) {
				const NkH264Nal &n = nals[i];
				if (n.type == 7 && !spsN) {
					spsN = annexB + n.offset;
					spsS = n.size;
				} else if (n.type == 8 && !ppsN) {
					ppsN = annexB + n.offset;
					ppsS = n.size;
				} else if ((n.type == 5 || n.type == 1) && !idrN) { // 1re slice (IDR ou P)
					idrN = annexB + n.offset;
					idrS = n.size;
				}
			}
			if (!spsN || !ppsN || !idrN)
				return false;

			NkH264Sps sps;
			NkH264Pps pps;
			if (!ParseSps(spsN, spsS, sps) || !ParsePps(ppsN, ppsS, pps, &sps))
				return false;
			// CABAC (entropyCodingMode==1) : géré pour les I-slices (voir dispatch plus bas) ;
			// P/B CABAC pas encore -> rejeté au dispatch.
			if (sps.frameMbsOnly != 1) // entrelacé non géré
				return false;

			// De-emulation + lecteur sur le slice IDR ; on lit l'en-tête inline puis les MB.
			NkVector<nk_uint8> rbsp;
			Deemul(idrN + 1, idrS - 1, rbsp);
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			const int32 nalRefIdc = (idrN[0] >> 5) & 3;

			const int32 nalType = idrN[0] & 0x1F;
			const int32 firstMb = (int32)br.UE();
			const int32 sliceType = (int32)br.UE();
			const int32 st = sliceType % 5;
			const bool isI = (st == 2), isP = (st == 0), isB = (st == 1);
			const bool isInter = isP || isB; // slices a compensation de mouvement (listes de refs)
			if (firstMb != 0 || (!isI && !isInter)) // seules I / P / B (slice unique a 0) gerees
				return false;
			if (isInter && numRefs <= 0) // P/B sans reference
				return false;
			// ⚠️ Les B ne sont decodees qu'en CABAC (les B CAVLC tomberaient dans le chemin P de la
			// branche `else if (isI) ... else <P>` et sortiraient des pixels FAUX au lieu d'un echec).
			if (isB && pps.entropyCodingMode != 1)
				return false;
			br.UE();				   // pps_id
			const int32 frameNum = (int32)br.U(sps.log2MaxFrameNum); // frame_num
			if (nalType == 5)
				br.UE(); // idr_pic_id
			// ── POC (§8.2.1.1, pic_order_cnt_type 0) ─────────────────────────────────
			// L'etat requis (prevPicOrderCntMsb/Lsb) est celui de l'IMAGE DE REFERENCE PRECEDENTE en
			// ordre de decodage — c'est exactement refs[0] (le DPB est trie plus recent d'abord), donc
			// la derivation reste SANS ETAT tant que l'appelant n'insere que les references dans le DPB.
			int32 pocLsb = 0, pocMsb = 0, poc = 0;
			if (sps.pocType == 0) {
				pocLsb = (int32)br.U(sps.log2MaxPocLsb); // pic_order_cnt_lsb
				if (pps.bottomFieldPocPresent)
					br.SE(); // delta_pic_order_cnt_bottom (frame coding : ignore)
				const int32 maxPocLsb = 1 << sps.log2MaxPocLsb;
				int32 prevMsb = 0, prevLsb = 0;
				if (nalType != 5 && numRefs > 0 && refs && refs[0]) {
					prevMsb = refs[0]->pocMsb;
					prevLsb = refs[0]->pocLsb;
				}
				if (pocLsb < prevLsb && (prevLsb - pocLsb) >= (maxPocLsb / 2))
					pocMsb = prevMsb + maxPocLsb;
				else if (pocLsb > prevLsb && (pocLsb - prevLsb) > (maxPocLsb / 2))
					pocMsb = prevMsb - maxPocLsb;
				else
					pocMsb = prevMsb;
				poc = pocMsb + pocLsb;
			} else if (sps.pocType == 2) {
				// §8.2.1.3 : POC = 2*(FrameNumOffset + frame_num), -1 si l'image n'est pas une
				// reference. x264 choisit ce type en BASELINE (sans B, ordre decodage == affichage).
				// FrameNumOffset absorbe le rebouclage de frame_num ; on le transporte dans pocMsb.
				const int32 maxFrameNum = 1 << sps.log2MaxFrameNum;
				int32 frameNumOffset = 0;
				if (nalType != 5 && numRefs > 0 && refs && refs[0])
					frameNumOffset = (refs[0]->frameNum > frameNum) ? refs[0]->pocMsb + maxFrameNum
																   : refs[0]->pocMsb;
				pocMsb = frameNumOffset; // ce que l'image SUIVANTE devra reprendre
				pocLsb = 0;
				poc = 2 * (frameNumOffset + frameNum) - ((nalRefIdc != 0) ? 0 : 1);
			} else if (sps.pocType == 1) {
				if (!sps.deltaPocAlwaysZero) {
					br.SE();
					br.SE();
				}
				// ⚠️ Type 1 NON derive : il exige les champs de cycle du SPS (offset_for_ref_frame[]…)
				// que l'on ne parse pas. Approximation valable tant que decodage == affichage (pas de B).
				// Aucun encodeur courant ne l'emet (x264 : type 0 avec B, type 2 en baseline).
				poc = 2 * frameNum;
				pocMsb = 0;
				pocLsb = 0;
			}
			if (pps.redundantPicCntPresent)
				br.UE();
			// ref_pic_list_modification (§7.3.3.1) : operations de reordonnancement de RefPicList0.
			// x264 s'en sert en profil Main avec weightp=2 (le DEFAUT) pour placer une copie ponderee
			// d'une reference dans la liste -> sans l'appliquer on compenserait la MAUVAISE image.
			struct ListMod {
					int32 idc = 0;			 // 0 = soustraire, 1 = ajouter
					int32 absDiffMinus1 = 0; // abs_diff_pic_num_minus1
			};
			NkVector<ListMod> listMods;
			bool hasLongTerm = false;
			// direct_spatial_mv_pred_flag (B uniquement) : 1 = Direct SPATIAL (le defaut x264), 0 = temporel.
			int32 directSpatial = 1;
			if (isB) {
				directSpatial = (int32)br.U1();
				// Le Direct SPATIAL seul est implemente (defaut x264) ; le temporel exigerait la mise
				// a l'echelle des MV de la co-localisee par les distances POC.
				if (!directSpatial)
					return false;
			}
			NkVector<ListMod> listMods1;					   // reordonnancement de RefPicList1 (B)
			int32 numRefActive = pps.numRefIdxL0DefaultActive;  // num_ref_idx_l0_active effectif
			int32 numRefActive1 = pps.numRefIdxL1DefaultActive; // num_ref_idx_l1_active effectif (B)
			if (isInter) {
				if (br.U1()) {							 // num_ref_idx_active_override_flag
					numRefActive = (int32)br.UE() + 1;	 // num_ref_idx_l0_active_minus1 + 1
					if (isB)
						numRefActive1 = (int32)br.UE() + 1; // num_ref_idx_l1_active_minus1 + 1
				}
				// ref_pic_list_modification_flag_l0 puis (B) _l1.
				auto readMods = [&](NkVector<ListMod> &dst) {
					if (!br.U1())
						return;
					uint32 idc;
					do {
						idc = br.UE();
						if (idc == 0 || idc == 1) {
							ListMod m;
							m.idc = (int32)idc;
							m.absDiffMinus1 = (int32)br.UE(); // abs_diff_pic_num_minus1
							if (dst.Size() < 32)
								dst.PushBack(m);
						} else if (idc == 2) {
							br.UE();		 // long_term_pic_num : references long terme non gerees
							hasLongTerm = true;
						}
					} while (idc != 3 && !br.Eof());
				};
				readMods(listMods);
				if (isB)
					readMods(listMods1);
			}
			// pred_weight_table() (§7.3.3.2) : presente en P/SP si weighted_pred_flag=1, et en B si
			// weighted_bipred_idc==1 (ponderation EXPLICITE ; l'idc==2 = IMPLICITE ne transmet aucune
			// table, les poids se derivent des distances POC). x264 active weightp PAR DEFAUT en Main
			// -> indispensable des qu'on sort du baseline, sinon tout le reste de l'en-tete est decale.
			int32 wpLumaDenom = 0, wpChromaDenom = 0;
			// [0] = liste L0, [1] = liste L1.
			int32 wpLumaW[2][16], wpLumaO[2][16], wpChromaW[2][16][2], wpChromaO[2][16][2];
			const bool useWeighted = (isP && pps.weightedPred != 0) || (isB && pps.weightedBipredIdc == 1);
			const bool useImplicit = (isB && pps.weightedBipredIdc == 2); // defaut x264 (weightb=1)
			for (int32 l = 0; l < 2; ++l)
				for (int32 i = 0; i < 16; ++i) {
					wpLumaW[l][i] = 1;
					wpLumaO[l][i] = 0;
					wpChromaW[l][i][0] = wpChromaW[l][i][1] = 1;
					wpChromaO[l][i][0] = wpChromaO[l][i][1] = 0;
				}
			if (useWeighted) {
				wpLumaDenom = (int32)br.UE();	// luma_log2_weight_denom
				wpChromaDenom = (int32)br.UE(); // chroma_log2_weight_denom (ChromaArrayType != 0)
				if (wpLumaDenom > 7 || wpChromaDenom > 7)
					return false;
				const int32 nLists = isB ? 2 : 1;
				for (int32 l = 0; l < nLists; ++l) {
					const int32 act = (l == 0) ? numRefActive : numRefActive1;
					const int32 nw = act > 16 ? 16 : act;
					for (int32 i = 0; i < 16; ++i) { // defaut si le drapeau est absent : poids neutre
						wpLumaW[l][i] = 1 << wpLumaDenom;
						wpLumaO[l][i] = 0;
						wpChromaW[l][i][0] = wpChromaW[l][i][1] = 1 << wpChromaDenom;
						wpChromaO[l][i][0] = wpChromaO[l][i][1] = 0;
					}
					for (int32 i = 0; i < nw; ++i) {
						if (br.U1()) { // luma_weight_lX_flag
							wpLumaW[l][i] = br.SE();
							wpLumaO[l][i] = br.SE();
						}
						if (br.U1()) { // chroma_weight_lX_flag
							for (int32 j = 0; j < 2; ++j) {
								wpChromaW[l][i][j] = br.SE();
								wpChromaO[l][i][j] = br.SE();
							}
						}
					}
				}
			}
			if (nalRefIdc != 0) {
				if (nalType == 5) {
					br.U1(); // no_output_of_prior_pics_flag
					br.U1(); // long_term_reference_flag
				} else if (br.U1()) { // adaptive_ref_pic_marking_mode_flag
					uint32 op;
					do {
						op = br.UE();
						if (op == 1 || op == 3)
							br.UE();
						if (op == 2)
							br.UE();
						if (op == 3 || op == 6)
							br.UE();
						if (op == 4)
							br.UE();
					} while (op != 0 && !br.Eof());
				}
			}
			// cabac_init_idc : present SEULEMENT si CABAC et slice non-I. Selectionne la variante de table
			// d'init des contextes. ⚠️ A lire meme si inutilise, sinon tout le reste de l'en-tete decale.
			int32 cabacInitIdc = 0;
			if (pps.entropyCodingMode == 1 && !isI) {
				cabacInitIdc = (int32)br.UE();
				if (cabacInitIdc < 0 || cabacInitIdc > 2)
					return false;
			}
			int32 qp = pps.picInitQp + br.SE();
			if (qp < 0 || qp > 51)
				return false;
			// deblocking_filter_control (présent dans ce PPS) : idc + offsets, AVANT les données MB.
			int32 disableDeblock = 0, alphaOff = 0, betaOff = 0;
			if (pps.deblockingControlPresent) {
				disableDeblock = (int32)br.UE(); // disable_deblocking_filter_idc
				if (disableDeblock != 1) {
					alphaOff = br.SE() * 2; // slice_alpha_c0_offset_div2 ×2
					betaOff = br.SE() * 2;	// slice_beta_offset_div2 ×2
				}
			}

			const int32 mbW = sps.picWidthInMbs;
			const int32 mbH = sps.picHeightInMapUnits; // frame_mbs_only=1
			if (mbW <= 0 || mbH <= 0)
				return false;
			const int32 lumaW = mbW * 16, lumaH = mbH * 16;
			const int32 chromaW = lumaW / 2, chromaH = lumaH / 2;

			out.y.Resize((uint64)lumaW * lumaH);
			out.cb.Resize((uint64)chromaW * chromaH);
			out.cr.Resize((uint64)chromaW * chromaH);
			for (uint64 i = 0; i < out.y.Size(); ++i)
				out.y[i] = 0;
			for (uint64 i = 0; i < out.cb.Size(); ++i) {
				out.cb[i] = 0;
				out.cr[i] = 0;
			}

			const int32 numMb = mbW * mbH;
			const uint64 n4 = (uint64)mbW * 4 * mbH * 4;
			NkVector<int32> lumaNz, cnz0, cnz1, i4mode, mvx4G, mvy4G, ref4G, mvdx4G, mvdy4G;
			NkVector<int32> mvx4G1, mvy4G1, ref4G1, mvdx4G1, mvdy4G1; // grilles de la liste L1 (B)
			NkVector<int32> direct4G; // par 4x4 : 1 si bloc Direct (contexte ref_idx des B)
			NkVector<uint8> shape16; // par MB : 1 si 16x16 ou intra (granularite du Direct spatial)
			NkVector<uint8> t8x8Grid; // par MB : 1 si transformée 8x8 (règle de déblocage High)
			lumaNz.Resize(n4);
			i4mode.Resize(n4);
			mvx4G.Resize(n4);
			mvy4G.Resize(n4);
			ref4G.Resize(n4);
			mvdx4G.Resize(n4);
			mvdy4G.Resize(n4);
			mvx4G1.Resize(n4);
			mvy4G1.Resize(n4);
			ref4G1.Resize(n4);
			mvdx4G1.Resize(n4);
			mvdy4G1.Resize(n4);
			cnz0.Resize((uint64)mbW * 2 * mbH * 2);
			cnz1.Resize((uint64)mbW * 2 * mbH * 2);
			for (uint64 i = 0; i < n4; ++i) {
				lumaNz[i] = 0;
				i4mode[i] = 2;
				mvx4G[i] = 0;
				mvy4G[i] = 0;
				ref4G[i] = -2; // 4x4 non décodé
				mvdx4G[i] = 0;
				mvdy4G[i] = 0;
				mvx4G1[i] = 0;
				mvy4G1[i] = 0;
				ref4G1[i] = -2;
				mvdx4G1[i] = 0;
				mvdy4G1[i] = 0;
			}
			direct4G.Resize(n4);
			for (uint64 i = 0; i < n4; ++i)
				direct4G[i] = 0;
			for (uint64 i = 0; i < cnz0.Size(); ++i) {
				cnz0[i] = 0;
				cnz1[i] = 0;
			}

			DecCtx c;
			c.Y = out.y.Data();
			c.Cb = out.cb.Data();
			c.Cr = out.cr.Data();
			c.lumaW = lumaW;
			c.lumaH = lumaH;
			c.chromaW = chromaW;
			c.chromaH = chromaH;
			c.mbW = mbW;
			c.mbH = mbH;
			c.nzW = mbW * 4;
			c.cnzW = mbW * 2;
			c.lumaNz = lumaNz.Data();
			c.chromaNz0 = cnz0.Data();
			c.chromaNz1 = cnz1.Data();
			c.i4mode = i4mode.Data();
			c.qp = qp;
			c.chromaQpOffset = pps.chromaQpIndexOffset;
			c.transform8x8Mode = pps.transform8x8Mode;
			c.directInference = sps.directInference;
			c.qpprimeBypass = sps.qpprimeBypass;
			// Lossless (§8.5.15) : seul le chemin CABAC gère le bypass — CAVLC+bypass = échec propre
			// plutôt qu'un décodage silencieusement faux.
			if (sps.qpprimeBypass && pps.entropyCodingMode == 0)
				return false;
			// Scaling matrices effectives (§8.5.9) : celles du PPS si présentes, sinon celles du SPS,
			// sinon listes PLATES (pointeurs nuls -> chemin de déquantification prouvé, inchangé).
			{
				const NkH264ScalingLists *eff =
					pps.scaling.present ? &pps.scaling : (sps.scaling.present ? &sps.scaling : nullptr);
				if (eff) {
					for (int32 i = 0; i < 6; ++i)
						c.wsl4[i] = eff->w4[i];
					c.wsl8[0] = eff->w8[0];
					c.wsl8[1] = eff->w8[1];
				}
			}
			// Grilles de mouvement : une par liste (L0 pour les P et les B, L1 pour les B seules).
			c.L[0].mvx4 = mvx4G.Data();
			c.L[0].mvy4 = mvy4G.Data();
			c.L[0].ref4 = ref4G.Data();
			shape16.Resize((uint64)(mbW * mbH));
			for (uint64 i = 0; i < shape16.Size(); ++i)
				shape16[i] = 1; // defaut : intra / 16x16
			c.mb16x16OrIntra = shape16.Data();
			t8x8Grid.Resize((uint64)(mbW * mbH));
			for (uint64 i = 0; i < t8x8Grid.Size(); ++i)
				t8x8Grid[i] = 0; // defaut : transformée 4x4
			c.transform8x8Mb = t8x8Grid.Data();
			c.L[0].mvdx4 = mvdx4G.Data();
			c.L[0].mvdy4 = mvdy4G.Data();
			c.L[1].mvx4 = mvx4G1.Data();
			c.L[1].mvy4 = mvy4G1.Data();
			c.L[1].ref4 = ref4G1.Data();
			c.L[1].mvdx4 = mvdx4G1.Data();
			c.L[1].mvdy4 = mvdy4G1.Data();
			c.direct4 = direct4G.Data();
			c.weightedPred = useWeighted ? 1 : 0;
			c.lumaLog2Denom = wpLumaDenom;
			c.chromaLog2Denom = wpChromaDenom;
			for (int32 l = 0; l < 2; ++l)
				for (int32 i = 0; i < 16; ++i) {
					c.L[l].lumaWeight[i] = wpLumaW[l][i];
					c.L[l].lumaOffset[i] = wpLumaO[l][i];
					for (int32 j = 0; j < 2; ++j) {
						c.L[l].chromaWeight[i][j] = wpChromaW[l][i][j];
						c.L[l].chromaOffset[i][j] = wpChromaO[l][i][j];
					}
				}
			if (isInter) {
				if (numRefs <= 0)
					return false; // référence incompatible
				if (hasLongTerm)
					return false; // references long terme non gerees
				const int32 nUse = numRefs > 16 ? 16 : numRefs;
				for (int32 i = 0; i < nUse; ++i) {
					const NkH264Frame *r = refs[i];
					if (!r || r->lumaW != lumaW || r->lumaH != lumaH)
						return false; // reference incompatible
				}
				const int32 maxPicNum = 1 << sps.log2MaxFrameNum;
				const int32 kNoPic = -1000000;
				int32 dpbPn[16]; // PicNum = FrameNumWrap (§8.2.4.1)
				for (int32 i = 0; i < nUse; ++i)
					dpbPn[i] = (refs[i]->frameNum > frameNum) ? refs[i]->frameNum - maxPicNum : refs[i]->frameNum;

				// ── Ordre initial (§8.2.4.2) : indices dans refs[] ───────────────────
				int32 init[2][16];
				int32 nInit[2] = {0, 0};
				if (isP) {
					// §8.2.4.2.1 : PicNum decroissant = l'ordre du DPB (plus recente d'abord).
					for (int32 i = 0; i < nUse; ++i)
						init[0][nInit[0]++] = i;
				} else {
					// §8.2.4.2.3 : ce sont les POC qui ordonnent (pas les PicNum). L0 = images du
					// PASSE par POC DECROISSANT puis du FUTUR par POC CROISSANT ; L1 = l'inverse.
					int32 past[16], nPast = 0, futu[16], nFutu = 0;
					for (int32 i = 0; i < nUse; ++i) {
						if (refs[i]->poc < poc)
							past[nPast++] = i;
						else if (refs[i]->poc > poc)
							futu[nFutu++] = i;
					}
					for (int32 a = 1; a < nPast; ++a) { // tri par insertion, POC decroissant
						const int32 v = past[a];
						int32 b = a - 1;
						while (b >= 0 && refs[past[b]]->poc < refs[v]->poc) {
							past[b + 1] = past[b];
							--b;
						}
						past[b + 1] = v;
					}
					for (int32 a = 1; a < nFutu; ++a) { // tri par insertion, POC croissant
						const int32 v = futu[a];
						int32 b = a - 1;
						while (b >= 0 && refs[futu[b]]->poc > refs[v]->poc) {
							futu[b + 1] = futu[b];
							--b;
						}
						futu[b + 1] = v;
					}
					for (int32 i = 0; i < nPast; ++i)
						init[0][nInit[0]++] = past[i];
					for (int32 i = 0; i < nFutu; ++i)
						init[0][nInit[0]++] = futu[i];
					for (int32 i = 0; i < nFutu; ++i)
						init[1][nInit[1]++] = futu[i];
					for (int32 i = 0; i < nPast; ++i)
						init[1][nInit[1]++] = past[i];
					// §8.2.4.2.3 : si les deux listes sont IDENTIQUES et de longueur > 1, echanger
					// les deux premieres entrees de L1 (sans quoi L0 et L1 prediraient a l'identique).
					if (nInit[0] == nInit[1] && nInit[1] > 1) {
						bool same = true;
						for (int32 i = 0; i < nInit[0]; ++i)
							if (init[0][i] != init[1][i]) {
								same = false;
								break;
							}
						if (same) {
							const int32 t = init[1][0];
							init[1][0] = init[1][1];
							init[1][1] = t;
						}
					}
				}

				// ── Reordonnancement (§8.2.4.3.1) + troncature, pour UNE liste ───────
				// L'image est prise dans le DPB, inseree en refIdx apres decalage a droite, puis la
				// compaction retire ses AUTRES copies a partir de refIdx (les entrees deja placees
				// avant refIdx sont donc protegees : c'est ainsi qu'une meme image peut figurer
				// DEUX FOIS dans la liste — exactement ce que fait x264 en weightp=2, via un
				// abs_diff_pic_num == MaxPicNum qui reboucle sur la meme image).
				auto buildList = [&](int32 li, const NkVector<ListMod> &mods, int32 nActive) -> bool {
					// La liste de travail va jusqu'a l'indice nAct INCLUS (le §8.2.4.3.1 manipule
					// num_ref_idx_active+1 entrees avant troncature).
					const int32 nAct = (nActive < 1) ? 1 : (nActive > 16 ? 16 : nActive);
					const uint8 *ly[18], *lcb[18], *lcr[18];
					int32 lpn[18], lpoc[18];
					for (int32 i = 0; i <= nAct + 1; ++i) {
						if (i < nInit[li]) {
							const NkH264Frame *r = refs[init[li][i]];
							ly[i] = r->y.Data();
							lcb[i] = r->cb.Data();
							lcr[i] = r->cr.Data();
							lpn[i] = dpbPn[init[li][i]];
							lpoc[i] = r->poc;
						} else {
							ly[i] = lcb[i] = lcr[i] = nullptr;
							lpn[i] = kNoPic;
							lpoc[i] = 0;
						}
					}
					int32 refIdx = 0, picNumPred = frameNum;
					for (uint64 m = 0; m < mods.Size(); ++m) {
						const int32 absDiff = mods[m].absDiffMinus1 + 1;
						int32 noWrap;
						if (mods[m].idc == 0) {
							noWrap = picNumPred - absDiff;
							if (noWrap < 0)
								noWrap += maxPicNum;
						} else {
							noWrap = picNumPred + absDiff;
							if (noWrap >= maxPicNum)
								noWrap -= maxPicNum;
						}
						picNumPred = noWrap;
						const int32 picNum = (noWrap > frameNum) ? noWrap - maxPicNum : noWrap;
						int32 src = -1;
						for (int32 j = 0; j < nUse; ++j)
							if (dpbPn[j] == picNum) {
								src = j;
								break;
							}
						if (src < 0 || refIdx > nAct)
							return false; // reference absente du DPB / flux invalide
						const uint8 *sy = refs[src]->y.Data();
						const uint8 *scb = refs[src]->cb.Data();
						const uint8 *scr = refs[src]->cr.Data();
						for (int32 cIdx = nAct; cIdx > refIdx; --cIdx) {
							ly[cIdx] = ly[cIdx - 1];
							lcb[cIdx] = lcb[cIdx - 1];
							lcr[cIdx] = lcr[cIdx - 1];
							lpn[cIdx] = lpn[cIdx - 1];
							lpoc[cIdx] = lpoc[cIdx - 1];
						}
						ly[refIdx] = sy;
						lcb[refIdx] = scb;
						lcr[refIdx] = scr;
						lpn[refIdx] = picNum;
						lpoc[refIdx] = refs[src]->poc;
						++refIdx;
						int32 nIdx = refIdx;
						for (int32 cIdx = refIdx; cIdx <= nAct; ++cIdx) {
							if (lpn[cIdx] != picNum) {
								ly[nIdx] = ly[cIdx];
								lcb[nIdx] = lcb[cIdx];
								lcr[nIdx] = lcr[cIdx];
								lpn[nIdx] = lpn[cIdx];
								lpoc[nIdx] = lpoc[cIdx];
								++nIdx;
							}
						}
					}
					// Sans reordonnancement la liste initiale suffit : on garde alors ses entrees.
					const int32 nFinal = mods.Size() > 0 ? nAct : (nAct < nInit[li] ? nAct : nInit[li]);
					for (int32 i = 0; i < nFinal; ++i) {
						if (!ly[i])
							return false; // entree "aucune image de reference" : flux invalide
						c.L[li].y[i] = ly[i];
						c.L[li].cb[i] = lcb[i];
						c.L[li].cr[i] = lcr[i];
						c.L[li].poc[i] = lpoc[i];
					}
					c.L[li].numRefs = nFinal;
					c.L[li].numRefActive = nFinal;
					return nFinal > 0;
				};
				if (!buildList(0, listMods, numRefActive))
					return false;
				if (isB && !buildList(1, listMods1, numRefActive1))
					return false;
				// ── Image CO-LOCALISEE = RefPicList1[0] (§8.4.1.2) ───────────────────
				// Le Direct spatial y teste si le bloc est immobile. On la retrouve par identite de
				// plan (buildList ne garde que des pointeurs) ; il faut aussi son champ de mouvement,
				// donc l'image DOIT l'avoir conserve (mvW correct).
				if (isB) {
					for (int32 i = 0; i < nUse; ++i) {
						if (refs[i]->y.Data() != c.L[1].y[0])
							continue;
						if (refs[i]->mvW != c.nzW || refs[i]->mvL0Ref.Size() == 0)
							break; // mouvement absent/incompatible -> pas de col_zero
						c.colL0x = refs[i]->mvL0x.Data();
						c.colL0y = refs[i]->mvL0y.Data();
						c.colL0Ref = refs[i]->mvL0Ref.Data();
						c.colL1x = refs[i]->mvL1x.Data();
						c.colL1y = refs[i]->mvL1y.Data();
						c.colL1Ref = refs[i]->mvL1Ref.Data();
						if (refs[i]->mb16x16OrIntra.Size() == (uint64)(mbW * mbH))
							c.col16x16OrIntra = refs[i]->mb16x16OrIntra.Data();
						break;
					}
				}

				// ── Poids IMPLICITES des B (§8.4.2.3.1, weighted_bipred_idc == 2) ────
				// Aucune table n'est transmise : les poids se derivent des distances POC entre
				// l'image courante et les deux references. denom = 5, offsets nuls.
				c.biPredMode = useWeighted ? 1 : (useImplicit ? 2 : 0);
				if (useImplicit) {
					// Cas degenere : une seule ref de chaque cote, exactement symetriques autour de
					// l'image courante -> poids 32/32 = simple moyenne, on n'active pas la ponderation.
					if (c.L[0].numRefs == 1 && c.L[1].numRefs == 1 &&
						c.L[0].poc[0] + c.L[1].poc[0] == 2 * poc) {
						c.biPredMode = 0;
					} else {
						c.lumaLog2Denom = 5;
						c.chromaLog2Denom = 5;
						for (int32 r0 = 0; r0 < 16; ++r0)
							for (int32 r1 = 0; r1 < 16; ++r1) {
								int32 w = 32; // defaut : moyenne
								if (r0 < c.L[0].numRefs && r1 < c.L[1].numRefs) {
									const int32 poc0 = c.L[0].poc[r0], poc1 = c.L[1].poc[r1];
									const int32 td = ClampI8(poc1 - poc0);
									if (td != 0) {
										const int32 tb = ClampI8(poc - poc0);
										const int32 ad = td < 0 ? -td : td;
										const int32 tx = (16384 + (ad >> 1)) / td;
										const int32 dsf = (tb * tx + 32) >> 8;
										if (dsf >= -64 && dsf <= 128)
											w = 64 - dsf; // w0 ; w1 = 64 - w0 = dsf
									}
								}
								c.implicitW0[r0][r1] = w;
							}
					}
				}
			}

			NkVector<int32> mbQp;
			mbQp.Resize((uint64)numMb);
			if (pps.entropyCodingMode == 1) {
				// ── CABAC (profils Main/High) — I, P et B ─────────────────────────────
				// ETAT : I et P BIT-EXACTS vs la reference (P : 96/96 combinaisons aux reglages
				// x264 Main PAR DEFAUT). Les B (Direct spatial) sont NEUVES et pas encore validees.
				const usize byteStart = (br.pos + 7) / 8; // cabac_alignment_one_bit -> octet
				CabacMb cab;
				cab.InitSlice(mbW, mbH, c.qp, isI, cabacInitIdc);
				cab.e.InitEngine(rbsp.Data(), (usize)rbsp.Size(), byteStart);
				int32 mbAddr = 0;
				while (mbAddr < numMb) {
					const int32 mbX = mbAddr % mbW, mbY = mbAddr / mbW;
					const int32 mbIdx = mbY * mbW + mbX;
					bool pcmMb = false; // I_PCM : QP de déblocage = 0 (c.qp, lui, reste inchangé)
					if (isB) {
						// B_Skip = B_Direct_16x16 sans residu ; sinon mb_type B (ou intra).
						if (DecodeMbSkipBCabac(cab, mbX, mbY)) {
							if (!DecodeMbCabacB(c, cab, mbX, mbY, 0, true))
								return false;
						} else {
							bool intraInB = false;
							const int32 mbTypeB = DecodeMbTypeBCabac(cab, mbX, mbY, intraInB);
							if (!intraInB) {
								if (!DecodeMbCabacB(c, cab, mbX, mbY, mbTypeB, false))
									return false;
							} else {
								// MB intra dans une B : aucune des deux listes n'est utilisee.
								ClearMvdGrid(c, c.L[0], mbX, mbY);
								ClearMvdGrid(c, c.L[1], mbX, mbY);
								for (int32 y = 0; y < 4; ++y)
									for (int32 x = 0; x < 4; ++x) {
										const int32 gi = (mbY * 4 + y) * c.nzW + (mbX * 4 + x);
										c.L[0].ref4[gi] = -1;
										c.L[1].ref4[gi] = -1;
									}
								if (mbTypeB == 0) {
									if (!DecodeMbCabacINxN(c, cab, mbX, mbY))
										return false;
								} else if (mbTypeB <= 24) {
									if (!DecodeMbCabacI16x16(c, cab, mbX, mbY, mbTypeB))
										return false;
								} else { // I_PCM
									if (!DecodeMbCabacIPcm(c, cab, mbX, mbY))
										return false;
									pcmMb = true;
								}
								cab.mbSkipMb[(uint64)mbIdx] = 0;
								cab.mbDirectMb[(uint64)mbIdx] = 0;
								c.mb16x16OrIntra[mbIdx] = 1; // intra
							}
						}
						mbQp[(uint64)mbAddr] = pcmMb ? 0 : c.qp;
						++mbAddr;
						if (cab.Terminate()) // end_of_slice_flag
							break;
						continue;
					}
					if (isP && DecodeMbSkipCabac(cab, mbX, mbY)) {
						// P_Skip : aucun residu, MV predite. Les grilles voisines passent a "vide".
						DecodeMbSkip(c, mbX, mbY);
						cab.mbTypeClass[(uint64)mbIdx] = 1;
						cab.cbpLumaMb[(uint64)mbIdx] = 0;
						cab.cbpChromaMb[(uint64)mbIdx] = 0;
						cab.chromaModeMb[(uint64)mbIdx] = 0;
						cab.lumaDcCodedMb[(uint64)mbIdx] = 0;
						cab.chromaDcCodedMb[(uint64)mbIdx] = 0;
						cab.mbSkipMb[(uint64)mbIdx] = 1;
						cab.prevQpDeltaNonZero = 0;
						ClearMvdGrid(c, c.L[0], mbX, mbY);
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x)
								c.lumaNz[(mbY * 4 + y) * c.nzW + (mbX * 4 + x)] = 0;
						for (int32 y = 0; y < 2; ++y)
							for (int32 x = 0; x < 2; ++x) {
								c.chromaNz0[(mbY * 2 + y) * c.cnzW + (mbX * 2 + x)] = 0;
								c.chromaNz1[(mbY * 2 + y) * c.cnzW + (mbX * 2 + x)] = 0;
							}
					} else {
						bool intraInP = false;
						const int32 mbType =
							isP ? DecodeMbTypePCabac(cab, intraInP) : DecodeMbTypeICabac(cab, mbX, mbY);
						if (isP && !intraInP) {
							if (!DecodeMbCabacP(c, cab, mbX, mbY, mbType))
								return false;
						} else {
							// MB intra (I-slice, ou intra a l'interieur d'une P-slice).
							if (isP) {
								ClearMvdGrid(c, c.L[0], mbX, mbY);
								for (int32 y = 0; y < 4; ++y)
									for (int32 x = 0; x < 4; ++x)
										c.L[0].ref4[(mbY * 4 + y) * c.nzW + (mbX * 4 + x)] = -1; // intra
							}
							if (mbType == 0) {
								if (!DecodeMbCabacINxN(c, cab, mbX, mbY))
									return false;
							} else if (mbType <= 24) {
								if (!DecodeMbCabacI16x16(c, cab, mbX, mbY, mbType))
									return false;
							} else { // I_PCM
								if (!DecodeMbCabacIPcm(c, cab, mbX, mbY))
									return false;
								pcmMb = true;
							}
							cab.mbSkipMb[(uint64)mbIdx] = 0;
						}
					}
					mbQp[(uint64)mbAddr] = pcmMb ? 0 : c.qp;
					++mbAddr;
					if (cab.Terminate()) // end_of_slice_flag
						break;
				}
			} else if (isI) {
				for (int32 mbAddr = 0; mbAddr < numMb; ++mbAddr) {
					const int32 mbX = mbAddr % mbW, mbY = mbAddr / mbW;
					const uint32 mbType = br.UE();
					if (mbType == 0) {
						if (!DecodeMbI4x4(c, br, mbX, mbY))
							return false;
					} else if (mbType <= 24) {
						if (!DecodeMbI16x16(c, br, mbX, mbY, (int32)mbType))
							return false;
					} else {
						return false; // I_PCM
					}
					mbQp[(uint64)mbAddr] = c.qp;
				}
			} else { // P-slice : mb_skip_run + P_L0_16x16 / intra
				int32 mbAddr = 0;
				while (mbAddr < numMb) {
					uint32 skipRun = br.UE();
					for (uint32 s = 0; s < skipRun && mbAddr < numMb; ++s) {
						DecodeMbSkip(c, mbAddr % mbW, mbAddr / mbW);
						mbQp[(uint64)mbAddr] = c.qp;
						++mbAddr;
					}
					if (mbAddr >= numMb)
						break;
					const int32 mbX = mbAddr % mbW, mbY = mbAddr / mbW;
					const uint32 mbType = br.UE();
					if (mbType <= 4) { // P_16x16(0)/16x8(1)/8x16(2)/8x8(3)/8x8ref0(4)
						if (!DecodeMbInterP(c, br, mbX, mbY, (int32)mbType))
							return false;
					} else { // intra dans une P-slice : I mb_type = mbType-5
						const int32 iType = (int32)mbType - 5;
						if (iType == 0) {
							if (!DecodeMbI4x4(c, br, mbX, mbY))
								return false;
						} else if (iType <= 24) {
							if (!DecodeMbI16x16(c, br, mbX, mbY, iType))
								return false;
						} else {
							return false; // I_PCM
						}
						// MB intra -> ref4 = -1 (voisin non disponible pour la prédiction de MV)
						const int32 bx0 = mbX * 4, by0 = mbY * 4;
						for (int32 by = by0; by < by0 + 4; ++by)
							for (int32 bx = bx0; bx < bx0 + 4; ++bx)
								c.L[0].ref4[by * c.nzW + bx] = -1;
					}
					mbQp[(uint64)mbAddr] = c.qp;
					++mbAddr;
				}
			}

			// Déblocage en boucle (§8.7), intra ET inter (bS par segment 4x4).
			if (disableDeblock != 1)
				Deblock(c, mbQp.Data(), alphaOff, betaOff, isB);

			out.lumaW = lumaW;
			out.lumaH = lumaH;
			out.chromaW = chromaW;
			out.chromaH = chromaH;
			out.cropW = sps.width;
			out.cropH = sps.height;
			out.frameNum = frameNum; // identifie cette image comme reference (PicNum) pour les P suivantes
			out.poc = poc;			 // ordre d'affichage + ordonnancement des listes L0/L1 des B
			out.pocLsb = pocLsb;
			out.pocMsb = pocMsb;
			out.isReference = (nalRefIdc != 0); // sinon : ne PAS inserer dans le DPB
			// Conserve le champ de mouvement : une B ultérieure lira celui de sa co-localisée
			// (RefPicList1[0]) pour son Direct spatial (§8.4.1.2.2).
			out.mvW = mbW * 4;
			out.mvH = mbH * 4;
			out.mb16x16OrIntra.Resize((uint64)(mbW * mbH));
			for (uint64 i = 0; i < out.mb16x16OrIntra.Size(); ++i)
				out.mb16x16OrIntra[i] = shape16[i];
			out.mvL0x.Resize(n4);
			out.mvL0y.Resize(n4);
			out.mvL0Ref.Resize(n4);
			out.mvL1x.Resize(n4);
			out.mvL1y.Resize(n4);
			out.mvL1Ref.Resize(n4);
			for (uint64 i = 0; i < n4; ++i) {
				out.mvL0x[i] = mvx4G[i];
				out.mvL0y[i] = mvy4G[i];
				out.mvL0Ref[i] = (ref4G[i] < 0) ? -1 : ref4G[i]; // -2 (non decode) et -1 (intra) -> -1
				out.mvL1x[i] = mvx4G1[i];
				out.mvL1y[i] = mvy4G1[i];
				out.mvL1Ref[i] = (ref4G1[i] < 0) ? -1 : ref4G1[i];
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
