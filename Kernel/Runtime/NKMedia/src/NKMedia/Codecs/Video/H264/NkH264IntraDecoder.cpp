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

			// Contexte de décodage d'une image (plans + grilles de voisinage).
			struct DecCtx {
					uint8 *Y = nullptr, *Cb = nullptr, *Cr = nullptr;
					int32 lumaW = 0, lumaH = 0, chromaW = 0, chromaH = 0;
					int32 mbW = 0, mbH = 0;
					int32 nzW = 0, cnzW = 0;
					int32 *lumaNz = nullptr, *chromaNz0 = nullptr, *chromaNz1 = nullptr, *i4mode = nullptr;
					int32 qp = 26;
					int32 chromaQpOffset = 0;
					// Inter (P-slice) : LISTE de plans de référence (RefPicList0, multi-référence) + grilles
					// MV/ref PAR BLOC 4x4. ref4 : -2 non décodé, -1 intra, >=0 = index dans la liste de réf.
					const uint8 *refListY[16] = {nullptr};
					const uint8 *refListCb[16] = {nullptr};
					const uint8 *refListCr[16] = {nullptr};
					int32 numRefs = 0;		// nombre de références disponibles (taille RefPicList0)
					int32 numRefActive = 1; // num_ref_idx_l0_active (PPS default / override slice)
					// Compat : refY/refCb/refCr = refListY[0] (référence la plus récente).
					const uint8 *refY = nullptr, *refCb = nullptr, *refCr = nullptr;
					int32 *mvx4 = nullptr, *mvy4 = nullptr, *ref4 = nullptr;
			};

			// MC luma quart-pel (§8.4.2.2.1) d'un rectangle w×h -> écrit dans predY[16*16] à (ox,oy).
			void McLumaRect(const DecCtx &c, int32 dx, int32 dy, int32 ox, int32 oy, int32 w, int32 h, int32 mvx,
							int32 mvy, uint8 predY[256], int32 refIdx = 0) {
				const int32 W = c.lumaW, H = c.lumaH;
				if (refIdx < 0)
					refIdx = 0;
				if (refIdx >= c.numRefs)
					refIdx = c.numRefs > 0 ? c.numRefs - 1 : 0;
				const uint8 *ref = c.refListY[refIdx];
				auto R = [&](int32 x, int32 y) -> int32 {
					x = x < 0 ? 0 : (x >= W ? W - 1 : x);
					y = y < 0 ? 0 : (y >= H ? H - 1 : y);
					return ref[(usize)y * W + x];
				};
				auto Hor = [&](int32 x, int32 y) -> int32 {
					return R(x - 2, y) - 5 * R(x - 1, y) + 20 * R(x, y) + 20 * R(x + 1, y) - 5 * R(x + 2, y) + R(x + 3, y);
				};
				auto Ver = [&](int32 x, int32 y) -> int32 {
					return R(x, y - 2) - 5 * R(x, y - 1) + 20 * R(x, y) + 20 * R(x, y + 1) - 5 * R(x, y + 2) + R(x, y + 3);
				};
				auto Bh = [&](int32 x, int32 y) -> int32 { return ClampU8((Hor(x, y) + 16) >> 5); };
				auto Hh = [&](int32 x, int32 y) -> int32 { return ClampU8((Ver(x, y) + 16) >> 5); };
				auto Jj = [&](int32 x, int32 y) -> int32 {
					const int32 j1 = Hor(x, y - 2) - 5 * Hor(x, y - 1) + 20 * Hor(x, y) + 20 * Hor(x, y + 1) -
									 5 * Hor(x, y + 2) + Hor(x, y + 3);
					return ClampU8((j1 + 512) >> 10);
				};
				const int32 fx = mvx & 3, fy = mvy & 3;
				const int32 bx = dx + (mvx >> 2), by = dy + (mvy >> 2);
				for (int32 y = 0; y < h; ++y)
					for (int32 x = 0; x < w; ++x) {
						const int32 ix = bx + x, iy = by + y;
						int32 v;
						switch (fy * 4 + fx) {
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
						predY[(oy + y) * 16 + (ox + x)] = (uint8)v;
					}
			}

			// MC chroma 1/8-pel bilinéaire (§8.4.2.2.2) d'un rectangle cw×ch -> cPred[64] à (ox,oy).
			void McChromaRect(const DecCtx &c, int32 comp, int32 dx, int32 dy, int32 ox, int32 oy, int32 cw, int32 ch,
							  int32 mvx, int32 mvy, uint8 cPred[64], int32 refIdx = 0) {
				if (refIdx < 0)
					refIdx = 0;
				if (refIdx >= c.numRefs)
					refIdx = c.numRefs > 0 ? c.numRefs - 1 : 0;
				const uint8 *ref = (comp == 0) ? c.refListCb[refIdx] : c.refListCr[refIdx];
				const int32 W = c.chromaW, H = c.chromaH;
				const int32 fx = mvx & 7, fy = mvy & 7, oxx = mvx >> 3, oyy = mvy >> 3;
				auto C = [&](int32 x, int32 y) -> int32 {
					x = x < 0 ? 0 : (x >= W ? W - 1 : x);
					y = y < 0 ? 0 : (y >= H ? H - 1 : y);
					return ref[(usize)y * W + x];
				};
				for (int32 y = 0; y < ch; ++y)
					for (int32 x = 0; x < cw; ++x) {
						const int32 rx = dx + x + oxx, ry = dy + y + oyy;
						const int32 A = C(rx, ry), Bb = C(rx + 1, ry), Cc = C(rx, ry + 1), D = C(rx + 1, ry + 1);
						cPred[(oy + y) * 8 + (ox + x)] = (uint8)(((8 - fx) * (8 - fy) * A + fx * (8 - fy) * Bb +
																 (8 - fx) * fy * Cc + fx * fy * D + 32) >>
																6);
					}
			}

			// Voisin 4x4 pour la prédiction de MV : dispo + ref (-1 = intra/hors) + MV.
			void GetMv4(const DecCtx &c, int32 x4, int32 y4, bool &avail, int32 &ref, int32 &mx, int32 &my) {
				const int32 w4 = c.mbW * 4, h4 = c.mbH * 4;
				if (x4 < 0 || y4 < 0 || x4 >= w4 || y4 >= h4 || c.ref4[y4 * c.nzW + x4] == -2) {
					avail = false;
					ref = -1;
					mx = my = 0;
					return;
				}
				avail = true;
				ref = c.ref4[y4 * c.nzW + x4];
				// Voisin INTRA (ref==-1) : sa MV vaut 0 pour la prediction (H.264 §8.4.1.3.2),
				// PAS la valeur périmée de mvx4/mvy4 (StoreMv4 n'est appelé que pour l'inter).
				if (ref < 0) {
					mx = my = 0;
				} else {
					mx = c.mvx4[y4 * c.nzW + x4];
					my = c.mvy4[y4 * c.nzW + x4];
				}
			}

			// Stocke la MV + l'index de référence d'une partition (grille 4x4). ref4 = refIdx (>=0 inter).
			void StoreMv4(const DecCtx &c, int32 bx, int32 by, int32 pw, int32 ph, int32 mvx, int32 mvy,
						  int32 refIdx) {
				for (int32 y = by; y < by + ph; ++y)
					for (int32 x = bx; x < bx + pw; ++x) {
						c.mvx4[y * c.nzW + x] = mvx;
						c.mvy4[y * c.nzW + x] = mvy;
						c.ref4[y * c.nzW + x] = refIdx;
					}
			}

			int32 Med3(int32 a, int32 b, int32 cc) {
				const int32 mn = a < b ? (a < cc ? a : cc) : (b < cc ? b : cc);
				const int32 mx = a > b ? (a > cc ? a : cc) : (b > cc ? b : cc);
				return a + b + cc - mn - mx;
			}

			// Prédiction de MV d'une partition (§8.4.1.3). (bx,by) = coin 4x4, pw/ph = taille 4x4,
			// mbType (1=16x8, 2=8x16 -> cas directionnels), part = index. refIdx=0 (baseline ref=1).
			void PredMvPart(const DecCtx &c, int32 bx, int32 by, int32 pw, int32 mbType, int32 part, int32 refIdx,
							int32 &pmx, int32 &pmy) {
				bool avA, avB, avC;
				int32 rA, rB, rC, ax, ay, bxx, byy, cx, cy;
				GetMv4(c, bx - 1, by, avA, rA, ax, ay);
				GetMv4(c, bx, by - 1, avB, rB, bxx, byy);
				GetMv4(c, bx + pw, by - 1, avC, rC, cx, cy);
				if (!avC)
					GetMv4(c, bx - 1, by - 1, avC, rC, cx, cy); // repli D
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
				const int32 bx = mbX * 4, by = mbY * 4;
				bool avA, avB;
				int32 rA, rB, ax, ay, bxx, byy;
				GetMv4(c, bx - 1, by, avA, rA, ax, ay);
				GetMv4(c, bx, by - 1, avB, rB, bxx, byy);
				if (!avA || !avB || (rA == 0 && ax == 0 && ay == 0) || (rB == 0 && bxx == 0 && byy == 0)) {
					smx = smy = 0;
					return;
				}
				PredMvPart(c, bx, by, 4, 0, 0, 0, smx, smy); // P_Skip -> référence 0
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
			void DecodeChromaResidual(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 cbpChroma,
									  const uint8 cPred[2][64]) {
				const int32 qpC = NkH264Transform::ChromaQp(Clamp(c.qp + c.chromaQpOffset, 0, 51));
				const int32 cpx = mbX * 8, cpy = mbY * 8;

				// DC chroma (4 coeffs raster 2x2 par composante) si cbp&3.
				int32 cDcRec[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
				if (cbpChroma & 3) {
					for (int32 comp = 0; comp < 2; ++comp) {
						int32 dcLvl[4];
						NkH264Cavlc::DecodeResidual(br, dcLvl, 4, -1);
						int32 gdc[4];
						NkH264Transform::Hadamard2x2(dcLvl, gdc);
						NkH264Transform::DequantChromaDC(gdc, cDcRec[comp], qpC);
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
						NkH264Transform::Dequant4x4(acRaster, deq, qpC);
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
				DecodeChromaResidual(c, br, mbX, mbY, cbpChroma, cPred);
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
					NkH264Transform::Dequant4x4(raster, deq, c.qp);
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
					NkH264Transform::DequantDC(gDc, dcRec, 16, c.qp);
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
					NkH264Transform::Dequant4x4(acRaster, deq, c.qp);
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
				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				int32 smx, smy;
				SkipMv(c, mbX, mbY, smx, smy);
				uint8 predY[256], cp[64];
				McLumaRect(c, px, py, 0, 0, 16, 16, smx, smy, predY);
				for (int32 y = 0; y < 16; ++y)
					for (int32 x = 0; x < 16; ++x)
						c.Y[(usize)(py + y) * c.lumaW + px + x] = predY[y * 16 + x];
				McChromaRect(c, 0, cpx, cpy, 0, 0, 8, 8, smx, smy, cp);
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cb[(usize)(cpy + y) * c.chromaW + cpx + x] = cp[y * 8 + x];
				McChromaRect(c, 1, cpx, cpy, 0, 0, 8, 8, smx, smy, cp);
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x)
						c.Cr[(usize)(cpy + y) * c.chromaW + cpx + x] = cp[y * 8 + x];
				ClearMbNz(c, mbX, mbY);
				StoreMv4(c, mbX * 4, mbY * 4, 4, 4, smx, smy, 0); // P_Skip -> référence 0
			}

			// --- Macrobloc inter P : partitions 16x16 / 16x8 / 8x16 / 8x8(+sous-mb) ---
			bool DecodeMbInterP(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 mbType) {
				const int32 px = mbX * 16, py = mbY * 16, cpx = mbX * 8, cpy = mbY * 8;
				const int32 gbx = mbX * 4, gby = mbY * 4;
				uint8 predY[256], cPred[2][64];

				// Compense une partition (bx4,by4 = coin 4x4 ; pw4/ph4 = taille 4x4) + stocke MV/refIdx.
				auto doPart = [&](int32 bx4, int32 by4, int32 pw4, int32 ph4, int32 mvx, int32 mvy, int32 ri) {
					McLumaRect(c, px + bx4 * 4, py + by4 * 4, bx4 * 4, by4 * 4, pw4 * 4, ph4 * 4, mvx, mvy, predY, ri);
					McChromaRect(c, 0, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[0], ri);
					McChromaRect(c, 1, cpx + bx4 * 2, cpy + by4 * 2, bx4 * 2, by4 * 2, pw4 * 2, ph4 * 2, mvx, mvy,
								 cPred[1], ri);
					StoreMv4(c, gbx + bx4, gby + by4, pw4, ph4, mvx, mvy, ri);
				};

				// Lit un ref_idx_l0 (te(v)) : range = numRefActive-1. range==0 -> 0 (non codé) ;
				// range==1 -> 1 bit inversé ; range>1 -> ue(v). (H.264 §9.1.1)
				const bool readRef = (c.numRefActive > 1);
				auto readRefIdx = [&]() -> int32 {
					if (!readRef)
						return 0;
					if (c.numRefActive == 2)
						return 1 - (int32)br.U1();
					return (int32)br.UE();
				};

				if (mbType == 0) { // P_L0_16x16 : 1 ref_idx puis 1 mvd
					const int32 ri = readRefIdx();
					int32 pmx, pmy;
					PredMvPart(c, gbx, gby, 4, 0, 0, ri, pmx, pmy);
					doPart(0, 0, 4, 4, pmx + br.SE(), pmy + br.SE(), ri);
				} else if (mbType == 1) { // P_L0_L0_16x8 : ref_idx[0..1] puis mvd[0..1]
					const int32 ri0 = readRefIdx(), ri1 = readRefIdx();
					const int32 ri[2] = {ri0, ri1};
					for (int32 part = 0; part < 2; ++part) {
						const int32 by4 = part * 2;
						int32 pmx, pmy;
						PredMvPart(c, gbx, gby + by4, 4, 1, part, ri[part], pmx, pmy);
						doPart(0, by4, 4, 2, pmx + br.SE(), pmy + br.SE(), ri[part]);
					}
				} else if (mbType == 2) { // P_L0_L0_8x16 : ref_idx[0..1] puis mvd[0..1]
					const int32 ri0 = readRefIdx(), ri1 = readRefIdx();
					const int32 ri[2] = {ri0, ri1};
					for (int32 part = 0; part < 2; ++part) {
						const int32 bx4 = part * 2;
						int32 pmx, pmy;
						PredMvPart(c, gbx + bx4, gby, 2, 2, part, ri[part], pmx, pmy);
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
							PredMvPart(c, gbx + sbx, gby + sby, 2, 3, 0, ri[b], pmx, pmy);
							doPart(sbx, sby, 2, 2, pmx + br.SE(), pmy + br.SE(), ri[b]);
						} else if (subType[b] == 1) { // 8x4
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, gbx + sbx, gby + sby + sp, 2, 3, 0, ri[b], pmx, pmy);
								doPart(sbx, sby + sp, 2, 1, pmx + br.SE(), pmy + br.SE(), ri[b]);
							}
						} else if (subType[b] == 2) { // 4x8
							for (int32 sp = 0; sp < 2; ++sp) {
								PredMvPart(c, gbx + sbx + sp, gby + sby, 1, 3, 0, ri[b], pmx, pmy);
								doPart(sbx + sp, sby, 1, 2, pmx + br.SE(), pmy + br.SE(), ri[b]);
							}
						} else { // 4x4
							for (int32 sp = 0; sp < 4; ++sp) {
								const int32 pbx = sbx + (sp & 1), pby = sby + (sp >> 1);
								PredMvPart(c, gbx + pbx, gby + pby, 1, 3, 0, ri[b], pmx, pmy);
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
					NkH264Transform::Dequant4x4(raster, deq, c.qp);
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col)
							c.Y[(usize)(py + y4 + r) * c.lumaW + px + x4 + col] =
								ClampU8(predY[(y4 + r) * 16 + (x4 + col)] + resRec[r * 4 + col]);
				}

				DecodeChromaResidual(c, br, mbX, mbY, cbpChroma, cPred);
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
			const int32 kTc0[52][3] = {
				{0, 0, 0},	 {0, 0, 0},	  {0, 0, 0},   {0, 0, 0},  {0, 0, 0},  {0, 0, 0},  {0, 0, 0},	  {0, 0, 0},
				{0, 0, 0},	 {0, 0, 0},	  {0, 0, 0},   {0, 0, 0},  {0, 0, 0},  {0, 0, 0},  {0, 0, 0},	  {0, 0, 0},
				{0, 0, 0},	 {0, 0, 1},	  {0, 0, 1},   {0, 0, 1},  {0, 0, 1},  {0, 0, 1},  {0, 0, 1},	  {0, 1, 1},
				{0, 1, 1},	 {1, 1, 1},	  {1, 1, 1},   {1, 1, 1},  {1, 1, 1},  {1, 1, 2},  {1, 1, 2},	  {1, 1, 2},
				{1, 1, 2},	 {1, 2, 3},	  {1, 2, 3},   {2, 2, 3},  {2, 2, 4},  {2, 3, 4},  {2, 3, 4},	  {3, 3, 5},
				{3, 4, 6},	 {3, 4, 6},	  {4, 5, 7},   {4, 5, 8},  {5, 6, 9},  {6, 7, 10}, {6, 8, 11},  {7, 9, 12},
				{8, 10, 13}, {9, 12, 15}, {10, 13, 17}, {11, 17, 25}};

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
			void Deblock(DecCtx &c, const int32 *mbQp, int32 alphaOff, int32 betaOff) {
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
				// Force de bord entre les blocs 4x4 q(qx,qy) et p(px,py) ; mbB = bord de MB.
				auto bsOf = [&](int32 qx, int32 qy, int32 px, int32 py, bool mbB) -> int32 {
					const int32 rq = c.ref4[qy * nzW + qx], rp = c.ref4[py * nzW + px];
					if (rq < 0 || rp < 0)
						return mbB ? 4 : 3; // intra impliqué
					if (c.lumaNz[qy * nzW + qx] > 0 || c.lumaNz[py * nzW + px] > 0)
						return 2;
					const int32 dx = c.mvx4[qy * nzW + qx] - c.mvx4[py * nzW + px];
					const int32 dy = c.mvy4[qy * nzW + qx] - c.mvy4[py * nzW + px];
					const int32 adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
					return (adx >= 4 || ady >= 4) ? 1 : 0;
				};

				for (int32 mbY = 0; mbY < c.mbH; ++mbY)
					for (int32 mbX = 0; mbX < mbW; ++mbX) {
						const int32 cur = mbY * mbW + mbX;
						const int32 qpQ = mbQp[cur];
						// Luma bords verticaux (par segment de 4 lignes).
						for (int32 e = 0; e < 4; ++e) {
							if (e == 0 && mbX == 0)
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
						for (int32 comp = 0; comp < 2; ++comp) {
							uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
							for (int32 ec = 0; ec < 2; ++ec) {
								if (ec == 0 && mbX == 0)
									continue;
								const int32 e = ec * 2, cx = mbX * 8 + ec * 4;
								const int32 qpcQ = chromaQpOf(qpQ);
								const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - 1]) + qpcQ + 1) >> 1) : qpcQ;
								for (int32 cr = 0; cr < 8; ++cr) {
									const int32 seg = cr / 2;
									const int32 qx = mbX * 4 + e, qy = mbY * 4 + seg;
									const int32 pxx = (e == 0) ? (mbX * 4 - 1) : (mbX * 4 + e - 1);
									const int32 bS = bsOf(qx, qy, pxx, qy, e == 0);
									if (!bS)
										continue;
									lumaEdge(qPavC, bS);
									FiltChroma(&rec[(usize)(mbY * 8 + cr) * c.chromaW + cx], 1, alpha, beta, bS, tc0);
								}
							}
							for (int32 ec = 0; ec < 2; ++ec) {
								if (ec == 0 && mbY == 0)
									continue;
								const int32 e = ec * 2, cy = mbY * 8 + ec * 4;
								const int32 qpcQ = chromaQpOf(qpQ);
								const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - mbW]) + qpcQ + 1) >> 1) : qpcQ;
								for (int32 cc = 0; cc < 8; ++cc) {
									const int32 seg = cc / 2;
									const int32 qx = mbX * 4 + seg, qy = mbY * 4 + e;
									const int32 pyy = (e == 0) ? (mbY * 4 - 1) : (mbY * 4 + e - 1);
									const int32 bS = bsOf(qx, qy, qx, pyy, e == 0);
									if (!bS)
										continue;
									lumaEdge(qPavC, bS);
									FiltChroma(&rec[(usize)cy * c.chromaW + mbX * 8 + cc], c.chromaW, alpha, beta, bS, tc0);
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
						for (int32 i = 0; i < n; ++i) {
							mbTypeClass[(uint64)i] = -1;
							cbpLumaMb[(uint64)i] = 0;
							cbpChromaMb[(uint64)i] = 0;
							chromaModeMb[(uint64)i] = 0;
							lumaDcCodedMb[(uint64)i] = 0;
							chromaDcCodedMb[(uint64)i] = 0;
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

			// ── Decodeurs de syntaxe I-slice (miroir exact de la reference H.264) ──────

			// mb_type (I-slice) : 0=I_NxN(I_4x4) ; 1..24=I_16x16 ; 25=I_PCM. (ctxIdxOffset 3)
			int32 DecodeMbTypeICabac(CabacMb &cab, int32 mbX, int32 mbY) {
				const int32 idx = mbY * cab.mbW + mbX;
				int32 base = kCtxMbTypeI;
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
				if (cab.Terminate())
					return 25; // I_PCM
				int32 mbType = 1;
				mbType += 12 * (int32)cab.Bin(base + 1);		// cbp_luma != 0
				if (cab.Bin(base + 2))							// cbp_chroma present
					mbType += 4 + 4 * (int32)cab.Bin(base + 3); // cbp_chroma == 2 ? 2 : 1
				mbType += 2 * (int32)cab.Bin(base + 4);			// pred mode (bit haut)
				mbType += (int32)cab.Bin(base + 5);				// pred mode (bit bas)
				return mbType;
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
			if (!ParseSps(spsN, spsS, sps) || !ParsePps(ppsN, ppsS, pps))
				return false;
			if (pps.entropyCodingMode != 0) // CABAC non géré
				return false;
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
			const bool isI = (st == 2), isP = (st == 0);
			if (firstMb != 0 || (!isI && !isP)) // seules I / P (slice unique à 0) gérées
				return false;
			if (isP && numRefs <= 0) // P-slice sans reference
				return false;
			br.UE();				   // pps_id
			br.U(sps.log2MaxFrameNum); // frame_num
			if (nalType == 5)
				br.UE(); // idr_pic_id
			if (sps.pocType == 0) {
				br.U(sps.log2MaxPocLsb);
				if (pps.bottomFieldPocPresent)
					br.SE();
			} else if (sps.pocType == 1 && !sps.deltaPocAlwaysZero) {
				br.SE();
				br.SE();
			}
			if (pps.redundantPicCntPresent)
				br.UE();
			int32 numRefActive = pps.numRefIdxL0DefaultActive; // num_ref_idx_l0_active effectif
			if (isP) {
				if (br.U1())									   // num_ref_idx_active_override_flag
					numRefActive = (int32)br.UE() + 1;		   // num_ref_idx_l0_active_minus1 + 1
				if (br.U1()) {								   // ref_pic_list_modification_flag_l0
					uint32 idc;
					do {
						idc = br.UE();
						if (idc == 0 || idc == 1)
							br.UE();
					} while (idc != 3 && !br.Eof());
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
			NkVector<int32> lumaNz, cnz0, cnz1, i4mode, mvx4G, mvy4G, ref4G;
			lumaNz.Resize(n4);
			i4mode.Resize(n4);
			mvx4G.Resize(n4);
			mvy4G.Resize(n4);
			ref4G.Resize(n4);
			cnz0.Resize((uint64)mbW * 2 * mbH * 2);
			cnz1.Resize((uint64)mbW * 2 * mbH * 2);
			for (uint64 i = 0; i < n4; ++i) {
				lumaNz[i] = 0;
				i4mode[i] = 2;
				mvx4G[i] = 0;
				mvy4G[i] = 0;
				ref4G[i] = -2; // 4x4 non décodé
			}
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
			c.mvx4 = mvx4G.Data();
			c.mvy4 = mvy4G.Data();
			c.ref4 = ref4G.Data();
			if (isP) {
				if (numRefs <= 0)
					return false; // référence incompatible
				int32 nUse = numRefs > 16 ? 16 : numRefs;
				for (int32 i = 0; i < nUse; ++i) {
					const NkH264Frame *r = refs[i];
					if (!r || r->lumaW != lumaW || r->lumaH != lumaH)
						return false; // reference incompatible
					c.refListY[i] = r->y.Data();
					c.refListCb[i] = r->cb.Data();
					c.refListCr[i] = r->cr.Data();
				}
				c.numRefs = nUse;
				c.numRefActive = (numRefActive < 1) ? 1 : (numRefActive > nUse ? nUse : numRefActive);
				c.refY = c.refListY[0];
				c.refCb = c.refListCb[0];
				c.refCr = c.refListCr[0];
			}

			NkVector<int32> mbQp;
			mbQp.Resize((uint64)numMb);
			if (isI) {
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
								c.ref4[by * c.nzW + bx] = -1;
					}
					mbQp[(uint64)mbAddr] = c.qp;
					++mbAddr;
				}
			}

			// Déblocage en boucle (§8.7), intra ET inter (bS par segment 4x4).
			if (disableDeblock != 1)
				Deblock(c, mbQp.Data(), alphaOff, betaOff);

			out.lumaW = lumaW;
			out.lumaH = lumaH;
			out.chromaW = chromaW;
			out.chromaH = chromaH;
			out.cropW = sps.width;
			out.cropH = sps.height;
			return true;
		}

	} // namespace media
} // namespace nkentseu
