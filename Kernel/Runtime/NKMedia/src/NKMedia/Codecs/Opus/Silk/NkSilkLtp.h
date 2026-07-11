// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkLtp.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — brique LTP (prédiction long-terme, RFC 6716 §4.2.7.6).
// Port fidèle de libopus (decode_pitch.c, decode_parameters.c section LTP).
//   - DecodePitch : lagIndex + contourIndex → lags de pitch par sous-trame
//     (codebooks de contour selon Fs et durée de trame).
//   - DecodeLtpGains : PERIndex (périodicité) + LTPIndex[k] → coefficients du
//     filtre LTP 5-taps Q14 par sous-trame (VQ codebooks).
//   - DecodeLtpScale : index d'échelle → gain d'échelle LTP Q14.
// Tables pitch/LTP générées depuis la référence (bit-exactes). Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Accès aux tables pitch/LTP (définies dans NkSilkLtpTables.cpp).
		struct NkSilkLtpTables {
				// Codebook de contour aplati + taille (colonnes) selon Fs/nb_subfr.
				static const int8 *CbLags(int32 Fs_kHz, int32 nb_subfr, int32 *cbk_size);
				static const int8 *LtpVq(int32 perIndex);	 // codebook VQ Q7 (5 taps/entrée)
				static int32 LtpVqSize(int32 perIndex);		 // 8 / 16 / 32
				static int16 LtpScale(int32 idx);			 // gain d'échelle Q14
		};

		struct NkSilkLtp {
				static constexpr int32 kLtpOrder = 5;

				static void DecodePitch(int16 lagIndex, int8 contourIndex, int32 *pitch_lags, int32 Fs_kHz,
										int32 nb_subfr);
				static void DecodeLtpGains(int8 PERIndex, const int8 *LTPIndex, int16 *LTPCoef_Q14, int32 nb_subfr);
				static int16 DecodeLtpScale(int8 scaleIndex);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
