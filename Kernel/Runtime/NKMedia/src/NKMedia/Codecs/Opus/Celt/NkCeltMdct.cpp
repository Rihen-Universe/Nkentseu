// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltMdct.cpp — MDCT/IMDCT directe + fenêtre + TDAC.
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltMdct.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // math C (cosf/sinf) — pas de STL

namespace nkentseu {
	namespace media {

		namespace {
			constexpr float32 kPi = 3.14159265358979323846f;
		}

		void NkCeltMdct::Forward(const float32 *in, int32 N, float32 *out) {
			if (!in || !out || N <= 0)
				return;
			const int32 twoN = 2 * N;
			const float32 n0 = 0.5f + (float32)N * 0.5f; // (1/2 + N/2)
			for (int32 k = 0; k < N; ++k) {
				const float32 kk = (float32)k + 0.5f;
				float32 acc = 0.0f;
				for (int32 n = 0; n < twoN; ++n)
					acc += in[n] * ::cosf((kPi / (float32)N) * ((float32)n + n0) * kk);
				out[k] = acc;
			}
		}

		void NkCeltMdct::Inverse(const float32 *in, int32 N, float32 *out) {
			if (!in || !out || N <= 0)
				return;
			const int32 twoN = 2 * N;
			const float32 n0 = 0.5f + (float32)N * 0.5f;
			const float32 scale = 2.0f / (float32)N;
			for (int32 n = 0; n < twoN; ++n) {
				const float32 nn = (float32)n + n0;
				float32 acc = 0.0f;
				for (int32 k = 0; k < N; ++k)
					acc += in[k] * ::cosf((kPi / (float32)N) * nn * ((float32)k + 0.5f));
				out[n] = acc * scale;
			}
		}

		void NkCeltMdct::SineWindow(int32 N, float32 *out2N) {
			if (!out2N || N <= 0)
				return;
			const int32 twoN = 2 * N;
			for (int32 n = 0; n < twoN; ++n)
				out2N[n] = ::sinf((kPi / (float32)twoN) * ((float32)n + 0.5f));
		}

		bool NkCeltMdct::SelfTest() {
			bool ok = true;
			const int32 N = 64;
			const int32 twoN = 2 * N;
			const int32 hop = N;
			const int32 sig = 4 * N; // signal de 256 échantillons

			float32 *win = (float32 *)memory::NkAlloc((size_t)twoN * sizeof(float32));
			SineWindow(N, win);

			// Vérif Princen-Bradley : w[n]^2 + w[n+N]^2 ≈ 1.
			for (int32 n = 0; n < N; ++n) {
				const float32 s = win[n] * win[n] + win[n + N] * win[n + N];
				if (s < 0.999f || s > 1.001f)
					ok = false;
			}

			// Signal déterministe.
			float32 *x = (float32 *)memory::NkAlloc((size_t)sig * sizeof(float32));
			for (int32 i = 0; i < sig; ++i)
				x[i] = 0.5f * ::sinf(0.13f * (float32)i) + 0.3f * ::cosf(0.07f * (float32)i + 1.0f);

			float32 *recon = (float32 *)memory::NkAlloc((size_t)sig * sizeof(float32));
			for (int32 i = 0; i < sig; ++i)
				recon[i] = 0.0f;

			float32 *frame = (float32 *)memory::NkAlloc((size_t)twoN * sizeof(float32));
			float32 *coef = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
			float32 *time = (float32 *)memory::NkAlloc((size_t)twoN * sizeof(float32));

			// Trames à 50% de recouvrement : analyse fenêtrée → MDCT → IMDCT → synthèse fenêtrée → overlap-add.
			for (int32 start = 0; start + twoN <= sig; start += hop) {
				for (int32 n = 0; n < twoN; ++n)
					frame[n] = x[start + n] * win[n];
				Forward(frame, N, coef);
				Inverse(coef, N, time);
				for (int32 n = 0; n < twoN; ++n)
					recon[start + n] += time[n] * win[n];
			}

			// Zone de reconstruction valide (recouvrement complet) : [N, sig-N).
			float32 maxErr = 0.0f;
			for (int32 i = N; i < sig - N; ++i) {
				const float32 e = recon[i] - x[i];
				const float32 ae = e < 0 ? -e : e;
				if (ae > maxErr)
					maxErr = ae;
			}
			if (maxErr > 1e-3f)
				ok = false;

			memory::NkFree(win);
			memory::NkFree(x);
			memory::NkFree(recon);
			memory::NkFree(frame);
			memory::NkFree(coef);
			memory::NkFree(time);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
