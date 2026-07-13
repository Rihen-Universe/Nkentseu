// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Decoder.h
// -----------------------------------------------------------------------------
// DÉCODEUR H.264 (AVC) — BRIQUE 1 : découpage NAL (Annex-B) + parsing SPS/PPS.
// Le pendant de NkH264Encoder. Un décodeur H.264 baseline complet est un GROS
// chantier (slice header, CAVLC, prédictions intra 4x4/16x16 + inter/MC, IDCT,
// déblocage, gestion des références) réparti sur plusieurs sessions. Cette 1re
// brique lit déjà la STRUCTURE du bitstream : elle isole les unités NAL, retire
// l'anti-émulation (00 00 03), et décode le SPS (profil, niveau, dimensions) via
// Exp-Golomb — la fondation indispensable de tout décodeur. Zero-STL, nkentseu::media.
//
// ÉTAT : parsing SPS ✅ (dimensions/profil). Décodage des IMAGES = à venir.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// Une unité NAL isolée (pointe dans le buffer source ; ne copie pas).
		struct NkH264Nal {
				usize offset = 0; // position de l'octet d'en-tête NAL dans le buffer
				usize size = 0;	  // taille (en-tête + RBSP, hors start code)
				int32 type = 0;	  // nal_unit_type (7=SPS, 8=PPS, 5=IDR, 1=non-IDR, …)
				int32 refIdc = 0;
		};

		// Infos extraites d'un SPS.
		struct NkH264Sps {
				bool valid = false;
				int32 profileIdc = 0;
				int32 levelIdc = 0;
				int32 width = 0;  // en pixels (après cropping, hypothèse 4:2:0)
				int32 height = 0; // en pixels
				int32 numRefFrames = 0;
				// Champs nécessaires au décodage du slice header / des macroblocs.
				int32 log2MaxFrameNum = 4;	  // log2_max_frame_num_minus4 + 4
				int32 pocType = 0;			  // pic_order_cnt_type
				int32 log2MaxPocLsb = 4;	  // (poc type 0)
				int32 deltaPocAlwaysZero = 0; // (poc type 1)
				int32 frameMbsOnly = 1;
				int32 picWidthInMbs = 0;
				int32 picHeightInMapUnits = 0;
		};

		// Infos extraites d'un PPS (baseline / CAVLC).
		struct NkH264Pps {
				bool valid = false;
				int32 ppsId = 0;
				int32 spsId = 0;
				int32 entropyCodingMode = 0;  // 0 = CAVLC, 1 = CABAC
				int32 bottomFieldPocPresent = 0;
				int32 numSliceGroups = 1;
				int32 picInitQp = 26;		 // 26 + pic_init_qp_minus26
				int32 chromaQpIndexOffset = 0;
				int32 deblockingControlPresent = 0;
				int32 constrainedIntraPred = 0;
				int32 redundantPicCntPresent = 0;
		};

		// En-tête de slice (chemin I-slice IDR baseline).
		struct NkH264SliceHeader {
				bool valid = false;
				int32 firstMbInSlice = 0;
				int32 sliceType = 0; // 2 ou 7 = I
				int32 ppsId = 0;
				int32 frameNum = 0;
				int32 idrPicId = 0;
				int32 sliceQp = 26; // pic_init_qp + slice_qp_delta
				bool isIntra = false;
		};

		// Image décodée en plans YUV 4:2:0 (dimensions codées = multiples de 16 ; cropW/H = affichage).
		struct NkH264Frame {
				NkVector<nk_uint8> y, cb, cr;
				int32 lumaW = 0, lumaH = 0;
				int32 chromaW = 0, chromaH = 0;
				int32 cropW = 0, cropH = 0;
		};

		class NkH264Decoder {
			public:
				// Découpe un flux Annex-B (start codes 00 00 01 / 00 00 00 01) en unités NAL.
				static void SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkH264Nal> &out);

				// Décode UNE image IDR (I-slice) d'un flux Annex-B (SPS+PPS+slice) -> YUV 4:2:0.
				static bool DecodeIdrFrame(const uint8 *annexB, usize size, NkH264Frame &out);

				// Décode UNE image (I-slice IDR OU P-slice) baseline CAVLC. `ref` = image précédente
				// décodée (nécessaire pour une P-slice ; nullptr pour une IDR). Renvoie false si non
				// géré (CABAC, B-slice, partitions P sub-16x16, I_PCM, slices multiples…).
				static bool DecodeFrame(const uint8 *annexB, usize size, const NkH264Frame *ref, NkH264Frame &out);

				// Décode un SPS depuis une unité NAL (type 7). data/size = l'unité NAL complète
				// (en-tête inclus). Retire l'anti-émulation et lit les champs Exp-Golomb.
				static bool ParseSps(const uint8 *nal, usize size, NkH264Sps &out);

				// Décode un PPS depuis une unité NAL (type 8).
				static bool ParsePps(const uint8 *nal, usize size, NkH264Pps &out);

				// Décode l'en-tête de slice (chemin I-slice IDR). `nalType` = 5 (IDR) ou 1 (non-IDR).
				static bool ParseSliceHeader(const uint8 *nal, usize size, const NkH264Sps &sps, const NkH264Pps &pps,
											 NkH264SliceHeader &out);

				// Auto-test headless : parse SPS + PPS + slice header d'un flux baseline connu.
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
