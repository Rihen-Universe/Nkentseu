// =============================================================================
// NKMedia/Codecs/Aac/NkAacTables.h
// -----------------------------------------------------------------------------
// Tables constantes du format AAC-LC (ISO/IEC 14496-3) — données du standard
// (obligatoires pour l'interopérabilité), pas du code importé :
//   - fréquences d'échantillonnage indexées (sampling_frequency_index),
//   - décalages de bandes de facteurs d'échelle (swb_offset) pour fenêtres
//     LONGUES (1024) et COURTES (128), par groupe de fréquence,
//   - fenêtres d'analyse/synthèse sine et KBD (Kaiser-Bessel Derived), CALCULÉES.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkAacTables {
			public:
				// sampling_frequency_index (0..11) → Hz. 0 hors plage.
				static int32 SampleRate(int32 index);
				// Hz → sampling_frequency_index (0..11), -1 si inconnu.
				static int32 SampleRateIndex(int32 hz);

				// Fenêtre LONGUE (1024) : offsets de bande [0..num_swb] terminant à 1024.
				static const uint16 *SwbOffsetLong(int32 sfIndex);
				static int32 NumSwbLong(int32 sfIndex);
				// Fenêtre COURTE (128) : offsets de bande [0..num_swb] terminant à 128.
				static const uint16 *SwbOffsetShort(int32 sfIndex);
				static int32 NumSwbShort(int32 sfIndex);

				// Remplit `out` (n valeurs) avec la fenêtre d'analyse. `kbd` : true = KBD
				// (alpha=4 long / 6 court), false = sine. n = 2048 (long) ou 256 (court).
				static void BuildWindow(float32 *out, int32 n, bool kbd);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
