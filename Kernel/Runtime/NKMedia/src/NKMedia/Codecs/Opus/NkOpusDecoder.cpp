// =============================================================================
// NKMedia/Codecs/Opus/NkOpusDecoder.cpp
// Dispatcher Opus : route TOC → CELT-only / SILK-only (+resample) / hybride.
// =============================================================================
#include "NkOpusDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		static inline int16 floatToI16(float32 v) {
			float32 s = v * 32768.0f;
			if (s > 32767.0f)
				s = 32767.0f;
			else if (s < -32768.0f)
				s = -32768.0f;
			return (int16)(s >= 0 ? s + 0.5f : s - 0.5f);
		}

		void NkOpusDecoder::Init(int32 channels) {
			mChannels = channels;
			mCelt.Init(channels);
			mSilkFsKHz = 0;
			mSilkNbSubfr = 0;
			mSMid[0] = mSMid[1] = 0;
		}

		int32 NkOpusDecoder::DecodePacket(const uint8 *data, int32 len, int16 *out48) {
			NkOpusPacketInfo op;
			if (!NkOpusPacket::Parse(data, len, op))
				return 0;

			int32 nOut = 0;
			for (int32 fr = 0; fr < op.frameCount; ++fr) {
				const uint8 *fd = data + (int32)op.frames[fr].offset;
				const int32 fl = (int32)op.frames[fr].size;
				if (fl <= 0)
					continue;

				if (op.mode == NkOpusMode::NK_SILK_ONLY) {
					const int32 fs = (op.bandwidth == NkOpusBandwidth::NK_NB) ? 8
									 : (op.bandwidth == NkOpusBandwidth::NK_MB) ? 12
																			   : 16;
					const int32 nb = (op.frameSizeMs <= 10.5f) ? 2 : 4;
					const int32 nFramesPerPkt = (op.frameSizeMs <= 20.5f) ? 1 : (int32)(op.frameSizeMs / 20.0f + 0.5f);
					if (mSilkFsKHz != fs || mSilkNbSubfr != nb) {
						mSilk.Init(fs, nb);
						mResampler.Init(fs * 1000, 48000);
						mSilkFsKHz = fs;
						mSilkNbSubfr = nb;
						mSMid[0] = mSMid[1] = 0;
					}
					NkOpusRangeDecoder rd;
					rd.Init(fd, (uint32)fl);
					int16 internal[960];
					const int32 n = mSilk.DecodePacket(rd, nFramesPerPkt, internal);
					// Rééchantillonne chaque trame SILK (frame_length) séparément (comme
					// silk_Decode) avec le tampon inter-trames de 2 échantillons.
					const int32 flen = fs * 5 * nb;
					for (int32 c = 0; c + flen <= n; c += flen) {
						int16 tmp[2 + 960];
						tmp[0] = mSMid[0];
						tmp[1] = mSMid[1];
						for (int32 i = 0; i < flen; ++i)
							tmp[2 + i] = internal[c + i];
						mSMid[0] = internal[c + flen - 2];
						mSMid[1] = internal[c + flen - 1];
						int16 o48[960 * 3 + 16];
						mResampler.Process(o48, &tmp[1], flen);
						const int32 no = flen * 48 / fs;
						for (int32 i = 0; i < no; ++i)
							out48[nOut++] = o48[i];
					}
				} else if (op.mode == NkOpusMode::NK_CELT_ONLY) {
					// LM depuis la durée de trame : 2.5→0, 5→1, 10→2, 20→3 ms.
					const int32 LM = (op.frameSizeMs <= 3.0f)	? 0
									 : (op.frameSizeMs <= 6.0f)	? 1
									 : (op.frameSizeMs <= 11.0f) ? 2
																 : 3;
					float32 pcmF[960 * 2];
					NkCeltDecoder::NkFrameFlags flags;
					if (mCelt.DecodeFrame(fd, fl, LM, pcmF, &flags)) {
						const int32 N = (1 << LM) * 120;
						for (int32 i = 0; i < N; ++i)
							out48[nOut++] = floatToI16(pcmF[i * mChannels]); // mono : 1er canal
					}
				} else {
					// HYBRIDE : SILK bande basse + CELT bande haute — À VENIR (nécessite
					// une bande de départ dans le décodeur CELT). Silence en attendant.
					const int32 N = (int32)(op.frameSizeMs * 48.0f + 0.5f);
					for (int32 i = 0; i < N; ++i)
						out48[nOut++] = 0;
				}
			}
			return nOut;
		}

		bool NkOpusDecoder::SelfTest() {
			// Sanité : construit un paquet Opus CELT-only silence (TOC config 28 = FB
			// 20ms code0) + charge utile minimale ; décode → 960 échantillons.
			NkOpusDecoder d;
			d.Init(1);
			uint8 pkt[64];
			pkt[0] = (uint8)((31 << 3) | 0); // config 31 (CELT FB 20ms), mono, code 0
			NkOpusRangeEncoder enc;
			enc.Init(pkt + 1, sizeof(pkt) - 1);
			enc.EncodeBitLogp(1, 15); // silence
			enc.Done();
			const int32 fl = (int32)enc.RangeBytes();
			int16 out[960 * 4];
			const int32 n = d.DecodePacket(pkt, 1 + fl, out);
			if (n != 960)
				return false; // 20ms @ 48kHz
			return true;
		}

	} // namespace media
} // namespace nkentseu
