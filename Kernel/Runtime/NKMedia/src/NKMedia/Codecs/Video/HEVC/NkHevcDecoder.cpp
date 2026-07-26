// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcDecoder.cpp — brique 1 (NAL split + VPS/SPS/PPS).
// =============================================================================
#include "NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h"
// Lecteur de bits MSB-first + Exp-Golomb : IDENTIQUE bit pour bit à H.264 (même
// convention u(n)/ue(v)/se(v), même RBSP anti-émulation) — réutilisé tel quel
// plutôt que dupliqué (namespace nkentseu::media, header-only, zero-STL).
#include "NKMedia/Codecs/Video/H264/NkH264BitReader.h"

namespace nkentseu {
	namespace media {

		namespace {

			int32 Clip3i(int32 lo, int32 hi, int32 v) {
				return v < lo ? lo : (v > hi ? hi : v);
			}

			// Retire les octets anti-émulation 00 00 03 -> 00 00 d'un RBSP (§7.3.1.1,
			// identique à H.264 : NAL header exclu, appelé sur les octets APRÈS lui).
			void Deemulate(const uint8 *src, usize n, NkVector<uint8> &out) {
				out.Clear();
				for (usize i = 0; i < n; ++i) {
					if (i + 2 < n && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
						out.PushBack(0);
						out.PushBack(0);
						i += 2; // saute le 03, la boucle avancera au-delà des deux 00
					} else {
						out.PushBack(src[i]);
					}
				}
			}

			// profile_tier_level() (§7.3.3). `profilePresentFlag` vaut toujours vrai pour
			// les appels VPS/SPS de haut niveau (seules les extensions scalables passent
			// faux, hors périmètre). Les 88 bits de constraint/compatibility flags et les
			// infos par sous-couche sont CONSOMMÉS bit-exactement (pour rester synchrone
			// avec la suite du RBSP) mais pas exposés — non nécessaires hors extensions.
			void ReadProfileTierLevel(NkH264BitReader &br, bool profilePresentFlag, int32 maxNumSubLayersMinus1,
									  NkHevcProfileTierLevel &out) {
				if (profilePresentFlag) {
					out.generalProfileSpace = (int32)br.U(2);
					out.generalTierFlag = br.U1() != 0;
					out.generalProfileIdc = (int32)br.U(5);
					br.Skip(32); // general_profile_compatibility_flag[0..31]
					br.Skip(4);	 // progressive/interlaced/non_packed/frame_only_constraint
					br.Skip(43); // constraint flags (profil-dépendants OU reserved_zero_43bits —
								 // même longueur totale dans les deux cas, §7.3.3)
					br.Skip(1);	 // general_inbld_flag OU reserved_zero_bit
				}
				out.generalLevelIdc = (int32)br.U(8);
				bool subProfilePresent[8] = {false, false, false, false, false, false, false, false};
				bool subLevelPresent[8] = {false, false, false, false, false, false, false, false};
				for (int32 i = 0; i < maxNumSubLayersMinus1 && i < 8; ++i) {
					subProfilePresent[i] = br.U1() != 0;
					subLevelPresent[i] = br.U1() != 0;
				}
				if (maxNumSubLayersMinus1 > 0)
					for (int32 i = maxNumSubLayersMinus1; i < 8; ++i)
						br.Skip(2); // reserved_zero_2bits[i]
				for (int32 i = 0; i < maxNumSubLayersMinus1 && i < 8; ++i) {
					if (subProfilePresent[i]) {
						br.Skip(2 + 1 + 5); // sub_layer_profile_space/tier_flag/profile_idc
						br.Skip(32);		 // sub_layer_profile_compatibility_flag[0..31]
						br.Skip(4);			 // 4 flags source/constraint
						br.Skip(43 + 1);	 // constraint flags + inbld/reserved (même schéma que general)
					}
					if (subLevelPresent[i])
						br.Skip(8); // sub_layer_level_idc[i]
				}
			}

			// Ceil(Log2(x)) — utilisé pour la largeur en bits de slice_segment_address
			// (§7.4.7.1 : Ceil(Log2(PicSizeInCtbsY))). x<=1 -> 0 (aucun bit nécessaire).
			int32 CeilLog2(uint32 x) {
				if (x < 2)
					return 0;
				int32 r = 0;
				uint32 v = 1;
				while (v < x) {
					v <<= 1;
					++r;
				}
				return r;
			}

			// st_ref_pic_set(stRpsIdx) (§7.3.7) + dérivation des deltas de POC (§7.4.8).
			// `stRpsIdx` = index du jeu en cours ; `numRps` = num_short_term_ref_pic_sets du
			// SPS (stRpsIdx == numRps <=> RPS signalé INLINE dans un slice header, seul cas où
			// delta_idx_minus1 est présent) ; `sets` = jeux déjà décodés (prédiction inter-RPS).
			bool ReadShortTermRps(NkH264BitReader &br, int32 stRpsIdx, int32 numRps,
								  const NkHevcShortTermRps *sets, NkHevcShortTermRps &out) {
				out = NkHevcShortTermRps{};
				bool interPred = false;
				if (stRpsIdx != 0)
					interPred = br.U1() != 0;
				if (interPred) {
					int32 deltaIdxMinus1 = 0;
					if (stRpsIdx == numRps)
						deltaIdxMinus1 = (int32)br.UE();
					const int32 refIdx = stRpsIdx - (deltaIdxMinus1 + 1);
					if (refIdx < 0 || refIdx >= numRps)
						return false;
					const NkHevcShortTermRps &ref = sets[refIdx];
					const int32 numDeltaPocsRef = ref.numNegativePics + ref.numPositivePics;
					if (numDeltaPocsRef > 32)
						return false;
					const bool sign = br.U1() != 0;
					const int32 absDelta = (int32)br.UE() + 1;
					const int32 deltaRps = sign ? -absDelta : absDelta;
					bool usedByCurr[33];
					bool useDelta[33];
					for (int32 j = 0; j <= numDeltaPocsRef; ++j) {
						usedByCurr[j] = br.U1() != 0;
						useDelta[j] = usedByCurr[j] ? true : (br.U1() != 0);
					}
					// Dérivation §7.4.8 (éq. 7-59 à 7-71) : reconstruit les listes S0/S1 du jeu
					// courant en décalant celles du jeu de référence de deltaRps, en gardant
					// l'ordre trié (S0 décroissant depuis 0, S1 croissant depuis 0).
					int32 i = 0;
					for (int32 j = ref.numPositivePics - 1; j >= 0; --j) {
						const int32 dPoc = ref.deltaPocS1[j] + deltaRps;
						if (dPoc < 0 && useDelta[ref.numNegativePics + j]) {
							out.deltaPocS0[i] = dPoc;
							out.usedS0[i++] = usedByCurr[ref.numNegativePics + j];
						}
					}
					if (deltaRps < 0 && useDelta[numDeltaPocsRef]) {
						out.deltaPocS0[i] = deltaRps;
						out.usedS0[i++] = usedByCurr[numDeltaPocsRef];
					}
					for (int32 j = 0; j < ref.numNegativePics; ++j) {
						const int32 dPoc = ref.deltaPocS0[j] + deltaRps;
						if (dPoc < 0 && useDelta[j]) {
							if (i >= 16)
								return false;
							out.deltaPocS0[i] = dPoc;
							out.usedS0[i++] = usedByCurr[j];
						}
					}
					out.numNegativePics = i;
					i = 0;
					for (int32 j = ref.numNegativePics - 1; j >= 0; --j) {
						const int32 dPoc = ref.deltaPocS0[j] + deltaRps;
						if (dPoc > 0 && useDelta[j]) {
							out.deltaPocS1[i] = dPoc;
							out.usedS1[i++] = usedByCurr[j];
						}
					}
					if (deltaRps > 0 && useDelta[numDeltaPocsRef]) {
						out.deltaPocS1[i] = deltaRps;
						out.usedS1[i++] = usedByCurr[numDeltaPocsRef];
					}
					for (int32 j = 0; j < ref.numPositivePics; ++j) {
						const int32 dPoc = ref.deltaPocS1[j] + deltaRps;
						if (dPoc > 0 && useDelta[ref.numNegativePics + j]) {
							if (i >= 16)
								return false;
							out.deltaPocS1[i] = dPoc;
							out.usedS1[i++] = usedByCurr[ref.numNegativePics + j];
						}
					}
					out.numPositivePics = i;
				} else {
					out.numNegativePics = (int32)br.UE();
					out.numPositivePics = (int32)br.UE();
					if (out.numNegativePics > 16 || out.numPositivePics > 16)
						return false;
					int32 prev = 0;
					for (int32 i = 0; i < out.numNegativePics; ++i) {
						prev -= (int32)br.UE() + 1; // delta_poc_s0_minus1 (deltas CUMULÉS)
						out.deltaPocS0[i] = prev;
						out.usedS0[i] = br.U1() != 0;
					}
					prev = 0;
					for (int32 i = 0; i < out.numPositivePics; ++i) {
						prev += (int32)br.UE() + 1; // delta_poc_s1_minus1
						out.deltaPocS1[i] = prev;
						out.usedS1[i] = br.U1() != 0;
					}
				}
				return true;
			}

		} // namespace

		void NkHevcDecoder::DeemulateRbsp(const uint8 *nal, usize size, NkVector<uint8> &out) {
			out.Clear();
			if (!nal || size < 2)
				return;
			Deemulate(nal + 2, size - 2, out);
		}

		void NkHevcDecoder::SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkHevcNal> &out) {
			out.Clear();
			if (!data || size < 5) // start code (3) + en-tête NAL (2 octets, contre 1 en H.264)
				return;
			usize i = 0;
			auto isStart3 = [&](usize p) {
				return p + 2 < size && data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 1;
			};
			auto emit = [&](usize off, usize end) {
				if (end <= off + 1) // moins de 2 octets = pas d'en-tête NAL complet
					return;
				NkHevcNal nal;
				nal.offset = off;
				nal.size = end - off;
				nal.type = (data[off] >> 1) & 0x3F;
				nal.layerId = (int32)(((data[off] & 1) << 5) | (data[off + 1] >> 3));
				nal.temporalId = (int32)(data[off + 1] & 0x7) - 1;
				out.PushBack(nal);
			};
			usize prevStart = (usize)-1;
			while (i + 2 < size) {
				if (isStart3(i)) {
					const usize nalStart = i + 3;
					if (prevStart != (usize)-1) {
						usize end = i;
						// retire un éventuel 00 précédant le start code (00 00 00 01)
						while (end > prevStart && data[end - 1] == 0)
							--end;
						emit(prevStart, end);
					}
					prevStart = nalStart;
					i = nalStart;
				} else {
					++i;
				}
			}
			if (prevStart != (usize)-1 && prevStart < size)
				emit(prevStart, size);
		}

		bool NkHevcDecoder::ParseVps(const uint8 *nal, usize size, int32 &outVpsId,
									NkHevcProfileTierLevel &outPtl) {
			outVpsId = -1;
			outPtl = NkHevcProfileTierLevel{};
			if (!nal || size < 4)
				return false;
			const int32 nalType = (nal[0] >> 1) & 0x3F;
			if (nalType != kHevcNalVps)
				return false;
			NkVector<uint8> rbsp;
			Deemulate(nal + 2, size - 2, rbsp); // après les 2 octets d'en-tête NAL
			if (rbsp.Size() < 6)
				return false;
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			outVpsId = (int32)br.U(4);
			br.Skip(1 + 1 + 6); // base_layer_internal/available_flag + max_layers_minus1
			const int32 maxSubLayersMinus1 = (int32)br.U(3);
			br.Skip(1 + 16); // temporal_id_nesting_flag + reserved_0xffff_16bits
			ReadProfileTierLevel(br, true, maxSubLayersMinus1, outPtl);
			return true;
		}

		bool NkHevcDecoder::ParseSps(const uint8 *nal, usize size, NkHevcSps &out) {
			out = NkHevcSps{};
			if (!nal || size < 4)
				return false;
			const int32 nalType = (nal[0] >> 1) & 0x3F;
			if (nalType != kHevcNalSps)
				return false;
			NkVector<uint8> rbsp;
			Deemulate(nal + 2, size - 2, rbsp);
			if (rbsp.Size() < 4)
				return false;
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			out.vpsId = (int32)br.U(4);
			out.maxSubLayersMinus1 = (int32)br.U(3);
			br.Skip(1); // sps_temporal_id_nesting_flag
			ReadProfileTierLevel(br, true, out.maxSubLayersMinus1, out.ptl);
			out.spsId = (int32)br.UE();
			out.chromaFormatIdc = (int32)br.UE();
			if (out.chromaFormatIdc == 3)
				out.separateColourPlane = br.U1() != 0;
			out.width = (int32)br.UE();
			out.height = (int32)br.UE();
			out.conformanceWindow = br.U1() != 0;
			if (out.conformanceWindow) {
				out.confWinLeft = (int32)br.UE();
				out.confWinRight = (int32)br.UE();
				out.confWinTop = (int32)br.UE();
				out.confWinBottom = (int32)br.UE();
			}
			out.bitDepthLuma = (int32)br.UE() + 8;
			out.bitDepthChroma = (int32)br.UE() + 8;
			out.log2MaxPocLsb = (int32)br.UE() + 4;
			// sps_max_dec_pic_buffering/num_reorder_pics/latency_increase par sous-couche
			// (§7.3.2.2.1) — consommés (bit-exactement) mais pas exposés, non nécessaires
			// hors gestion DPB (brique décodage image).
			const bool subLayerOrderingInfoPresent = br.U1() != 0;
			const int32 startIdx = subLayerOrderingInfoPresent ? 0 : out.maxSubLayersMinus1;
			for (int32 i = startIdx; i <= out.maxSubLayersMinus1; ++i) {
				br.UE(); // sps_max_dec_pic_buffering_minus1[i]
				br.UE(); // sps_max_num_reorder_pics[i]
				br.UE(); // sps_max_latency_increase_plus1[i]
			}
			// Taille des CTU — nécessaire à PicSizeInCtbsY (largeur en bits de
			// slice_segment_address, cf. NkHevcDecoder::ParseSliceHeader) : voir champs
			// log2MinCbSizeY/log2DiffMaxMinCbSizeY dans NkHevcSps.
			out.log2MinCbSizeY = (int32)br.UE() + 3;
			out.log2DiffMaxMinCbSizeY = (int32)br.UE();
			out.log2MinTbSizeY = (int32)br.UE() + 2;
			out.log2DiffMaxMinTbSizeY = (int32)br.UE();
			out.maxTransformHierarchyDepthInter = (int32)br.UE();
			out.maxTransformHierarchyDepthIntra = (int32)br.UE();
			out.scalingListEnabled = br.U1() != 0;
			if (out.scalingListEnabled) {
				const bool dataPresent = br.U1() != 0;
				if (dataPresent)
					return false; // scaling_list_data() pas encore porté (refus propre)
			}
			out.ampEnabled = br.U1() != 0;
			out.sampleAdaptiveOffsetEnabled = br.U1() != 0;
			out.pcmEnabled = br.U1() != 0;
			if (out.pcmEnabled) {
				out.pcmBitDepthLuma = (int32)br.U(4) + 1;   // pcm_sample_bit_depth_luma_minus1
				out.pcmBitDepthChroma = (int32)br.U(4) + 1; // pcm_sample_bit_depth_chroma_minus1
				out.log2MinPcmCbSize = (int32)br.UE() + 3;  // log2_min_pcm_..._minus3
				out.log2MaxPcmCbSize = out.log2MinPcmCbSize + (int32)br.UE();
				out.pcmLoopFilterDisabled = br.U1() != 0;   // pcm_loop_filter_disabled_flag
			}
			out.numShortTermRefPicSets = (int32)br.UE();
			if (out.numShortTermRefPicSets > 64)
				return false;
			for (int32 i = 0; i < out.numShortTermRefPicSets; ++i)
				if (!ReadShortTermRps(br, i, out.numShortTermRefPicSets, out.shortTermRps,
									  out.shortTermRps[i]))
					return false;
			out.longTermRefPicsPresent = br.U1() != 0;
			if (out.longTermRefPicsPresent) {
				out.numLongTermRefPicsSps = (int32)br.UE();
				if (out.numLongTermRefPicsSps > 32)
					return false;
				for (int32 i = 0; i < out.numLongTermRefPicsSps; ++i) {
					br.Skip(out.log2MaxPocLsb); // lt_ref_pic_poc_lsb_sps
					br.Skip(1);					// used_by_curr_pic_lt_sps_flag
				}
			}
			out.spsTemporalMvpEnabled = br.U1() != 0;
			out.strongIntraSmoothingEnabled = br.U1() != 0;
			// S'ARRÊTE ICI (briques 1-3) : vui_parameters()/sps_extension restent à porter
			// (informatif seulement, pas nécessaire au décodage des slices).
			out.valid = (out.width > 0 && out.height > 0);
			return out.valid;
		}

		bool NkHevcDecoder::ParsePps(const uint8 *nal, usize size, NkHevcPps &out) {
			out = NkHevcPps{};
			if (!nal || size < 4)
				return false;
			const int32 nalType = (nal[0] >> 1) & 0x3F;
			if (nalType != kHevcNalPps)
				return false;
			NkVector<uint8> rbsp;
			Deemulate(nal + 2, size - 2, rbsp);
			if (rbsp.Size() < 2)
				return false;
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			out.ppsId = (int32)br.UE();
			out.spsId = (int32)br.UE();
			out.dependentSliceSegmentsEnabled = br.U1() != 0;
			out.outputFlagPresent = br.U1() != 0;
			out.numExtraSliceHeaderBits = (int32)br.U(3);
			out.signDataHiding = br.U1() != 0;
			out.cabacInitPresent = br.U1() != 0;
			out.numRefIdxL0DefaultActive = (int32)br.UE() + 1;
			out.numRefIdxL1DefaultActive = (int32)br.UE() + 1;
			out.initQp = (int32)br.SE() + 26;
			out.constrainedIntraPred = br.U1() != 0;
			out.transformSkipEnabled = br.U1() != 0;
			out.cuQpDeltaEnabled = br.U1() != 0;
			if (out.cuQpDeltaEnabled)
				out.diffCuQpDeltaDepth = (int32)br.UE();
			out.ppsCbQpOffset = (int32)br.SE();
			out.ppsCrQpOffset = (int32)br.SE();
			out.sliceChromaQpOffsetsPresent = br.U1() != 0;
			out.weightedPred = br.U1() != 0;
			out.weightedBipred = br.U1() != 0;
			out.transquantBypassEnabled = br.U1() != 0;
			out.tilesEnabled = br.U1() != 0;
			out.entropyCodingSyncEnabled = br.U1() != 0;
			if (out.tilesEnabled) {
				out.numTileColumnsMinus1 = (int32)br.UE();
				out.numTileRowsMinus1 = (int32)br.UE();
				const bool uniformSpacing = br.U1() != 0;
				if (!uniformSpacing) {
					for (int32 i = 0; i < out.numTileColumnsMinus1; ++i)
						br.UE(); // column_width_minus1[i] (pas stocké — brique 1)
					for (int32 i = 0; i < out.numTileRowsMinus1; ++i)
						br.UE(); // row_height_minus1[i]
				}
				out.loopFilterAcrossTiles = br.U1() != 0;
			}
			out.loopFilterAcrossSlices = br.U1() != 0;
			out.deblockingFilterControlPresent = br.U1() != 0;
			if (out.deblockingFilterControlPresent) {
				out.deblockingFilterOverrideEnabled = br.U1() != 0;
				out.ppsDeblockingFilterDisabled = br.U1() != 0;
				if (!out.ppsDeblockingFilterDisabled) {
					out.ppsBetaOffsetDiv2 = (int32)br.SE();
					out.ppsTcOffsetDiv2 = (int32)br.SE();
				}
			}
			if (br.U1()) // pps_scaling_list_data_present_flag
				return false; // scaling_list_data() pas encore porté (refus propre)
			out.listsModificationPresent = br.U1() != 0;
			out.log2ParallelMergeLevel = (int32)br.UE() + 2;
			out.sliceSegmentHeaderExtensionPresent = br.U1() != 0;
			// S'ARRÊTE ICI : pps_extension (range/multilayer/3D) non porté — jamais présent
			// dans les flux Main/Main10 classiques.
			out.valid = true;
			return true;
		}

		bool NkHevcDecoder::ParseSliceHeader(const uint8 *nal, usize size, const NkHevcSps &sps, const NkHevcPps &pps,
											 NkHevcSliceHeader &out) {
			out = NkHevcSliceHeader{};
			if (!nal || size < 4 || !sps.valid || !pps.valid)
				return false;
			const int32 nalType = (nal[0] >> 1) & 0x3F;
			if (nalType > 31) // au-delà de 31 = non-VCL (VPS/SPS/PPS/SEI/…), pas une slice
				return false;
			out.nalType = nalType;
			out.isIdr = (nalType == kHevcNalIdrWRadl || nalType == kHevcNalIdrNLp);
			NkVector<uint8> rbsp;
			Deemulate(nal + 2, size - 2, rbsp);
			if (rbsp.Size() < 1)
				return false;
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			out.firstSliceSegmentInPic = br.U1() != 0;
			if (nalType >= kHevcNalBlaWLp && nalType <= 23) // BLA_W_LP(16)..RSV_IRAP_VCL23(23)
				out.noOutputOfPriorPics = br.U1() != 0;
			out.ppsId = (int32)br.UE();
			if (!out.firstSliceSegmentInPic) {
				if (pps.dependentSliceSegmentsEnabled)
					out.dependentSliceSegment = br.U1() != 0;
				// slice_segment_address u(v) : Ceil(Log2(PicSizeInCtbsY)) bits (§7.4.7.1).
				const int32 ctbLog2Size = sps.log2MinCbSizeY + sps.log2DiffMaxMinCbSizeY;
				const int32 ctbSize = 1 << ctbLog2Size;
				const int32 picWidthInCtbs = (sps.width + ctbSize - 1) / ctbSize;
				const int32 picHeightInCtbs = (sps.height + ctbSize - 1) / ctbSize;
				const int32 picSizeInCtbs = picWidthInCtbs * picHeightInCtbs;
				const int32 addrBits = CeilLog2((uint32)picSizeInCtbs);
				out.sliceSegmentAddress = addrBits > 0 ? (int32)br.U(addrBits) : 0;
			}
			if (!out.dependentSliceSegment) {
				for (int32 i = 0; i < pps.numExtraSliceHeaderBits; ++i)
					br.U1(); // slice_reserved_flag[i] (discarde)
				out.sliceType = (int32)br.UE();
				if (pps.outputFlagPresent)
					out.picOutputFlag = br.U1() != 0;
				if (sps.separateColourPlane)
					out.colourPlaneId = (int32)br.U(2);
				int32 ltUsedCount = 0; // refs long terme "used by curr" (pour NumPicTotalCurr)
				if (!out.isIdr) {
					out.picOrderCntLsb = (int32)br.U(sps.log2MaxPocLsb);
					out.shortTermRefPicSetSpsFlag = br.U1() != 0;
					if (!out.shortTermRefPicSetSpsFlag) {
						// RPS signalé INLINE (cas x265 par défaut : num_short_term_ref_pic_sets
						// SPS = 0, chaque slice porte le sien) — stRpsIdx == numRps.
						if (!ReadShortTermRps(br, sps.numShortTermRefPicSets, sps.numShortTermRefPicSets,
											  sps.shortTermRps, out.rps))
							return false;
					} else if (sps.numShortTermRefPicSets > 1) {
						out.shortTermRefPicSetIdx =
							(int32)br.U(CeilLog2((uint32)sps.numShortTermRefPicSets));
						if (out.shortTermRefPicSetIdx >= sps.numShortTermRefPicSets)
							return false;
						out.rps = sps.shortTermRps[out.shortTermRefPicSetIdx];
					} else if (sps.numShortTermRefPicSets == 1) {
						out.rps = sps.shortTermRps[0];
					}
					if (sps.longTermRefPicsPresent) {
						int32 numLtSps = 0;
						if (sps.numLongTermRefPicsSps > 0)
							numLtSps = (int32)br.UE();
						const int32 numLtPics = (int32)br.UE();
						if (numLtSps + numLtPics > 32)
							return false;
						for (int32 i = 0; i < numLtSps + numLtPics; ++i) {
							if (i < numLtSps) {
								if (sps.numLongTermRefPicsSps > 1)
									br.U(CeilLog2((uint32)sps.numLongTermRefPicsSps)); // lt_idx_sps
								// used_by_curr est porté par le SPS pour ces entrées — non
								// suivi ici (x265 n'émet pas de refs long terme) : approximé
								// "used" pour rester conservateur sur NumPicTotalCurr.
								++ltUsedCount;
							} else {
								br.U(sps.log2MaxPocLsb); // poc_lsb_lt
								if (br.U1())			 // used_by_curr_pic_lt_flag
									++ltUsedCount;
							}
							if (br.U1())	// delta_poc_msb_present_flag
								br.UE();	// delta_poc_msb_cycle_lt
						}
					}
					if (sps.spsTemporalMvpEnabled)
						out.sliceTemporalMvpEnabled = br.U1() != 0;
				}
				if (sps.sampleAdaptiveOffsetEnabled) {
					out.saoLuma = br.U1() != 0;
					if (sps.chromaFormatIdc != 0 && !sps.separateColourPlane) // ChromaArrayType != 0
						out.saoChroma = br.U1() != 0;
				}
				if (out.sliceType == kHevcSliceP || out.sliceType == kHevcSliceB) {
					const bool isB = (out.sliceType == kHevcSliceB);
					out.numRefIdxL0Active = pps.numRefIdxL0DefaultActive;
					out.numRefIdxL1Active = isB ? pps.numRefIdxL1DefaultActive : 0;
					if (br.U1()) { // num_ref_idx_active_override_flag
						out.numRefIdxL0Active = (int32)br.UE() + 1;
						if (isB)
							out.numRefIdxL1Active = (int32)br.UE() + 1;
					}
					int32 numPicTotalCurr = ltUsedCount;
					for (int32 i = 0; i < out.rps.numNegativePics; ++i)
						if (out.rps.usedS0[i])
							++numPicTotalCurr;
					for (int32 i = 0; i < out.rps.numPositivePics; ++i)
						if (out.rps.usedS1[i])
							++numPicTotalCurr;
					if (pps.listsModificationPresent && numPicTotalCurr > 1)
						return false; // ref_pic_lists_modification() pas encore porté (x265 : jamais émis)
					if (isB)
						out.mvdL1Zero = br.U1() != 0;
					if (pps.cabacInitPresent)
						out.cabacInit = br.U1() != 0;
					if (out.sliceTemporalMvpEnabled) {
						if (isB)
							out.collocatedFromL0 = br.U1() != 0;
						if ((out.collocatedFromL0 && out.numRefIdxL0Active > 1) ||
							(!out.collocatedFromL0 && out.numRefIdxL1Active > 1))
							out.collocatedRefIdx = (int32)br.UE();
					}
					if ((pps.weightedPred && !isB) || (pps.weightedBipred && isB)) {
						// pred_weight_table() (§7.3.6.3) — poids/offsets RÉSOLUS (brique 10,
						// §8.5.3.3.4.2 pour les valeurs par défaut). x265 active weightp par
						// défaut sur les P. Hypothèse Main/Main10 mono-couche : les flags
						// luma/chroma_weight_lX_flag sont toujours présents (la condition
						// §7.3.6.3 "même couche ET même POC" qui les ferait sauter n'existe
						// qu'en SHVC/SCC).
						const bool hasChroma = (sps.chromaFormatIdc != 0 && !sps.separateColourPlane);
						out.lumaLog2WeightDenom = (int32)br.UE();
						out.chromaLog2WeightDenom = out.lumaLog2WeightDenom;
						if (hasChroma)
							out.chromaLog2WeightDenom += (int32)br.SE();
						const int32 lumaDefault = 1 << out.lumaLog2WeightDenom;
						const int32 chromaDefault = 1 << out.chromaLog2WeightDenom;
						for (int32 list = 0; list < (isB ? 2 : 1); ++list) {
							const int32 numRefs = (list == 0) ? out.numRefIdxL0Active : out.numRefIdxL1Active;
							if (numRefs > 16)
								return false;
							int32 *lumaW = (list == 0) ? out.lumaWeightL0 : out.lumaWeightL1;
							int32 *lumaO = (list == 0) ? out.lumaOffsetL0 : out.lumaOffsetL1;
							int32 (*chromaW)[2] = (list == 0) ? out.chromaWeightL0 : out.chromaWeightL1;
							int32 (*chromaO)[2] = (list == 0) ? out.chromaOffsetL0 : out.chromaOffsetL1;
							bool lumaFlag[16], chromaFlag[16];
							for (int32 i = 0; i < numRefs; ++i)
								lumaFlag[i] = br.U1() != 0;
							for (int32 i = 0; i < numRefs; ++i)
								chromaFlag[i] = hasChroma ? (br.U1() != 0) : false;
							for (int32 i = 0; i < numRefs; ++i) {
								lumaW[i] = lumaDefault;
								lumaO[i] = 0;
								if (lumaFlag[i]) {
									lumaW[i] = lumaDefault + (int32)br.SE(); // delta_luma_weight
									lumaO[i] = (int32)br.SE();				 // luma_offset (signé direct)
								}
								chromaW[i][0] = chromaW[i][1] = chromaDefault;
								chromaO[i][0] = chromaO[i][1] = 0;
								if (chromaFlag[i])
									for (int32 j = 0; j < 2; ++j) {
										chromaW[i][j] = chromaDefault + (int32)br.SE(); // delta_chroma_weight
										const int32 deltaOffset = (int32)br.SE();
										// §7.4.7.3 : offset final recentré sur le poids (évite le
										// biais qu'introduirait un simple decalage additif quand
										// le poids diffère du défaut).
										chromaO[i][j] = Clip3i(-128, 127,
																deltaOffset + 128 -
																	((128 * chromaW[i][j]) >> out.chromaLog2WeightDenom));
									}
							}
						}
				}
					out.maxNumMergeCand = 5 - (int32)br.UE(); // five_minus_max_num_merge_cand
					if (out.maxNumMergeCand < 1 || out.maxNumMergeCand > 5)
						return false;
				}
				out.sliceQp = pps.initQp + (int32)br.SE(); // slice_qp_delta
				if (out.sliceQp < -12 || out.sliceQp > 51)  // borne basse = -QpBdOffsetY max (profondeur 14)
					return false;
				if (pps.sliceChromaQpOffsetsPresent) {
					out.sliceCbQpOffset = (int32)br.SE();
					out.sliceCrQpOffset = (int32)br.SE();
				}
				// Déblocage : défauts PPS, remplacés si la slice signale un override.
				out.deblockingFilterDisabled = pps.ppsDeblockingFilterDisabled;
				out.sliceBetaOffsetDiv2 = pps.ppsBetaOffsetDiv2;
				out.sliceTcOffsetDiv2 = pps.ppsTcOffsetDiv2;
				bool deblockingOverride = false;
				if (pps.deblockingFilterOverrideEnabled)
					deblockingOverride = br.U1() != 0;
				if (deblockingOverride) {
					out.deblockingFilterDisabled = br.U1() != 0;
					if (!out.deblockingFilterDisabled) {
						out.sliceBetaOffsetDiv2 = (int32)br.SE();
						out.sliceTcOffsetDiv2 = (int32)br.SE();
					}
				}
				out.loopFilterAcrossSlices = pps.loopFilterAcrossSlices;
				if (pps.loopFilterAcrossSlices &&
					(out.saoLuma || out.saoChroma || !out.deblockingFilterDisabled))
					out.loopFilterAcrossSlices = br.U1() != 0;
			}
			// Points d'entrée (§7.4.7.1) — HORS du bloc !dependent (les slices dépendantes
			// en ont aussi). x265 par défaut : WPP (entropy_coding_sync) → 1 offset par
			// rangée de CTU au-delà de la première.
			if (pps.tilesEnabled || pps.entropyCodingSyncEnabled) {
				out.numEntryPointOffsets = (int32)br.UE();
				if (out.numEntryPointOffsets > 4096)
					return false;
				if (out.numEntryPointOffsets > 0) {
					const int32 offsetLen = (int32)br.UE() + 1;
					if (offsetLen < 1 || offsetLen > 32)
						return false;
					for (int32 i = 0; i < out.numEntryPointOffsets; ++i)
						out.entryPointOffsets.PushBack(br.U(offsetLen) + 1u);
				}
			}
			if (pps.sliceSegmentHeaderExtensionPresent) {
				const int32 extLen = (int32)br.UE();
				if (extLen > 256)
					return false;
				br.Skip(extLen * 8); // slice_segment_header_extension_data_byte[]
			}
			// byte_alignment() (§7.3.2.11) : un bit à 1 obligatoire puis des zéros jusqu'à
			// l'octet — vérifié STRICTEMENT (excellent détecteur de désynchronisation : si
			// un seul champ au-dessus a été mal lu, ce bit a 1 chance sur 2 d'être faux).
			if (br.U1() != 1)
				return false;
			while ((br.pos & 7) != 0)
				if (br.U1() != 0)
					return false;
			if (br.pos > br.sizeBits)
				return false;
			out.dataByteOffset = br.pos >> 3; // début de slice_data() dans le RBSP dé-émulé
			out.valid = true;
			return true;
		}

		int32 NkHevcDecoder::ComputePoc(int32 picOrderCntLsb, int32 log2MaxPocLsb, bool isIdr,
										int32 prevPocTid0) {
			if (isIdr)
				return 0; // IDR : PicOrderCntMsb=0 ET picOrderCntLsb=0 (jamais lu, §7.4.7.1)
			const int32 maxLsb = 1 << log2MaxPocLsb;
			// prevPocTid0 = Msb+Lsb par construction -> Lsb = prevPocTid0 mod maxLsb (positif).
			const int32 prevLsb = ((prevPocTid0 % maxLsb) + maxLsb) % maxLsb;
			const int32 prevMsb = prevPocTid0 - prevLsb;
			int32 msb = prevMsb;
			if (picOrderCntLsb < prevLsb && (prevLsb - picOrderCntLsb) >= maxLsb / 2)
				msb = prevMsb + maxLsb;
			else if (picOrderCntLsb > prevLsb && (picOrderCntLsb - prevLsb) > maxLsb / 2)
				msb = prevMsb - maxLsb;
			return msb + picOrderCntLsb;
		}

		void NkHevcDecoder::BuildRefPicLists(const NkHevcShortTermRps &rps, int32 poc,
											 int32 numRefIdxL0Active, int32 numRefIdxL1Active, bool isB,
											 NkHevcRefPicLists &out) {
			out = NkHevcRefPicLists{};
			// PocStCurrBefore/After (§8.3.2) : deltas CUMULÉS déjà résolus dans le RPS
			// (S0 = négatifs, ordre proximité croissante ; S1 = positifs, idem).
			int32 before[16], nBefore = 0;
			int32 after[16], nAfter = 0;
			for (int32 i = 0; i < rps.numNegativePics && i < 16; ++i)
				if (rps.usedS0[i])
					before[nBefore++] = poc + rps.deltaPocS0[i];
			for (int32 i = 0; i < rps.numPositivePics && i < 16; ++i)
				if (rps.usedS1[i])
					after[nAfter++] = poc + rps.deltaPocS1[i];
			// §8.3.4 : TempList0 = before puis after (refs long terme non supportées —
			// jamais émises par x265) ; repli MODULO si NumPicTotalCurr < num_ref_idx_active
			// (spec : RefPicListTemp0[rIdx] = TempList0[rIdx % NumPicTotalCurr]).
			int32 temp0[32], n0 = 0;
			for (int32 i = 0; i < nBefore; ++i)
				temp0[n0++] = before[i];
			for (int32 i = 0; i < nAfter; ++i)
				temp0[n0++] = after[i];
			if (n0 > 0) {
				out.numL0 = numRefIdxL0Active < 16 ? numRefIdxL0Active : 16;
				for (int32 i = 0; i < out.numL0; ++i)
					out.l0[i] = temp0[i % n0];
			}
			if (isB) {
				int32 temp1[32], n1 = 0;
				for (int32 i = 0; i < nAfter; ++i)
					temp1[n1++] = after[i];
				for (int32 i = 0; i < nBefore; ++i)
					temp1[n1++] = before[i];
				if (n1 > 0) {
					out.numL1 = numRefIdxL1Active < 16 ? numRefIdxL1Active : 16;
					for (int32 i = 0; i < out.numL1; ++i)
						out.l1[i] = temp1[i % n1];
				}
			}
		}

		bool NkHevcDecoder::SelfTest() {
			// SplitNalsAnnexB : 2 NALs synthétiques (VPS type=32, SPS type=33), en-tête
			// 2 octets HEVC (contre 1 en H.264) — vérifie l'extraction type/layer/temporal.
			uint8 buf[] = {
				0, 0, 0, 1,										// start code
				(uint8)(kHevcNalVps << 1), 0x01, 0xAA, 0xBB,	// VPS : layer=0, temporal_id_plus1=1
				0, 0, 0, 1,										// start code
				(uint8)(kHevcNalSps << 1), 0x01, 0xCC, 0xDD, 0xEE, // SPS : layer=0, temporal_id_plus1=1
			};
			NkVector<NkHevcNal> nals;
			NkHevcDecoder::SplitNalsAnnexB(buf, sizeof(buf), nals);
			if (nals.Size() != 2)
				return false;
			if (nals[0].type != kHevcNalVps || nals[0].layerId != 0 || nals[0].temporalId != 0)
				return false;
			if (nals[1].type != kHevcNalSps || nals[1].layerId != 0 || nals[1].temporalId != 0)
				return false;
			if (nals[0].size != 4 || nals[1].size != 5) // en-tête (2) + charge utile factice
				return false;

			// VPS/SPS/PPS/slice(IDR) RÉELS (322x242 4:2:0 8-bit profil Main, produits par
			// ffmpeg/libx265 — cf. validation --hevcheader vs ffprobe). Slice tronquée à 24
			// octets : le slice header tient dans les tout premiers octets du NAL, le reste
			// (slice_data CABAC) n'est pas nécessaire à ce test structurel.
			static const uint8 kVps[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00,
										 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
										 0x3c, 0x95, 0x98, 0x09};
			static const uint8 kSpsReal[] = {0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90,
											 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3c, 0xa0, 0x0a,
											 0x48, 0x0f, 0x9c, 0x92, 0x65, 0x95, 0x9a, 0x49, 0x32, 0xbc,
											 0x05, 0xa0, 0x20, 0x00, 0x00, 0x03, 0x00, 0x20, 0x00, 0x00,
											 0x03, 0x03, 0x21};
			static const uint8 kPpsReal[] = {0x44, 0x01, 0xc1, 0x72, 0xb4, 0x62, 0x40};
			static const uint8 kIdrReal[] = {0x28, 0x01, 0xaf, 0x1d, 0x20, 0xab, 0x19, 0x8e, 0xb4, 0x20,
											 0xb3, 0xdb, 0xdf, 0x8c, 0x6e, 0x90, 0x4f, 0xff, 0xeb, 0x9d,
											 0x3d, 0x33, 0x81, 0xb4};

			int32 vpsId;
			NkHevcProfileTierLevel ptl;
			if (!ParseVps(kVps, sizeof(kVps), vpsId, ptl) || vpsId != 0 || ptl.generalProfileIdc != 1)
				return false;

			NkHevcSps sps;
			if (!ParseSps(kSpsReal, sizeof(kSpsReal), sps))
				return false;
			if (!sps.valid || sps.width != 328 || sps.height != 248 || sps.ptl.generalProfileIdc != 1)
				return false;
			if (sps.chromaFormatIdc != 1 || sps.bitDepthLuma != 8 || sps.bitDepthChroma != 8)
				return false;
			if (!sps.conformanceWindow || sps.confWinRight != 3 || sps.confWinBottom != 3)
				return false;

			NkHevcPps pps;
			if (!ParsePps(kPpsReal, sizeof(kPpsReal), pps))
				return false;
			if (!pps.valid || pps.ppsId != 0 || pps.spsId != 0 || !pps.cuQpDeltaEnabled)
				return false;
			if (!pps.entropyCodingSyncEnabled || pps.tilesEnabled)
				return false;

			// Slice header IDR -> premiere slice (couvre toute l'image), I-slice, PPS 0,
			// aucune "sortie des images anterieures" supprimee (flux mono-GOP).
			NkHevcSliceHeader sh;
			if (!ParseSliceHeader(kIdrReal, sizeof(kIdrReal), sps, pps, sh))
				return false;
			if (!sh.valid || !sh.firstSliceSegmentInPic || sh.noOutputOfPriorPics)
				return false;
			if (sh.ppsId != 0 || sh.sliceType != kHevcSliceI || !sh.isIdr)
				return false;

			// ComputePoc (brique 9) : IDR -> 0 ; sans wraparound (poc = lsb direct, cas de
			// TOUS nos flux de test réels — log2MaxPocLsb=8, GOP de 25 images) ; AVEC
			// wraparound (maxLsb=16, prevPocTid0=14 -> lsb=2 doit redevenir POC=18, pas 2).
			if (ComputePoc(0, 8, true, 999) != 0)
				return false;
			if (ComputePoc(4, 8, false, 0) != 4) // P après l'IDR (poc_lsb=4 réel, brique 3)
				return false;
			if (ComputePoc(2, 8, false, 4) != 2) // B suivant (poc_lsb=2 réel)
				return false;
			if (ComputePoc(2, 4, false, 14) != 18) // wraparound : lsb 14->2 = +4 en POC réel
				return false;

			// BuildRefPicLists (brique 9) : 1 seule réf disponible (delta -4, POC courant 8
			// -> réf attendue POC 4), num_ref_idx_l0_active=3 -> répétition modulo (§8.3.4).
			NkHevcShortTermRps rps;
			rps.numNegativePics = 1;
			rps.deltaPocS0[0] = -4;
			rps.usedS0[0] = true;
			NkHevcRefPicLists lists;
			BuildRefPicLists(rps, 8, 3, 0, false, lists);
			if (lists.numL0 != 3 || lists.l0[0] != 4 || lists.l0[1] != 4 || lists.l0[2] != 4)
				return false;
			if (lists.numL1 != 0)
				return false;
			// Cas B : 2 refs (avant + après), L0 = before puis after, L1 = after puis before.
			rps.numPositivePics = 1;
			rps.deltaPocS1[0] = 2;
			rps.usedS1[0] = true;
			BuildRefPicLists(rps, 8, 2, 2, true, lists);
			if (lists.numL0 != 2 || lists.l0[0] != 4 || lists.l0[1] != 10)
				return false;
			if (lists.numL1 != 2 || lists.l1[0] != 10 || lists.l1[1] != 4)
				return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
