// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkSynthesis.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK — cœur DSP (RFC 6716 §4.2.7.9). Port fidèle de libopus
// (decode_core.c silk_decode_core + LPC_analysis_filter.c). Transforme les
// paramètres décodés (excitation, gains, coefficients LPC & LTP, lags de pitch)
// en signal PCM : reconstruction de l'excitation (offset + graine LCG de signe),
// prédiction long-terme (voisé), filtre de synthèse LPC court-terme, mise à
// l'échelle par le gain. État persistant inter-trames (mémoire LTP, état LPC,
// historique de sortie pour le re-blanchiment). Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// État du décodeur SILK (buffers persistants + config de trame).
		struct NkSilkDecoderState {
				static constexpr int32 kMaxLpcOrder = 16;
				static constexpr int32 kMaxSubfrLength = 80;   // 16 kHz × 5 ms
				static constexpr int32 kMaxFrameLength = 320;  // 16 kHz × 20 ms
				static constexpr int32 kMaxLtpMemLength = 320; // 16 kHz × 20 ms

				int32 Fs_kHz = 16, nb_subfr = 4, LPC_order = 16;
				int32 subfr_length = 80, frame_length = 320, ltp_mem_length = 320;
				int32 prev_gain_Q16 = 65536, prevSignalType = 0, lagPrev = 100, lossCnt = 0;
				// Indices de la trame courante.
				int32 signalType = 0, quantOffsetType = 0, NLSFInterpCoef_Q2 = 4, Seed = 0;
				// Buffers persistants.
				int32 exc_Q14[kMaxFrameLength];
				int32 sLPC_Q14_buf[kMaxLpcOrder];
				int16 outBuf[kMaxLtpMemLength + kMaxFrameLength];

				void Configure(int32 fs_kHz, int32 nbSubfr, int32 lpcOrder);
				void Reset();
		};

		struct NkSilkSynthesis {
				static constexpr int32 kLtpOrder = 5;

				// Cœur : produit xq[frame_length] à partir des paramètres décodés.
				//   PredCoef_Q12[2][order] : LPC des 2 demi-trames.
				//   LTPCoef_Q14[nb_subfr*5], Gains_Q16[nb_subfr], pitchL[nb_subfr].
				static void DecodeCore(NkSilkDecoderState &s, const int16 PredCoef_Q12[2][16],
									   const int16 *LTPCoef_Q14, const int32 *Gains_Q16, const int32 *pitchL,
									   int32 LTP_scale_Q14, const int16 *pulses, int16 *xq);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
