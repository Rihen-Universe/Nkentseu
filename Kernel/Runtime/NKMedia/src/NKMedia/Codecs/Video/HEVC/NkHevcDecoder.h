// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h
// -----------------------------------------------------------------------------
// DÉCODEUR H.265/HEVC — BRIQUE 1 : découpage NAL (Annex-B, en-tête 2 octets) +
// parsing VPS/SPS/PPS. Évolution directe de NkH264Decoder (même bit reader
// Exp-Golomb MSB-first, même méthode Annex-B + anti-émulation 00 00 03) — un
// décodeur HEVC complet (CABAC, quadtree CTU/CU/PU/TU, intra 35 modes, inter
// merge/AMVP, transformées DST/DCT, déblocage + SAO) est un GROS chantier
// réparti sur plusieurs sessions, comme H.264/VP8/VP9 avant lui. Cette 1re
// brique lit déjà la STRUCTURE du bitstream : isole les unités NAL (en-tête
// 2 octets : forbidden_zero_bit + nal_unit_type(6) + nuh_layer_id(6) +
// nuh_temporal_id_plus1(3), contre 1 octet en H.264), et décode VPS/SPS/PPS
// (profil, niveau, dimensions, chroma, profondeur de bits) via Exp-Golomb.
//
// ÉTAT : parsing NAL + SPS (dimensions/profil/niveau) ✅. Décodage des IMAGES
// (CABAC + CTU) = à venir. Zero-STL, nkentseu::media.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// Une unité NAL isolée (pointe dans le buffer source ; ne copie pas). §7.3.1.2.
		struct NkHevcNal {
				usize offset = 0;	 // position de l'octet d'en-tête NAL (2 octets) dans le buffer
				usize size = 0;		 // taille (en-tête + RBSP, hors start code)
				int32 type = 0;		 // nal_unit_type (32=VPS, 33=SPS, 34=PPS, 19/20=IDR, 21=CRA, …)
				int32 layerId = 0;	 // nuh_layer_id (0 = couche de base)
				int32 temporalId = 0; // nuh_temporal_id_plus1 - 1
		};

		// Types de NAL utiles (Table 7-1) — pas la liste complète, juste ceux
		// consultés par le décodeur (structure + futures briques inter/intra).
		enum NkHevcNalType : nk_int32 {
			kHevcNalTrailN = 0,
			kHevcNalTrailR = 1,
			kHevcNalTsaN = 2,
			kHevcNalTsaR = 3,
			kHevcNalStsaN = 4,
			kHevcNalStsaR = 5,
			kHevcNalRadlN = 6,
			kHevcNalRadlR = 7,
			kHevcNalRaslN = 8,
			kHevcNalRaslR = 9,
			kHevcNalBlaWLp = 16,
			kHevcNalBlaWRadl = 17,
			kHevcNalBlaNLp = 18,
			kHevcNalIdrWRadl = 19,
			kHevcNalIdrNLp = 20,
			kHevcNalCra = 21,
			kHevcNalVps = 32,
			kHevcNalSps = 33,
			kHevcNalPps = 34,
			kHevcNalAud = 35,
			kHevcNalEos = 36,
			kHevcNalEob = 37,
			kHevcNalFd = 38,
			kHevcNalPrefixSei = 39,
			kHevcNalSuffixSei = 40,
		};

		// profile_tier_level() (§7.3.3) — seuls les champs "general_*" utiles à la
		// brique 1 sont exposés ; les 88 bits de constraint/compatibility flags et
		// les infos par sous-couche sont lus (bit-exactement, pour rester synchrone
		// avec le reste du RBSP) mais pas exposés — pas nécessaires hors extensions
		// scalables/multivues.
		struct NkHevcProfileTierLevel {
				int32 generalProfileSpace = 0;
				bool generalTierFlag = false;
				int32 generalProfileIdc = 0;
				int32 generalLevelIdc = 0; // = level_idc réel * 30 (ex. niveau 5.0 -> 150)
		};

		// short_term_ref_pic_set() DÉRIVÉ (§7.4.8) : deltas de POC CUMULÉS (négatifs pour
		// S0, positifs pour S1), après résolution éventuelle de la prédiction inter-RPS.
		struct NkHevcShortTermRps {
				int32 numNegativePics = 0;
				int32 numPositivePics = 0;
				int32 deltaPocS0[16] = {0}; // valeurs NÉGATIVES, ordre POC décroissant
				bool usedS0[16] = {false};
				int32 deltaPocS1[16] = {0}; // valeurs POSITIVES, ordre POC croissant
				bool usedS1[16] = {false};
		};

		// Infos extraites d'un SPS. §7.3.2.2.
		struct NkHevcSps {
				bool valid = false;
				int32 spsId = 0;
				int32 vpsId = 0;
				int32 maxSubLayersMinus1 = 0;
				NkHevcProfileTierLevel ptl;
				int32 chromaFormatIdc = 1; // 0=mono,1=4:2:0,2=4:2:2,3=4:4:4
				bool separateColourPlane = false;
				int32 width = 0;  // pic_width_in_luma_samples (AVANT conformance window crop)
				int32 height = 0; // pic_height_in_luma_samples
				// Fenêtre de conformance (recadrage affichage) — en unités de SOUS-ÉCHANTILLON
				// chroma (§7.4.3.2.1 : ×SubWidthC/SubHeightC pour convertir en pixels luma).
				bool conformanceWindow = false;
				int32 confWinLeft = 0, confWinRight = 0, confWinTop = 0, confWinBottom = 0;
				int32 bitDepthLuma = 8;   // bit_depth_luma_minus8 + 8
				int32 bitDepthChroma = 8; // bit_depth_chroma_minus8 + 8
				int32 log2MaxPocLsb = 4;  // log2_max_pic_order_cnt_lsb_minus4 + 4
				// Taille des CTU (arbre de codage) — nécessaire pour dériver PicSizeInCtbsY,
				// qui fixe la largeur en bits de slice_segment_address dans le slice header
				// (§7.4.7.1) : CtbLog2SizeY = log2MinCbSizeY + log2DiffMaxMinCbSizeY.
				int32 log2MinCbSizeY = 3;		 // log2_min_luma_coding_block_size_minus3 + 3
				int32 log2DiffMaxMinCbSizeY = 0; // log2_diff_max_min_luma_coding_block_size
				bool scalingListEnabled = false;
				bool ampEnabled = false;
				bool sampleAdaptiveOffsetEnabled = false; // gate des flags SAO du slice header
				bool pcmEnabled = false;
				// Jeux de RPS candidats signalés dans le SPS (une slice les référence par index,
				// ou signale le sien inline — x265 par défaut : num=0, RPS inline par slice).
				int32 numShortTermRefPicSets = 0; // <= 64
				NkHevcShortTermRps shortTermRps[64];
				bool longTermRefPicsPresent = false;
				int32 numLongTermRefPicsSps = 0;
				bool spsTemporalMvpEnabled = false; // gate de slice_temporal_mvp_enabled_flag
				bool strongIntraSmoothingEnabled = false;
		};

		// Type de slice (§7.4.7.1, Table 7-7) — même convention numérique que H.264
		// (0=B, 1=P, 2=I) bien que ce soit une coïncidence de la spec, pas un partage.
		enum NkHevcSliceType : nk_int32 {
			kHevcSliceB = 0,
			kHevcSliceP = 1,
			kHevcSliceI = 2,
		};

		// En-tête de slice — briques 2+3 : parsing COMPLET jusqu'à slice_qp_delta inclus
		// (avant les offsets QP chroma/SAO-déblocage par slice/points d'entrée tuiles).
		// Refus propre (return false) sur : pred_weight_table (pondération explicite),
		// ref_pic_lists_modification (si réellement présent), scaling_list_data.
		struct NkHevcSliceHeader {
				bool valid = false;
				int32 nalType = 0;
				bool firstSliceSegmentInPic = false;
				bool noOutputOfPriorPics = false; // valide seulement si IRAP (BLA/IDR/CRA)
				int32 ppsId = 0;
				bool dependentSliceSegment = false;
				int32 sliceSegmentAddress = 0;
				int32 sliceType = 0; // NkHevcSliceType
				bool picOutputFlag = true;
				int32 colourPlaneId = 0;
				bool isIdr = false;
				int32 picOrderCntLsb = 0;				// valide seulement si !isIdr
				bool shortTermRefPicSetSpsFlag = false; // valide seulement si !isIdr
				// RPS effectif de la slice (inline décodé, ou copié du SPS via l'index).
				NkHevcShortTermRps rps;
				int32 shortTermRefPicSetIdx = 0;
				bool sliceTemporalMvpEnabled = false;
				bool saoLuma = false, saoChroma = false;
				int32 numRefIdxL0Active = 0, numRefIdxL1Active = 0; // résolus (override ou défauts PPS)
				bool mvdL1Zero = false;
				bool cabacInit = false;
				bool collocatedFromL0 = true;
				int32 collocatedRefIdx = 0;
				int32 maxNumMergeCand = 5; // 5 - five_minus_max_num_merge_cand
				int32 sliceQp = 26;		   // 26 + init_qp_minus26 (PPS) + slice_qp_delta
		};

		// Infos extraites d'un PPS. §7.3.2.3 (sous-ensemble structurel — brique 1).
		struct NkHevcPps {
				bool valid = false;
				int32 ppsId = 0;
				int32 spsId = 0;
				bool dependentSliceSegmentsEnabled = false;
				bool outputFlagPresent = false;
				int32 numExtraSliceHeaderBits = 0;
				bool signDataHiding = false;
				bool cabacInitPresent = false;
				int32 numRefIdxL0DefaultActive = 1; // num_ref_idx_l0_default_active_minus1 + 1
				int32 numRefIdxL1DefaultActive = 1;
				int32 initQp = 26; // 26 + init_qp_minus26
				bool constrainedIntraPred = false;
				bool transformSkipEnabled = false;
				bool cuQpDeltaEnabled = false;
				bool weightedPred = false;	 // weighted_pred_flag (P) — gate de pred_weight_table
				bool weightedBipred = false; // weighted_bipred_flag (B)
				bool tilesEnabled = false;
				bool entropyCodingSyncEnabled = false;
				int32 numTileColumnsMinus1 = 0, numTileRowsMinus1 = 0;
				bool loopFilterAcrossTiles = true;
				bool loopFilterAcrossSlices = false;
				bool deblockingFilterControlPresent = false;
				bool deblockingFilterOverrideEnabled = false;
				bool ppsDeblockingFilterDisabled = false;
				int32 ppsBetaOffsetDiv2 = 0, ppsTcOffsetDiv2 = 0;
				bool listsModificationPresent = false;
				int32 log2ParallelMergeLevel = 2; // log2_parallel_merge_level_minus2 + 2
				bool sliceSegmentHeaderExtensionPresent = false;
		};

		struct NkHevcDecoder {
			public:
				// Découpe un flux Annex-B (start codes 00 00 01 / 00 00 00 01) en unités
				// NAL individuelles. En-tête NAL HEVC = 2 OCTETS (contre 1 en H.264) :
				// forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6) +
				// nuh_temporal_id_plus1(3).
				static void SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkHevcNal> &out);

				// Parse un VPS (§7.3.2.1) — validation structurelle minimale (id +
				// profile_tier_level) : le VPS complet (couches/sous-couches HRD) sert aux
				// extensions scalables/multivues, hors périmètre décodage simple couche.
				static bool ParseVps(const uint8 *nal, usize size, int32 &outVpsId,
									 NkHevcProfileTierLevel &outPtl);

				// Parse un SPS (§7.3.2.2) : dimensions/profil/niveau/chroma/profondeur de
				// bits + taille des CTU (log2MinCbSizeY/log2DiffMaxMinCbSizeY, nécessaire
				// pour dériver PicSizeInCtbsY côté slice header) — ne consomme PAS
				// scaling_list_data()/TU-tree/PCM/st_ref_pic_set()/vui_parameters() (pas
				// nécessaires ici, chacun un chantier propre pour les briques suivantes).
				static bool ParseSps(const uint8 *nal, usize size, NkHevcSps &out);

				// Parse un PPS (§7.3.2.3) : sous-ensemble structurel (tuiles, QP init,
				// flags de contrôle) — s'arrête avant deblocking_filter_control /
				// scaling_list_data / pps_extension.
				static bool ParsePps(const uint8 *nal, usize size, NkHevcPps &out);

				// Parse un slice_segment_header() (§7.3.6.1) — briques 2+3 : COMPLET jusqu'à
				// slice_qp_delta inclus (RPS inline via st_ref_pic_set() ou par index SPS,
				// refs long terme, temporal MVP, SAO, listes de références, merge cand, QP).
				// Refus propre sur pred_weight_table/ref_pic_lists_modification/scaling lists
				// (jamais émis par x265 par défaut). Nécessite le SPS/PPS déjà résolus via
				// slice_pic_parameter_set_id (à faire par l'appelant, comme pour H.264).
				static bool ParseSliceHeader(const uint8 *nal, usize size, const NkHevcSps &sps,
											 const NkHevcPps &pps, NkHevcSliceHeader &out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
