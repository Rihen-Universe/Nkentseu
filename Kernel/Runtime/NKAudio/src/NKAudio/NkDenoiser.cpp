// =============================================================================
// NKAudio/NkDenoiser.cpp — débruitage + normalisation offline (voir .h).
// FFT radix-2 iterative maison ; STFT Hann 50% overlap-add ; soustraction spectrale.
// =============================================================================
#include "NKAudio/NkDenoiser.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // convention NKAudio : math C directe (cf. NkAudioAnalyzer)

namespace nkentseu {
	namespace audio {

		// Wrappers math (mêmes noms que NKMath pour la lisibilité, impl. C).
		static inline float32 NkSin(float32 x) {
			return ::sinf(x);
		}
		static inline float32 NkCos(float32 x) {
			return ::cosf(x);
		}
		static inline float32 NkSqrt(float32 x) {
			return ::sqrtf(x);
		}
		static inline float32 NkPow(float32 x, float32 y) {
			return ::powf(x, y);
		}
		static inline float32 NkAbs(float32 x) {
			return x < 0.0f ? -x : x;
		}

		namespace {

			constexpr float32 kPi = 3.14159265358979323846f;

			float32 DbToLin(float32 db) {
				return NkPow(10.0f, db / 20.0f);
			}

			// FFT radix-2 en place (Cooley-Tukey, decimation-in-time). n = puissance de 2.
			// dir = -1 (forward), +1 (inverse, non normalisé).
			void Fft(float32 *re, float32 *im, int32 n, int32 dir) {
				// Permutation bit-reverse.
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
				// Papillons.
				for (int32 len = 2; len <= n; len <<= 1) {
					const float32 ang = (float32)dir * 2.0f * kPi / (float32)len;
					const float32 wr = NkCos(ang);
					const float32 wi = NkSin(ang);
					for (int32 i = 0; i < n; i += len) {
						float32 cwr = 1.0f;
						float32 cwi = 0.0f;
						const int32 half = len >> 1;
						for (int32 k = 0; k < half; ++k) {
							const int32 a = i + k;
							const int32 b = i + k + half;
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

			// Passe-haut simple (DC blocker 1er ordre) : y[n] = x[n] - x[n-1] + R*y[n-1].
			void HighPass(float32 *x, int32 n, int32 sampleRate, float32 cutoffHz) {
				if (n <= 0 || sampleRate <= 0)
					return;
				const float32 R = 1.0f - (2.0f * kPi * cutoffHz / (float32)sampleRate);
				float32 clampR = R < 0.0f ? 0.0f : (R > 0.9999f ? 0.9999f : R);
				float32 prevX = 0.0f;
				float32 prevY = 0.0f;
				for (int32 i = 0; i < n; ++i) {
					const float32 in = x[i];
					const float32 y = in - prevX + clampR * prevY;
					prevX = in;
					prevY = y;
					x[i] = y;
				}
			}

			// Débruitage spectral STFT (Hann, 50% overlap-add) par GAIN DE WIENER LISSÉ.
			// Profil de bruit (puissance) estimé sur les premières trames. Le gain par bin est
			// lissé EN FRÉQUENCE (3 taps) et EN TEMPS (récursif) → trames cohérentes → pas de
			// « musical noise » ni d'amplification du plancher (cf. soustraction naïve).
			void SpectralSubtract(float32 *x, int32 n, int32 sampleRate, const NkDenoiseOptions &opt) {
				const int32 N = 1024;	 // taille de trame (puissance de 2)
				const int32 hop = N / 2; // 50% overlap
				if (n < N)
					return;

				// Fenêtre de Hann.
				float32 *win = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
				for (int32 i = 0; i < N; ++i)
					win[i] = 0.5f * (1.0f - NkCos(2.0f * kPi * (float32)i / (float32)(N - 1)));

				const int32 nFrames = 1 + (n - N) / hop;
				const int32 half = N / 2 + 1;

				// Profil de bruit en PUISSANCE (|X|²) moyen sur les premières trames.
				float32 *noisePow = (float32 *)memory::NkAlloc((size_t)half * sizeof(float32));
				for (int32 k = 0; k < half; ++k)
					noisePow[k] = 0.0f;
				const int32 noiseFrames0 = (int32)((opt.noiseMs / 1000.0f) * (float32)sampleRate / (float32)hop);
				int32 noiseFrames = noiseFrames0 < 1 ? 1 : (noiseFrames0 > nFrames ? nFrames : noiseFrames0);

				float32 *re = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
				float32 *im = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));

				// Passe 1 : estimer la puissance du bruit.
				for (int32 f = 0; f < noiseFrames; ++f) {
					const int32 off = f * hop;
					for (int32 i = 0; i < N; ++i) {
						re[i] = x[off + i] * win[i];
						im[i] = 0.0f;
					}
					Fft(re, im, N, -1);
					for (int32 k = 0; k < half; ++k)
						noisePow[k] += re[k] * re[k] + im[k] * im[k];
				}
				for (int32 k = 0; k < half; ++k)
					noisePow[k] /= (float32)noiseFrames;

				// Sortie overlap-add + normalisation de fenêtre (somme des win²).
				float32 *acc = (float32 *)memory::NkAlloc((size_t)n * sizeof(float32));
				float32 *wsum = (float32 *)memory::NkAlloc((size_t)n * sizeof(float32));
				for (int32 i = 0; i < n; ++i) {
					acc[i] = 0.0f;
					wsum[i] = 0.0f;
				}

				// Gains lissés en temps (état entre trames) + tampon de gain brut/frequence.
				float32 *prevGain = (float32 *)memory::NkAlloc((size_t)half * sizeof(float32));
				float32 *rawGain = (float32 *)memory::NkAlloc((size_t)half * sizeof(float32));
				for (int32 k = 0; k < half; ++k)
					prevGain[k] = 1.0f;

				const float32 gFloor = opt.spectralFloor;			// plancher de gain (0..1)
				const float32 gFloor2 = gFloor * gFloor;
				const float32 alpha = opt.overSubtraction;			// sur-soustraction (puissance)
				const float32 tSmooth = 0.5f;						// lissage temporel (0=aucun)

				// Passe 2 : gain + reconstruction.
				for (int32 f = 0; f < nFrames; ++f) {
					const int32 off = f * hop;
					for (int32 i = 0; i < N; ++i) {
						re[i] = x[off + i] * win[i];
						im[i] = 0.0f;
					}
					Fft(re, im, N, -1);

					// Gain de Wiener (puissance) par bin, borné par le plancher.
					for (int32 k = 0; k < half; ++k) {
						const float32 p = re[k] * re[k] + im[k] * im[k];
						float32 g2 = (p > 1e-12f) ? (p - alpha * noisePow[k]) / p : gFloor2;
						if (g2 < gFloor2)
							g2 = gFloor2;
						if (g2 > 1.0f)
							g2 = 1.0f;
						rawGain[k] = NkSqrt(g2);
					}
					// Lissage EN FRÉQUENCE (3 taps) puis EN TEMPS (récursif).
					for (int32 k = 0; k < half; ++k) {
						const float32 gm = rawGain[k > 0 ? k - 1 : 0];
						const float32 gc = rawGain[k];
						const float32 gp = rawGain[k < half - 1 ? k + 1 : half - 1];
						float32 g = (gm + gc + gp) * (1.0f / 3.0f);
						g = tSmooth * prevGain[k] + (1.0f - tSmooth) * g;
						prevGain[k] = g;
						re[k] *= g;
						im[k] *= g;
						if (k > 0 && k < N / 2) {
							re[N - k] = re[k];
							im[N - k] = -im[k];
						}
					}

					Fft(re, im, N, +1);
					const float32 invN = 1.0f / (float32)N;
					for (int32 i = 0; i < N; ++i) {
						acc[off + i] += re[i] * invN * win[i];
						wsum[off + i] += win[i] * win[i];
					}
				}

				for (int32 i = 0; i < n; ++i)
					x[i] = (wsum[i] > 1e-6f) ? (acc[i] / wsum[i]) : x[i];

				memory::NkFree(win);
				memory::NkFree(noisePow);
				memory::NkFree(re);
				memory::NkFree(im);
				memory::NkFree(acc);
				memory::NkFree(wsum);
				memory::NkFree(prevGain);
				memory::NkFree(rawGain);
			}

			// Noise gate à enveloppe (attack/release) : atténue sous le seuil.
			void NoiseGate(float32 *x, int32 n, int32 sampleRate, const NkDenoiseOptions &opt) {
				const float32 thresh = DbToLin(opt.gateThreshDb);
				const float32 floorGain = DbToLin(opt.gateFloorDb);
				const float32 atkCoef = NkPow(0.01f, 1.0f / (opt.attackMs * 0.001f * (float32)sampleRate + 1.0f));
				const float32 relCoef = NkPow(0.01f, 1.0f / (opt.releaseMs * 0.001f * (float32)sampleRate + 1.0f));
				float32 env = 0.0f;
				float32 gain = 1.0f;
				for (int32 i = 0; i < n; ++i) {
					const float32 a = NkAbs(x[i]);
					// Enveloppe (suiveur de crête).
					if (a > env)
						env = a;
					else
						env = env * 0.999f;
					const float32 target = (env >= thresh) ? 1.0f : floorGain;
					const float32 c = (target < gain) ? atkCoef : relCoef;
					gain = c * gain + (1.0f - c) * target;
					x[i] *= gain;
				}
			}

			// Normalisation crête vers targetPeakDb (limitée par maxGainDb).
			void Normalize(float32 *x, int32 n, const NkDenoiseOptions &opt) {
				float32 peak = 0.0f;
				for (int32 i = 0; i < n; ++i) {
					const float32 a = NkAbs(x[i]);
					if (a > peak)
						peak = a;
				}
				if (peak < 1e-6f)
					return;
				const float32 target = DbToLin(opt.targetPeakDb);
				float32 gain = target / peak;
				const float32 maxGain = DbToLin(opt.maxGainDb);
				if (gain > maxGain)
					gain = maxGain;
				for (int32 i = 0; i < n; ++i) {
					float32 v = x[i] * gain;
					v = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); // sécurité clip
					x[i] = v;
				}
			}

		} // namespace

		bool NkDenoiser::ProcessMono(const float32 *in, int32 frames, int32 sampleRate, NkVector<float32> &out,
									 const NkDenoiseOptions &opt) {
			if (in == nullptr || frames <= 0 || sampleRate <= 0)
				return false;
			out.Resize((uint64)frames);
			for (int32 i = 0; i < frames; ++i)
				out[(uint64)i] = in[i];
			float32 *x = out.Data();

			if (opt.highPass)
				HighPass(x, frames, sampleRate, opt.highPassHz);
			if (opt.spectral)
				SpectralSubtract(x, frames, sampleRate, opt);
			if (opt.gate)
				NoiseGate(x, frames, sampleRate, opt);
			if (opt.normalize)
				Normalize(x, frames, opt);
			return true;
		}

		bool NkDenoiser::Process(const float32 *interleaved, int32 frames, int32 channels, int32 sampleRate,
								 NkVector<float32> &out, const NkDenoiseOptions &opt) {
			if (interleaved == nullptr || frames <= 0 || channels <= 0 || sampleRate <= 0)
				return false;
			if (channels == 1)
				return ProcessMono(interleaved, frames, sampleRate, out, opt);

			out.Resize((uint64)frames * (uint64)channels);
			NkVector<float32> chan;
			chan.Resize((uint64)frames);
			NkVector<float32> clean;
			for (int32 c = 0; c < channels; ++c) {
				for (int32 i = 0; i < frames; ++i)
					chan[(uint64)i] = interleaved[i * channels + c];
				if (!ProcessMono(chan.Data(), frames, sampleRate, clean, opt))
					return false;
				for (int32 i = 0; i < frames; ++i)
					out[(uint64)(i * channels + c)] = clean[(uint64)i];
			}
			return true;
		}

		bool NkDenoiser::SelfTest() {
			bool ok = true;

			const int32 sr = 16000;
			const int32 frames = sr; // 1 s
			NkVector<float32> sig;
			sig.Resize((uint64)frames);

			// PRNG déterministe (xorshift) pour du bruit reproductible.
			uint32 st = 0x12345678u;
			auto rnd = [&st]() -> float32 {
				st ^= st << 13;
				st ^= st >> 17;
				st ^= st << 5;
				return ((float32)(st & 0xFFFFFF) / (float32)0xFFFFFF) * 2.0f - 1.0f;
			};

			// 0..0.4s : bruit seul (profil). 0.4..0.6s : sinus 440Hz + bruit. 0.6..1s : bruit seul
			// (grand silence : on mesure la queue 0.85..1s, loin de la frontière du sinus pour éviter
			// le « smearing » transitoire de la soustraction spectrale, ~1 trame = 64 ms).
			const float32 noiseAmp = 0.08f;
			for (int32 i = 0; i < frames; ++i) {
				const float32 t = (float32)i / (float32)sr;
				float32 s = noiseAmp * rnd();
				if (t >= 0.4f && t < 0.6f)
					s += 0.5f * NkSin(2.0f * kPi * 440.0f * t);
				sig[(uint64)i] = s;
			}

			// RMS d'une région [a,b).
			auto rms = [](const float32 *x, int32 a, int32 b) -> float32 {
				float32 acc = 0.0f;
				for (int32 i = a; i < b; ++i)
					acc += x[i] * x[i];
				const int32 n = b - a;
				return n > 0 ? NkSqrt(acc / (float32)n) : 0.0f;
			};

			const float32 tailInBefore = rms(sig.Data(), (int32)(0.85f * sr), frames);
			const float32 sigInBefore = rms(sig.Data(), (int32)(0.45f * sr), (int32)(0.55f * sr));

			NkDenoiseOptions opt;
			opt.normalize = false; // pour comparer les niveaux bruts (la normalisation fausserait le ratio)
			opt.highPass = false;
			opt.gate = false;
			NkVector<float32> outv;
			if (!NkDenoiser::ProcessMono(sig.Data(), frames, sr, outv, opt))
				return false;

			const float32 tailOut = rms(outv.Data(), (int32)(0.85f * sr), frames);
			const float32 sigOut = rms(outv.Data(), (int32)(0.45f * sr), (int32)(0.55f * sr));

			// Le plancher de bruit (queue silencieuse) doit baisser nettement.
			if (!(tailOut < tailInBefore * 0.7f))
				ok = false;
			// Le sinus doit survivre (garder l'essentiel de son énergie).
			if (!(sigOut > sigInBefore * 0.5f))
				ok = false;

			// Test normalisation : un signal faible remonte au niveau cible.
			{
				NkVector<float32> quiet;
				quiet.Resize(2048);
				for (int32 i = 0; i < 2048; ++i)
					quiet[(uint64)i] = 0.02f * NkSin(2.0f * kPi * 300.0f * (float32)i / (float32)sr);
				NkDenoiseOptions o2;
				o2.spectral = false;
				o2.gate = false;
				o2.highPass = false;
				o2.normalize = true;
				o2.targetPeakDb = -1.0f;
				NkVector<float32> qn;
				NkDenoiser::ProcessMono(quiet.Data(), 2048, sr, qn, o2);
				float32 peak = 0.0f;
				for (int32 i = 0; i < 2048; ++i) {
					const float32 a = NkAbs(qn[(uint64)i]);
					if (a > peak)
						peak = a;
				}
				if (!(peak > 0.5f)) // 0.02 crête → doit remonter bien au-dessus
					ok = false;
			}

			return ok;
		}

	} // namespace audio
} // namespace nkentseu
