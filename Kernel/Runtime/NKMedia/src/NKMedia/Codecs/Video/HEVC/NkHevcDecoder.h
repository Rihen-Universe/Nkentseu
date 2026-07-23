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
				bool tilesEnabled = false;
				bool entropyCodingSyncEnabled = false;
				int32 numTileColumnsMinus1 = 0, numTileRowsMinus1 = 0;
				bool loopFilterAcrossTiles = true;
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

				// Parse un SPS (§7.3.2.2) : s'arrête après bit_depth/POC (dimensions,
				// profil, niveau, chroma, profondeur de bits) — ne consomme PAS
				// st_ref_pic_set()/scaling_list_data()/vui_parameters() (pas nécessaires
				// à la brique 1, et chacun de ces syntax elements est un chantier propre).
				static bool ParseSps(const uint8 *nal, usize size, NkHevcSps &out);

				// Parse un PPS (§7.3.2.3) : sous-ensemble structurel (tuiles, QP init,
				// flags de contrôle) — s'arrête avant deblocking_filter_control /
				// scaling_list_data / pps_extension.
				static bool ParsePps(const uint8 *nal, usize size, NkHevcPps &out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
