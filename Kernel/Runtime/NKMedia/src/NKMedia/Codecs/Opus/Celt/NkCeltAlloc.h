// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltAlloc.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — ALLOCATION DE BITS (rate.c), cœur : table `band_allocation`
// (11 niveaux de qualité × 21 bandes) + BISSECTION sur la qualité pour trouver la
// ligne d'allocation la plus haute qui tient dans le budget total, puis construction
// des vecteurs d'interpolation `bits1`/`bits2` (+ seuils `thresh`, tilt `trim`).
// C'est l'entrée d'`interp_bits2pulses` (répartition fine → pulses/énergie fine).
// Port de clt_compute_allocation (partie déterministe). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltAlloc {
			public:
				static constexpr int32 kNumQuality = 11; // lignes de band_allocation
				static constexpr int32 kNumBands = 21;
				static constexpr int32 kBitRes = 3;

				// Table d'allocation par bande et par niveau de qualité (libopus).
				static const uint8 *BandAllocation(); // 11*21 octets

				// Sélectionne la ligne de qualité `lo` (la plus haute tenant dans `total`) et construit
				// `bits1`/`bits2` (unités BITRES) + `thresh`/`trimOffset` pour les bandes [start,end).
				// `offsets` = boosts dynalloc (peut être nullptr = 0). `cap` = plafond par bande.
				// Renvoie la ligne `lo` retenue.
				static int32 BuildInterp(int32 start, int32 end, const int32 *offsets, const int32 *cap,
										 int32 allocTrim, int32 total, int32 C, int32 LM, int32 *bits1, int32 *bits2,
										 int32 *thresh, int32 *trimOffset);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
