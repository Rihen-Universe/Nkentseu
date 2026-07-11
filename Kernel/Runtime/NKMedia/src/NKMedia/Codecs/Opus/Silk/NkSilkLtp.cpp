// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkLtp.cpp
// Port fidèle de libopus (decode_pitch.c, decode_parameters.c section LTP).
// =============================================================================
#include "NkSilkLtp.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		// pitch_est_defines.h
		static constexpr int32 PE_MIN_LAG_MS = 2;
		static constexpr int32 PE_MAX_LAG_MS = 18;

		// silk_decode_pitch
		void NkSilkLtp::DecodePitch(int16 lagIndex, int8 contourIndex, int32 *pitch_lags, int32 Fs_kHz,
									int32 nb_subfr) {
			int32 cbk_size = 0;
			const int8 *Lag_CB_ptr = NkSilkLtpTables::CbLags(Fs_kHz, nb_subfr, &cbk_size);

			const int32 min_lag = M::SMULBB(PE_MIN_LAG_MS, Fs_kHz);
			const int32 max_lag = M::SMULBB(PE_MAX_LAG_MS, Fs_kHz);
			const int32 lag = min_lag + lagIndex;

			for (int32 k = 0; k < nb_subfr; k++) {
				// matrix_ptr(base, k, contourIndex, cbk_size) = base[k*cbk_size + col]
				pitch_lags[k] = lag + Lag_CB_ptr[k * cbk_size + contourIndex];
				pitch_lags[k] = M::LIMIT(pitch_lags[k], min_lag, max_lag);
			}
		}

		// decode_parameters.c : reconstruction des coefficients LTP (Q7 → Q14).
		void NkSilkLtp::DecodeLtpGains(int8 PERIndex, const int8 *LTPIndex, int16 *LTPCoef_Q14, int32 nb_subfr) {
			const int8 *cbk_ptr_Q7 = NkSilkLtpTables::LtpVq(PERIndex);
			for (int32 k = 0; k < nb_subfr; k++) {
				const int32 Ix = LTPIndex[k];
				for (int32 i = 0; i < kLtpOrder; i++) {
					LTPCoef_Q14[k * kLtpOrder + i] = (int16)M::LSHIFT(cbk_ptr_Q7[Ix * kLtpOrder + i], 7);
				}
			}
		}

		int16 NkSilkLtp::DecodeLtpScale(int8 scaleIndex) {
			return NkSilkLtpTables::LtpScale(scaleIndex);
		}

		bool NkSilkLtp::SelfTest() {
			// 1) Lags de pitch bornés à [min_lag, max_lag] pour Fs 8/16 kHz et
			//    durées 4/2 sous-trames (couvre les 4 codebooks de contour).
			const int32 fss[2] = {8, 16};
			const int32 nsf[2] = {4, 2};
			for (int32 f = 0; f < 2; f++) {
				for (int32 s = 0; s < 2; s++) {
					const int32 Fs = fss[f], nb = nsf[s];
					const int32 min_lag = PE_MIN_LAG_MS * Fs, max_lag = PE_MAX_LAG_MS * Fs;
					int32 lags[4];
					for (int32 li = 0; li < 50; li += 7) {
						for (int32 ci = 0; ci < 3; ci++) {
							DecodePitch((int16)li, (int8)ci, lags, Fs, nb);
							for (int32 k = 0; k < nb; k++)
								if (lags[k] < min_lag || lags[k] > max_lag)
									return false;
						}
					}
				}
			}

			// 2) Gains LTP : sélection du codebook par PERIndex (tailles 8/16/32)
			//    + coefficients Q14 dans la plage int8<<7.
			for (int32 per = 0; per < 3; per++) {
				const int32 sz = NkSilkLtpTables::LtpVqSize(per);
				if (sz != (8 << per))
					return false;
				const int8 idx[4] = {(int8)(sz - 1), 0, (int8)(sz / 2), 1};
				int16 coef[4 * 5];
				DecodeLtpGains((int8)per, idx, coef, 4);
				for (int32 i = 0; i < 20; i++)
					if (coef[i] < -128 * 128 || coef[i] > 127 * 128)
						return false;
			}

			// 3) Échelle LTP : les 3 valeurs exactes de la table.
			if (DecodeLtpScale(0) != 15565 || DecodeLtpScale(1) != 12288 || DecodeLtpScale(2) != 8192)
				return false;

			return true;
		}

	} // namespace media
} // namespace nkentseu
