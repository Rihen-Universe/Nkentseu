// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DEEMPHASIS (celt_decoder.c) : dernier étage de sortie qui
// annule la pré-emphase appliquée à l'encodage. Filtre IIR 1-pôle (intégrateur à
// fuite) de coefficient `coef0` (0.85 à 48 kHz). Aller-retour préemphase→deemphasis
// = identité, prouvé. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkCeltDeemphasis {
			public:
				static constexpr float32 kPreemphCoef48k = 0.8500061f;

				// Applique le deemphasis (décodage) : out[n] = in[n] + coef·out[n-1]. `mem` = état (à conserver
				// entre trames), initialisé à 0. Écrit `out` (peut = in).
				static void Apply(const float32 *in, float32 *out, int32 N, float32 coef, float32 *mem);

				// Pré-emphase (encodage, pour test) : out[n] = in[n] − coef·in[n-1]. `mem` = in[n-1].
				static void Preemph(const float32 *in, float32 *out, int32 N, float32 coef, float32 *mem);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
