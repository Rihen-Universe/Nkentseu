// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkResampler.cpp
// Port fidèle de libopus (resampler.c, resampler_private_IIR_FIR.c,
// resampler_private_up2_HQ.c, resampler_rom.c) — chemin décodeur (upsampling).
// =============================================================================
#include "NkSilkResampler.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		// ── Tables ROM (libopus silk/resampler_rom.h) ────────────────────────────
		static const int16 kUp2HQ_0[3] = {1746, 14986, 39083 - 65536};
		static const int16 kUp2HQ_1[3] = {6854, 25769, 55542 - 65536};
		static const int16 kFracFIR12[12][4] = {
			{189, -600, 617, 30567},  {117, -159, -1070, 29704}, {52, 221, -2392, 28276},
			{-4, 529, -3350, 26341},  {-48, 758, -3956, 23973},  {-80, 905, -4235, 21254},
			{-99, 972, -4222, 18278}, {-107, 967, -3957, 15143},  {-103, 896, -3487, 11950},
			{-91, 773, -2865, 8798},  {-71, 611, -2143, 5784},    {-46, 425, -1375, 2996},
		};

		// silk/resampler.c : delay_matrix_dec[3][6] (in 8/12/16 × out 8/12/16/24/48/96).
		static const int8 kDelayDec[3][6] = {
			{4, 0, 2, 0, 0, 0}, {0, 9, 4, 7, 4, 4}, {0, 3, 12, 7, 7, 7}};

		static inline int32 rateID(int32 R) {
			return M::minInt(5, ((((R >> 12) - (R > 16000 ? 1 : 0)) >> (R > 24000 ? 1 : 0)) - 1));
		}

		// silk_resampler_private_up2_HQ : upsample 2x (filtres passe-tout, état Q10).
		static void up2HQ(int32 *S, int16 *out, const int16 *in, int32 len) {
			for (int32 k = 0; k < len; k++) {
				const int32 in32 = M::LSHIFT((int32)in[k], 10);
				int32 Y, X, out32_1, out32_2;
				// Échantillon pair.
				Y = M::SUB32(in32, S[0]);
				X = M::SMULWB(Y, kUp2HQ_0[0]);
				out32_1 = M::ADD32(S[0], X);
				S[0] = M::ADD32(in32, X);
				Y = M::SUB32(out32_1, S[1]);
				X = M::SMULWB(Y, kUp2HQ_0[1]);
				out32_2 = M::ADD32(S[1], X);
				S[1] = M::ADD32(out32_1, X);
				Y = M::SUB32(out32_2, S[2]);
				X = M::SMLAWB(Y, Y, kUp2HQ_0[2]);
				out32_1 = M::ADD32(S[2], X);
				S[2] = M::ADD32(out32_2, X);
				out[2 * k] = (int16)M::SAT16(M::RSHIFT_ROUND(out32_1, 10));
				// Échantillon impair.
				Y = M::SUB32(in32, S[3]);
				X = M::SMULWB(Y, kUp2HQ_1[0]);
				out32_1 = M::ADD32(S[3], X);
				S[3] = M::ADD32(in32, X);
				Y = M::SUB32(out32_1, S[4]);
				X = M::SMULWB(Y, kUp2HQ_1[1]);
				out32_2 = M::ADD32(S[4], X);
				S[4] = M::ADD32(out32_1, X);
				Y = M::SUB32(out32_2, S[5]);
				X = M::SMLAWB(Y, Y, kUp2HQ_1[2]);
				out32_1 = M::ADD32(S[5], X);
				S[5] = M::ADD32(out32_2, X);
				out[2 * k + 1] = (int16)M::SAT16(M::RSHIFT_ROUND(out32_1, 10));
			}
		}

		// silk_resampler_private_IIR_FIR_INTERPOL : interpolation FIR fractionnaire.
		static int16 *interpol(int16 *out, int16 *buf, int32 max_index_Q16, int32 index_increment_Q16) {
			for (int32 index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16) {
				const int32 table_index = M::SMULWB(index_Q16 & 0xFFFF, 12);
				int16 *bp = &buf[index_Q16 >> 16];
				int32 res = M::SMULBB(bp[0], kFracFIR12[table_index][0]);
				res = M::SMLABB_ovflw(res, bp[1], kFracFIR12[table_index][1]);
				res = M::SMLABB_ovflw(res, bp[2], kFracFIR12[table_index][2]);
				res = M::SMLABB_ovflw(res, bp[3], kFracFIR12[table_index][3]);
				res = M::SMLABB_ovflw(res, bp[4], kFracFIR12[11 - table_index][3]);
				res = M::SMLABB_ovflw(res, bp[5], kFracFIR12[11 - table_index][2]);
				res = M::SMLABB_ovflw(res, bp[6], kFracFIR12[11 - table_index][1]);
				res = M::SMLABB_ovflw(res, bp[7], kFracFIR12[11 - table_index][0]);
				*out++ = (int16)M::SAT16(M::RSHIFT_ROUND(res, 15));
			}
			return out;
		}

		// silk_resampler_private_IIR_FIR.
		static void iirFir(NkSilkResampler &S, int16 *out, const int16 *in, int32 inLen) {
			int16 buf[2 * NkSilkResampler::kMaxBatchMs * 16 + NkSilkResampler::kOrderFIR12];
			for (int32 i = 0; i < NkSilkResampler::kOrderFIR12; i++)
				buf[i] = S.sFIR_i16[i];
			const int32 inc = S.invRatio_Q16;
			int32 nIn = 0;
			while (true) {
				nIn = M::minInt(inLen, S.batchSize);
				up2HQ(S.sIIR, &buf[NkSilkResampler::kOrderFIR12], in, nIn);
				const int32 max_index_Q16 = M::LSHIFT(nIn, 16 + 1);
				out = interpol(out, buf, max_index_Q16, inc);
				in += nIn;
				inLen -= nIn;
				if (inLen > 0) {
					for (int32 i = 0; i < NkSilkResampler::kOrderFIR12; i++)
						buf[i] = buf[(nIn << 1) + i];
				} else {
					break;
				}
			}
			for (int32 i = 0; i < NkSilkResampler::kOrderFIR12; i++)
				S.sFIR_i16[i] = buf[(nIn << 1) + i];
		}

		void NkSilkResampler::Init(int32 Fs_Hz_in, int32 Fs_Hz_out) {
			for (int32 i = 0; i < kMaxIIROrder; i++)
				sIIR[i] = 0;
			for (int32 i = 0; i < kMaxFIROrder; i++)
				sFIR_i16[i] = 0;
			for (int32 i = 0; i < 96; i++)
				delayBuf[i] = 0;

			inputDelay = kDelayDec[rateID(Fs_Hz_in)][rateID(Fs_Hz_out)];
			Fs_in_kHz = M::DIV32_16(Fs_Hz_in, 1000);
			Fs_out_kHz = M::DIV32_16(Fs_Hz_out, 1000);
			batchSize = Fs_in_kHz * kMaxBatchMs;

			int32 up2x = 0;
			if (Fs_Hz_out > Fs_Hz_in) {
				if (Fs_Hz_out == Fs_Hz_in * 2) {
					function = 1; // up2 wrapper (non utilisé pour 8/12/16→48)
				} else {
					function = 2; // IIR_FIR
					up2x = 1;
				}
			} else if (Fs_Hz_out < Fs_Hz_in) {
				function = 3; // down FIR (non porté : décodeur upsample)
			} else {
				function = 0; // copie
			}

			invRatio_Q16 = M::LSHIFT(M::DIV32(M::LSHIFT(Fs_Hz_in, 14 + up2x), Fs_Hz_out), 2);
			while (M::SMULWW(invRatio_Q16, Fs_Hz_out) < M::LSHIFT(Fs_Hz_in, up2x))
				invRatio_Q16++;
		}

		void NkSilkResampler::Process(int16 *out, const int16 *in, int32 inLen) {
			const int32 nSamples = Fs_in_kHz - inputDelay;
			for (int32 i = 0; i < nSamples; i++)
				delayBuf[inputDelay + i] = in[i];

			if (function == 2) {
				iirFir(*this, out, delayBuf, Fs_in_kHz);
				iirFir(*this, &out[Fs_out_kHz], &in[nSamples], inLen - Fs_in_kHz);
			} else {
				// copie (Fs_in == Fs_out) — chemin décodeur trivial.
				for (int32 i = 0; i < Fs_in_kHz; i++)
					out[i] = delayBuf[i];
				for (int32 i = 0; i < inLen - Fs_in_kHz; i++)
					out[Fs_out_kHz + i] = in[nSamples + i];
			}

			for (int32 i = 0; i < inputDelay; i++)
				delayBuf[i] = in[inLen - inputDelay + i];
		}

		bool NkSilkResampler::SelfTest() {
			// Sanité : 16k→48k produit 3× d'échantillons, sortie bornée, déterministe.
			NkSilkResampler a, b;
			a.Init(16000, 48000);
			b.Init(16000, 48000);
			if (a.inputDelay != 7 || a.Fs_out_kHz != 48 || a.function != 2)
				return false;
			int16 in[320], outA[960], outB[960];
			for (int32 i = 0; i < 320; i++)
				in[i] = (int16)(8000.0 * (i % 40 < 20 ? 1 : -1)); // onde carrée
			a.Process(outA, in, 320);
			b.Process(outB, in, 320);
			for (int32 i = 0; i < 960; i++) {
				if (outA[i] != outB[i])
					return false; // déterministe
				if (outA[i] > 32767 || outA[i] < -32768)
					return false;
			}
			// L'énergie de sortie doit être non nulle (le signal passe).
			int64 e = 0;
			for (int32 i = 0; i < 960; i++)
				e += (int64)outA[i] * outA[i];
			return e > 0;
		}

	} // namespace media
} // namespace nkentseu
