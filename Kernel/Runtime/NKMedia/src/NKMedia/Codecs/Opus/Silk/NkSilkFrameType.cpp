// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkFrameType.cpp
// Port fidèle de libopus (decode_indices.c en-tête + decode_parameters.c interp).
// =============================================================================
#include "NkSilkFrameType.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		// libopus silk/tables_other.c
		static const uint8 kType_offset_VAD_iCDF[4] = {232, 158, 10, 0};
		static const uint8 kType_offset_no_VAD_iCDF[2] = {230, 0};
		static const uint8 kNLSF_interp_factor_iCDF[5] = {243, 221, 192, 181, 0};
		static const uint8 kUniform4_iCDF[4] = {192, 128, 64, 0};

		void NkSilkFrameType::DecodeType(NkOpusRangeDecoder &dec, int32 vadFlag, int32 *signalType,
										 int32 *quantOffsetType) {
			int32 Ix;
			if (vadFlag) {
				Ix = dec.DecodeIcdf(kType_offset_VAD_iCDF, 8) + 2;
			} else {
				Ix = dec.DecodeIcdf(kType_offset_no_VAD_iCDF, 8);
			}
			*signalType = M::RSHIFT(Ix, 1);
			*quantOffsetType = Ix & 1;
		}

		int32 NkSilkFrameType::DecodeNlsfInterpFactor(NkOpusRangeDecoder &dec, int32 nb_subfr) {
			if (nb_subfr == 4) {
				return dec.DecodeIcdf(kNLSF_interp_factor_iCDF, 8);
			}
			return 4;
		}

		int32 NkSilkFrameType::DecodeSeed(NkOpusRangeDecoder &dec) {
			return dec.DecodeIcdf(kUniform4_iCDF, 8);
		}

		void NkSilkFrameType::InterpolateNlsf(int16 *xi, const int16 *x0, const int16 *x1, int32 ifact_Q2, int32 d) {
			for (int32 i = 0; i < d; i++) {
				xi[i] = (int16)(x0[i] + M::RSHIFT(M::MUL(ifact_Q2, x1[i] - x0[i]), 2));
			}
		}

		bool NkSilkFrameType::SelfTest() {
			uint8 buf[32];

			// 1) Type de trame — aller-retour (VAD : signalType∈{1,2} ; non-VAD : 0).
			for (int32 vad = 0; vad < 2; ++vad) {
				const int32 stMin = vad ? 1 : 0, stMax = vad ? 2 : 0;
				for (int32 st = stMin; st <= stMax; ++st) {
					for (int32 qot = 0; qot < 2; ++qot) {
						const int32 Ix = (st << 1) | qot;
						const int32 sym = vad ? (Ix - 2) : Ix;
						NkOpusRangeEncoder enc;
						enc.Init(buf, sizeof(buf));
						enc.EncodeIcdf(sym, vad ? kType_offset_VAD_iCDF : kType_offset_no_VAD_iCDF, 8);
						enc.Done();
						NkOpusRangeDecoder dec;
						dec.Init(buf, enc.RangeBytes());
						int32 gotSt = -1, gotQot = -1;
						DecodeType(dec, vad, &gotSt, &gotQot);
						if (gotSt != st || gotQot != qot)
							return false;
					}
				}
			}

			// 2) Facteur d'interpolation + graine — aller-retour.
			for (int32 v = 0; v < 5; ++v) {
				NkOpusRangeEncoder enc;
				enc.Init(buf, sizeof(buf));
				enc.EncodeIcdf(v, kNLSF_interp_factor_iCDF, 8);
				if (v < 4)
					enc.EncodeIcdf(v, kUniform4_iCDF, 8);
				enc.Done();
				NkOpusRangeDecoder dec;
				dec.Init(buf, enc.RangeBytes());
				if (DecodeNlsfInterpFactor(dec, 4) != v)
					return false;
				if (v < 4 && DecodeSeed(dec) != v)
					return false;
			}
			// nb_subfr != 4 → facteur = 4 sans lecture.
			{
				NkOpusRangeEncoder enc;
				enc.Init(buf, sizeof(buf));
				enc.EncodeIcdf(0, kUniform4_iCDF, 8);
				enc.Done();
				NkOpusRangeDecoder dec;
				dec.Init(buf, enc.RangeBytes());
				if (DecodeNlsfInterpFactor(dec, 2) != 4)
					return false;
			}

			// 3) Interpolation NLSF : ifact=0 → x0, ifact=4 → x1, ifact=2 → milieu.
			{
				const int16 x0[4] = {1000, 5000, 12000, 20000};
				const int16 x1[4] = {2000, 6000, 14000, 24000};
				int16 xi[4];
				InterpolateNlsf(xi, x0, x1, 0, 4);
				for (int32 i = 0; i < 4; ++i)
					if (xi[i] != x0[i])
						return false;
				InterpolateNlsf(xi, x0, x1, 4, 4);
				for (int32 i = 0; i < 4; ++i)
					if (xi[i] != x1[i])
						return false;
				InterpolateNlsf(xi, x0, x1, 2, 4);
				for (int32 i = 0; i < 4; ++i)
					if (xi[i] != (int16)(x0[i] + ((x1[i] - x0[i]) >> 1)))
						return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
