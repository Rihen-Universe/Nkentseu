// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDecoder.cpp — orchestration CELT (celt_decode).
// EN COURS : flags + chemin silence corrects ; chemin non-silence en assemblage.
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {
			const uint8 kTapsetIcdf[3] = {2, 1, 0};
			constexpr float32 kMinEnergyDb = -28.0f;
		}

		void NkCeltDecoder::Init(int32 C) {
			mC = (C < 1) ? 1 : (C > kMaxChannels ? kMaxChannels : C);
			for (int32 i = 0; i < kNumBands * kMaxChannels; ++i)
				mOldEBands[i] = kMinEnergyDb;
			for (int32 c = 0; c < kMaxChannels; ++c)
				mPreemphMem[c] = 0.0f;
			for (int32 i = 0; i < kOverlap * kMaxChannels; ++i)
				mOverlapMem[i] = 0.0f;
			mInit = true;
		}

		bool NkCeltDecoder::DecodeFrame(const uint8 *data, int32 len, int32 LM, float32 *pcm, NkFrameFlags *outFlags) {
			if (!mInit)
				Init(mC);
			if (data == nullptr || len <= 0 || pcm == nullptr || LM < 0 || LM > 3)
				return false;

			const int32 C = mC;
			const int32 N = (1 << LM) * kShortMdctSize;
			const int32 total = len * 8; // budget en bits

			NkOpusRangeDecoder dec;
			dec.Init(data, (uint32)len);

			// --- Drapeau SILENCE (ordre ec exact). ---
			bool silence;
			const int32 tell0 = dec.Tell();
			if (tell0 >= total)
				silence = true;
			else if (tell0 == 1)
				silence = dec.DecodeBitLogp(15) != 0;
			else
				silence = false;

			bool transient = false;
			bool intra = false;

			if (!silence) {
				// --- Post-filtre (start==0). On lit les paramètres pour NE PAS désynchroniser l'ec. ---
				if (dec.Tell() + 16 <= total) {
					if (dec.DecodeBitLogp(1)) {
						const uint32 octave = dec.DecodeUint(6);
						(void)((16u << octave) + dec.DecodeBits(4 + (uint32)octave) - 1u); // pitch
						(void)dec.DecodeBits(3);									   // qg (gain)
						if (dec.Tell() + 2 <= total)
							(void)dec.DecodeIcdf(kTapsetIcdf, 2); // tapset
					}
				}
				// --- Transient. ---
				if (LM > 0 && dec.Tell() + 3 <= total)
					transient = dec.DecodeBitLogp(3) != 0;
				// --- Intra. ---
				if (dec.Tell() + 3 <= total)
					intra = dec.DecodeBitLogp(3) != 0;
			}

			if (outFlags) {
				outFlags->silence = silence;
				outFlags->transient = transient;
				outFlags->intra = intra;
			}

			// --- Reconstruction ---
			// Tampon temps (par canal), N échantillons.
			float32 *time = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));

			for (int32 c = 0; c < C; ++c) {
				if (silence) {
					// Énergies au minimum, spectre nul → sortie = 0 (+ queue d'overlap, nulle en régime établi).
					for (int32 i = 0; i < kNumBands; ++i)
						mOldEBands[i + c * kNumBands] = kMinEnergyDb;
					for (int32 i = 0; i < N; ++i)
						time[i] = 0.0f;
				} else {
					// TODO (assemblage en cours) : unquant_coarse_energy(intra) → tf → spread → dynalloc →
					// compute_allocation → quant_all_bands (AlgUnquant par bande) → anti-collapse → denorm →
					// IMDCT CELT + overlap-add. Pour l'instant : silence de repli (chemin incomplet).
					for (int32 i = 0; i < N; ++i)
						time[i] = 0.0f;
				}

				// Deemphasis → PCM interleaved.
				float32 *pcmC = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
				NkCeltDeemphasis::Apply(time, pcmC, N, NkCeltDeemphasis::kPreemphCoef48k, &mPreemphMem[c]);
				for (int32 i = 0; i < N; ++i)
					pcm[i * C + c] = pcmC[i];
				memory::NkFree(pcmC);
			}

			memory::NkFree(time);
			return true;
		}

		bool NkCeltDecoder::SelfTest() {
			bool ok = true;
			const uint32 CAP = 256;
			uint8 *buf = (uint8 *)memory::NkAlloc(CAP);
			if (!buf)
				return false;

			// Fabrique une trame CELT de SILENCE : bit de silence = 1 (écrit quand tell==1).
			NkOpusRangeEncoder enc;
			enc.Init(buf, CAP);
			enc.EncodeBitLogp(1, 15); // silence = 1
			enc.Done();
			const int32 len = (int32)enc.RangeBytes();

			const int32 LM = 3;
			const int32 N = (1 << LM) * kShortMdctSize;
			const int32 C = 1;

			NkCeltDecoder decoder;
			decoder.Init(C);
			float32 *pcm = (float32 *)memory::NkAlloc((size_t)N * C * sizeof(float32));
			NkCeltDecoder::NkFrameFlags flags;
			const bool okDec = decoder.DecodeFrame(buf, len > 0 ? len : 1, LM, pcm, &flags);
			if (!okDec)
				ok = false;
			if (!flags.silence)
				ok = false; // le flag silence doit être détecté
			// PCM tout zéro (1re trame de silence).
			for (int32 i = 0; i < N * C; ++i)
				if (pcm[i] < -1e-6f || pcm[i] > 1e-6f)
					ok = false;

			memory::NkFree(buf);
			memory::NkFree(pcm);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
