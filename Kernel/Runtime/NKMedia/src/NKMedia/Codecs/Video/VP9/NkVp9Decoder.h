// =============================================================================
// NKMedia/Codecs/Video/VP9/NkVp9Decoder.h
// -----------------------------------------------------------------------------
// Décodeur VP9 from-scratch (spec "VP9 Bitstream & Decoding Process Specification"
// v0.7, réécrit à la sauce Nkentseu — zéro code libvpx importé). CHANTIER EN COURS,
// brique par brique comme VP8 (NkVp8Decoder) :
//   Brique 1 (ICI) : SUPERFRAMES (index en fin de charge utile, marqueur 0xC0|..)
//     + EN-TÊTE DE TRAME NON COMPRESSÉ (§6.2 : profil, sync code, color config,
//     tailles, refs, filtre de boucle, quantification, segmentation, tiles).
//   Briques suivantes : bool decoder compressé (identique VP8), probas par défaut,
//     modes intra, résiduels/transformées (4x4→32x32, DCT/ADST), reconstruction
//     image clé, inter (MC 1/8 pel, compound), loop filter, branchement reader.
// Zero-STL, namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// --- Superframe VP9 : jusqu'à 8 trames concaténées + index final. ---
		// Le DERNIER octet 0b110xxxxx = marqueur : bits 0-2 = nb trames - 1,
		// bits 3-4 = taille des entrées - 1. L'index complet (même octet au début
		// ET à la fin) précède ce marqueur. Une charge sans marqueur = 1 trame.
		struct NkVp9Superframe {
				static constexpr int32 kMaxFrames = 8;
				int32 count = 0; // nombre de trames (>= 1)
				usize offsets[kMaxFrames] = {0};
				usize sizes[kMaxFrames] = {0};
		};

		// Types de trame (§7.2).
		enum NkVp9FrameType : nk_int32 {
			kVp9KeyFrame = 0,
			kVp9NonKeyFrame = 1
		};

		// Filtres d'interpolation (§6.2.5).
		enum NkVp9InterpFilter : nk_int32 {
			kVp9Eighttap = 0,
			kVp9EighttapSmooth = 1,
			kVp9EighttapSharp = 2,
			kVp9Bilinear = 3,
			kVp9Switchable = 4
		};

		// En-tête de trame NON COMPRESSÉ (§6.2 uncompressed_header). Les champs non
		// présents dans le flux gardent leur valeur par défaut.
		struct NkVp9FrameHeader {
				// Trame "show existing" : réaffiche une trame de référence, aucune donnée.
				bool showExistingFrame = false;
				int32 frameToShowMapIdx = 0;

				int32 profile = 0; // 0-3
				int32 frameType = kVp9KeyFrame;
				bool showFrame = false;
				bool errorResilient = false;
				bool intraOnly = false; // non-key sans référence inter (fond d'écran/refresh)

				// Color config (clé / intra-only).
				int32 bitDepth = 8;
				int32 colorSpace = 0; // 0=UNKNOWN,1=BT601,2=BT709,...,7=RGB
				bool colorRangeFull = false;
				int32 subsamplingX = 1;
				int32 subsamplingY = 1;

				// Dimensions.
				int32 width = 0;
				int32 height = 0;
				int32 renderWidth = 0;
				int32 renderHeight = 0;

				// Références (non-key).
				int32 resetFrameContext = 0;
				uint32 refreshFrameFlags = 0; // 8 bits — slots de référence rafraîchis
				int32 refFrameIdx[3] = {0, 0, 0};	  // LAST/GOLDEN/ALTREF -> slot 0-7
				bool refFrameSignBias[3] = {false, false, false};
				bool allowHighPrecisionMv = false;
				int32 interpFilter = kVp9Switchable;

				// Contexte/parallélisme.
				bool refreshFrameContext = false;
				bool frameParallelDecoding = false;
				int32 frameContextIdx = 0;

				// Filtre de boucle (§6.2.8).
				int32 lfLevel = 0;
				int32 lfSharpness = 0;
				bool lfDeltaEnabled = false;
				int32 lfRefDeltas[4] = {1, 0, -1, -1};
				int32 lfModeDeltas[2] = {0, 0};

				// Quantification (§6.2.9).
				int32 baseQIdx = 0;
				int32 deltaQYDc = 0;
				int32 deltaQUvDc = 0;
				int32 deltaQUvAc = 0;
				bool lossless = false;

				// Segmentation (§6.2.10).
				bool segEnabled = false;
				bool segUpdateMap = false;
				bool segTemporalUpdate = false;
				uint8 segTreeProbs[7] = {255, 255, 255, 255, 255, 255, 255};
				uint8 segPredProbs[3] = {255, 255, 255};
				bool segAbsDelta = false;
				bool segFeatureEnabled[8][4] = {{false}};
				int32 segFeatureData[8][4] = {{0}};

				// Tiles (§6.2.11).
				int32 tileColsLog2 = 0;
				int32 tileRowsLog2 = 0;

				// Taille de l'en-tête compressé (octets) qui suit immédiatement.
				int32 headerSizeBytes = 0;
				// Offset (en octets, depuis le début de la trame) de l'en-tête compressé.
				int32 uncompressedBytes = 0;
		};

		// Modes de référence de trame (§7.4.12).
		enum NkVp9ReferenceMode : nk_int32 {
			kVp9SingleReference = 0,
			kVp9CompoundReference = 1,
			kVp9ReferenceModeSelect = 2
		};

		// Composante de vecteur de mouvement (nmv_component).
		struct NkVp9NmvComponent {
				uint8 sign = 128;
				uint8 classes[10] = {0};
				uint8 class0[1] = {0};
				uint8 bits[10] = {0};
				uint8 class0Fr[2][3] = {{0}};
				uint8 fr[3] = {0};
				uint8 class0Hp = 160;
				uint8 hp = 128;
		};

		// CONTEXTE D'ENTROPIE DE TRAME (FRAME_CONTEXT) : toutes les probabilités
		// adaptées, initialisées aux valeurs par défaut normatives (NkVp9Tables.inc)
		// puis mises à jour par l'en-tête compressé (diff update subexp).
		struct NkVp9FrameContext {
				uint8 yModeProb[4][9];
				uint8 uvModeProb[10][9];
				uint8 partitionProb[16][3];
				uint8 coefProbs[4][2][2][6][6][3]; // [txSize][plane][ref][bande][ctx][nœud]
				uint8 switchableInterpProb[4][2];
				uint8 interModeProbs[7][3];
				uint8 intraInterProb[4];
				uint8 compInterProb[5];
				uint8 singleRefProb[5][2];
				uint8 compRefProb[5];
				uint8 txProbs32[2][3];
				uint8 txProbs16[2][2];
				uint8 txProbs8[2][1];
				uint8 skipProbs[3];
				uint8 nmvJoints[3];
				NkVp9NmvComponent nmvComps[2];
		};

		// Résultat du parse de l'en-tête compressé.
		struct NkVp9CompressedHeader {
				int32 txMode = 0;		 // 0..4 (4 = TX_MODE_SELECT)
				int32 referenceMode = 0; // NkVp9ReferenceMode
		};

		struct NkVp9Decoder {
			public:
				// Découpe une charge utile (trame WebM/IVF) en trames VP9 individuelles
				// via l'index de superframe (§Annexe B). Sans index : 1 trame = tout.
				static bool ParseSuperframe(const uint8 *data, usize size, NkVp9Superframe &out);

				// Parse l'en-tête NON COMPRESSÉ d'une trame (§6.2). Renvoie false si le
				// flux est invalide (marqueur/sync code/profil non gérés).
				// `refW/refH` : dimensions à utiliser quand la taille est HÉRITÉE d'une
				// trame de référence (flag par réf) — le décodeur complet passera celles
				// du slot référencé ; 0 = inconnu → width/height = sentinelle négative et
				// tile_info/headerSize ne sont pas fiables.
				static bool ParseUncompressedHeader(const uint8 *data, usize size, NkVp9FrameHeader &out,
													int32 refW = 0, int32 refH = 0);

				// Initialise le contexte aux probabilités par défaut normatives.
				static void InitDefaultFrameContext(NkVp9FrameContext &fc);

				// Parse l'EN-TÊTE COMPRESSÉ (§6.3, read_compressed_header) : bool decoder
				// (identique VP8 — ⚠ mais VP9 lit UN BIT MARQUEUR à l'init, qui doit
				// valoir 0) + mises à jour de probabilités (diff update : flag proba 252
				// puis delta subexp remappé via inv_map_table). `data` pointe sur les
				// `size` octets de l'en-tête compressé (après l'en-tête non compressé).
				// Renvoie false si marqueur invalide ou débordement du bool decoder.
				static bool ParseCompressedHeader(const uint8 *data, usize size, const NkVp9FrameHeader &hdr,
												  NkVp9FrameContext &fc, NkVp9CompressedHeader &out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
