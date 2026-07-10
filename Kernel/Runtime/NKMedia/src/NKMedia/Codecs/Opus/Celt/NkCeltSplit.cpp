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
			int32 Ilog(uint32 v) {
				int32 r = 0;
				while (v) {
					++r;
					v >>= 1;
				}
				return r;
			}
			// FRAC_MUL16 (RFC 6716) : (16384 + a·b) >> 15.
			int32 FracMul16(int32 a, int32 b) {
				return (16384 + a * b) >> 15;
			}
		} // namespace

		int32 NkCeltSplit::BitexactCos(int32 x) {
			int32 tmp = (4096 + x * x) >> 13;
			int32 x2 = tmp;
			x2 = (32767 - x2) + FracMul16(x2, -7651 + FracMul16(x2, 8277 + FracMul16(-626, x2)));
			return 1 + x2;
		}

		int32 NkCeltSplit::BitexactLog2Tan(int32 isin, int32 icos) {
			const int32 lc = Ilog((uint32)icos);
			const int32 ls = Ilog((uint32)isin);
			icos <<= (15 - lc);
			isin <<= (15 - ls);
			return (ls - lc) * (1 << 11) + FracMul16(isin, FracMul16(isin, -2597) + 7932) -
				   FracMul16(icos, FracMul16(icos, -2597) + 7932);
		}

		uint32 NkCeltSplit::Isqrt32(uint32 val) {
			if (val == 0)
				return 0;
			uint32 g = 0;
			int32 bshift = (Ilog(val) - 1) >> 1;
			uint32 b = 1u << bshift;
			do {
				const uint32 t = (((uint32)g << 1) + b) << bshift;
				if (t <= val) {
					g += b;
					val -= t;
				}
				b >>= 1;
				bshift--;
			} while (bshift >= 0);
			return g;
		}

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

			// isqrt32 : racines entières exactes.
			{
				if (Isqrt32(0) != 0 || Isqrt32(16) != 4 || Isqrt32(24) != 4 || Isqrt32(25) != 5)
					ok = false;
				if (Isqrt32(99) != 9 || Isqrt32(100) != 10 || Isqrt32(1000000u) != 1000)
					ok = false;
			}

			// bitexact_cos : décroissant sur (0,16384], ~max près de 0, ~min près de 16384.
			{
				const int32 c1 = BitexactCos(1);
				const int32 cmid = BitexactCos(8192);
				const int32 chi = BitexactCos(16383);
				if (!(c1 > cmid && cmid > chi))
					ok = false; // strictement décroissant
				if (c1 < 32000)
					ok = false; // cos≈1 près de 0
				if (chi > 2000)
					ok = false; // cos≈0 près de π/2
			}

			// bitexact_log2tan : isin==icos → log2(tan)=log2(1)=0.
			{
				if (BitexactLog2Tan(10000, 10000) != 0)
					ok = false;
				// isin>icos → tan>1 → log>0 ; isin<icos → <0.
				if (!(BitexactLog2Tan(20000, 10000) > 0))
					ok = false;
				if (!(BitexactLog2Tan(10000, 20000) < 0))
					ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
