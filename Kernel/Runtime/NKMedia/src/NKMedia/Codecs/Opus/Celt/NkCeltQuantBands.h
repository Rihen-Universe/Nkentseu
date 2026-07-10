// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DÉCODAGE DES BANDES (quant_all_bands, algo RFC 6716,
// réécrit à la sauce Nkentseu). Boucle sur les 21 bandes ; chaque bande décode sa
// forme spectrale via split récursif (angle θ + haar), stéréo (non ici : mono),
// folding (LCG / spectre replié), et AlgUnquant aux feuilles. Produit le spectre
// normalisé `X` + les masques de collapse. Chemin DÉCODEUR MONO (ghomala' = CELT
// mono). Zero-STL, nkentseu::media.
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
				// Décode les bandes [start,end) pour un flux MONO. `X_` (taille M*eBands[nbBands]) reçoit le
				// spectre normalisé. `collapseMasks` (nbBands octets) reçoit les masques. `pulses` = allocation
				// par bande (index cache). `shortBlocks` = transient. `tfRes` = résolution T/F par bande.
				// `totalBits`/`balance` = budget (unités BITRES). `LM` = facteur, `codedBands` = bandes codées.
				// `seed` = graine LCG (in/out).
				static void QuantAllBands(NkOpusRangeDecoder &dec, int32 start, int32 end, float32 *X_,
										  uint8 *collapseMasks, const int32 *pulses, int32 shortBlocks, int32 spread,
										  const int32 *tfRes, int32 totalBits, int32 balance, int32 LM,
										  int32 codedBands, uint32 *seed);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
