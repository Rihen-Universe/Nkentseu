// =============================================================================
// NKMedia/Codecs/Aac/NkAacFilterbank.h
// -----------------------------------------------------------------------------
// Banc de filtres de synthèse AAC-LC (ISO/IEC 14496-3 §4.6.11) : IMDCT + fenêtrage
// + recouvrement-addition (overlap-add). Transforme le spectre fréquentiel d'un
// canal (1024 valeurs) en 1024 échantillons temporels, en gérant les 4 séquences
// de fenêtre (ONLY_LONG / LONG_START / EIGHT_SHORT / LONG_STOP) et les deux formes
// (sine / KBD) courante et précédente. État de recouvrement conservé par canal.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkAacFilterbank {
			public:
				// Réinitialise l'état de recouvrement (démarrage / changement de canal).
				void Reset();

				// Une trame : `freqIn` (1024 coefficients fréquentiels déquantifiés, désentrelacés)
				// → `timeOut` (1024 échantillons temporels). Met à jour le recouvrement interne.
				// `windowSequence` 0..3, `windowShape`/`windowShapePrev` 0=sine 1=KBD.
				void Process(const float32 *freqIn, int32 windowSequence, int32 windowShape, int32 windowShapePrev,
							 float32 *timeOut);

				static bool SelfTest();

			private:
				float32 mOverlap[1024] = {0};
		};

	} // namespace media
} // namespace nkentseu
