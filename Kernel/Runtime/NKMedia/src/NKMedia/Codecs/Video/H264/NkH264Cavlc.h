// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Cavlc.h
// -----------------------------------------------------------------------------
// H.264 CAVLC (Context-Adaptive Variable-Length Coding, §9.2) — codage entropique
// des coefficients résiduels d'un bloc 4×4 : coeff_token (total_coeff/trailing_ones
// selon le contexte nC), signes des trailing ones, niveaux (prefix/suffix adaptatif),
// total_zeros, run_before. Tables du standard (décodées depuis minih264, vérifiées).
// Réécrit à la sauce Nkentseu. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Video/H264/NkH264BitWriter.h"

namespace nkentseu {
	namespace media {

		struct NkH264Cavlc {
			public:
				// Encode un bloc résiduel. `coef` = coefficients en ordre de BALAYAGE (zig-zag),
				// `maxNumCoeff` = 16 (bloc complet / DC luma), 15 (AC de l'Intra_16×16), 4 (DC chroma 2×2).
				// `nC` = contexte (moyenne des coeffs non nuls voisins) ; nC = -1 pour le DC chroma.
				// Renvoie total_coeff (pour la mise à jour du contexte nC des voisins).
				static int32 EncodeResidual(NkH264BitWriter &bs, const int32 *coef, int32 maxNumCoeff, int32 nC);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
