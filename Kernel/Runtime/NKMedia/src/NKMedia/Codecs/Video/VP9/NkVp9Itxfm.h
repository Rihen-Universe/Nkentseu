// =============================================================================
// NKMedia/Codecs/Video/VP9/NkVp9Itxfm.h
// -----------------------------------------------------------------------------
// Décodeur VP9 — TRANSFORMÉES INVERSES normatives (port fidèle de la logique de
// vpx_dsp/inv_txfm.c + vp9/common/vp9_idct.c, réécrit — zéro code importé) :
// iDCT 4/8/16/32, iADST 4/8/16 (transformées hybrides par type), iWHT 4x4
// (lossless). Arithmétique ENTIÈRE exacte : constantes cospi/sinpi Q14, arrondi
// dct_const_round_shift, stockage intermédiaire en int16 (wrap normatif).
// Chaque *_Add ajoute le résidu au bloc prédit (clamp 0-255).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkVp9Itxfm {
			public:
				// txType : 0=DCT_DCT, 1=ADST_DCT, 2=DCT_ADST, 3=ADST_ADST.
				static void Iht4x4Add(const int16 *input, uint8 *dest, int32 stride, int32 txType);
				static void Iht8x8Add(const int16 *input, uint8 *dest, int32 stride, int32 txType);
				static void Iht16x16Add(const int16 *input, uint8 *dest, int32 stride, int32 txType);
				static void Idct32x32Add(const int16 *input, uint8 *dest, int32 stride);
				// Lossless (WHT 4x4, coefficients pré-décalés de 2).
				static void Iwht4x4Add(const int16 *input, uint8 *dest, int32 stride);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
