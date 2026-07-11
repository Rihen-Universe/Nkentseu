// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkTop.cpp
// Port fidèle de libopus (dec_API.c silk_Decode — chemin mono, sans FEC).
// =============================================================================
#include "NkSilkTop.h"
#include "NkSilkMath.h"
#include "NkSilkExcitation.h" // tables pulses pour le self-test

namespace nkentseu {
	namespace media {

		using M = NkSilkMath;

		void NkSilkTop::Init(int32 fs_kHz, int32 nb_subfr) {
			dec.Init(fs_kHz, nb_subfr);
		}

		int32 NkSilkTop::DecodePacket(NkOpusRangeDecoder &rd, int32 nFramesPerPacket, int16 *out) {
			// En-tête couche LP : flags VAD (1 bit/trame) + flag LBRR (1 bit).
			int32 VAD_flags[3] = {0, 0, 0};
			for (int32 i = 0; i < nFramesPerPacket; i++)
				VAD_flags[i] = rd.DecodeBitLogp(1);
			const int32 LBRR_flag = rd.DecodeBitLogp(1);
			(void)LBRR_flag; // flux sans FEC : pas de trames LBRR à sauter

			int32 nOut = 0;
			for (int32 i = 0; i < nFramesPerPacket; i++) {
				const int32 condCoding =
					(i == 0) ? NkSilkIndices::kCodeIndependently : NkSilkIndices::kCodeConditionally;
				dec.DecodeFrame(rd, out + nOut, VAD_flags[i], condCoding);
				nOut += dec.core.frame_length;
			}
			return nOut;
		}

		// ── Self-test : en-tête (VAD/LBRR) + trame(s) encodées → décode paquet ───
		namespace {
			// Encode une trame NON-VOISÉE (index + pulses nuls) dans l'ordre du flux.
			void encodeUnvoicedFrame(NkOpusRangeEncoder &enc, const NkSilkFrameConfig &cfg, int32 st, int32 qot,
									 int32 vad, int32 condIndep) {
				static const uint8 kVAD[4] = {232, 158, 10, 0};
				static const uint8 kNoVAD[2] = {230, 0};
				static const uint8 kGainICDF[3][8] = {{224, 112, 44, 15, 3, 2, 1, 0},
													  {254, 237, 192, 132, 70, 23, 4, 0},
													  {255, 252, 226, 155, 61, 11, 2, 0}};
				static const uint8 kU8[8] = {224, 192, 160, 128, 96, 64, 32, 0};
				static const uint8 kDelta[41] = {250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27,
												 26,  25,  24,  23,  22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
												 12,  11,  10,  9,   8,  7,  6,  5,  4,  3,  2,  1,  0};
				static const uint8 kInterp[5] = {243, 221, 192, 181, 0};
				static const uint8 kU4[4] = {192, 128, 64, 0};

				const int32 Ix = (st << 1) | qot;
				enc.EncodeIcdf(vad ? (Ix - 2) : Ix, vad ? kVAD : kNoVAD, 8);
				// gains
				enc.EncodeIcdf(3, kGainICDF[st], 8);
				enc.EncodeIcdf(4, kU8, 8);
				for (int32 k = 1; k < cfg.nb_subfr; k++)
					enc.EncodeIcdf((k * 7) % 41, kDelta, 8);
				// NLSF (CB1=0, résidus 0)
				enc.EncodeIcdf(0, &cfg.nlsfCB->CB1_iCDF[(st >> 1) * cfg.nlsfCB->nVectors], 8);
				for (int32 i = 0; i < cfg.nlsfCB->order; i += 2) {
					const uint8 e = cfg.nlsfCB->ec_sel[i / 2];
					enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(e, 1) & 7, 9)], 8);
					enc.EncodeIcdf(4, &cfg.nlsfCB->ec_iCDF[M::SMULBB(M::RSHIFT(e, 5) & 7, 9)], 8);
				}
				// interp (nb==4) + seed
				if (cfg.nb_subfr == 4)
					enc.EncodeIcdf(2, kInterp, 8);
				enc.EncodeIcdf(3, kU4, 8);
				(void)condIndep;
				// pulses : rate level + counts nuls
				const int32 R = 3, frame_length = cfg.Fs_kHz * 5 * cfg.nb_subfr, iter = frame_length / 16;
				enc.EncodeIcdf(R, kSilk_rate_levels_iCDF[st >> 1], 8);
				for (int32 b = 0; b < iter; b++)
					enc.EncodeIcdf(0, kSilk_pulses_per_block_iCDF[R], 8);
			}
		} // namespace

		bool NkSilkTop::SelfTest() {
			const int32 fs = 16, nb = 4, nFrames = 1;
			NkSilkFrameConfig cfg;
			cfg.Set(fs, nb);
			const int32 frame_length = fs * 5 * nb;

			uint8 buf[512];
			NkOpusRangeEncoder enc;
			enc.Init(buf, sizeof(buf));
			// En-tête : VAD (1 trame) + LBRR=0.
			enc.EncodeBitLogp(1, 1); // VAD flag trame 0
			enc.EncodeBitLogp(0, 1); // LBRR flag
			encodeUnvoicedFrame(enc, cfg, /*st*/ 1, /*qot*/ 0, /*vad*/ 1, /*indep*/ 1);
			enc.Done();

			int16 outA[320], outB[320];
			NkSilkTop ta, tb;
			ta.Init(fs, nb);
			tb.Init(fs, nb);
			NkOpusRangeDecoder rdA, rdB;
			rdA.Init(buf, enc.RangeBytes());
			rdB.Init(buf, enc.RangeBytes());
			const int32 nA = ta.DecodePacket(rdA, nFrames, outA);
			const int32 nB = tb.DecodePacket(rdB, nFrames, outB);

			if (nA != frame_length || nB != frame_length)
				return false;
			if (ta.dec.ix.signalType != 1) // VAD → non-voisé recouvré
				return false;
			for (int32 i = 0; i < frame_length; i++)
				if (outA[i] != outB[i])
					return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
