// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltEnergy.cpp — énergie grossière CELT (quant_bands.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltEnergy.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltLaplace.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {

			// Coefficients de prédiction (libopus, Q15 → float).
			const float32 kPredCoef[4] = {29440.f / 32768.f, 26112.f / 32768.f, 21248.f / 32768.f, 16384.f / 32768.f};
			const float32 kBetaCoef[4] = {30147.f / 32768.f, 22282.f / 32768.f, 12124.f / 32768.f, 6554.f / 32768.f};
			const float32 kBetaIntra = 4915.f / 32768.f;

			// Modèle de probabilité Laplace par (LM, intra) : 42 octets = 21 bandes × 2 params.
			const uint8 kProb[4][2][42] = {
				{{72,  127, 65,  129, 66,  128, 65,  128, 64,  128, 62,  128, 64,  128, 64,  128, 92, 78, 92, 79, 92,
				  78,  90,  79,  116, 41,  115, 40,  114, 40,  132, 26,  132, 26,  145, 17,  161, 12, 176, 10, 177, 11},
				 {24,  179, 48,  138, 54,  135, 54,  132, 53,  134, 56,  133, 55,  132, 55,  132, 61, 114, 70, 96, 74,
				  88,  75,  88,  87,  74,  89,  66,  91,  67,  100, 59,  108, 50,  120, 40,  122, 37, 97,  43, 78, 50}},
				{{83,  78,  84,  81,  88,  75,  86,  74,  87,  71,  90,  73,  93,  74,  93,  74,  109, 40, 114, 36, 117,
				  34,  117, 34,  143, 17,  145, 18,  146, 19,  162, 12,  165, 10,  178, 7,   189, 6,  190, 8,  177, 9},
				 {23,  178, 54,  115, 63,  102, 66,  98,  69,  99,  74,  89,  71,  91,  73,  91,  78, 89, 86, 80, 92,
				  66,  93,  64,  102, 59,  103, 60,  104, 60,  117, 52,  123, 44,  138, 35,  133, 31, 97,  38, 77, 45}},
				{{61,  90,  93,  60,  105, 42,  107, 41,  110, 45,  116, 38,  113, 38,  112, 38,  124, 26, 132, 27, 136,
				  19,  140, 20,  155, 14,  159, 16,  158, 18,  170, 13,  177, 10,  187, 8,   192, 6,  175, 9,  159, 10},
				 {21,  178, 59,  110, 71,  86,  75,  85,  84,  83,  91,  66,  88,  73,  87,  72,  92, 75, 98, 72, 105,
				  58,  107, 54,  115, 52,  114, 55,  112, 56,  129, 51,  132, 40,  150, 33,  140, 29, 98,  35, 77, 42}},
				{{42,  108, 85,  57,  104, 46,  111, 33,  105, 40,  111, 33,  108, 33,  105, 38,  111, 27, 120, 29, 122,
				  22,  129, 20,  138, 16,  141, 20,  140, 20,  154, 15,  158, 10,  172, 9,   175, 7,  156, 8,  143, 8},
				 {17,  179, 47,  133, 71,  92,  78,  83,  89,  77,  92,  62,  90,  63,  89,  62,  92, 73, 99, 70, 111,
				  50,  111, 49,  117, 49,  116, 50,  115, 52,  130, 47,  132, 43,  149, 36,  138, 28, 99,  32, 76, 40}}};

			// small_energy_icdf (chemin bas-budget) : {2,1,0}.
			const uint8 kSmallEnergyIcdf[3] = {2, 1, 0};

			int32 IMinI(int32 a, int32 b) {
				return a < b ? a : b;
			}
			float32 Max16(float32 a, float32 b) {
				return a > b ? a : b;
			}

		} // namespace

		void NkCeltEnergy::UnquantCoarse(NkOpusRangeDecoder &dec, float32 *oldEBands, int32 nbBands, int32 start,
										 int32 end, bool intra, int32 C, int32 LM) {
			const uint8 *prob = kProb[LM][intra ? 1 : 0];
			const float32 coef = intra ? 0.0f : kPredCoef[LM];
			const float32 beta = intra ? kBetaIntra : kBetaCoef[LM];
			const int32 budget = (int32)dec.storage * 8;
			float32 prev[2] = {0.0f, 0.0f};

			for (int32 i = start; i < end; ++i) {
				for (int32 c = 0; c < C; ++c) {
					int32 qi;
					const int32 tell = dec.Tell();
					if (budget - tell >= 15) {
						const int32 pi = 2 * IMinI(i, 20);
						qi = NkCeltLaplace::Decode(dec, (uint32)prob[pi] << 7, (int32)prob[pi + 1] << 6);
					} else if (budget - tell >= 2) {
						qi = dec.DecodeIcdf(kSmallEnergyIcdf, 2);
						qi = (qi >> 1) ^ -(qi & 1);
					} else if (budget - tell >= 1) {
						qi = -dec.DecodeBitLogp(1);
					} else {
						qi = -1;
					}
					const float32 q = (float32)qi;
					float32 &e = oldEBands[i + c * nbBands];
					e = Max16(-9.0f, e);
					const float32 tmp = coef * e + prev[c] + q;
					e = tmp;
					prev[c] = prev[c] + q - beta * q;
				}
			}
		}

		void NkCeltEnergy::QuantCoarseSimple(NkOpusRangeEncoder &enc, const float32 *targetE, float32 *oldEBands,
											 int32 nbBands, int32 start, int32 end, bool intra, int32 C, int32 LM) {
			const uint8 *prob = kProb[LM][intra ? 1 : 0];
			const float32 coef = intra ? 0.0f : kPredCoef[LM];
			const float32 beta = intra ? kBetaIntra : kBetaCoef[LM];
			float32 prev[2] = {0.0f, 0.0f};

			for (int32 i = start; i < end; ++i) {
				for (int32 c = 0; c < C; ++c) {
					float32 &e = oldEBands[i + c * nbBands];
					e = Max16(-9.0f, e);
					// prédiction, puis qi = arrondi du résidu.
					const float32 pred = coef * e + prev[c];
					const float32 x = targetE[i + c * nbBands];
					int32 qi = (int32)(x - pred >= 0 ? (x - pred + 0.5f) : (x - pred - 0.5f));
					// encode qi (chemin Laplace ; Encode peut clamper → récupère la valeur codée).
					const int32 pi = 2 * IMinI(i, 20);
					int32 coded = qi;
					NkCeltLaplace::Encode(enc, &coded, (uint32)prob[pi] << 7, (int32)prob[pi + 1] << 6);
					const float32 q = (float32)coded;
					e = pred + q;
					prev[c] = prev[c] + q - beta * q;
				}
			}
		}

		void NkCeltEnergy::UnquantFine(NkOpusRangeDecoder &dec, float32 *oldEBands, int32 nbBands,
									   const int32 *fineQuant, int32 start, int32 end, int32 C) {
			for (int32 i = start; i < end; ++i) {
				if (fineQuant[i] <= 0)
					continue;
				for (int32 c = 0; c < C; ++c) {
					const uint32 q2 = dec.DecodeBits((uint32)fineQuant[i]);
					// offset = (q2 + 0.5) / 2^fineQuant - 0.5   (en dB).
					const float32 frac = (float32)(1 << fineQuant[i]);
					const float32 offset = ((float32)q2 + 0.5f) / frac - 0.5f;
					oldEBands[i + c * nbBands] += offset;
				}
			}
		}

		void NkCeltEnergy::QuantFineSimple(NkOpusRangeEncoder &enc, float32 *oldEBands, const float32 *residual,
										   int32 nbBands, const int32 *fineQuant, int32 start, int32 end, int32 C) {
			for (int32 i = start; i < end; ++i) {
				if (fineQuant[i] <= 0)
					continue;
				for (int32 c = 0; c < C; ++c) {
					const int32 frac = 1 << fineQuant[i];
					// q2 = floor((residual + 0.5) * frac), clampé à [0, frac-1].
					int32 q2 = (int32)((residual[i + c * nbBands] + 0.5f) * (float32)frac);
					if (q2 < 0)
						q2 = 0;
					if (q2 > frac - 1)
						q2 = frac - 1;
					enc.EncodeBits((uint32)q2, (uint32)fineQuant[i]);
					const float32 offset = ((float32)q2 + 0.5f) / (float32)frac - 0.5f;
					oldEBands[i + c * nbBands] += offset;
				}
			}
		}

		bool NkCeltEnergy::SelfTest() {
			bool ok = true;
			const int32 nb = NkCeltBands::kNumBands;
			const uint32 CAP = 8192;
			uint8 *buffer = (uint8 *)memory::NkAlloc(CAP);
			if (!buffer)
				return false;

			for (int32 lm = 0; lm < 4; ++lm) {
				for (int32 intra = 0; intra < 2; ++intra) {
					const int32 C = 1;
					// Énergies cibles (dB) déterministes (motif -5..5 dB).
					float32 target[21];
					for (int32 i = 0; i < nb; ++i)
						target[i] = (float32)((i * 7) % 11 - 5);
					// encode.
					float32 encE[21];
					for (int32 i = 0; i < nb; ++i)
						encE[i] = 0.0f;
					// Paramètres d'énergie fine : quelques bits par bande + résidus déterministes.
					int32 fineQuant[21];
					float32 residual[21];
					for (int32 i = 0; i < nb; ++i) {
						fineQuant[i] = (i % 4); // 0..3 bits
						residual[i] = (float32)((i * 3) % 7) / 7.0f - 0.5f; // -0.5..~0.5
					}

					NkOpusRangeEncoder enc;
					enc.Init(buffer, CAP);
					NkCeltEnergy::QuantCoarseSimple(enc, target, encE, nb, 0, nb, intra != 0, C, lm);
					NkCeltEnergy::QuantFineSimple(enc, encE, residual, nb, fineQuant, 0, nb, C);
					enc.Done();
					if (enc.error != 0)
						ok = false;
					// decode : coarse puis fine, dans le même flux.
					float32 decE[21];
					for (int32 i = 0; i < nb; ++i)
						decE[i] = 0.0f;
					NkOpusRangeDecoder dec;
					dec.Init(buffer, CAP);
					NkCeltEnergy::UnquantCoarse(dec, decE, nb, 0, nb, intra != 0, C, lm);
					NkCeltEnergy::UnquantFine(dec, decE, nb, fineQuant, 0, nb, C);
					// L'énergie reconstruite (grossière + fine) doit être identique enc/dec.
					for (int32 i = 0; i < nb; ++i) {
						const float32 d = decE[i] - encE[i];
						if (d > 1e-3f || d < -1e-3f)
							ok = false;
					}
				}
			}

			memory::NkFree(buffer);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
