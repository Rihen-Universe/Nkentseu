// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltPvq.cpp — PVQ/CWRS (libopus cwrs.c).
// U(n,k) via récurrence symétrique ; icwrs (vecteur→index) + cwrsi (index→vecteur).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltPvq.h"

namespace nkentseu {
	namespace media {

		namespace {

			int32 Abs32(int32 v) {
				return v < 0 ? -v : v;
			}

			// U(n,k) : récurrence U(n,k)=U(n-1,k)+U(n,k-1)+U(n-1,k-1),
			// bases U(0,0)=1, U(0,k>0)=0, U(n>0,0)=0. Symétrique U(n,k)=U(k,n).
			// Table (k+1) itérée sur n → gère les grandes bandes (n jusqu'à ~1408) avec k petit.
			// (Débordement uint32 possible si V(n,k)≥2^32 — filtré en amont par FitsIn32.)
			uint32 PvqU(int32 n, int32 k) {
				if (n < 0 || k < 0)
					return 0;
				if (n == 0)
					return k == 0 ? 1u : 0u;
				if (k == 0)
					return 0u;
				static const int32 MAXK = 200; // couvre get_pulses(MAX_PSEUDO=40)=128 + marge (V(2,128) → k=129)
				if (k > MAXK)
					return 0;
				uint32 row[MAXK + 1];
				uint32 prev[MAXK + 1];
				for (int32 j = 0; j <= k; ++j)
					prev[j] = (j == 0) ? 1u : 0u; // ligne n=0
				for (int32 i = 1; i <= n; ++i) {
					row[0] = 0u; // U(i,0)=0
					for (int32 j = 1; j <= k; ++j)
						row[j] = prev[j] + row[j - 1] + prev[j - 1];
					for (int32 j = 0; j <= k; ++j)
						prev[j] = row[j];
				}
				return prev[k];
			}

			uint32 PvqV(int32 n, int32 k) {
				if (k == 0)
					return 1u;
				if (n == 0)
					return 0u;
				return PvqU(n, k) + PvqU(n, k + 1);
			}

			// vecteur → index (icwrs, cwrs.c).
			uint32 Icwrs(int32 n, const int32 *y) {
				int32 j = n - 1;
				uint32 i = (y[j] < 0) ? 1u : 0u;
				int32 k = Abs32(y[j]);
				do {
					j--;
					i += PvqU(n - j, k);
					k += Abs32(y[j]);
					if (y[j] < 0)
						i += PvqU(n - j, k + 1);
				} while (j > 0);
				return i;
			}

			// index → vecteur (cwrsi, cwrs.c). U(a,b)=PvqU(a,b) (symétrique).
			void Cwrsi(int32 n, int32 k, uint32 i, int32 *y) {
				int32 s;
				int32 k0;
				int32 val;
				while (n > 2) {
					uint32 p, q;
					if (k >= n) {
						// beaucoup de pulses.
						p = PvqU(n, k + 1);
						s = -(int32)(i >= p);
						i -= p & (uint32)s;
						k0 = k;
						q = PvqU(n, n);
						if (q > i) {
							k = n;
							do {
								p = PvqU(--k, n);
							} while (p > i);
						} else {
							for (p = PvqU(n, k); p > i; p = PvqU(n, k))
								k--;
						}
						i -= p;
						val = (k0 - k + s) ^ s;
						*y++ = val;
					} else {
						// beaucoup de dimensions.
						p = PvqU(k, n);
						q = PvqU(k + 1, n);
						if (p <= i && i < q) {
							i -= p;
							*y++ = 0;
						} else {
							s = -(int32)(i >= q);
							i -= q & (uint32)s;
							k0 = k;
							do {
								p = PvqU(--k, n);
							} while (p > i);
							i -= p;
							val = (k0 - k + s) ^ s;
							*y++ = val;
						}
					}
					n--;
				}
				// n == 2.
				{
					const uint32 p = (uint32)(2 * k + 1);
					s = -(int32)(i >= p);
					i -= p & (uint32)s;
					k0 = k;
					k = (int32)((i + 1) >> 1);
					if (k)
						i -= (uint32)(2 * k - 1);
					val = (k0 - k + s) ^ s;
					*y++ = val;
				}
				// n == 1.
				{
					s = -(int32)i;
					val = (k + s) ^ s;
					*y = val;
				}
			}

		} // namespace

		uint32 NkCeltPvq::V(int32 n, int32 k) {
			return PvqV(n, k);
		}

		void NkCeltPvq::EncodePulses(NkOpusRangeEncoder &enc, const int32 *y, int32 n, int32 k) {
			if (k <= 0 || n < 2)
				return;
			enc.EncodeUint(Icwrs(n, y), PvqV(n, k));
		}

		void NkCeltPvq::DecodePulses(NkOpusRangeDecoder &dec, int32 *y, int32 n, int32 k) {
			if (k <= 0 || n < 2)
				return;
			Cwrsi(n, k, dec.DecodeUint(PvqV(n, k)), y);
		}

		bool NkCeltPvq::SelfTest() {
			bool ok = true;

			// 1) V(n,k) : valeurs connues. V(1,k)=2 (k>0), V(2,k)=4k, V(n,0)=1.
			if (PvqV(2, 1) != 4u || PvqV(2, 2) != 8u || PvqV(2, 3) != 12u)
				ok = false;
			if (PvqV(3, 0) != 1u)
				ok = false;

			// 2) Aller-retour vecteur → index → vecteur (énumération exhaustive petits cas).
			//    Pour chaque (n,k), on parcourt tous les index [0,V) et vérifie icwrs(cwrsi(i))==i.
			const int32 cases[][2] = {{2, 1}, {2, 3}, {3, 2}, {4, 2}, {5, 3}, {6, 2}};
			for (uint32 c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
				const int32 n = cases[c][0];
				const int32 k = cases[c][1];
				const uint32 vtot = PvqV(n, k);
				for (uint32 idx = 0; idx < vtot; ++idx) {
					int32 y[8] = {0, 0, 0, 0, 0, 0, 0, 0};
					Cwrsi(n, k, idx, y);
					// somme des |pulses| == k
					int32 sum = 0;
					for (int32 j = 0; j < n; ++j)
						sum += Abs32(y[j]);
					if (sum != k)
						ok = false;
					// index reconstruit == idx
					if (Icwrs(n, y) != idx)
						ok = false;
				}
			}

			// 3) Aller-retour via le range coder (encode → decode).
			{
				const int32 n = 5, k = 3;
				int32 y[5] = {1, 0, -1, 0, 1}; // somme |.| = 3
				uint8 buf[256];
				NkOpusRangeEncoder enc;
				enc.Init(buf, 256);
				NkCeltPvq::EncodePulses(enc, y, n, k);
				enc.Done();
				int32 y2[5] = {0, 0, 0, 0, 0};
				NkOpusRangeDecoder dec;
				dec.Init(buf, 256);
				NkCeltPvq::DecodePulses(dec, y2, n, k);
				for (int32 j = 0; j < n; ++j)
					if (y[j] != y2[j])
						ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
