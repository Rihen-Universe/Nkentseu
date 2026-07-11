// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkSynthesis.cpp
// Port fidèle de libopus (decode_core.c, LPC_analysis_filter.c).
// =============================================================================
#include "NkSilkSynthesis.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		static constexpr int32 MAX_LPC_ORDER = 16;
		static constexpr int32 LTP_ORDER = 5;
		static constexpr int32 QUANT_LEVEL_ADJUST_Q10 = 80;
		static constexpr int32 TYPE_VOICED = 2;

		// silk/tables_other.c : {UVL,UVH},{VL,VH} (offsets de quantification Q10).
		static const int16 kQuantOffsets_Q10[2][2] = {{100, 240}, {32, 100}};

		void NkSilkDecoderState::Configure(int32 fs_kHz, int32 nbSubfr, int32 lpcOrder) {
			Fs_kHz = fs_kHz;
			nb_subfr = nbSubfr;
			LPC_order = lpcOrder;
			subfr_length = fs_kHz * 5;
			frame_length = subfr_length * nbSubfr;
			ltp_mem_length = fs_kHz * 20;
			Reset();
		}

		void NkSilkDecoderState::Reset() {
			prev_gain_Q16 = 65536;
			prevSignalType = 0;
			lagPrev = 100;
			lossCnt = 0;
			for (int32 i = 0; i < kMaxLpcOrder; i++)
				sLPC_Q14_buf[i] = 0;
			for (int32 i = 0; i < kMaxFrameLength; i++)
				exc_Q14[i] = 0;
			for (int32 i = 0; i < kMaxLtpMemLength + kMaxFrameLength; i++)
				outBuf[i] = 0;
		}

		// silk_LPC_analysis_filter (chemin C portable, sans CELT-FIR).
		static void lpcAnalysisFilter(int16 *out, const int16 *in, const int16 *B, int32 len, int32 d) {
			for (int32 ix = d; ix < len; ix++) {
				const int16 *in_ptr = &in[ix - 1];
				int32 out32_Q12 = M::SMULBB(in_ptr[0], B[0]);
				out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-1], B[1]);
				out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-2], B[2]);
				out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-3], B[3]);
				out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-4], B[4]);
				out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-5], B[5]);
				for (int32 j = 6; j < d; j += 2) {
					out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-j], B[j]);
					out32_Q12 = M::SMLABB_ovflw(out32_Q12, in_ptr[-j - 1], B[j + 1]);
				}
				out32_Q12 = M::SUB32_ovflw(M::LSHIFT((int32)in_ptr[1], 12), out32_Q12);
				const int32 out32 = M::RSHIFT_ROUND(out32_Q12, 12);
				out[ix] = (int16)M::SAT16(out32);
			}
			for (int32 j = 0; j < d; j++)
				out[j] = 0;
		}

		void NkSilkSynthesis::DecodeCore(NkSilkDecoderState &s, const int16 PredCoef_Q12[2][16],
										 const int16 *LTPCoef_Q14, const int32 *Gains_Q16, const int32 *pitchL,
										 int32 LTP_scale_Q14, const int16 *pulses, int16 *xq) {
			int16 sLTP[NkSilkDecoderState::kMaxLtpMemLength];
			int32 sLTP_Q15[NkSilkDecoderState::kMaxLtpMemLength + NkSilkDecoderState::kMaxFrameLength];
			int32 res_Q14[NkSilkDecoderState::kMaxSubfrLength];
			int32 sLPC_Q14[NkSilkDecoderState::kMaxSubfrLength + MAX_LPC_ORDER];
			int16 A_Q12_tmp[MAX_LPC_ORDER];

			const int32 offset_Q10 = kQuantOffsets_Q10[s.signalType >> 1][s.quantOffsetType];
			const int32 NLSF_interpolation_flag = (s.NLSFInterpCoef_Q2 < (1 << 2)) ? 1 : 0;

			// Reconstruction de l'excitation (offset + graine LCG pour le signe).
			int32 rand_seed = s.Seed;
			for (int32 i = 0; i < s.frame_length; i++) {
				rand_seed = M::RAND(rand_seed);
				s.exc_Q14[i] = M::LSHIFT((int32)pulses[i], 14);
				if (s.exc_Q14[i] > 0) {
					s.exc_Q14[i] -= QUANT_LEVEL_ADJUST_Q10 << 4;
				} else if (s.exc_Q14[i] < 0) {
					s.exc_Q14[i] += QUANT_LEVEL_ADJUST_Q10 << 4;
				}
				s.exc_Q14[i] += offset_Q10 << 4;
				if (rand_seed < 0)
					s.exc_Q14[i] = -s.exc_Q14[i];
				rand_seed = M::ADD32_ovflw(rand_seed, pulses[i]);
			}

			for (int32 i = 0; i < MAX_LPC_ORDER; i++)
				sLPC_Q14[i] = s.sLPC_Q14_buf[i];

			int32 *pexc_Q14 = s.exc_Q14;
			int16 *pxq = xq;
			int32 sLTP_buf_idx = s.ltp_mem_length;
			int32 lag = 0;

			for (int32 k = 0; k < s.nb_subfr; k++) {
				int32 *pres_Q14 = res_Q14;
				const int16 *A_Q12 = PredCoef_Q12[k >> 1];
				for (int32 j = 0; j < s.LPC_order; j++)
					A_Q12_tmp[j] = A_Q12[j];
				const int16 *B_Q14 = &LTPCoef_Q14[k * LTP_ORDER];
				int32 signalType = s.signalType;

				const int32 Gain_Q10 = M::RSHIFT(Gains_Q16[k], 6);
				int32 inv_gain_Q31 = M::INVERSE32_varQ(Gains_Q16[k], 47);

				int32 gain_adj_Q16;
				if (Gains_Q16[k] != s.prev_gain_Q16) {
					gain_adj_Q16 = M::DIV32_varQ(s.prev_gain_Q16, Gains_Q16[k], 16);
					for (int32 i = 0; i < MAX_LPC_ORDER; i++)
						sLPC_Q14[i] = M::SMULWW(gain_adj_Q16, sLPC_Q14[i]);
				} else {
					gain_adj_Q16 = (int32)1 << 16;
				}
				s.prev_gain_Q16 = Gains_Q16[k];

				if (signalType == TYPE_VOICED) {
					lag = pitchL[k];
					if (k == 0 || (k == 2 && NLSF_interpolation_flag)) {
						const int32 start_idx = s.ltp_mem_length - lag - s.LPC_order - LTP_ORDER / 2;
						if (k == 2) {
							for (int32 i = 0; i < 2 * s.subfr_length; i++)
								s.outBuf[s.ltp_mem_length + i] = xq[i];
						}
						lpcAnalysisFilter(&sLTP[start_idx], &s.outBuf[start_idx + k * s.subfr_length], A_Q12,
										  s.ltp_mem_length - start_idx, s.LPC_order);
						if (k == 0)
							inv_gain_Q31 = M::LSHIFT(M::SMULWB(inv_gain_Q31, LTP_scale_Q14), 2);
						for (int32 i = 0; i < lag + LTP_ORDER / 2; i++)
							sLTP_Q15[sLTP_buf_idx - i - 1] = M::SMULWB(inv_gain_Q31, sLTP[s.ltp_mem_length - i - 1]);
					} else {
						if (gain_adj_Q16 != (int32)1 << 16)
							for (int32 i = 0; i < lag + LTP_ORDER / 2; i++)
								sLTP_Q15[sLTP_buf_idx - i - 1] =
									M::SMULWW(gain_adj_Q16, sLTP_Q15[sLTP_buf_idx - i - 1]);
					}
				}

				if (signalType == TYPE_VOICED) {
					int32 *pred_lag_ptr = &sLTP_Q15[sLTP_buf_idx - lag + LTP_ORDER / 2];
					for (int32 i = 0; i < s.subfr_length; i++) {
						int32 LTP_pred_Q13 = 2;
						LTP_pred_Q13 = M::SMLAWB(LTP_pred_Q13, pred_lag_ptr[0], B_Q14[0]);
						LTP_pred_Q13 = M::SMLAWB(LTP_pred_Q13, pred_lag_ptr[-1], B_Q14[1]);
						LTP_pred_Q13 = M::SMLAWB(LTP_pred_Q13, pred_lag_ptr[-2], B_Q14[2]);
						LTP_pred_Q13 = M::SMLAWB(LTP_pred_Q13, pred_lag_ptr[-3], B_Q14[3]);
						LTP_pred_Q13 = M::SMLAWB(LTP_pred_Q13, pred_lag_ptr[-4], B_Q14[4]);
						pred_lag_ptr++;
						pres_Q14[i] = M::ADD_LSHIFT32(pexc_Q14[i], LTP_pred_Q13, 1);
						sLTP_Q15[sLTP_buf_idx] = M::LSHIFT(pres_Q14[i], 1);
						sLTP_buf_idx++;
					}
				} else {
					pres_Q14 = pexc_Q14;
				}

				for (int32 i = 0; i < s.subfr_length; i++) {
					int32 LPC_pred_Q10 = M::RSHIFT(s.LPC_order, 1);
					for (int32 j = 0; j < s.LPC_order; j++)
						LPC_pred_Q10 = M::SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 1 - j], A_Q12_tmp[j]);

					sLPC_Q14[MAX_LPC_ORDER + i] = M::ADD_SAT32(pres_Q14[i], M::LSHIFT_SAT32(LPC_pred_Q10, 4));
					pxq[i] = (int16)M::SAT16(
						M::RSHIFT_ROUND(M::SMULWW(sLPC_Q14[MAX_LPC_ORDER + i], Gain_Q10), 8));
				}

				for (int32 i = 0; i < MAX_LPC_ORDER; i++)
					sLPC_Q14[i] = sLPC_Q14[s.subfr_length + i];
				pexc_Q14 += s.subfr_length;
				pxq += s.subfr_length;
			}

			for (int32 i = 0; i < MAX_LPC_ORDER; i++)
				s.sLPC_Q14_buf[i] = sLPC_Q14[i];
		}

		bool NkSilkSynthesis::SelfTest() {
			// 1) Cas analytique : trame NON-VOISÉE, LPC nul → la sortie doit valoir
			//    exactement le signal d'excitation (offset + biais LPC) mis à
			//    l'échelle par le gain. Recalcul indépendant → comparaison exacte.
			{
				NkSilkDecoderState s;
				s.Configure(16, 2, 10);
				s.signalType = 1; // non-voisé (≠ TYPE_VOICED)
				s.quantOffsetType = 0;
				s.NLSFInterpCoef_Q2 = 4;
				s.Seed = 0;
				const int32 G = 200000; // Q16
				int16 PredCoef[2][16] = {{0}};
				int16 LTPCoef[4 * 5] = {0};
				int32 Gains[2] = {G, G};
				int32 pitchL[2] = {0, 0};
				int16 pulses[320] = {0};
				int16 xq[320] = {0};
				NkSilkSynthesis::DecodeCore(s, PredCoef, LTPCoef, Gains, pitchL, 0, pulses, xq);

				// Recalcul de référence.
				const int32 offset = kQuantOffsets_Q10[1 >> 1][0]; // = 100
				const int32 Gain_Q10 = M::RSHIFT(G, 6);
				const int32 bias = M::LSHIFT_SAT32(M::RSHIFT(10, 1), 4); // LPC nul → biais order/2 << 4
				int32 rand_seed = 0;
				for (int32 i = 0; i < s.frame_length; i++) {
					rand_seed = M::RAND(rand_seed);
					int32 exc = 0;			  // pulses = 0
					exc += offset << 4;
					if (rand_seed < 0)
						exc = -exc;
					rand_seed = M::ADD32_ovflw(rand_seed, 0);
					const int32 sLPC = M::ADD_SAT32(exc, bias);
					const int16 expected = (int16)M::SAT16(M::RSHIFT_ROUND(M::SMULWW(sLPC, Gain_Q10), 8));
					if (xq[i] != expected)
						return false;
				}
			}

			// 2) Déterminisme : deux exécutions (état ré-initialisé) → sortie identique,
			//    chemin VOISÉ (LTP actif), sortie bornée.
			{
				int16 PredCoef[2][16] = {{0}};
				for (int32 h = 0; h < 2; ++h)
					for (int32 j = 0; j < 16; ++j)
						PredCoef[h][j] = (int16)((j == 0) ? 1500 : 0); // 1-pôle léger
				int16 LTPCoef[4 * 5];
				for (int32 j = 0; j < 20; ++j)
					LTPCoef[j] = (int16)(j % 5 == 2 ? 8000 : 1000);
				int32 Gains[4] = {150000, 150000, 150000, 150000};
				int32 pitchL[4] = {100, 100, 100, 100};
				int16 pulses[320];
				for (int32 i = 0; i < 320; ++i)
					pulses[i] = (int16)((i % 17 == 0) ? 3 : (i % 31 == 0 ? -2 : 0));

				int16 xqA[320] = {0}, xqB[320] = {0};
				NkSilkDecoderState a;
				a.Configure(16, 4, 16);
				a.signalType = TYPE_VOICED;
				a.quantOffsetType = 1;
				a.NLSFInterpCoef_Q2 = 4;
				a.Seed = 7;
				NkSilkSynthesis::DecodeCore(a, PredCoef, LTPCoef, Gains, pitchL, 12288, pulses, xqA);

				NkSilkDecoderState b;
				b.Configure(16, 4, 16);
				b.signalType = TYPE_VOICED;
				b.quantOffsetType = 1;
				b.NLSFInterpCoef_Q2 = 4;
				b.Seed = 7;
				NkSilkSynthesis::DecodeCore(b, PredCoef, LTPCoef, Gains, pitchL, 12288, pulses, xqB);

				for (int32 i = 0; i < 320; ++i)
					if (xqA[i] != xqB[i])
						return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
