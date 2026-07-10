// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltVq.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — DÉCODAGE DE LA FORME DE BANDE (alg_unquant, vq.c) : décode
// le vecteur de pulses (PVQ/CWRS) → NORMALISE (norme unité × gain) → ROTATION de
// « spreading » (exp_rotation, réduit les artefacts tonals). La rotation est
// orthonormale (conserve la norme). Produit la forme spectrale normalisée `X` d'une
// bande. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltVq {
			public:
				// Décode la forme d'une bande : pulses (N dims, K pulses) → X normalisé (norme = gain),
				// avec spreading `spread` (0=aucun..3) et `B` blocs. Renvoie le masque de collapse.
				static uint32 AlgUnquant(NkOpusRangeDecoder &dec, float32 *X, int32 N, int32 K, int32 spread,
										 int32 B, float32 gain);

				// Rotation de spreading (exp_rotation), exposée pour test. dir=-1 (décodage).
				static void ExpRotation(float32 *X, int32 len, int32 dir, int32 stride, int32 K, int32 spread);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
