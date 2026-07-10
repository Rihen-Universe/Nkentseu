// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltRate.cpp — bits<->pulses (cœur allocation CELT).
// Coût = log2(V(N,K)) en 1/8 de bit, calculé en VIRGULE FIXE BIT-EXACT (algorithme
// RFC 6716) — nécessaire à l'interop. Réécriture Nkentseu (pas de code importé).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltRate.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltPvq.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Position du bit de poids fort (1-indexé) : Ilog(0)=0, Ilog(1)=1, Ilog(2)=2...
			int32 Ilog(uint32 v) {
				int32 r = 0;
				while (v) {
					++r;
					v >>= 1;
				}
				return r;
			}

			// log2 en virgule fixe : renvoie floor(log2(val)·2^frac) arrondi (RFC 6716).
			// Méthode : normalisation en 16.16 puis extraction des bits fractionnaires par
			// carrés successifs en Q15. Réécriture Nkentseu de l'algorithme du format.
			int32 Log2Frac(uint32 val, int32 frac) {
				if (val == 0)
					return 0;
				const int32 l = Ilog(val);
				// Puissance de 2 exacte : pas d'arrondi.
				if ((val & (val - 1)) == 0)
					return (l - 1) << frac;
				// Normalise val autour de 2^16 (arrondi vers le haut, anti-débordement).
				if (l > 16)
					val = ((val - 1) >> (l - 16)) + 1;
				else
					val <<= (16 - l);
				int32 acc = (l - 1) << frac;
				// Au moins une itération (l'arrondi ci-dessus peut décaler la partie entière).
				int32 f = frac;
				do {
					const int32 b = (int32)(val >> 16);
					acc += b << f;
					val = (val + (uint32)b) >> b;
					val = (val * val + 0x7FFFu) >> 15; // carré en Q15, arrondi
				} while (f-- > 0);
				return acc + (val > 0x8000u ? 1 : 0);
			}
		} // namespace

		int32 NkCeltRate::PulsesToBits(int32 N, int32 K) {
			if (K <= 0 || N < 1)
				return 0;
			const uint32 v = NkCeltPvq::V(N, K);
			if (v <= 1u)
				return 0;
			return Log2Frac(v, kBitRes); // coût en 1/8 de bit, bit-exact
		}

		int32 NkCeltRate::BitsToPulses(int32 N, int32 budget) {
			if (budget <= 0 || N < 1)
				return 0;
			// Recherche du plus grand K tel que le coût tient dans le budget.
			// PulsesToBits est croissant en K → recherche linéaire bornée (dichotomie possible).
			int32 K = 0;
			const int32 KMAX = 512;
			uint32 prevV = NkCeltPvq::V(N, 0);
			for (int32 k = 1; k <= KMAX; ++k) {
				const uint32 v = NkCeltPvq::V(N, k);
				if (v == 0 || v <= prevV || v >= 0x80000000u) // débordement / hors périmètre
					break;
				prevV = v;
				if (PulsesToBits(N, k) <= budget)
					K = k;
				else
					break;
			}
			return K;
		}

		bool NkCeltRate::SelfTest() {
			bool ok = true;
			const int32 Ns[4] = {2, 4, 8, 16};

			for (int32 ni = 0; ni < 4; ++ni) {
				const int32 N = Ns[ni];
				if (NkCeltRate::PulsesToBits(N, 0) != 0)
					ok = false;

				// Kmax sûr : tant que V(N,K) est valide (non nul, strictement croissant → pas de débordement).
				int32 kmax = 0;
				uint32 prevV = NkCeltPvq::V(N, 0);
				for (int32 k = 1; k <= 64; ++k) {
					const uint32 v = NkCeltPvq::V(N, k);
					if (v == 0 || v <= prevV || v >= 0x80000000u)
						break;
					prevV = v;
					kmax = k;
				}

				// 1) Monotonie du coût.
				int32 prevB = -1;
				for (int32 k = 0; k <= kmax; ++k) {
					const int32 b = NkCeltRate::PulsesToBits(N, k);
					if (b < prevB)
						ok = false;
					prevB = b;
				}

				// 2) Aller-retour (relaxé sur les collisions d'arrondi) : k2 ≥ K et même coût.
				for (int32 K = 1; K <= kmax; ++K) {
					const int32 b = NkCeltRate::PulsesToBits(N, K);
					const int32 k2 = NkCeltRate::BitsToPulses(N, b);
					if (k2 < K)
						ok = false;
					if (NkCeltRate::PulsesToBits(N, k2) != b)
						ok = false;
				}

				// 3) Cohérence budget : le K choisi tient, K+1 dépasse (si K+1 ≤ kmax).
				const int32 budgets[3] = {20, 50, 100};
				for (int32 bi = 0; bi < 3; ++bi) {
					const int32 budget = budgets[bi];
					const int32 K = NkCeltRate::BitsToPulses(N, budget);
					if (NkCeltRate::PulsesToBits(N, K) > budget)
						ok = false;
					if (K < kmax && NkCeltRate::PulsesToBits(N, K + 1) <= budget)
						ok = false;
				}
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
