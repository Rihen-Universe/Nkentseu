// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkDecoder.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — ASSEMBLAGE d'une trame (RFC 6716 §4.2.7). Port fidèle de
// libopus (decode_frame.c + decode_parameters.c). Relie les briques prouvées :
//   decode_indices → decode_pulses → decode_parameters (gains déquant + NLSF→LPC
//   + interpolation + pitch/LTP) → decode_core (synthèse) → mise à jour outBuf.
// Détient tout l'état persistant du décodeur SILK mono. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NkSilkSynthesis.h" // NkSilkDecoderState
#include "NkSilkIndices.h"	 // NkSilkIndicesData / State / FrameConfig
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkSilkDecoder {
				// État persistant.
				NkSilkDecoderState core;   // buffers de synthèse (LTP mem, état LPC, outBuf)
				NkSilkIndicesState ixState; // ec_prevSignalType / ec_prevLagIndex
				NkSilkFrameConfig cfg;		// tables selon Fs
				NkSilkIndicesData ix;		// index de la trame courante
				int8 LastGainIndex = 10;
				int16 prevNLSF_Q15[16] = {0};
				int32 first_frame_after_reset = 1;

				// Contrôle décodé (par trame).
				int16 PredCoef_Q12[2][16] = {{0}};
				int16 LTPCoef_Q14[4 * 5] = {0};
				int32 Gains_Q16[4] = {0};
				int32 pitchL[4] = {0};
				int32 LTP_scale_Q14 = 0;

				void Init(int32 fs_kHz, int32 nb_subfr);
				void DecodeParameters(int32 condCoding);
				// Décode une trame SILK complète depuis le range decoder → PCM interne.
				void DecodeFrame(NkOpusRangeDecoder &dec, int16 *pOut, int32 vadFlag, int32 condCoding);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
