// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltRate.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — CONVERSION BITS ↔ PULSES (cœur de l'allocation, rate.c).
// Le coût en bits pour coder K pulses sur N dimensions ≈ log2(V(N,K)) ; l'allocation
// répartit un budget de bits par bande → nombre de pulses K. Ici : `PulsesToBits`
// (coût, en 1/8 de bit = « BITRES ») et `BitsToPulses` (K max tenant dans un budget).
// S'appuie sur `NkCeltPvq::V`. Testé : monotonie + aller-retour.
//
// Coût calculé en VIRGULE FIXE BIT-EXACT (log2 par carrés successifs, algorithme du
// format RFC 6716, réécrit à la sauce Nkentseu) → compatible interop. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltRate {
			public:
				static constexpr int32 kBitRes = 3; // résolution : 1 bit = 8 unités (1<<3)

				// Coût (unités BITRES) pour coder K pulses sur N dimensions = Log2Frac(V(N,K)).
				static int32 PulsesToBits(int32 N, int32 K);
				// Nombre max de pulses K tel que PulsesToBits(N,K) ≤ budget (unités BITRES).
				static int32 BitsToPulses(int32 N, int32 budget);

				// --- Cache de pulses (structure d'index get_pulses, RFC 6716) ---
				// V(N,K) tient-il dans 32 bits ? (tables maxN/maxK du format).
				static bool FitsIn32(int32 N, int32 K);
				// Mapping index→pulses réels : get_pulses(q).
				static int32 GetPulses(int32 q);
				// Index de cache `q` (0..count) dont le coût tient dans `bits` (recherche dichotomique).
				static int32 Bits2Pulses(int32 N, int32 bits);
				// Coût (unités BITRES) de l'index de cache `q`.
				static int32 Pulses2Bits(int32 N, int32 q);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
