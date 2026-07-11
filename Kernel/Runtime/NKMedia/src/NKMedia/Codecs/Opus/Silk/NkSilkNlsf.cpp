// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkNlsf.cpp
// Port fidèle de libopus (NLSF_decode.c, NLSF_unpack.c, NLSF_stabilize.c,
// decode_indices.c section LSF).
// =============================================================================
#include "NkSilkNlsf.h"
#include "NkSilkMath.h"
#include "NkSilkLpc.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		static constexpr int32 NLSF_QUANT_MAX_AMPLITUDE = 4;
		static constexpr double NLSF_QUANT_LEVEL_ADJ = 0.1;

		// libopus silk/tables_other.c
		static const uint8 kNLSF_EXT_iCDF[7] = {100, 40, 16, 7, 3, 1, 0};

		// silk_insertion_sort_increasing_all_values_int16 (repli de stabilize).
		static void insertionSortInc(int16 *a, int32 L) {
			for (int32 i = 1; i < L; i++) {
				const int16 v = a[i];
				int32 j = i - 1;
				while (j >= 0 && a[j] > v) {
					a[j + 1] = a[j];
					j--;
				}
				a[j + 1] = v;
			}
		}

		// silk_NLSF_unpack : indices d'entropie + prédicteurs pour l'index CB1.
		static void unpack(int16 *ec_ix, uint8 *pred_Q8, const NkSilkNlsfCB *cb, int32 CB1_index) {
			const uint8 *ec_sel_ptr = &cb->ec_sel[CB1_index * cb->order / 2];
			for (int32 i = 0; i < cb->order; i += 2) {
				const uint8 entry = *ec_sel_ptr++;
				ec_ix[i] = M::SMULBB(M::RSHIFT(entry, 1) & 7, 2 * NLSF_QUANT_MAX_AMPLITUDE + 1);
				pred_Q8[i] = cb->pred_Q8[i + (entry & 1) * (cb->order - 1)];
				ec_ix[i + 1] = M::SMULBB(M::RSHIFT(entry, 5) & 7, 2 * NLSF_QUANT_MAX_AMPLITUDE + 1);
				pred_Q8[i + 1] = cb->pred_Q8[i + (M::RSHIFT(entry, 4) & 1) * (cb->order - 1) + 1];
			}
		}

		// silk_NLSF_residual_dequant : déquantiseur prédictif des résidus NLSF.
		static void residualDequant(int16 *x_Q10, const int8 *indices, const uint8 *pred_coef_Q8,
									int32 quant_step_size_Q16, int16 order) {
			int32 out_Q10 = 0;
			for (int32 i = order - 1; i >= 0; i--) {
				const int32 pred_Q10 = M::RSHIFT(M::SMULBB(out_Q10, (int16)pred_coef_Q8[i]), 8);
				out_Q10 = M::LSHIFT(indices[i], 10);
				if (out_Q10 > 0) {
					out_Q10 = M::SUB16(out_Q10, M::FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
				} else if (out_Q10 < 0) {
					out_Q10 = M::ADD16(out_Q10, M::FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
				}
				out_Q10 = M::SMLAWB(pred_Q10, out_Q10, quant_step_size_Q16);
				x_Q10[i] = (int16)out_Q10;
			}
		}

		void NkSilkNlsf::DecodeIndices(NkOpusRangeDecoder &dec, int8 *NLSFIndices, const NkSilkNlsfCB *cb,
									   int32 signalType) {
			int16 ec_ix[16];
			uint8 pred_Q8[16];
			// Étage 1 : index du vecteur CB1 (table selon la classe de signal).
			NLSFIndices[0] = (int8)dec.DecodeIcdf(&cb->CB1_iCDF[(signalType >> 1) * cb->nVectors], 8);
			unpack(ec_ix, pred_Q8, cb, NLSFIndices[0]);
			// Étage 2 : résidus par coefficient (contexte ec_ix[i]) + extension.
			for (int32 i = 0; i < cb->order; i++) {
				int32 Ix = dec.DecodeIcdf(&cb->ec_iCDF[ec_ix[i]], 8);
				if (Ix == 0) {
					Ix -= dec.DecodeIcdf(kNLSF_EXT_iCDF, 8);
				} else if (Ix == 2 * NLSF_QUANT_MAX_AMPLITUDE) {
					Ix += dec.DecodeIcdf(kNLSF_EXT_iCDF, 8);
				}
				NLSFIndices[i + 1] = (int8)(Ix - NLSF_QUANT_MAX_AMPLITUDE);
			}
		}

		void NkSilkNlsf::Decode(int16 *pNLSF_Q15, const int8 *NLSFIndices, const NkSilkNlsfCB *cb) {
			uint8 pred_Q8[16];
			int16 ec_ix[16];
			int16 res_Q10[16];

			unpack(ec_ix, pred_Q8, cb, NLSFIndices[0]);
			residualDequant(res_Q10, &NLSFIndices[1], pred_Q8, cb->quantStepSize_Q16, cb->order);

			const uint8 *pCB_element = &cb->CB1_NLSF_Q8[NLSFIndices[0] * cb->order];
			const int16 *pCB_Wght_Q9 = &cb->CB1_Wght_Q9[NLSFIndices[0] * cb->order];
			for (int32 i = 0; i < cb->order; i++) {
				const int32 NLSF_Q15_tmp = M::ADD_LSHIFT32(
					M::DIV32_16(M::LSHIFT((int32)res_Q10[i], 14), pCB_Wght_Q9[i]), (int16)pCB_element[i], 7);
				pNLSF_Q15[i] = (int16)M::LIMIT(NLSF_Q15_tmp, 0, 32767);
			}

			Stabilize(pNLSF_Q15, cb->deltaMin_Q15, cb->order);
		}

		// silk_NLSF_stabilize
		void NkSilkNlsf::Stabilize(int16 *NLSF_Q15, const int16 *NDeltaMin_Q15, int32 L) {
			const int32 MAX_LOOPS = 20;
			int32 I = 0, k, loops;
			int16 center_freq_Q15;
			int32 diff_Q15, min_diff_Q15, min_center_Q15, max_center_Q15;

			for (loops = 0; loops < MAX_LOOPS; loops++) {
				// Plus petite distance.
				min_diff_Q15 = NLSF_Q15[0] - NDeltaMin_Q15[0];
				I = 0;
				for (int32 i = 1; i <= L - 1; i++) {
					diff_Q15 = NLSF_Q15[i] - (NLSF_Q15[i - 1] + NDeltaMin_Q15[i]);
					if (diff_Q15 < min_diff_Q15) {
						min_diff_Q15 = diff_Q15;
						I = i;
					}
				}
				diff_Q15 = (1 << 15) - (NLSF_Q15[L - 1] + NDeltaMin_Q15[L]);
				if (diff_Q15 < min_diff_Q15) {
					min_diff_Q15 = diff_Q15;
					I = L;
				}
				if (min_diff_Q15 >= 0)
					return;

				if (I == 0) {
					NLSF_Q15[0] = NDeltaMin_Q15[0];
				} else if (I == L) {
					NLSF_Q15[L - 1] = (int16)((1 << 15) - NDeltaMin_Q15[L]);
				} else {
					min_center_Q15 = 0;
					for (k = 0; k < I; k++)
						min_center_Q15 += NDeltaMin_Q15[k];
					min_center_Q15 += M::RSHIFT(NDeltaMin_Q15[I], 1);
					max_center_Q15 = 1 << 15;
					for (k = L; k > I; k--)
						max_center_Q15 -= NDeltaMin_Q15[k];
					max_center_Q15 -= M::RSHIFT(NDeltaMin_Q15[I], 1);
					center_freq_Q15 = (int16)M::LIMIT(
						M::RSHIFT_ROUND((int32)NLSF_Q15[I - 1] + (int32)NLSF_Q15[I], 1), min_center_Q15, max_center_Q15);
					NLSF_Q15[I - 1] = (int16)(center_freq_Q15 - M::RSHIFT(NDeltaMin_Q15[I], 1));
					NLSF_Q15[I] = (int16)(NLSF_Q15[I - 1] + NDeltaMin_Q15[I]);
				}
			}

			// Repli (tri par insertion + contraintes de distance).
			if (loops == MAX_LOOPS) {
				insertionSortInc(&NLSF_Q15[0], L);
				NLSF_Q15[0] = (int16)M::maxInt(NLSF_Q15[0], NDeltaMin_Q15[0]);
				for (int32 i = 1; i < L; i++)
					NLSF_Q15[i] = (int16)M::maxInt(NLSF_Q15[i], M::ADD_SAT16(NLSF_Q15[i - 1], NDeltaMin_Q15[i]));
				NLSF_Q15[L - 1] = (int16)M::minInt(NLSF_Q15[L - 1], (1 << 15) - NDeltaMin_Q15[L]);
				for (int32 i = L - 2; i >= 0; i--)
					NLSF_Q15[i] = (int16)M::minInt(NLSF_Q15[i], NLSF_Q15[i + 1] - NDeltaMin_Q15[i + 1]);
			}
		}

		bool NkSilkNlsf::SelfTest() {
			const NkSilkNlsfCB *cbs[2] = {&kNlsfCB_NB_MB, &kNlsfCB_WB};
			uint8 buf[160];
			for (int32 c = 0; c < 2; c++) {
				const NkSilkNlsfCB *cb = cbs[c];
				for (int32 st = 0; st < 3; st++) {
					const int32 c1 = (st * 7 + c * 3) % cb->nVectors;
					int8 expected[17];
					expected[0] = (int8)c1;

					// Encode le chemin d'index (résidus dans la plage sans extension).
					NkOpusRangeEncoder enc;
					enc.Init(buf, sizeof(buf));
					enc.EncodeIcdf(c1, &cb->CB1_iCDF[(st >> 1) * cb->nVectors], 8);
					int16 ec_ix[16];
					uint8 pred_Q8[16];
					unpack(ec_ix, pred_Q8, cb, c1);
					for (int32 i = 0; i < cb->order; i++) {
						const int32 q = ((i * 5 + st) % 7) - 3; // -3..3 (évite l'extension)
						expected[i + 1] = (int8)q;
						enc.EncodeIcdf(q + NLSF_QUANT_MAX_AMPLITUDE, &cb->ec_iCDF[ec_ix[i]], 8);
					}
					enc.Done();

					// Décode et compare les index.
					NkOpusRangeDecoder dec;
					dec.Init(buf, enc.RangeBytes());
					int8 got[17];
					DecodeIndices(dec, got, cb, st);
					for (int32 i = 0; i < cb->order + 1; i++)
						if (got[i] != expected[i])
							return false;

					// Reconstruit le NLSF : ordonné + dans [0, 32767].
					int16 nlsf[16];
					Decode(nlsf, got, cb);
					for (int32 i = 0; i < cb->order; i++)
						if (nlsf[i] < 0 || nlsf[i] > 32767)
							return false;
					for (int32 i = 1; i < cb->order; i++)
						if (nlsf[i] < nlsf[i - 1])
							return false;

					// NLSF → LPC : filtre STABLE (inverse_pred_gain > 0).
					int16 a[16];
					NkSilkLpc::nlsf2a(a, nlsf, cb->order);
					if (NkSilkLpc::lpcInversePredGain(a, cb->order) <= 0)
						return false;
				}
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
