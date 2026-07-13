// =============================================================================
// NKSpeech/NkGriffinLim.cpp — vocodeur Griffin-Lim (spectrogramme → onde).
// =============================================================================
#include "NKSpeech/NkGriffinLim.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

		using math::NkCos;
		using math::NkSin;
		using math::NkSqrt;

		namespace {
			const float32 kPi = 3.14159265358979323846f;

			// FFT radix-2 en place. inverse=false : e^{-j..} + non normalisée.
			// inverse=true : e^{+j..} + division par n (transformée inverse).
			void Fft(float32 *re, float32 *im, int32 n, bool inverse) {
				for (int32 i = 1, j = 0; i < n; ++i) {
					int32 bit = n >> 1;
					for (; j & bit; bit >>= 1)
						j ^= bit;
					j ^= bit;
					if (i < j) {
						float32 t = re[i];
						re[i] = re[j];
						re[j] = t;
						t = im[i];
						im[i] = im[j];
						im[j] = t;
					}
				}
				for (int32 len = 2; len <= n; len <<= 1) {
					const float32 ang = (inverse ? 2.0f : -2.0f) * kPi / static_cast<float32>(len);
					const float32 wr = NkCos(ang);
					const float32 wi = NkSin(ang);
					const int32 half = len >> 1;
					for (int32 i = 0; i < n; i += len) {
						float32 cwr = 1.0f, cwi = 0.0f;
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
				if (inverse) {
					const float32 inv = 1.0f / static_cast<float32>(n);
					for (int32 i = 0; i < n; ++i) {
						re[i] *= inv;
						im[i] *= inv;
					}
				}
			}

			// Fenêtre de Hann périodique de longueur N.
			void HannWindow(NkVector<float32> &w, int32 N) {
				w.Resize((nk_size)N);
				for (int32 n = 0; n < N; ++n)
					w[(nk_size)n] = 0.5f * (1.0f - NkCos(2.0f * kPi * (float32)n / (float32)N));
			}

			int32 FrameCount(int32 samples, int32 N, int32 hop) {
				if (samples < N)
					return 1;
				return 1 + (samples - N) / hop;
			}
		} // namespace

		NkMagSpectrogram NkGriffinLim::Magnitude(const float32 *mono, int32 samples, const NkGriffinLimConfig &cfg) {
			NkMagSpectrogram out;
			if (!mono || samples <= 0 || cfg.fftSize <= 0 || cfg.hopSize <= 0)
				return out;
			const int32 N = cfg.fftSize, hop = cfg.hopSize, bins = N / 2 + 1;
			const int32 frames = FrameCount(samples, N, hop);
			NkVector<float32> win;
			HannWindow(win, N);
			out.frames = frames;
			out.bins = bins;
			out.data.Resize((nk_size)frames * (nk_size)bins);
			NkVector<float32> re, im;
			re.Resize((nk_size)N);
			im.Resize((nk_size)N);
			for (int32 f = 0; f < frames; ++f) {
				const int32 off = f * hop;
				for (int32 n = 0; n < N; ++n) {
					const int32 idx = off + n;
					const float32 s = (idx < samples) ? mono[idx] : 0.0f;
					re[(nk_size)n] = s * win[(nk_size)n];
					im[(nk_size)n] = 0.0f;
				}
				Fft(re.Data(), im.Data(), N, false);
				for (int32 k = 0; k < bins; ++k) {
					const float32 mr = re[(nk_size)k], mi = im[(nk_size)k];
					out.data[(nk_size)f * (nk_size)bins + (nk_size)k] = NkSqrt(mr * mr + mi * mi);
				}
			}
			return out;
		}

		NkVector<float32> NkGriffinLim::Reconstruct(const NkMagSpectrogram &mag, const NkGriffinLimConfig &cfg) {
			NkVector<float32> outSig;
			if (mag.frames <= 0 || mag.bins <= 0)
				return outSig;
			const int32 N = cfg.fftSize, hop = cfg.hopSize, bins = N / 2 + 1, frames = mag.frames;
			if (mag.bins != bins)
				return outSig;
			const int32 sigLen = (frames - 1) * hop + N;

			NkVector<float32> win;
			HannWindow(win, N);
			// Dénominateur d'overlap-add (Σ des fenêtres² qui recouvrent chaque échantillon).
			NkVector<float32> den;
			den.Resize((nk_size)sigLen);
			for (int32 i = 0; i < sigLen; ++i)
				den[(nk_size)i] = 0.0f;
			for (int32 f = 0; f < frames; ++f)
				for (int32 n = 0; n < N; ++n)
					den[(nk_size)(f * hop + n)] += win[(nk_size)n] * win[(nk_size)n];
			for (int32 i = 0; i < sigLen; ++i)
				if (den[(nk_size)i] < 1e-8f)
					den[(nk_size)i] = 1e-8f;

			// Spectre complet complexe [frames*N] ; phase initiale nulle (magnitude cible, im=0).
			NkVector<float32> re, im;
			re.Resize((nk_size)frames * (nk_size)N);
			im.Resize((nk_size)frames * (nk_size)N);
			for (int32 f = 0; f < frames; ++f) {
				for (int32 k = 0; k < bins; ++k) {
					const float32 m = mag.data[(nk_size)f * (nk_size)bins + (nk_size)k];
					re[(nk_size)f * (nk_size)N + (nk_size)k] = m;
					im[(nk_size)f * (nk_size)N + (nk_size)k] = 0.0f;
				}
				for (int32 k = bins; k < N; ++k) { // symétrie hermitienne
					re[(nk_size)f * (nk_size)N + (nk_size)k] = re[(nk_size)f * (nk_size)N + (nk_size)(N - k)];
					im[(nk_size)f * (nk_size)N + (nk_size)k] = 0.0f;
				}
			}

			outSig.Resize((nk_size)sigLen);
			NkVector<float32> fr, fi; // trame de travail
			fr.Resize((nk_size)N);
			fi.Resize((nk_size)N);

			auto iSTFT = [&]() {
				for (int32 i = 0; i < sigLen; ++i)
					outSig[(nk_size)i] = 0.0f;
				for (int32 f = 0; f < frames; ++f) {
					for (int32 n = 0; n < N; ++n) {
						fr[(nk_size)n] = re[(nk_size)f * (nk_size)N + (nk_size)n];
						fi[(nk_size)n] = im[(nk_size)f * (nk_size)N + (nk_size)n];
					}
					Fft(fr.Data(), fi.Data(), N, true); // inverse -> temps réel dans fr
					for (int32 n = 0; n < N; ++n)
						outSig[(nk_size)(f * hop + n)] += fr[(nk_size)n] * win[(nk_size)n];
				}
				for (int32 i = 0; i < sigLen; ++i)
					outSig[(nk_size)i] /= den[(nk_size)i];
			};

			// Fast Griffin-Lim (FGLA, Perraudin 2013) : a chaque tour on PROJETTE sur la magnitude
			// cible (phase gardee) puis on EXTRAPOLE avec un momentum -> convergence bien plus
			// rapide vers une phase coherente (moins de buzz, moins d'energie parasite).
			NkVector<float32> tRe, tIm; // projection precedente (pour le momentum)
			tRe.Resize((nk_size)frames * (nk_size)N);
			tIm.Resize((nk_size)frames * (nk_size)N);
			for (nk_size i = 0; i < tRe.Size(); ++i) {
				tRe[i] = re[i];
				tIm[i] = im[i];
			}
			const float32 mom = cfg.momentum;
			for (int32 it = 0; it < cfg.iterations; ++it) {
				iSTFT();
				for (int32 f = 0; f < frames; ++f) {
					const int32 off = f * hop;
					for (int32 n = 0; n < N; ++n) {
						fr[(nk_size)n] = outSig[(nk_size)(off + n)] * win[(nk_size)n];
						fi[(nk_size)n] = 0.0f;
					}
					Fft(fr.Data(), fi.Data(), N, false); // STFT
					for (int32 k = 0; k < bins; ++k) {
						const float32 cr = fr[(nk_size)k], ci = fi[(nk_size)k];
						const float32 m = NkSqrt(cr * cr + ci * ci) + 1e-9f;
						const float32 scale = mag.data[(nk_size)f * (nk_size)bins + (nk_size)k] / m;
						const float32 pr = cr * scale, pi = ci * scale; // projection sur la magnitude
						const nk_size idx = (nk_size)f * (nk_size)N + (nk_size)k;
						const float32 sr = pr + mom * (pr - tRe[idx]); // extrapolation momentum
						const float32 si = pi + mom * (pi - tIm[idx]);
						re[idx] = sr;
						im[idx] = si;
						tRe[idx] = pr; // memorise la projection pure pour le prochain tour
						tIm[idx] = pi;
						if (k > 0 && k < N - bins + 1) { // miroir hermitien (conjugue)
							const nk_size ridx = (nk_size)f * (nk_size)N + (nk_size)(N - k);
							re[ridx] = sr;
							im[ridx] = -si;
							tRe[ridx] = pr;
							tIm[ridx] = -pi;
						}
					}
				}
			}
			// Synthese finale depuis la projection PURE (sans overshoot du momentum).
			for (nk_size i = 0; i < re.Size(); ++i) {
				re[i] = tRe[i];
				im[i] = tIm[i];
			}
			iSTFT(); // synthese finale
			return outSig;
		}

		bool NkGriffinLim::SelfTest() {
			// Signal connu : somme de deux sinus (300 Hz + 700 Hz), 0,5 s à 16 kHz.
			const int32 sr = 16000, samples = sr / 2;
			NkVector<float32> sig;
			sig.Resize((nk_size)samples);
			for (int32 i = 0; i < samples; ++i) {
				const float32 t = (float32)i / (float32)sr;
				sig[(nk_size)i] = 0.5f * NkSin(2.0f * kPi * 300.0f * t) + 0.3f * NkSin(2.0f * kPi * 700.0f * t);
			}

			NkGriffinLimConfig cfg;
			cfg.fftSize = 1024;
			cfg.hopSize = 256;
			cfg.iterations = 60;

			NkMagSpectrogram mag = NkGriffinLim::Magnitude(sig.Data(), samples, cfg);
			if (mag.frames <= 2 || mag.bins != cfg.fftSize / 2 + 1)
				return false;
			NkVector<float32> recon = NkGriffinLim::Reconstruct(mag, cfg);
			if ((int32)recon.Size() < cfg.fftSize) // reconstruction plus courte (dernière trame partielle abandonnée) = normal
				return false;

			// (1) La magnitude du signal RECONSTRUIT doit re-coller à la cible (Griffin-Lim
			//     converge la magnitude, pas la phase). Erreur relative moyenne sur les trames
			//     internes (on saute les bords, moins recouverts).
			NkMagSpectrogram magR = NkGriffinLim::Magnitude(recon.Data(), (int32)recon.Size(), cfg);
			const int32 F = (mag.frames < magR.frames) ? mag.frames : magR.frames;
			double num = 0.0, den = 0.0;
			for (int32 f = 2; f < F - 2; ++f)
				for (int32 k = 0; k < mag.bins; ++k) {
					const double a = mag.data[(nk_size)f * (nk_size)mag.bins + (nk_size)k];
					const double b = magR.data[(nk_size)f * (nk_size)magR.bins + (nk_size)k];
					num += (a - b) * (a - b);
					den += a * a;
				}
			const double relErr = (den > 0.0) ? NkSqrt((float32)(num / den)) : 1.0f;

			// (2) Énergie préservée (ordre de grandeur), sur la région INTÉRIEURE commune
			//     (on saute fftSize échantillons de bord, moins recouverts par l'overlap-add).
			const int32 L = ((int32)recon.Size() < samples) ? (int32)recon.Size() : samples;
			const int32 lo = cfg.fftSize, hi = L - cfg.fftSize;
			double eIn = 0.0, eOut = 0.0;
			for (int32 i = lo; i < hi; ++i) {
				eIn += (double)sig[(nk_size)i] * sig[(nk_size)i];
				eOut += (double)recon[(nk_size)i] * recon[(nk_size)i];
			}
			const double eRatio = (eIn > 0.0) ? eOut / eIn : 0.0;

			const bool magOk = relErr < 0.15;               // magnitude reconstruite fidèle (~2,4 % obtenu)
			const bool energyOk = eRatio > 0.5 && eRatio < 2.0; // énergie du même ordre (~1,0 obtenu)
			return magOk && energyOk;
		}

	} // namespace ai
} // namespace nkentseu
