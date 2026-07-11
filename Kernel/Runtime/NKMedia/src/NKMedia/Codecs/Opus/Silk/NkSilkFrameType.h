// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkFrameType.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — brique EN-TÊTE de trame (RFC 6716 §4.2.7.3 & §4.2.7.5.1).
// Port fidèle de libopus (decode_indices.c). Décode, en tête de trame SILK :
//   - le TYPE de trame : signalType (inactif/non-voisé/voisé) + quantOffsetType,
//     via la table VAD ou non-VAD selon l'activité vocale de la trame ;
//   - le facteur d'interpolation NLSF (trames 20 ms) ;
//   - la graine (seed) LCG de l'excitation.
// Fournit aussi l'interpolation NLSF (decode_parameters.c) entre trame
// précédente et courante. Aller-retour prouvé. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkSilkFrameType {
				// Décode signalType {0,1,2} + quantOffsetType {0,1} selon le flag VAD.
				static void DecodeType(NkOpusRangeDecoder &dec, int32 vadFlag, int32 *signalType,
									   int32 *quantOffsetType);

				// Facteur d'interpolation NLSF Q2 (0..4). nb_subfr==4 → décodé, sinon 4.
				static int32 DecodeNlsfInterpFactor(NkOpusRangeDecoder &dec, int32 nb_subfr);

				// Graine LCG de l'excitation (0..3).
				static int32 DecodeSeed(NkOpusRangeDecoder &dec);

				// Interpolation NLSF : xi = x0 + (ifact_Q2 * (x1 - x0)) >> 2. [d]
				static void InterpolateNlsf(int16 *xi, const int16 *x0, const int16 *x1, int32 ifact_Q2, int32 d);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
