// =============================================================================
// NKMedia/Codecs/Aac/NkAacDecoder.h
// -----------------------------------------------------------------------------
// Décodeur AAC-LC complet (ISO/IEC 14496-3) — ASSEMBLAGE. Décode un raw_data_block
// (suite d'éléments SCE/CPE/LFE/DSE/PCE/FIL/END) en PCM, en enchaînant les briques :
// NkAacIcs (parse canal) → NkAacDequant (spectre) → NkAacFilterbank (IMDCT+overlap).
// Mono (SCE) ET stéréo (CPE : M/S, intensity stereo, PNS, TNS) opérationnels, validés
// bit-proche vs ffmpeg (corr 1.000000). Un paquet AAC (raw_data_block) = 1024
// échantillons/canal. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Aac/NkAacFilterbank.h"

namespace nkentseu {
	namespace media {

		struct NkAacDecoder {
			public:
				// `sampleRate` en Hz, `channels` (1 ou 2). Renvoie false si config non gérée.
				bool Init(int32 sampleRate, int32 channels);

				// Décode un raw_data_block (`data/len`) → `pcmOut` (int16 entrelacé, 1024
				// échantillons/canal). Renvoie le nombre d'échantillons par canal (0 si échec).
				int32 DecodeFrame(const uint8 *data, int32 len, int16 *pcmOut);

				static bool SelfTest();

			private:
				int32 mSfIndex = 3;
				int32 mChannels = 1;
				int32 mPrevWindowShape[2] = {0, 0};
				uint32 mRandomState = 0x1f2e3d4c; // état RNG PNS (LCG ffmpeg), persistant / trames+canaux
				NkAacFilterbank mFb[2];
		};

	} // namespace media
} // namespace nkentseu
