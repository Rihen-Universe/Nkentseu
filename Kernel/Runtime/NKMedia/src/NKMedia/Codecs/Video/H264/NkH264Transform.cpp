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
			const int32 m = qp % 6;
			const int32 v = kV[m][0];
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
