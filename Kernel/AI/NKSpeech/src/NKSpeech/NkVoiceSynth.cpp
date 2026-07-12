// =============================================================================
// NKSpeech/NkVoiceSynth.cpp — synthèse vocale par formants (source-filtre).
// =============================================================================
#include "NKSpeech/NkVoiceSynth.h"
#include "NKSpeech/NkGriffinLim.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

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
			// Enveloppe de formants : somme de résonances (Lorentziennes) à F1/F2/F3.
			float32 FormantEnvelope(float32 freq, const NkPhone &p) {
				const float32 bw = 90.0f; // largeur de bande (Hz)
				float32 e = 0.0f;
				const float32 fs[3] = {p.f1, p.f2, p.f3};
				const float32 amp[3] = {1.0f, 0.7f, 0.35f}; // formants hauts plus faibles
				for (int i = 0; i < 3; ++i) {
					if (fs[i] <= 0.0f)
						continue;
					const float32 d = (freq - fs[i]) / bw;
					e += amp[i] / (1.0f + d * d);
				}
				// Pente spectrale douce de la source glottale (−6 dB/octave approx).
				const float32 tilt = 1.0f / (1.0f + freq / 1000.0f);
				return e * tilt;
			}

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
			const int32 sr = cfg.sampleRate, N = cfg.fftSize, hop = cfg.hopSize, bins = N / 2 + 1;
			const float32 binHz = (float32)sr / (float32)N;

			// Durée totale → nombre de trames.
			float32 totalMs = 0.0f;
			for (uint32 i = 0; i < phones.Size(); ++i)
				totalMs += phones[i].durationMs;
			const int32 totalSamples = (int32)(totalMs * 0.001f * (float32)sr);
			if (totalSamples < N)
				return out;
			const int32 frames = 1 + (totalSamples - N) / hop;

			NkMagSpectrogram mag;
			mag.frames = frames;
			mag.bins = bins;
			mag.data.Resize((nk_size)frames * (nk_size)bins);

			uint32 rng = 0x1234abcdu;
			for (int32 f = 0; f < frames; ++f) {
				// Phone courant au centre de la trame.
				const float32 tMs = ((float32)(f * hop + N / 2) / (float32)sr) * 1000.0f;
				float32 acc = 0.0f;
				const NkPhone *cur = &phones[phones.Size() - 1];
				for (uint32 i = 0; i < phones.Size(); ++i) {
					acc += phones[i].durationMs;
					if (tMs < acc) {
						cur = &phones[i];
						break;
					}
				}
				float32 *row = mag.data.Data() + (nk_size)f * (nk_size)bins;
				for (int32 k = 0; k < bins; ++k)
					row[k] = 0.0f;
				if (cur->gain <= 0.0f)
					continue;

				if (cur->voiced) {
					// Source = peigne d'harmoniques de F0, mis en forme par l'enveloppe de formants.
					for (int32 h = 1;; ++h) {
						const float32 freq = (float32)h * cfg.f0;
						if (freq >= (float32)sr * 0.5f)
							break;
						const int32 k = (int32)(freq / binHz + 0.5f);
						if (k >= 0 && k < bins)
							row[k] += cur->gain * FormantEnvelope(freq, *cur);
					}
				} else {
					// Non-voisé : bruit large bande mis en forme par l'enveloppe (fricative).
					for (int32 k = 1; k < bins; ++k) {
						const float32 freq = (float32)k * binHz;
						const float32 nz = 0.5f * (Lcg(rng) + 1.0f); // magnitude ≥ 0
						row[k] = cur->gain * nz * (0.3f + FormantEnvelope(freq, *cur));
					}
				}
			}

			// Vocodeur : magnitude → onde (Griffin-Lim).
			NkGriffinLimConfig gl;
			gl.fftSize = N;
			gl.hopSize = hop;
			gl.iterations = cfg.glIterations;
			out = NkGriffinLim::Reconstruct(mag, gl);

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
