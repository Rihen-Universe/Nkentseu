// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkExcitation.cpp
// Port fidèle de libopus (decode_pulses.c, shell_coder.c, code_signs.c).
// =============================================================================
#include "NkSilkExcitation.h"
#include "NkSilkMath.h"

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		static constexpr int32 SHELL = 16;			  // SHELL_CODEC_FRAME_LENGTH
		static constexpr int32 LOG2_SHELL = 4;		  // LOG2_SHELL_CODEC_FRAME_LENGTH
		static constexpr int32 SILK_MAX_PULSES = 16;  // seuil d'extension LSB
		static constexpr int32 N_RATE_LEVELS = 10;
		static constexpr int32 kMaxShellBlocks = 24;  // MAX_FRAME_LENGTH/16 (=20) + marge

		// silk_dec_map(x) = 2x-1 (0→-1, 1→+1) ; silk_enc_map(x) = (x>>15)+1.
		static inline int32 decMap(int32 x) {
			return M::LSHIFT(x, 1) - 1;
		}
		static inline int32 encMap(int32 x) {
			return M::RSHIFT(x, 15) + 1;
		}

		// ── Décodage shell (arbre binaire de splits) ─────────────────────────────
		static void decodeSplit(int16 *p_child1, int16 *p_child2, NkOpusRangeDecoder &dec, int32 p,
								const uint8 *shell_table) {
			if (p > 0) {
				p_child1[0] = (int16)dec.DecodeIcdf(&shell_table[kSilk_shell_code_table_offsets[p]], 8);
				p_child2[0] = (int16)(p - p_child1[0]);
			} else {
				p_child1[0] = 0;
				p_child2[0] = 0;
			}
		}

		static void shellDecoder(int16 *pulses0, NkOpusRangeDecoder &dec, int32 pulses4) {
			int16 pulses3[2], pulses2[4], pulses1[8];
			decodeSplit(&pulses3[0], &pulses3[1], dec, pulses4, kSilk_shell_code_table3);
			decodeSplit(&pulses2[0], &pulses2[1], dec, pulses3[0], kSilk_shell_code_table2);
			decodeSplit(&pulses1[0], &pulses1[1], dec, pulses2[0], kSilk_shell_code_table1);
			decodeSplit(&pulses0[0], &pulses0[1], dec, pulses1[0], kSilk_shell_code_table0);
			decodeSplit(&pulses0[2], &pulses0[3], dec, pulses1[1], kSilk_shell_code_table0);
			decodeSplit(&pulses1[2], &pulses1[3], dec, pulses2[1], kSilk_shell_code_table1);
			decodeSplit(&pulses0[4], &pulses0[5], dec, pulses1[2], kSilk_shell_code_table0);
			decodeSplit(&pulses0[6], &pulses0[7], dec, pulses1[3], kSilk_shell_code_table0);
			decodeSplit(&pulses2[2], &pulses2[3], dec, pulses3[1], kSilk_shell_code_table2);
			decodeSplit(&pulses1[4], &pulses1[5], dec, pulses2[2], kSilk_shell_code_table1);
			decodeSplit(&pulses0[8], &pulses0[9], dec, pulses1[4], kSilk_shell_code_table0);
			decodeSplit(&pulses0[10], &pulses0[11], dec, pulses1[5], kSilk_shell_code_table0);
			decodeSplit(&pulses1[6], &pulses1[7], dec, pulses2[3], kSilk_shell_code_table1);
			decodeSplit(&pulses0[12], &pulses0[13], dec, pulses1[6], kSilk_shell_code_table0);
			decodeSplit(&pulses0[14], &pulses0[15], dec, pulses1[7], kSilk_shell_code_table0);
		}

		// silk_decode_signs
		static void decodeSigns(NkOpusRangeDecoder &dec, int16 *pulses, int32 length, int32 signalType,
								int32 quantOffsetType, const int32 *sum_pulses) {
			uint8 icdf[2];
			icdf[1] = 0;
			int16 *q_ptr = pulses;
			const int32 base = M::SMULBB(7, quantOffsetType + M::LSHIFT(signalType, 1));
			const uint8 *icdf_ptr = &kSilk_sign_iCDF[base];
			length = M::RSHIFT(length + SHELL / 2, LOG2_SHELL);
			for (int32 i = 0; i < length; i++) {
				const int32 p = sum_pulses[i];
				if (p > 0) {
					icdf[0] = icdf_ptr[M::minInt(p & 0x1F, 6)];
					for (int32 j = 0; j < SHELL; j++) {
						if (q_ptr[j] > 0) {
							q_ptr[j] = (int16)(q_ptr[j] * decMap((int32)dec.DecodeIcdf(icdf, 8)));
						}
					}
				}
				q_ptr += SHELL;
			}
		}

		// silk_decode_pulses
		void NkSilkExcitation::DecodePulses(NkOpusRangeDecoder &dec, int16 *pulses, int32 signalType,
											int32 quantOffsetType, int32 frame_length) {
			int32 sum_pulses[kMaxShellBlocks], nLshifts[kMaxShellBlocks];

			const int32 RateLevelIndex = dec.DecodeIcdf(kSilk_rate_levels_iCDF[signalType >> 1], 8);

			int32 iter = M::RSHIFT(frame_length, LOG2_SHELL);
			if (iter * SHELL < frame_length)
				iter++; // 10 ms @ 12 kHz

			const uint8 *cdf_ptr = kSilk_pulses_per_block_iCDF[RateLevelIndex];
			for (int32 i = 0; i < iter; i++) {
				nLshifts[i] = 0;
				sum_pulses[i] = dec.DecodeIcdf(cdf_ptr, 8);
				while (sum_pulses[i] == SILK_MAX_PULSES + 1) {
					nLshifts[i]++;
					sum_pulses[i] = dec.DecodeIcdf(
						&kSilk_pulses_per_block_iCDF[N_RATE_LEVELS - 1][0] + (nLshifts[i] == 10 ? 1 : 0), 8);
				}
			}

			for (int32 i = 0; i < iter; i++) {
				if (sum_pulses[i] > 0) {
					shellDecoder(&pulses[i * SHELL], dec, sum_pulses[i]);
				} else {
					for (int32 k = 0; k < SHELL; k++)
						pulses[i * SHELL + k] = 0;
				}
			}

			for (int32 i = 0; i < iter; i++) {
				if (nLshifts[i] > 0) {
					const int32 nLS = nLshifts[i];
					int16 *pp = &pulses[i * SHELL];
					for (int32 k = 0; k < SHELL; k++) {
						int32 abs_q = pp[k];
						for (int32 j = 0; j < nLS; j++) {
							abs_q = M::LSHIFT(abs_q, 1);
							abs_q += dec.DecodeIcdf(kSilk_lsb_iCDF, 8);
						}
						pp[k] = (int16)abs_q;
					}
					sum_pulses[i] |= nLS << 5;
				}
			}

			decodeSigns(dec, pulses, frame_length, signalType, quantOffsetType, sum_pulses);
		}

		// ── Encodeur miroir (pour le self-test round-trip uniquement) ────────────
		static void encodeSplit(NkOpusRangeEncoder &enc, int32 p_child1, int32 p, const uint8 *shell_table) {
			if (p > 0)
				enc.EncodeIcdf(p_child1, &shell_table[kSilk_shell_code_table_offsets[p]], 8);
		}
		static void combinePulses(int32 *out, const int32 *in, int32 len) {
			for (int32 k = 0; k < len; k++)
				out[k] = in[2 * k] + in[2 * k + 1];
		}
		static void shellEncoder(NkOpusRangeEncoder &enc, const int32 *pulses0) {
			int32 p1[8], p2[4], p3[2], p4[1];
			combinePulses(p1, pulses0, 8);
			combinePulses(p2, p1, 4);
			combinePulses(p3, p2, 2);
			combinePulses(p4, p3, 1);
			encodeSplit(enc, p3[0], p4[0], kSilk_shell_code_table3);
			encodeSplit(enc, p2[0], p3[0], kSilk_shell_code_table2);
			encodeSplit(enc, p1[0], p2[0], kSilk_shell_code_table1);
			encodeSplit(enc, pulses0[0], p1[0], kSilk_shell_code_table0);
			encodeSplit(enc, pulses0[2], p1[1], kSilk_shell_code_table0);
			encodeSplit(enc, p1[2], p2[1], kSilk_shell_code_table1);
			encodeSplit(enc, pulses0[4], p1[2], kSilk_shell_code_table0);
			encodeSplit(enc, pulses0[6], p1[3], kSilk_shell_code_table0);
			encodeSplit(enc, p2[2], p3[1], kSilk_shell_code_table2);
			encodeSplit(enc, p1[4], p2[2], kSilk_shell_code_table1);
			encodeSplit(enc, pulses0[8], p1[4], kSilk_shell_code_table0);
			encodeSplit(enc, pulses0[10], p1[5], kSilk_shell_code_table0);
			encodeSplit(enc, p1[6], p2[3], kSilk_shell_code_table1);
			encodeSplit(enc, pulses0[12], p1[6], kSilk_shell_code_table0);
			encodeSplit(enc, pulses0[14], p1[7], kSilk_shell_code_table0);
		}
		static void encodeSigns(NkOpusRangeEncoder &enc, const int8 *pulses, int32 length, int32 signalType,
								int32 quantOffsetType, const int32 *sum_pulses) {
			uint8 icdf[2];
			icdf[1] = 0;
			const int8 *q_ptr = pulses;
			const int32 base = M::SMULBB(7, quantOffsetType + M::LSHIFT(signalType, 1));
			const uint8 *icdf_ptr = &kSilk_sign_iCDF[base];
			length = M::RSHIFT(length + SHELL / 2, LOG2_SHELL);
			for (int32 i = 0; i < length; i++) {
				const int32 p = sum_pulses[i];
				if (p > 0) {
					icdf[0] = icdf_ptr[M::minInt(p & 0x1F, 6)];
					for (int32 j = 0; j < SHELL; j++)
						if (q_ptr[j] != 0)
							enc.EncodeIcdf(encMap(q_ptr[j]), icdf, 8);
				}
				q_ptr += SHELL;
			}
		}

		bool NkSilkExcitation::SelfTest() {
			// Aller-retour complet : construit une trame d'excitation signée (sommes
			// <= 16 par bloc, pas d'extension LSB), l'encode exactement comme
			// decode_pulses l'attend (rate level → counts → shell → signes), décode,
			// et vérifie l'identité. Couvre 2 classes de signal × 2 offsets de quant.
			const int32 signalTypes[2] = {0, 2};
			const int32 nb_blocks = 2;
			const int32 frame_length = nb_blocks * SHELL;
			const int32 R = 3; // niveau de débit arbitraire (symétrique enc/dec)

			for (int32 sti = 0; sti < 2; ++sti) {
				const int32 signalType = signalTypes[sti];
				for (int32 qot = 0; qot < 2; ++qot) {
					int8 orig[kMaxShellBlocks * SHELL] = {0};
					int32 sum_pulses[kMaxShellBlocks] = {0};

					// Motif déterministe (petites amplitudes signées) par bloc.
					for (int32 b = 0; b < nb_blocks; ++b) {
						const int32 off = b * SHELL;
						orig[off + 0] = (int8)(2 + b);
						orig[off + 5] = (int8)(-(1 + qot));
						orig[off + 10] = (int8)(1 + sti);
						int32 s = 0;
						for (int32 k = 0; k < SHELL; ++k)
							s += orig[off + k] > 0 ? orig[off + k] : -orig[off + k];
						sum_pulses[b] = s;
					}

					// Encode (ordre EXACT de decode_pulses).
					uint8 buf[256];
					NkOpusRangeEncoder enc;
					enc.Init(buf, sizeof(buf));
					enc.EncodeIcdf(R, kSilk_rate_levels_iCDF[signalType >> 1], 8);
					for (int32 b = 0; b < nb_blocks; ++b)
						enc.EncodeIcdf(sum_pulses[b], kSilk_pulses_per_block_iCDF[R], 8);
					for (int32 b = 0; b < nb_blocks; ++b) {
						if (sum_pulses[b] > 0) {
							int32 absP[SHELL];
							for (int32 k = 0; k < SHELL; ++k)
								absP[k] = orig[b * SHELL + k] > 0 ? orig[b * SHELL + k] : -orig[b * SHELL + k];
							shellEncoder(enc, absP);
						}
					}
					encodeSigns(enc, orig, frame_length, signalType, qot, sum_pulses);
					enc.Done();

					// Décode et compare.
					NkOpusRangeDecoder dec;
					dec.Init(buf, enc.RangeBytes());
					int16 got[kMaxShellBlocks * SHELL] = {0};
					DecodePulses(dec, got, signalType, qot, frame_length);
					for (int32 k = 0; k < frame_length; ++k)
						if (got[k] != (int16)orig[k])
							return false;
				}
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
