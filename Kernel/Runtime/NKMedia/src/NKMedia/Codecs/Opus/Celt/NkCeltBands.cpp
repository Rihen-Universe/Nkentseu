// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltBands.cpp — table des bandes CELT (libopus).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"

namespace nkentseu {
	namespace media {

		// eband5ms (libopus modes.c) : 22 bornes → 21 bandes, base 2.5 ms (120 éch.) à 48 kHz.
		//   0 200 400 600 800 1k 1.2 1.4 1.6 2k 2.4 2.8 3.2 4k 4.8 5.6 6.8 8k 9.6 12k 15.6 20k
		static const int16 kEband5ms[22] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 12,
											14, 16, 20, 24, 28, 34, 40, 48, 60, 78, 100};

		const int16 *NkCeltBands::Eband5ms() {
			return kEband5ms;
		}

		int32 NkCeltBands::BandEdges(int32 LM, int32 *outEdges) {
			if (outEdges == nullptr)
				return 0;
			if (LM < 0)
				LM = 0;
			if (LM > 3)
				LM = 3;
			for (int32 i = 0; i <= kNumBands; ++i)
				outEdges[i] = (int32)kEband5ms[i] << LM;
			return kNumBands;
		}

		bool NkCeltBands::SelfTest() {
			bool ok = true;

			// 21 bandes, bornes strictement croissantes, dernière = 100 (×2^LM).
			int32 edges[kNumBands + 1];
			const int32 n = BandEdges(0, edges);
			if (n != kNumBands)
				ok = false;
			if (edges[0] != 0 || edges[kNumBands] != 100)
				ok = false;
			for (int32 i = 1; i <= kNumBands; ++i)
				if (edges[i] <= edges[i - 1])
					ok = false;

			// LM=3 (960 éch.) : tout est ×8 ; dernière borne = 800 bins (= 960/2 ... vérif d'échelle).
			int32 e3[kNumBands + 1];
			BandEdges(3, e3);
			if (e3[kNumBands] != 800)
				ok = false;
			for (int32 i = 0; i <= kNumBands; ++i)
				if (e3[i] != edges[i] * 8)
					ok = false;

			return ok;
		}

	} // namespace media
} // namespace nkentseu
