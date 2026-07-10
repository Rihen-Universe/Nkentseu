// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltEnergy.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — ÉNERGIE GROSSIÈRE des bandes (libopus quant_bands.c
// unquant_coarse_energy). Chaque bande a une énergie (en dB) prédite depuis la
// bande précédente (prédiction temps + fréquence) + un résidu codé en LAPLACE.
// Fournit le décodeur + un encodeur SIMPLIFIÉ (chemin Laplace) pour prouver
// l'aller-retour. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltEnergy {
			public:
				// Décode l'énergie grossière des bandes [start,end) pour `C` canaux, facteur `LM` (0..3),
				// mode `intra`. `oldEBands` = tableau [nbBands*C] (dB) mis à jour en place.
				static void UnquantCoarse(NkOpusRangeDecoder &dec, float32 *oldEBands, int32 nbBands, int32 start,
										  int32 end, bool intra, int32 C, int32 LM);

				// Encodeur SIMPLIFIÉ (chemin Laplace uniquement, pour les tests) : quantifie `targetE`
				// (dB) et écrit les résidus ; met à jour `oldEBands` comme le fera le décodeur.
				static void QuantCoarseSimple(NkOpusRangeEncoder &enc, const float32 *targetE, float32 *oldEBands,
											  int32 nbBands, int32 start, int32 end, bool intra, int32 C, int32 LM);

				// ÉNERGIE FINE : raffine chaque bande avec `fineQuant[i]` bits bruts (unquant_fine_energy).
				static void UnquantFine(NkOpusRangeDecoder &dec, float32 *oldEBands, int32 nbBands,
										const int32 *fineQuant, int32 start, int32 end, int32 C);
				// Encodeur d'énergie fine (pour tests) : encode `residual` (dB, à raffiner) → oldEBands.
				static void QuantFineSimple(NkOpusRangeEncoder &enc, float32 *oldEBands, const float32 *residual,
											int32 nbBands, const int32 *fineQuant, int32 start, int32 end, int32 C);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
