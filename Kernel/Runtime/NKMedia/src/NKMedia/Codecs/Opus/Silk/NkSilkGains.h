// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkGains.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — brique : GAINS de sous-trame (RFC 6716 §4.2.7.4).
// Port fidèle de libopus (silk/decode_indices.c section Gains, silk/gain_quant.c
// silk_gains_dequant, tables silk/tables_gain.c). Décode les index de gain via le
// range coder (codage indépendant/conditionnel + delta) puis les déquantifie en
// gains linéaires Q16 (échelle log uniforme + hystérésis inter-sous-trame).
// Aller-retour range-coder prouvé (SelfTest). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkSilkGains {
				// Codage conditionnel (RFC : condCoding). Valeurs libopus.
				static constexpr int32 kCodeIndependently = 0;
				static constexpr int32 kCodeConditionally = 2;

				// Décode les nb_subfr index de gain depuis le range decoder → ind[].
				// signalType ∈ {0,1,2} (choisit la table du 1er gain en indépendant).
				static void DecodeIndices(NkOpusRangeDecoder &dec, int8 ind[4], int32 signalType, int32 condCoding,
										  int32 nb_subfr);

				// Déquantifie ind[] → gain_Q16[] (linéaire Q16), en maintenant l'état
				// *prev_ind (hystérésis). conditional = condCoding (0 = indépendant).
				static void Dequant(int32 gain_Q16[4], const int8 ind[4], int8 *prev_ind, int32 conditional,
									int32 nb_subfr);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
