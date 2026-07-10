// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltSplit.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — prérequis de `quant_all_bands` : transformée de HAAR (haar1,
// utilisée par le split récursif des bandes et le folding — orthonormale, involutive)
// et `compute_qn` (nombre de niveaux de quantification de l'angle θ de split).
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltSplit {
			public:
				// Transformée de Haar sur place (bands.c haar1) : paires (a,b) → ((a+b)/√2,(a-b)/√2).
				// `stride` = entrelacement (blocs). Orthonormale et involutive (haar1∘haar1 = identité).
				static void Haar1(float32 *X, int32 N0, int32 stride);

				// Nombre de niveaux de quantification de l'angle de split (bands.c compute_qn).
				static int32 ComputeQn(int32 N, int32 b, int32 offset, int32 pulseCap, int32 stereo);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
