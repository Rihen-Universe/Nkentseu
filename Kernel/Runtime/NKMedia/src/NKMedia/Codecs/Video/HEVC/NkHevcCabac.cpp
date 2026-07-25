// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcCabac.cpp — init contextes CABAC HEVC (§9.3.2.2).
// Tables d'initValues alignées sur la référence ffmpeg (libavcodec/hevc/cabac.c),
// cross-checkées numériquement contre elle (script de vérification, brique 4).
// =============================================================================
#include "NKMedia/Codecs/Video/HEVC/NkHevcCabac.h"

namespace nkentseu {
	namespace media {

		namespace {

			// "Context Never Used" — valeur neutre (état équiprobable) pour les contextes
			// qu'un initType donné ne peut pas rencontrer (ex. éléments inter en slice I).
			constexpr uint8 kCnu = 154;

			// [initType][ctxIdx] — §9.3.2.2 Tables 9-5..9-31, disposition ffmpeg.
			const uint8 kHevcInitValues[3][kHevcContextCount] = {
				{
					// initType 0 (slices I)
					153,											 // sao_merge_flag
					200,											 // sao_type_idx
					139, 141, 157,									 // split_cu_flag
					154,											 // cu_transquant_bypass_flag
					kCnu, kCnu, kCnu,								 // cu_skip_flag (inter — CNU en I)
					154, 154, 154,									 // cu_qp_delta_abs
					kCnu,											 // pred_mode_flag
					184, kCnu, kCnu, kCnu,							 // part_mode
					184,											 // prev_intra_luma_pred_flag
					63, 139,										 // intra_chroma_pred_mode
					kCnu,											 // merge_flag
					kCnu,											 // merge_idx
					kCnu, kCnu, kCnu, kCnu, kCnu,					 // inter_pred_idc
					kCnu, kCnu,										 // ref_idx_l0
					kCnu, kCnu,										 // ref_idx_l1
					kCnu, kCnu,										 // abs_mvd_greater0_flag
					kCnu, kCnu,										 // abs_mvd_greater1_flag
					kCnu,											 // mvp_l0_flag
					kCnu,											 // rqt_root_cbf (no_residual)
					153, 138, 138,									 // split_transform_flag
					111, 141,										 // cbf_luma
					94, 138, 182, 154, 154,							 // cbf_cb / cbf_cr
					139, 139,										 // transform_skip_flag
					139, 139,										 // explicit_rdpcm_flag (ext)
					139, 139,										 // explicit_rdpcm_dir_flag (ext)
					110, 110, 124, 125, 140, 153, 125, 127, 140,	 // last_sig_coeff_x_prefix
					109, 111, 143, 127, 111, 79, 108, 123, 63,		 //   (18)
					110, 110, 124, 125, 140, 153, 125, 127, 140,	 // last_sig_coeff_y_prefix
					109, 111, 143, 127, 111, 79, 108, 123, 63,		 //   (18)
					91, 171, 134, 141,								 // coded_sub_block_flag
					111, 111, 125, 110, 110, 94, 124, 108, 124,	 // sig_coeff_flag (44)
					107, 125, 141, 179, 153, 125, 107, 125, 141,	 //
					179, 153, 125, 107, 125, 141, 179, 153, 125,	 //
					140, 139, 182, 182, 152, 136, 152, 136, 153,	 //
					136, 139, 111, 136, 139, 111,					 //
					141, 111,										 //   (+2 ext transform-skip)
					140, 92, 137, 138, 140, 152, 138, 139, 153,	 // coeff_abs_level_greater1 (24)
					74, 149, 92, 139, 107, 122, 152, 140, 179,		 //
					166, 182, 140, 227, 122, 197,					 //
					138, 153, 136, 167, 152, 152,					 // coeff_abs_level_greater2 (6)
					154, 154, 154, 154, 154, 154, 154, 154,			 // log2_res_scale_abs (ext)
					154, 154,										 // res_scale_sign_flag (ext)
					154,											 // cu_chroma_qp_offset_flag (ext)
					154,											 // cu_chroma_qp_offset_idx (ext)
				},
				{
					// initType 1
					153,											 // sao_merge_flag
					185,											 // sao_type_idx
					107, 139, 126,									 // split_cu_flag
					154,											 // cu_transquant_bypass_flag
					197, 185, 201,									 // cu_skip_flag
					154, 154, 154,									 // cu_qp_delta_abs
					149,											 // pred_mode_flag
					154, 139, 154, 154,								 // part_mode
					154,											 // prev_intra_luma_pred_flag
					152, 139,										 // intra_chroma_pred_mode
					110,											 // merge_flag
					122,											 // merge_idx
					95, 79, 63, 31, 31,								 // inter_pred_idc
					153, 153,										 // ref_idx_l0
					153, 153,										 // ref_idx_l1
					140, 198,										 // abs_mvd_greater0_flag
					140, 198,										 // abs_mvd_greater1_flag
					168,											 // mvp_l0_flag
					79,												 // rqt_root_cbf
					124, 138, 94,									 // split_transform_flag
					153, 111,										 // cbf_luma
					149, 107, 167, 154, 154,						 // cbf_cb / cbf_cr
					139, 139,										 // transform_skip_flag
					139, 139,										 // explicit_rdpcm_flag
					139, 139,										 // explicit_rdpcm_dir_flag
					125, 110, 94, 110, 95, 79, 125, 111, 110,		 // last_sig_coeff_x_prefix
					78, 110, 111, 111, 95, 94, 108, 123, 108,		 //   (18)
					125, 110, 94, 110, 95, 79, 125, 111, 110,		 // last_sig_coeff_y_prefix
					78, 110, 111, 111, 95, 94, 108, 123, 108,		 //   (18)
					121, 140, 61, 154,								 // coded_sub_block_flag
					155, 154, 139, 153, 139, 123, 123, 63, 153,	 // sig_coeff_flag (44)
					166, 183, 140, 136, 153, 154, 166, 183, 140,	 //
					136, 153, 154, 166, 183, 140, 136, 153, 154,	 //
					170, 153, 123, 123, 107, 121, 107, 121, 167,	 //
					151, 183, 140, 151, 183, 140,					 //
					140, 140,										 //   (+2 ext)
					154, 196, 196, 167, 154, 152, 167, 182, 182,	 // coeff_abs_level_greater1 (24)
					134, 149, 136, 153, 121, 136, 137, 169, 194,	 //
					166, 167, 154, 167, 137, 182,					 //
					107, 167, 91, 122, 107, 167,					 // coeff_abs_level_greater2 (6)
					154, 154, 154, 154, 154, 154, 154, 154,			 // log2_res_scale_abs
					154, 154,										 // res_scale_sign_flag
					154,											 // cu_chroma_qp_offset_flag
					154,											 // cu_chroma_qp_offset_idx
				},
				{
					// initType 2
					153,											 // sao_merge_flag
					160,											 // sao_type_idx
					107, 139, 126,									 // split_cu_flag
					154,											 // cu_transquant_bypass_flag
					197, 185, 201,									 // cu_skip_flag
					154, 154, 154,									 // cu_qp_delta_abs
					134,											 // pred_mode_flag
					154, 139, 154, 154,								 // part_mode
					183,											 // prev_intra_luma_pred_flag
					152, 139,										 // intra_chroma_pred_mode
					154,											 // merge_flag
					137,											 // merge_idx
					95, 79, 63, 31, 31,								 // inter_pred_idc
					153, 153,										 // ref_idx_l0
					153, 153,										 // ref_idx_l1
					169, 198,										 // abs_mvd_greater0_flag
					169, 198,										 // abs_mvd_greater1_flag
					168,											 // mvp_l0_flag
					79,												 // rqt_root_cbf
					224, 167, 122,									 // split_transform_flag
					153, 111,										 // cbf_luma
					149, 92, 167, 154, 154,							 // cbf_cb / cbf_cr
					139, 139,										 // transform_skip_flag
					139, 139,										 // explicit_rdpcm_flag
					139, 139,										 // explicit_rdpcm_dir_flag
					125, 110, 124, 110, 95, 94, 125, 111, 111,		 // last_sig_coeff_x_prefix
					79, 125, 126, 111, 111, 79, 108, 123, 93,		 //   (18)
					125, 110, 124, 110, 95, 94, 125, 111, 111,		 // last_sig_coeff_y_prefix
					79, 125, 126, 111, 111, 79, 108, 123, 93,		 //   (18)
					121, 140, 61, 154,								 // coded_sub_block_flag
					170, 154, 139, 153, 139, 123, 123, 63, 124,	 // sig_coeff_flag (44)
					166, 183, 140, 136, 153, 154, 166, 183, 140,	 //
					136, 153, 154, 166, 183, 140, 136, 153, 154,	 //
					170, 153, 138, 138, 122, 121, 122, 121, 167,	 //
					151, 183, 140, 151, 183, 140,					 //
					140, 140,										 //   (+2 ext)
					154, 196, 167, 167, 154, 152, 167, 182, 182,	 // coeff_abs_level_greater1 (24)
					134, 149, 136, 153, 121, 136, 122, 169, 208,	 //
					166, 167, 154, 152, 167, 182,					 //
					107, 167, 91, 107, 107, 167,					 // coeff_abs_level_greater2 (6)
					154, 154, 154, 154, 154, 154, 154, 154,			 // log2_res_scale_abs
					154, 154,										 // res_scale_sign_flag
					154,											 // cu_chroma_qp_offset_flag
					154,											 // cu_chroma_qp_offset_idx
				},
			};

		} // namespace

		int32 NkHevcCabacState::InitTypeFor(int32 sliceType, bool cabacInitFlag) {
			if (sliceType == 2) // I
				return 0;
			if (sliceType == 1) // P
				return cabacInitFlag ? 2 : 1;
			return cabacInitFlag ? 1 : 2; // B
		}

		const uint8 *NkHevcCabacState::InitValues(int32 initType) {
			if (initType < 0 || initType > 2)
				return nullptr;
			return kHevcInitValues[initType];
		}

		void NkHevcCabacState::Init(int32 sliceQp, int32 initType) {
			if (initType < 0)
				initType = 0;
			if (initType > 2)
				initType = 2;
			const uint8 *iv = kHevcInitValues[initType];
			for (int32 i = 0; i < kHevcContextCount; ++i) {
				// §9.3.2.2 : initValue 8 bits -> (m, n), puis MÊME formule qu'H.264.
				const int32 slopeIdx = iv[i] >> 4;
				const int32 offsetIdx = iv[i] & 15;
				const int32 m = slopeIdx * 5 - 45;
				const int32 n = (offsetIdx << 3) - 16;
				ctx[i] = NkCabacInitOne(m, n, sliceQp);
			}
		}

		bool NkHevcCabacState::SelfTest() {
			// 1) Cohérence du layout : la disposition chaînée doit donner 179 contextes
			//    (comme la référence ffmpeg), et des sommes de contrôle identiques aux
			//    valeurs calculées indépendamment depuis le fichier source ffmpeg.
			if (kHevcContextCount != 179)
				return false;
			const uint32 kExpectedSums[3] = {24768, 24504, 24894};
			for (int32 t = 0; t < 3; ++t) {
				uint32 sum = 0;
				const uint8 *iv = InitValues(t);
				for (int32 i = 0; i < kHevcContextCount; ++i)
					sum += iv[i];
				if (sum != kExpectedSums[t])
					return false;
			}

			// 2) Formule d'init (§9.3.2.2) : initValue 154 (0x9A) -> slope 9, offset 10 ->
			//    m=0, n=64 -> preCtxState=64 quel que soit le QP -> état ÉQUIPROBABLE
			//    (pStateIdx=0, valMPS=1). C'est le sens de la valeur "CNU".
			NkHevcCabacState st;
			st.Init(26, 0);
			if (st.ctx[kHevcCtxCuTransquantBypass].pStateIdx != 0 ||
				st.ctx[kHevcCtxCuTransquantBypass].valMPS != 1)
				return false;
			//    sao_type_idx en I (initValue 200=0xC8 : slope 12, offset 8 -> m=15, n=48)
			//    à QP 33 : pre = ((15*33)>>4)+48 = 78 -> pStateIdx=14, valMPS=1.
			st.Init(33, 0);
			if (st.ctx[kHevcCtxSaoTypeIdx].pStateIdx != 14 || st.ctx[kHevcCtxSaoTypeIdx].valMPS != 1)
				return false;

			// 3) Moteur (partagé H.264, régression mécanique) : séquence connue calculée
			//    indépendamment (implémentation Python de l'algorithme de la spec sur les
			//    mêmes octets) — 8 bypass depuis {0xAA,0x55,...} puis 4 decisions sur un
			//    contexte équiprobable, puis terminate=0.
			static const uint8 kBytes[8] = {0xAA, 0x55, 0x33, 0xCC, 0x0F, 0xF0, 0x99, 0x66};
			NkCabacEngine eng;
			eng.InitEngine(kBytes, sizeof(kBytes), 0);
			if (eng.codIRange != 510 || eng.codIOffset != 340)
				return false;
			static const uint8 kExpectedBypass[8] = {1, 0, 1, 0, 1, 0, 1, 1};
			for (int32 i = 0; i < 8; ++i)
				if (eng.DecodeBypass() != kExpectedBypass[i])
					return false;
			NkCabacCtx c; // équiprobable : pStateIdx=0, valMPS=0
			static const uint8 kExpectedDecision[4] = {0, 0, 0, 0};
			for (int32 i = 0; i < 4; ++i)
				if (eng.DecodeDecision(c) != kExpectedDecision[i])
					return false;
			if (eng.DecodeTerminate() != 0)
				return false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
