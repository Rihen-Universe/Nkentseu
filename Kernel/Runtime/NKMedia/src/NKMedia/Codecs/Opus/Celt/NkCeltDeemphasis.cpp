// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.cpp — deemphasis CELT (celt_decoder.c).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h"

namespace nkentseu {
	namespace media {

		void NkCeltDeemphasis::Apply(const float32 *in, float32 *out, int32 N, float32 coef, float32 *mem) {
			float32 m = mem ? *mem : 0.0f;
			for (int32 j = 0; j < N; ++j) {
				const float32 tmp = in[j] + m;
				m = coef * tmp;
				out[j] = tmp;
			}
			if (mem)
				*mem = m;
		}

		void NkCeltDeemphasis::Preemph(const float32 *in, float32 *out, int32 N, float32 coef, float32 *mem) {
			float32 prev = mem ? *mem : 0.0f;
			for (int32 j = 0; j < N; ++j) {
				const float32 x = in[j];
				out[j] = x - coef * prev;
				prev = x;
			}
			if (mem)
				*mem = prev;
		}

		bool NkCeltDeemphasis::SelfTest() {
			bool ok = true;
			const float32 coef = kPreemphCoef48k;
			const int32 N = 256;

			// Signal déterministe.
			float32 s[256], p[256], r[256];
			for (int32 i = 0; i < N; ++i)
				s[i] = 0.5f * (float32)((i * 13) % 17 - 8);

			// Aller-retour : préemphase → deemphasis = identité.
			float32 memP = 0.0f, memD = 0.0f;
			NkCeltDeemphasis::Preemph(s, p, N, coef, &memP);
			NkCeltDeemphasis::Apply(p, r, N, coef, &memD);
			for (int32 i = 0; i < N; ++i) {
				const float32 d = r[i] - s[i];
				if (d > 1e-3f || d < -1e-3f)
					ok = false;
			}

			// Réponse impulsionnelle du deemphasis : décroissance géométrique coef^n.
			{
				float32 imp[64], out[64];
				for (int32 i = 0; i < 64; ++i)
					imp[i] = 0.0f;
				imp[0] = 1.0f;
				float32 mem = 0.0f;
				NkCeltDeemphasis::Apply(imp, out, 64, coef, &mem);
				float32 expected = 1.0f;
				for (int32 i = 0; i < 10; ++i) {
					if (out[i] < expected - 1e-3f || out[i] > expected + 1e-3f)
						ok = false;
					expected *= coef;
				}
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
