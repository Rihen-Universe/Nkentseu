// =============================================================================
// NKMedia/Codecs/Aac/NkAacIcs.h
// -----------------------------------------------------------------------------
// Flux d'un canal AAC-LC (individual_channel_stream, ISO/IEC 14496-3 §4.4.2) :
// global_gain + ics_info (séquence/forme de fenêtre, max_sfb, regroupement) +
// section_data (codebook par bande) + scale_factor_data (facteurs différentiels,
// +PNS/intensity) + pulse/tns (bits consommés, appliqués plus tard) + spectral_data
// (coefficients quantifiés via Huffman). Un objet = l'état décodé d'UN canal pour
// une trame. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"

namespace nkentseu {
	namespace media {

		// Données TNS brutes (coefficients non décodés ; application en brique TNS).
		struct NkAacTnsData {
				uint8 nFilt[8];
				uint8 coefRes[8];
				uint8 length[8][4];
				uint8 order[8][4];
				uint8 direction[8][4];
				uint8 coefCompress[8][4];
				uint8 coef[8][4][32];
		};

		// Données de pulses (ajout d'amplitude sur quelques coefficients ; appliqué en dequant).
		struct NkAacPulseData {
				uint8 numberPulse;	  // + 1 = nombre réel
				uint8 pulseStartSfb;
				uint8 pulseOffset[4];
				uint8 pulseAmp[4];
		};

		struct NkAacIcs {
			public:
				static constexpr int32 kMaxSfb = 51;
				static constexpr int32 kMaxWindows = 8;
				static constexpr int32 kFrameLen = 1024;

				// --- ics_info ---
				int32 windowSequence = 0; // 0=ONLY_LONG 1=LONG_START 2=EIGHT_SHORT 3=LONG_STOP
				int32 windowShape = 0;	  // 0=sine 1=KBD
				int32 maxSfb = 0;
				int32 scaleFactorGrouping = 0;
				int32 numWindows = 1;
				int32 numWindowGroups = 1;
				int32 windowGroupLength[kMaxWindows] = {1};
				int32 numSwb = 0;
				uint16 swbOffset[64] = {0};			   // offsets de bande (0..numSwb) → coefficients
				uint16 sectSfbOffset[8][64] = {{0}};   // offset coefficient par groupe/bande

				// --- niveau élément ---
				int32 globalGain = 0;
				bool pulseDataPresent = false;
				bool tnsDataPresent = false;
				bool gainControlPresent = false;
				NkAacPulseData pulse = {};
				NkAacTnsData tns = {};

				// --- section_data ---
				int32 numSec[kMaxWindows] = {0};
				uint8 sectCb[8][kMaxSfb] = {{0}};
				uint8 sectStart[8][kMaxSfb] = {{0}};
				uint8 sectEnd[8][kMaxSfb] = {{0}};
				uint8 sfbCb[8][kMaxSfb] = {{0}}; // codebook par sfb (développé depuis les sections)

				// --- scale_factor_data ---
				int16 scaleFactors[8][kMaxSfb] = {{0}};

				// --- spectral_data : coefficients quantifiés signés (layout groupé) ---
				int32 specQuant[kFrameLen] = {0};

				// Parse le flux d'UN canal (SCE ; common_window=false). `sfIndex` =
				// sampling_frequency_index (0..11). Renvoie false si invalide / non-LC.
				bool ParseChannel(NkAacBitReader &br, int32 sfIndex);

				static bool SelfTest();

			private:
				bool ParseIcsInfo(NkAacBitReader &br, int32 sfIndex);
				void ComputeGrouping(int32 sfIndex);
				bool ParseSectionData(NkAacBitReader &br);
				bool ParseScaleFactorData(NkAacBitReader &br);
				void ParsePulseData(NkAacBitReader &br);
				void ParseTnsData(NkAacBitReader &br);
				void ParseSpectralData(NkAacBitReader &br);
		};

	} // namespace media
} // namespace nkentseu
