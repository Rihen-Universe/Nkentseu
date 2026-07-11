// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkIndices.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — ASSEMBLAGE des index de trame (RFC 6716 §4.2.7). Port
// fidèle de libopus (decode_indices.c complet + decoder_set_fs.c pour la
// sélection des tables selon Fs). Lit dans l'ordre exact du flux : type de trame
// → index de gains → index NLSF → facteur d'interp → (si voisé) lag de pitch +
// contour + gains LTP + échelle → graine. Aller-retour prouvé. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"
#include "NkSilkNlsf.h" // NkSilkNlsfCB + codebooks

namespace nkentseu {
	namespace media {

		// Tables d'index (définies dans NkSilkIndicesTables.cpp, générées depuis libopus).
		extern const uint8 kSilk_pitch_lag_iCDF[32];
		extern const uint8 kSilk_pitch_delta_iCDF[21];
		extern const uint8 kSilk_pitch_contour_iCDF[34];
		extern const uint8 kSilk_pitch_contour_NB_iCDF[11];
		extern const uint8 kSilk_pitch_contour_10_ms_iCDF[12];
		extern const uint8 kSilk_pitch_contour_10_ms_NB_iCDF[3];
		extern const uint8 kSilk_LTP_per_index_iCDF[3];
		extern const uint8 kSilk_LTP_gain_iCDF_0[8];
		extern const uint8 kSilk_LTP_gain_iCDF_1[16];
		extern const uint8 kSilk_LTP_gain_iCDF_2[32];
		extern const uint8 *const kSilk_LTP_gain_iCDF_ptrs[3];
		extern const uint8 kSilk_LTPscale_iCDF[3];
		extern const uint8 kSilk_uniform4_iCDF[4];
		extern const uint8 kSilk_uniform6_iCDF[6];
		extern const uint8 kSilk_uniform8_iCDF[8];

		// Tous les index décodés d'une trame SILK.
		struct NkSilkIndicesData {
				int32 signalType = 0, quantOffsetType = 0;
				int8 GainsIndices[4] = {0, 0, 0, 0};
				int8 NLSFIndices[17] = {0};
				int32 NLSFInterpCoef_Q2 = 4;
				int16 lagIndex = 0;
				int8 contourIndex = 0;
				int8 PERIndex = 0;
				int8 LTPIndex[4] = {0, 0, 0, 0};
				int8 LTP_scaleIndex = 0;
				int32 Seed = 0;
		};

		// État inter-trames pour le codage delta du lag (decode_indices.c).
		struct NkSilkIndicesState {
				int32 ec_prevSignalType = 0;
				int32 ec_prevLagIndex = 0;
		};

		// Config de trame (tables sélectionnées selon Fs/nb_subfr — decoder_set_fs.c).
		struct NkSilkFrameConfig {
				int32 Fs_kHz = 16, nb_subfr = 4, LPC_order = 16;
				const NkSilkNlsfCB *nlsfCB = nullptr;
				const uint8 *pitch_contour_iCDF = nullptr;
				const uint8 *pitch_lag_low_bits_iCDF = nullptr;
				void Set(int32 fs_kHz, int32 nbSubfr);
		};

		struct NkSilkIndices {
				static constexpr int32 kCodeIndependently = 0;
				static constexpr int32 kCodeConditionally = 2;
				static constexpr int32 kTypeVoiced = 2;

				static void DecodeIndices(NkOpusRangeDecoder &dec, NkSilkIndicesData &ix, NkSilkIndicesState &st,
										  const NkSilkFrameConfig &cfg, int32 vadFlag, int32 condCoding);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
