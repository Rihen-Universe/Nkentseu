// =============================================================================
// NKMedia/Codecs/Aac/NkAacFilterbank.cpp — IMDCT + fenêtrage + overlap-add (ISO 14496-3).
// IMDCT directe (somme cosinus tabulée) ; normalisation standard 2/N. Les 4 séquences
// de fenêtre reproduisent le placement/recouvrement du standard (Table 4.6.11).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacFilterbank.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"

#include <cmath> // cos

namespace nkentseu {
	namespace media {

		namespace {
			constexpr double kPi = 3.14159265358979323846;

			// Demi-fenêtres cachées : [forme 0=sine/1=KBD]. Long = 1024, court = 128.
			// (La fenêtre complète, symétrique, se déduit par miroir.)
			float32 gWinLong[2][1024];
			float32 gWinShort[2][128];
			// Tables cosinus pour l'IMDCT : cos(2π j / (4N)), taille 4N. N=2048 → 8192, N=256 → 1024.
			float32 gCos2048[8192];
			float32 gCos256[1024];
			bool gInit = false;

			void BuildTables() {
				if (gInit)
					return;
				float32 full2048[2048];
				float32 full256[256];
				for (int32 shape = 0; shape < 2; ++shape) {
					NkAacTables::BuildWindow(full2048, 2048, shape == 1);
					NkAacTables::BuildWindow(full256, 256, shape == 1);
					for (int32 i = 0; i < 1024; ++i)
						gWinLong[shape][i] = full2048[i]; // 1re moitié = demi-fenêtre
					for (int32 i = 0; i < 128; ++i)
						gWinShort[shape][i] = full256[i];
				}
				for (int32 j = 0; j < 8192; ++j)
					gCos2048[j] = (float32)::cos(2.0 * kPi * (double)j / 8192.0);
				for (int32 j = 0; j < 1024; ++j)
					gCos256[j] = (float32)::cos(2.0 * kPi * (double)j / 1024.0);
				gInit = true;
			}

			// IMDCT AAC : out[n] = (2/N) Σ_{k=0}^{N/2-1} spec[k]·cos((2π/N)(n+n0)(k+½)),
			// n0=(N/2+1)/2. Argument = 2π·m/(4N) avec m=(2n + N/2 + 1)(2k+1).
			void ImdctN(const float32 *spec, float32 *out, int32 N) {
				const int32 half = N >> 1;
				const int32 L = 4 * N;
				const int32 base = half + 1; // 2·n0
				const float32 *cosTab = (N == 2048) ? gCos2048 : gCos256;
				const float32 norm = 2.0f / (float32)N;
				for (int32 n = 0; n < N; ++n) {
					const int32 a = 2 * n + base;
					double s = 0.0;
					for (int32 k = 0; k < half; ++k) {
						const int32 m = (int32)(((int64)a * (2 * k + 1)) % L);
						s += (double)spec[k] * (double)cosTab[m];
					}
					out[n] = norm * (float32)s;
				}
			}
		} // namespace

		void NkAacFilterbank::Reset() {
			for (int32 i = 0; i < 1024; ++i)
				mOverlap[i] = 0.0f;
		}

		void NkAacFilterbank::Process(const float32 *freqIn, int32 windowSequence, int32 windowShape,
									  int32 windowShapePrev, float32 *timeOut) {
			BuildTables();
			const float32 *winLong = gWinLong[windowShape ? 1 : 0];
			const float32 *winLongPrev = gWinLong[windowShapePrev ? 1 : 0];
			const float32 *winShort = gWinShort[windowShape ? 1 : 0];
			const float32 *winShortPrev = gWinShort[windowShapePrev ? 1 : 0];
			const int32 nlong = 1024;
			const int32 nshort = 128;
			const int32 trans = 64;		// nshort/2
			const int32 nflat = 448;	// (nlong-nshort)/2

			static float32 transf[2048];

			if (windowSequence == 0) { // ONLY_LONG
				ImdctN(freqIn, transf, 2048);
				for (int32 i = 0; i < nlong; ++i)
					timeOut[i] = mOverlap[i] + transf[i] * winLongPrev[i];
				for (int32 i = 0; i < nlong; ++i)
					mOverlap[i] = transf[nlong + i] * winLong[nlong - 1 - i];
			} else if (windowSequence == 1) { // LONG_START
				ImdctN(freqIn, transf, 2048);
				for (int32 i = 0; i < nlong; ++i)
					timeOut[i] = mOverlap[i] + transf[i] * winLongPrev[i];
				for (int32 i = 0; i < nflat; ++i)
					mOverlap[i] = transf[nlong + i];
				for (int32 i = 0; i < nshort; ++i)
					mOverlap[nflat + i] = transf[nlong + nflat + i] * winShort[nshort - 1 - i];
				for (int32 i = 0; i < nflat; ++i)
					mOverlap[nflat + nshort + i] = 0.0f;
			} else if (windowSequence == 3) { // LONG_STOP
				ImdctN(freqIn, transf, 2048);
				for (int32 i = 0; i < nflat; ++i)
					timeOut[i] = mOverlap[i];
				for (int32 i = 0; i < nshort; ++i)
					timeOut[nflat + i] = mOverlap[nflat + i] + transf[nflat + i] * winShortPrev[i];
				for (int32 i = 0; i < nflat; ++i)
					timeOut[nflat + nshort + i] = mOverlap[nflat + nshort + i] + transf[nflat + nshort + i];
				for (int32 i = 0; i < nlong; ++i)
					mOverlap[i] = transf[nlong + i] * winLong[nlong - 1 - i];
			} else { // EIGHT_SHORT (windowSequence == 2)
				for (int32 w = 0; w < 8; ++w)
					ImdctN(freqIn + w * nshort, transf + 2 * nshort * w, 256);
				for (int32 i = 0; i < nflat; ++i)
					timeOut[i] = mOverlap[i];
				for (int32 i = 0; i < nshort; ++i) {
					timeOut[nflat + i] = mOverlap[nflat + i] + transf[nshort * 0 + i] * winShortPrev[i];
					timeOut[nflat + 1 * nshort + i] = mOverlap[nflat + nshort * 1 + i] +
													  transf[nshort * 1 + i] * winShort[nshort - 1 - i] +
													  transf[nshort * 2 + i] * winShort[i];
					timeOut[nflat + 2 * nshort + i] = mOverlap[nflat + nshort * 2 + i] +
													  transf[nshort * 3 + i] * winShort[nshort - 1 - i] +
													  transf[nshort * 4 + i] * winShort[i];
					timeOut[nflat + 3 * nshort + i] = mOverlap[nflat + nshort * 3 + i] +
													  transf[nshort * 5 + i] * winShort[nshort - 1 - i] +
													  transf[nshort * 6 + i] * winShort[i];
					if (i < trans)
						timeOut[nflat + 4 * nshort + i] = mOverlap[nflat + nshort * 4 + i] +
														  transf[nshort * 7 + i] * winShort[nshort - 1 - i] +
														  transf[nshort * 8 + i] * winShort[i];
				}
				for (int32 i = 0; i < nshort; ++i) {
					if (i >= trans)
						mOverlap[nflat + 4 * nshort + i - nlong] =
							transf[nshort * 7 + i] * winShort[nshort - 1 - i] + transf[nshort * 8 + i] * winShort[i];
					mOverlap[nflat + 5 * nshort + i - nlong] =
						transf[nshort * 9 + i] * winShort[nshort - 1 - i] + transf[nshort * 10 + i] * winShort[i];
					mOverlap[nflat + 6 * nshort + i - nlong] =
						transf[nshort * 11 + i] * winShort[nshort - 1 - i] + transf[nshort * 12 + i] * winShort[i];
					mOverlap[nflat + 7 * nshort + i - nlong] =
						transf[nshort * 13 + i] * winShort[nshort - 1 - i] + transf[nshort * 14 + i] * winShort[i];
					mOverlap[nflat + 8 * nshort + i - nlong] = transf[nshort * 15 + i] * winShort[nshort - 1 - i];
				}
				for (int32 i = 0; i < nflat; ++i)
					mOverlap[nflat + nshort + i] = 0.0f;
			}
		}

		// ---------------------------------------------------------------------------
		bool NkAacFilterbank::SelfTest() {
			BuildTables();
			// Reconstruction TDAC (ONLY_LONG) : un signal passé par MDCT directe (fenêtrée)
			// puis IMDCT + fenêtrage + overlap-add doit se reconstruire (fenêtres à PR=1).
			// Signal test sur 4 trames chevauchantes (hop 1024) ; la trame du milieu doit
			// reconstruire l'entrée.
			const int32 N = 2048;
			const int32 hop = 1024;
			const int32 base = 1025; // 2·n0 pour N=2048
			auto sig = [](int32 n) -> float32 {
				return (float32)::cos(2.0 * kPi * 5.3 * (double)n / 2048.0) +
					   0.5f * (float32)::sin(2.0 * kPi * 11.0 * (double)n / 2048.0);
			};
			// MDCT directe fenêtrée d'une trame x[2048] (fenêtre sine) → X[1024].
			auto fwdMdct = [&](const float32 *x, float32 *X) {
				const float32 *win = gWinLong[0];
				for (int32 k = 0; k < 1024; ++k) {
					double s = 0.0;
					for (int32 n = 0; n < N; ++n) {
						const float32 wn = (n < 1024) ? win[n] : win[2047 - n];
						const int32 m = (int32)(((int64)(2 * n + base) * (2 * k + 1)) % 8192);
						s += (double)(x[n] * wn) * (double)gCos2048[m];
					}
					// Facteur 2 : adjoint exact de l'IMDCT normalisée 2/N (rend la paire PR).
					X[k] = (float32)(2.0 * s);
				}
			};

			NkAacFilterbank fb;
			fb.Reset();
			float32 recon[4][1024];
			for (int32 f = 0; f < 4; ++f) {
				float32 x[2048];
				for (int32 n = 0; n < N; ++n)
					x[n] = sig(f * hop + n);
				float32 X[1024];
				fwdMdct(x, X);
				fb.Process(X, 0, 0, 0, recon[f]);
			}
			// La trame 2 (indices 2*1024..3*1024) reconstruit sig(2*1024 .. 3*1024).
			float32 maxErr = 0.0f;
			for (int32 n = 0; n < 1024; ++n) {
				const float32 e = recon[2][n] - sig(2 * hop + n);
				const float32 ae = e < 0 ? -e : e;
				if (ae > maxErr)
					maxErr = ae;
			}
			if (maxErr > 1e-3f)
				return false;

			// L'IMDCT d'un seul coefficient donne bien une cosinusoïde d'amplitude 2/N.
			{
				float32 spec[1024] = {0};
				spec[3] = 1.0f;
				float32 out[2048];
				ImdctN(spec, out, 2048);
				// out[0] = (2/2048)·cos((2π/2048)(0+512.5)(3.5)).
				const double expected = (2.0 / 2048.0) * ::cos(2.0 * kPi / 2048.0 * 512.5 * 3.5);
				if (out[0] - (float32)expected > 1e-4f || (float32)expected - out[0] > 1e-4f)
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
