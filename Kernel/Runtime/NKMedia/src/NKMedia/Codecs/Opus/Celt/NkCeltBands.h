// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltBands.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DISPOSITION DES BANDES (critical bands). Table `eband5ms`
// (bornes des 21 bandes CELT en unités du plus court MDCT, base 2.5 ms/120 éch. à
// 48 kHz). Pour une trame de facteur LM (0..3 → 120/240/480/960 éch.), les bornes
// sont mises à l'échelle ×2^LM. Sert au décodage énergie/allocation/PVQ. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltBands {
			public:
				// Nombre de bandes CELT (mode 48 kHz standard).
				static constexpr int32 kNumBands = 21;

				// Bornes de bandes (22 valeurs → 21 bandes) en « unités MDCT court ».
				// eband5ms[i] = début de la bande i (en multiples de 200 Hz à 48 kHz).
				static const int16 *Eband5ms(); // pointe sur 22 entrées

				// Remplit `outEdges` (kNumBands+1 entrées) avec les bornes en BINS MDCT
				// pour un facteur de longueur `LM` (0..3). Renvoie le nombre de bandes.
				static int32 BandEdges(int32 LM, int32 *outEdges);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
