// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkIndices.cpp
// Port fidèle de libopus (decode_indices.c complet + decoder_set_fs.c).
// =============================================================================
#include "NkSilkIndices.h"
#include "NkSilkMath.h"
#include "NkSilkFrameType.h"
#include "NkSilkGains.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		void NkSilkFrameConfig::Set(int32 fs_kHz, int32 nbSubfr) {
			Fs_kHz = fs_kHz;
			nb_subfr = nbSubfr;
			// Contour de pitch (decoder_set_fs.c).
			if (fs_kHz == 8) {
				pitch_contour_iCDF = (nbSubfr == 4) ? kSilk_pitch_contour_NB_iCDF : kSilk_pitch_contour_10_ms_NB_iCDF;
			} else {
				pitch_contour_iCDF = (nbSubfr == 4) ? kSilk_pitch_contour_iCDF : kSilk_pitch_contour_10_ms_iCDF;
			}
			// Ordre LPC + codebook NLSF.
			if (fs_kHz == 8 || fs_kHz == 12) {
				LPC_order = 10;
				nlsfCB = &kNlsfCB_NB_MB;
			} else {
				LPC_order = 16;
				nlsfCB = &kNlsfCB_WB;
			}
			// Bits de poids faible du lag.
			if (fs_kHz == 16) {
				pitch_lag_low_bits_iCDF = kSilk_uniform8_iCDF;
			} else if (fs_kHz == 12) {
				pitch_lag_low_bits_iCDF = kSilk_uniform6_iCDF;
			} else {
				pitch_lag_low_bits_iCDF = kSilk_uniform4_iCDF;
			}
		}

		void NkSilkIndices::DecodeIndices(NkOpusRangeDecoder &dec, NkSilkIndicesData &ix, NkSilkIndicesState &st,
										  const NkSilkFrameConfig &cfg, int32 vadFlag, int32 condCoding) {
			// 1) Type de trame.
			NkSilkFrameType::DecodeType(dec, vadFlag, &ix.signalType, &ix.quantOffsetType);
			// 2) Index de gains.
			NkSilkGains::DecodeIndices(dec, ix.GainsIndices, ix.signalType, condCoding, cfg.nb_subfr);
			// 3) Index NLSF.
			NkSilkNlsf::DecodeIndices(dec, ix.NLSFIndices, cfg.nlsfCB, ix.signalType);
			// 4) Facteur d'interpolation NLSF.
			ix.NLSFInterpCoef_Q2 = NkSilkFrameType::DecodeNlsfInterpFactor(dec, cfg.nb_subfr);

			// 5) Pitch + LTP (uniquement voisé).
			if (ix.signalType == kTypeVoiced) {
				int32 decode_absolute = 1;
				if (condCoding == kCodeConditionally && st.ec_prevSignalType == kTypeVoiced) {
					int32 delta = dec.DecodeIcdf(kSilk_pitch_delta_iCDF, 8);
					if (delta > 0) {
						delta = delta - 9;
						ix.lagIndex = (int16)(st.ec_prevLagIndex + delta);
						decode_absolute = 0;
					}
				}
				if (decode_absolute) {
					ix.lagIndex = (int16)(dec.DecodeIcdf(kSilk_pitch_lag_iCDF, 8) * M::RSHIFT(cfg.Fs_kHz, 1));
					ix.lagIndex = (int16)(ix.lagIndex + dec.DecodeIcdf(cfg.pitch_lag_low_bits_iCDF, 8));
				}
				st.ec_prevLagIndex = ix.lagIndex;

				ix.contourIndex = (int8)dec.DecodeIcdf(cfg.pitch_contour_iCDF, 8);

				ix.PERIndex = (int8)dec.DecodeIcdf(kSilk_LTP_per_index_iCDF, 8);
				for (int32 k = 0; k < cfg.nb_subfr; k++) {
					ix.LTPIndex[k] = (int8)dec.DecodeIcdf(kSilk_LTP_gain_iCDF_ptrs[ix.PERIndex], 8);
				}
				if (condCoding == kCodeIndependently) {
					ix.LTP_scaleIndex = (int8)dec.DecodeIcdf(kSilk_LTPscale_iCDF, 8);
				} else {
					ix.LTP_scaleIndex = 0;
				}
			}
			st.ec_prevSignalType = ix.signalType;

			// 6) Graine.
			ix.Seed = dec.DecodeIcdf(kSilk_uniform4_iCDF, 8);
		}

		// ── Self-test : aller-retour complet du décodage des index ───────────────
		bool NkSilkIndices::SelfTest() {
			// Configs : (16k,4,voisé) (8k,4,voisé) (12k,2,non-voisé). condCoding INDEP.
			struct Cfg {
					int32 fs, nb, vad, st;
			};
			const Cfg cfgs[3] = {{16, 4, 1, 2}, {8, 4, 1, 2}, {12, 2, 1, 1}};
			uint8 buf[256];

			for (int32 c = 0; c < 3; ++c) {
				NkSilkFrameConfig cfg;
				cfg.Set(cfgs[c].fs, cfgs[c].nb);
				const int32 vad = cfgs[c].vad, st = cfgs[c].st, qot = c & 1;

				// Vérité déterministe.
				NkSilkIndicesData truth;
				truth.signalType = st;
				truth.quantOffsetType = qot;
				const int32 lagHi = 5 + c, lagLo = 2 + c;

				NkOpusRangeEncoder enc;
				enc.Init(buf, sizeof(buf));

				// (1) type — table VAD/non-VAD comme NkSilkFrameType.
				{
					static const uint8 kVAD[4] = {232, 158, 10, 0};
					static const uint8 kNoVAD[2] = {230, 0};
					const int32 Ix = (st << 1) | qot;
					enc.EncodeIcdf(vad ? (Ix - 2) : Ix, vad ? kVAD : kNoVAD, 8);
				}
				// (2) gains — indépendant : MSB (table signalType) + LSB uniforme, puis deltas.
				{
					static const uint8 kGainICDF[3][8] = {{224, 112, 44, 15, 3, 2, 1, 0},
														  {254, 237, 192, 132, 70, 23, 4, 0},
														  {255, 252, 226, 155, 61, 11, 2, 0}};
					static const uint8 kU8[8] = {224, 192, 160, 128, 96, 64, 32, 0};
					static const uint8 kDelta[41] = {250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27,
													 26,  25,  24,  23,  22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
													 12,  11,  10,  9,   8,  7,  6,  5,  4,  3,  2,  1,  0};
					const int32 msb = 3, lsb = 4;
					truth.GainsIndices[0] = (int8)((msb << 3) + lsb);
					enc.EncodeIcdf(msb, kGainICDF[st], 8);
					enc.EncodeIcdf(lsb, kU8, 8);
					for (int32 k = 1; k < cfg.nb_subfr; k++) {
						const int32 d = (k * 7) % 41;
						truth.GainsIndices[k] = (int8)d;
						enc.EncodeIcdf(d, kDelta, 8);
					}
				}
				// (3) NLSF — CB1 + résidus (plage sans extension).
				{
					const int32 c1 = (st * 5 + c) % cfg.nlsfCB->nVectors;
					truth.NLSFIndices[0] = (int8)c1;
					enc.EncodeIcdf(c1, &cfg.nlsfCB->CB1_iCDF[(st >> 1) * cfg.nlsfCB->nVectors], 8);
					// unpack contexts (identique à NkSilkNlsf::unpack).
					for (int32 i = 0; i < cfg.nlsfCB->order; i += 2) {
						const uint8 entry = cfg.nlsfCB->ec_sel[(c1 * cfg.nlsfCB->order / 2) + (i / 2)];
						const int32 ctx0 = M::SMULBB(M::RSHIFT(entry, 1) & 7, 9);
						const int32 ctx1 = M::SMULBB(M::RSHIFT(entry, 5) & 7, 9);
						const int32 q0 = ((i * 5) % 7) - 3, q1 = (((i + 1) * 5) % 7) - 3;
						truth.NLSFIndices[i + 1] = (int8)q0;
						truth.NLSFIndices[i + 2] = (int8)q1;
						enc.EncodeIcdf(q0 + 4, &cfg.nlsfCB->ec_iCDF[ctx0], 8);
						enc.EncodeIcdf(q1 + 4, &cfg.nlsfCB->ec_iCDF[ctx1], 8);
					}
				}
				// (4) interp factor (nb_subfr==4 → présent).
				{
					static const uint8 kInterp[5] = {243, 221, 192, 181, 0};
					if (cfg.nb_subfr == 4) {
						truth.NLSFInterpCoef_Q2 = 2;
						enc.EncodeIcdf(2, kInterp, 8);
					} else {
						truth.NLSFInterpCoef_Q2 = 4;
					}
				}
				// (5) pitch/LTP (voisé).
				if (st == kTypeVoiced) {
					truth.lagIndex = (int16)(lagHi * M::RSHIFT(cfg.Fs_kHz, 1) + lagLo);
					enc.EncodeIcdf(lagHi, kSilk_pitch_lag_iCDF, 8);
					enc.EncodeIcdf(lagLo, cfg.pitch_lag_low_bits_iCDF, 8);
					truth.contourIndex = (int8)1;
					enc.EncodeIcdf(1, cfg.pitch_contour_iCDF, 8);
					const int32 per = 1;
					truth.PERIndex = (int8)per;
					enc.EncodeIcdf(per, kSilk_LTP_per_index_iCDF, 8);
					for (int32 k = 0; k < cfg.nb_subfr; k++) {
						const int32 li = (k + 1) % 16;
						truth.LTPIndex[k] = (int8)li;
						enc.EncodeIcdf(li, kSilk_LTP_gain_iCDF_ptrs[per], 8);
					}
					truth.LTP_scaleIndex = (int8)2;
					enc.EncodeIcdf(2, kSilk_LTPscale_iCDF, 8);
				}
				// (6) seed.
				truth.Seed = 3;
				enc.EncodeIcdf(3, kSilk_uniform4_iCDF, 8);
				enc.Done();

				// Décode et compare.
				NkSilkIndicesState state;
				NkSilkIndicesData got;
				NkOpusRangeDecoder dec;
				dec.Init(buf, enc.RangeBytes());
				DecodeIndices(dec, got, state, cfg, vad, kCodeIndependently);

				if (got.signalType != truth.signalType || got.quantOffsetType != truth.quantOffsetType)
					return false;
				for (int32 k = 0; k < cfg.nb_subfr; k++)
					if (got.GainsIndices[k] != truth.GainsIndices[k])
						return false;
				for (int32 i = 0; i < cfg.nlsfCB->order + 1; i++)
					if (got.NLSFIndices[i] != truth.NLSFIndices[i])
						return false;
				if (got.NLSFInterpCoef_Q2 != truth.NLSFInterpCoef_Q2)
					return false;
				if (st == kTypeVoiced) {
					if (got.lagIndex != truth.lagIndex || got.contourIndex != truth.contourIndex)
						return false;
					if (got.PERIndex != truth.PERIndex || got.LTP_scaleIndex != truth.LTP_scaleIndex)
						return false;
					for (int32 k = 0; k < cfg.nb_subfr; k++)
						if (got.LTPIndex[k] != truth.LTPIndex[k])
							return false;
				}
				if (got.Seed != truth.Seed)
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
