// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDenorm.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DÉNORMALISATION DES BANDES (bands.c denormalise_bands) :
// reconvertit chaque forme de bande NORMALISÉE (norme unité, cf. NkCeltVq) en
// spectre fréquentiel en la multipliant par son ÉNERGIE : freq = forme · 2^(E+eMean).
// C'est l'étape juste avant l'IMDCT (fréquence → temps). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltDenorm {
			public:
				// `X` = formes normalisées concaténées (indexées par bin MDCT, M=1<<LM). `bandLogE` = énergie
				// log2 par bande (coarse+fine). `freq` (taille M*eBands[nbBands]) reçoit le spectre. Dénormalise
				// les bandes [start,end) ; met à zéro le reste. Mono.
				static void DenormaliseBands(const float32 *X, float32 *freq, const float32 *bandLogE, int32 start,
											 int32 end, int32 LM);

				// eMean par bande (table libopus).
				static float32 EMean(int32 band);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
