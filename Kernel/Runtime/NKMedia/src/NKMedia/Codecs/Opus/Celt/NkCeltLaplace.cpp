// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltLaplace.cpp — Laplace coder CELT (libopus laplace.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltLaplace.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 LAPLACE_LOG_MINP = 0;
			constexpr uint32 LAPLACE_MINP = (1u << LAPLACE_LOG_MINP); // 1
			constexpr int32 LAPLACE_NMIN = 16;

			uint32 IMin(uint32 a, uint32 b) {
				return a < b ? a : b;
			}
			int32 IMinI(int32 a, int32 b) {
				return a < b ? a : b;
			}

			uint32 LaplaceFreq1(uint32 fs0, int32 decay) {
				uint32 ft = 32768u - LAPLACE_MINP * (2 * LAPLACE_NMIN) - fs0;
				return (uint32)((uint64)ft * (uint32)(16384 - decay) >> 15);
			}
		} // namespace

		void NkCeltLaplace::Encode(NkOpusRangeEncoder &enc, int32 *value, uint32 fs, int32 decay) {
			uint32 fl = 0;
			int32 val = *value;
			if (val) {
				const int32 s = -(val < 0 ? 1 : 0);
				val = (val + s) ^ s;
				fl = fs;
				fs = LaplaceFreq1(fs, decay);
				int32 i;
				for (i = 1; fs > 0 && i < val; ++i) {
					fs *= 2;
					fl += fs + 2 * LAPLACE_MINP;
					fs = (uint32)(((uint64)fs * (uint32)decay) >> 15);
				}
				if (!fs) {
					int32 di;
					int32 ndi_max = (int32)((32768u - fl + LAPLACE_MINP - 1) >> LAPLACE_LOG_MINP);
					ndi_max = (ndi_max - s) >> 1;
					di = IMinI(val - i, ndi_max - 1);
					fl += (uint32)(2 * di + 1 + s) * LAPLACE_MINP;
					fs = IMin(LAPLACE_MINP, 32768u - fl);
					*value = (i + di + s) ^ s;
				} else {
					fs += LAPLACE_MINP;
					fl += fs & ~(uint32)s;
				}
			}
			enc.EncodeBin(fl, fl + fs, 15);
		}

		int32 NkCeltLaplace::Decode(NkOpusRangeDecoder &dec, uint32 fs, int32 decay) {
			int32 val = 0;
			uint32 fl = 0;
			const uint32 fm = dec.DecodeBin(15);
			if (fm >= fs) {
				val++;
				fl = fs;
				fs = LaplaceFreq1(fs, decay) + LAPLACE_MINP;
				while (fs > LAPLACE_MINP && fm >= fl + 2 * fs) {
					fs *= 2;
					fl += fs;
					fs = (uint32)(((uint64)(fs - 2 * LAPLACE_MINP) * (uint32)decay) >> 15);
					fs += LAPLACE_MINP;
					val++;
				}
				if (fs <= LAPLACE_MINP) {
					const int32 di = (int32)((fm - fl) >> (LAPLACE_LOG_MINP + 1));
					val += di;
					fl += (uint32)(2 * di) * LAPLACE_MINP;
				}
				if (fm < fl + fs)
					val = -val;
				else
					fl += fs;
			}
			dec.Update(fl, IMin(fl + fs, 32768u), 32768u);
			return val;
		}

		bool NkCeltLaplace::SelfTest() {
			bool ok = true;
			const uint32 CAP = 4096;
			uint8 *buffer = (uint8 *)memory::NkAlloc(CAP);
			if (!buffer)
				return false;

			// Paramètres typiques (Q15). Valeurs à coder, y compris grandes (clamp possible).
			const uint32 fsArr[3] = {8000, 16000, 24000};
			const int32 decayArr[3] = {2000, 6000, 12000};
			int32 vals[9] = {0, 1, -1, 3, -5, 10, -12, 40, -30};
			int32 coded[9]; // valeur réellement codée (après clamp éventuel)

			NkOpusRangeEncoder enc;
			enc.Init(buffer, CAP);
			for (int32 i = 0; i < 9; ++i) {
				coded[i] = vals[i];
				NkCeltLaplace::Encode(enc, &coded[i], fsArr[i % 3], decayArr[i % 3]);
			}
			enc.Done();
			if (enc.error != 0)
				ok = false;

			NkOpusRangeDecoder dec;
			dec.Init(buffer, CAP);
			for (int32 i = 0; i < 9; ++i) {
				const int32 v = NkCeltLaplace::Decode(dec, fsArr[i % 3], decayArr[i % 3]);
				if (v != coded[i])
					ok = false;
			}

			memory::NkFree(buffer);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
