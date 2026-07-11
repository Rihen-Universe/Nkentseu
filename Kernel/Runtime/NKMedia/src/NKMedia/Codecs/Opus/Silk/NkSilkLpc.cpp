// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkLpc.cpp
// Port fidèle de libopus (bwexpander_32.c, LPC_fit.c, LPC_inv_pred_gain.c,
// NLSF2A.c, table_LSF_cos.c).
// =============================================================================
#include "NkSilkLpc.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		// ── Table cosinus LSF (libopus silk/table_LSF_cos.c), Q12, 128+1 valeurs ──
		static const int16 kLSFCosTab_Q12[129] = {
			8192,  8190,  8182,  8170,  8152,  8130,  8104,  8072,  8034,  7994,  7946,  7896,  7840,
			7778,  7714,  7644,  7568,  7490,  7406,  7318,  7226,  7128,  7026,  6922,  6812,  6698,
			6580,  6458,  6332,  6204,  6070,  5934,  5792,  5648,  5502,  5352,  5198,  5040,  4880,
			4718,  4552,  4382,  4212,  4038,  3862,  3684,  3502,  3320,  3136,  2948,  2760,  2570,
			2378,  2186,  1990,  1794,  1598,  1400,  1202,  1002,  802,   602,   402,   202,   0,
			-202,  -402,  -602,  -802,  -1002, -1202, -1400, -1598, -1794, -1990, -2186, -2378, -2570,
			-2760, -2948, -3136, -3320, -3502, -3684, -3862, -4038, -4212, -4382, -4552, -4718, -4880,
			-5040, -5198, -5352, -5502, -5648, -5792, -5934, -6070, -6204, -6332, -6458, -6580, -6698,
			-6812, -6922, -7026, -7128, -7226, -7318, -7406, -7490, -7568, -7644, -7714, -7778, -7840,
			-7896, -7946, -7994, -8034, -8072, -8104, -8130, -8152, -8170, -8182, -8190, -8192,
		};

		// silk_bwexpander_32
		void NkSilkLpc::bwexpander32(int32 *ar, int32 d, int32 chirp_Q16) {
			const int32 chirp_minus_one_Q16 = chirp_Q16 - 65536;
			for (int32 i = 0; i < d - 1; i++) {
				ar[i] = M::SMULWW(chirp_Q16, ar[i]);
				chirp_Q16 += M::RSHIFT_ROUND(M::MUL(chirp_Q16, chirp_minus_one_Q16), 16);
			}
			ar[d - 1] = M::SMULWW(chirp_Q16, ar[d - 1]);
		}

		// silk_LPC_fit
		void NkSilkLpc::lpcFit(int16 *a_QOUT, int32 *a_QIN, int32 QOUT, int32 QIN, int32 d) {
			int32 i, k, idx = 0;
			for (i = 0; i < 10; i++) {
				int32 maxabs = 0;
				for (k = 0; k < d; k++) {
					const int32 absval = M::abs32(a_QIN[k]);
					if (absval > maxabs) {
						maxabs = absval;
						idx = k;
					}
				}
				maxabs = M::RSHIFT_ROUND(maxabs, QIN - QOUT);
				if (maxabs > M::kInt16Max) {
					maxabs = M::minInt(maxabs, 163838);
					const int32 chirp_Q16 = M::FIX_CONST(0.999, 16) -
											M::DIV32(M::LSHIFT(maxabs - M::kInt16Max, 14),
													 M::RSHIFT(M::MUL(maxabs, idx + 1), 2));
					bwexpander32(a_QIN, d, chirp_Q16);
				} else {
					break;
				}
			}
			if (i == 10) {
				for (k = 0; k < d; k++) {
					a_QOUT[k] = (int16)M::SAT16(M::RSHIFT_ROUND(a_QIN[k], QIN - QOUT));
					a_QIN[k] = M::LSHIFT((int32)a_QOUT[k], QIN - QOUT);
				}
			} else {
				for (k = 0; k < d; k++) {
					a_QOUT[k] = (int16)M::RSHIFT_ROUND(a_QIN[k], QIN - QOUT);
				}
			}
		}

		// LPC_inverse_pred_gain_QA_c (QA = 24)
		static int32 invPredGainQA(int32 *A_QA, int32 order) {
			static constexpr int32 QA = 24;
			static const int32 A_LIMIT = M::FIX_CONST(0.99975, QA);
			static const int32 kMinInvGain = M::FIX_CONST(1.0 / 1.0e4 /* MAX_PREDICTION_POWER_GAIN */, 30);

			int32 k, n;
			int32 invGain_Q30 = M::FIX_CONST(1, 30);
			for (k = order - 1; k > 0; k--) {
				if ((A_QA[k] > A_LIMIT) || (A_QA[k] < -A_LIMIT))
					return 0;
				const int32 rc_Q31 = -M::LSHIFT(A_QA[k], 31 - QA);
				const int32 rc_mult1_Q30 = M::SUB32(M::FIX_CONST(1, 30), M::SMMUL(rc_Q31, rc_Q31));
				invGain_Q30 = M::LSHIFT(M::SMMUL(invGain_Q30, rc_mult1_Q30), 2);
				if (invGain_Q30 < kMinInvGain)
					return 0;
				const int32 mult2Q = 32 - M::CLZ32(M::abs32(rc_mult1_Q30));
				const int32 rc_mult2 = M::INVERSE32_varQ(rc_mult1_Q30, mult2Q + 30);
				for (n = 0; n < (k + 1) >> 1; n++) {
					int64 tmp64;
					const int32 tmp1 = A_QA[n];
					const int32 tmp2 = A_QA[k - n - 1];
					tmp64 = M::RSHIFT_ROUND64(
						M::SMULL(M::SUB_SAT32(tmp1, M::MUL32_FRAC_Q(tmp2, rc_Q31, 31)), rc_mult2), mult2Q);
					if (tmp64 > M::kInt32Max || tmp64 < M::kInt32Min)
						return 0;
					A_QA[n] = (int32)tmp64;
					tmp64 = M::RSHIFT_ROUND64(
						M::SMULL(M::SUB_SAT32(tmp2, M::MUL32_FRAC_Q(tmp1, rc_Q31, 31)), rc_mult2), mult2Q);
					if (tmp64 > M::kInt32Max || tmp64 < M::kInt32Min)
						return 0;
					A_QA[k - n - 1] = (int32)tmp64;
				}
			}
			// k == 0
			if ((A_QA[0] > A_LIMIT) || (A_QA[0] < -A_LIMIT))
				return 0;
			const int32 rc_Q31 = -M::LSHIFT(A_QA[0], 31 - QA);
			const int32 rc_mult1_Q30 = M::SUB32(M::FIX_CONST(1, 30), M::SMMUL(rc_Q31, rc_Q31));
			invGain_Q30 = M::LSHIFT(M::SMMUL(invGain_Q30, rc_mult1_Q30), 2);
			if (invGain_Q30 < kMinInvGain)
				return 0;
			return invGain_Q30;
		}

		// silk_LPC_inverse_pred_gain_c (entrée Q12)
		int32 NkSilkLpc::lpcInversePredGain(const int16 *A_Q12, int32 order) {
			static constexpr int32 QA = 24;
			int32 Atmp_QA[kMaxOrderLpc];
			int32 DC_resp = 0;
			for (int32 k = 0; k < order; k++) {
				DC_resp += (int32)A_Q12[k];
				Atmp_QA[k] = M::LSHIFT((int32)A_Q12[k], QA - 12);
			}
			if (DC_resp >= 4096)
				return 0;
			return invPredGainQA(Atmp_QA, order);
		}

		// silk_NLSF2A_find_poly (QA = 16)
		static void findPoly(int32 *out, const int32 *cLSF, int32 dd) {
			static constexpr int32 QA = 16;
			out[0] = M::LSHIFT(1, QA);
			out[1] = -cLSF[0];
			for (int32 k = 1; k < dd; k++) {
				const int32 ftmp = cLSF[2 * k]; // QA
				out[k + 1] = M::LSHIFT(out[k - 1], 1) - (int32)M::RSHIFT_ROUND64(M::SMULL(ftmp, out[k]), QA);
				for (int32 n = k; n > 1; n--) {
					out[n] += out[n - 2] - (int32)M::RSHIFT_ROUND64(M::SMULL(ftmp, out[n - 1]), QA);
				}
				out[1] -= ftmp;
			}
		}

		// silk_NLSF2A
		void NkSilkLpc::nlsf2a(int16 *a_Q12, const int16 *NLSF, int32 d) {
			static constexpr int32 QA = 16;
			static const uint8 ordering16[16] = {0, 15, 8, 7, 4, 11, 12, 3, 2, 13, 10, 5, 6, 9, 14, 1};
			static const uint8 ordering10[10] = {0, 9, 6, 3, 4, 5, 8, 1, 2, 7};
			const uint8 *ordering = (d == 16) ? ordering16 : ordering10;

			int32 cos_LSF_QA[kMaxOrderLpc];
			int32 P[kMaxOrderLpc / 2 + 1], Q[kMaxOrderLpc / 2 + 1];
			int32 a32_QA1[kMaxOrderLpc];

			// LSF → 2*cos(LSF) via interpolation linéaire de la table.
			for (int32 k = 0; k < d; k++) {
				const int32 f_int = M::RSHIFT(NLSF[k], 15 - 7);				  // 0..127
				const int32 f_frac = NLSF[k] - M::LSHIFT(f_int, 15 - 7);	  // 0..255
				const int32 cos_val = kLSFCosTab_Q12[f_int];				  // Q12
				const int32 delta = kLSFCosTab_Q12[f_int + 1] - cos_val;	  // Q12
				cos_LSF_QA[ordering[k]] = M::RSHIFT_ROUND(M::LSHIFT(cos_val, 8) + M::MUL(delta, f_frac), 20 - QA);
			}

			const int32 dd = M::RSHIFT(d, 1);
			findPoly(P, &cos_LSF_QA[0], dd);
			findPoly(Q, &cos_LSF_QA[1], dd);

			for (int32 k = 0; k < dd; k++) {
				const int32 Ptmp = P[k + 1] + P[k];
				const int32 Qtmp = Q[k + 1] - Q[k];
				a32_QA1[k] = -Qtmp - Ptmp;			// QA+1
				a32_QA1[d - k - 1] = Qtmp - Ptmp;	// QA+1
			}

			lpcFit(a_Q12, a32_QA1, 12, QA + 1, d);

			// Si instable, expansion de bande jusqu'à stabilité (max 16 itérations).
			for (int32 i = 0; lpcInversePredGain(a_Q12, d) == 0 && i < 16; i++) {
				bwexpander32(a32_QA1, d, 65536 - M::LSHIFT(2, i));
				for (int32 k = 0; k < d; k++) {
					a_Q12[k] = (int16)M::RSHIFT_ROUND(a32_QA1[k], QA + 1 - 12);
				}
			}
		}

		bool NkSilkLpc::SelfTest() {
			// 1) Filtre trivialement instable : DC_resp >= 4096 → 0.
			{
				int16 a[10];
				for (int32 i = 0; i < 10; i++)
					a[i] = 4096;
				if (lpcInversePredGain(a, 10) != 0)
					return false;
			}

			// 2) NLSF ordonné et régulièrement espacé → filtre LPC STABLE (>0).
			//    Couvre tout le chemin NLSF2A (table cos + convolution + fit +
			//    boucle de stabilité) pour d = 10 et d = 16.
			for (int32 d = 10; d <= 16; d += 6) {
				int16 nlsf[16];
				for (int32 k = 0; k < d; k++)
					nlsf[k] = (int16)((int32)(k + 1) * 32768 / (d + 1));
				int16 a[16];
				nlsf2a(a, nlsf, d);
				if (lpcInversePredGain(a, d) <= 0)
					return false;
			}

			// 3) bwexpander : réduit l'amplitude (chirp < 1.0).
			{
				int32 ar[2] = {100000, 100000};
				bwexpander32(ar, 2, 32768); // chirp 0.5
				if (!(ar[0] > 0 && ar[0] < 100000))
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
