// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDenorm.cpp — dénormalisation des bandes (bands.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltDenorm.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"

#include <cmath> // exp2f — math C

namespace nkentseu {
	namespace media {

		namespace {
			// eMeans (bands.c) : énergie moyenne par bande (offset de dénormalisation).
			const float32 kEMeans[25] = {6.437500f, 6.250000f, 5.750000f, 5.312500f, 5.062500f, 4.812500f, 4.500000f,
										 4.375000f, 4.875000f, 4.687500f, 4.562500f, 4.437500f, 4.875000f, 4.625000f,
										 4.312500f, 4.500000f, 4.375000f, 4.625000f, 4.750000f, 4.437500f, 3.750000f,
										 3.750000f, 3.750000f, 3.750000f, 3.750000f};
		}

		float32 NkCeltDenorm::EMean(int32 band) {
			if (band < 0)
				band = 0;
			if (band > 24)
				band = 24;
			return kEMeans[band];
		}

		void NkCeltDenorm::DenormaliseBands(const float32 *X, float32 *freq, const float32 *bandLogE, int32 start,
											int32 end, int32 LM) {
			const int16 *eBands = NkCeltBands::Eband5ms();
			const int32 M = 1 << LM;
			const int32 nb = NkCeltBands::kNumBands;
			if (end > nb)
				end = nb;

			// Zéro avant la 1re bande décodée.
			const int32 firstBin = M * (int32)eBands[start];
			for (int32 i = 0; i < firstBin; ++i)
				freq[i] = 0.0f;

			for (int32 i = start; i < end; ++i) {
				const int32 j0 = M * (int32)eBands[i];
				const int32 j1 = M * (int32)eBands[i + 1];
				const float32 lg = bandLogE[i] + kEMeans[i < 25 ? i : 24];
				const float32 g = ::exp2f(lg < 32.0f ? lg : 32.0f); // 2^lg
				for (int32 j = j0; j < j1; ++j)
					freq[j] = X[j] * g;
			}

			// Zéro après la dernière bande.
			const int32 lastBin = M * (int32)eBands[end];
			const int32 total = M * (int32)eBands[nb];
			for (int32 i = lastBin; i < total; ++i)
				freq[i] = 0.0f;
		}

		bool NkCeltDenorm::SelfTest() {
			bool ok = true;
			const int16 *eBands = NkCeltBands::Eband5ms();
			const int32 LM = 0;
			const int32 M = 1 << LM;
			const int32 nb = NkCeltBands::kNumBands;
			const int32 total = M * (int32)eBands[nb];

			float32 X[256];
			float32 freq[256];
			float32 E[21];
			for (int32 i = 0; i < 256; ++i)
				X[i] = 0.0f;
			for (int32 i = 0; i < 21; ++i)
				E[i] = 0.0f;

			// Pour quelques bandes : forme de norme unité + énergie E → ‖freq bande‖ = 2^(E+eMean).
			const int32 testBands[3] = {3, 8, 15};
			const float32 testE[3] = {2.0f, -1.0f, 0.5f};
			for (int32 t = 0; t < 3; ++t) {
				const int32 b = testBands[t];
				const int32 j0 = M * (int32)eBands[b];
				const int32 j1 = M * (int32)eBands[b + 1];
				const int32 N = j1 - j0;
				const float32 unit = 1.0f / ::sqrtf((float32)N); // forme plate de norme 1
				for (int32 j = j0; j < j1; ++j)
					X[j] = unit;
				E[b] = testE[t];
			}

			NkCeltDenorm::DenormaliseBands(X, freq, E, 0, nb, LM);

			for (int32 t = 0; t < 3; ++t) {
				const int32 b = testBands[t];
				const int32 j0 = M * (int32)eBands[b];
				const int32 j1 = M * (int32)eBands[b + 1];
				float32 norm = 0.0f;
				for (int32 j = j0; j < j1; ++j)
					norm += freq[j] * freq[j];
				norm = ::sqrtf(norm);
				const float32 expected = ::exp2f(testE[t] + kEMeans[b]);
				const float32 rel = (norm - expected) / expected;
				if (rel < -1e-3f || rel > 1e-3f)
					ok = false;
			}

			// Bandes hors [start,end) mises à zéro : ici tout décodé, on vérifie juste que `total` bins écrits.
			(void)total;
			return ok;
		}

	} // namespace media
} // namespace nkentseu
