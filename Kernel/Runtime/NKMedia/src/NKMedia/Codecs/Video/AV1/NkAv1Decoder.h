// =============================================================================
// NKMedia/Codecs/Video/AV1/NkAv1Decoder.h
// -----------------------------------------------------------------------------
// Décodeur AV1 FROM SCRATCH (spec "AV1 Bitstream & Decoding Process Specification",
// AOMedia). Écrit à la main depuis la spécification — ZÉRO code dav1d/libaom/ffmpeg
// importé (ces outils = oracle de test uniquement). AV1 descend directement de VP9
// (arbre de partition récursif, DPB/refs, transformées) mais remplace le bool coder
// par un décodeur ARITHMÉTIQUE MULTI-SYMBOLE à CDF adaptatif (§8.2).
//
// MEGA-CHANTIER, construit brique par brique (comme VP8/VP9/HEVC de ce module) :
//   Briques 1-2 : OBU parsing (§5.2-5.6) + SYMBOL DECODER CDF (§8.2).
//   Briques 3-4 : Sequence/Frame headers + partition tree + modes intra +
//     coefficients + transformées inverses + déblocage — BIT-EXACT (keyframes).
//   Briques INTER (2026-07-28) : frame header inter (§5.9), pile de MV (§7.10),
//     champ de MV temporel (§7.9), modes inter single/compound (§5.11),
//     compensation de mouvement 8-tap + compound average/distance/wedge/diffwtd +
//     inter-intra + OBMC + warp local/global (§7.11.3), var-tx inter, CDEF (§7.15),
//     DPB 8 slots + CDF persistantes -> NkAv1StreamDecoder, BIT-EXACT (voir rapport).
//   Restes (refus propre) : palette, intrabc, loop restoration, superres,
//     film grain, 10/12-bit, 4:4:4/4:2:2, monochrome.
// Zero-STL strict (NkVector / NKMemory / NkMath). Namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// --- Types d'OBU (§6.2.2, Table "obu_type"). ---
		enum NkAv1ObuType : nk_int32 {
			kAv1ObuReserved0 = 0,
			kAv1ObuSequenceHeader = 1,
			kAv1ObuTemporalDelimiter = 2,
			kAv1ObuFrameHeader = 3,
			kAv1ObuTileGroup = 4,
			kAv1ObuMetadata = 5,
			kAv1ObuFrame = 6, // frame_header + tile_group combinés
			kAv1ObuRedundantFrameHeader = 7,
			kAv1ObuTileList = 8,
			kAv1ObuPadding = 15
		};

		// --- Types de trame (frame_type, §6.8.2). ---
		enum NkAv1FrameType : nk_int32 {
			kAv1KeyFrame = 0,
			kAv1InterFrame = 1,
			kAv1IntraOnlyFrame = 2,
			kAv1SwitchFrame = 3
		};

		// En-tête d'un OBU (§5.3.2 obu_header + §5.3.1 taille LEB128).
		struct NkAv1ObuHeader {
				NkAv1ObuType type = kAv1ObuReserved0;
				bool hasSizeField = false;
				bool hasExtension = false;
				int32 temporalId = 0;
				int32 spatialId = 0;
				usize headerBytes = 0;  // taille de l'en-tête OBU (1 ou 2) + LEB128 taille
				usize payloadSize = 0;  // obu_size (octets de charge utile)
				usize payloadOffset = 0; // offset du 1er octet de charge dans le buffer OBU
		};

		// --- Sequence Header OBU (§5.5). Champs nécessaires au décodage. ---
		struct NkAv1SequenceHeader {
				int32 seqProfile = 0;               // 0..2
				bool stillPicture = false;
				bool reducedStillPictureHeader = false;

				// Timing / decoder model : on parse mais ne conserve que ce qui est utile.
				bool timingInfoPresent = false;
				bool decoderModelInfoPresent = false;
				bool initialDisplayDelayPresent = false;
				int32 operatingPointsCnt = 1;
				int32 operatingPointIdc[32] = {0};
				int32 seqLevelIdx[32] = {0};
				int32 seqTier[32] = {0};

				int32 frameWidthBitsMinus1 = 0;
				int32 frameHeightBitsMinus1 = 0;
				int32 maxFrameWidthMinus1 = 0;
				int32 maxFrameHeightMinus1 = 0;

				bool frameIdNumbersPresent = false;
				int32 deltaFrameIdLengthMinus2 = 0;
				int32 additionalFrameIdLengthMinus1 = 0;

				bool use128x128Superblock = false;
				bool enableFilterIntra = false;
				bool enableIntraEdgeFilter = false;

				bool enableInterintraCompound = false;
				bool enableMaskedCompound = false;
				bool enableWarpedMotion = false;
				bool enableDualFilter = false;
				bool enableOrderHint = false;
				bool enableJntComp = false;
				bool enableRefFrameMvs = false;
				int32 seqForceScreenContentTools = 2; // SELECT_SCREEN_CONTENT_TOOLS = 2
				int32 seqForceIntegerMv = 2;          // SELECT_INTEGER_MV = 2
				int32 orderHintBits = 0;

				bool enableSuperres = false;
				bool enableCdef = false;
				bool enableRestoration = false;

				// color_config (§5.5.2).
				int32 bitDepth = 8;
				bool mono = false;                 // mono_chrome
				bool colorDescriptionPresent = false;
				int32 colorPrimaries = 2;          // CP_UNSPECIFIED
				int32 transferCharacteristics = 2; // TC_UNSPECIFIED
				int32 matrixCoefficients = 2;      // MC_UNSPECIFIED
				bool colorRangeFull = false;
				int32 subsamplingX = 1;
				int32 subsamplingY = 1;
				int32 chromaSamplePosition = 0;
				bool separateUvDeltaQ = false;

				bool filmGrainParamsPresent = false;
				bool valid = false;
		};

		// --- Loop filter / quantization / segmentation / CDEF / LR / tiles (frame hdr). ---
		struct NkAv1TileInfo {
				int32 tileColsLog2 = 0;
				int32 tileRowsLog2 = 0;
				int32 tileCols = 1;
				int32 tileRows = 1;
				int32 miColStarts[65] = {0}; // en unités MI (4px) — début de chaque colonne de tile
				int32 miRowStarts[65] = {0};
				int32 contextUpdateTileId = 0;
				int32 tileSizeBytes = 4; // TileSizeBytes = tile_size_bytes_minus_1 + 1
				bool uniformTileSpacing = true;
		};

		struct NkAv1QuantParams {
				int32 baseQIdx = 0;
				int32 deltaQYDc = 0;
				int32 deltaQUDc = 0;
				int32 deltaQUAc = 0;
				int32 deltaQVDc = 0;
				int32 deltaQVAc = 0;
				bool usingQmatrix = false;
				int32 qmY = 0, qmU = 0, qmV = 0;
		};

		struct NkAv1SegmentationParams {
				bool enabled = false;
				bool updateMap = false;
				bool temporalUpdate = false;
				bool updateData = false;
				bool featureEnabled[8][8] = {{false}};
				int32 featureData[8][8] = {{0}};
				int32 lastActiveSegId = 0;
				bool preSkipSegidPresent = false;
		};

		struct NkAv1LoopFilterParams {
				int32 level[4] = {0, 0, 0, 0}; // [0]=Y vert, [1]=Y horz, [2]=U, [3]=V
				int32 sharpness = 0;
				bool deltaEnabled = false;
				int32 refDeltas[8] = {1, 0, 0, 0, -1, 0, -1, -1};
				int32 modeDeltas[2] = {0, 0};
		};

		struct NkAv1CdefParams {
				int32 dampingMinus3 = 0;
				int32 bits = 0; // cdef_bits
				int32 yPriStrength[8] = {0};
				int32 ySecStrength[8] = {0};
				int32 uvPriStrength[8] = {0};
				int32 uvSecStrength[8] = {0};
		};

		struct NkAv1LrParams {
				int32 frameRestorationType[3] = {0, 0, 0}; // RESTORE_NONE=0
				int32 loopRestorationSize[3] = {0, 0, 0};
				bool usesLr = false;
		};

		// --- Frame header (§5.9) — keyframes intra + trames INTER (§5.9 complet). ---
		struct NkAv1FrameHeader {
				bool showExistingFrame = false;
				int32 frameToShow = 0;
				bool frameIsIntra = true;

				int32 frameType = kAv1KeyFrame;
				bool showFrame = true;
				bool showableFrame = false;
				bool errorResilientMode = false;
				bool disableCdfUpdate = false;
				int32 allowScreenContentTools = 0;
				int32 forceIntegerMv = 0;
				int32 currentFrameId = 0;
				bool frameSizeOverride = false;
				int32 orderHint = 0;
				int32 primaryRefFrame = 7; // PRIMARY_REF_NONE = 7

				bool disableFrameEndUpdateCdf = false;
				bool allowHighPrecisionMv = false;
				bool useRefFrameMvs = false;
				bool allowIntrabc = false;
				bool reducedTxSet = false;

				int32 frameWidth = 0;
				int32 frameHeight = 0;
				int32 upscaledWidth = 0;
				int32 renderWidth = 0;
				int32 renderHeight = 0;
				int32 superresDenom = 8; // SUPERRES_NUM = 8 => pas d'upscale
				int32 miCols = 0;
				int32 miRows = 0;

				int32 interpolationFilter = 0;
				bool isMotionModeSwitchable = false;

				int32 refreshFrameFlags = 0;
				int32 refFrameIdx[7] = {-1, -1, -1, -1, -1, -1, -1};

				// --- Champs inter (§5.9 chemins non-keyframe). Indices de trame de
				// référence : INTRA_FRAME=0, LAST=1..ALTREF=7 (NONE=-1). ---
				int32 orderHints[8] = {0};       // OrderHints[refFrame] (1..7)
				bool refFrameSignBias[8] = {false};
				int32 gmType[8] = {0};           // GmType[ref] : 0=IDENTITY 1=TRANSLATION 2=ROTZOOM 3=AFFINE
				int32 gmParams[8][6] = {{0}};
				int32 skipModeFrame[2] = {0, 0};

				bool codedLossless = false;
				bool allLossless = false;
				bool losslessArray[8] = {false};

				int32 txMode = 0; // 0=ONLY_4X4, 1=LARGEST, 2=SELECT (TX_MODE_SELECT)
				int32 referenceSelect = 0;
				bool skipModePresent = false;
				bool reducedTxSetUsed = false;
				bool allowWarpedMotion = false;

				bool deltaQPresent = false;
				int32 deltaQRes = 0;
				bool deltaLfPresent = false;
				int32 deltaLfRes = 0;
				bool deltaLfMulti = false;

				NkAv1QuantParams quant;
				NkAv1SegmentationParams seg;
				NkAv1LoopFilterParams lf;
				NkAv1CdefParams cdef;
				NkAv1LrParams lr;
				NkAv1TileInfo tiles;

				// Offset (octets, depuis le début de l'OBU de charge de trame) où commence
				// la donnée des tiles (après le frame header). Rempli pour OBU_FRAME.
				usize headerEndByte = 0;
				bool valid = false;
		};

		// Image décodée (I420 8-bit ; strides = largeurs de plan).
		struct NkAv1Image {
				NkVector<nk_uint8> y, u, v;
				int32 width = 0, height = 0;
				int32 yStride = 0, uvStride = 0;
				int32 uvWidth = 0, uvHeight = 0;
				int32 bitDepth = 8;
		};

		// Statistiques de parse (diagnostic — briques structurelles).
		struct NkAv1ParseStats {
				int32 obuCount = 0;
				int32 tdCount = 0;
				int32 seqHdrCount = 0;
				int32 frameHdrCount = 0;
				int32 tileGroupCount = 0;
				int32 frameObuCount = 0;
				int32 metadataCount = 0;
				int32 paddingCount = 0;
				int32 tilesParsed = 0;
				int32 sbCount = 0;      // superblocs parcourus
				int32 partLeaves = 0;   // feuilles de partition
				usize bytesConsumed = 0;
				bool overran = false;
		};

		// Emplacement (offset + taille) de la donnée compressée d'une tile dans la
		// charge du tile group (entrée du décodeur de symboles).
		struct NkAv1TileEntry {
				usize offset = 0; // offset (octets) dans la charge du tile group
				usize size = 0;   // taille (octets) de la donnée de tile
				int32 tileNum = 0;
				int32 tileRow = 0;
				int32 tileCol = 0;
		};

		struct NkAv1Decoder {
			public:
				// Parse un en-tête OBU (§5.3). `data`/`size` pointent sur le début d'un OBU.
				// Renvoie false si le buffer est trop court / réservé invalide. `out.payloadOffset`
				// et `out.payloadSize` délimitent la charge utile ; le prochain OBU commence à
				// `out.payloadOffset + out.payloadSize` (si hasSizeField ; sinon jusqu'à la fin).
				static bool ParseObuHeader(const uint8 *data, usize size, NkAv1ObuHeader &out);

				// Parcourt TOUS les OBU d'une "temporal unit" (une trame IVF), remplit les
				// statistiques et parse structurellement seq/frame headers. `seqOut` reçoit le
				// dernier sequence header vu (persistant pour le flux). Renvoie false si un OBU
				// déborde. Ne reconstruit AUCUN pixel (brique structurelle).
				static bool ParseTemporalUnit(const uint8 *data, usize size, NkAv1SequenceHeader &seqOut,
											  NkAv1FrameHeader &frameOut, NkAv1ParseStats &stats);

				// Parse un Sequence Header OBU (§5.5). `data`/`size` = charge utile de l'OBU.
				static bool ParseSequenceHeader(const uint8 *data, usize size, NkAv1SequenceHeader &out);

				// Parse le frame header (§5.9) contenu dans un OBU_FRAME ou OBU_FRAME_HEADER.
				// `seq` = sequence header courant. Remplit `out` (dont headerEndByte = offset,
				// en octets alignés, où débute la donnée de tiles pour un OBU_FRAME).
				static bool ParseFrameHeader(const uint8 *data, usize size, const NkAv1SequenceHeader &seq,
											 NkAv1FrameHeader &out);

				// Parse un tile_group_obu (§5.11.1) : lit tg_start/tg_end + les champs de
				// taille de tile (le(TileSizeBytes)), et remplit `tilesOut` avec l'offset et
				// la taille de CHAQUE tile dans la charge. `data`/`size` = charge du tile
				// group (pour un OBU_FRAME : payload + frameHeader.headerEndByte, taille
				// restante). Renvoie false si les tailles ne partitionnent pas EXACTEMENT la
				// charge. C'est la porte d'entrée du décodeur de symboles (une init_symbol
				// par tile). Vérifiable structurellement.
				static bool ParseTileGroup(const uint8 *data, usize size, const NkAv1FrameHeader &fh,
										   NkVector<NkAv1TileEntry> &tilesOut, int32 &tgStart, int32 &tgEnd);

				// Décode une trame CLÉ intra en pixels (foundations — voir .cpp pour l'état réel
				// de bit-exactitude). `data`/`size` = OBU_FRAME (ou séquence complète d'OBU d'une
				// TU). Renvoie false si non supporté / erreur. Brique en cours.
				static bool DecodeKeyFrame(const uint8 *data, usize size, NkAv1Image &out,
										   NkAv1ParseStats *statsOut = nullptr);

				static bool SelfTest();
		};

		// =====================================================================
		// Décodeur de FLUX AV1 avec état persistant (DPB 8 slots, CDF sauvegardées,
		// champ de MV, order hints) — nécessaire pour les trames INTER. Une instance
		// par flux ; lui passer chaque temporal unit (une trame IVF) dans l'ordre.
		// Les images sorties sont en ORDRE D'AFFICHAGE (show_frame /
		// show_existing_frame gérés). From-scratch depuis la spec, comme le reste.
		// =====================================================================
		class NkAv1StreamDecoder {
			public:
				NkAv1StreamDecoder();
				~NkAv1StreamDecoder();
				NkAv1StreamDecoder(const NkAv1StreamDecoder &) = delete;
				NkAv1StreamDecoder &operator=(const NkAv1StreamDecoder &) = delete;

				// Décode une temporal unit. Ajoute 0..n images (ordre d'affichage) à
				// outFrames. Renvoie false sur erreur ou feature non supportée (la
				// raison est disponible via LastError()).
				bool DecodeTemporalUnit(const uint8 *data, usize size, NkVector<NkAv1Image> &outFrames);

				// Message court (statique) décrivant la dernière erreur / le refus.
				const char *LastError() const;

			private:
				struct Impl;
				Impl *mImpl = nullptr;
		};

	} // namespace media
} // namespace nkentseu
