// =============================================================================
// NKMedia/Codecs/Aac/NkAacDecoder.cpp — assemblage raw_data_block → PCM (ISO 14496-3).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacDecoder.h"
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"
#include "NKMedia/Codecs/Aac/NkAacIcs.h"
#include "NKMedia/Codecs/Aac/NkAacDequant.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Identifiants d'éléments syntaxiques (id_syn_ele, Table 4.4.3).
			constexpr int32 ID_SCE = 0;
			constexpr int32 ID_CPE = 1;
			constexpr int32 ID_CCE = 2;
			constexpr int32 ID_LFE = 3;
			constexpr int32 ID_DSE = 4;
			constexpr int32 ID_PCE = 5;
			constexpr int32 ID_FIL = 6;
			constexpr int32 ID_END = 7;

			inline int16 FloatToI16(float32 v) {
				float32 s = v; // deja en domaine int16 (dequant + IMDCT 2/N)
				if (s > 32767.0f)
					s = 32767.0f;
				else if (s < -32768.0f)
					s = -32768.0f;
				return (int16)(s >= 0 ? s + 0.5f : s - 0.5f);
			}

			// Saute un fill_element (extension_payload).
			void SkipFil(NkAacBitReader &br) {
				int32 cnt = (int32)br.ReadBits(4);
				if (cnt == 15)
					cnt += (int32)br.ReadBits(8) - 1;
				for (int32 i = 0; i < cnt; ++i)
					br.ReadBits(8);
			}

			// Saute un data_stream_element.
			void SkipDse(NkAacBitReader &br) {
				(void)br.ReadBits(4); // element_instance_tag
				const int32 align = (int32)br.ReadBit();
				int32 cnt = (int32)br.ReadBits(8);
				if (cnt == 255)
					cnt += (int32)br.ReadBits(8);
				if (align)
					br.ByteAlign();
				for (int32 i = 0; i < cnt; ++i)
					br.ReadBits(8);
			}
		} // namespace

		bool NkAacDecoder::Init(int32 sampleRate, int32 channels) {
			mSfIndex = NkAacTables::SampleRateIndex(sampleRate);
			if (mSfIndex < 0)
				return false;
			mChannels = (channels == 2) ? 2 : 1;
			mPrevWindowShape[0] = mPrevWindowShape[1] = 0;
			mFb[0].Reset();
			mFb[1].Reset();
			return true;
		}

		int32 NkAacDecoder::DecodeFrame(const uint8 *data, int32 len, int16 *pcmOut) {
			NkAacBitReader br(data, (usize)len);
			float32 chTime[2][1024];
			bool chDecoded[2] = {false, false};
			int32 nch = 0;

			for (int32 guard = 0; guard < 64; ++guard) {
				const int32 id = (int32)br.ReadBits(3);
				if (id == ID_END)
					break;
				if (id == ID_SCE || id == ID_LFE) {
					(void)br.ReadBits(4); // element_instance_tag
					NkAacIcs ics;
					if (!ics.ParseChannel(br, mSfIndex))
						return 0;
					float32 spec[1024];
					NkAacDequant::Apply(ics, spec);
					// (TNS s'appliquerait ici — brique à venir.)
					const int32 c = (nch < 2) ? nch : 0;
					mFb[c].Process(spec, ics.windowSequence, ics.windowShape, mPrevWindowShape[c], chTime[c]);
					mPrevWindowShape[c] = ics.windowShape;
					chDecoded[c] = true;
					++nch;
				} else if (id == ID_FIL) {
					SkipFil(br);
				} else if (id == ID_DSE) {
					SkipDse(br);
				} else if (id == ID_CPE || id == ID_CCE || id == ID_PCE) {
					// Stéréo/couplage/config non gérés dans ce chemin mono → arrêt propre.
					return 0;
				}
				if (br.Overrun())
					return 0;
			}

			// Entrelacement PCM (canaux décodés ; complète en silence si besoin).
			const int32 outCh = mChannels;
			for (int32 i = 0; i < 1024; ++i)
				for (int32 c = 0; c < outCh; ++c) {
					const float32 v = chDecoded[c] ? chTime[c][i] : 0.0f;
					pcmOut[i * outCh + c] = FloatToI16(v);
				}
			return 1024;
		}

		// ---------------------------------------------------------------------------
		bool NkAacDecoder::SelfTest() {
			// Sanité : un raw_data_block réduit à ID_END (3 bits = 7) décode 1024 échantillons
			// de silence sans erreur.
			uint8 data[4] = {0xE0, 0, 0, 0}; // 111 (ID_END) en tête
			NkAacDecoder d;
			if (!d.Init(44100, 1))
				return false;
			int16 pcm[1024];
			const int32 n = d.DecodeFrame(data, 4, pcm);
			if (n != 1024)
				return false;
			for (int32 i = 0; i < 1024; ++i)
				if (pcm[i] != 0)
					return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
