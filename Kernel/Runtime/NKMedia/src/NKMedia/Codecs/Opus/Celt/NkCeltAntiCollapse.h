// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — ANTI-COLLAPSE (bands.c anti_collapse) : quand une bande n'a
// reçu aucun pulse sur un bloc court (masque de collapse à 0), elle est « effondrée »
// (silence spectral qui créerait des artefacts). On la remplit d'un bruit pseudo-
// aléatoire (LCG) calibré sur l'énergie/profondeur, puis on renormalise. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltAntiCollapse {
			public:
				// LCG d'Opus (celt_lcg_rand).
				static uint32 LcgRand(uint32 seed);
				// Renormalise `X` (N éléments) à la norme `gain`.
				static void RenormaliseVector(float32 *X, int32 N, float32 gain);

				// Anti-collapse d'une bande. `X` pointe le début de la bande (N0<<LM éléments, entrelacés par bloc).
				// `collapseMask` = bits des blocs non effondrés. `logE`/`prev1`/`prev2` = énergies (dB). `pulses`
				// = pulses alloués. Renvoie la graine LCG mise à jour.
				static uint32 ApplyBand(float32 *X, int32 N0, int32 LM, uint32 collapseMask, float32 logE,
										float32 prev1, float32 prev2, int32 pulses, uint32 seed);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
