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
			// S'ARRÊTE ICI (brique 1) : sps_sub_layer_ordering_info/CTU-CU-TU sizes/
			// scaling_list_data()/st_ref_pic_set()/vui_parameters() restent à porter
			// pour les briques suivantes (contenu réel), pas nécessaires aux infos
			// structurelles (dimensions/profil/niveau/chroma/profondeur de bits).
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
			return true;
		}

	} // namespace media
} // namespace nkentseu
