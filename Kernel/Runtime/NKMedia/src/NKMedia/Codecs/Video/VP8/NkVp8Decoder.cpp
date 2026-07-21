// =============================================================================
// NKMedia/Codecs/Video/VP8/NkVp8Decoder.cpp
// -----------------------------------------------------------------------------
// Brique 3 : en-tête compressé complet (RFC 6386 §9.2-9.11). Ordre des champs et
// logique VÉRIFIÉS contre le décodeur de référence libvpx (vp8/decoder/decodeframe.c
// `vp8_decode_frame` + vp8/decoder/decodemv.c `mb_mode_mv_init`), pas seulement la
// prose du RFC — la prose seule laisse trop de place à l'erreur d'ordre (ex. le
// nombre de partitions §9.5 est lu APRÈS les deltas de filtre de boucle et AVANT la
// quantification, ce qui n'est pas évident à la simple lecture du RFC).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKMedia/Codecs/Video/VP8/NkVp8Decoder.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8Tables.inc"
#include <cstring>

namespace nkentseu {
	namespace media {

		void NkVp8ResetFrameContext(NkVp8FrameContext &fc) {
			memcpy(fc.coefProbs, kVp8DefaultCoefProbs, sizeof(fc.coefProbs));
			memcpy(fc.yModeProb, kVp8YModeProb, sizeof(fc.yModeProb));
			memcpy(fc.uvModeProb, kVp8UvModeProb, sizeof(fc.uvModeProb));
			memcpy(fc.mvContext, kVp8DefaultMvContext, sizeof(fc.mvContext));
		}

		bool NkVp8ParseCompressedHeader(NkVp8BoolDecoder &bd, bool keyFrame, NkVp8FrameHeader &out,
										 NkVp8FrameContext &fc, NkVp8Segmentation &seg,
										 NkVp8LoopFilterDeltas &lfDeltas) {
			out = NkVp8FrameHeader();

			// §9.2 : sur une image clé, TOUT l'état persistant repart des valeurs par défaut
			// (miroir de init_frame() côté libvpx — mvc/ymode/uvmode/coef_probs ET
			// segmentation/deltas de filtre de boucle).
			if (keyFrame) {
				NkVp8ResetFrameContext(fc);
				seg = NkVp8Segmentation();
				lfDeltas = NkVp8LoopFilterDeltas();
				out.colorSpace = bd.GetFlag();
				out.clampingType = bd.GetFlag();
			}

			// §9.3 : segmentation.
			out.segmentationEnabled = bd.GetFlag();
			if (out.segmentationEnabled) {
				out.updateMbSegmentationMap = bd.GetFlag();
				out.updateMbSegmentationData = bd.GetFlag();
				if (out.updateMbSegmentationData) {
					seg.absDelta = bd.GetFlag();
					const int32 featureBits[2] = {7, 6}; // quant, loop filter (§9.3, mb_feature_data_bits)
					for (int32 i = 0; i < 2; ++i)
						for (int32 j = 0; j < 4; ++j)
							seg.featureData[i][j] = bd.GetOptionalSignedLiteral(featureBits[i]);
				}
				if (out.updateMbSegmentationMap) {
					for (int32 i = 0; i < 3; ++i)
						seg.segmentTreeProbs[i] = bd.GetFlag() ? (uint8)bd.GetLiteral(8) : 255;
				}
			}

			// §9.4 : type/niveau/netteté du filtre de boucle + ajustements par mode/référence.
			out.filterType = bd.GetFlag();
			out.filterLevel = (int32)bd.GetLiteral(6);
			out.sharpnessLevel = (int32)bd.GetLiteral(3);
			out.lfDeltaEnabled = bd.GetFlag();
			if (out.lfDeltaEnabled) {
				out.lfDeltaUpdate = bd.GetFlag();
				if (out.lfDeltaUpdate) {
					for (int32 i = 0; i < 4; ++i) {
						if (bd.GetFlag()) {
							const int32 mag = (int32)bd.GetLiteral(6);
							lfDeltas.refLfDeltas[i] = (int8)(bd.GetFlag() ? -mag : mag);
						}
					}
					for (int32 i = 0; i < 4; ++i) {
						if (bd.GetFlag()) {
							const int32 mag = (int32)bd.GetLiteral(6);
							lfDeltas.modeLfDeltas[i] = (int8)(bd.GetFlag() ? -mag : mag);
						}
					}
				}
			}

			// §9.5 : nombre de partitions de coefficients (2^n, n in [0,3]) — lu ICI, entre le
			// filtre de boucle et la quantification (ordre vérifié contre libvpx, PAS évident
			// depuis la seule prose RFC).
			out.log2NbrOfDctPartitions = (int32)bd.GetLiteral(2);

			// §9.6 : quantification — 1 index de base + 5 deltas optionnels.
			out.baseQIndex = (int32)bd.GetLiteral(7);
			out.y1dcDeltaQ = bd.GetOptionalSignedLiteral(4);
			out.y2dcDeltaQ = bd.GetOptionalSignedLiteral(4);
			out.y2acDeltaQ = bd.GetOptionalSignedLiteral(4);
			out.uvdcDeltaQ = bd.GetOptionalSignedLiteral(4);
			out.uvacDeltaQ = bd.GetOptionalSignedLiteral(4);

			// §9.7 : rafraîchissement des tampons de référence (images NON clés seulement —
			// une image clé rafraîchit implicitement TOUT, rien à lire ici).
			if (!keyFrame) {
				out.refreshGolden = bd.GetFlag();
				out.refreshAltRef = bd.GetFlag();
				if (!out.refreshGolden)
					out.copyBufferToGf = (int32)bd.GetLiteral(2);
				if (!out.refreshAltRef)
					out.copyBufferToArf = (int32)bd.GetLiteral(2);
				out.signBiasGolden = bd.GetFlag();
				out.signBiasAltRef = bd.GetFlag();
			} else {
				out.refreshGolden = true;
				out.refreshAltRef = true;
			}

			out.refreshEntropyProbs = bd.GetFlag();
			out.refreshLastFrame = keyFrame || bd.GetFlag();

			// §13.4 : mise à jour des probabilités de token — 4×8×3×11 = 1056 décisions
			// "faut-il remplacer cette probabilité ?", chacune conditionnée par
			// `kVp8CoefUpdateProbs` (PAS 128 uniforme : certaines entrées sont mises à jour
			// très rarement (255 = quasi jamais), d'autres beaucoup plus souvent).
			for (int32 i = 0; i < 4; ++i)
				for (int32 j = 0; j < 8; ++j)
					for (int32 k = 0; k < 3; ++k)
						for (int32 l = 0; l < 11; ++l)
							if (bd.GetBool(kVp8CoefUpdateProbs[i][j][k][l]))
								fc.coefProbs[i][j][k][l] = (uint8)bd.GetLiteral(8);

			// §9.10 : mb_no_skip_coeff (mb_mode_mv_init côté libvpx — lu ICI, entre les
			// probabilités de token et le décodage mode/mv, PAS dans le corps par-MB).
			out.mbNoSkipCoeff = bd.GetFlag();
			if (out.mbNoSkipCoeff)
				out.probSkipFalse = (int32)bd.GetLiteral(8);

			// §9.9/§16-17 : probabilités de mode inter + mise à jour ymode/uv_mode/mv —
			// UNIQUEMENT sur les images non-clés (une image clé n'a que des macroblocs intra,
			// dont les modes utilisent des tables FIXES contexte-dépendantes — kVp8KfYModeProb/
			// kVp8KfUvModeProb/kVp8KfBModeProb — jamais mises à jour par l'en-tête).
			if (!keyFrame) {
				out.probIntra = (int32)bd.GetLiteral(8);
				out.probLast = (int32)bd.GetLiteral(8);
				out.probGf = (int32)bd.GetLiteral(8);

				if (bd.GetFlag())
					for (int32 i = 0; i < 4; ++i)
						fc.yModeProb[i] = (uint8)bd.GetLiteral(8);

				if (bd.GetFlag())
					for (int32 i = 0; i < 3; ++i)
						fc.uvModeProb[i] = (uint8)bd.GetLiteral(8);

				// §17.2 : mise à jour des probabilités de vecteur de mouvement (2 composantes
				// [row,col] × 19 probabilités/composante, chacune conditionnée par
				// `kVp8MvUpdateProbs` ; la nouvelle valeur (si mise à jour) est un littéral 7
				// bits ré-échelonné ×2 (jamais 0 -> 1, cf. libvpx `x ? x<<1 : 1`).
				for (int32 comp = 0; comp < 2; ++comp) {
					for (int32 i = 0; i < 19; ++i) {
						if (bd.GetBool(kVp8MvUpdateProbs[comp][i])) {
							const int32 x = (int32)bd.GetLiteral(7);
							fc.mvContext[comp][i] = (uint8)(x ? (x << 1) : 1);
						}
					}
				}
			}

			return true;
		}

	} // namespace media
} // namespace nkentseu
