// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkExcitation.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — brique EXCITATION (signal résiduel, RFC 6716 §4.2.7.8).
// Port fidèle de libopus (decode_pulses.c, shell_coder.c, code_signs.c).
//   - niveau de débit (rate level) + nombre de pulses par bloc de 16 (+ extension
//     LSB) ; décodage shell (arbre binaire de splits) → amplitudes non signées ;
//     bits de poids faible ; puis signes → signal d'excitation signé.
// Tables générées depuis la référence (bit-exactes). Aller-retour prouvé.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		// Tables (définies dans NkSilkExcitationTables.cpp, générées depuis libopus).
		extern const uint8 kSilk_rate_levels_iCDF[2][9];
		extern const uint8 kSilk_pulses_per_block_iCDF[10][18];
		extern const uint8 kSilk_shell_code_table0[152];
		extern const uint8 kSilk_shell_code_table1[152];
		extern const uint8 kSilk_shell_code_table2[152];
		extern const uint8 kSilk_shell_code_table3[152];
		extern const uint8 kSilk_shell_code_table_offsets[17];
		extern const uint8 kSilk_sign_iCDF[42];
		extern const uint8 kSilk_lsb_iCDF[2];

		struct NkSilkExcitation {
				static constexpr int32 kShellFrameLen = 16;

				// Décode le signal d'excitation (pulses signés) d'une trame SILK.
				static void DecodePulses(NkOpusRangeDecoder &dec, int16 *pulses, int32 signalType,
										 int32 quantOffsetType, int32 frame_length);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
