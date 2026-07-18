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
				int32 numRefIdxL0DefaultActive = 1; // num_ref_idx_l0_default_active_minus1 + 1
				int32 numRefIdxL1DefaultActive = 1; // num_ref_idx_l1_default_active_minus1 + 1 (B)
				int32 transform8x8Mode = 0;			// transform_8x8_mode_flag (extension High)
				int32 secondChromaQpOffset = 0;		// second_chroma_qp_index_offset (defaut = chromaQpIndexOffset)
				int32 weightedPred = 0;				// weighted_pred_flag : pred_weight_table dans les slices P/SP
				int32 weightedBipredIdc = 0;		// weighted_bipred_idc : 1 = explicite en B, 2 = implicite
		};

		// En-tête de slice (chemin I-slice IDR baseline).
		struct NkH264SliceHeader {
				bool valid = false;
				int32 firstMbInSlice = 0;
				int32 sliceType = 0; // 2 ou 7 = I
				int32 ppsId = 0;
				int32 frameNum = 0;
				int32 idrPicId = 0;
				int32 sliceQp = 26;	   // pic_init_qp + slice_qp_delta
				int32 cabacInitIdc = 0; // 0..2 : variante de table d'init CABAC (P/B seulement)
				bool isIntra = false;
		};

		// Image décodée en plans YUV 4:2:0 (dimensions codées = multiples de 16 ; cropW/H = affichage).
		struct NkH264Frame {
				NkVector<nk_uint8> y, cb, cr;
				int32 lumaW = 0, lumaH = 0;
				int32 chromaW = 0, chromaH = 0;
				int32 cropW = 0, cropH = 0;
				// frame_num de l'image : identifie la reference pour ref_pic_list_modification
				// (§8.2.4.3.1 reordonne la liste par PicNum, derive de frame_num).
				int32 frameNum = 0;
				// ── Ordre d'affichage (POC, §8.2.1) ──────────────────────────────────
				// Avec des B-frames l'ordre de DECODAGE n'est PAS l'ordre d'AFFICHAGE : c'est le POC
				// qui donne l'ordre d'affichage, et il ordonne aussi les listes L0/L1 des B (§8.2.4.2.3).
				int32 poc = 0;	  // PicOrderCnt
				int32 pocLsb = 0; // pic_order_cnt_lsb   } etat necessaire a la derivation du POC
				int32 pocMsb = 0; // PicOrderCntMsb      } de l'image SUIVANTE (§8.2.1.1)
				// nal_ref_idc != 0 : l'image sert de reference et entre dans le DPB. Les B de x264 sont
				// non-references par defaut (pas de B-pyramid) : elles ne doivent PAS y entrer, sinon
				// l'etat POC "image de reference precedente" est fausse.
				bool isReference = true;
				// ── Champ de mouvement (grilles par bloc 4x4, largeur mbW*4) ──────────
				// Le Direct SPATIAL des B (§8.4.1.2.2) interroge l'image CO-LOCALISEE (= RefPicList1[0])
				// pour detecter un bloc "immobile" (reference 0 + MV quasi nulle) : chaque image doit
				// donc CONSERVER le mouvement avec lequel elle a ete decodee. mvRef : -1 = intra/inutilise.
				NkVector<nk_int32> mvL0x, mvL0y, mvL0Ref;
				NkVector<nk_int32> mvL1x, mvL1y, mvL1Ref;
				int32 mvW = 0, mvH = 0; // dimensions de ces grilles (mbW*4, mbH*4)
				// Par MACROBLOC : 1 si le MB est 16x16 ou intra. Le Direct spatial en a besoin pour la
				// GRANULARITE du test "bloc immobile" de la co-localisee : un seul test pour tout le MB
				// si elle est 16x16/intra, un test par 8x8 sinon.
				NkVector<nk_uint8> mb16x16OrIntra;
		};

		class NkH264Decoder {
			public:
				// Découpe un flux Annex-B (start codes 00 00 01 / 00 00 00 01) en unités NAL.
				static void SplitNalsAnnexB(const uint8 *data, usize size, NkVector<NkH264Nal> &out);

				// Décode UNE image IDR (I-slice) d'un flux Annex-B (SPS+PPS+slice) -> YUV 4:2:0.
				static bool DecodeIdrFrame(const uint8 *annexB, usize size, NkH264Frame &out);

				// Décode UNE image (I-slice IDR OU P-slice) baseline CAVLC. `ref` = image précédente
				// décodée (nécessaire pour une P-slice ; nullptr pour une IDR). Renvoie false si non
				// géré (CABAC, B-slice, I_PCM, slices multiples…).
				static bool DecodeFrame(const uint8 *annexB, usize size, const NkH264Frame *ref, NkH264Frame &out);

				// Variante MULTI-RÉFÉRENCE : `refs` = RefPicList0 (index 0 = référence la plus récente),
				// `numRefs` sa taille. Nécessaire pour les flux P baseline avec num_ref_idx_l0_active > 1
				// (ref_idx_l0 codé par partition). Une IDR ignore la liste.
				static bool DecodeFrame(const uint8 *annexB, usize size, const NkH264Frame *const *refs, int32 numRefs,
										NkH264Frame &out);

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
