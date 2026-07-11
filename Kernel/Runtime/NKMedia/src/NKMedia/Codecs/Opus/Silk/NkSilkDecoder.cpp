// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkDecoder.cpp
// Port fidèle de libopus (decode_frame.c + decode_parameters.c).
// =============================================================================
#include "NkSilkDecoder.h"
#include "NkSilkMath.h"
#include "NkSilkGains.h"
#include "NkSilkNlsf.h"
#include "NkSilkLpc.h"
#include "NkSilkLtp.h"
#include "NkSilkFrameType.h"
#include "NkSilkExcitation.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		static constexpr int32 TYPE_VOICED = 2;

		void NkSilkDecoder::Init(int32 fs_kHz, int32 nb_subfr) {
			cfg.Set(fs_kHz, nb_subfr);
			core.Configure(fs_kHz, nb_subfr, cfg.LPC_order);
			ixState = NkSilkIndicesState{};
			LastGainIndex = 10;
			for (int32 j = 0; j < 16; j++)
				prevNLSF_Q15[j] = 0;
			first_frame_after_reset = 1;
		}

		// silk_decode_parameters
		void NkSilkDecoder::DecodeParameters(int32 condCoding) {
			// Gains (déquantification avec hystérésis).
			NkSilkGains::Dequant(Gains_Q16, ix.GainsIndices, &LastGainIndex, condCoding, cfg.nb_subfr);

			// NLSF → LPC (demi-trame 1).
			int16 pNLSF_Q15[16], pNLSF0_Q15[16];
			NkSilkNlsf::Decode(pNLSF_Q15, ix.NLSFIndices, cfg.nlsfCB);
			NkSilkLpc::nlsf2a(PredCoef_Q12[1], pNLSF_Q15, cfg.LPC_order);

			if (first_frame_after_reset)
				ix.NLSFInterpCoef_Q2 = 4;

			// Demi-trame 0 : interpolée depuis le NLSF précédent, ou copie.
			if (ix.NLSFInterpCoef_Q2 < (1 << 2)) {
				NkSilkFrameType::InterpolateNlsf(pNLSF0_Q15, prevNLSF_Q15, pNLSF_Q15, ix.NLSFInterpCoef_Q2,
												 cfg.LPC_order);
				NkSilkLpc::nlsf2a(PredCoef_Q12[0], pNLSF0_Q15, cfg.LPC_order);
			} else {
				for (int32 j = 0; j < cfg.LPC_order; j++)
					PredCoef_Q12[0][j] = PredCoef_Q12[1][j];
			}
			for (int32 j = 0; j < cfg.LPC_order; j++)
				prevNLSF_Q15[j] = pNLSF_Q15[j];

			// Pitch + LTP (voisé).
			if (ix.signalType == TYPE_VOICED) {
				NkSilkLtp::DecodePitch(ix.lagIndex, ix.contourIndex, pitchL, cfg.Fs_kHz, cfg.nb_subfr);
				NkSilkLtp::DecodeLtpGains(ix.PERIndex, ix.LTPIndex, LTPCoef_Q14, cfg.nb_subfr);
				LTP_scale_Q14 = NkSilkLtp::DecodeLtpScale(ix.LTP_scaleIndex);
			} else {
				for (int32 k = 0; k < cfg.nb_subfr; k++)
					pitchL[k] = 0;
				for (int32 j = 0; j < cfg.nb_subfr * 5; j++)
					LTPCoef_Q14[j] = 0;
				ix.PERIndex = 0;
				LTP_scale_Q14 = 0;
			}
		}

		// silk_decode_frame (chemin normal, sans PLC/CNG — décodage propre).
		void NkSilkDecoder::DecodeFrame(NkOpusRangeDecoder &dec, int16 *pOut, int32 vadFlag, int32 condCoding) {
			int16 pulses[384];

			NkSilkIndices::DecodeIndices(dec, ix, ixState, cfg, vadFlag, condCoding);
			NkSilkExcitation::DecodePulses(dec, pulses, ix.signalType, ix.quantOffsetType, core.frame_length);
			DecodeParameters(condCoding);

			// Synchronise les indices de trame vers l'état de synthèse.
			core.signalType = ix.signalType;
			core.quantOffsetType = ix.quantOffsetType;
			core.NLSFInterpCoef_Q2 = ix.NLSFInterpCoef_Q2;
			core.Seed = ix.Seed;

			NkSilkSynthesis::DecodeCore(core, PredCoef_Q12, LTPCoef_Q14, Gains_Q16, pitchL, LTP_scale_Q14, pulses,
										pOut);

			// Mise à jour du buffer de sortie (historique glissant).
			const int32 mv_len = core.ltp_mem_length - core.frame_length;
			for (int32 i = 0; i < mv_len; i++)
				core.outBuf[i] = core.outBuf[core.frame_length + i];
			for (int32 i = 0; i < core.frame_length; i++)
				core.outBuf[mv_len + i] = pOut[i];

			first_frame_after_reset = 0;
			core.prevSignalType = ix.signalType;
			core.lagPrev = pitchL[cfg.nb_subfr - 1];
		}

		// ── Self-test ────────────────────────────────────────────────────────────
		bool NkSilkDecoder::SelfTest() {
			// 1) decode_parameters : index valides → gains > 0, LPC STABLE, pitch borné.
			{
				NkSilkDecoder d;
				d.Init(16, 4); // WB, order 16
				d.ix.signalType = TYPE_VOICED;
				d.ix.quantOffsetType = 0;
				d.ix.NLSFInterpCoef_Q2 = 4;
				// Gains : 1er indépendant (index 0..63), suivants delta.
				d.ix.GainsIndices[0] = (int8)((3 << 3) + 4);
				d.ix.GainsIndices[1] = 20;
				d.ix.GainsIndices[2] = 20;
				d.ix.GainsIndices[3] = 20;
				for (int32 i = 0; i < 17; i++)
					d.ix.NLSFIndices[i] = 0; // CB1=0, résidus 0 → vecteur codebook
				d.ix.lagIndex = 80;
				d.ix.contourIndex = 1;
				d.ix.PERIndex = 1;
				d.ix.LTPIndex[0] = 3;
				d.ix.LTPIndex[1] = 4;
				d.ix.LTPIndex[2] = 5;
				d.ix.LTPIndex[3] = 6;
				d.ix.LTP_scaleIndex = 1;

				d.DecodeParameters(NkSilkIndices::kCodeIndependently);
				for (int32 k = 0; k < 4; k++)
					if (d.Gains_Q16[k] <= 0)
						return false;
				if (NkSilkLpc::lpcInversePredGain(d.PredCoef_Q12[1], 16) <= 0)
					return false;
				const int32 min_lag = 2 * 16, max_lag = 18 * 16;
				for (int32 k = 0; k < 4; k++)
					if (d.pitchL[k] < min_lag || d.pitchL[k] > max_lag)
						return false;
			}

			// 2) decode_frame : encode un flux de trame minimal (index non-voisés +
			//    pulses nuls), décode, vérifie la synchro (signalType recouvré) et le
			//    déterminisme (deux décodeurs → même PCM).
			{
				const int32 fs = 16, nb = 4;
				NkSilkFrameConfig cfg;
				cfg.Set(fs, nb);
				const int32 frame_length = fs * 5 * nb; // 320
				const int32 vad = 1, st = 1 /*non-voisé*/, qot = 0;

				uint8 buf[512];
				NkOpusRangeEncoder enc;
				enc.Init(buf, sizeof(buf));
				// (a) type non-voisé (VAD) : Ix=(st<<1)|qot=2, sym=Ix-2=0.
				{
					static const uint8 kVAD[4] = {232, 158, 10, 0};
					enc.EncodeIcdf(((st << 1) | qot) - 2, kVAD, 8);
				}
				// (b) gains indep.
				{
					static const uint8 kGainICDF[3][8] = {{224, 112, 44, 15, 3, 2, 1, 0},
														  {254, 237, 192, 132, 70, 23, 4, 0},
														  {255, 252, 226, 155, 61, 11, 2, 0}};
					static const uint8 kU8[8] = {224, 192, 160, 128, 96, 64, 32, 0};
					static const uint8 kDelta[41] = {250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27,
													 26,  25,  24,  23,  22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
													 12,  11,  10,  9,   8,  7,  6,  5,  4,  3,  2,  1,  0};
					enc.EncodeIcdf(3, kGainICDF[st], 8);
					enc.EncodeIcdf(4, kU8, 8);
					for (int32 k = 1; k < nb; k++)
						enc.EncodeIcdf((k * 7) % 41, kDelta, 8);
				}
				// (c) NLSF : CB1=0 + résidus 0.
				{
					enc.EncodeIcdf(0, &cfg.nlsfCB->CB1_iCDF[(st >> 1) * cfg.nlsfCB->nVectors], 8);
					for (int32 i = 0; i < cfg.nlsfCB->order; i += 2) {
						const uint8 entry = cfg.nlsfCB->ec_sel[(0 * cfg.nlsfCB->order / 2) + (i / 2)];
						enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(entry, 1) & 7, 9)], 8);
						enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(entry, 5) & 7, 9)], 8);
					}
				}
				// (d) interp factor (nb==4).
				{
					static const uint8 kInterp[5] = {243, 221, 192, 181, 0};
					enc.EncodeIcdf(2, kInterp, 8);
				}
				// (e) seed (non-voisé → pas de pitch/LTP).
				{
					static const uint8 kU4[4] = {192, 128, 64, 0};
					enc.EncodeIcdf(3, kU4, 8);
				}
				// (f) pulses : rate level + counts nuls (aucun pulse).
				{
					const int32 R = 3, iter = frame_length / 16;
					enc.EncodeIcdf(R, kSilk_rate_levels_iCDF[st >> 1], 8);
					for (int32 i = 0; i < iter; i++)
						enc.EncodeIcdf(0, kSilk_pulses_per_block_iCDF[R], 8);
				}
				enc.Done();

				int16 outA[320], outB[320];
				NkSilkDecoder da, db;
				da.Init(fs, nb);
				db.Init(fs, nb);
				NkOpusRangeDecoder decA, decB;
				decA.Init(buf, enc.RangeBytes());
				decB.Init(buf, enc.RangeBytes());
				da.DecodeFrame(decA, outA, vad, NkSilkIndices::kCodeIndependently);
				db.DecodeFrame(decB, outB, vad, NkSilkIndices::kCodeIndependently);

				if (da.ix.signalType != st || da.ix.Seed != 3)
					return false;
				for (int32 i = 0; i < frame_length; i++)
					if (outA[i] != outB[i])
						return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
