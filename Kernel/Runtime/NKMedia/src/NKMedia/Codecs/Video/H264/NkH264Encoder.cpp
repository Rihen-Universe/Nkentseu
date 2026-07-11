// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Encoder.cpp — encodeur H.264 baseline (brique 2b).
// SPS/PPS + slice IDR (I) + macroblocs I_16×16 (prédiction V/H/DC) + résidu luma
// (DC Hadamard + AC 4×4) codé CAVLC + reconstruction pour la prédiction des voisins.
// Chroma : prédiction seule (CBP chroma = 0). Réécrit à la sauce Nkentseu.
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKMedia/Codecs/Video/H264/NkH264Transform.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {
			inline uint8 ClampU8(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}
			inline int32 Clamp(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			// Balayage zig-zag 4×4 (position raster pour chaque indice de scan).
			const int32 kZigZag[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};

			// Position (x4,y4) en pixels du bloc luma 4×4 d'indice blkIdx (scan H.264 §6.4.3 :
			// Z-order des quatre 8×8, Z-order des quatre 4×4 dans chaque 8×8).
			inline void LumaBlk(int32 blkIdx, int32 &x4, int32 &y4) {
				const int32 b8 = blkIdx >> 2, b4 = blkIdx & 3;
				x4 = (b8 & 1) * 8 + (b4 & 1) * 4;
				y4 = (b8 >> 1) * 8 + (b4 >> 1) * 4;
			}

			// Contexte nC (§9.2.1) à partir des total_coeff voisins (grille), -1 = indisponible.
			inline int32 PredictNc(int32 nA, int32 nB) {
				if (nA >= 0 && nB >= 0)
					return (nA + nB + 1) >> 1;
				if (nA >= 0)
					return nA;
				if (nB >= 0)
					return nB;
				return 0;
			}

			// Disponibilité des échantillons haut-droite par bloc luma 4×4 (Z-scan) :
			// 2 = toujours (dans le MB, déjà décodé) ; 1 = dépend du MB au-dessus ; 3 = MB haut-droite ; 0 = jamais.
			const int32 kTrAvail[16] = {1, 1, 2, 0, 1, 3, 2, 0, 2, 2, 2, 0, 2, 0, 2, 0};

			// coded_block_pattern Intra (Table 9-4, ChromaArrayType 1/2) : codeNum → CBP.
			const int32 kCbpIntra[48] = {47, 31, 15, 0,  23, 27, 29, 30, 7,	11, 13, 14, 39, 43, 45, 46,
										 16, 3,	 5,	 10, 12, 19, 21, 26, 28, 35, 37, 42, 44, 1,	 2,	 4,
										 8,	 17, 18, 20, 24, 6,	 9,	 22, 25, 32, 33, 34, 36, 40, 38, 41};

			// Prédiction Intra_4×4 (§8.3.1.2), mode 0..8. t[0..7] = haut + haut-droite, l[0..3] = gauche,
			// tl = coin. avt/avl = disponibilité haut/gauche (pour DC). Sort pred[16] (raster y*4+x).
			void Predict4x4(int32 mode, const int32 t[8], const int32 l[4], int32 tl, bool avt, bool avl,
							uint8 pred[16]) {
				auto T = [&](int32 i) -> int32 { return i < 0 ? tl : t[i]; };
				auto L = [&](int32 j) -> int32 { return j < 0 ? tl : l[j]; };
				auto P = [&](int32 y, int32 x, int32 v) { pred[y * 4 + x] = ClampU8(v); };
				switch (mode) {
					case 0: // Vertical
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x)
								P(y, x, t[x]);
						break;
					case 1: // Horizontal
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x)
								P(y, x, l[y]);
						break;
					case 2: { // DC
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
					case 3: // Diagonal Down-Left
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 i = x + y;
								P(y, x, (x == 3 && y == 3) ? (t[6] + 3 * t[7] + 2) >> 2
														   : (t[i] + 2 * t[i + 1] + t[i + 2] + 2) >> 2);
							}
						break;
					case 4: // Diagonal Down-Right
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
					case 5: // Vertical-Right
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
					case 6: // Horizontal-Down
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
					case 7: // Vertical-Left
						for (int32 y = 0; y < 4; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const int32 h = x + (y >> 1);
								if ((y & 1) == 0)
									P(y, x, (t[h] + t[h + 1] + 1) >> 1);
								else
									P(y, x, (t[h] + 2 * t[h + 1] + t[h + 2] + 2) >> 2);
							}
						break;
					default: // 8 : Horizontal-Up
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
			// --- Filtre de déblocage (§8.7) : tables α/β (8-16) et tC0 (8-17) ---
			const int32 kAlpha[52] = {0,	0,	 0,	  0,   0,	0,	 0,	  0,   0,	0,	 0,	  0,	0,
									  0,	0,	 0,	  4,   4,	5,	 6,	  7,   8,	9,	 10,  12,	13,
									  15,	17,	 20,  22,  25,	28,	 32,  36,  40,	45,	 50,  56,	63,
									  71,	80,	 90,  101, 113, 127, 144, 162, 182, 203, 226, 255, 255};
			const int32 kBeta[52] = {0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,	 2,	 2,
									 2,	 3,	 3,	 3,	 3,	 4,	 4,	 4,	 6,	 6,	 7,	 7,	 8,	 8,	 9,	 9,	 10, 10,
									 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18};
			const int32 kTc0[52][3] = {
				{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},
				{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 0},
				{0, 0, 0},	{0, 0, 0},	{0, 0, 0},	{0, 0, 1},	{0, 0, 1},	{0, 0, 1},	{0, 0, 1},
				{0, 0, 1},	{0, 0, 1},	{0, 1, 1},	{0, 1, 1},	{1, 1, 1},	{1, 1, 1},	{1, 1, 1},
				{1, 1, 1},	{1, 1, 2},	{1, 1, 2},	{1, 1, 2},	{1, 1, 2},	{1, 2, 3},	{1, 2, 3},
				{2, 2, 3},	{2, 2, 4},	{2, 3, 4},	{2, 3, 4},	{3, 3, 5},	{3, 4, 6},	{3, 4, 6},
				{4, 5, 7},	{4, 5, 8},	{5, 6, 9},	{6, 7, 10}, {6, 8, 11}, {7, 9, 12}, {8, 10, 13},
				{9, 12, 15}, {10, 13, 17}, {11, 17, 25}};

			// Filtre luma d'une ligne perpendiculaire au bord. `q` pointe q0 ; `d` = pas p→q.
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

			// Filtre chroma (ne modifie que p0/q0).
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
		} // namespace

		// --------------------------------------------------------------------------
		bool NkH264Encoder::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen, int32 qp) {
			if (width <= 0 || height <= 0 || fpsNum <= 0 || fpsDen <= 0)
				return false;
			mWidth = width;
			mHeight = height;
			mFpsNum = fpsNum;
			mFpsDen = fpsDen;
			mQp = Clamp(qp, 0, 51);
			mMbW = (width + 15) / 16;
			mMbH = (height + 15) / 16;
			mLumaW = mMbW * 16;
			mLumaH = mMbH * 16;
			mChromaW = mLumaW / 2;
			mChromaH = mLumaH / 2;
			mFrame = 0;

			const usize lumaN = (usize)mLumaW * mLumaH, chromaN = (usize)mChromaW * mChromaH;
			mY = (uint8 *)memory::NkAlloc(lumaN);
			mCb = (uint8 *)memory::NkAlloc(chromaN);
			mCr = (uint8 *)memory::NkAlloc(chromaN);
			mRecY = (uint8 *)memory::NkAlloc(lumaN);
			mRecCb = (uint8 *)memory::NkAlloc(chromaN);
			mRecCr = (uint8 *)memory::NkAlloc(chromaN);
			mLumaNz = (int32 *)memory::NkAlloc((usize)mMbW * 4 * mMbH * 4 * sizeof(int32));
			mChromaNz[0] = (int32 *)memory::NkAlloc((usize)mMbW * 2 * mMbH * 2 * sizeof(int32));
			mChromaNz[1] = (int32 *)memory::NkAlloc((usize)mMbW * 2 * mMbH * 2 * sizeof(int32));
			mI4Mode = (int32 *)memory::NkAlloc((usize)mMbW * 4 * mMbH * 4 * sizeof(int32));
			if (!mY || !mCb || !mCr || !mRecY || !mRecCb || !mRecCr || !mLumaNz || !mChromaNz[0] || !mChromaNz[1] ||
				!mI4Mode) {
				Free();
				return false;
			}

			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode)) {
				Free();
				return false;
			}
			mOpen = true;
			return true;
		}

		void NkH264Encoder::Free() {
			uint8 *bufs[6] = {mY, mCb, mCr, mRecY, mRecCb, mRecCr};
			for (int32 i = 0; i < 6; ++i)
				if (bufs[i])
					memory::NkFree(bufs[i]);
			if (mLumaNz)
				memory::NkFree(mLumaNz);
			for (int32 c = 0; c < 2; ++c)
				if (mChromaNz[c])
					memory::NkFree(mChromaNz[c]);
			if (mI4Mode)
				memory::NkFree(mI4Mode);
			mY = mCb = mCr = mRecY = mRecCb = mRecCr = nullptr;
			mLumaNz = nullptr;
			mChromaNz[0] = mChromaNz[1] = nullptr;
			mI4Mode = nullptr;
		}

		// --------------------------------------------------------------------------
		void NkH264Encoder::ConvertToYuv(const uint8 *pixels, NkVideoInputFormat fmt) {
			const int32 ibpp = (fmt == NkVideoInputFormat::RGBA32) ? 4 : 3;
			for (int32 y = 0; y < mLumaH; ++y) {
				const int32 sy = Clamp(y, 0, mHeight - 1);
				for (int32 x = 0; x < mLumaW; ++x) {
					const int32 sx = Clamp(x, 0, mWidth - 1);
					const uint8 *p = pixels + ((usize)sy * mWidth + sx) * ibpp;
					int32 r, g, b;
					if (fmt == NkVideoInputFormat::BGR24) {
						b = p[0];
						g = p[1];
						r = p[2];
					} else {
						r = p[0];
						g = p[1];
						b = p[2];
					}
					mY[(usize)y * mLumaW + x] = ClampU8((77 * r + 150 * g + 29 * b) >> 8);
				}
			}
			for (int32 cy = 0; cy < mChromaH; ++cy)
				for (int32 cx = 0; cx < mChromaW; ++cx) {
					int32 sr = 0, sg = 0, sb = 0;
					for (int32 dy = 0; dy < 2; ++dy)
						for (int32 dx = 0; dx < 2; ++dx) {
							const int32 yy = Clamp(cy * 2 + dy, 0, mHeight - 1);
							const int32 xx = Clamp(cx * 2 + dx, 0, mWidth - 1);
							const uint8 *p = pixels + ((usize)yy * mWidth + xx) * ibpp;
							if (fmt == NkVideoInputFormat::BGR24) {
								sb += p[0];
								sg += p[1];
								sr += p[2];
							} else {
								sr += p[0];
								sg += p[1];
								sb += p[2];
							}
						}
					const int32 r = sr / 4, g = sg / 4, b = sb / 4;
					mCb[(usize)cy * mChromaW + cx] = ClampU8(128 + ((-43 * r - 85 * g + 128 * b) >> 8));
					mCr[(usize)cy * mChromaW + cx] = ClampU8(128 + ((128 * r - 107 * g - 21 * b) >> 8));
				}
		}

		// --------------------------------------------------------------------------
		// Un macrobloc I_16×16 : prédiction luma (V/H/DC), résidu (DC Hadamard + AC),
		// CAVLC, reconstruction. Chroma en prédiction DC seule (pas de résidu).
		void NkH264Encoder::EncodeMacroblock(NkH264BitWriter &bs, int32 mbX, int32 mbY) {
			const int32 px = mbX * 16, py = mbY * 16;
			const bool availTop = mbY > 0, availLeft = mbX > 0;
			const int32 nzW = mMbW * 4;

			// --- voisins luma reconstruits ---
			int32 top[16], left[16];
			for (int32 i = 0; i < 16; ++i) {
				top[i] = availTop ? mRecY[(usize)(py - 1) * mLumaW + px + i] : 0;
				left[i] = availLeft ? mRecY[(usize)(py + i) * mLumaW + px - 1] : 0;
			}
			int32 sumTop = 0, sumLeft = 0;
			for (int32 i = 0; i < 16; ++i) {
				sumTop += top[i];
				sumLeft += left[i];
			}

			// --- source luma ---
			uint8 src[256];
			for (int32 j = 0; j < 16; ++j)
				for (int32 i = 0; i < 16; ++i)
					src[j * 16 + i] = mY[(usize)(py + j) * mLumaW + px + i];

			// --- choix du mode 16×16 (0=V,1=H,2=DC) par SAD ---
			int32 dcVal;
			if (availTop && availLeft)
				dcVal = (sumTop + sumLeft + 16) >> 5;
			else if (availTop)
				dcVal = (sumTop + 8) >> 4;
			else if (availLeft)
				dcVal = (sumLeft + 8) >> 4;
			else
				dcVal = 128;

			uint8 pred[256];
			int32 bestMode = 2;
			int64 bestSad = -1;
			for (int32 mode = 0; mode < 3; ++mode) {
				if (mode == 0 && !availTop)
					continue;
				if (mode == 1 && !availLeft)
					continue;
				int64 sad = 0;
				for (int32 j = 0; j < 16; ++j)
					for (int32 i = 0; i < 16; ++i) {
						int32 p;
						if (mode == 0)
							p = top[i];
						else if (mode == 1)
							p = left[j];
						else
							p = dcVal;
						const int32 d = (int32)src[j * 16 + i] - p;
						sad += d < 0 ? -d : d;
					}
				if (bestSad < 0 || sad < bestSad) {
					bestSad = sad;
					bestMode = mode;
				}
			}
			// construit la prédiction retenue
			for (int32 j = 0; j < 16; ++j)
				for (int32 i = 0; i < 16; ++i) {
					if (bestMode == 0)
						pred[j * 16 + i] = (uint8)top[i];
					else if (bestMode == 1)
						pred[j * 16 + i] = (uint8)left[j];
					else
						pred[j * 16 + i] = (uint8)dcVal;
				}

			// --- transformée + quantification par bloc 4×4 ---
			int32 dc4[16];		 // DC transformé (brut) de chaque bloc, arrangement raster 4×4
			int32 acLvl[16][16]; // niveaux quantifiés (raster) par bloc ; positions 1..15 utilisées
			for (int32 blk = 0; blk < 16; ++blk) {
				int32 x4, y4;
				LumaBlk(blk, x4, y4);
				int32 res[16], coef[16];
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c) {
						const int32 s = src[(y4 + r) * 16 + (x4 + c)];
						const int32 p = pred[(y4 + r) * 16 + (x4 + c)];
						res[r * 4 + c] = s - p;
					}
				NkH264Transform::Forward4x4(res, coef);
				dc4[(y4 / 4) * 4 + (x4 / 4)] = coef[0];
				NkH264Transform::Quant4x4(coef, acLvl[blk], mQp, true);
			}

			// --- DC luma : Hadamard direct + quant DC ---
			int32 hdc[16], dcLvl[16];
			NkH264Transform::HadamardForward4x4(dc4, hdc);
			NkH264Transform::QuantDC(hdc, dcLvl, 16, mQp, true);
			// reconstruction du DC — ordre standard décodeur (§8.5.10) : Hadamard inverse des
			// NIVEAUX puis mise à l'échelle (×16). Donne le coeff (0,0) de chaque bloc 4×4.
			int32 gDc[16], dcRec[16];
			NkH264Transform::HadamardInverse4x4(dcLvl, gDc);
			NkH264Transform::DequantDC(gDc, dcRec, 16, mQp);

			// --- CBP luma : y a-t-il un coefficient AC non nul ? ---
			bool cbpLuma = false;
			for (int32 blk = 0; blk < 16 && !cbpLuma; ++blk)
				for (int32 k = 1; k < 16; ++k)
					if (acLvl[blk][kZigZag[k]] != 0) {
						cbpLuma = true;
						break;
					}

			// --- chroma (prédiction DC + résidu DC 2×2 / AC + reconstruction) ---
			ChromaMb cmb;
			ComputeChroma(mbX, mbY, availTop, availLeft, cmb);
			const int32 cbpChroma = cmb.cbp;

			// --- écriture bitstream : mb_type, intra_chroma_pred_mode, mb_qp_delta ---
			const int32 mbType = 1 + bestMode + 4 * cbpChroma + 12 * (cbpLuma ? 1 : 0);
			bs.Ue((uint32)mbType);
			bs.Ue(0); // intra_chroma_pred_mode = DC
			bs.Se(0); // mb_qp_delta = 0 (QP constant)

			// --- résidu : DC luma (16), puis AC luma (15) si cbpLuma ---
			const int32 bx0 = mbX * 4, by0 = mbY * 4;
			// nC du bloc DC = celui du bloc 0 (position MB (0,0))
			{
				const int32 nA = availLeft ? mLumaNz[by0 * nzW + (bx0 - 1)] : -1;
				const int32 nB = availTop ? mLumaNz[(by0 - 1) * nzW + bx0] : -1;
				const int32 nC = PredictNc(nA, nB);
				int32 dcScan[16];
				for (int32 k = 0; k < 16; ++k)
					dcScan[k] = dcLvl[kZigZag[k]];
				NkH264Cavlc::EncodeResidual(bs, dcScan, 16, nC);
			}

			for (int32 blk = 0; blk < 16; ++blk) {
				int32 x4, y4;
				LumaBlk(blk, x4, y4);
				const int32 bx = bx0 + x4 / 4, by = by0 + y4 / 4;
				int32 tc = 0;
				if (cbpLuma) {
					// nC depuis les voisins (grille déjà remplie pour les blocs précédents)
					const int32 nAx = bx - 1, nBy = by - 1;
					const int32 nA = (bx > 0) ? mLumaNz[by * nzW + nAx] : -1;
					const int32 nB = (by > 0) ? mLumaNz[nBy * nzW + bx] : -1;
					const int32 nC = PredictNc(nA, nB);
					int32 acScan[15];
					for (int32 k = 1; k < 16; ++k)
						acScan[k - 1] = acLvl[blk][kZigZag[k]];
					tc = NkH264Cavlc::EncodeResidual(bs, acScan, 15, nC);
				}
				mLumaNz[by * nzW + bx] = tc;

				// --- reconstruction du bloc (dequant AC + DC Hadamard + inverse 4×4 + prédiction) ---
				int32 deq[16];
				NkH264Transform::Dequant4x4(acLvl[blk], deq, mQp);
				deq[0] = dcRec[(y4 / 4) * 4 + (x4 / 4)];
				if (!cbpLuma)
					for (int32 k = 1; k < 16; ++k)
						deq[k] = 0; // AC non transmis → nul côté décodeur
				int32 resRec[16];
				NkH264Transform::Inverse4x4(deq, resRec);
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c) {
						const int32 p = pred[(y4 + r) * 16 + (x4 + c)];
						mRecY[(usize)(py + y4 + r) * mLumaW + px + x4 + c] = ClampU8(p + resRec[r * 4 + c]);
					}
			}

			// mode Intra_4×4 des blocs de ce MB = DC(2) pour la prédiction du mode des voisins I_4×4.
			for (int32 by = by0; by < by0 + 4; ++by)
				for (int32 bx = bx0; bx < bx0 + 4; ++bx)
					mI4Mode[by * nzW + bx] = 2;

			// --- résidu chroma (DC puis AC) ---
			WriteChromaResidual(bs, mbX, mbY, cmb);
		}

		// --------------------------------------------------------------------------
		// Chroma partagé : prédiction DC 4 quadrants + résidu (DC 2×2 + AC) + reconstruction.
		void NkH264Encoder::ComputeChroma(int32 mbX, int32 mbY, bool availTop, bool availLeft, ChromaMb &c) {
			const int32 qpC = NkH264Transform::ChromaQp(mQp);
			int32 cDcRec[2][4];
			uint8 cPred[2][64];
			for (int32 comp = 0; comp < 2; ++comp) {
				const uint8 *csrc = comp == 0 ? mCb : mCr;
				const uint8 *rec = comp == 0 ? mRecCb : mRecCr;
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				int32 ct[8], cl[8];
				for (int32 i = 0; i < 8; ++i) {
					ct[i] = availTop ? rec[(usize)(cpy - 1) * mChromaW + cpx + i] : 0;
					cl[i] = availLeft ? rec[(usize)(cpy + i) * mChromaW + cpx - 1] : 0;
				}
				const int32 sTL = ct[0] + ct[1] + ct[2] + ct[3], sTR = ct[4] + ct[5] + ct[6] + ct[7];
				const int32 sLT = cl[0] + cl[1] + cl[2] + cl[3], sLB = cl[4] + cl[5] + cl[6] + cl[7];
				int32 q[4];
				q[0] = availTop && availLeft ? (sTL + sLT + 4) >> 3
										   : availTop ? (sTL + 2) >> 2
										   : availLeft ? (sLT + 2) >> 2
													   : 128;
				q[1] = availTop ? (sTR + 2) >> 2 : availLeft ? (sLT + 2) >> 2 : 128;
				q[2] = availLeft ? (sLB + 2) >> 2 : availTop ? (sTL + 2) >> 2 : 128;
				q[3] = availTop && availLeft ? (sTR + sLB + 4) >> 3
										   : availTop ? (sTR + 2) >> 2
										   : availLeft ? (sLB + 2) >> 2
													   : 128;
				for (int32 j = 0; j < 8; ++j)
					for (int32 i = 0; i < 8; ++i)
						cPred[comp][j * 8 + i] = ClampU8(q[(i < 4 ? 0 : 1) + (j < 4 ? 0 : 2)]);

				int32 cdc4[4];
				for (int32 blk = 0; blk < 4; ++blk) {
					const int32 bx4 = (blk & 1) * 4, by4 = (blk >> 1) * 4;
					int32 res[16], coef[16];
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col) {
							const int32 s = csrc[(usize)(cpy + by4 + r) * mChromaW + cpx + bx4 + col];
							res[r * 4 + col] = s - cPred[comp][(by4 + r) * 8 + (bx4 + col)];
						}
					NkH264Transform::Forward4x4(res, coef);
					cdc4[blk] = coef[0];
					NkH264Transform::Quant4x4(coef, c.lvl[comp][blk], qpC, true);
				}
				int32 fdc[4], gdc[4];
				NkH264Transform::Hadamard2x2(cdc4, fdc);
				NkH264Transform::QuantChromaDC(fdc, c.dcLvl[comp], qpC, true);
				NkH264Transform::Hadamard2x2(c.dcLvl[comp], gdc);
				NkH264Transform::DequantChromaDC(gdc, cDcRec[comp], qpC);
			}

			bool anyDc = false, anyAc = false;
			for (int32 comp = 0; comp < 2; ++comp)
				for (int32 blk = 0; blk < 4; ++blk) {
					if (c.dcLvl[comp][blk] != 0)
						anyDc = true;
					for (int32 k = 1; k < 16; ++k)
						if (c.lvl[comp][blk][kZigZag[k]] != 0)
							anyAc = true;
				}
			c.cbp = anyAc ? 2 : (anyDc ? 1 : 0);

			for (int32 comp = 0; comp < 2; ++comp) {
				uint8 *rec = comp == 0 ? mRecCb : mRecCr;
				const int32 cpx = mbX * 8, cpy = mbY * 8;
				for (int32 blk = 0; blk < 4; ++blk) {
					const int32 bx4 = (blk & 1) * 4, by4 = (blk >> 1) * 4;
					int32 deq[16];
					NkH264Transform::Dequant4x4(c.lvl[comp][blk], deq, qpC);
					if (c.cbp < 2)
						for (int32 k = 1; k < 16; ++k)
							deq[k] = 0;
					deq[0] = (c.cbp >= 1) ? cDcRec[comp][blk] : 0;
					int32 resRec[16];
					NkH264Transform::Inverse4x4(deq, resRec);
					for (int32 r = 0; r < 4; ++r)
						for (int32 col = 0; col < 4; ++col) {
							const int32 p = cPred[comp][(by4 + r) * 8 + (bx4 + col)];
							rec[(usize)(cpy + by4 + r) * mChromaW + cpx + bx4 + col] = ClampU8(p + resRec[r * 4 + col]);
						}
				}
			}
		}

		void NkH264Encoder::WriteChromaResidual(NkH264BitWriter &bs, int32 mbX, int32 mbY, const ChromaMb &c) {
			const int32 cnzW = mMbW * 2;
			const int32 cbx0 = mbX * 2, cby0 = mbY * 2;
			if (c.cbp & 3) {
				for (int32 comp = 0; comp < 2; ++comp)
					NkH264Cavlc::EncodeResidual(bs, c.dcLvl[comp], 4, -1); // chroma DC (scan raster 2×2)
			}
			for (int32 comp = 0; comp < 2; ++comp)
				for (int32 blk = 0; blk < 4; ++blk) {
					const int32 bx = cbx0 + (blk & 1), by = cby0 + (blk >> 1);
					int32 tc = 0;
					if (c.cbp & 2) {
						const int32 nA = (bx > 0) ? mChromaNz[comp][by * cnzW + (bx - 1)] : -1;
						const int32 nB = (by > 0) ? mChromaNz[comp][(by - 1) * cnzW + bx] : -1;
						const int32 nC = PredictNc(nA, nB);
						int32 acScan[15];
						for (int32 k = 1; k < 16; ++k)
							acScan[k - 1] = c.lvl[comp][blk][kZigZag[k]];
						tc = NkH264Cavlc::EncodeResidual(bs, acScan, 15, nC);
					}
					mChromaNz[comp][by * cnzW + bx] = tc;
				}
		}

		// --------------------------------------------------------------------------
		// Macrobloc I_4×4 : 16 blocs 4×4, chacun prédit (9 modes, choix SAD) depuis les voisins
		// reconstruits, résidu transformé/quantifié/CAVLC. Reconstruction immédiate (prédiction en cascade).
		void NkH264Encoder::EncodeMbIntra4x4(NkH264BitWriter &bs, int32 mbX, int32 mbY) {
			const int32 px = mbX * 16, py = mbY * 16;
			const int32 nzW = mMbW * 4;
			const bool availTop = mbY > 0, availLeft = mbX > 0;
			const bool availTrMb = (mbY > 0) && (mbX < mMbW - 1);

			uint8 src[256];
			for (int32 j = 0; j < 16; ++j)
				for (int32 i = 0; i < 16; ++i)
					src[j * 16 + i] = mY[(usize)(py + j) * mLumaW + px + i];

			int32 chosen[16], prevFlag[16], remMode[16];
			int32 lvlBlk[16][16];

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
					t[i] = avt ? mRecY[(usize)(Y - 1) * mLumaW + X + i] : 0;
				for (int32 i = 0; i < 4; ++i)
					t[4 + i] = (avt && avtr) ? mRecY[(usize)(Y - 1) * mLumaW + X + 4 + i] : t[3];
				for (int32 j = 0; j < 4; ++j)
					l[j] = avl ? mRecY[(usize)(Y + j) * mLumaW + X - 1] : 0;
				tl = avtl ? mRecY[(usize)(Y - 1) * mLumaW + X - 1] : 0;

				// choix du mode : SAD minimal parmi les modes disponibles.
				uint8 predBest[16];
				int32 bestMode = 2;
				int64 bestSad = -1;
				for (int32 m = 0; m < 9; ++m) {
					if ((m == 0) && !avt)
						continue;
					if ((m == 1 || m == 8) && !avl)
						continue;
					if ((m == 3 || m == 7) && !(avt && avtr))
						continue;
					if ((m == 4 || m == 5 || m == 6) && !avtl)
						continue;
					uint8 pr[16];
					Predict4x4(m, t, l, tl, avt, avl, pr);
					int64 sad = 0;
					for (int32 r = 0; r < 4; ++r)
						for (int32 c = 0; c < 4; ++c) {
							const int32 d = (int32)src[(y4 + r) * 16 + (x4 + c)] - pr[r * 4 + c];
							sad += d < 0 ? -d : d;
						}
					if (bestSad < 0 || sad < bestSad) {
						bestSad = sad;
						bestMode = m;
						for (int32 i = 0; i < 16; ++i)
							predBest[i] = pr[i];
					}
				}
				chosen[blk] = bestMode;

				int32 res[16], coef[16];
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c)
						res[r * 4 + c] = (int32)src[(y4 + r) * 16 + (x4 + c)] - predBest[r * 4 + c];
				NkH264Transform::Forward4x4(res, coef);
				NkH264Transform::Quant4x4(coef, lvlBlk[blk], mQp, true);

				// reconstruction immédiate (sert de voisin aux blocs suivants).
				int32 deq[16], resRec[16];
				NkH264Transform::Dequant4x4(lvlBlk[blk], deq, mQp);
				NkH264Transform::Inverse4x4(deq, resRec);
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c)
						mRecY[(usize)(Y + r) * mLumaW + X + c] = ClampU8(predBest[r * 4 + c] + resRec[r * 4 + c]);

				// prédiction du mode (min des modes voisins) → prev_flag / rem.
				const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
				const int32 intraA = (bx > 0) ? mI4Mode[by * nzW + (bx - 1)] : 2;
				const int32 intraB = (by > 0) ? mI4Mode[(by - 1) * nzW + bx] : 2;
				const int32 predMode = intraA < intraB ? intraA : intraB;
				if (bestMode == predMode) {
					prevFlag[blk] = 1;
					remMode[blk] = 0;
				} else {
					prevFlag[blk] = 0;
					remMode[blk] = (bestMode < predMode) ? bestMode : bestMode - 1;
				}
				mI4Mode[by * nzW + bx] = bestMode;
			}

			// chroma (prédiction DC + résidu + reconstruction).
			ChromaMb cmb;
			ComputeChroma(mbX, mbY, availTop, availLeft, cmb);

			// CBP luma : 1 bit par 8×8 (groupe de 4 blocs en Z-scan).
			int32 lumaCbp = 0;
			for (int32 k = 0; k < 4; ++k) {
				bool coded = false;
				for (int32 b = 0; b < 4 && !coded; ++b)
					for (int32 i = 0; i < 16; ++i)
						if (lvlBlk[k * 4 + b][i] != 0) {
							coded = true;
							break;
						}
				if (coded)
					lumaCbp |= (1 << k);
			}
			const int32 cbp = lumaCbp + 16 * cmb.cbp;
			int32 codeNum = 0;
			for (int32 n = 0; n < 48; ++n)
				if (kCbpIntra[n] == cbp) {
					codeNum = n;
					break;
				}

			// écriture : mb_type, mb_pred (16 modes), intra_chroma, cbp, mb_qp_delta.
			bs.Ue(0); // mb_type = I_4×4
			for (int32 blk = 0; blk < 16; ++blk) {
				bs.PutFlag(prevFlag[blk]);
				if (!prevFlag[blk])
					bs.PutBits((uint32)remMode[blk], 3);
			}
			bs.Ue(0);				// intra_chroma_pred_mode = DC
			bs.Ue((uint32)codeNum); // coded_block_pattern (Intra)
			if (cbp > 0)
				bs.Se(0); // mb_qp_delta

			// résidu luma : chaque 8×8 codé → ses 4 blocs (LumaLevel4x4, maxNumCoeff=16).
			for (int32 k = 0; k < 4; ++k) {
				if (!(lumaCbp & (1 << k)))
					continue;
				for (int32 b = 0; b < 4; ++b) {
					const int32 blk = k * 4 + b;
					int32 x4, y4;
					LumaBlk(blk, x4, y4);
					const int32 bx = mbX * 4 + x4 / 4, by = mbY * 4 + y4 / 4;
					const int32 nA = (bx > 0) ? mLumaNz[by * nzW + (bx - 1)] : -1;
					const int32 nB = (by > 0) ? mLumaNz[(by - 1) * nzW + bx] : -1;
					const int32 nC = PredictNc(nA, nB);
					int32 scan[16];
					for (int32 i = 0; i < 16; ++i)
						scan[i] = lvlBlk[blk][kZigZag[i]];
					mLumaNz[by * nzW + bx] = NkH264Cavlc::EncodeResidual(bs, scan, 16, nC);
				}
			}

			WriteChromaResidual(bs, mbX, mbY, cmb);
			(void)chosen;
		}

		// --------------------------------------------------------------------------
		// Déblocage en boucle (§8.7). Toutes les MB étant intra, bS = 4 (bord de MB) ou 3 (interne).
		// Ordre standard : par MB en raster, bords verticaux (gauche→droite) puis horizontaux (haut→bas).
		void NkH264Encoder::DeblockFrame() {
			if (!mDeblock)
				return;
			const int32 idx = Clamp(mQp, 0, 51);
			const int32 alpha = kAlpha[idx], beta = kBeta[idx];
			const int32 idxC = NkH264Transform::ChromaQp(mQp);
			const int32 alphaC = kAlpha[idxC], betaC = kBeta[idxC];

			for (int32 mbY = 0; mbY < mMbH; ++mbY)
				for (int32 mbX = 0; mbX < mMbW; ++mbX) {
					// --- luma : 4 bords verticaux puis 4 horizontaux ---
					for (int32 e = 0; e < 4; ++e) {
						if (e == 0 && mbX == 0)
							continue; // bord gauche de l'image
						const int32 bS = (e == 0) ? 4 : 3;
						const int32 tc0 = (bS < 4) ? kTc0[idx][bS - 1] : 0;
						const int32 x = mbX * 16 + e * 4;
						for (int32 r = 0; r < 16; ++r)
							FiltLuma(&mRecY[(usize)(mbY * 16 + r) * mLumaW + x], 1, alpha, beta, bS, tc0);
					}
					for (int32 e = 0; e < 4; ++e) {
						if (e == 0 && mbY == 0)
							continue; // bord haut de l'image
						const int32 bS = (e == 0) ? 4 : 3;
						const int32 tc0 = (bS < 4) ? kTc0[idx][bS - 1] : 0;
						const int32 y = mbY * 16 + e * 4;
						for (int32 c = 0; c < 16; ++c)
							FiltLuma(&mRecY[(usize)y * mLumaW + mbX * 16 + c], mLumaW, alpha, beta, bS, tc0);
					}
					// --- chroma (4:2:0) : bords x=0/4 puis y=0/4, sur Cb et Cr ---
					for (int32 comp = 0; comp < 2; ++comp) {
						uint8 *rec = comp == 0 ? mRecCb : mRecCr;
						for (int32 e = 0; e < 2; ++e) {
							if (e == 0 && mbX == 0)
								continue;
							const int32 bS = (e == 0) ? 4 : 3;
							const int32 tc0 = (bS < 4) ? kTc0[idxC][bS - 1] : 0;
							const int32 cx = mbX * 8 + e * 4;
							for (int32 r = 0; r < 8; ++r)
								FiltChroma(&rec[(usize)(mbY * 8 + r) * mChromaW + cx], 1, alphaC, betaC, bS, tc0);
						}
						for (int32 e = 0; e < 2; ++e) {
							if (e == 0 && mbY == 0)
								continue;
							const int32 bS = (e == 0) ? 4 : 3;
							const int32 tc0 = (bS < 4) ? kTc0[idxC][bS - 1] : 0;
							const int32 cy = mbY * 8 + e * 4;
							for (int32 c = 0; c < 8; ++c)
								FiltChroma(&rec[(usize)cy * mChromaW + mbX * 8 + c], mChromaW, alphaC, betaC, bS, tc0);
						}
					}
				}
		}

		// --------------------------------------------------------------------------
		void NkH264Encoder::EncodeFrame(NkVector<uint8> &out) {
			NkH264BitWriter bs;

			// SPS (nal_unit_type 7, ref_idc 3).
			bs.Clear();
			bs.PutBits(66, 8);	 // profile_idc = baseline
			bs.PutBits(0x80, 8); // constraint_set0_flag=1 + reserved
			bs.PutBits(51, 8);	 // level_idc = 5.1 (large, jamais rejeté)
			bs.Ue(0);			 // seq_parameter_set_id
			bs.Ue(0);			 // log2_max_frame_num_minus4 → frame_num sur 4 bits
			bs.Ue(0);			 // pic_order_cnt_type = 0
			bs.Ue(0);			 // log2_max_pic_order_cnt_lsb_minus4 → poc sur 4 bits
			bs.Ue(1);			 // max_num_ref_frames
			bs.PutBits(0, 1);	 // gaps_in_frame_num_value_allowed_flag
			bs.Ue((uint32)(mMbW - 1));
			bs.Ue((uint32)(mMbH - 1));
			bs.PutBits(1, 1); // frame_mbs_only_flag
			bs.PutBits(1, 1); // direct_8x8_inference_flag
			const bool crop = (mLumaW != mWidth) || (mLumaH != mHeight);
			bs.PutBits(crop ? 1 : 0, 1);
			if (crop) {
				bs.Ue(0);							  // crop left
				bs.Ue((uint32)((mLumaW - mWidth) / 2));	  // crop right (CropUnitX=2)
				bs.Ue(0);							  // crop top
				bs.Ue((uint32)((mLumaH - mHeight) / 2)); // crop bottom (CropUnitY=2)
			}
			bs.PutBits(0, 1); // vui_parameters_present_flag
			bs.TrailingBits();
			bs.EmitNal(out, 3, 7);

			// PPS (nal_unit_type 8, ref_idc 3).
			bs.Clear();
			bs.Ue(0);		  // pic_parameter_set_id
			bs.Ue(0);		  // seq_parameter_set_id
			bs.PutBits(0, 1); // entropy_coding_mode_flag = CAVLC
			bs.PutBits(0, 1); // bottom_field_pic_order_in_frame_present_flag
			bs.Ue(0);		  // num_slice_groups_minus1
			bs.Ue(0);		  // num_ref_idx_l0_default_active_minus1
			bs.Ue(0);		  // num_ref_idx_l1_default_active_minus1
			bs.PutBits(0, 1); // weighted_pred_flag
			bs.PutBits(0, 2); // weighted_bipred_idc
			bs.Se(mQp - 26);  // pic_init_qp_minus26
			bs.Se(0);		  // pic_init_qs_minus26
			bs.Se(0);		  // chroma_qp_index_offset
			bs.PutBits(1, 1); // deblocking_filter_control_present_flag (on signale le déblocage par tranche)
			bs.PutBits(0, 1); // constrained_intra_pred_flag
			bs.PutBits(0, 1); // redundant_pic_cnt_present_flag
			bs.TrailingBits();
			bs.EmitNal(out, 3, 8);

			// Slice IDR (nal_unit_type 5, ref_idc 3) : en-tête + données macroblocs.
			bs.Clear();
			bs.Ue(0);					  // first_mb_in_slice
			bs.Ue(2);					  // slice_type = I
			bs.Ue(0);					  // pic_parameter_set_id
			bs.PutBits(0, 4);			  // frame_num (IDR = 0)
			bs.Ue((uint32)(mFrame & 1));  // idr_pic_id (alterne)
			bs.PutBits(0, 4);			  // pic_order_cnt_lsb (IDR = 0)
			bs.PutBits(0, 1);			  // no_output_of_prior_pics_flag
			bs.PutBits(0, 1);			  // long_term_reference_flag
			bs.Se(0);					  // slice_qp_delta (SliceQPY = pic_init_qp)
			// contrôle du déblocage (PPS deblocking_filter_control_present_flag = 1).
			bs.Ue(mDeblock ? 0u : 1u); // disable_deblocking_filter_idc (0 = actif, 1 = désactivé)
			if (mDeblock) {
				bs.Se(0); // slice_alpha_c0_offset_div2
				bs.Se(0); // slice_beta_offset_div2
			}

			// données de tranche : tous les macroblocs en ordre raster.
			for (int32 i = 0; i < mMbW * 4 * mMbH * 4; ++i) {
				mLumaNz[i] = 0;
				mI4Mode[i] = 2; // DC par défaut
			}
			for (int32 i = 0; i < mMbW * 2 * mMbH * 2; ++i)
				mChromaNz[0][i] = mChromaNz[1][i] = 0;
			for (int32 mbY = 0; mbY < mMbH; ++mbY)
				for (int32 mbX = 0; mbX < mMbW; ++mbX) {
					// Choix I_16×16 / I_4×4 par activité (détail fin des blocs 4×4). Heuristique simple :
					// zone plate → I_16×16 (en-tête compact) ; zone texturée → I_4×4 (prédiction fine).
					const int32 px = mbX * 16, py = mbY * 16;
					int64 detail = 0;
					for (int32 by = 0; by < 4; ++by)
						for (int32 bx = 0; bx < 4; ++bx) {
							int32 sum = 0;
							for (int32 r = 0; r < 4; ++r)
								for (int32 c = 0; c < 4; ++c)
									sum += mY[(usize)(py + by * 4 + r) * mLumaW + px + bx * 4 + c];
							const int32 mean = sum >> 4;
							for (int32 r = 0; r < 4; ++r)
								for (int32 c = 0; c < 4; ++c) {
									const int32 d = mY[(usize)(py + by * 4 + r) * mLumaW + px + bx * 4 + c] - mean;
									detail += d < 0 ? -d : d;
								}
						}
					if (detail > 256 * 6) // ~6 niveaux d'écart moyen/pixel → I_4×4
						EncodeMbIntra4x4(bs, mbX, mbY);
					else
						EncodeMacroblock(bs, mbX, mbY);
				}

			bs.TrailingBits();
			bs.EmitNal(out, 3, 5);

			// Déblocage en boucle : filtre la reconstruction (sortie + future référence P).
			DeblockFrame();
		}

		// --------------------------------------------------------------------------
		bool NkH264Encoder::EnableReconDump(const char *path) {
			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			mReconDump = mReconFile.Open(path, (NkFileMode)mode);
			return mReconDump;
		}

		bool NkH264Encoder::WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt) {
			if (!mOpen || !pixels)
				return false;
			ConvertToYuv(pixels, fmt);
			NkVector<uint8> out;
			EncodeFrame(out);
			if (out.Size() > 0)
				mFile.Write(out.Data(), out.Size());

			// dump de reconstruction (YUV420 planar recadré) pour validation externe.
			if (mReconDump) {
				const int32 cw = (mWidth + 1) / 2, ch = (mHeight + 1) / 2;
				for (int32 y = 0; y < mHeight; ++y)
					mReconFile.Write(&mRecY[(usize)y * mLumaW], (usize)mWidth);
				for (int32 y = 0; y < ch; ++y)
					mReconFile.Write(&mRecCb[(usize)y * mChromaW], (usize)cw);
				for (int32 y = 0; y < ch; ++y)
					mReconFile.Write(&mRecCr[(usize)y * mChromaW], (usize)cw);
			}
			++mFrame;
			return true;
		}

		bool NkH264Encoder::Close() {
			if (!mOpen)
				return false;
			mFile.Close();
			Free();
			mOpen = false;
			return true;
		}

		// --------------------------------------------------------------------------
		bool NkH264Encoder::SelfTest() {
			// Encode un dégradé 32×32 en mémoire et vérifie la présence des NAL SPS/PPS/IDR.
			NkH264Encoder enc;
			enc.mWidth = 32;
			enc.mHeight = 32;
			enc.mQp = 26;
			enc.mMbW = 2;
			enc.mMbH = 2;
			enc.mLumaW = 32;
			enc.mLumaH = 32;
			enc.mChromaW = 16;
			enc.mChromaH = 16;
			enc.mFrame = 0;
			const usize lumaN = 32 * 32, chromaN = 16 * 16;
			enc.mY = (uint8 *)memory::NkAlloc(lumaN);
			enc.mCb = (uint8 *)memory::NkAlloc(chromaN);
			enc.mCr = (uint8 *)memory::NkAlloc(chromaN);
			enc.mRecY = (uint8 *)memory::NkAlloc(lumaN);
			enc.mRecCb = (uint8 *)memory::NkAlloc(chromaN);
			enc.mRecCr = (uint8 *)memory::NkAlloc(chromaN);
			enc.mLumaNz = (int32 *)memory::NkAlloc((usize)8 * 8 * sizeof(int32));
			enc.mChromaNz[0] = (int32 *)memory::NkAlloc((usize)4 * 4 * sizeof(int32));
			enc.mChromaNz[1] = (int32 *)memory::NkAlloc((usize)4 * 4 * sizeof(int32));
			enc.mI4Mode = (int32 *)memory::NkAlloc((usize)8 * 8 * sizeof(int32));
			if (!enc.mY || !enc.mLumaNz || !enc.mChromaNz[0] || !enc.mChromaNz[1] || !enc.mI4Mode) {
				enc.Free();
				return false;
			}
			for (int32 y = 0; y < 32; ++y)
				for (int32 x = 0; x < 32; ++x)
					enc.mY[y * 32 + x] = (uint8)(x * 8 + y * 4);
			for (int32 i = 0; i < (int32)chromaN; ++i) {
				enc.mCb[i] = 128;
				enc.mCr[i] = 128;
			}

			NkVector<uint8> out;
			enc.EncodeFrame(out);
			enc.Free();

			if (out.Size() < 16)
				return false;
			// parcourt les NAL Annex-B, collecte les types rencontrés.
			bool sawSps = false, sawPps = false, sawIdr = false;
			for (uint64 i = 0; i + 4 < out.Size(); ++i) {
				if (out[i] == 0 && out[i + 1] == 0 && out[i + 2] == 0 && out[i + 3] == 1) {
					const int32 t = out[i + 4] & 0x1F;
					if (t == 7)
						sawSps = true;
					else if (t == 8)
						sawPps = true;
					else if (t == 5)
						sawIdr = true;
				}
			}
			return sawSps && sawPps && sawIdr;
		}

	} // namespace media
} // namespace nkentseu
