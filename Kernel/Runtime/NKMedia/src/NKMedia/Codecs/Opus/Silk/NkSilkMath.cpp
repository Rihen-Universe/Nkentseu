// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkMath.cpp
// Port fidèle de libopus silk/log2lin.c (silk_log2lin).
// =============================================================================
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		// Port fidèle de libopus silk/Inlines.h (silk_INVERSE32_varQ).
		int32 NkSilkMath::INVERSE32_varQ(int32 b32, int32 Qres) {
			const int32 b_headrm = CLZ32(abs32(b32)) - 1;
			const int32 b32_nrm = LSHIFT(b32, b_headrm);				 // Q: b_headrm
			const int32 b32_inv = DIV32_16(kInt32Max >> 2, RSHIFT(b32_nrm, 16)); // Q: 29+16-b_headrm
			int32 result = LSHIFT(b32_inv, 16);							 // Q: 61-b_headrm
			const int32 err_Q32 = LSHIFT(((int32)1 << 29) - SMULWB(b32_nrm, b32_inv), 3); // Q32
			result = SMLAWW(result, err_Q32, b32_inv);					 // Q: 61-b_headrm
			const int32 lshift = 61 - b_headrm - Qres;
			if (lshift <= 0) {
				return LSHIFT_SAT32(result, -lshift);
			} else {
				if (lshift < 32) {
					return RSHIFT(result, lshift);
				} else {
					return 0; // évite un résultat indéfini
				}
			}
		}

		// Port fidèle de libopus silk/Inlines.h (silk_DIV32_varQ).
		int32 NkSilkMath::DIV32_varQ(int32 a32, int32 b32, int32 Qres) {
			const int32 a_headrm = CLZ32(abs32(a32)) - 1;
			int32 a32_nrm = LSHIFT(a32, a_headrm);
			const int32 b_headrm = CLZ32(abs32(b32)) - 1;
			const int32 b32_nrm = LSHIFT(b32, b_headrm);
			const int32 b32_inv = DIV32_16(kInt32Max >> 2, RSHIFT(b32_nrm, 16));
			int32 result = SMULWB(a32_nrm, b32_inv);
			a32_nrm = SUB32_ovflw(a32_nrm, LSHIFT_ovflw(SMMUL(b32_nrm, result), 3));
			result = SMLAWB(result, a32_nrm, b32_inv);
			const int32 lshift = 29 + a_headrm - b_headrm - Qres;
			if (lshift < 0) {
				return LSHIFT_SAT32(result, -lshift);
			} else {
				if (lshift < 32) {
					return RSHIFT(result, lshift);
				} else {
					return 0;
				}
			}
		}

		int32 NkSilkMath::log2lin(int32 inLog_Q7) {
			int32 out, frac_Q7;

			if (inLog_Q7 < 0) {
				return 0;
			} else if (inLog_Q7 >= 3967) {
				return kInt32Max;
			}

			out = LSHIFT(1, RSHIFT(inLog_Q7, 7));
			frac_Q7 = inLog_Q7 & 0x7F;
			if (inLog_Q7 < 2048) {
				// out += out * (frac + frac*(128-frac)*(-174)/2^16) / 2^7
				out = ADD_RSHIFT32(out, MUL(out, SMLAWB(frac_Q7, SMULBB(frac_Q7, 128 - frac_Q7), -174)), 7);
			} else {
				out = MLA(out, RSHIFT(out, 7), SMLAWB(frac_Q7, SMULBB(frac_Q7, 128 - frac_Q7), -174));
			}
			return out;
		}

	} // namespace media
} // namespace nkentseu
