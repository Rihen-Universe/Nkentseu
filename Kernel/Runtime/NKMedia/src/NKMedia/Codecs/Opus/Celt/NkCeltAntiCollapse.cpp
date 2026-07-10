// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.cpp — anti-collapse CELT (bands.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h"

#include <cmath> // exp2f/sqrtf — math C

namespace nkentseu {
	namespace media {

		uint32 NkCeltAntiCollapse::LcgRand(uint32 seed) {
			return 1664525u * seed + 1013904223u;
		}

		void NkCeltAntiCollapse::RenormaliseVector(float32 *X, int32 N, float32 gain) {
			float32 e = 0.0f;
			for (int32 i = 0; i < N; ++i)
				e += X[i] * X[i];
			if (e <= 0.0f)
				return;
			const float32 g = gain / ::sqrtf(e);
			for (int32 i = 0; i < N; ++i)
				X[i] *= g;
		}

		uint32 NkCeltAntiCollapse::ApplyBand(float32 *X, int32 N0, int32 LM, uint32 collapseMask, float32 logE,
											 float32 prev1, float32 prev2, int32 pulses, uint32 seed) {
			const int32 blocks = 1 << LM;
			// profondeur ≈ (1+pulses)/N0 >> LM.
			const int32 depth = ((1 + pulses) / (N0 > 0 ? N0 : 1)) >> LM;
			const float32 thresh = 0.5f * ::exp2f(-0.125f * (float32)depth);
			const float32 sqrt1 = 1.0f / ::sqrtf((float32)(N0 << LM));

			const float32 pmin = prev1 < prev2 ? prev1 : prev2;
			float32 ediff = logE - pmin;
			if (ediff < 0.0f)
				ediff = 0.0f;
			float32 r = (ediff < 16.0f) ? 2.0f * ::exp2f(-ediff) : 0.0f;
			if (LM == 3)
				r *= 1.41421356f; // sqrt(2)
			if (r > thresh)
				r = thresh;
			r = r * sqrt1;

			int32 renormalize = 0;
			for (int32 k = 0; k < blocks; ++k) {
				if (!(collapseMask & (1u << k))) {
					for (int32 j = 0; j < N0; ++j) {
						seed = LcgRand(seed);
						X[(j << LM) + k] = (seed & 0x8000u) ? r : -r;
					}
					renormalize = 1;
				}
			}
			if (renormalize)
				RenormaliseVector(X, N0 << LM, 1.0f);
			return seed;
		}

		bool NkCeltAntiCollapse::SelfTest() {
			bool ok = true;

			// LCG : suite déterministe reproductible.
			{
				const uint32 s0 = 42u;
				const uint32 s1 = LcgRand(s0);
				const uint32 s2 = LcgRand(s0);
				if (s1 != s2)
					ok = false; // déterministe
				if (s1 == s0)
					ok = false;
			}

			// Renormalise : norme cible atteinte.
			{
				float32 X[4] = {3.0f, 4.0f, 0.0f, 0.0f}; // norme 5
				RenormaliseVector(X, 4, 2.0f);
				float32 n = 0.0f;
				for (int32 i = 0; i < 4; ++i)
					n += X[i] * X[i];
				n = ::sqrtf(n);
				if (n < 2.0f - 1e-4f || n > 2.0f + 1e-4f)
					ok = false;
			}

			// Bande entièrement effondrée → remplie de bruit ±r, renormalisée à la norme unité.
			{
				const int32 N0 = 8, LM = 2;
				const int32 N = N0 << LM;
				float32 X[64];
				for (int32 i = 0; i < N; ++i)
					X[i] = 0.0f;
				const uint32 mask = 0; // tous les blocs effondrés
				const uint32 s = ApplyBand(X, N0, LM, mask, /*logE*/ 3.0f, /*prev1*/ 0.0f, /*prev2*/ 0.0f,
										   /*pulses*/ 4, /*seed*/ 12345u);
				(void)s;
				// non nul.
				int32 nonZero = 0;
				float32 norm = 0.0f;
				for (int32 i = 0; i < N; ++i) {
					if (X[i] != 0.0f)
						nonZero = 1;
					norm += X[i] * X[i];
				}
				norm = ::sqrtf(norm);
				if (!nonZero)
					ok = false;
				if (norm < 1.0f - 1e-3f || norm > 1.0f + 1e-3f)
					ok = false; // renormalisée à 1
			}

			// Bande NON effondrée (masque plein) → inchangée.
			{
				const int32 N0 = 4, LM = 1;
				const int32 N = N0 << LM;
				float32 X[16];
				for (int32 i = 0; i < N; ++i)
					X[i] = 0.25f;
				const uint32 mask = (1u << (1 << LM)) - 1u; // tous blocs présents
				ApplyBand(X, N0, LM, mask, 3.0f, 0.0f, 0.0f, 4, 999u);
				for (int32 i = 0; i < N; ++i)
					if (X[i] < 0.25f - 1e-6f || X[i] > 0.25f + 1e-6f)
						ok = false; // inchangé
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
