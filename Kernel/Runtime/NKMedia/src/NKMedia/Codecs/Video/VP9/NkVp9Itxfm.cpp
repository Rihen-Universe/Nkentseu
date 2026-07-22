// =============================================================================
// NKMedia/Codecs/Video/VP9/NkVp9Itxfm.cpp — transformées inverses VP9 (port
// fidèle étape par étape de vpx_dsp/inv_txfm.c ; le mimétisme strict de
// l'arithmétique — casts int16, arrondis Q14, ordre des papillons — est ce qui
// donne le bit-exact, leçon du filtre de boucle VP8).
// =============================================================================
#include "NKMedia/Codecs/Video/VP9/NkVp9Itxfm.h"

namespace nkentseu {
	namespace media {

		namespace {

			typedef int16 TranLow;	// tran_low_t (build 8 bits)
			typedef int32 TranHigh; // tran_high_t

			// Constantes Q14 du format (txfm_common.h) : cospi_k_64 = round(16384·cos(k·π/64)).
			constexpr TranHigh kCospi[32] = {0,	   16364, 16305, 16207, 16069, 15893, 15679, 15426,
											15137, 14811, 14449, 14053, 13623, 13160, 12665, 12140,
											11585, 11003, 10394, 9760,	9102,  8423,  7723,	 7005,
											6270,  5520,  4756,	 3981,	3196,  2404,  1606,	 804};
			constexpr TranHigh kSinpi1 = 5283, kSinpi2 = 9929, kSinpi3 = 13377, kSinpi4 = 15212;

#define NK_COSPI(k) (kCospi[(k)])

			inline TranHigh DctRound(TranHigh x) {
				return (x + (1 << 13)) >> 14; // dct_const_round_shift (Q14)
			}
			inline int32 RoundPow2(int32 x, int32 n) {
				return (x + (1 << (n - 1))) >> n;
			}
			inline uint8 ClipPixelAdd(uint8 d, int32 t) {
				const int32 v = (int32)d + t;
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}

			// --- idct4 (inv_txfm.c idct4_c) ---
			void Idct4(const TranLow *input, TranLow *output) {
				int16 step[4];
				TranHigh temp1, temp2;
				temp1 = ((int16)input[0] + (int16)input[2]) * NK_COSPI(16);
				temp2 = ((int16)input[0] - (int16)input[2]) * NK_COSPI(16);
				step[0] = (int16)DctRound(temp1);
				step[1] = (int16)DctRound(temp2);
				temp1 = (int16)input[1] * NK_COSPI(24) - (int16)input[3] * NK_COSPI(8);
				temp2 = (int16)input[1] * NK_COSPI(8) + (int16)input[3] * NK_COSPI(24);
				step[2] = (int16)DctRound(temp1);
				step[3] = (int16)DctRound(temp2);
				output[0] = (TranLow)(step[0] + step[3]);
				output[1] = (TranLow)(step[1] + step[2]);
				output[2] = (TranLow)(step[1] - step[2]);
				output[3] = (TranLow)(step[0] - step[3]);
			}

			// --- iadst4 (inv_txfm.c iadst4_c) ---
			void Iadst4(const TranLow *input, TranLow *output) {
				TranHigh s0, s1, s2, s3, s4, s5, s6, s7;
				TranLow x0 = input[0], x1 = input[1], x2 = input[2], x3 = input[3];
				if (!(x0 | x1 | x2 | x3)) {
					output[0] = output[1] = output[2] = output[3] = 0;
					return;
				}
				s0 = kSinpi1 * x0;
				s1 = kSinpi2 * x0;
				s2 = kSinpi3 * x1;
				s3 = kSinpi4 * x2;
				s4 = kSinpi1 * x2;
				s5 = kSinpi2 * x3;
				s6 = kSinpi4 * x3;
				s7 = (TranHigh)(x0 - x2 + x3);
				// (s7 : WRAPLOW = pas de troncature int16 en build standard.)
				s0 = s0 + s3 + s5;
				s1 = s1 - s4 - s6;
				s3 = s2;
				s2 = kSinpi3 * s7;
				output[0] = (TranLow)DctRound(s0 + s3);
				output[1] = (TranLow)DctRound(s1 + s3);
				output[2] = (TranLow)DctRound(s2);
				output[3] = (TranLow)DctRound(s0 + s1 - s3);
			}

			// --- idct8 (inv_txfm.c idct8_c) ---
			void Idct8(const TranLow *input, TranLow *output) {
				int16 step1[8], step2[8];
				TranHigh temp1, temp2;
				// étape 1
				step1[0] = (int16)input[0];
				step1[2] = (int16)input[4];
				step1[1] = (int16)input[2];
				step1[3] = (int16)input[6];
				temp1 = (int16)input[1] * NK_COSPI(28) - (int16)input[7] * NK_COSPI(4);
				temp2 = (int16)input[1] * NK_COSPI(4) + (int16)input[7] * NK_COSPI(28);
				step1[4] = (int16)DctRound(temp1);
				step1[7] = (int16)DctRound(temp2);
				temp1 = (int16)input[5] * NK_COSPI(12) - (int16)input[3] * NK_COSPI(20);
				temp2 = (int16)input[5] * NK_COSPI(20) + (int16)input[3] * NK_COSPI(12);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				// étape 2
				temp1 = (step1[0] + step1[2]) * NK_COSPI(16);
				temp2 = (step1[0] - step1[2]) * NK_COSPI(16);
				step2[0] = (int16)DctRound(temp1);
				step2[1] = (int16)DctRound(temp2);
				temp1 = step1[1] * NK_COSPI(24) - step1[3] * NK_COSPI(8);
				temp2 = step1[1] * NK_COSPI(8) + step1[3] * NK_COSPI(24);
				step2[2] = (int16)DctRound(temp1);
				step2[3] = (int16)DctRound(temp2);
				step2[4] = (int16)(step1[4] + step1[5]);
				step2[5] = (int16)(step1[4] - step1[5]);
				step2[6] = (int16)(-step1[6] + step1[7]);
				step2[7] = (int16)(step1[6] + step1[7]);
				// étape 3
				step1[0] = (int16)(step2[0] + step2[3]);
				step1[1] = (int16)(step2[1] + step2[2]);
				step1[2] = (int16)(step2[1] - step2[2]);
				step1[3] = (int16)(step2[0] - step2[3]);
				step1[4] = step2[4];
				temp1 = (step2[6] - step2[5]) * NK_COSPI(16);
				temp2 = (step2[5] + step2[6]) * NK_COSPI(16);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				step1[7] = step2[7];
				// étape 4
				output[0] = (TranLow)(step1[0] + step1[7]);
				output[1] = (TranLow)(step1[1] + step1[6]);
				output[2] = (TranLow)(step1[2] + step1[5]);
				output[3] = (TranLow)(step1[3] + step1[4]);
				output[4] = (TranLow)(step1[3] - step1[4]);
				output[5] = (TranLow)(step1[2] - step1[5]);
				output[6] = (TranLow)(step1[1] - step1[6]);
				output[7] = (TranLow)(step1[0] - step1[7]);
			}

			// --- iadst8 (inv_txfm.c iadst8_c) ---
			void Iadst8(const TranLow *input, TranLow *output) {
				int32 s0, s1, s2, s3, s4, s5, s6, s7;
				TranHigh x0 = input[7], x1 = input[0], x2 = input[5], x3 = input[2];
				TranHigh x4 = input[3], x5 = input[4], x6 = input[1], x7 = input[6];
				if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7)) {
					for (int32 i = 0; i < 8; ++i)
						output[i] = 0;
					return;
				}
				// étape 1
				s0 = (int32)(NK_COSPI(2) * x0 + NK_COSPI(30) * x1);
				s1 = (int32)(NK_COSPI(30) * x0 - NK_COSPI(2) * x1);
				s2 = (int32)(NK_COSPI(10) * x2 + NK_COSPI(22) * x3);
				s3 = (int32)(NK_COSPI(22) * x2 - NK_COSPI(10) * x3);
				s4 = (int32)(NK_COSPI(18) * x4 + NK_COSPI(14) * x5);
				s5 = (int32)(NK_COSPI(14) * x4 - NK_COSPI(18) * x5);
				s6 = (int32)(NK_COSPI(26) * x6 + NK_COSPI(6) * x7);
				s7 = (int32)(NK_COSPI(6) * x6 - NK_COSPI(26) * x7);
				// (WRAPLOW sur tran_high : pas de troncature int16 en build standard.)
				x0 = DctRound(s0 + s4);
				x1 = DctRound(s1 + s5);
				x2 = DctRound(s2 + s6);
				x3 = DctRound(s3 + s7);
				x4 = DctRound(s0 - s4);
				x5 = DctRound(s1 - s5);
				x6 = DctRound(s2 - s6);
				x7 = DctRound(s3 - s7);
				// étape 2
				s0 = (int32)x0;
				s1 = (int32)x1;
				s2 = (int32)x2;
				s3 = (int32)x3;
				s4 = (int32)(NK_COSPI(8) * x4 + NK_COSPI(24) * x5);
				s5 = (int32)(NK_COSPI(24) * x4 - NK_COSPI(8) * x5);
				s6 = (int32)(-NK_COSPI(24) * x6 + NK_COSPI(8) * x7);
				s7 = (int32)(NK_COSPI(8) * x6 + NK_COSPI(24) * x7);
				x0 = s0 + s2;
				x1 = s1 + s3;
				x2 = s0 - s2;
				x3 = s1 - s3;
				x4 = DctRound(s4 + s6);
				x5 = DctRound(s5 + s7);
				x6 = DctRound(s4 - s6);
				x7 = DctRound(s5 - s7);
				// étape 3
				s2 = (int32)(NK_COSPI(16) * (x2 + x3));
				s3 = (int32)(NK_COSPI(16) * (x2 - x3));
				s6 = (int32)(NK_COSPI(16) * (x6 + x7));
				s7 = (int32)(NK_COSPI(16) * (x6 - x7));
				x2 = DctRound(s2);
				x3 = DctRound(s3);
				x6 = DctRound(s6);
				x7 = DctRound(s7);
				output[0] = (TranLow)x0;
				output[1] = (TranLow)-x4;
				output[2] = (TranLow)x6;
				output[3] = (TranLow)-x2;
				output[4] = (TranLow)x3;
				output[5] = (TranLow)-x7;
				output[6] = (TranLow)x5;
				output[7] = (TranLow)-x1;
			}

			// --- idct16 (inv_txfm.c idct16_c) ---
			void Idct16(const TranLow *input, TranLow *output) {
				int16 step1[16], step2[16];
				TranHigh temp1, temp2;
				// étape 1 (réordonnancement pair/impair)
				step1[0] = (int16)input[0];
				step1[1] = (int16)input[8];
				step1[2] = (int16)input[4];
				step1[3] = (int16)input[12];
				step1[4] = (int16)input[2];
				step1[5] = (int16)input[10];
				step1[6] = (int16)input[6];
				step1[7] = (int16)input[14];
				step1[8] = (int16)input[1];
				step1[9] = (int16)input[9];
				step1[10] = (int16)input[5];
				step1[11] = (int16)input[13];
				step1[12] = (int16)input[3];
				step1[13] = (int16)input[11];
				step1[14] = (int16)input[7];
				step1[15] = (int16)input[15];
				// étape 2
				for (int32 i = 0; i < 8; ++i)
					step2[i] = step1[i];
				temp1 = step1[8] * NK_COSPI(30) - step1[15] * NK_COSPI(2);
				temp2 = step1[8] * NK_COSPI(2) + step1[15] * NK_COSPI(30);
				step2[8] = (int16)DctRound(temp1);
				step2[15] = (int16)DctRound(temp2);
				temp1 = step1[9] * NK_COSPI(14) - step1[14] * NK_COSPI(18);
				temp2 = step1[9] * NK_COSPI(18) + step1[14] * NK_COSPI(14);
				step2[9] = (int16)DctRound(temp1);
				step2[14] = (int16)DctRound(temp2);
				temp1 = step1[10] * NK_COSPI(22) - step1[13] * NK_COSPI(10);
				temp2 = step1[10] * NK_COSPI(10) + step1[13] * NK_COSPI(22);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				temp1 = step1[11] * NK_COSPI(6) - step1[12] * NK_COSPI(26);
				temp2 = step1[11] * NK_COSPI(26) + step1[12] * NK_COSPI(6);
				step2[11] = (int16)DctRound(temp1);
				step2[12] = (int16)DctRound(temp2);
				// étape 3
				step1[0] = step2[0];
				step1[1] = step2[1];
				step1[2] = step2[2];
				step1[3] = step2[3];
				temp1 = step2[4] * NK_COSPI(28) - step2[7] * NK_COSPI(4);
				temp2 = step2[4] * NK_COSPI(4) + step2[7] * NK_COSPI(28);
				step1[4] = (int16)DctRound(temp1);
				step1[7] = (int16)DctRound(temp2);
				temp1 = step2[5] * NK_COSPI(12) - step2[6] * NK_COSPI(20);
				temp2 = step2[5] * NK_COSPI(20) + step2[6] * NK_COSPI(12);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				step1[8] = (int16)(step2[8] + step2[9]);
				step1[9] = (int16)(step2[8] - step2[9]);
				step1[10] = (int16)(-step2[10] + step2[11]);
				step1[11] = (int16)(step2[10] + step2[11]);
				step1[12] = (int16)(step2[12] + step2[13]);
				step1[13] = (int16)(step2[12] - step2[13]);
				step1[14] = (int16)(-step2[14] + step2[15]);
				step1[15] = (int16)(step2[14] + step2[15]);
				// étape 4
				temp1 = (step1[0] + step1[1]) * NK_COSPI(16);
				temp2 = (step1[0] - step1[1]) * NK_COSPI(16);
				step2[0] = (int16)DctRound(temp1);
				step2[1] = (int16)DctRound(temp2);
				temp1 = step1[2] * NK_COSPI(24) - step1[3] * NK_COSPI(8);
				temp2 = step1[2] * NK_COSPI(8) + step1[3] * NK_COSPI(24);
				step2[2] = (int16)DctRound(temp1);
				step2[3] = (int16)DctRound(temp2);
				step2[4] = (int16)(step1[4] + step1[5]);
				step2[5] = (int16)(step1[4] - step1[5]);
				step2[6] = (int16)(-step1[6] + step1[7]);
				step2[7] = (int16)(step1[6] + step1[7]);
				step2[8] = step1[8];
				step2[15] = step1[15];
				temp1 = -step1[9] * NK_COSPI(8) + step1[14] * NK_COSPI(24);
				temp2 = step1[9] * NK_COSPI(24) + step1[14] * NK_COSPI(8);
				step2[9] = (int16)DctRound(temp1);
				step2[14] = (int16)DctRound(temp2);
				temp1 = -step1[10] * NK_COSPI(24) - step1[13] * NK_COSPI(8);
				temp2 = -step1[10] * NK_COSPI(8) + step1[13] * NK_COSPI(24);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				step2[11] = step1[11];
				step2[12] = step1[12];
				// étape 5
				step1[0] = (int16)(step2[0] + step2[3]);
				step1[1] = (int16)(step2[1] + step2[2]);
				step1[2] = (int16)(step2[1] - step2[2]);
				step1[3] = (int16)(step2[0] - step2[3]);
				step1[4] = step2[4];
				temp1 = (step2[6] - step2[5]) * NK_COSPI(16);
				temp2 = (step2[5] + step2[6]) * NK_COSPI(16);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				step1[7] = step2[7];
				step1[8] = (int16)(step2[8] + step2[11]);
				step1[9] = (int16)(step2[9] + step2[10]);
				step1[10] = (int16)(step2[9] - step2[10]);
				step1[11] = (int16)(step2[8] - step2[11]);
				step1[12] = (int16)(-step2[12] + step2[15]);
				step1[13] = (int16)(-step2[13] + step2[14]);
				step1[14] = (int16)(step2[13] + step2[14]);
				step1[15] = (int16)(step2[12] + step2[15]);
				// étape 6
				step2[0] = (int16)(step1[0] + step1[7]);
				step2[1] = (int16)(step1[1] + step1[6]);
				step2[2] = (int16)(step1[2] + step1[5]);
				step2[3] = (int16)(step1[3] + step1[4]);
				step2[4] = (int16)(step1[3] - step1[4]);
				step2[5] = (int16)(step1[2] - step1[5]);
				step2[6] = (int16)(step1[1] - step1[6]);
				step2[7] = (int16)(step1[0] - step1[7]);
				step2[8] = step1[8];
				step2[9] = step1[9];
				temp1 = (-step1[10] + step1[13]) * NK_COSPI(16);
				temp2 = (step1[10] + step1[13]) * NK_COSPI(16);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				temp1 = (-step1[11] + step1[12]) * NK_COSPI(16);
				temp2 = (step1[11] + step1[12]) * NK_COSPI(16);
				step2[11] = (int16)DctRound(temp1);
				step2[12] = (int16)DctRound(temp2);
				step2[14] = step1[14];
				step2[15] = step1[15];
				// étape 7
				for (int32 i = 0; i < 8; ++i) {
					output[i] = (TranLow)(step2[i] + step2[15 - i]);
					output[8 + i] = (TranLow)(step2[7 - i] - step2[8 + i]);
				}
			}

			// --- iadst16 (inv_txfm.c iadst16_c) ---
			void Iadst16(const TranLow *input, TranLow *output) {
				TranHigh s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15;
				TranHigh x0 = input[15], x1 = input[0], x2 = input[13], x3 = input[2];
				TranHigh x4 = input[11], x5 = input[4], x6 = input[9], x7 = input[6];
				TranHigh x8 = input[7], x9 = input[8], x10 = input[5], x11 = input[10];
				TranHigh x12 = input[3], x13 = input[12], x14 = input[1], x15 = input[14];
				if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7 | x8 | x9 | x10 | x11 | x12 | x13 |
					  x14 | x15)) {
					for (int32 i = 0; i < 16; ++i)
						output[i] = 0;
					return;
				}
				// étape 1
				s0 = x0 * NK_COSPI(1) + x1 * NK_COSPI(31);
				s1 = x0 * NK_COSPI(31) - x1 * NK_COSPI(1);
				s2 = x2 * NK_COSPI(5) + x3 * NK_COSPI(27);
				s3 = x2 * NK_COSPI(27) - x3 * NK_COSPI(5);
				s4 = x4 * NK_COSPI(9) + x5 * NK_COSPI(23);
				s5 = x4 * NK_COSPI(23) - x5 * NK_COSPI(9);
				s6 = x6 * NK_COSPI(13) + x7 * NK_COSPI(19);
				s7 = x6 * NK_COSPI(19) - x7 * NK_COSPI(13);
				s8 = x8 * NK_COSPI(17) + x9 * NK_COSPI(15);
				s9 = x8 * NK_COSPI(15) - x9 * NK_COSPI(17);
				s10 = x10 * NK_COSPI(21) + x11 * NK_COSPI(11);
				s11 = x10 * NK_COSPI(11) - x11 * NK_COSPI(21);
				s12 = x12 * NK_COSPI(25) + x13 * NK_COSPI(7);
				s13 = x12 * NK_COSPI(7) - x13 * NK_COSPI(25);
				s14 = x14 * NK_COSPI(29) + x15 * NK_COSPI(3);
				s15 = x14 * NK_COSPI(3) - x15 * NK_COSPI(29);
				// (WRAPLOW sur tran_high : pas de troncature int16 en build standard.)
				x0 = DctRound(s0 + s8);
				x1 = DctRound(s1 + s9);
				x2 = DctRound(s2 + s10);
				x3 = DctRound(s3 + s11);
				x4 = DctRound(s4 + s12);
				x5 = DctRound(s5 + s13);
				x6 = DctRound(s6 + s14);
				x7 = DctRound(s7 + s15);
				x8 = DctRound(s0 - s8);
				x9 = DctRound(s1 - s9);
				x10 = DctRound(s2 - s10);
				x11 = DctRound(s3 - s11);
				x12 = DctRound(s4 - s12);
				x13 = DctRound(s5 - s13);
				x14 = DctRound(s6 - s14);
				x15 = DctRound(s7 - s15);
				// étape 2
				s0 = x0;
				s1 = x1;
				s2 = x2;
				s3 = x3;
				s4 = x4;
				s5 = x5;
				s6 = x6;
				s7 = x7;
				s8 = x8 * NK_COSPI(4) + x9 * NK_COSPI(28);
				s9 = x8 * NK_COSPI(28) - x9 * NK_COSPI(4);
				s10 = x10 * NK_COSPI(20) + x11 * NK_COSPI(12);
				s11 = x10 * NK_COSPI(12) - x11 * NK_COSPI(20);
				s12 = -x12 * NK_COSPI(28) + x13 * NK_COSPI(4);
				s13 = x12 * NK_COSPI(4) + x13 * NK_COSPI(28);
				s14 = -x14 * NK_COSPI(12) + x15 * NK_COSPI(20);
				s15 = x14 * NK_COSPI(20) + x15 * NK_COSPI(12);
				x0 = s0 + s4;
				x1 = s1 + s5;
				x2 = s2 + s6;
				x3 = s3 + s7;
				x4 = s0 - s4;
				x5 = s1 - s5;
				x6 = s2 - s6;
				x7 = s3 - s7;
				x8 = DctRound(s8 + s12);
				x9 = DctRound(s9 + s13);
				x10 = DctRound(s10 + s14);
				x11 = DctRound(s11 + s15);
				x12 = DctRound(s8 - s12);
				x13 = DctRound(s9 - s13);
				x14 = DctRound(s10 - s14);
				x15 = DctRound(s11 - s15);
				// étape 3
				s0 = x0;
				s1 = x1;
				s2 = x2;
				s3 = x3;
				s4 = x4 * NK_COSPI(8) + x5 * NK_COSPI(24);
				s5 = x4 * NK_COSPI(24) - x5 * NK_COSPI(8);
				s6 = -x6 * NK_COSPI(24) + x7 * NK_COSPI(8);
				s7 = x6 * NK_COSPI(8) + x7 * NK_COSPI(24);
				s8 = x8;
				s9 = x9;
				s10 = x10;
				s11 = x11;
				s12 = x12 * NK_COSPI(8) + x13 * NK_COSPI(24);
				s13 = x12 * NK_COSPI(24) - x13 * NK_COSPI(8);
				s14 = -x14 * NK_COSPI(24) + x15 * NK_COSPI(8);
				s15 = x14 * NK_COSPI(8) + x15 * NK_COSPI(24);
				x0 = s0 + s2;
				x1 = s1 + s3;
				x2 = s0 - s2;
				x3 = s1 - s3;
				x4 = DctRound(s4 + s6);
				x5 = DctRound(s5 + s7);
				x6 = DctRound(s4 - s6);
				x7 = DctRound(s5 - s7);
				x8 = s8 + s10;
				x9 = s9 + s11;
				x10 = s8 - s10;
				x11 = s9 - s11;
				x12 = DctRound(s12 + s14);
				x13 = DctRound(s13 + s15);
				x14 = DctRound(s12 - s14);
				x15 = DctRound(s13 - s15);
				// étape 4
				s2 = (-NK_COSPI(16)) * (x2 + x3);
				s3 = NK_COSPI(16) * (x2 - x3);
				s6 = NK_COSPI(16) * (x6 + x7);
				s7 = NK_COSPI(16) * (-x6 + x7);
				s10 = NK_COSPI(16) * (x10 + x11);
				s11 = NK_COSPI(16) * (-x10 + x11);
				s14 = (-NK_COSPI(16)) * (x14 + x15);
				s15 = NK_COSPI(16) * (x14 - x15);
				x2 = DctRound(s2);
				x3 = DctRound(s3);
				x6 = DctRound(s6);
				x7 = DctRound(s7);
				x10 = DctRound(s10);
				x11 = DctRound(s11);
				x14 = DctRound(s14);
				x15 = DctRound(s15);
				output[0] = (TranLow)x0;
				output[1] = (TranLow)-x8;
				output[2] = (TranLow)x12;
				output[3] = (TranLow)-x4;
				output[4] = (TranLow)x6;
				output[5] = (TranLow)x14;
				output[6] = (TranLow)x10;
				output[7] = (TranLow)x2;
				output[8] = (TranLow)x3;
				output[9] = (TranLow)x11;
				output[10] = (TranLow)x15;
				output[11] = (TranLow)x7;
				output[12] = (TranLow)x5;
				output[13] = (TranLow)-x13;
				output[14] = (TranLow)x9;
				output[15] = (TranLow)-x1;
			}

			// --- idct32 (inv_txfm.c idct32_c) ---
			void Idct32(const TranLow *input, TranLow *output) {
				int16 step1[32], step2[32];
				TranHigh temp1, temp2;
				// étape 1 : réordonnancement + papillons impairs Q14
				static const int32 kEvenIn[16] = {0,  16, 8,  24, 4,  20, 12, 28,
												  2,  18, 10, 26, 6,  22, 14, 30};
				for (int32 i = 0; i < 16; ++i)
					step1[i] = (int16)input[kEvenIn[i]];
				// (paires : entrée impaire basse/haute, cospi bas/haut — ordre normatif)
				static const int32 kOddIn[8][2] = {{1, 31}, {17, 15}, {9, 23}, {25, 7},
												   {5, 27}, {21, 11}, {13, 19}, {29, 3}};
				static const int32 kOddCos[8] = {31, 15, 23, 7, 27, 11, 19, 3};
				for (int32 i = 0; i < 8; ++i) {
					const int16 a = (int16)input[kOddIn[i][0]];
					const int16 b = (int16)input[kOddIn[i][1]];
					const TranHigh cLo = NK_COSPI(kOddCos[i]);
					const TranHigh cHi = NK_COSPI(32 - kOddCos[i]);
					step1[16 + i] = (int16)DctRound(a * cLo - b * cHi);
					step1[31 - i] = (int16)DctRound(a * cHi + b * cLo);
				}
				// étape 2
				for (int32 i = 0; i < 8; ++i)
					step2[i] = step1[i];
				temp1 = step1[8] * NK_COSPI(30) - step1[15] * NK_COSPI(2);
				temp2 = step1[8] * NK_COSPI(2) + step1[15] * NK_COSPI(30);
				step2[8] = (int16)DctRound(temp1);
				step2[15] = (int16)DctRound(temp2);
				temp1 = step1[9] * NK_COSPI(14) - step1[14] * NK_COSPI(18);
				temp2 = step1[9] * NK_COSPI(18) + step1[14] * NK_COSPI(14);
				step2[9] = (int16)DctRound(temp1);
				step2[14] = (int16)DctRound(temp2);
				temp1 = step1[10] * NK_COSPI(22) - step1[13] * NK_COSPI(10);
				temp2 = step1[10] * NK_COSPI(10) + step1[13] * NK_COSPI(22);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				temp1 = step1[11] * NK_COSPI(6) - step1[12] * NK_COSPI(26);
				temp2 = step1[11] * NK_COSPI(26) + step1[12] * NK_COSPI(6);
				step2[11] = (int16)DctRound(temp1);
				step2[12] = (int16)DctRound(temp2);
				step2[16] = (int16)(step1[16] + step1[17]);
				step2[17] = (int16)(step1[16] - step1[17]);
				step2[18] = (int16)(-step1[18] + step1[19]);
				step2[19] = (int16)(step1[18] + step1[19]);
				step2[20] = (int16)(step1[20] + step1[21]);
				step2[21] = (int16)(step1[20] - step1[21]);
				step2[22] = (int16)(-step1[22] + step1[23]);
				step2[23] = (int16)(step1[22] + step1[23]);
				step2[24] = (int16)(step1[24] + step1[25]);
				step2[25] = (int16)(step1[24] - step1[25]);
				step2[26] = (int16)(-step1[26] + step1[27]);
				step2[27] = (int16)(step1[26] + step1[27]);
				step2[28] = (int16)(step1[28] + step1[29]);
				step2[29] = (int16)(step1[28] - step1[29]);
				step2[30] = (int16)(-step1[30] + step1[31]);
				step2[31] = (int16)(step1[30] + step1[31]);
				// étape 3
				step1[0] = step2[0];
				step1[1] = step2[1];
				step1[2] = step2[2];
				step1[3] = step2[3];
				temp1 = step2[4] * NK_COSPI(28) - step2[7] * NK_COSPI(4);
				temp2 = step2[4] * NK_COSPI(4) + step2[7] * NK_COSPI(28);
				step1[4] = (int16)DctRound(temp1);
				step1[7] = (int16)DctRound(temp2);
				temp1 = step2[5] * NK_COSPI(12) - step2[6] * NK_COSPI(20);
				temp2 = step2[5] * NK_COSPI(20) + step2[6] * NK_COSPI(12);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				step1[8] = (int16)(step2[8] + step2[9]);
				step1[9] = (int16)(step2[8] - step2[9]);
				step1[10] = (int16)(-step2[10] + step2[11]);
				step1[11] = (int16)(step2[10] + step2[11]);
				step1[12] = (int16)(step2[12] + step2[13]);
				step1[13] = (int16)(step2[12] - step2[13]);
				step1[14] = (int16)(-step2[14] + step2[15]);
				step1[15] = (int16)(step2[14] + step2[15]);
				step1[16] = step2[16];
				step1[31] = step2[31];
				temp1 = -step2[17] * NK_COSPI(4) + step2[30] * NK_COSPI(28);
				temp2 = step2[17] * NK_COSPI(28) + step2[30] * NK_COSPI(4);
				step1[17] = (int16)DctRound(temp1);
				step1[30] = (int16)DctRound(temp2);
				temp1 = -step2[18] * NK_COSPI(28) - step2[29] * NK_COSPI(4);
				temp2 = -step2[18] * NK_COSPI(4) + step2[29] * NK_COSPI(28);
				step1[18] = (int16)DctRound(temp1);
				step1[29] = (int16)DctRound(temp2);
				step1[19] = step2[19];
				step1[20] = step2[20];
				temp1 = -step2[21] * NK_COSPI(20) + step2[26] * NK_COSPI(12);
				temp2 = step2[21] * NK_COSPI(12) + step2[26] * NK_COSPI(20);
				step1[21] = (int16)DctRound(temp1);
				step1[26] = (int16)DctRound(temp2);
				temp1 = -step2[22] * NK_COSPI(12) - step2[25] * NK_COSPI(20);
				temp2 = -step2[22] * NK_COSPI(20) + step2[25] * NK_COSPI(12);
				step1[22] = (int16)DctRound(temp1);
				step1[25] = (int16)DctRound(temp2);
				step1[23] = step2[23];
				step1[24] = step2[24];
				step1[27] = step2[27];
				step1[28] = step2[28];
				// étape 4
				temp1 = (step1[0] + step1[1]) * NK_COSPI(16);
				temp2 = (step1[0] - step1[1]) * NK_COSPI(16);
				step2[0] = (int16)DctRound(temp1);
				step2[1] = (int16)DctRound(temp2);
				temp1 = step1[2] * NK_COSPI(24) - step1[3] * NK_COSPI(8);
				temp2 = step1[2] * NK_COSPI(8) + step1[3] * NK_COSPI(24);
				step2[2] = (int16)DctRound(temp1);
				step2[3] = (int16)DctRound(temp2);
				step2[4] = (int16)(step1[4] + step1[5]);
				step2[5] = (int16)(step1[4] - step1[5]);
				step2[6] = (int16)(-step1[6] + step1[7]);
				step2[7] = (int16)(step1[6] + step1[7]);
				step2[8] = step1[8];
				step2[15] = step1[15];
				temp1 = -step1[9] * NK_COSPI(8) + step1[14] * NK_COSPI(24);
				temp2 = step1[9] * NK_COSPI(24) + step1[14] * NK_COSPI(8);
				step2[9] = (int16)DctRound(temp1);
				step2[14] = (int16)DctRound(temp2);
				temp1 = -step1[10] * NK_COSPI(24) - step1[13] * NK_COSPI(8);
				temp2 = -step1[10] * NK_COSPI(8) + step1[13] * NK_COSPI(24);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				step2[11] = step1[11];
				step2[12] = step1[12];
				step2[16] = (int16)(step1[16] + step1[19]);
				step2[17] = (int16)(step1[17] + step1[18]);
				step2[18] = (int16)(step1[17] - step1[18]);
				step2[19] = (int16)(step1[16] - step1[19]);
				step2[20] = (int16)(-step1[20] + step1[23]);
				step2[21] = (int16)(-step1[21] + step1[22]);
				step2[22] = (int16)(step1[21] + step1[22]);
				step2[23] = (int16)(step1[20] + step1[23]);
				step2[24] = (int16)(step1[24] + step1[27]);
				step2[25] = (int16)(step1[25] + step1[26]);
				step2[26] = (int16)(step1[25] - step1[26]);
				step2[27] = (int16)(step1[24] - step1[27]);
				step2[28] = (int16)(-step1[28] + step1[31]);
				step2[29] = (int16)(-step1[29] + step1[30]);
				step2[30] = (int16)(step1[29] + step1[30]);
				step2[31] = (int16)(step1[28] + step1[31]);
				// étape 5
				step1[0] = (int16)(step2[0] + step2[3]);
				step1[1] = (int16)(step2[1] + step2[2]);
				step1[2] = (int16)(step2[1] - step2[2]);
				step1[3] = (int16)(step2[0] - step2[3]);
				step1[4] = step2[4];
				temp1 = (step2[6] - step2[5]) * NK_COSPI(16);
				temp2 = (step2[5] + step2[6]) * NK_COSPI(16);
				step1[5] = (int16)DctRound(temp1);
				step1[6] = (int16)DctRound(temp2);
				step1[7] = step2[7];
				step1[8] = (int16)(step2[8] + step2[11]);
				step1[9] = (int16)(step2[9] + step2[10]);
				step1[10] = (int16)(step2[9] - step2[10]);
				step1[11] = (int16)(step2[8] - step2[11]);
				step1[12] = (int16)(-step2[12] + step2[15]);
				step1[13] = (int16)(-step2[13] + step2[14]);
				step1[14] = (int16)(step2[13] + step2[14]);
				step1[15] = (int16)(step2[12] + step2[15]);
				step1[16] = step2[16];
				step1[17] = step2[17];
				temp1 = -step2[18] * NK_COSPI(8) + step2[29] * NK_COSPI(24);
				temp2 = step2[18] * NK_COSPI(24) + step2[29] * NK_COSPI(8);
				step1[18] = (int16)DctRound(temp1);
				step1[29] = (int16)DctRound(temp2);
				temp1 = -step2[19] * NK_COSPI(8) + step2[28] * NK_COSPI(24);
				temp2 = step2[19] * NK_COSPI(24) + step2[28] * NK_COSPI(8);
				step1[19] = (int16)DctRound(temp1);
				step1[28] = (int16)DctRound(temp2);
				temp1 = -step2[20] * NK_COSPI(24) - step2[27] * NK_COSPI(8);
				temp2 = -step2[20] * NK_COSPI(8) + step2[27] * NK_COSPI(24);
				step1[20] = (int16)DctRound(temp1);
				step1[27] = (int16)DctRound(temp2);
				temp1 = -step2[21] * NK_COSPI(24) - step2[26] * NK_COSPI(8);
				temp2 = -step2[21] * NK_COSPI(8) + step2[26] * NK_COSPI(24);
				step1[21] = (int16)DctRound(temp1);
				step1[26] = (int16)DctRound(temp2);
				step1[22] = step2[22];
				step1[23] = step2[23];
				step1[24] = step2[24];
				step1[25] = step2[25];
				step1[30] = step2[30];
				step1[31] = step2[31];
				// étape 6
				step2[0] = (int16)(step1[0] + step1[7]);
				step2[1] = (int16)(step1[1] + step1[6]);
				step2[2] = (int16)(step1[2] + step1[5]);
				step2[3] = (int16)(step1[3] + step1[4]);
				step2[4] = (int16)(step1[3] - step1[4]);
				step2[5] = (int16)(step1[2] - step1[5]);
				step2[6] = (int16)(step1[1] - step1[6]);
				step2[7] = (int16)(step1[0] - step1[7]);
				step2[8] = step1[8];
				step2[9] = step1[9];
				temp1 = (-step1[10] + step1[13]) * NK_COSPI(16);
				temp2 = (step1[10] + step1[13]) * NK_COSPI(16);
				step2[10] = (int16)DctRound(temp1);
				step2[13] = (int16)DctRound(temp2);
				temp1 = (-step1[11] + step1[12]) * NK_COSPI(16);
				temp2 = (step1[11] + step1[12]) * NK_COSPI(16);
				step2[11] = (int16)DctRound(temp1);
				step2[12] = (int16)DctRound(temp2);
				step2[14] = step1[14];
				step2[15] = step1[15];
				step2[16] = (int16)(step1[16] + step1[23]);
				step2[17] = (int16)(step1[17] + step1[22]);
				step2[18] = (int16)(step1[18] + step1[21]);
				step2[19] = (int16)(step1[19] + step1[20]);
				step2[20] = (int16)(step1[19] - step1[20]);
				step2[21] = (int16)(step1[18] - step1[21]);
				step2[22] = (int16)(step1[17] - step1[22]);
				step2[23] = (int16)(step1[16] - step1[23]);
				step2[24] = (int16)(-step1[24] + step1[31]);
				step2[25] = (int16)(-step1[25] + step1[30]);
				step2[26] = (int16)(-step1[26] + step1[29]);
				step2[27] = (int16)(-step1[27] + step1[28]);
				step2[28] = (int16)(step1[27] + step1[28]);
				step2[29] = (int16)(step1[26] + step1[29]);
				step2[30] = (int16)(step1[25] + step1[30]);
				step2[31] = (int16)(step1[24] + step1[31]);
				// étape 7
				for (int32 i = 0; i < 8; ++i) {
					step1[i] = (int16)(step2[i] + step2[15 - i]);
					step1[8 + i] = (int16)(step2[7 - i] - step2[8 + i]);
				}
				step1[16] = step2[16];
				step1[17] = step2[17];
				step1[18] = step2[18];
				step1[19] = step2[19];
				temp1 = (-step2[20] + step2[27]) * NK_COSPI(16);
				temp2 = (step2[20] + step2[27]) * NK_COSPI(16);
				step1[20] = (int16)DctRound(temp1);
				step1[27] = (int16)DctRound(temp2);
				temp1 = (-step2[21] + step2[26]) * NK_COSPI(16);
				temp2 = (step2[21] + step2[26]) * NK_COSPI(16);
				step1[21] = (int16)DctRound(temp1);
				step1[26] = (int16)DctRound(temp2);
				temp1 = (-step2[22] + step2[25]) * NK_COSPI(16);
				temp2 = (step2[22] + step2[25]) * NK_COSPI(16);
				step1[22] = (int16)DctRound(temp1);
				step1[25] = (int16)DctRound(temp2);
				temp1 = (-step2[23] + step2[24]) * NK_COSPI(16);
				temp2 = (step2[23] + step2[24]) * NK_COSPI(16);
				step1[23] = (int16)DctRound(temp1);
				step1[24] = (int16)DctRound(temp2);
				step1[28] = step2[28];
				step1[29] = step2[29];
				step1[30] = step2[30];
				step1[31] = step2[31];
				// étape finale
				for (int32 i = 0; i < 16; ++i) {
					output[i] = (TranLow)(step1[i] + step1[31 - i]);
					output[31 - i] = (TranLow)(step1[i] - step1[31 - i]);
				}
			}

			typedef void (*Txfm1D)(const TranLow *, TranLow *);

			// Passe lignes puis colonnes + ajout au bloc prédit (shift 4/5/6/6).
			void Transform2DAdd(const TranLow *input, uint8 *dest, int32 stride, Txfm1D rows,
								Txfm1D cols, int32 n, int32 shift) {
				TranLow out[32 * 32];
				TranLow tempIn[32], tempOut[32];
				for (int32 i = 0; i < n; ++i)
					rows(input + i * n, out + i * n);
				for (int32 i = 0; i < n; ++i) {
					for (int32 j = 0; j < n; ++j)
						tempIn[j] = out[j * n + i];
					cols(tempIn, tempOut);
					for (int32 j = 0; j < n; ++j)
						dest[j * stride + i] =
							ClipPixelAdd(dest[j * stride + i], RoundPow2(tempOut[j], shift));
				}
			}

		} // namespace

		void NkVp9Itxfm::Iht4x4Add(const int16 *input, uint8 *dest, int32 stride, int32 txType) {
			static const Txfm1D kIht4[4][2] = {{Idct4, Idct4},
											   {Iadst4, Idct4},
											   {Idct4, Iadst4},
											   {Iadst4, Iadst4}};
			Transform2DAdd(input, dest, stride, kIht4[txType][0], kIht4[txType][1], 4, 4);
		}

		void NkVp9Itxfm::Iht8x8Add(const int16 *input, uint8 *dest, int32 stride, int32 txType) {
			static const Txfm1D kIht8[4][2] = {{Idct8, Idct8},
											   {Iadst8, Idct8},
											   {Idct8, Iadst8},
											   {Iadst8, Iadst8}};
			Transform2DAdd(input, dest, stride, kIht8[txType][0], kIht8[txType][1], 8, 5);
		}

		void NkVp9Itxfm::Iht16x16Add(const int16 *input, uint8 *dest, int32 stride, int32 txType) {
			static const Txfm1D kIht16[4][2] = {{Idct16, Idct16},
												{Iadst16, Idct16},
												{Idct16, Iadst16},
												{Iadst16, Iadst16}};
			Transform2DAdd(input, dest, stride, kIht16[txType][0], kIht16[txType][1], 16, 6);
		}

		void NkVp9Itxfm::Idct32x32Add(const int16 *input, uint8 *dest, int32 stride) {
			// Lignes : saute les lignes entièrement nulles (résultat identique).
			TranLow out[32 * 32];
			TranLow tempIn[32], tempOut[32];
			for (int32 i = 0; i < 32; ++i) {
				int16 zero = 0;
				for (int32 j = 0; j < 32; ++j)
					zero |= (int16)input[i * 32 + j];
				if (zero)
					Idct32(input + i * 32, out + i * 32);
				else
					for (int32 j = 0; j < 32; ++j)
						out[i * 32 + j] = 0;
			}
			for (int32 i = 0; i < 32; ++i) {
				for (int32 j = 0; j < 32; ++j)
					tempIn[j] = out[j * 32 + i];
				Idct32(tempIn, tempOut);
				for (int32 j = 0; j < 32; ++j)
					dest[j * stride + i] = ClipPixelAdd(dest[j * stride + i], RoundPow2(tempOut[j], 6));
			}
		}

		void NkVp9Itxfm::Iwht4x4Add(const int16 *input, uint8 *dest, int32 stride) {
			// WHT réversible (lossless) — coefficients pré-décalés de UNIT_QUANT_SHIFT=2.
			TranLow output[16];
			TranHigh a1, b1, c1, d1, e1;
			const TranLow *ip = input;
			TranLow *op = output;
			for (int32 i = 0; i < 4; ++i) {
				a1 = ip[0] >> 2;
				c1 = ip[1] >> 2;
				d1 = ip[2] >> 2;
				b1 = ip[3] >> 2;
				a1 += c1;
				d1 -= b1;
				e1 = (a1 - d1) >> 1;
				b1 = e1 - b1;
				c1 = e1 - c1;
				a1 -= b1;
				d1 += c1;
				op[0] = (TranLow)a1;
				op[1] = (TranLow)b1;
				op[2] = (TranLow)c1;
				op[3] = (TranLow)d1;
				ip += 4;
				op += 4;
			}
			ip = output;
			for (int32 i = 0; i < 4; ++i) {
				a1 = ip[4 * 0];
				c1 = ip[4 * 1];
				d1 = ip[4 * 2];
				b1 = ip[4 * 3];
				a1 += c1;
				d1 -= b1;
				e1 = (a1 - d1) >> 1;
				b1 = e1 - b1;
				c1 = e1 - c1;
				a1 -= b1;
				d1 += c1;
				dest[stride * 0] = ClipPixelAdd(dest[stride * 0], (int32)(TranLow)a1);
				dest[stride * 1] = ClipPixelAdd(dest[stride * 1], (int32)(TranLow)b1);
				dest[stride * 2] = ClipPixelAdd(dest[stride * 2], (int32)(TranLow)c1);
				dest[stride * 3] = ClipPixelAdd(dest[stride * 3], (int32)(TranLow)d1);
				++ip;
				++dest;
			}
		}

		bool NkVp9Itxfm::SelfTest() {
			// DC pur : input[0]=64 (Q… arbitraire) → sortie constante attendue via la
			// formule normative (deux DctRound de cospi_16 + arrondi final).
			{
				int16 in[16] = {0};
				in[0] = 64;
				uint8 dst[16] = {0};
				Iht4x4Add(in, dst, 4, 0);
				const int32 a = (int32)((64 * 11585 + (1 << 13)) >> 14);
				const int32 b = (int32)(((TranHigh)a * 11585 + (1 << 13)) >> 14);
				const uint8 expect = (uint8)((b + 8) >> 4);
				for (int32 i = 0; i < 16; ++i)
					if (dst[i] != expect)
						return false;
			}
			// WHT : DC=4 (<<2 déjà appliqué par convention d'entrée) → +1 partout.
			{
				int16 in[16] = {0};
				in[0] = 16; // (16>>2)=4 → a1=4 → e1... sortie DC=1 par pixel
				uint8 dst[16];
				for (int32 i = 0; i < 16; ++i)
					dst[i] = 100;
				Iwht4x4Add(in, dst, 4);
				for (int32 i = 0; i < 16; ++i)
					if (dst[i] != 101)
						return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
