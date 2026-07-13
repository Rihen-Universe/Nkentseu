// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Decoder.cpp — brique 1 (NAL split + SPS).
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"

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

			br.UE(); // log2_max_frame_num_minus4
			const uint32 pocType = br.UE();
			if (pocType == 0) {
				br.UE(); // log2_max_pic_order_cnt_lsb_minus4
			} else if (pocType == 1) {
				br.U1(); // delta_pic_order_always_zero_flag
				br.SE(); // offset_for_non_ref_pic
				br.SE(); // offset_for_top_to_bottom_field
				const uint32 num = br.UE();
				for (uint32 i = 0; i < num; ++i)
					br.SE();
			}
			out.numRefFrames = (int32)br.UE(); // max_num_ref_frames
			br.U1();						   // gaps_in_frame_num_value_allowed_flag

			const uint32 wMbs = br.UE() + 1;   // pic_width_in_mbs_minus1
			const uint32 hMap = br.UE() + 1;   // pic_height_in_map_units_minus1
			const uint32 frameMbsOnly = br.U1();
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

		bool NkH264Decoder::SelfTest() {
			// SPS baseline 176x144 (profil 66) réel produit par x264.
			static const uint8 kSps[] = {0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x02, 0xc4, 0xec, 0x04, 0x40, 0x00, 0x00,
										 0x03, 0x00, 0x40, 0x00, 0x00, 0x03, 0x02, 0x83, 0xc4, 0x89, 0x92};
			// 1) découpage NAL (on préfixe un start code).
			NkVector<nk_uint8> stream;
			stream.PushBack(0);
			stream.PushBack(0);
			stream.PushBack(1);
			for (usize i = 0; i < sizeof(kSps); ++i)
				stream.PushBack(kSps[i]);
			NkVector<NkH264Nal> nals;
			SplitNalsAnnexB(stream.Data(), (usize)stream.Size(), nals);
			if (nals.Size() != 1 || nals[0].type != 7)
				return false;

			// 2) parsing SPS -> profil 66, 176x144.
			NkH264Sps sps;
			if (!ParseSps(kSps, sizeof(kSps), sps))
				return false;
			return sps.valid && sps.profileIdc == 66 && sps.width == 176 && sps.height == 144;
		}

	} // namespace media
} // namespace nkentseu
