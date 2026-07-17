// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Decoder.cpp — brique 1 (NAL split + SPS).
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
#include "NKMedia/Codecs/Video/H264/NkH264BitReader.h"

namespace nkentseu {
	namespace media {

		namespace {

			// Lecteur de bits MSB-first sur un RBSP dé-émulé, avec Exp-Golomb.
			struct BitReader {
					const uint8 *d = nullptr;
					usize n = 0;
					usize bit = 0; // position en bits

					uint32 U1() {
						if (bit >= n * 8)
							return 0;
						const uint32 v = (d[bit >> 3] >> (7 - (bit & 7))) & 1u;
						++bit;
						return v;
					}
					uint32 U(int32 k) {
						uint32 v = 0;
						for (int32 i = 0; i < k; ++i)
							v = (v << 1) | U1();
						return v;
					}
					// Exp-Golomb non signé.
					uint32 UE() {
						int32 z = 0;
						while (bit < n * 8 && U1() == 0)
							++z;
						uint32 v = 0;
						for (int32 i = 0; i < z; ++i)
							v = (v << 1) | U1();
						return ((1u << z) - 1u) + v;
					}
					// Exp-Golomb signé.
					int32 SE() {
						const uint32 k = UE();
						return (k & 1) ? (int32)((k + 1) / 2) : -(int32)(k / 2);
					}
			};

			// Retire les octets anti-émulation 00 00 03 -> 00 00 d'un RBSP.
			void Deemulate(const uint8 *src, usize n, NkVector<nk_uint8> &out) {
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

		} // namespace

		void NkH264Decoder::SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkH264Nal> &out) {
			out.Clear();
			if (!data || size < 4)
				return;
			usize i = 0;
			// Cherche le premier start code.
			auto isStart3 = [&](usize p) { return p + 2 < size && data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 1; };
			usize prevStart = (usize)-1;
			while (i + 2 < size) {
				if (isStart3(i)) {
					const usize nalStart = i + 3;
					if (prevStart != (usize)-1) {
						usize end = i;
						// retire un éventuel 00 précédant le start code (00 00 00 01)
						while (end > prevStart && data[end - 1] == 0)
							--end;
						if (end > prevStart) {
							NkH264Nal nal;
							nal.offset = prevStart;
							nal.size = end - prevStart;
							nal.refIdc = (data[prevStart] >> 5) & 3;
							nal.type = data[prevStart] & 0x1F;
							out.PushBack(nal);
						}
					}
					prevStart = nalStart;
					i = nalStart;
				} else {
					++i;
				}
			}
			if (prevStart != (usize)-1 && prevStart < size) {
				NkH264Nal nal;
				nal.offset = prevStart;
				nal.size = size - prevStart;
				nal.refIdc = (data[prevStart] >> 5) & 3;
				nal.type = data[prevStart] & 0x1F;
				out.PushBack(nal);
			}
		}

		bool NkH264Decoder::ParseSps(const uint8 *nal, usize size, NkH264Sps &out) {
			out = NkH264Sps{};
			if (!nal || size < 5)
				return false;
			if ((nal[0] & 0x1F) != 7)
				return false; // pas un SPS

			// RBSP = tout après l'octet d'en-tête NAL, dé-émulé.
			NkVector<nk_uint8> rbsp;
			Deemulate(nal + 1, size - 1, rbsp);
			if (rbsp.Size() < 4)
				return false;
			const uint8 *r = rbsp.Data();

			const int32 profile = r[0];
			const int32 level = r[2];

			BitReader br;
			br.d = r + 3; // après profile/constraint/level (3 octets)
			br.n = (usize)rbsp.Size() - 3;
			br.bit = 0;

			br.UE(); // seq_parameter_set_id

			// Profils "high" : bloc chroma supplémentaire.
			if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 ||
				profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 ||
				profile == 139 || profile == 134 || profile == 135) {
				const uint32 chroma = br.UE();
				if (chroma == 3)
					br.U1(); // separate_colour_plane_flag
				br.UE();	 // bit_depth_luma_minus8
				br.UE();	 // bit_depth_chroma_minus8
				br.U1();	 // qpprime_y_zero_transform_bypass_flag
				if (br.U1()) // seq_scaling_matrix_present_flag
					return false; // listes de quantif. custom non gérées dans cette brique
			}

			out.log2MaxFrameNum = (int32)br.UE() + 4; // log2_max_frame_num_minus4
			const uint32 pocType = br.UE();
			out.pocType = (int32)pocType;
			if (pocType == 0) {
				out.log2MaxPocLsb = (int32)br.UE() + 4; // log2_max_pic_order_cnt_lsb_minus4
			} else if (pocType == 1) {
				out.deltaPocAlwaysZero = (int32)br.U1(); // delta_pic_order_always_zero_flag
				br.SE();								 // offset_for_non_ref_pic
				br.SE();								 // offset_for_top_to_bottom_field
				const uint32 num = br.UE();
				for (uint32 i = 0; i < num; ++i)
					br.SE();
			}
			out.numRefFrames = (int32)br.UE(); // max_num_ref_frames
			br.U1();						   // gaps_in_frame_num_value_allowed_flag

			const uint32 wMbs = br.UE() + 1;   // pic_width_in_mbs_minus1
			const uint32 hMap = br.UE() + 1;   // pic_height_in_map_units_minus1
			const uint32 frameMbsOnly = br.U1();
			out.picWidthInMbs = (int32)wMbs;
			out.picHeightInMapUnits = (int32)hMap;
			out.frameMbsOnly = (int32)frameMbsOnly;
			if (!frameMbsOnly)
				br.U1(); // mb_adaptive_frame_field_flag
			br.U1();	 // direct_8x8_inference_flag

			int32 cropL = 0, cropR = 0, cropT = 0, cropB = 0;
			if (br.U1()) { // frame_cropping_flag
				cropL = (int32)br.UE();
				cropR = (int32)br.UE();
				cropT = (int32)br.UE();
				cropB = (int32)br.UE();
			}
			// (vui_parameters_present_flag suit — non nécessaire pour les dimensions.)

			const int32 width = (int32)wMbs * 16;
			const int32 height = (int32)hMap * (2 - (int32)frameMbsOnly) * 16;
			// Cropping (unités : 4:2:0 -> 2 en X, 2*(2-frameMbsOnly) en Y).
			const int32 cropUnitX = 2;
			const int32 cropUnitY = 2 * (2 - (int32)frameMbsOnly);

			out.valid = true;
			out.profileIdc = profile;
			out.levelIdc = level;
			out.width = width - cropUnitX * (cropL + cropR);
			out.height = height - cropUnitY * (cropT + cropB);
			return out.width > 0 && out.height > 0;
		}

		bool NkH264Decoder::ParsePps(const uint8 *nal, usize size, NkH264Pps &out) {
			out = NkH264Pps{};
			if (!nal || size < 2 || (nal[0] & 0x1F) != 8)
				return false;
			NkVector<nk_uint8> rbsp;
			Deemulate(nal + 1, size - 1, rbsp);
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());
			out.ppsId = (int32)br.UE();
			out.spsId = (int32)br.UE();
			out.entropyCodingMode = (int32)br.U1();
			out.bottomFieldPocPresent = (int32)br.U1();
			out.numSliceGroups = (int32)br.UE() + 1;
			if (out.numSliceGroups > 1)
				return false; // slice groups (FMO) non gérés
			out.numRefIdxL0DefaultActive = (int32)br.UE() + 1; // num_ref_idx_l0_default_active_minus1 + 1
			out.numRefIdxL1DefaultActive = (int32)br.UE() + 1; // num_ref_idx_l1_default_active_minus1 + 1
			out.weightedPred = (int32)br.U1();		// weighted_pred_flag (P/SP)
			out.weightedBipredIdc = (int32)br.U(2); // weighted_bipred_idc (B)
			out.picInitQp = 26 + br.SE(); // pic_init_qp_minus26
			br.SE();					  // pic_init_qs_minus26
			out.chromaQpIndexOffset = br.SE();
			out.deblockingControlPresent = (int32)br.U1();
			out.constrainedIntraPred = (int32)br.U1();
			out.redundantPicCntPresent = (int32)br.U1();
			out.valid = true;
			return true;
		}

		bool NkH264Decoder::ParseSliceHeader(const uint8 *nal, usize size, const NkH264Sps &sps, const NkH264Pps &pps,
											 NkH264SliceHeader &out) {
			out = NkH264SliceHeader{};
			if (!nal || size < 2)
				return false;
			const int32 nalType = nal[0] & 0x1F;
			const int32 nalRefIdc = (nal[0] >> 5) & 3;
			NkVector<nk_uint8> rbsp;
			Deemulate(nal + 1, size - 1, rbsp);
			NkH264BitReader br(rbsp.Data(), (usize)rbsp.Size());

			out.firstMbInSlice = (int32)br.UE();
			out.sliceType = (int32)br.UE();
			out.ppsId = (int32)br.UE();
			const int32 st = out.sliceType % 5;
			out.isIntra = (st == 2); // 2 = I

			out.frameNum = (int32)br.U(sps.log2MaxFrameNum);
			// frame_mbs_only_flag supposé (pas de field_pic_flag).
			const bool idr = (nalType == 5);
			if (idr)
				out.idrPicId = (int32)br.UE();

			if (sps.pocType == 0) {
				br.U(sps.log2MaxPocLsb); // pic_order_cnt_lsb
				if (pps.bottomFieldPocPresent)
					br.SE(); // delta_pic_order_cnt_bottom
			} else if (sps.pocType == 1 && !sps.deltaPocAlwaysZero) {
				br.SE();
				br.SE();
			}
			if (pps.redundantPicCntPresent)
				br.UE(); // redundant_pic_cnt

			// (I-slice : pas de ref_pic_list_modification ni pred_weight_table.)
			if (nalRefIdc != 0) {
				if (idr) {
					br.U1(); // no_output_of_prior_pics_flag
					br.U1(); // long_term_reference_flag
				} else {
					if (br.U1()) { // adaptive_ref_pic_marking_mode_flag
						uint32 op;
						do {
							op = br.UE();
							if (op == 1 || op == 3)
								br.UE();
							if (op == 2)
								br.UE();
							if (op == 3 || op == 6)
								br.UE();
							if (op == 4)
								br.UE();
						} while (op != 0 && !br.Eof());
					}
				}
			}
			// cabac_init_idc : présent SEULEMENT si CABAC et slice non-I. Sélectionne la variante de
			// table d'init des contextes. ⚠️ Doit être lu même si on ne s'en sert pas, sinon tout le
			// reste de l'en-tête (slice_qp_delta…) est décalé.
			if (pps.entropyCodingMode == 1 && !out.isIntra)
				out.cabacInitIdc = (int32)br.UE();
			out.sliceQp = pps.picInitQp + br.SE(); // slice_qp_delta
			if (pps.deblockingControlPresent) {
				const uint32 idc = br.UE();
				if (idc != 1) {
					br.SE();
					br.SE();
				}
			}
			out.valid = (out.sliceQp >= 0 && out.sliceQp <= 51 && out.cabacInitIdc >= 0 && out.cabacInitIdc <= 2);
			return out.valid;
		}

		bool NkH264Decoder::SelfTest() {
			// Flux baseline 48x32 (profil 66, CAVLC, no-deblock) réel produit par x264.
			static const uint8 kSps[] = {0x67, 0x42, 0xc0, 0x0a, 0xdc, 0xd6, 0xc0, 0x44, 0x00, 0x00, 0x03,
										 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x08, 0x3c, 0x48, 0x9e};
			static const uint8 kPps[] = {0x68, 0xce, 0x32, 0xc8};
			static const uint8 kIdr[] = {0x65, 0x88, 0x84, 0x3a, 0xf0, 0xdc, 0x2d,
										 0x02, 0xf1, 0xb1, 0x40, 0x00, 0x41, 0x0e};

			// 1) découpage NAL (SPS + PPS enchaînés avec start codes).
			NkVector<nk_uint8> stream;
			const uint8 sc[3] = {0, 0, 1};
			for (int32 k = 0; k < 3; ++k)
				stream.PushBack(sc[k]);
			for (usize i = 0; i < sizeof(kSps); ++i)
				stream.PushBack(kSps[i]);
			for (int32 k = 0; k < 3; ++k)
				stream.PushBack(sc[k]);
			for (usize i = 0; i < sizeof(kPps); ++i)
				stream.PushBack(kPps[i]);
			NkVector<NkH264Nal> nals;
			SplitNalsAnnexB(stream.Data(), (usize)stream.Size(), nals);
			if (nals.Size() != 2 || nals[0].type != 7 || nals[1].type != 8)
				return false;

			// 2) SPS -> profil 66, 48x32.
			NkH264Sps sps;
			if (!ParseSps(kSps, sizeof(kSps), sps))
				return false;
			if (!sps.valid || sps.profileIdc != 66 || sps.width != 48 || sps.height != 32)
				return false;

			// 3) PPS -> CAVLC (entropyCodingMode 0).
			NkH264Pps pps;
			if (!ParsePps(kPps, sizeof(kPps), pps))
				return false;
			if (!pps.valid || pps.entropyCodingMode != 0)
				return false;

			// 4) slice header IDR -> I-slice, QP plausible.
			NkH264SliceHeader sh;
			if (!ParseSliceHeader(kIdr, sizeof(kIdr), sps, pps, sh))
				return false;
			return sh.valid && sh.isIntra && sh.sliceQp >= 0 && sh.sliceQp <= 51;
		}

	} // namespace media
} // namespace nkentseu
