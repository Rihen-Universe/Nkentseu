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
					int32 lumaW = 0, chromaW = 0;
					int32 mbW = 0, mbH = 0;
					int32 nzW = 0, cnzW = 0;
					int32 *lumaNz = nullptr, *chromaNz0 = nullptr, *chromaNz1 = nullptr, *i4mode = nullptr;
					int32 qp = 26;
					int32 chromaQpOffset = 0;
			};

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

			// Décode et reconstruit le résidu chroma (DC + AC) après prédiction cPred.
			void DecodeChroma(DecCtx &c, NkH264BitReader &br, int32 mbX, int32 mbY, int32 chromaMode, int32 cbpChroma) {
				const int32 qpC = NkH264Transform::ChromaQp(Clamp(c.qp + c.chromaQpOffset, 0, 51));
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

			// Déblocage d'une image INTRA : bS = 4 (bord MB) / 3 (interne). QP moyen par bord.
			void DeblockIntra(DecCtx &c, const int32 *mbQp, int32 alphaOff, int32 betaOff) {
				const int32 mbW = c.mbW;
				auto lumaEdge = [&](int32 qPav, int32 bS, int32 &alpha, int32 &beta, int32 &tc0) {
					const int32 ia = Clamp(qPav + alphaOff, 0, 51), ib = Clamp(qPav + betaOff, 0, 51);
					alpha = kAlpha[ia];
					beta = kBeta[ib];
					tc0 = (bS < 4) ? kTc0[ia][bS - 1] : 0;
				};
				auto chromaEdge = [&](int32 qPavC, int32 bS, int32 &alpha, int32 &beta, int32 &tc0) {
					const int32 ia = Clamp(qPavC + alphaOff, 0, 51), ib = Clamp(qPavC + betaOff, 0, 51);
					alpha = kAlpha[ia];
					beta = kBeta[ib];
					tc0 = (bS < 4) ? kTc0[ia][bS - 1] : 0;
				};
				auto chromaQpOf = [&](int32 lumaQp) {
					return NkH264Transform::ChromaQp(Clamp(lumaQp + c.chromaQpOffset, 0, 51));
				};

				for (int32 mbY = 0; mbY < c.mbH; ++mbY)
					for (int32 mbX = 0; mbX < mbW; ++mbX) {
						const int32 cur = mbY * mbW + mbX;
						const int32 qpQ = mbQp[cur];
						int32 alpha, beta, tc0;
						// Luma bords verticaux.
						for (int32 e = 0; e < 4; ++e) {
							if (e == 0 && mbX == 0)
								continue;
							const int32 bS = (e == 0) ? 4 : 3;
							const int32 qPav = (e == 0) ? ((mbQp[cur - 1] + qpQ + 1) >> 1) : qpQ;
							lumaEdge(qPav, bS, alpha, beta, tc0);
							const int32 x = mbX * 16 + e * 4;
							for (int32 row = 0; row < 16; ++row)
								FiltLuma(&c.Y[(usize)(mbY * 16 + row) * c.lumaW + x], 1, alpha, beta, bS, tc0);
						}
						// Luma bords horizontaux.
						for (int32 e = 0; e < 4; ++e) {
							if (e == 0 && mbY == 0)
								continue;
							const int32 bS = (e == 0) ? 4 : 3;
							const int32 qPav = (e == 0) ? ((mbQp[cur - mbW] + qpQ + 1) >> 1) : qpQ;
							lumaEdge(qPav, bS, alpha, beta, tc0);
							const int32 y = mbY * 16 + e * 4;
							for (int32 col = 0; col < 16; ++col)
								FiltLuma(&c.Y[(usize)y * c.lumaW + mbX * 16 + col], c.lumaW, alpha, beta, bS, tc0);
						}
						// Chroma 4:2:0 : bords cx/cy = 0 et 4 (ec = 0,1).
						for (int32 comp = 0; comp < 2; ++comp) {
							uint8 *rec = (comp == 0) ? c.Cb : c.Cr;
							for (int32 ec = 0; ec < 2; ++ec) {
								if (ec == 0 && mbX == 0)
									continue;
								const int32 bS = (ec == 0) ? 4 : 3;
								const int32 qpcQ = chromaQpOf(qpQ);
								const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - 1]) + qpcQ + 1) >> 1) : qpcQ;
								chromaEdge(qPavC, bS, alpha, beta, tc0);
								const int32 cx = mbX * 8 + ec * 4;
								for (int32 cr = 0; cr < 8; ++cr)
									FiltChroma(&rec[(usize)(mbY * 8 + cr) * c.chromaW + cx], 1, alpha, beta, bS, tc0);
							}
							for (int32 ec = 0; ec < 2; ++ec) {
								if (ec == 0 && mbY == 0)
									continue;
								const int32 bS = (ec == 0) ? 4 : 3;
								const int32 qpcQ = chromaQpOf(qpQ);
								const int32 qPavC = (ec == 0) ? ((chromaQpOf(mbQp[cur - mbW]) + qpcQ + 1) >> 1) : qpcQ;
								chromaEdge(qPavC, bS, alpha, beta, tc0);
								const int32 cy = mbY * 8 + ec * 4;
								for (int32 cc = 0; cc < 8; ++cc)
									FiltChroma(&rec[(usize)cy * c.chromaW + mbX * 8 + cc], c.chromaW, alpha, beta, bS, tc0);
							}
						}
					}
			}

		} // namespace

		bool NkH264Decoder::DecodeIdrFrame(const uint8 *annexB, usize size, NkH264Frame &out) {
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
				} else if (n.type == 5 && !idrN) {
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

			const int32 firstMb = (int32)br.UE();
			const int32 sliceType = (int32)br.UE();
			if (firstMb != 0 || (sliceType % 5) != 2) // I-slice unique à partir de 0
				return false;
			br.UE(); // pps_id
			br.U(sps.log2MaxFrameNum); // frame_num
			br.UE();				   // idr_pic_id (IDR)
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
			if (nalRefIdc != 0) {
				br.U1(); // no_output_of_prior_pics_flag
				br.U1(); // long_term_reference_flag
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

			NkVector<int32> lumaNz, cnz0, cnz1, i4mode;
			lumaNz.Resize((uint64)mbW * 4 * mbH * 4);
			cnz0.Resize((uint64)mbW * 2 * mbH * 2);
			cnz1.Resize((uint64)mbW * 2 * mbH * 2);
			i4mode.Resize((uint64)mbW * 4 * mbH * 4);
			for (uint64 i = 0; i < lumaNz.Size(); ++i) {
				lumaNz[i] = 0;
				i4mode[i] = 2;
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
			c.chromaW = chromaW;
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

			const int32 numMb = mbW * mbH;
			NkVector<int32> mbQp;
			mbQp.Resize((uint64)numMb);
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
					return false; // I_PCM / inter non gérés
				}
				mbQp[(uint64)mbAddr] = c.qp;
			}

			// Déblocage en boucle (§8.7) sur l'image intra, sauf si désactivé.
			if (disableDeblock != 1)
				DeblockIntra(c, mbQp.Data(), alphaOff, betaOff);

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
