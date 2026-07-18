// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Transform.cpp — transformée + quant H.264.
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Transform.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Classe de position (0 : (pair,pair) ; 1 : (impair,impair) ; 2 : autres).
			inline int32 PosClass(int32 idx) {
				const int32 r = idx >> 2, c = idx & 3;
				const int32 re = r & 1, ce = c & 1;
				if (re == 0 && ce == 0)
					return 0;
				if (re == 1 && ce == 1)
					return 1;
				return 2;
			}
			// MF (quantification) : [qp%6][classe].
			const int32 kMF[6][3] = {{13107, 5243, 8066}, {11916, 4660, 7490}, {10082, 4194, 6554},
									 {9362, 3647, 5825},   {8192, 3355, 5243}, {7282, 2893, 4559}};
			// normAdjust (déquantification) : [qp%6][classe].
			const int32 kV[6][3] = {{10, 16, 13}, {11, 18, 14}, {13, 20, 16},
									{14, 23, 18}, {16, 25, 20}, {18, 29, 23}};

			inline int32 Sign(int32 v) {
				return v < 0 ? -1 : 1;
			}
		} // namespace

		void NkH264Transform::Forward4x4(const int32 in[16], int32 out[16]) {
			int32 t[16];
			for (int32 i = 0; i < 4; ++i) {
				const int32 *r = in + i * 4;
				const int32 a = r[0] + r[3], b = r[1] + r[2], c = r[1] - r[2], d = r[0] - r[3];
				t[i * 4 + 0] = a + b;
				t[i * 4 + 1] = 2 * d + c;
				t[i * 4 + 2] = a - b;
				t[i * 4 + 3] = d - 2 * c;
			}
			for (int32 i = 0; i < 4; ++i) {
				const int32 a = t[0 * 4 + i] + t[3 * 4 + i], b = t[1 * 4 + i] + t[2 * 4 + i];
				const int32 c = t[1 * 4 + i] - t[2 * 4 + i], d = t[0 * 4 + i] - t[3 * 4 + i];
				out[0 * 4 + i] = a + b;
				out[1 * 4 + i] = 2 * d + c;
				out[2 * 4 + i] = a - b;
				out[3 * 4 + i] = d - 2 * c;
			}
		}

		void NkH264Transform::Inverse4x4(const int32 in[16], int32 out[16]) {
			int32 t[16];
			for (int32 i = 0; i < 4; ++i) {
				const int32 *r = in + i * 4;
				const int32 a = r[0] + r[2], b = r[0] - r[2], c = (r[1] >> 1) - r[3], d = r[1] + (r[3] >> 1);
				t[i * 4 + 0] = a + d;
				t[i * 4 + 1] = b + c;
				t[i * 4 + 2] = b - c;
				t[i * 4 + 3] = a - d;
			}
			for (int32 i = 0; i < 4; ++i) {
				const int32 a = t[0 * 4 + i] + t[2 * 4 + i], b = t[0 * 4 + i] - t[2 * 4 + i];
				const int32 c = (t[1 * 4 + i] >> 1) - t[3 * 4 + i], d = t[1 * 4 + i] + (t[3 * 4 + i] >> 1);
				out[0 * 4 + i] = (a + d + 32) >> 6;
				out[1 * 4 + i] = (b + c + 32) >> 6;
				out[2 * 4 + i] = (b - c + 32) >> 6;
				out[3 * 4 + i] = (a - d + 32) >> 6;
			}
		}

		void NkH264Transform::HadamardForward4x4(const int32 in[16], int32 out[16]) {
			int32 t[16];
			for (int32 i = 0; i < 4; ++i) {
				const int32 *r = in + i * 4;
				const int32 a = r[0] + r[3], b = r[1] + r[2], c = r[1] - r[2], d = r[0] - r[3];
				t[i * 4 + 0] = a + b;
				t[i * 4 + 1] = d + c;
				t[i * 4 + 2] = a - b;
				t[i * 4 + 3] = d - c;
			}
			for (int32 i = 0; i < 4; ++i) {
				const int32 a = t[0 * 4 + i] + t[3 * 4 + i], b = t[1 * 4 + i] + t[2 * 4 + i];
				const int32 c = t[1 * 4 + i] - t[2 * 4 + i], d = t[0 * 4 + i] - t[3 * 4 + i];
				out[0 * 4 + i] = (a + b) >> 1;
				out[1 * 4 + i] = (d + c) >> 1;
				out[2 * 4 + i] = (a - b) >> 1;
				out[3 * 4 + i] = (d - c) >> 1;
			}
		}

		void NkH264Transform::HadamardInverse4x4(const int32 in[16], int32 out[16]) {
			int32 t[16];
			for (int32 i = 0; i < 4; ++i) {
				const int32 *r = in + i * 4;
				const int32 a = r[0] + r[2], b = r[0] - r[2], c = r[1] - r[3], d = r[1] + r[3];
				t[i * 4 + 0] = a + d;
				t[i * 4 + 1] = b + c;
				t[i * 4 + 2] = b - c;
				t[i * 4 + 3] = a - d;
			}
			for (int32 i = 0; i < 4; ++i) {
				const int32 a = t[0 * 4 + i] + t[2 * 4 + i], b = t[0 * 4 + i] - t[2 * 4 + i];
				const int32 c = t[1 * 4 + i] - t[3 * 4 + i], d = t[1 * 4 + i] + t[3 * 4 + i];
				out[0 * 4 + i] = a + d;
				out[1 * 4 + i] = b + c;
				out[2 * 4 + i] = b - c;
				out[3 * 4 + i] = a - d;
			}
		}

		void NkH264Transform::Quant4x4(const int32 coef[16], int32 lvl[16], int32 qp, bool intra) {
			const int32 qbits = 15 + qp / 6;
			const int32 m = qp % 6;
			const int32 f = (1 << qbits) / (intra ? 3 : 6);
			for (int32 i = 0; i < 16; ++i) {
				const int32 mf = kMF[m][PosClass(i)];
				const int32 a = coef[i] < 0 ? -coef[i] : coef[i];
				lvl[i] = Sign(coef[i]) * (int32)(((int64)a * mf + f) >> qbits);
			}
		}

		void NkH264Transform::Dequant4x4(const int32 lvl[16], int32 coef[16], int32 qp) {
			const int32 shift = qp / 6;
			const int32 m = qp % 6;
			for (int32 i = 0; i < 16; ++i)
				coef[i] = (lvl[i] * kV[m][PosClass(i)]) << shift;
		}

		void NkH264Transform::QuantDC(const int32 dc[16], int32 lvl[16], int32 n, int32 qp, bool intra) {
			// Le DC utilise la classe 0. Facteur ×2 et qbits+1 (spec 8.5.10).
			const int32 qbits = 15 + qp / 6;
			const int32 m = qp % 6;
			const int32 mf = kMF[m][0];
			const int32 f = (1 << (qbits + 1)) / (intra ? 3 : 6);
			for (int32 i = 0; i < n; ++i) {
				const int32 a = dc[i] < 0 ? -dc[i] : dc[i];
				lvl[i] = Sign(dc[i]) * (int32)(((int64)a * mf + f) >> (qbits + 1));
			}
		}

		void NkH264Transform::DequantDC(const int32 lvl[16], int32 dc[16], int32 n, int32 qp) {
			// Entrée = coefficients APRÈS Hadamard inverse (les niveaux ont déjà traversé A·(·)·A).
			// LevelScale = weightScale(16) × normAdjust(kV) → facteur ×16 (spec 8.5.10.2).
			const int32 m = qp % 6;
			const int32 v = 16 * kV[m][0];
			if (qp >= 36) {
				const int32 shift = qp / 6 - 6;
				for (int32 i = 0; i < n; ++i)
					dc[i] = (lvl[i] * v) << shift;
			} else {
				const int32 shift = 6 - qp / 6;
				const int32 add = 1 << (5 - qp / 6);
				for (int32 i = 0; i < n; ++i)
					dc[i] = (lvl[i] * v + add) >> shift;
			}
		}

		void NkH264Transform::Dequant8x8(const int32 lvl[64], int32 coef[64], int32 qp) {
			// normAdjust8x8 (Table 8-14) : 6 valeurs par (qp%6), selectionnees par la position (i,j).
			static const int32 v8[6][6] = {{20, 18, 32, 19, 25, 24}, {22, 19, 35, 21, 28, 26},
				{26, 23, 42, 24, 33, 31}, {28, 25, 45, 26, 35, 33},
				{32, 28, 51, 30, 40, 38}, {36, 32, 58, 34, 46, 43}};
			const int32 m = qp % 6, shift = qp / 6;
			for (int32 idx = 0; idx < 64; ++idx) {
				const int32 i = idx >> 3, j = idx & 7;
				const int32 i4 = i & 3, j4 = j & 3, i2 = i & 1, j2 = j & 1;
				int32 cls;
				if (i4 == 0 && j4 == 0) cls = 0;
				else if (i2 == 1 && j2 == 1) cls = 1;
				else if (i4 == 2 && j4 == 2) cls = 2;
				else if ((i4 == 0 && j2 == 1) || (i2 == 1 && j4 == 0)) cls = 3;
				else if ((i4 == 0 && j4 == 2) || (i4 == 2 && j4 == 0)) cls = 4;
				else cls = 5;
				const int32 ls = 16 * v8[m][cls]; // weightScale FLAT=16 ; LevelScale = 16 * v8
				if (qp >= 36) coef[idx] = (lvl[idx] * ls) << (shift - 6);
				else { const int32 sh = 6 - shift; coef[idx] = (lvl[idx] * ls + (1 << (sh - 1))) >> sh; }
			}
		}

		void NkH264Transform::Inverse8x8(const int32 in[64], int32 out[64]) {
			// Inverse 8x8 separable (8.5.13.2) : 1D sur lignes puis colonnes ; (.+32)>>6 a la fin.
			auto idct1d = [](const int32 d[8], int32 e[8]) {
				const int32 a0 = d[0] + d[4], a4 = d[0] - d[4];
				const int32 a2 = (d[2] >> 1) - d[6], a6 = d[2] + (d[6] >> 1);
				const int32 b0 = a0 + a6, b2 = a4 + a2, b4 = a4 - a2, b6 = a0 - a6;
				const int32 a1 = -d[3] + d[5] - d[7] - (d[7] >> 1);
				const int32 a3 = d[1] + d[7] - d[3] - (d[3] >> 1);
				const int32 a5 = -d[1] + d[7] + d[5] + (d[5] >> 1);
				const int32 a7 = d[3] + d[5] + d[1] + (d[1] >> 1);
				const int32 b1 = a1 + (a7 >> 2), b7 = a7 - (a1 >> 2);
				const int32 b3 = a3 + (a5 >> 2), b5 = (a3 >> 2) - a5;
				e[0] = b0 + b7; e[7] = b0 - b7; e[1] = b2 + b5; e[6] = b2 - b5;
				e[2] = b4 + b3; e[5] = b4 - b3; e[3] = b6 + b1; e[4] = b6 - b1;
			};
			int32 t[64], row[8], er[8];
			for (int32 i = 0; i < 8; ++i) {
				for (int32 j = 0; j < 8; ++j) row[j] = in[i * 8 + j];
				idct1d(row, er);
				for (int32 j = 0; j < 8; ++j) t[i * 8 + j] = er[j];
			}
			int32 col[8], ec[8];
			for (int32 j = 0; j < 8; ++j) {
				for (int32 i = 0; i < 8; ++i) col[i] = t[i * 8 + j];
				idct1d(col, ec);
				for (int32 i = 0; i < 8; ++i) out[i * 8 + j] = (ec[i] + 32) >> 6;
			}
		}

		void NkH264Transform::Hadamard2x2(const int32 in[4], int32 out[4]) {
			// 2×2 Hadamard (in raster [TL,TR,BL,BR]) ; directe = inverse (à un facteur 4 près,
			// absorbé par la paire quant/dequant).
			out[0] = in[0] + in[1] + in[2] + in[3];
			out[1] = in[0] - in[1] + in[2] - in[3];
			out[2] = in[0] + in[1] - in[2] - in[3];
			out[3] = in[0] - in[1] - in[2] + in[3];
		}

		void NkH264Transform::QuantChromaDC(const int32 f[4], int32 lvl[4], int32 qp, bool intra) {
			const int32 qbits = 15 + qp / 6;
			const int32 m = qp % 6;
			const int32 mf = kMF[m][0];
			const int32 off = (1 << (qbits + 1)) / (intra ? 3 : 6);
			for (int32 i = 0; i < 4; ++i) {
				const int32 a = f[i] < 0 ? -f[i] : f[i];
				lvl[i] = Sign(f[i]) * (int32)(((int64)a * mf + off) >> (qbits + 1));
			}
		}

		void NkH264Transform::DequantChromaDC(const int32 g[4], int32 dc[4], int32 qp) {
			// Entrée = niveaux APRÈS Hadamard inverse 2×2. Spec 8.5.11.2 : dcC = (g·LevelScale)<<(qP/6) >>5.
			const int32 m = qp % 6;
			const int32 v = 16 * kV[m][0];
			const int32 sh = qp / 6;
			for (int32 i = 0; i < 4; ++i)
				dc[i] = ((g[i] * v) << sh) >> 5;
		}

		int32 NkH264Transform::ChromaQp(int32 lumaQp) {
			// qPi = Clip3(0,51,QPy) → QPc (table 8-15). Identité < 30, puis compression.
			static const int32 kMap[52] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
										   13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
										   26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35,
										   35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39};
			const int32 qpi = lumaQp < 0 ? 0 : (lumaQp > 51 ? 51 : lumaQp);
			return kMap[qpi];
		}

		bool NkH264Transform::SelfTest() {
			bool ok = true;

			// 1) Cœur direct/inverse : round-trip d'un résidu, PSNR élevé à bas QP.
			for (int32 qp = 6; qp <= 36; qp += 10) {
				double se = 0.0;
				int32 nblk = 0;
				for (int32 seed = 1; seed <= 8; ++seed) {
					int32 res[16], coef[16], lvl[16], deq[16], rec[16];
					int32 s = seed * 2654435761u;
					for (int32 i = 0; i < 16; ++i) {
						s = s * 1103515245 + 12345;
						res[i] = ((s >> 16) & 0x3F) - 32; // résidu -32..31
					}
					NkH264Transform::Forward4x4(res, coef);
					NkH264Transform::Quant4x4(coef, lvl, qp, true);
					NkH264Transform::Dequant4x4(lvl, deq, qp);
					NkH264Transform::Inverse4x4(deq, rec);
					for (int32 i = 0; i < 16; ++i) {
						const double d = (double)rec[i] - res[i];
						se += d * d;
					}
					++nblk;
				}
				const double mse = se / (nblk * 16);
				// Round-trip cohérent : quasi sans perte à bas QP, erreur bornée croissante avec QP.
				if (qp <= 6 && mse > 4.0)
					ok = false;
				if (mse > 300.0)
					ok = false; // borne large tous QP (résidu ±32, variance ~340)
			}

			// 2) Hadamard : matrice orthogonale correcte — un DC constant reste concentré en (0,0).
			{
				int32 in[16], f[16];
				for (int32 i = 0; i < 16; ++i)
					in[i] = 5; // bloc DC constant
				NkH264Transform::HadamardForward4x4(in, f);
				// Toute l'énergie doit être en f[0] ; les autres coefficients nuls.
				if (f[0] == 0)
					ok = false;
				for (int32 i = 1; i < 16; ++i)
					if (f[i] != 0)
						ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
