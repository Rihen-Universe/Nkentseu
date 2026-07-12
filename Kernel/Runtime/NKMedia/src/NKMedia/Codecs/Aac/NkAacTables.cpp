// =============================================================================
// NKMedia/Codecs/Aac/NkAacTables.cpp — tables AAC-LC (ISO/IEC 14496-3).
// Les valeurs numériques (fréquences, swb_offset) sont des constantes du standard.
// Les fenêtres sine/KBD sont CALCULÉES (sin / Bessel I0).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacTables.h"

#include <cmath> // sin, sqrt (fonctions math C, cf. convention NKMedia)

namespace nkentseu {
	namespace media {

		namespace {
			constexpr double kPi = 3.14159265358979323846;

			// sampling_frequency_index → Hz (Table 1.6.2.4).
			const int32 kSampleRates[12] = {96000, 88200, 64000, 48000, 44100, 32000,
											24000, 22050, 16000, 12000, 11025, 8000};

			// --- swb_offset fenêtre LONGUE (1024) : 7 tables uniques, terminant à 1024. ---
			const uint16 kSwbLong96[] = {0,	  4,   8,	12,	 16,  20,  24,	28,	 32,  36,  40,
										 44,  48,  52,	56,	 64,  72,  80,	88,	 96,  108, 120,
										 132, 144, 156, 172, 188, 212, 240, 276, 320, 384, 448,
										 512, 576, 640, 704, 768, 832, 896, 960, 1024};
			const uint16 kSwbLong64[] = {0,	  4,   8,	12,	 16,  20,  24,	28,	 32,  36,  40,	44,
										 48,  52,  56,	64,	 72,  80,  88,	100, 112, 124, 140, 156,
										 172, 192, 216, 240, 268, 304, 344, 384, 424, 464, 504, 544,
										 584, 624, 664, 704, 744, 784, 824, 864, 904, 944, 984, 1024};
			const uint16 kSwbLong48[] = {0,	  4,   8,	12,	 16,  20,  24,	28,	 32,  36,  40,	48,
										 56,  64,  72,	80,	 88,  96,  108, 120, 132, 144, 160, 176,
										 196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512,
										 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896,
										 928, 1024};
			const uint16 kSwbLong32[] = {0,	  4,   8,	12,	 16,  20,  24,	28,	 32,  36,  40,	48,	 56,
										 64,  72,  80,	88,	 96,  108, 120, 132, 144, 160, 176, 196, 216,
										 240, 264, 292, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608,
										 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 960, 992, 1024};
			const uint16 kSwbLong24[] = {0,	  4,   8,	12,	 16,  20,  24,	28,	 32,  36,  40,	44,
										 52,  60,  68,	76,	 84,  92,  100, 108, 116, 124, 136, 148,
										 160, 172, 188, 204, 220, 240, 260, 284, 308, 336, 364, 396,
										 432, 468, 508, 552, 600, 652, 704, 768, 832, 896, 960, 1024};
			const uint16 kSwbLong16[] = {0,	  8,   16,	24,	 32,  40,  48,	56,	 64,  72,  80,
										 88,  100, 112, 124, 136, 148, 160, 172, 184, 196, 212,
										 228, 244, 260, 280, 300, 320, 344, 368, 396, 424, 456,
										 492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024};
			const uint16 kSwbLong8[] = {0,	 12,  24,  36,	48,	 60,  72,  84,	96,	 108, 120,
										132, 144, 156, 172, 188, 204, 220, 236, 252, 268, 288,
										308, 328, 348, 372, 396, 420, 448, 476, 508, 544, 580,
										620, 664, 712, 764, 820, 880, 944, 1024};

			// --- swb_offset fenêtre COURTE (128) : 6 tables uniques, terminant à 128. ---
			const uint16 kSwbShort96[] = {0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128};
			const uint16 kSwbShort64[] = {0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128};
			const uint16 kSwbShort48[] = {0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 128};
			const uint16 kSwbShort24[] = {0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 128};
			const uint16 kSwbShort16[] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 128};
			const uint16 kSwbShort8[] = {0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128};

			// num_swb (= nombre de bandes ; les tables ci-dessus ont num_swb+1 offsets).
			const uint8 kNumSwbLong[12] = {41, 41, 47, 49, 49, 51, 47, 47, 43, 43, 43, 40};
			const uint8 kNumSwbShort[12] = {12, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15};

			// sf_index → table (indices de regroupement des fréquences proches).
			const uint16 *kLongBySf[12] = {kSwbLong96, kSwbLong96, kSwbLong64, kSwbLong48,
										   kSwbLong48, kSwbLong32, kSwbLong24, kSwbLong24,
										   kSwbLong16, kSwbLong16, kSwbLong16, kSwbLong8};
			const uint16 *kShortBySf[12] = {kSwbShort96, kSwbShort96, kSwbShort64, kSwbShort48,
											kSwbShort48, kSwbShort48, kSwbShort24, kSwbShort24,
											kSwbShort16, kSwbShort16, kSwbShort16, kSwbShort8};

			// Bessel I0 (série entière ; converge vite pour les arguments AAC).
			double BesselI0(double x) {
				double sum = 1.0, term = 1.0;
				const double x2 = x * x * 0.25;
				for (int32 k = 1; k < 64; ++k) {
					term *= x2 / ((double)k * (double)k);
					sum += term;
					if (term < 1e-15 * sum)
						break;
				}
				return sum;
			}
		} // namespace

		int32 NkAacTables::SampleRate(int32 index) {
			return (index >= 0 && index < 12) ? kSampleRates[index] : 0;
		}

		int32 NkAacTables::SampleRateIndex(int32 hz) {
			for (int32 i = 0; i < 12; ++i)
				if (kSampleRates[i] == hz)
					return i;
			return -1;
		}

		const uint16 *NkAacTables::SwbOffsetLong(int32 sfIndex) {
			return (sfIndex >= 0 && sfIndex < 12) ? kLongBySf[sfIndex] : kLongBySf[3];
		}
		int32 NkAacTables::NumSwbLong(int32 sfIndex) {
			return (sfIndex >= 0 && sfIndex < 12) ? (int32)kNumSwbLong[sfIndex] : 0;
		}
		const uint16 *NkAacTables::SwbOffsetShort(int32 sfIndex) {
			return (sfIndex >= 0 && sfIndex < 12) ? kShortBySf[sfIndex] : kShortBySf[3];
		}
		int32 NkAacTables::NumSwbShort(int32 sfIndex) {
			return (sfIndex >= 0 && sfIndex < 12) ? (int32)kNumSwbShort[sfIndex] : 0;
		}

		void NkAacTables::BuildWindow(float32 *out, int32 n, bool kbd) {
			const int32 half = n >> 1;
			if (!kbd) {
				// Fenêtre sinus : W[k] = sin(π/n · (k + 0.5)).
				for (int32 k = 0; k < n; ++k)
					out[k] = (float32)::sin(kPi / (double)n * ((double)k + 0.5));
				return;
			}
			// Fenêtre KBD : alpha=4 (long, n=2048) / 6 (court, n=256).
			const double alpha = (n > 512) ? 4.0 : 6.0;
			const double i0a = BesselI0(kPi * alpha);
			// w[p], p=0..half (noyau Kaiser-Bessel), puis somme cumulée + racine.
			double cumul = 0.0;
			double total = 0.0;
			// pré-calcul du total.
			for (int32 p = 0; p <= half; ++p) {
				const double t = (2.0 * (double)p / (double)half) - 1.0;
				total += BesselI0(kPi * alpha * ::sqrt(1.0 - t * t)) / i0a;
			}
			for (int32 kk = 0; kk < half; ++kk) {
				const double t = (2.0 * (double)kk / (double)half) - 1.0;
				cumul += BesselI0(kPi * alpha * ::sqrt(1.0 - t * t)) / i0a;
				const float32 v = (float32)::sqrt(cumul / total);
				out[kk] = v;			// moitié gauche
				out[n - 1 - kk] = v;	// moitié droite (symétrie)
			}
		}

		bool NkAacTables::SelfTest() {
			// 1) Aller-retour fréquence ↔ index.
			if (NkAacTables::SampleRate(4) != 44100 || NkAacTables::SampleRateIndex(48000) != 3)
				return false;
			if (NkAacTables::SampleRateIndex(12345) != -1)
				return false;

			// 2) swb_offset : monotone, commence à 0, termine à N, num_swb+1 offsets.
			for (int32 sf = 0; sf < 12; ++sf) {
				const uint16 *L = NkAacTables::SwbOffsetLong(sf);
				const int32 nL = NkAacTables::NumSwbLong(sf);
				if (L[0] != 0 || L[nL] != 1024)
					return false;
				for (int32 i = 1; i <= nL; ++i)
					if (L[i] <= L[i - 1])
						return false;
				const uint16 *S = NkAacTables::SwbOffsetShort(sf);
				const int32 nS = NkAacTables::NumSwbShort(sf);
				if (S[0] != 0 || S[nS] != 128)
					return false;
				for (int32 i = 1; i <= nS; ++i)
					if (S[i] <= S[i - 1])
						return false;
			}

			// 3) Fenêtres : propriété de Princen-Bradley W[k]² + W[k+N/2]² = 1
			//    (reconstruction parfaite de l'overlap-add MDCT), sine ET KBD.
			for (int32 pass = 0; pass < 2; ++pass) {
				const bool kbd = (pass == 1);
				float32 w[2048];
				NkAacTables::BuildWindow(w, 2048, kbd);
				for (int32 k = 0; k < 1024; ++k) {
					const float32 s = w[k] * w[k] + w[k + 1024] * w[k + 1024];
					if (s < 0.999f || s > 1.001f)
						return false;
				}
				// bornes [0,1] + symétrie.
				if (w[0] < 0.0f || w[0] > 0.05f || w[1023] < 0.95f)
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
