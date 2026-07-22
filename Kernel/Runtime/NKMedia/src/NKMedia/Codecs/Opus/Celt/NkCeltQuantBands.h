// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DÉCODAGE DES BANDES (quant_all_bands, algo RFC 6716,
// réécrit à la sauce Nkentseu). Boucle sur les 21 bandes ; chaque bande décode sa
// forme spectrale via split récursif (angle θ + haar), folding (LCG / spectre
// replié), et AlgUnquant aux feuilles. Produit le spectre normalisé `X` (+ `Y` en
// stéréo) + les masques de collapse. Chemins MONO et STÉRÉO (quant_band_stereo :
// θ mid/side à pdf en escalier, cas N=2 à 1 bit de signe, intensity → qn=1 + flag
// inv, dual stereo → 2 canaux indépendants, stereo_merge). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltQuantBands {
			public:
				// Décode les bandes [start,end). `X_` (taille M*eBands[nbBands]) reçoit le spectre normalisé
				// du canal 0 ; `Y_` = canal 1 en stéréo (nullptr = mono). `collapseMasks` (nbBands*C octets,
				// indexé [i*C+c]) reçoit les masques. `pulses` = allocation par bande (index cache).
				// `shortBlocks` = transient. `dualStereo`/`intensity` = paramètres stéréo décodés par
				// l'allocation (ignorés si Y_ == nullptr). `tfRes` = résolution T/F par bande.
				// `totalBits`/`balance` = budget (unités BITRES). `LM` = facteur, `codedBands` = bandes codées.
				// `seed` = graine LCG (in/out).
				static void QuantAllBands(NkOpusRangeDecoder &dec, int32 start, int32 end, float32 *X_, float32 *Y_,
										  uint8 *collapseMasks, const int32 *pulses, int32 shortBlocks, int32 spread,
										  int32 dualStereo, int32 intensity, const int32 *tfRes, int32 totalBits,
										  int32 balance, int32 LM, int32 codedBands, uint32 *seed);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
