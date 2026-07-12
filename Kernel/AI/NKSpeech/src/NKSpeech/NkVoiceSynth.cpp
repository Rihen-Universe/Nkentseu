// =============================================================================
// NKSpeech/NkVoiceSynth.cpp — synthèse vocale par formants (source-filtre).
// =============================================================================
#include "NKSpeech/NkVoiceSynth.h"
#include "NKSpeech/NkGriffinLim.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

		using math::NkCos;
		using math::NkExp;
		using math::NkSqrt;

		NkPhone NkVoiceSynth::Vowel(char v, float32 durationMs) {
			NkPhone p;
			p.voiced = true;
			p.durationMs = durationMs;
			// Formants approximés (voix d'homme). Sources : tables de phonétique acoustique.
			switch (v) {
				case 'a':
					p.f1 = 800.0f;
					p.f2 = 1200.0f;
					p.f3 = 2500.0f;
					break;
				case 'e':
					p.f1 = 500.0f;
					p.f2 = 1800.0f;
					p.f3 = 2500.0f;
					break;
				case 'i':
					p.f1 = 300.0f;
					p.f2 = 2300.0f;
					p.f3 = 3000.0f;
					break;
				case 'o':
					p.f1 = 500.0f;
					p.f2 = 900.0f;
					p.f3 = 2500.0f;
					break;
				case 'u':
					p.f1 = 320.0f;
					p.f2 = 800.0f;
					p.f3 = 2500.0f;
					break;
				default: // silence
					p.f1 = p.f2 = p.f3 = 0.0f;
					p.gain = 0.0f;
					break;
			}
			return p;
		}

		namespace {
			// Résonateur numérique de formant (Klatt) : y[n] = a·x[n] + b·y[n-1] + c·y[n-2].
			// Résonance à la fréquence F (Hz), largeur de bande BW (Hz). Gain unité en continu.
			struct Resonator {
					float32 a = 1.0f, b = 0.0f, c = 0.0f;
					float32 y1 = 0.0f, y2 = 0.0f;
					void Set(float32 F, float32 BW, int32 sr) {
						const float32 kPi = 3.14159265358979323846f;
						const float32 r = NkExp(-kPi * BW / (float32)sr);
						c = -r * r;
						b = 2.0f * r * NkCos(2.0f * kPi * F / (float32)sr);
						a = 1.0f - b - c;
					}
					float32 Step(float32 x) {
						const float32 y = a * x + b * y1 + c * y2;
						y2 = y1;
						y1 = y;
						return y;
					}
			};

			// LCG déterministe pour le bruit des non-voisés.
			inline float32 Lcg(uint32 &s) {
				s = s * 1664525u + 1013904223u;
				return (float32)((s >> 8) & 0xFFFFu) / 32767.5f - 1.0f;
			}
		} // namespace

		NkVector<float32> NkVoiceSynth::Synthesize(const NkVector<NkPhone> &phones, const NkVoiceSynthConfig &cfg) {
			NkVector<float32> out;
			if (phones.Size() == 0 || cfg.sampleRate <= 0)
				return out;
			const int32 sr = cfg.sampleRate;

			float32 totalMs = 0.0f;
			for (uint32 i = 0; i < phones.Size(); ++i)
				totalMs += phones[i].durationMs;
			const int32 totalSamples = (int32)(totalMs * 0.001f * (float32)sr);
			if (totalSamples <= 0)
				return out;
			out.Resize((nk_size)totalSamples);

			// SYNTHÈSE TEMPORELLE source-filtre : une SOURCE (train d'impulsions glottiques à F0
			// pour les voisés, bruit pour les non-voisés) excite 3 RÉSONATEURS de formants en
			// parallèle (F1/F2/F3). Le résonateur « sonne » entre deux impulsions → son SOUTENU
			// (une vraie voyelle), pas un grain isolé.
			Resonator r1, r2, r3;
			uint32 rng = 0x1234abcdu;
			float32 phase = 0.0f; // phase du train d'impulsions (0..1 par période F0)
			int32 idx = 0;
			for (uint32 pi = 0; pi < phones.Size(); ++pi) {
				const NkPhone &p = phones[pi];
				const int32 nS = (int32)(p.durationMs * 0.001f * (float32)sr);
				if (p.f1 > 0.0f)
					r1.Set(p.f1, 80.0f, sr);
				if (p.f2 > 0.0f)
					r2.Set(p.f2, 100.0f, sr);
				if (p.f3 > 0.0f)
					r3.Set(p.f3, 120.0f, sr);
				const float32 step = cfg.f0 / (float32)sr; // incrément de phase par échantillon
				for (int32 n = 0; n < nS && idx < totalSamples; ++n, ++idx) {
					float32 src = 0.0f;
					if (p.gain > 0.0f) {
						if (p.voiced) {
							phase += step;
							if (phase >= 1.0f) { // une impulsion glottique par période F0
								phase -= 1.0f;
								src = 1.0f;
							}
						} else {
							src = 0.3f * Lcg(rng); // fricative : bruit
						}
						src *= p.gain;
					}
					// Formants en parallèle (F1 dominant), enveloppe d'attaque/relâche douce.
					float32 y = r1.Step(src) + 0.7f * r2.Step(src) + 0.3f * r3.Step(src);
					float32 env = 1.0f;
					const int32 fade = nS / 6 > 0 ? nS / 6 : 1;
					if (n < fade)
						env = (float32)n / (float32)fade;
					else if (n > nS - fade)
						env = (float32)(nS - n) / (float32)fade;
					out[(nk_size)idx] = y * env;
				}
			}

			// Normalisation à ~0,9 crête.
			float32 peak = 1e-6f;
			for (uint32 i = 0; i < out.Size(); ++i) {
				const float32 a = out[i] < 0.0f ? -out[i] : out[i];
				if (a > peak)
					peak = a;
			}
			const float32 g = 0.9f / peak;
			for (uint32 i = 0; i < out.Size(); ++i)
				out[i] *= g;
			return out;
		}

		bool NkVoiceSynth::SelfTest() {
			// Synthétise la voyelle 'a' (F1≈800, F2≈1200) puis vérifie que le spectre du signal
			// produit présente bien de l'énergie autour de F1 et F2.
			NkVector<NkPhone> seq;
			seq.PushBack(Vowel('a', 400.0f));
			NkVoiceSynthConfig cfg;
			NkVector<float32> wav = NkVoiceSynth::Synthesize(seq, cfg);
			if ((int32)wav.Size() < cfg.fftSize)
				return false;

			// (0) Son SOUTENU (pas des clics isolés) : une bonne part des échantillons doit
			//     dépasser 10 % de la crête. Un train de clics échouerait ici.
			float32 peak = 1e-6f;
			for (uint32 i = 0; i < wav.Size(); ++i) {
				const float32 a = wav[i] < 0.0f ? -wav[i] : wav[i];
				if (a > peak)
					peak = a;
			}
			int32 loud = 0;
			for (uint32 i = 0; i < wav.Size(); ++i) {
				const float32 a = wav[i] < 0.0f ? -wav[i] : wav[i];
				if (a > 0.1f * peak)
					++loud;
			}
			const double sustained = (double)loud / (double)wav.Size();
			if (sustained < 0.30)
				return false; // signal soutenu attendu (voyelle tenue), pas des impulsions

			NkGriffinLimConfig gl;
			gl.fftSize = cfg.fftSize;
			gl.hopSize = cfg.hopSize;
			NkMagSpectrogram m = NkGriffinLim::Magnitude(wav.Data(), (int32)wav.Size(), gl);
			if (m.frames <= 2)
				return false;
			// Spectre moyen (trames internes).
			NkVector<double> avg;
			avg.Resize((nk_size)m.bins);
			for (int32 k = 0; k < m.bins; ++k)
				avg[(nk_size)k] = 0.0;
			for (int32 f = 2; f < m.frames - 2; ++f)
				for (int32 k = 0; k < m.bins; ++k)
					avg[(nk_size)k] += m.data[(nk_size)f * (nk_size)m.bins + (nk_size)k];

			const float32 binHz = (float32)cfg.sampleRate / (float32)cfg.fftSize;
			auto energyAround = [&](float32 fc, float32 halfBw) {
				const int32 k0 = (int32)((fc - halfBw) / binHz);
				const int32 k1 = (int32)((fc + halfBw) / binHz);
				double s = 0.0;
				for (int32 k = (k0 > 0 ? k0 : 0); k <= k1 && k < m.bins; ++k)
					s += avg[(nk_size)k];
				return s;
			};
			double total = 0.0;
			for (int32 k = 0; k < m.bins; ++k)
				total += avg[(nk_size)k];
			if (total <= 0.0)
				return false;
			const double eF1 = energyAround(800.0f, 150.0f) / total;
			const double eF2 = energyAround(1200.0f, 150.0f) / total;
			// Les deux régions de formants doivent concentrer une part significative de l'énergie.
			return eF1 > 0.05 && eF2 > 0.02;
		}

	} // namespace ai
} // namespace nkentseu
