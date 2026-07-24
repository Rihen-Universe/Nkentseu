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

		} // namespace

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
			// S'ARRÊTE ICI (brique 2) : TU-tree sizes/scaling_list_data()/amp/sao/pcm/
			// st_ref_pic_set()/long_term_ref/temporal_mvp/strong_intra_smoothing/
			// vui_parameters() restent à porter pour les briques suivantes (contenu réel
			// et slice header complet), pas nécessaires aux infos structurelles.
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
				br.UE(); // diff_cu_qp_delta_depth
			br.SE();	 // pps_cb_qp_offset
			br.SE();	 // pps_cr_qp_offset
			br.U1();	 // pps_slice_chroma_qp_offsets_present_flag
			br.U1();	 // weighted_pred_flag
			br.U1();	 // weighted_bipred_flag
			br.U1();	 // transquant_bypass_enabled_flag
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
			// S'ARRÊTE ICI (brique 1) : pps_loop_filter_across_slices/deblocking_filter_
			// control/scaling_list_data/pps_extension restent à porter pour les briques
			// suivantes.
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
				if (!out.isIdr) {
					out.picOrderCntLsb = (int32)br.U(sps.log2MaxPocLsb);
					out.shortTermRefPicSetSpsFlag = br.U1() != 0;
					// S'ARRÊTE ICI (brique 2) : short_term_ref_pic_set()/long-term ref/
					// ref_pic_lists_modification()/pred_weight_table()/deblocking
					// overrides restent à porter pour les briques suivantes.
				}
			}
			out.valid = true;
			return true;
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
			return true;
		}

	} // namespace media
} // namespace nkentseu
