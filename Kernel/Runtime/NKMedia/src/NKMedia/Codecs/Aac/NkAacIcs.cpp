// =============================================================================
// NKMedia/Codecs/Aac/NkAacIcs.cpp — flux d'un canal AAC-LC (ISO 14496-3 §4.4.2).
// Chemin AAC-LC standard (pas d'ER/RVLC, pas de prédiction, pas de gain control).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacIcs.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"
#include "NKMedia/Codecs/Aac/NkAacHuffman.h"
#include "NKMedia/Video/NkBitWriter.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Séquences de fenêtre.
			constexpr int32 EIGHT_SHORT = 2;
			// Codebooks spéciaux (Table 4.4).
			constexpr int32 ZERO_HCB = 0;
			constexpr int32 FIRST_PAIR_HCB = 5;
			constexpr int32 NOISE_HCB = 13;
			constexpr int32 INTENSITY_HCB2 = 14;
			constexpr int32 INTENSITY_HCB = 15;

			inline int32 BitSet(int32 a, int32 n) {
				return (a >> n) & 1;
			}
		} // namespace

		void NkAacIcs::ComputeGrouping(int32 sfIndex) {
			if (windowSequence != EIGHT_SHORT) {
				numWindows = 1;
				numWindowGroups = 1;
				windowGroupLength[0] = 1;
				numSwb = NkAacTables::NumSwbLong(sfIndex);
				const uint16 *swb = NkAacTables::SwbOffsetLong(sfIndex);
				for (int32 i = 0; i < numSwb; ++i) {
					swbOffset[i] = swb[i];
					sectSfbOffset[0][i] = swb[i];
				}
				swbOffset[numSwb] = (uint16)kFrameLen;
				sectSfbOffset[0][numSwb] = (uint16)kFrameLen;
				return;
			}
			// EIGHT_SHORT : 8 fenêtres, regroupées selon scale_factor_grouping.
			numWindows = 8;
			numSwb = NkAacTables::NumSwbShort(sfIndex);
			const uint16 *swb = NkAacTables::SwbOffsetShort(sfIndex);
			for (int32 i = 0; i < numSwb; ++i)
				swbOffset[i] = swb[i];
			swbOffset[numSwb] = (uint16)(kFrameLen / 8);

			numWindowGroups = 1;
			windowGroupLength[0] = 1;
			for (int32 i = 0; i < numWindows - 1; ++i) {
				if (BitSet(scaleFactorGrouping, 6 - i) == 0) {
					++numWindowGroups;
					windowGroupLength[numWindowGroups - 1] = 1;
				} else {
					windowGroupLength[numWindowGroups - 1] += 1;
				}
			}
			// sect_sfb_offset par groupe (offsets de coefficients cumulés, largeur × longueur de groupe).
			for (int32 g = 0; g < numWindowGroups; ++g) {
				int32 sectSfb = 0;
				int32 offset = 0;
				for (int32 i = 0; i < numSwb; ++i) {
					int32 width = (i + 1 == numSwb) ? ((kFrameLen / 8) - (int32)swb[i]) : ((int32)swb[i + 1] - (int32)swb[i]);
					width *= windowGroupLength[g];
					sectSfbOffset[g][sectSfb++] = (uint16)offset;
					offset += width;
				}
				sectSfbOffset[g][sectSfb] = (uint16)offset;
			}
		}

		bool NkAacIcs::ParseIcsInfo(NkAacBitReader &br, int32 sfIndex) {
			if (br.ReadBit() != 0) // ics_reserved_bit
				return false;
			windowSequence = (int32)br.ReadBits(2);
			windowShape = (int32)br.ReadBit();
			if (windowSequence == EIGHT_SHORT) {
				maxSfb = (int32)br.ReadBits(4);
				scaleFactorGrouping = (int32)br.ReadBits(7);
			} else {
				maxSfb = (int32)br.ReadBits(6);
			}
			ComputeGrouping(sfIndex);
			if (maxSfb > numSwb)
				return false;
			if (windowSequence != EIGHT_SHORT) {
				const int32 predictorDataPresent = (int32)br.ReadBit();
				if (predictorDataPresent) // AAC-LC : pas de prédiction
					return false;
			}
			return true;
		}

		bool NkAacIcs::ParseSectionData(NkAacBitReader &br) {
			const int32 sectBits = (windowSequence == EIGHT_SHORT) ? 3 : 5;
			const int32 sectEscVal = (1 << sectBits) - 1;
			for (int32 g = 0; g < numWindowGroups; ++g) {
				int32 k = 0;
				int32 i = 0;
				while (k < maxSfb) {
					if (i >= kMaxSfb)
						return false;
					const int32 cb = (int32)br.ReadBits(4);
					if (cb == 12) // codebook réservé/interdit
						return false;
					int32 sectLen = 0;
					int32 incr = (int32)br.ReadBits(sectBits);
					while (incr == sectEscVal) {
						sectLen += incr;
						incr = (int32)br.ReadBits(sectBits);
					}
					sectLen += incr;
					if (k + sectLen > maxSfb)
						return false;
					sectCb[g][i] = (uint8)cb;
					sectStart[g][i] = (uint8)k;
					sectEnd[g][i] = (uint8)(k + sectLen);
					for (int32 sfb = k; sfb < k + sectLen && sfb < kMaxSfb; ++sfb)
						sfbCb[g][sfb] = (uint8)cb;
					k += sectLen;
					++i;
				}
				numSec[g] = i;
			}
			return true;
		}

		bool NkAacIcs::ParseScaleFactorData(NkAacBitReader &br) {
			int32 scaleFactor = globalGain;
			int32 isPosition = 0;
			int32 noiseEnergy = globalGain - 90;
			bool noisePcmFlag = true;
			for (int32 g = 0; g < numWindowGroups; ++g) {
				for (int32 sfb = 0; sfb < maxSfb; ++sfb) {
					const int32 cb = (int32)sfbCb[g][sfb];
					if (cb == ZERO_HCB) {
						scaleFactors[g][sfb] = 0;
					} else if (cb == INTENSITY_HCB || cb == INTENSITY_HCB2) {
						isPosition += NkAacHuffman::DecodeScaleFactor(br) - 60;
						scaleFactors[g][sfb] = (int16)isPosition;
					} else if (cb == NOISE_HCB) {
						int32 t;
						if (noisePcmFlag) {
							noisePcmFlag = false;
							t = (int32)br.ReadBits(9);
						} else {
							t = NkAacHuffman::DecodeScaleFactor(br) - 60;
						}
						noiseEnergy += t;
						scaleFactors[g][sfb] = (int16)noiseEnergy;
					} else {
						scaleFactor += NkAacHuffman::DecodeScaleFactor(br) - 60;
						if (scaleFactor < 0 || scaleFactor > 255)
							return false;
						scaleFactors[g][sfb] = (int16)scaleFactor;
					}
				}
			}
			return true;
		}

		void NkAacIcs::ParsePulseData(NkAacBitReader &br) {
			pulse.numberPulse = (uint8)br.ReadBits(2);
			pulse.pulseStartSfb = (uint8)br.ReadBits(6);
			for (int32 i = 0; i < pulse.numberPulse + 1; ++i) {
				pulse.pulseOffset[i] = (uint8)br.ReadBits(5);
				pulse.pulseAmp[i] = (uint8)br.ReadBits(4);
			}
		}

		void NkAacIcs::ParseTnsData(NkAacBitReader &br) {
			const int32 nFiltBits = (windowSequence == EIGHT_SHORT) ? 1 : 2;
			const int32 lengthBits = (windowSequence == EIGHT_SHORT) ? 4 : 6;
			const int32 orderBits = (windowSequence == EIGHT_SHORT) ? 3 : 5;
			for (int32 w = 0; w < numWindows; ++w) {
				tns.nFilt[w] = (uint8)br.ReadBits(nFiltBits);
				int32 startCoefBits = 3;
				if (tns.nFilt[w]) {
					tns.coefRes[w] = (uint8)br.ReadBit();
					if (tns.coefRes[w])
						startCoefBits = 4;
				}
				for (int32 filt = 0; filt < tns.nFilt[w] && filt < 4; ++filt) {
					tns.length[w][filt] = (uint8)br.ReadBits(lengthBits);
					tns.order[w][filt] = (uint8)br.ReadBits(orderBits);
					if (tns.order[w][filt]) {
						tns.direction[w][filt] = (uint8)br.ReadBit();
						tns.coefCompress[w][filt] = (uint8)br.ReadBit();
						const int32 coefBits = startCoefBits - (int32)tns.coefCompress[w][filt];
						for (int32 i = 0; i < (int32)tns.order[w][filt] && i < 32; ++i)
							tns.coef[w][filt][i] = (uint8)br.ReadBits(coefBits);
					}
				}
			}
		}

		void NkAacIcs::ParseSpectralData(NkAacBitReader &br) {
			for (int32 i = 0; i < kFrameLen; ++i)
				specQuant[i] = 0;
			const int32 nshort = kFrameLen / 8;
			int32 groups = 0;
			for (int32 g = 0; g < numWindowGroups; ++g) {
				int32 p = groups * nshort;
				for (int32 s = 0; s < numSec[g]; ++s) {
					const int32 cb = (int32)sectCb[g][s];
					const int32 inc = (cb >= FIRST_PAIR_HCB) ? 2 : 4;
					const int32 startOff = (int32)sectSfbOffset[g][sectStart[g][s]];
					const int32 endOff = (int32)sectSfbOffset[g][sectEnd[g][s]];
					if (cb == ZERO_HCB || cb == NOISE_HCB || cb == INTENSITY_HCB || cb == INTENSITY_HCB2) {
						p += (endOff - startOff);
						continue;
					}
					for (int32 k = startOff; k < endOff; k += inc) {
						int32 vals[4] = {0, 0, 0, 0};
						NkAacHuffman::DecodeSpectral(br, cb, vals);
						for (int32 j = 0; j < inc && (p + j) < kFrameLen; ++j)
							specQuant[p + j] = vals[j];
						p += inc;
					}
				}
				groups += windowGroupLength[g];
			}
		}

		bool NkAacIcs::ParseChannel(NkAacBitReader &br, int32 sfIndex) {
			globalGain = (int32)br.ReadBits(8);
			if (!ParseIcsInfo(br, sfIndex))
				return false;
			if (!ParseSectionData(br))
				return false;
			if (!ParseScaleFactorData(br))
				return false;
			pulseDataPresent = br.ReadBit() != 0;
			if (pulseDataPresent)
				ParsePulseData(br);
			tnsDataPresent = br.ReadBit() != 0;
			if (tnsDataPresent)
				ParseTnsData(br);
			gainControlPresent = br.ReadBit() != 0;
			if (gainControlPresent) // AAC-LC : gain control interdit
				return false;
			ParseSpectralData(br);
			return !br.Overrun();
		}

		// ---------------------------------------------------------------------------
		bool NkAacIcs::SelfTest() {
			// Construit un individual_channel_stream LONG valide (sf_index=4 = 44100),
			// max_sfb=4 (→ 16 coefficients), une section cb=1 (quad signé), facteurs
			// d'échelle à delta nul, 4 codewords spectraux connus → parse et vérifie.
			const int32 sfIndex = 4;

			// Helper : écrit le codeword du facteur d'échelle de valeur brute `val`.
			auto writeSf = [](NkBitWriter &w, int32 val) -> bool {
				const NkAacHcbBook &bk = kAacHcbBooks[0];
				for (int32 i = 0; i < bk.count; ++i)
					if ((int32)bk.entries[i].v[0] == val) {
						w.PutBits(bk.entries[i].cw, bk.entries[i].len);
						return true;
					}
				return false;
			};

			NkBitWriter w;
			w.PutBits(100, 8);	// global_gain
			// ics_info : reserved=0, window_sequence=0 (ONLY_LONG), shape=0, max_sfb=4.
			w.PutBits(0, 1);
			w.PutBits(0, 2);
			w.PutBits(0, 1);
			w.PutBits(4, 6);
			w.PutBits(0, 1); // predictor_data_present=0
			// section_data (long, sect_bits=5) : cb=1, sect_len=4 (< 31).
			w.PutBits(1, 4);
			w.PutBits(4, 5);
			// scale_factor_data : 4 bandes cb=1 → 4 facteurs, delta 0 → valeur brute 60.
			for (int32 i = 0; i < 4; ++i)
				if (!writeSf(w, 60))
					return false;
			// pulse=0, tns=0, gain_control=0.
			w.PutBits(0, 1);
			w.PutBits(0, 1);
			w.PutBits(0, 1);
			// spectral_data : 4 codewords cb=1 (quad). On prend 4 entrées connues.
			const NkAacHcbBook &cb1 = kAacHcbBooks[1];
			int32 expectSpec[16] = {0};
			for (int32 d = 0; d < 4; ++d) {
				const NkAacHcbEntry &e = cb1.entries[d * 5 + 1]; // entrées variées, non nulles
				w.PutBits(e.cw, e.len);
				for (int32 j = 0; j < 4; ++j)
					expectSpec[d * 4 + j] = (int32)e.v[j]; // cb1 signé → pas de bit de signe
			}
			w.AlignByteZero();

			NkAacIcs ics;
			NkAacBitReader br(w.Data(), w.Size());
			if (!ics.ParseChannel(br, sfIndex))
				return false;
			if (ics.windowSequence != 0 || ics.maxSfb != 4 || ics.numWindowGroups != 1)
				return false;
			if (ics.globalGain != 100)
				return false;
			if (ics.numSec[0] != 1 || ics.sectCb[0][0] != 1 || ics.sectEnd[0][0] != 4)
				return false;
			for (int32 sfb = 0; sfb < 4; ++sfb)
				if (ics.scaleFactors[0][sfb] != 100)
					return false; // global_gain + 0
			for (int32 i = 0; i < 16; ++i)
				if (ics.specQuant[i] != expectSpec[i])
					return false;
			// Coefficients au-delà de la dernière bande codée = 0.
			for (int32 i = 16; i < NkAacIcs::kFrameLen; ++i)
				if (ics.specQuant[i] != 0)
					return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
