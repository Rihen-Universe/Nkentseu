// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkTop.cpp
// Port fidèle de libopus (dec_API.c silk_Decode — chemin mono, sans FEC).
// =============================================================================
#include "NkSilkTop.h"
#include "NkSilkMath.h"
#include "NkSilkExcitation.h" // tables pulses pour le self-test

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		namespace {
			// Tables stéréo (tables_other.c) — petites, recopiées vérifiées à la main.
			const int16 kStereoPredQuantQ13[16] = {-13732, -10050, -8266, -7526, -6500, -5000, -2950, -820,
												   820,	   2950,   5000,  6500,	 7526,	8266,  10050, 13732};
			const uint8 kStereoPredJointIcdf[25] = {249, 247, 246, 245, 244, 234, 210, 202, 201, 200, 197, 174, 82,
													59,	 56,  55,  54,	46,	 22,  12,  11,	10,	 9,	  7,   0};
			const uint8 kStereoOnlyCodeMidIcdf[2] = {64, 0};
			const uint8 kUniform3Icdf[3] = {171, 85, 0};
			const uint8 kUniform5Icdf[5] = {205, 154, 102, 51, 0};

			constexpr int32 kStereoInterpLenMs = 8;	  // STEREO_INTERP_LEN_MS
			constexpr int32 kStereoQuantSubSteps = 5; // STEREO_QUANT_SUB_STEPS

			// silk_stereo_decode_pred : poids de prédiction mid/side (Q13).
			void StereoDecodePred(NkOpusRangeDecoder &rd, int32 predQ13[2]) {
				int32 ix[2][3];
				const int32 n = (int32)rd.DecodeIcdf(kStereoPredJointIcdf, 8);
				ix[0][2] = n / 5;
				ix[1][2] = n - 5 * ix[0][2];
				for (int32 c = 0; c < 2; c++) {
					ix[c][0] = (int32)rd.DecodeIcdf(kUniform3Icdf, 8);
					ix[c][1] = (int32)rd.DecodeIcdf(kUniform5Icdf, 8);
				}
				for (int32 c = 0; c < 2; c++) {
					ix[c][0] += 3 * ix[c][2];
					const int32 lowQ13 = kStereoPredQuantQ13[ix[c][0]];
					// SILK_FIX_CONST(0.5/5, 16) = 6554.
					const int32 stepQ13 = M::SMULWB(kStereoPredQuantQ13[ix[c][0] + 1] - lowQ13, 6554);
					predQ13[c] = lowQ13 + stepQ13 * (2 * ix[c][1] + 1);
				}
				// Soustrait le 2e prédicteur du 1er (simplifie l'application).
				predQ13[0] -= predQ13[1];
				(void)kStereoQuantSubSteps;
			}

			// silk_stereo_MS_to_LR : mid/side → gauche/droite, avec interpolation des
			// prédicteurs sur 8 ms et tampons 2 échantillons (x1/x2 : frame_length+2).
			void StereoMsToLr(int16 *x1, int16 *x2, int16 sMid[2], int16 sSide[2], int32 predPrevQ13[2],
							  const int32 predQ13[2], int32 fsKHz, int32 frameLength) {
				// Tampons : 2 échantillons précédents en tête, sauvegarde des 2 derniers.
				x1[0] = sMid[0];
				x1[1] = sMid[1];
				x2[0] = sSide[0];
				x2[1] = sSide[1];
				sMid[0] = x1[frameLength];
				sMid[1] = x1[frameLength + 1];
				sSide[0] = x2[frameLength];
				sSide[1] = x2[frameLength + 1];

				// Interpolation des prédicteurs + prédiction ajoutée au side.
				int32 pred0Q13 = predPrevQ13[0];
				int32 pred1Q13 = predPrevQ13[1];
				const int32 denomQ16 = M::DIV32_16((int32)1 << 16, kStereoInterpLenMs * fsKHz);
				const int32 delta0Q13 = M::RSHIFT_ROUND(M::SMULBB(predQ13[0] - predPrevQ13[0], denomQ16), 16);
				const int32 delta1Q13 = M::RSHIFT_ROUND(M::SMULBB(predQ13[1] - predPrevQ13[1], denomQ16), 16);
				int32 n = 0;
				for (; n < kStereoInterpLenMs * fsKHz; n++) {
					pred0Q13 += delta0Q13;
					pred1Q13 += delta1Q13;
					int32 sum = M::LSHIFT(M::ADD_LSHIFT32(x1[n] + (int32)x1[n + 2], x1[n + 1], 1), 9); // Q11
					sum = M::SMLAWB(M::LSHIFT((int32)x2[n + 1], 8), sum, pred0Q13);					   // Q8
					sum = M::SMLAWB(sum, M::LSHIFT((int32)x1[n + 1], 11), pred1Q13);				   // Q8
					x2[n + 1] = (int16)M::SAT16(M::RSHIFT_ROUND(sum, 8));
				}
				pred0Q13 = predQ13[0];
				pred1Q13 = predQ13[1];
				for (; n < frameLength; n++) {
					int32 sum = M::LSHIFT(M::ADD_LSHIFT32(x1[n] + (int32)x1[n + 2], x1[n + 1], 1), 9); // Q11
					sum = M::SMLAWB(M::LSHIFT((int32)x2[n + 1], 8), sum, pred0Q13);					   // Q8
					sum = M::SMLAWB(sum, M::LSHIFT((int32)x1[n + 1], 11), pred1Q13);				   // Q8
					x2[n + 1] = (int16)M::SAT16(M::RSHIFT_ROUND(sum, 8));
				}
				predPrevQ13[0] = predQ13[0];
				predPrevQ13[1] = predQ13[1];

				// mid/side → gauche/droite.
				for (n = 0; n < frameLength; n++) {
					const int32 sum = x1[n + 1] + (int32)x2[n + 1];
					const int32 diff = x1[n + 1] - (int32)x2[n + 1];
					x1[n + 1] = (int16)M::SAT16(sum);
					x2[n + 1] = (int16)M::SAT16(diff);
				}
			}
		} // namespace

		void NkSilkTop::Init(int32 fs_kHz, int32 nb_subfr, int32 nChannels) {
			dec.Init(fs_kHz, nb_subfr);
			stereo = (nChannels == 2) ? 1 : 0;
			if (stereo) {
				decSide.Init(fs_kHz, nb_subfr);
				predPrevQ13[0] = predPrevQ13[1] = 0;
				sMid[0] = sMid[1] = 0;
				sSide[0] = sSide[1] = 0;
				prevDecodeOnlyMiddle = 0;
			}
		}

		int32 NkSilkTop::DecodePacket(NkOpusRangeDecoder &rd, int32 nFramesPerPacket, int16 *out) {
			// En-tête couche LP : flags VAD (1 bit/trame) + flag LBRR (1 bit), ×2 en stéréo.
			int32 VAD_flags[3] = {0, 0, 0};
			int32 VAD_side[3] = {0, 0, 0};
			for (int32 i = 0; i < nFramesPerPacket; i++)
				VAD_flags[i] = rd.DecodeBitLogp(1);
			const int32 LBRR_flag = rd.DecodeBitLogp(1);
			(void)LBRR_flag; // flux sans FEC : pas de trames LBRR à sauter
			if (stereo) {
				for (int32 i = 0; i < nFramesPerPacket; i++)
					VAD_side[i] = rd.DecodeBitLogp(1);
				const int32 LBRR_side = rd.DecodeBitLogp(1);
				(void)LBRR_side;
			}

			int32 nOut = 0;
			const int32 flen = dec.core.frame_length;
			for (int32 i = 0; i < nFramesPerPacket; i++) {
				const int32 condCoding =
					(i == 0) ? NkSilkIndices::kCodeIndependently : NkSilkIndices::kCodeConditionally;

				if (!stereo) {
					dec.DecodeFrame(rd, out + nOut, VAD_flags[i], condCoding);
					nOut += flen;
					continue;
				}

				// --- STÉRÉO (dec_API.c silk_Decode, une trame par itération) ---
				int32 predQ13[2];
				StereoDecodePred(rd, predQ13);
				int32 onlyMid = 0;
				if (VAD_side[i] == 0)
					onlyMid = (int32)rd.DecodeIcdf(kStereoOnlyCodeMidIcdf, 8);

				// Reprise du side après une période mid-only : reset PARTIEL du canal side
				// (outBuf, sLPC, lagPrev, LastGainIndex, prevSignalType, first_frame) —
				// l'état entropique (ec_prev*) et prevNLSF ne sont PAS touchés (libopus).
				if (onlyMid == 0 && prevDecodeOnlyMiddle == 1) {
					for (int32 k = 0;
						 k < NkSilkDecoderState::kMaxLtpMemLength + NkSilkDecoderState::kMaxFrameLength; ++k)
						decSide.core.outBuf[k] = 0;
					for (int32 k = 0; k < NkSilkDecoderState::kMaxLpcOrder; ++k)
						decSide.core.sLPC_Q14_buf[k] = 0;
					decSide.core.lagPrev = 100;
					decSide.LastGainIndex = 10;
					decSide.core.prevSignalType = 0; // TYPE_NO_VOICE_ACTIVITY
					decSide.first_frame_after_reset = 1;
				}

				// Tampons trame + 2 échantillons d'état en tête (MS_to_LR lit x[n+2]).
				int16 x1[2 + NkSilkDecoderState::kMaxFrameLength];
				int16 x2[2 + NkSilkDecoderState::kMaxFrameLength];

				// Canal mid.
				dec.DecodeFrame(rd, &x1[2], VAD_flags[i], condCoding);
				// Canal side (ou silence si mid-only).
				if (!onlyMid) {
					const int32 ccSide = (i == 0) ? NkSilkIndices::kCodeIndependently
										 : (prevDecodeOnlyMiddle
												? NkSilkIndices::kCodeIndependentlyNoLtpScaling
												: NkSilkIndices::kCodeConditionally);
					decSide.DecodeFrame(rd, &x2[2], VAD_side[i], ccSide);
				} else {
					for (int32 k = 0; k < flen; ++k)
						x2[2 + k] = 0;
				}

				StereoMsToLr(x1, x2, sMid, sSide, predPrevQ13, predQ13, dec.core.Fs_kHz, flen);
				prevDecodeOnlyMiddle = onlyMid;

				// Sortie : (flen+2) gauche puis (flen+2) droite (cf. NkSilkTop.h).
				for (int32 k = 0; k < flen + 2; ++k)
					out[nOut + k] = x1[k];
				nOut += flen + 2;
				for (int32 k = 0; k < flen + 2; ++k)
					out[nOut + k] = x2[k];
				nOut += flen + 2;
			}
			return nOut;
		}

		// ── Self-test : en-tête (VAD/LBRR) + trame(s) encodées → décode paquet ───
		namespace {
			// Encode une trame NON-VOISÉE (index + pulses nuls) dans l'ordre du flux.
			void encodeUnvoicedFrame(NkOpusRangeEncoder &enc, const NkSilkFrameConfig &cfg, int32 st, int32 qot,
									 int32 vad, int32 condIndep) {
				static const uint8 kVAD[4] = {232, 158, 10, 0};
				static const uint8 kNoVAD[2] = {230, 0};
				static const uint8 kGainICDF[3][8] = {{224, 112, 44, 15, 3, 2, 1, 0},
													  {254, 237, 192, 132, 70, 23, 4, 0},
													  {255, 252, 226, 155, 61, 11, 2, 0}};
				static const uint8 kU8[8] = {224, 192, 160, 128, 96, 64, 32, 0};
				static const uint8 kDelta[41] = {250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27,
												 26,  25,  24,  23,  22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
												 12,  11,  10,  9,   8,  7,  6,  5,  4,  3,  2,  1,  0};
				static const uint8 kInterp[5] = {243, 221, 192, 181, 0};
				static const uint8 kU4[4] = {192, 128, 64, 0};

				const int32 Ix = (st << 1) | qot;
				enc.EncodeIcdf(vad ? (Ix - 2) : Ix, vad ? kVAD : kNoVAD, 8);
				// gains
				enc.EncodeIcdf(3, kGainICDF[st], 8);
				enc.EncodeIcdf(4, kU8, 8);
				for (int32 k = 1; k < cfg.nb_subfr; k++)
					enc.EncodeIcdf((k * 7) % 41, kDelta, 8);
				// NLSF (CB1=0, résidus 0)
				enc.EncodeIcdf(0, &cfg.nlsfCB->CB1_iCDF[(st >> 1) * cfg.nlsfCB->nVectors], 8);
				for (int32 i = 0; i < cfg.nlsfCB->order; i += 2) {
					const uint8 e = cfg.nlsfCB->ec_sel[i / 2];
					enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(e, 1) & 7, 9)], 8);
					enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(e, 5) & 7, 9)], 8);
				}
				// interp (nb==4) + seed
				if (cfg.nb_subfr == 4)
					enc.EncodeIcdf(2, kInterp, 8);
				enc.EncodeIcdf(3, kU4, 8);
				(void)condIndep;
				// pulses : rate level + counts nuls
				const int32 R = 3, frame_length = cfg.Fs_kHz * 5 * cfg.nb_subfr, iter = frame_length / 16;
				enc.EncodeIcdf(R, kSilk_rate_levels_iCDF[st >> 1], 8);
				for (int32 b = 0; b < iter; b++)
					enc.EncodeIcdf(0, kSilk_pulses_per_block_iCDF[R], 8);
			}
		} // namespace

		bool NkSilkTop::SelfTest() {
			const int32 fs = 16, nb = 4, nFrames = 1;
			NkSilkFrameConfig cfg;
			cfg.Set(fs, nb);
			const int32 frame_length = fs * 5 * nb;

			uint8 buf[512];
			NkOpusRangeEncoder enc;
			enc.Init(buf, sizeof(buf));
			// En-tête : VAD (1 trame) + LBRR=0.
			enc.EncodeBitLogp(1, 1); // VAD flag trame 0
			enc.EncodeBitLogp(0, 1); // LBRR flag
			encodeUnvoicedFrame(enc, cfg, /*st*/ 1, /*qot*/ 0, /*vad*/ 1, /*indep*/ 1);
			enc.Done();

			int16 outA[320], outB[320];
			NkSilkTop ta, tb;
			ta.Init(fs, nb);
			tb.Init(fs, nb);
			NkOpusRangeDecoder rdA, rdB;
			rdA.Init(buf, enc.RangeBytes());
			rdB.Init(buf, enc.RangeBytes());
			const int32 nA = ta.DecodePacket(rdA, nFrames, outA);
			const int32 nB = tb.DecodePacket(rdB, nFrames, outB);

			if (nA != frame_length || nB != frame_length)
				return false;
			if (ta.dec.ix.signalType != 1) // VAD → non-voisé recouvré
				return false;
			for (int32 i = 0; i < frame_length; i++)
				if (outA[i] != outB[i])
					return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
