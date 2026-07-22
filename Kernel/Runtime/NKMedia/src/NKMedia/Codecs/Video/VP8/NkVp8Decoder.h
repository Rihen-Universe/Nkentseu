// =============================================================================
// NKMedia/Codecs/Video/VP8/NkVp8Decoder.h
// -----------------------------------------------------------------------------
// Décodeur vidéo VP8 (RFC 6386) — from-scratch, zero-STL, namespace nkentseu::media.
// ⚠️ CHANTIER EN COURS (démarré 2026-07-21) : construit brique par brique, comme le
// décodeur H.264 (voir Codecs/Video/H264/). État actuel : brique 1 (décodeur booléen,
// NkVp8BoolDecoder.h) + brique 2 (cette brique : en-tête non compressé — "frame tag" —
// et démarrage du décodeur booléen sur la 1ère partition). Reste : en-tête compressé
// complet (segmentation, filtre de boucle, quantification, probabilités de token/mv —
// §9), prédiction intra (16x16/4x4/chroma), prédiction inter (vecteurs de mouvement +
// interpolation sous-pel 6-tap), transformée (WHT DC + IDCT 4x4), filtre de boucle.
// Ne PAS prétendre décoder d'images tant que ces briques ne sont pas livrées.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8BoolDecoder.h"

namespace nkentseu {
	namespace media {

		// §9.1 : en-tête NON compressé (3 octets communs + 7 de plus si key_frame).
		// ⚠️ Convention RFC : le bit 0 du "frame tag" vaut 0 pour une image CLÉ (inversé
		// par rapport à l'intuition) — `keyFrame` ci-dessous est déjà normalisé (true =
		// image clé).
		struct NkVp8FrameTag {
				bool keyFrame = false;
				int32 version = 0;
				bool showFrame = false;
				uint32 firstPartSize = 0; // taille en octets de la 1ère partition (compressée)
				// Présents uniquement si keyFrame :
				int32 width = 0, height = 0;
				int32 horizScale = 0, vertScale = 0; // 2 bits chacun (§9.1, mise à l'échelle d'affichage)
				usize headerSize = 0;				  // taille totale de CET en-tête non compressé (3 ou 10 octets)
		};

		// Parse le frame tag (en-tête non compressé) en tête de CHAQUE frame VP8. Ne touche
		// PAS au décodeur booléen (lecture directe d'octets, little-endian sur 24/16 bits).
		// Renvoie false si trop court ou (keyFrame) si le start code 0x9d 0x01 0x2a est absent.
		inline bool NkVp8ParseFrameTag(const uint8 *data, usize size, NkVp8FrameTag &out) {
			if (size < 3)
				return false;
			const uint32 tmp = (uint32)data[0] | ((uint32)data[1] << 8) | ((uint32)data[2] << 16);
			out.keyFrame = (tmp & 0x1u) == 0; // 0 = clé (§9.1)
			out.version = (int32)((tmp >> 1) & 0x7u);
			out.showFrame = ((tmp >> 4) & 0x1u) != 0;
			out.firstPartSize = (tmp >> 5) & 0x7FFFFu; // 19 bits
			if (!out.keyFrame) {
				out.headerSize = 3;
				return true;
			}
			if (size < 10)
				return false;
			if (data[3] != 0x9D || data[4] != 0x01 || data[5] != 0x2A)
				return false; // start code absent -> pas un flux VP8 clé valide
			const uint32 w16 = (uint32)data[6] | ((uint32)data[7] << 8);
			const uint32 h16 = (uint32)data[8] | ((uint32)data[9] << 8);
			out.width = (int32)(w16 & 0x3FFFu);
			out.horizScale = (int32)(w16 >> 14);
			out.height = (int32)(h16 & 0x3FFFu);
			out.vertScale = (int32)(h16 >> 14);
			out.headerSize = 10;
			return true;
		}

		// §9.7/§16 (via entropymode.c/entropymv.c) : état PERSISTANT entre images — les
		// probabilités "courantes" utilisées pour décoder. Réinitialisé aux tables par défaut
		// sur CHAQUE image clé (`NkVp8ResetFrameContext`) ; muté EN PLACE par l'en-tête
		// compressé de chaque image (token_prob_update §13.4 +, pour les images non-clés,
		// mise à jour de ymode_prob/uv_mode_prob/mvc). ⚠️ Repli `refresh_entropy_probs==0` :
		// l'appelant doit sauvegarder une COPIE avant d'appeler le parseur et la restaurer
		// après le décodage complet de l'image si `header.refreshEntropyProbs==false` — ce
		// parseur ne fait QUE la mutation "pour cette image", pas la logique de repli
		// (pas encore de boucle de décodage complète à ce stade du chantier).
		struct NkVp8FrameContext {
				uint8 coefProbs[4][8][3][11];
				uint8 yModeProb[4];
				uint8 uvModeProb[3];
				uint8 mvContext[2][19];
		};

		void NkVp8ResetFrameContext(NkVp8FrameContext &fc);

		// §9.3 : état de segmentation (persistant, mais seulement les champs mis à jour
		// CETTE image sont modifiés — voir `updateMbSegmentationData/Map` dans le header).
		struct NkVp8Segmentation {
				bool absDelta = false; // false = SEGMENT_DELTADATA, true = SEGMENT_ABSDATA
				int32 featureData[2][4] = {}; // [0]=quant,[1]=loop filter ; par segment (4 max)
				uint8 segmentTreeProbs[3] = {255, 255, 255};
		};

		// §9.4 : deltas de filtre de boucle par référence/mode (persistants).
		struct NkVp8LoopFilterDeltas {
				int8 refLfDeltas[4] = {};	// [intra, last, golden, altref]
				int8 modeLfDeltas[4] = {}; // [B_PRED, ZEROMV, MV, SPLITMV]
		};

		// §9.2-9.11 : champs TRANSITOIRES de l'en-tête compressé d'UNE image (ne persistent
		// pas tels quels — certains, comme les deltas de quantification, sont réappliqués
		// depuis zéro à chaque image puisqu'ils sont eux-mêmes lus dans l'en-tête).
		struct NkVp8FrameHeader {
				// Clé uniquement (§9.2) :
				int32 colorSpace = 0;
				int32 clampingType = 0;
				// Segmentation (§9.3) :
				bool segmentationEnabled = false;
				bool updateMbSegmentationMap = false;
				bool updateMbSegmentationData = false;
				// Filtre de boucle (§9.4) :
				int32 filterType = 0; // 0 = normal, 1 = simple
				int32 filterLevel = 0;
				int32 sharpnessLevel = 0;
				bool lfDeltaEnabled = false;
				bool lfDeltaUpdate = false;
				// Partitions (§9.5) :
				int32 log2NbrOfDctPartitions = 0;
				// Quantification (§9.6) :
				int32 baseQIndex = 0;
				int32 y1dcDeltaQ = 0, y2dcDeltaQ = 0, y2acDeltaQ = 0, uvdcDeltaQ = 0, uvacDeltaQ = 0;
				// Rafraîchissement des tampons de référence (§9.7, non-clé uniquement) :
				bool refreshGolden = false;
				bool refreshAltRef = false;
				int32 copyBufferToGf = 0;
				int32 copyBufferToArf = 0;
				bool signBiasGolden = false;
				bool signBiasAltRef = false;
				bool refreshEntropyProbs = false;
				bool refreshLastFrame = false; // toujours vrai sur une image clé
				// Skip de coefficients (§9.10) :
				bool mbNoSkipCoeff = false;
				int32 probSkipFalse = 0;
				// Probabilités de mode/référence inter (§9.9, non-clé uniquement) :
				int32 probIntra = 0, probLast = 0, probGf = 0;
		};

		// Parse l'en-tête compressé COMPLET (§9.2-9.11, tout ce qui précède le premier
		// macrobloc) : segmentation, filtre de boucle, partitions, quantification,
		// rafraîchissement, mise à jour des probabilités de token (mute `fc.coefProbs` EN
		// PLACE via `kVp8CoefUpdateProbs`), skip de coefficients, et (images non-clés)
		// probabilités de mode/référence + mise à jour ymode/uv_mode/mv (mute `fc` EN PLACE).
		// `bd` doit déjà être initialisé sur la 1ère partition (voir NkVp8BoolDecoder::Init).
		bool NkVp8ParseCompressedHeader(NkVp8BoolDecoder &bd, bool keyFrame, NkVp8FrameHeader &out,
										 NkVp8FrameContext &fc, NkVp8Segmentation &seg,
										 NkVp8LoopFilterDeltas &lfDeltas);

		// ── Modes de prédiction (§8.2, §11) ──────────────────────────────────────────
		// ⚠️ Ces valeurs sont NORMATIVES : elles INDEXENT directement les tables de
		// probabilités extraites (`kVp8KfBModeProb[above][left]`…). L'ordre vient des enums
		// `MB_PREDICTION_MODE` / `B_PREDICTION_MODE` de la référence libvpx (`blockd.h`), et
		// il est vérifié PAR SCRIPT lors de la génération de `NkVp8Tables.inc` (assertions
		// dans `extract.py` : les arbres résolvent ces symboles, un réordonnancement amont
		// casserait la génération au lieu de passer inaperçu).
		enum : uint8 {
			// Modes de macrobloc (luma 16×16, et chroma qui n'en utilise que les 4 premiers).
			kVp8MbDcPred = 0,
			kVp8MbVPred = 1,
			kVp8MbHPred = 2,
			kVp8MbTmPred = 3,
			kVp8MbBPred = 4, // luma en 16 sous-blocs 4×4, chacun son propre mode
			// Modes de sous-bloc 4×4 (B_PRED).
			kVp8BDcPred = 0,
			kVp8BTmPred = 1,
			kVp8BVePred = 2,
			kVp8BHePred = 3,
			kVp8BLdPred = 4,
			kVp8BRdPred = 5,
			kVp8BVrPred = 6,
			kVp8BVlPred = 7,
			kVp8BHdPred = 8,
			kVp8BHuPred = 9,
		};

		// Information de mode d'UN macrobloc. Sur une image clé, tous les MB sont intra
		// (pas de `refFrame`/vecteurs de mouvement à ce stade du chantier).
		struct NkVp8MbModeInfo {
				uint8 yMode = kVp8MbDcPred;
				uint8 uvMode = kVp8MbDcPred;
				uint8 segmentId = 0;
				uint8 skipCoeff = 0;
				uint8 bModes[16] = {}; // significatif seulement si yMode == kVp8MbBPred
		};

		// §11.3 : décode les modes de TOUS les macroblocs d'une image CLÉ depuis la 1ère
		// partition (à appeler juste après `NkVp8ParseCompressedHeader`, le décodeur booléen
		// étant positionné là où celui-ci l'a laissé).
		//
		// ⚠️ Disposition de `out` : tableau AVEC BORDURE, `stride = mbCols + 1`, taille
		// `(mbRows + 1) * stride`. Le macrobloc (r,c) est à l'index `(r + 1) * stride + (c + 1)`.
		// La 1ère ligne et la 1ère colonne sont une bordure laissée à zéro : elle rend les
		// voisins HORS IMAGE naturellement égaux à DC_PRED/B_DC_PRED, exactement ce que la
		// spec demande, SANS test de bord dans le chemin chaud (même astuce que libvpx, qui
		// alloue `mip` puis décale `mi = mip + stride + 1`).
		//
		// Les modes des sous-blocs 4×4 sont décodés avec un contexte (mode du sous-bloc
		// AU-DESSUS, mode du sous-bloc À GAUCHE) qui traverse les frontières de macrobloc —
		// d'où la nécessité de garder tout le tableau, pas seulement le MB courant.
		bool NkVp8DecodeKeyFrameModes(NkVp8BoolDecoder &bd, const NkVp8FrameHeader &hdr,
									   const NkVp8Segmentation &seg, int32 mbCols, int32 mbRows,
									   NkVector<NkVp8MbModeInfo> &out);

		// ── Résidus / coefficients (§13) ─────────────────────────────────────────────
		// Contexte d'entropie d'UN macrobloc : 4 (Y) + 2 (U) + 2 (V) + 1 (Y2) = 9 entrées,
		// chacune valant 0/1 selon que le bloc 4×4 correspondant avait des coefficients.
		// Le contexte d'un bloc est la SOMME de son voisin du dessus et de gauche (0..2) →
		// il faut donc un contexte « au-dessus » PAR COLONNE de macrobloc (persistant sur
		// toute l'image) et un contexte « à gauche » remis à zéro à chaque début de ligne.
		struct NkVp8EntropyContext {
				uint8 v[9] = {};
		};

		// Coefficients quantifiés d'un macrobloc : 25 blocs 4×4 — 0-15 = Y, 16-19 = U,
		// 20-23 = V, 24 = Y2 (les DC des 16 blocs Y, transformés par WHT ; présent seulement
		// quand le MB n'est PAS en B_PRED). `eobs[b]` = position du dernier coefficient non
		// nul + 1 (0 = bloc vide).
		struct NkVp8MbCoeffs {
				int16 coeffs[25][16] = {};
				uint8 eobs[25] = {};
		};

		// §13.3 : décode les coefficients d'UN macrobloc depuis la partition de tokens.
		// Met à jour `above`/`left` EN PLACE. Renvoie `eobTotal` (somme des eobs, avec la
		// convention libvpx `-16` sur le bloc Y2) : la référence s'en sert pour forcer le
		// saut du filtre de boucle quand il vaut 0.
		int32 NkVp8DecodeMbTokens(NkVp8BoolDecoder &bd, const NkVp8FrameContext &fc, bool isBPred,
								   NkVp8EntropyContext &above, NkVp8EntropyContext &left,
								   NkVp8MbCoeffs &out);

		// §13.1 : remise à zéro du contexte d'un macrobloc SAUTÉ (`mb_skip_coeff`).
		// ⚠️ Subtilité normative : les 8 premières entrées (Y/U/V) sont toujours remises à
		// zéro, mais l'entrée Y2 (indice 8) ne l'est QUE si le macrobloc possède un bloc Y2
		// (donc PAS en B_PRED) — sinon elle garde sa valeur précédente.
		void NkVp8ResetMbTokenContext(bool isBPred, NkVp8EntropyContext &above,
									   NkVp8EntropyContext &left);

		// Statistiques de validation du décodage des résidus d'une image complète.
		struct NkVp8ResidualStats {
				int64 nonZeroCoeffs = 0;
				int64 eobTotal = 0;
				int32 skippedMbs = 0;
				int32 decodedMbs = 0;
				int32 minCoeff = 0;
				int32 maxCoeff = 0;
		};

		// Parcourt tous les macroblocs d'une image clé et décode leurs résidus depuis la
		// partition de tokens `bd` (distincte de la 1ère partition qui portait l'en-tête et
		// les modes). Les coefficients ne sont PAS conservés (le décodeur final travaillera
		// macrobloc par macrobloc) : cette fonction sert à valider que toute la partition est
		// consommée exactement, et à collecter des statistiques.
		bool NkVp8DecodeKeyFrameResiduals(NkVp8BoolDecoder &bd, const NkVp8FrameContext &fc,
										   const NkVector<NkVp8MbModeInfo> &mbInfo, int32 mbCols,
										   int32 mbRows, NkVp8ResidualStats &stats);

		// ── Reconstruction (§14 déquantification, §14.3-14.4 transformées, §12 intra) ──
		// Image reconstruite en YUV 4:2:0, planes AVEC BORDURE (1 pixel à gauche, 1 ligne
		// au-dessus, et une marge à droite). La bordure porte les valeurs normatives 127
		// (ligne du dessus) et 129 (colonne de gauche) qui servent de voisins « virtuels »
		// aux macroblocs de bord — c'est ce qui permet aux prédicteurs V/H/TM de s'appliquer
		// sans cas particulier sur les bords de l'image.
		struct NkVp8Image {
				NkVector<uint8> y, u, v;
				int32 yStride = 0, uvStride = 0;
				int32 width = 0, height = 0;	 // dimensions AFFICHÉES (peuvent ne pas être
				int32 mbCols = 0, mbRows = 0;	 // multiples de 16 ; le buffer, lui, l'est)
				int32 yOrigin = 0, uvOrigin = 0; // décalage du pixel (0,0) dans le vecteur

				uint8 *Y() { return y.Data() + yOrigin; }
				uint8 *U() { return u.Data() + uvOrigin; }
				uint8 *V() { return v.Data() + uvOrigin; }
				const uint8 *Y() const { return y.Data() + yOrigin; }
				const uint8 *U() const { return u.Data() + uvOrigin; }
				const uint8 *V() const { return v.Data() + uvOrigin; }
		};

		// §14.1 : facteurs de déquantification d'un macrobloc, dérivés de l'index de
		// quantification (plus les deltas de l'en-tête). Un facteur DC + un facteur AC par
		// type de plan.
		struct NkVp8Dequant {
				int16 y1[2] = {};	 // [0] = DC, [1] = AC
				int16 y2[2] = {};
				int16 uv[2] = {};
		};

		void NkVp8ComputeDequant(const NkVp8FrameHeader &hdr, int32 qIndex, NkVp8Dequant &out);

		// Décode une image CLÉ complète en pixels : modes déjà décodés (`mbInfo`) + résidus
		// lus depuis `tokenBd`, puis pour chaque macrobloc prédiction intra + ajout du résidu,
		// et enfin le FILTRE DE BOUCLE (§15, normal ou simple selon l'en-tête) si
		// `filterLevel > 0`. `lfDeltas` = ajustements par référence/mode lus dans l'en-tête.
		bool NkVp8ReconstructKeyFrame(NkVp8BoolDecoder &tokenBd, const NkVp8FrameContext &fc,
									   const NkVp8FrameHeader &hdr,
									   const NkVp8LoopFilterDeltas &lfDeltas,
									   const NkVector<NkVp8MbModeInfo> &mbInfo, int32 width,
									   int32 height, NkVp8Image &out);

	} // namespace media
} // namespace nkentseu
