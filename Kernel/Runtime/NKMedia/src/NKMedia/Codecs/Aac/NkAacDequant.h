// =============================================================================
// NKMedia/Codecs/Aac/NkAacDequant.h
// -----------------------------------------------------------------------------
// Déquantification AAC-LC (ISO/IEC 14496-3 §4.6.2/§4.6.3) : à partir des
// coefficients quantifiés d'un canal (NkAacIcs), produit le spectre fréquentiel
// flottant, DÉSENTRELACÉ par fenêtre :
//   1. application des pulses (fenêtres longues) ;
//   2. déquantification inverse x = sign(q)·|q|^(4/3) ;
//   3. facteur d'échelle 2^(0.25·(sf-100)) par bande ;
//   4. désentrelacement des groupes de fenêtres courtes.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Aac/NkAacIcs.h"

namespace nkentseu {
	namespace media {

		struct NkAacDequant {
			public:
				// Remplit `specOut` (1024 flottants) avec le spectre déquantifié et remis à
				// l'échelle, désentrelacé par fenêtre. Les bandes intensity/PNS reçoivent 0
				// (traitées par la brique stéréo/bruit).
				static void Apply(const NkAacIcs &ics, float32 *specOut);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
