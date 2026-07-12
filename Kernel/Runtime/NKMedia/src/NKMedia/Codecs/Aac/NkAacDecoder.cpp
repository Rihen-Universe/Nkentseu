// =============================================================================
// NKMedia/Codecs/Aac/NkAacDecoder.cpp — assemblage raw_data_block → PCM (ISO 14496-3).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacDecoder.h"
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"
#include "NKMedia/Codecs/Aac/NkAacIcs.h"
#include "NKMedia/Codecs/Aac/NkAacDequant.h"
#include "NKMedia/Codecs/Aac/NkAacTns.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"

#include <cmath> // powf (intensity stereo)

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

			// Décodage M/S (mid/side) : pour les bandes marquées, L=M+S et R=M−S, sur le
			// spectre déquantifié désentrelacé. Ignore les bandes intensity (cb 14/15 côté
			// droit) et bruit/PNS (cb 13). `msMask` : 1 = par bande (msUsed), 2 = toutes.
			void MsDecode(const NkAacIcs &ics, const NkAacIcs &icsr, int32 msMask, const uint8 (*msUsed)[51],
						  float32 *l, float32 *r) {
				const int32 nshort = NkAacIcs::kFrameLen / 8;
				const int32 swbMax = (int32)ics.swbOffset[ics.numSwb];
				int32 group = 0;
				for (int32 g = 0; g < ics.numWindowGroups; ++g) {
					for (int32 b = 0; b < ics.windowGroupLength[g]; ++b) {
						for (int32 sfb = 0; sfb < ics.maxSfb; ++sfb) {
							const bool use = (msMask == 2) || (msUsed[g][sfb] != 0);
							const int32 cbR = (int32)icsr.sfbCb[g][sfb];
							const int32 cbL = (int32)ics.sfbCb[g][sfb];
							const bool intensity = (cbR == 14 || cbR == 15);
							const bool noise = (cbL == 13);
							if (use && !intensity && !noise) {
								int32 hi = (int32)ics.swbOffset[sfb + 1];
								if (hi > swbMax)
									hi = swbMax;
								for (int32 i = (int32)ics.swbOffset[sfb]; i < hi; ++i) {
									const int32 k = group * nshort + i;
									const float32 tmp = l[k] - r[k];
									l[k] = l[k] + r[k];
									r[k] = tmp;
								}
							}
						}
						++group;
					}
				}
			}

			// Décodage intensity stereo : pour les bandes marquées intensity (cb 14/15 côté
			// droit), reconstruit le canal droit à partir du gauche : R = L·2^(−0.25·is_pos),
			// avec un signe combiné à l'info M/S (cf. is_decode/invert_intensity). Après M/S.
			void IsDecode(const NkAacIcs &ics, const NkAacIcs &icsr, int32 msMask, const uint8 (*msUsed)[51],
						  float32 *l, float32 *r) {
				const int32 nshort = NkAacIcs::kFrameLen / 8;
				const int32 swbMax = (int32)ics.swbOffset[ics.numSwb];
				int32 group = 0;
				for (int32 g = 0; g < icsr.numWindowGroups; ++g) {
					for (int32 b = 0; b < icsr.windowGroupLength[g]; ++b) {
						for (int32 sfb = 0; sfb < icsr.maxSfb; ++sfb) {
							const int32 cb = (int32)icsr.sfbCb[g][sfb];
							const int32 isInt = (cb == 15) ? 1 : (cb == 14) ? -1 : 0;
							if (isInt != 0) {
								const int32 isPos = (int32)icsr.scaleFactors[g][sfb];
								float32 scale = ::powf(0.5f, 0.25f * (float32)isPos);
								const int32 invert = (msMask == 1) ? (1 - 2 * (int32)msUsed[g][sfb]) : 1;
								if (isInt != invert)
									scale = -scale;
								int32 hi = (int32)icsr.swbOffset[sfb + 1];
								if (hi > swbMax)
									hi = swbMax;
								for (int32 i = (int32)icsr.swbOffset[sfb]; i < hi; ++i) {
									const int32 k = group * nshort + i;
									r[k] = l[k] * scale;
								}
							}
						}
						++group;
					}
				}
			}

			// Générateur pseudo-aléatoire de faad (parité calculée par popcount).
			inline uint32 NeRng(uint32 *r1, uint32 *r2) {
				const uint32 t3 = *r1;
				const uint32 t4 = *r2;
				uint32 t1 = *r1 & 0xF5u;
				uint32 t2 = *r2 >> 25;
				uint32 p1 = (uint32)(__builtin_popcount(t1) & 1);
				t2 &= 0x63u;
				p1 <<= 31;
				const uint32 p2 = (uint32)(__builtin_popcount(t2) & 1);
				*r1 = (t3 >> 1) | p1;
				*r2 = (t4 + t4) | p2;
				return *r1 ^ *r2;
			}

			// Remplit une bande de bruit : `size` valeurs aléatoires normalisées à énergie
			// unité × 2^(0.25·(noise_energy−100)). ⚠️ RNG ≠ celui de ffmpeg → énergie correcte
			// mais pas les échantillons exacts (impossible pour du bruit).
			void GenNoise(float32 *spec, int32 size, int32 noiseEnergy, uint32 *r1, uint32 *r2) {
				if (noiseEnergy < -120)
					noiseEnergy = -120;
				else if (noiseEnergy > 120)
					noiseEnergy = 120;
				double energy = 0.0;
				for (int32 i = 0; i < size; ++i) {
					const float32 t = (float32)(int32)NeRng(r1, r2);
					spec[i] = t;
					energy += (double)t * (double)t;
				}
				if (energy > 0.0) {
					const float32 scale =
						(float32)(1.0 / ::sqrt(energy)) * ::powf(2.0f, 0.25f * (float32)(noiseEnergy - 100));
					for (int32 i = 0; i < size; ++i)
						spec[i] *= scale;
				}
			}

			// PNS (perceptual noise substitution) : remplit les bandes de bruit (cb 13) d'un
			// canal, sur le spectre désentrelacé. RNG persistant entre trames.
			void PnsDecode(const NkAacIcs &ics, float32 *spec, uint32 *r1, uint32 *r2) {
				const int32 nshort = NkAacIcs::kFrameLen / 8;
				const int32 swbMax = (int32)ics.swbOffset[ics.numSwb];
				int32 group = 0;
				for (int32 g = 0; g < ics.numWindowGroups; ++g) {
					for (int32 b = 0; b < ics.windowGroupLength[g]; ++b) {
						const int32 base = group * nshort;
						for (int32 sfb = 0; sfb < ics.maxSfb; ++sfb) {
							if ((int32)ics.sfbCb[g][sfb] == 13) {
								int32 begin = (int32)ics.swbOffset[sfb];
								int32 end = (int32)ics.swbOffset[sfb + 1];
								if (begin > swbMax)
									begin = swbMax;
								if (end > swbMax)
									end = swbMax;
								GenNoise(&spec[base + begin], end - begin, (int32)ics.scaleFactors[g][sfb], r1, r2);
							}
						}
						++group;
					}
				}
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
					PnsDecode(ics, spec, &mR1, &mR2);
					NkAacTns::Apply(ics, mSfIndex, spec);
					const int32 c = (nch < 2) ? nch : 0;
					mFb[c].Process(spec, ics.windowSequence, ics.windowShape, mPrevWindowShape[c], chTime[c]);
					mPrevWindowShape[c] = ics.windowShape;
					chDecoded[c] = true;
					++nch;
				} else if (id == ID_CPE) {
					(void)br.ReadBits(4); // element_instance_tag
					const int32 commonWindow = (int32)br.ReadBit();
					NkAacIcs shared;
					int32 msMask = 0;
					uint8 msUsed[8][51] = {{0}};
					if (commonWindow) {
						if (!shared.ParseIcsInfoPublic(br, mSfIndex))
							return 0;
						msMask = (int32)br.ReadBits(2);
						if (msMask == 3)
							return 0;
						if (msMask == 1)
							for (int32 g = 0; g < shared.numWindowGroups; ++g)
								for (int32 sfb = 0; sfb < shared.maxSfb; ++sfb)
									msUsed[g][sfb] = (uint8)br.ReadBit();
					}
					NkAacIcs ics1, ics2;
					if (!ics1.ParseChannel(br, mSfIndex, commonWindow ? &shared : nullptr))
						return 0;
					if (!ics2.ParseChannel(br, mSfIndex, commonWindow ? &shared : nullptr))
						return 0;
					float32 spec1[1024];
					float32 spec2[1024];
					NkAacDequant::Apply(ics1, spec1);
					NkAacDequant::Apply(ics2, spec2);
					PnsDecode(ics1, spec1, &mR1, &mR2);
					PnsDecode(ics2, spec2, &mR1, &mR2);
					if (msMask >= 1)
						MsDecode(ics1, ics2, msMask, msUsed, spec1, spec2);
					IsDecode(ics1, ics2, msMask, msUsed, spec1, spec2);
					NkAacTns::Apply(ics1, mSfIndex, spec1);
					NkAacTns::Apply(ics2, mSfIndex, spec2);
					mFb[0].Process(spec1, ics1.windowSequence, ics1.windowShape, mPrevWindowShape[0], chTime[0]);
					mFb[1].Process(spec2, ics2.windowSequence, ics2.windowShape, mPrevWindowShape[1], chTime[1]);
					mPrevWindowShape[0] = ics1.windowShape;
					mPrevWindowShape[1] = ics2.windowShape;
					chDecoded[0] = chDecoded[1] = true;
					nch = 2;
				} else if (id == ID_FIL) {
					SkipFil(br);
				} else if (id == ID_DSE) {
					SkipDse(br);
				} else if (id == ID_CCE || id == ID_PCE) {
					// Couplage/config non gérés → arrêt propre.
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
