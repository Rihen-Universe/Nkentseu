// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltRate.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — CONVERSION BITS ↔ PULSES (cœur de l'allocation, rate.c).
// Le coût en bits pour coder K pulses sur N dimensions ≈ log2(V(N,K)) ; l'allocation
// répartit un budget de bits par bande → nombre de pulses K. Ici : `PulsesToBits`
// (coût, en 1/8 de bit = « BITRES ») et `BitsToPulses` (K max tenant dans un budget).
// S'appuie sur `NkCeltPvq::V`. Testé : monotonie + aller-retour.
//
// ⚠️ Approximation du cache libopus (log2 flottant) — le raffinement bit-exact
// (cache entier de rate.c) sera nécessaire pour un décodage bit-exact final.
// Zero-STL, nkentseu::media.
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

				// Coût (en unités BITRES) pour coder K pulses sur N dimensions ≈ 8·log2(V(N,K)).
				static int32 PulsesToBits(int32 N, int32 K);
				// Nombre max de pulses K tel que PulsesToBits(N,K) ≤ budget (unités BITRES).
				static int32 BitsToPulses(int32 N, int32 budget);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
