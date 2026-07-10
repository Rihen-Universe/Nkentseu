// =============================================================================
// NKSpeech/NkAudioFeatures.cpp — MFCC / log-Mel from-scratch (voir .h).
// Pré-emphase → trames Hann → FFT radix-2 → spectre de puissance → banc de filtres
// Mel → log → DCT-II = MFCC (+ deltas). Zero-STL, namespace nkentseu::ai.
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKMemory/NKMemory.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

		using math::NkCos;
		using math::NkFloor;
		using math::NkLog10;
		using math::NkPow;
		using math::NkSin;
		using math::NkSqrt;

		namespace {

			constexpr float32 kPi = 3.14159265358979323846f;

			int32 NextPow2(int32 v) {
				int32 p = 1;
				while (p < v)
					p <<= 1;
				return p;
			}

			// FFT radix-2 en place (dir=-1 forward). n = puissance de 2.
			void Fft(float32 *re, float32 *im, int32 n) {
				for (int32 i = 1, j = 0; i < n; ++i) {
					int32 bit = n >> 1;
					for (; j & bit; bit >>= 1)
						j ^= bit;
					j ^= bit;
					if (i < j) {
						float32 tr = re[i];
						re[i] = re[j];
						re[j] = tr;
						float32 ti = im[i];
						im[i] = im[j];
						im[j] = ti;
					}
				}
				for (int32 len = 2; len <= n; len <<= 1) {
					const float32 ang = -2.0f * kPi / static_cast<float32>(len);
					const float32 wr = NkCos(ang);
					const float32 wi = NkSin(ang);
					for (int32 i = 0; i < n; i += len) {
						float32 cwr = 1.0f, cwi = 0.0f;
						const int32 half = len >> 1;
						for (int32 k = 0; k < half; ++k) {
							const int32 a = i + k, b = i + k + half;
							const float32 xr = re[b] * cwr - im[b] * cwi;
							const float32 xi = re[b] * cwi + im[b] * cwr;
							re[b] = re[a] - xr;
							im[b] = im[a] - xi;
							re[a] += xr;
							im[a] += xi;
							const float32 ncwr = cwr * wr - cwi * wi;
							cwi = cwr * wi + cwi * wr;
							cwr = ncwr;
						}
					}
				}
			}

			float32 HzToMel(float32 hz) {
				return 2595.0f * NkLog10(1.0f + hz / 700.0f);
			}
			float32 MelToHz(float32 mel) {
				return 700.0f * (NkPow(10.0f, mel / 2595.0f) - 1.0f);
			}

		} // namespace

		NkFeatureMatrix NkAudioFeatures::LogMelSpectrogram(const float32 *mono, int32 samples,
														   const NkAudioFeatureConfig &cfg) {
			NkFeatureMatrix out;
			if (mono == nullptr || samples <= 0 || cfg.sampleRate <= 0)
				return out;

			const int32 frameLen = cfg.frameSizeMs * cfg.sampleRate / 1000;
			const int32 frameStep = cfg.frameStepMs * cfg.sampleRate / 1000;
			if (frameLen <= 0 || frameStep <= 0)
				return out;
			const int32 N = NextPow2(cfg.fftSize > frameLen ? cfg.fftSize : frameLen);
			const int32 half = N / 2 + 1;
			const int32 mel = cfg.melBands;
			if (samples < frameLen)
				return out;
			const int32 frames = 1 + (samples - frameLen) / frameStep;

			// Fenêtre de Hann.
			float32 *win = (float32 *)memory::NkAlloc((size_t)frameLen * sizeof(float32));
			for (int32 i = 0; i < frameLen; ++i)
				win[i] = 0.5f * (1.0f - NkCos(2.0f * kPi * (float32)i / (float32)(frameLen - 1)));

			// Banc de filtres Mel (mel+2 points → mel triangles) en bins FFT.
			float32 *melHz = (float32 *)memory::NkAlloc((size_t)(mel + 2) * sizeof(float32));
			const float32 mlo = HzToMel(cfg.fMin);
			const float32 mhi = HzToMel(cfg.fMax < (float32)cfg.sampleRate * 0.5f ? cfg.fMax
																				   : (float32)cfg.sampleRate * 0.5f);
			int32 *melBin = (int32 *)memory::NkAlloc((size_t)(mel + 2) * sizeof(int32));
			for (int32 m = 0; m < mel + 2; ++m) {
				const float32 mm = mlo + (mhi - mlo) * (float32)m / (float32)(mel + 1);
				melHz[m] = MelToHz(mm);
				int32 b = (int32)NkFloor((float32)(N + 1) * melHz[m] / (float32)cfg.sampleRate);
				if (b < 0)
					b = 0;
				if (b > half - 1)
					b = half - 1;
				melBin[m] = b;
			}

			out.frames = frames;
			out.dims = mel;
			out.data.Resize((uint64)frames * (uint64)mel);

			float32 *re = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
			float32 *im = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
			float32 *power = (float32 *)memory::NkAlloc((size_t)half * sizeof(float32));

			for (int32 f = 0; f < frames; ++f) {
				const int32 off = f * frameStep;
				// Pré-emphase + fenêtrage (zero-pad jusqu'à N).
				for (int32 i = 0; i < N; ++i) {
					float32 s = 0.0f;
					if (i < frameLen) {
						const float32 cur = mono[off + i];
						const float32 prev = (off + i - 1 >= 0) ? mono[off + i - 1] : 0.0f;
						s = (cur - cfg.preEmphasis * prev) * win[i];
					}
					re[i] = s;
					im[i] = 0.0f;
				}
				Fft(re, im, N);
				for (int32 k = 0; k < half; ++k)
					power[k] = (re[k] * re[k] + im[k] * im[k]) / (float32)N;

				// Filtres Mel triangulaires → log.
				for (int32 m = 0; m < mel; ++m) {
					const int32 b0 = melBin[m];
					const int32 b1 = melBin[m + 1];
					const int32 b2 = melBin[m + 2];
					float32 acc = 0.0f;
					for (int32 k = b0; k <= b2 && k < half; ++k) {
						float32 w = 0.0f;
						if (k >= b0 && k <= b1 && b1 > b0)
							w = (float32)(k - b0) / (float32)(b1 - b0);
						else if (k > b1 && k <= b2 && b2 > b1)
							w = (float32)(b2 - k) / (float32)(b2 - b1);
						acc += power[k] * w;
					}
					out.data[(uint64)f * (uint64)mel + (uint64)m] = NkLog10(acc + 1e-10f);
				}
			}

			memory::NkFree(win);
			memory::NkFree(melHz);
			memory::NkFree(melBin);
			memory::NkFree(re);
			memory::NkFree(im);
			memory::NkFree(power);
			return out;
		}

		NkFeatureMatrix NkAudioFeatures::MFCC(const float32 *mono, int32 samples, const NkAudioFeatureConfig &cfg) {
			NkFeatureMatrix logmel = LogMelSpectrogram(mono, samples, cfg);
			NkFeatureMatrix out;
			if (logmel.frames <= 0 || logmel.dims <= 0)
				return out;
			const int32 frames = logmel.frames;
			const int32 mel = logmel.dims;
			const int32 nc = cfg.mfccCount;
			const int32 baseDims = nc;

			// DCT-II du log-mel → MFCC de base.
			NkVector<float32> base;
			base.Resize((uint64)frames * (uint64)baseDims);
			for (int32 f = 0; f < frames; ++f) {
				for (int32 i = 0; i < nc; ++i) {
					float32 acc = 0.0f;
					for (int32 m = 0; m < mel; ++m) {
						const float32 lm = logmel.data[(uint64)f * (uint64)mel + (uint64)m];
						acc += lm * NkCos(kPi * (float32)i * ((float32)m + 0.5f) / (float32)mel);
					}
					base[(uint64)f * (uint64)baseDims + (uint64)i] = acc;
				}
			}

			if (!cfg.useDeltas) {
				out.frames = frames;
				out.dims = baseDims;
				out.data = base;
				return out;
			}

			// Deltas + delta-deltas (dérivée temporelle simple, bords clampés).
			const int32 dims = baseDims * 3;
			out.frames = frames;
			out.dims = dims;
			out.data.Resize((uint64)frames * (uint64)dims);
			auto baseAt = [&](int32 f, int32 i) -> float32 {
				int32 ff = f < 0 ? 0 : (f > frames - 1 ? frames - 1 : f);
				return base[(uint64)ff * (uint64)baseDims + (uint64)i];
			};
			// d'abord copie base + delta.
			NkVector<float32> delta;
			delta.Resize((uint64)frames * (uint64)baseDims);
			for (int32 f = 0; f < frames; ++f)
				for (int32 i = 0; i < baseDims; ++i)
					delta[(uint64)f * (uint64)baseDims + (uint64)i] = 0.5f * (baseAt(f + 1, i) - baseAt(f - 1, i));
			auto deltaAt = [&](int32 f, int32 i) -> float32 {
				int32 ff = f < 0 ? 0 : (f > frames - 1 ? frames - 1 : f);
				return delta[(uint64)ff * (uint64)baseDims + (uint64)i];
			};
			for (int32 f = 0; f < frames; ++f) {
				for (int32 i = 0; i < baseDims; ++i) {
					const float32 b = base[(uint64)f * (uint64)baseDims + (uint64)i];
					const float32 d = delta[(uint64)f * (uint64)baseDims + (uint64)i];
					const float32 dd = 0.5f * (deltaAt(f + 1, i) - deltaAt(f - 1, i));
					out.data[(uint64)f * (uint64)dims + (uint64)i] = b;
					out.data[(uint64)f * (uint64)dims + (uint64)(baseDims + i)] = d;
					out.data[(uint64)f * (uint64)dims + (uint64)(2 * baseDims + i)] = dd;
				}
			}
			return out;
		}

		bool NkAudioFeatures::SelfTest() {
			bool ok = true;

			NkAudioFeatureConfig cfg;
			cfg.sampleRate = 16000;
			cfg.melBands = 40;
			cfg.mfccCount = 13;
			cfg.useDeltas = true;

			// Sinus pur à 1000 Hz, 0.5 s.
			const int32 sr = cfg.sampleRate;
			const int32 samples = sr / 2;
			const float32 freq = 1000.0f;
			NkVector<float32> sig;
			sig.Resize((uint64)samples);
			for (int32 i = 0; i < samples; ++i)
				sig[(uint64)i] = 0.6f * NkSin(2.0f * kPi * freq * (float32)i / (float32)sr);

			// (1) log-mel : le canal Mel dominant doit correspondre à ~1000 Hz.
			NkFeatureMatrix lm = NkAudioFeatures::LogMelSpectrogram(sig.Data(), samples, cfg);
			if (lm.frames <= 0 || lm.dims != cfg.melBands)
				ok = false;
			if (ok) {
				// Énergie moyenne par bande.
				NkVector<float32> avg;
				avg.Resize((uint64)lm.dims);
				for (int32 m = 0; m < lm.dims; ++m) {
					float32 a = 0.0f;
					for (int32 f = 0; f < lm.frames; ++f)
						a += lm.data[(uint64)f * (uint64)lm.dims + (uint64)m];
					avg[(uint64)m] = a / (float32)lm.frames;
				}
				int32 argmax = 0;
				for (int32 m = 1; m < lm.dims; ++m)
					if (avg[(uint64)m] > avg[(uint64)argmax])
						argmax = m;
				// Centre Hz de la bande argmax (via l'échelle Mel).
				const float32 mlo = HzToMel(cfg.fMin);
				const float32 mhi = HzToMel((float32)sr * 0.5f);
				const float32 mm = mlo + (mhi - mlo) * (float32)(argmax + 1) / (float32)(cfg.melBands + 1);
				const float32 centerHz = MelToHz(mm);
				// Doit être « proche » de 1000 Hz (tolérance large : largeur des filtres Mel).
				const float32 rel = (centerHz - 1000.0f) / 1000.0f;
				if (rel < -0.5f || rel > 0.5f)
					ok = false;
			}

			// (2) MFCC : dimensions correctes (13*3 avec deltas) + déterministe.
			NkFeatureMatrix a = NkAudioFeatures::MFCC(sig.Data(), samples, cfg);
			if (a.frames <= 0 || a.dims != cfg.mfccCount * 3)
				ok = false;
			NkFeatureMatrix b = NkAudioFeatures::MFCC(sig.Data(), samples, cfg);
			if (a.frames != b.frames || a.dims != b.dims)
				ok = false;
			if (ok) {
				for (uint64 i = 0; i < a.data.Size(); ++i) {
					const float32 d = a.data[i] - b.data[i];
					if (d > 1e-6f || d < -1e-6f) {
						ok = false;
						break;
					}
				}
			}

			// (3) silence → MFCC finis (pas de NaN/inf grossier).
			{
				NkVector<float32> sil;
				sil.Resize(4096);
				for (int32 i = 0; i < 4096; ++i)
					sil[(uint64)i] = 0.0f;
				NkFeatureMatrix s = NkAudioFeatures::MFCC(sil.Data(), 4096, cfg);
				if (s.frames <= 0)
					ok = false;
				for (uint64 i = 0; i < s.data.Size(); ++i) {
					const float32 v = s.data[i];
					if (!(v == v) || v > 1e6f || v < -1e6f) { // NaN / explosion
						ok = false;
						break;
					}
				}
			}

			return ok;
		}

	} // namespace ai
} // namespace nkentseu
