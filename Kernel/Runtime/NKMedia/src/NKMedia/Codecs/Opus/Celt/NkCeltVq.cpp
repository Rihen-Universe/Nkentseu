// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltVq.cpp — décodage forme de bande (vq.c alg_unquant).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltVq.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltPvq.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // cosf/sinf/sqrtf — math C

namespace nkentseu {
	namespace media {

		namespace {
			constexpr float32 kPi = 3.14159265358979323846f;

			// Une passe de rotation de Givens (exp_rotation1, vq.c).
			void ExpRotation1(float32 *X, int32 len, int32 stride, float32 c, float32 s) {
				const float32 ms = -s;
				float32 *Xptr = X;
				for (int32 i = 0; i < len - stride; ++i) {
					const float32 x1 = Xptr[0];
					const float32 x2 = Xptr[stride];
					Xptr[stride] = c * x2 + s * x1;
					*Xptr++ = c * x1 + ms * x2;
				}
				Xptr = &X[len - 2 * stride - 1];
				for (int32 i = len - 2 * stride - 1; i >= 0; --i) {
					const float32 x1 = Xptr[0];
					const float32 x2 = Xptr[stride];
					Xptr[stride] = c * x2 + s * x1;
					*Xptr-- = c * x1 + ms * x2;
				}
			}
		} // namespace

		void NkCeltVq::ExpRotation(float32 *X, int32 len, int32 dir, int32 stride, int32 K, int32 spread) {
			static const int32 SPREAD_FACTOR[3] = {15, 10, 5};
			if (2 * K >= len || spread == 0)
				return; // SPREAD_NONE
			const int32 factor = SPREAD_FACTOR[spread - 1];
			const float32 gain = (float32)len / (float32)(len + factor * K);
			const float32 theta = 0.5f * gain * gain;
			const float32 c = ::cosf(0.5f * kPi * theta);
			const float32 s = ::sinf(0.5f * kPi * theta);

			int32 stride2 = 0;
			if (len >= 8 * stride) {
				stride2 = 1;
				while ((stride2 * stride2 + stride2) * stride + (stride >> 2) < len)
					stride2++;
			}
			const int32 len2 = len / stride;
			for (int32 i = 0; i < stride; ++i) {
				if (dir < 0) {
					if (stride2)
						ExpRotation1(X + i * len2, len2, stride2, s, c);
					ExpRotation1(X + i * len2, len2, 1, c, s);
				} else {
					ExpRotation1(X + i * len2, len2, 1, c, -s);
					if (stride2)
						ExpRotation1(X + i * len2, len2, stride2, s, -c);
				}
			}
		}

		uint32 NkCeltVq::AlgUnquant(NkOpusRangeDecoder &dec, float32 *X, int32 N, int32 K, int32 spread, int32 B,
									float32 gain) {
			if (N <= 0)
				return 0;
			int32 stackIy[176];
			int32 *iy = (N <= 176) ? stackIy : (int32 *)memory::NkAlloc((size_t)N * sizeof(int32));
			for (int32 i = 0; i < N; ++i)
				iy[i] = 0;

			if (K > 0)
				NkCeltPvq::DecodePulses(dec, iy, N, K);

			// normalise_residual : X = gain * iy / sqrt(Σ iy²).
			float32 yy = 0.0f;
			for (int32 i = 0; i < N; ++i)
				yy += (float32)iy[i] * (float32)iy[i];
			const float32 g = (yy > 0.0f) ? (gain / ::sqrtf(yy)) : 0.0f;
			for (int32 i = 0; i < N; ++i)
				X[i] = g * (float32)iy[i];

			// exp_rotation (spreading), sens décodage.
			NkCeltVq::ExpRotation(X, N, -1, B, K, spread);

			// masque de collapse : blocs non nuls (pour l'anti-collapse, étape suivante).
			uint32 collapse = 0;
			const int32 N0 = (B > 0) ? N / B : N;
			for (int32 b = 0; b < B; ++b) {
				int32 nz = 0;
				for (int32 j = 0; j < N0; ++j)
					if (iy[b + j * B] != 0) {
						nz = 1;
						break;
					}
				if (nz)
					collapse |= (1u << b);
			}

			if (iy != stackIy)
				memory::NkFree(iy);
			return collapse;
		}

		bool NkCeltVq::SelfTest() {
			bool ok = true;
			uint8 buf[512];

			// Pour plusieurs (N,K,spread) : encode un vecteur de pulses, AlgUnquant, vérifie ‖X‖ = gain
			// (la normalisation donne la norme unité, la rotation orthonormale la conserve).
			const int32 cases[][2] = {{8, 3}, {16, 5}, {24, 7}, {12, 4}};
			for (uint32 ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ++ci) {
				const int32 N = cases[ci][0];
				const int32 K = cases[ci][1];
				for (int32 spread = 0; spread <= 3; ++spread) {
					// vecteur de pulses valide (somme |.| = K).
					int32 y[32];
					for (int32 i = 0; i < N; ++i)
						y[i] = 0;
					int32 rem = K;
					int32 idx = 0;
					while (rem > 0) {
						y[idx % N] += (idx & 1) ? -1 : 1;
						rem--;
						idx++;
					}
					// recompte K réel (les +/- peuvent s'annuler) → recalcule.
					int32 kReal = 0;
					for (int32 i = 0; i < N; ++i)
						kReal += y[i] < 0 ? -y[i] : y[i];
					if (kReal == 0)
						continue;

					NkOpusRangeEncoder enc;
					enc.Init(buf, 512);
					NkCeltPvq::EncodePulses(enc, y, N, kReal);
					enc.Done();

					float32 X[32];
					NkOpusRangeDecoder d;
					d.Init(buf, 512);
					const float32 gain = 1.0f;
					NkCeltVq::AlgUnquant(d, X, N, kReal, spread, 1, gain);

					float32 norm = 0.0f;
					for (int32 i = 0; i < N; ++i)
						norm += X[i] * X[i];
					norm = ::sqrtf(norm);
					if (norm < gain - 1e-3f || norm > gain + 1e-3f)
						ok = false; // norme conservée
				}
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
