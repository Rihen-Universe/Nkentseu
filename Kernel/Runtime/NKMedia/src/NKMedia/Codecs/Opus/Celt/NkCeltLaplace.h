// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltLaplace.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — sous-brique : CODEUR DE LAPLACE (laplace.c de libopus).
// Utilisé par le décodage de l'ÉNERGIE GROSSIÈRE des bandes CELT (prédiction +
// résidu Laplace via le range coder). Port fidèle : décodeur + encodeur, aller-
// retour prouvé. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltLaplace {
			public:
				// Décode une valeur signée de loi de Laplace. `fs` = P(0) initiale (Q15), `decay` = décroissance.
				static int32 Decode(NkOpusRangeDecoder &dec, uint32 fs, int32 decay);
				// Encode `value` (peut être clampé → écrit la valeur réellement codée dans *value).
				static void Encode(NkOpusRangeEncoder &enc, int32 *value, uint32 fs, int32 decay);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
