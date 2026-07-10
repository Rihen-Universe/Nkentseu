// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltSplit.cpp — haar1 + compute_qn (bands.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltSplit.h"

#include <cmath> // sqrtf — math C

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 BITRES = 3;
			const float32 kSqrtHalf = 0.70710678118654752440f; // 1/√2

			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}
			int32 Sudiv(int32 n, int32 d) {
				return d != 0 ? n / d : 0;
			}
		} // namespace

		void NkCeltSplit::Haar1(float32 *X, int32 N0, int32 stride) {
			N0 >>= 1;
			for (int32 i = 0; i < stride; ++i)
				for (int32 j = 0; j < N0; ++j) {
					const float32 a = kSqrtHalf * X[stride * 2 * j + i];
					const float32 b = kSqrtHalf * X[stride * (2 * j + 1) + i];
					X[stride * 2 * j + i] = a + b;
					X[stride * (2 * j + 1) + i] = a - b;
				}
		}

		int32 NkCeltSplit::ComputeQn(int32 N, int32 b, int32 offset, int32 pulseCap, int32 stereo) {
			static const int32 exp2_table8[8] = {16384, 17866, 19483, 21247, 23170, 25267, 27554, 30048};
			int32 qn, qb;
			int32 N2 = 2 * N - 1;
			if (stereo && N == 2)
				N2--;
			qb = Sudiv(b + N2 * offset, N2);
			qb = IMin(b - pulseCap - (4 << BITRES), qb);
			qb = IMin(8 << BITRES, qb);
			if (qb < ((1 << BITRES) >> 1)) {
				qn = 1;
			} else {
				qn = exp2_table8[qb & 0x7] >> (14 - (qb >> BITRES));
				qn = (qn + 1) >> 1 << 1;
			}
			return qn;
		}

		bool NkCeltSplit::SelfTest() {
			bool ok = true;

			// Haar1 : orthonormale (conserve la norme) et involutive (2× = identité).
			{
				float32 X[8] = {1.0f, -2.0f, 3.0f, 0.5f, -1.5f, 2.5f, 0.25f, -0.75f};
				float32 orig[8];
				float32 n0 = 0.0f;
				for (int32 i = 0; i < 8; ++i) {
					orig[i] = X[i];
					n0 += X[i] * X[i];
				}
				NkCeltSplit::Haar1(X, 8, 1);
				float32 n1 = 0.0f;
				for (int32 i = 0; i < 8; ++i)
					n1 += X[i] * X[i];
				if (n1 < n0 - 1e-4f || n1 > n0 + 1e-4f)
					ok = false; // norme conservée
				NkCeltSplit::Haar1(X, 8, 1);
				for (int32 i = 0; i < 8; ++i)
					if (X[i] < orig[i] - 1e-4f || X[i] > orig[i] + 1e-4f)
						ok = false; // involutive
			}

			// compute_qn : qn pair (ou 1), croissant avec le budget b, borné à 256.
			{
				int32 prev = 0;
				for (int32 b = 0; b <= 100 << BITRES; b += 8) {
					const int32 qn = NkCeltSplit::ComputeQn(8, b, 0, 40, 0);
					if (qn != 1 && (qn & 1))
						ok = false; // pair si > 1
					if (qn > 256)
						ok = false;
					if (qn < prev)
						ok = false; // non-décroissant
					prev = qn;
				}
				// petit budget → qn = 1 (pas de split).
				if (NkCeltSplit::ComputeQn(8, 0, 0, 40, 0) != 1)
					ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
