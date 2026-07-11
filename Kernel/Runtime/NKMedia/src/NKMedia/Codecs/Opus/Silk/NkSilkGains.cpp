// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkGains.cpp
// Port fidèle de libopus (tables_gain.c, decode_indices.c, gain_quant.c).
// =============================================================================
#include "NkSilkGains.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		// ── Tables (libopus silk/tables_gain.c + tables_other.c) ─────────────────
		// iCDF (inverse CDF, se terminent par 0) — lues avec ftb = 8.
		static const uint8 kGainICDF[3][8] = {
			{224, 112, 44, 15, 3, 2, 1, 0},
			{254, 237, 192, 132, 70, 23, 4, 0},
			{255, 252, 226, 155, 61, 11, 2, 0},
		};
		static const uint8 kDeltaGainICDF[41] = {
			250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20,
			19,  18,  17,  16,  15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
		};
		static const uint8 kUniform8ICDF[8] = {224, 192, 160, 128, 96, 64, 32, 0};

		// ── Constantes de quantification des gains (libopus silk/define.h) ───────
		static constexpr int32 MIN_QGAIN_DB = 2;
		static constexpr int32 MAX_QGAIN_DB = 88;
		static constexpr int32 N_LEVELS_QGAIN = 64;
		static constexpr int32 MAX_DELTA_GAIN_QUANT = 36;
		static constexpr int32 MIN_DELTA_GAIN_QUANT = -4;

		static constexpr int32 OFFSET = ((MIN_QGAIN_DB * 128) / 6 + 16 * 128);
		static constexpr int32 INV_SCALE_Q16 =
			((65536 * (((MAX_QGAIN_DB - MIN_QGAIN_DB) * 128) / 6)) / (N_LEVELS_QGAIN - 1));

		void NkSilkGains::DecodeIndices(NkOpusRangeDecoder &dec, int8 ind[4], int32 signalType, int32 condCoding,
										int32 nb_subfr) {
			// Premier sous-trame : indépendant (MSB via table de signalType, puis
			// LSB uniforme 3 bits) OU delta si codage conditionnel.
			if (condCoding == kCodeConditionally) {
				ind[0] = (int8)dec.DecodeIcdf(kDeltaGainICDF, 8);
			} else {
				ind[0] = (int8)NkSilkMath::LSHIFT(dec.DecodeIcdf(kGainICDF[signalType], 8), 3);
				ind[0] = (int8)(ind[0] + (int8)dec.DecodeIcdf(kUniform8ICDF, 8));
			}
			// Sous-trames suivantes : toujours delta.
			for (int32 i = 1; i < nb_subfr; i++) {
				ind[i] = (int8)dec.DecodeIcdf(kDeltaGainICDF, 8);
			}
		}

		void NkSilkGains::Dequant(int32 gain_Q16[4], const int8 ind[4], int8 *prev_ind, int32 conditional,
								  int32 nb_subfr) {
			for (int32 k = 0; k < nb_subfr; k++) {
				if (k == 0 && conditional == 0) {
					// Gain indépendant : borne inférieure relative au précédent.
					*prev_ind = (int8)NkSilkMath::maxInt(ind[k], *prev_ind - 16);
				} else {
					const int32 ind_tmp = ind[k] + MIN_DELTA_GAIN_QUANT;
					const int32 double_step_size_threshold = 2 * MAX_DELTA_GAIN_QUANT - N_LEVELS_QGAIN + *prev_ind;
					if (ind_tmp > double_step_size_threshold) {
						*prev_ind = (int8)(*prev_ind + (NkSilkMath::LSHIFT(ind_tmp, 1) - double_step_size_threshold));
					} else {
						*prev_ind = (int8)(*prev_ind + ind_tmp);
					}
				}
				*prev_ind = (int8)NkSilkMath::LIMIT_int(*prev_ind, 0, N_LEVELS_QGAIN - 1);

				gain_Q16[k] =
					NkSilkMath::log2lin(NkSilkMath::min32(NkSilkMath::SMULWB(INV_SCALE_Q16, *prev_ind) + OFFSET, 3967));
			}
		}

		// ── Self-test : points exacts de log2lin + aller-retour range-coder ──────
		bool NkSilkGains::SelfTest() {
			// 1) Points exacts de log2lin (frac = 0 → 2^(inLog>>7)).
			if (NkSilkMath::log2lin(0) != 1)
				return false;
			if (NkSilkMath::log2lin(128) != 2)
				return false;
			if (NkSilkMath::log2lin(896) != 128)
				return false;

			// 2) Aller-retour : encode les index avec les mêmes tables icdf, décode,
			//    et vérifie l'identité (prouve tables + flux de décode cohérents).
			const int32 subfrs[2] = {2, 4};
			uint8 buf[64];
			for (int32 st = 0; st < 3; ++st) {
				for (int32 si = 0; si < 2; ++si) {
					const int32 ns = subfrs[si];

					// -- indépendant --
					{
						NkOpusRangeEncoder enc;
						enc.Init(buf, sizeof(buf));
						const int32 msb = (st + 3) & 7;
						const int32 lsb = (st + 5) & 7;
						enc.EncodeIcdf(msb, kGainICDF[st], 8);
						enc.EncodeIcdf(lsb, kUniform8ICDF, 8);
						int8 expected[4];
						expected[0] = (int8)((msb << 3) + lsb);
						for (int32 i = 1; i < ns; i++) {
							const int32 d = (i * 7 + st) % 41;
							expected[i] = (int8)d;
							enc.EncodeIcdf(d, kDeltaGainICDF, 8);
						}
						enc.Done();

						NkOpusRangeDecoder dec;
						dec.Init(buf, enc.RangeBytes());
						int8 got[4] = {0, 0, 0, 0};
						DecodeIndices(dec, got, st, kCodeIndependently, ns);
						for (int32 i = 0; i < ns; i++)
							if (got[i] != expected[i])
								return false;
					}

					// -- conditionnel (1er sous-trame en delta) --
					{
						NkOpusRangeEncoder enc;
						enc.Init(buf, sizeof(buf));
						int8 expected[4];
						for (int32 i = 0; i < ns; i++) {
							const int32 d = (i * 11 + st * 3) % 41;
							expected[i] = (int8)d;
							enc.EncodeIcdf(d, kDeltaGainICDF, 8);
						}
						enc.Done();

						NkOpusRangeDecoder dec;
						dec.Init(buf, enc.RangeBytes());
						int8 got[4] = {0, 0, 0, 0};
						DecodeIndices(dec, got, st, kCodeConditionally, ns);
						for (int32 i = 0; i < ns; i++)
							if (got[i] != expected[i])
								return false;
					}
				}
			}

			// 3) Déquantification : monotonie croissante des gains sur un balayage
			//    d'index en codage indépendant (sanité de l'échelle log + hystérésis).
			{
				int32 prevGain = -1;
				for (int32 idx = 0; idx < N_LEVELS_QGAIN; ++idx) {
					int8 prev_ind = 0;
					int8 ind[4] = {(int8)idx, 0, 0, 0};
					int32 g[4] = {0, 0, 0, 0};
					// forcer l'indépendance avec un prev_ind très bas pour que
					// *prev_ind = max(idx, prev_ind-16) = idx (idx >= -16).
					prev_ind = 0;
					Dequant(g, ind, &prev_ind, kCodeIndependently, 1);
					if (g[0] < prevGain)
						return false;
					prevGain = g[0];
				}
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
