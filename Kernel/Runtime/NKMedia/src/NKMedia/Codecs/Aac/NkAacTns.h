// =============================================================================
// NKMedia/Codecs/Aac/NkAacTns.h
// -----------------------------------------------------------------------------
// Temporal Noise Shaping (TNS) AAC-LC (ISO/IEC 14496-3 §4.6.9). Filtre tout-pôle
// (auto-régressif) appliqué au spectre déquantifié, par fenêtre et par filtre TNS,
// pour remodeler l'enveloppe temporelle du bruit de quantification. Les paramètres
// (ordre, longueur, direction, coefficients) sont déjà lus par NkAacIcs ; ici on
// les décode en coefficients LPC et on filtre le spectre en place. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Aac/NkAacIcs.h"

namespace nkentseu {
	namespace media {

		struct NkAacTns {
			public:
				// Applique le TNS en place sur `spec` (spectre déquantifié désentrelacé, 1024).
				// `sfIndex` = sampling_frequency_index (pour la limite de bandes TNS).
				static void Apply(const NkAacIcs &ics, int32 sfIndex, float32 *spec);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
