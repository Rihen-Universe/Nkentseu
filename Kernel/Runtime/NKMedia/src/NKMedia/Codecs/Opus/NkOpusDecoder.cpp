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
			mSilkInternalCh = 0;
			mSMid[0] = mSMid[1] = 0;
			mPrevMode = -1;
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

				// Reset de l'état CELT au changement de mode (cf. opus_decoder.c : OPUS_RESET_STATE
				// avant le décodage CELT si mode != prev_mode). Évite que l'overlap/énergie d'un mode
				// précédent pollue les trames CELT/hybride suivantes sur un flux mixte.
				const int32 curMode = (int32)op.mode;
				if (op.mode != NkOpusMode::NK_SILK_ONLY && mPrevMode >= 0 && curMode != mPrevMode)
					mCelt.Init(mChannels);
				mPrevMode = curMode;

				// GARDE : un paquet TOC MONO à composante CELT (CELT-only ou hybride) dans un
				// flux stéréo est sauté proprement — notre NkCeltDecoder a un nombre de canaux
				// figé (cas rarissime : libopus garde le TOC stéréo sur un flux stéréo).
				// Le SILK-only mono-dans-stéréo est géré (canaux internes par paquet + duplication).
				if (mChannels == 2 && !op.stereo && op.mode != NkOpusMode::NK_SILK_ONLY)
					continue;

				if (op.mode == NkOpusMode::NK_SILK_ONLY) {
					const int32 fs = (op.bandwidth == NkOpusBandwidth::NK_NB) ? 8
									 : (op.bandwidth == NkOpusBandwidth::NK_MB) ? 12
																			   : 16;
					const int32 nb = (op.frameSizeMs <= 10.5f) ? 2 : 4;
					const int32 nFramesPerPkt = (op.frameSizeMs <= 20.5f) ? 1 : (int32)(op.frameSizeMs / 20.0f + 0.5f);
					// Le nombre de canaux INTERNE est celui du TOC de CE paquet (un flux
					// stéréo peut porter des paquets SILK mono) ; la sortie reste mChannels.
					const int32 internalCh = (mChannels == 2 && op.stereo) ? 2 : 1;
					if (mSilkFsKHz != fs || mSilkNbSubfr != nb || mSilkInternalCh != internalCh) {
						mSilk.Init(fs, nb, internalCh);
						mResampler.Init(fs * 1000, 48000);
						if (internalCh == 2)
							mResamplerSide.Init(fs * 1000, 48000);
						mSilkFsKHz = fs;
						mSilkNbSubfr = nb;
						mSilkInternalCh = internalCh;
						mSMid[0] = mSMid[1] = 0;
					}
					NkOpusRangeDecoder rd;
					rd.Init(fd, (uint32)fl);
					// Stéréo : 3 trames max × 2 canaux × (320+2) échantillons.
					int16 internal[2 * 3 * (320 + 2)];
					const int32 n = mSilk.DecodePacket(rd, nFramesPerPkt, internal);
					const int32 flen = fs * 5 * nb;
					if (internalCh == 2) {
						// NkSilkTop a déjà appliqué MS→LR et les tampons inter-trames : chaque
						// trame arrive en 2 blocs (flen+2) [L puis R], le resampler lit &x[1]
						// (retard d'1 échantillon, convention silk_Decode).
						const int32 blk = flen + 2;
						for (int32 c = 0; c + 2 * blk <= n; c += 2 * blk) {
							int16 *xL = internal + c;
							int16 *xR = internal + c + blk;
							int16 o48L[960 + 16], o48R[960 + 16];
							mResampler.Process(o48L, &xL[1], flen);
							mResamplerSide.Process(o48R, &xR[1], flen);
							const int32 no = flen * 48 / fs;
							for (int32 i = 0; i < no; ++i) {
								out48[nOut++] = o48L[i];
								out48[nOut++] = o48R[i];
							}
						}
					} else {
						// Rééchantillonne chaque trame SILK (frame_length) séparément (comme
						// silk_Decode) avec le tampon inter-trames de 2 échantillons. Si la
						// sortie est stéréo (paquet SILK mono dans un flux stéréo), duplique.
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
							for (int32 i = 0; i < no; ++i) {
								out48[nOut++] = o48[i];
								if (mChannels == 2)
									out48[nOut++] = o48[i];
							}
						}
					}
				} else if (op.mode == NkOpusMode::NK_CELT_ONLY) {
					// LM depuis la durée de trame : 2.5→0, 5→1, 10→2, 20→3 ms.
					const int32 LM = (op.frameSizeMs <= 3.0f)	? 0
									 : (op.frameSizeMs <= 6.0f)	? 1
									 : (op.frameSizeMs <= 11.0f) ? 2
																 : 3;
					// Dernière bande codée selon la bande passante du TOC (opus_decoder.c) :
					// NB→13, WB→17, SWB→19, FB→21. (MB n'existe pas en CELT-only.)
					const int32 celtEnd = (op.bandwidth == NkOpusBandwidth::NK_NB)	 ? 13
										  : (op.bandwidth == NkOpusBandwidth::NK_MB ||
											 op.bandwidth == NkOpusBandwidth::NK_WB) ? 17
										  : (op.bandwidth == NkOpusBandwidth::NK_SWB) ? 19
																					  : 21;
					float32 pcmF[960 * 2];
					NkCeltDecoder::NkFrameFlags flags;
					if (mCelt.DecodeFrame(fd, fl, LM, pcmF, &flags, celtEnd)) {
						const int32 N = (1 << LM) * 120;
						// Sortie entrelacée (mono : N valeurs ; stéréo : 2N valeurs L R L R…).
						for (int32 i = 0; i < N * mChannels; ++i)
							out48[nOut++] = floatToI16(pcmF[i]);
					}
				} else {
					// HYBRIDE (config 12-15) : SILK bande basse (WB 16 kHz) + CELT bande
					// haute (bandes 17→endband), MÊME range decoder, sortie CELT ajoutée.
					// Stéréo : SILK stéréo (MS→LR) + CELT stéréo, tampon entrelacé.
					const int32 LM = (op.frameSizeMs <= 11.0f) ? 2 : 3;
					const int32 nb = (op.frameSizeMs <= 11.0f) ? 2 : 4;
					const int32 endband = (op.bandwidth == NkOpusBandwidth::NK_SWB) ? 19 : 21;
					const int32 N48 = 120 << LM; // échantillons 48 kHz de la trame
					const int32 internalCh = (mChannels == 2 && op.stereo) ? 2 : 1;
					if (mSilkFsKHz != 16 || mSilkNbSubfr != nb || mSilkInternalCh != internalCh) {
						mSilk.Init(16, nb, internalCh);
						mResampler.Init(16000, 48000);
						if (internalCh == 2)
							mResamplerSide.Init(16000, 48000);
						mSilkFsKHz = 16;
						mSilkNbSubfr = nb;
						mSilkInternalCh = internalCh;
						mSMid[0] = mSMid[1] = 0;
					}
					NkOpusRangeDecoder rd;
					rd.Init(fd, (uint32)fl);

					// 1) SILK (bande basse) → 16 kHz → rééchantillonne → tampon float [-1,1]
					// (entrelacé L R si stéréo).
					int16 internal[2 * (320 + 2)];
					const int32 nInt = mSilk.DecodePacket(rd, 1, internal);
					float32 buf48[(960 + 16) * 2];
					for (int32 i = 0; i < N48 * mChannels; ++i)
						buf48[i] = 0.0f;
					const int32 flen = 16 * 5 * nb; // longueur trame SILK
					if (internalCh == 2) {
						// NkSilkTop a déjà produit [L(flen+2)][R(flen+2)] avec MS→LR appliqué.
						const int32 blk = flen + 2;
						if (nInt >= 2 * blk) {
							int16 o48L[960 + 16], o48R[960 + 16];
							mResampler.Process(o48L, internal + 1, flen);
							mResamplerSide.Process(o48R, internal + blk + 1, flen);
							const int32 no = flen * 3;
							for (int32 i = 0; i < no && i < N48; ++i) {
								buf48[2 * i] = (float32)o48L[i] * (1.0f / 32768.0f);
								buf48[2 * i + 1] = (float32)o48R[i] * (1.0f / 32768.0f);
							}
						}
					} else {
						int32 w = 0;
						for (int32 cc = 0; cc + flen <= nInt; cc += flen) {
							int16 tmp[2 + 960];
							tmp[0] = mSMid[0];
							tmp[1] = mSMid[1];
							for (int32 i = 0; i < flen; ++i)
								tmp[2 + i] = internal[cc + i];
							mSMid[0] = internal[cc + flen - 2];
							mSMid[1] = internal[cc + flen - 1];
							int16 o48[960 + 16];
							mResampler.Process(o48, &tmp[1], flen);
							const int32 no = flen * 48 / 16;
							if (mChannels == 2) {
								// Paquet hybride TOC mono dans un flux stéréo : duplique L=R.
								for (int32 i = 0; i < no && w < N48; ++i) {
									const float32 v = (float32)o48[i] * (1.0f / 32768.0f);
									buf48[2 * w] = v;
									buf48[2 * w + 1] = v;
									++w;
								}
							} else {
								for (int32 i = 0; i < no && w < N48; ++i)
									buf48[w++] = (float32)o48[i] * (1.0f / 32768.0f);
							}
						}
					}

					// 2) Flag redundancy (lu pour rester synchro ; flux propre → 0).
					int32 redundancyBytes = 0;
					if (rd.Tell() + 37 <= fl * 8) {
						if (rd.DecodeBitLogp(12)) {
							(void)rd.DecodeBitLogp(1);						 // celt_to_silk
							redundancyBytes = (int32)rd.DecodeUint(256) + 2; // trame redondante (ignorée)
							rd.storage -= (uint32)redundancyBytes;
						}
					}

					// 3) CELT bande haute (bandes 17→endband), ajouté par-dessus SILK.
					// SILK bande basse BIT-EXACT vs libopus ; bande haute CELT validée (corr 0.992
					// vs libopus/ffmpeg sur flux mixte). La clé était special_hybrid_folding dans
					// NkCeltQuantBands (repli de la 2e bande hybride) — sans lui : NaN.
					NkCeltDecoder::NkFrameFlags flags;
					mCelt.DecodeShared(rd, fl - redundancyBytes, LM, 17, endband, buf48, true, &flags);

					// 4) → int16 (entrelacé si stéréo).
					for (int32 i = 0; i < N48 * mChannels; ++i)
						out48[nOut++] = floatToI16(buf48[i]);
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
