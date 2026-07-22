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

		namespace {

			// §11.3 : mode du sous-bloc 4×4 situé AU-DESSUS du sous-bloc `b` (0..15, ordre
			// raster dans le MB). Si `b` est sur la ligne du haut du MB, le voisin est dans le
			// MB au-dessus (`mi - stride`) — et si CE MB n'est pas en B_PRED, son mode 16×16
			// est PROJETÉ sur l'équivalent 4×4 (DC→B_DC, V→B_VE, H→B_HE, TM→B_TM). C'est ce
			// mapping qui permet à un MB B_PRED de prédire son contexte depuis un voisin
			// 16×16, et il est asymétrique (V_PRED→B_VE_PRED, pas B_VR_PRED) : le recopier
			// tel quel depuis la référence, ne pas le "deviner" par symétrie de nom.
			inline int32 Vp8ProjectMbModeTo4x4(uint8 mbMode) {
				switch (mbMode) {
					case kVp8MbDcPred: return kVp8BDcPred;
					case kVp8MbVPred: return kVp8BVePred;
					case kVp8MbHPred: return kVp8BHePred;
					case kVp8MbTmPred: return kVp8BTmPred;
					default: return kVp8BDcPred; // inclut B_PRED (traité par l'appelant) et l'inter
				}
			}

			inline int32 Vp8AboveBlockMode(const NkVp8MbModeInfo *mi, int32 b, int32 stride) {
				if ((b >> 2) == 0) { // ligne du haut du MB -> voisin dans le MB au-dessus
					const NkVp8MbModeInfo *up = mi - stride;
					if (up->yMode == kVp8MbBPred)
						return up->bModes[b + 12]; // même colonne, dernière ligne du MB au-dessus
					return Vp8ProjectMbModeTo4x4(up->yMode);
				}
				return mi->bModes[b - 4];
			}

			inline int32 Vp8LeftBlockMode(const NkVp8MbModeInfo *mi, int32 b) {
				if ((b & 3) == 0) { // colonne de gauche du MB -> voisin dans le MB à gauche
					const NkVp8MbModeInfo *lf = mi - 1;
					if (lf->yMode == kVp8MbBPred)
						return lf->bModes[b + 3]; // même ligne, dernière colonne du MB à gauche
					return Vp8ProjectMbModeTo4x4(lf->yMode);
				}
				return mi->bModes[b - 1];
			}

		} // namespace

		bool NkVp8DecodeKeyFrameModes(NkVp8BoolDecoder &bd, const NkVp8FrameHeader &hdr,
									   const NkVp8Segmentation &seg, int32 mbCols, int32 mbRows,
									   NkVector<NkVp8MbModeInfo> &out) {
			if (mbCols <= 0 || mbRows <= 0)
				return false;

			// Bordure d'une colonne à gauche + une ligne au-dessus (voir la note du header) :
			// laissée aux valeurs par défaut (DC_PRED / B_DC_PRED), ce qui donne exactement le
			// contexte attendu pour les macroblocs de la 1ère ligne / 1ère colonne.
			const int32 stride = mbCols + 1;
			out.Clear();
			out.Resize((uint64)((mbRows + 1) * stride));
			for (uint64 i = 0; i < out.Size(); ++i)
				out[i] = NkVp8MbModeInfo();

			for (int32 r = 0; r < mbRows; ++r) {
				for (int32 c = 0; c < mbCols; ++c) {
					NkVp8MbModeInfo *mi = out.Data() + (int64)(r + 1) * stride + (c + 1);

					// §9.3/§10 : identifiant de segment, seulement si la CARTE de segmentation
					// est mise à jour cette image (arbre binaire à 2 niveaux, 3 probabilités).
					if (hdr.segmentationEnabled && hdr.updateMbSegmentationMap) {
						if (bd.GetBool(seg.segmentTreeProbs[0]))
							mi->segmentId = (uint8)(2 + bd.GetBool(seg.segmentTreeProbs[2]));
						else
							mi->segmentId = (uint8)bd.GetBool(seg.segmentTreeProbs[1]);
					}

					// §9.10 : drapeau "ce MB n'a aucun coefficient" (si la fonctionnalité est
					// active pour l'image).
					mi->skipCoeff = hdr.mbNoSkipCoeff ? (uint8)bd.GetBool(hdr.probSkipFalse) : (uint8)0;

					// §11.2 : mode luma. ⚠️ Sur une image CLÉ on utilise l'arbre ET les
					// probabilités "kf" (kVp8KfYModeTree/kVp8KfYModeProb), qui sont DIFFÉRENTS
					// de ceux des images inter (kVp8YModeTree/fc.yModeProb) — l'arbre kf place
					// B_PRED en première feuille, l'autre le place en dernier. Confondre les
					// deux décode des modes plausibles mais faux, sans erreur visible ici.
					mi->yMode = (uint8)bd.GetTree(kVp8KfYModeTree, kVp8KfYModeProb);

					if (mi->yMode == kVp8MbBPred) {
						// §11.3 : 16 modes de sous-bloc, chacun conditionné par le contexte
						// (au-dessus, à gauche) — d'où une table de probabilités 10×10×9.
						for (int32 b = 0; b < 16; ++b) {
							const int32 above = Vp8AboveBlockMode(mi, b, stride);
							const int32 left = Vp8LeftBlockMode(mi, b);
							mi->bModes[b] = (uint8)bd.GetTree(kVp8BModeTree, kVp8KfBModeProb[above][left]);
						}
					}

					// §11.4 : mode chroma (mêmes 4 modes que le luma 16×16, arbre dédié).
					mi->uvMode = (uint8)bd.GetTree(kVp8UvModeTree, kVp8KfUvModeProb);
				}
			}
			return true;
		}

		namespace {

			// §13.2 : décode les coefficients d'UN bloc 4×4. Renvoie la position du dernier
			// coefficient non nul + 1 (0 si le bloc est entièrement vide).
			//
			// `prob` = `fc.coefProbs[typeDeBloc]`, indexé ensuite par [bande][contexte][nœud].
			// `ctx` = 0..2 (somme des drapeaux « non vide » du voisin du dessus et de gauche).
			// `n` = indice du premier coefficient à lire (1 si le DC vient du bloc Y2).
			//
			// ⚠️ Le contexte utilisé pour le coefficient SUIVANT dépend de la VALEUR qu'on
			// vient de lire (0 → contexte 0, 1 → contexte 1, >1 → contexte 2) : c'est ce
			// ré-adressage de `p` à chaque itération qui porte l'essentiel de la compression.
			int32 Vp8GetCoeffs(NkVp8BoolDecoder &bd, const uint8 (*prob)[3][11], int32 ctx, int32 n,
							   int16 *out) {
				const uint8 *p = prob[n][ctx];
				if (!bd.GetBool(p[0])) // 1er "EOB" : joue le rôle d'un bit "ce bloc est vide"
					return 0;
				for (;;) {
					++n;
					if (!bd.GetBool(p[1])) { // coefficient nul
						p = prob[kVp8CoefBands[n]][0];
					} else {
						int32 v;
						if (!bd.GetBool(p[2])) { // valeur 1
							p = prob[kVp8CoefBands[n]][1];
							v = 1;
						} else {
							if (!bd.GetBool(p[3])) {
								if (!bd.GetBool(p[4]))
									v = 2;
								else
									v = 3 + bd.GetBool(p[5]);
							} else {
								if (!bd.GetBool(p[6])) {
									if (!bd.GetBool(p[7]))
										v = 5 + bd.GetBool(kVp8Pcat1[0]);
									else {
										v = 7 + 2 * bd.GetBool(kVp8Pcat2[0]);
										v += bd.GetBool(kVp8Pcat2[1]);
									}
								} else {
									// Grandes valeurs : catégories 3..6, magnitude codée par des
									// bits supplémentaires à probabilités CONSTANTES (kVp8Pcat3456,
									// terminée par une sentinelle 0).
									const int32 bit1 = bd.GetBool(p[8]);
									const int32 bit0 = bd.GetBool(p[9 + bit1]);
									const int32 cat = 2 * bit1 + bit0;
									v = 0;
									for (const uint8 *tab = kVp8Pcat3456[cat]; *tab; ++tab)
										v += v + bd.GetBool(*tab);
									v += 3 + (8 << cat);
								}
							}
							p = prob[kVp8CoefBands[n]][2];
						}
						// Le coefficient est stocké à sa position RASTER via le zigzag inverse.
						// Le signe est un bit non biaisé (la référence l'implémente à la main,
						// mais c'est exactement GetBool(128) — vérifié algébriquement).
						out[kVp8Zigzag[n - 1]] = (int16)(bd.GetFlag() ? -v : v);
						if (n == 16 || !bd.GetBool(p[0])) // EOB
							return n;
					}
					if (n == 16)
						return 16;
				}
			}

		} // namespace

		void NkVp8ResetMbTokenContext(bool isBPred, NkVp8EntropyContext &above,
									   NkVp8EntropyContext &left) {
			for (int32 i = 0; i < 8; ++i) { // Y (0-3), U (4-5), V (6-7)
				above.v[i] = 0;
				left.v[i] = 0;
			}
			// ⚠️ L'entrée Y2 n'est remise à zéro que si le MB POSSÈDE un bloc Y2. Un MB en
			// B_PRED n'en a pas : son contexte Y2 doit rester INCHANGÉ pour le MB suivant.
			if (!isBPred) {
				above.v[8] = 0;
				left.v[8] = 0;
			}
		}

		int32 NkVp8DecodeMbTokens(NkVp8BoolDecoder &bd, const NkVp8FrameContext &fc, bool isBPred,
								   NkVp8EntropyContext &above, NkVp8EntropyContext &left,
								   NkVp8MbCoeffs &out) {
			out = NkVp8MbCoeffs();
			int32 eobTotal = 0;
			int32 skipDc = 0;
			const uint8 (*coefProbs)[3][11];

			if (!isBPred) {
				// Bloc Y2 (type 1) : porte les DC des 16 blocs Y (transformés par WHT).
				const int32 nz =
					Vp8GetCoeffs(bd, fc.coefProbs[1], above.v[8] + left.v[8], 0, out.coeffs[24]);
				above.v[8] = left.v[8] = (nz > 0) ? 1 : 0;
				out.eobs[24] = (uint8)nz;
				eobTotal += nz - 16; // convention libvpx (permet de détecter "MB vide")
				coefProbs = fc.coefProbs[0]; // Y SANS le DC (il vient de Y2)
				skipDc = 1;
			} else {
				coefProbs = fc.coefProbs[3]; // Y AVEC son DC
				skipDc = 0;
			}

			for (int32 i = 0; i < 16; ++i) { // 16 blocs luma, ordre raster 4×4
				uint8 &a = above.v[i & 3];
				uint8 &l = left.v[(i & 0xC) >> 2];
				int32 nz = Vp8GetCoeffs(bd, coefProbs, (int32)a + (int32)l, skipDc, out.coeffs[i]);
				a = l = (nz > 0) ? 1 : 0; // contexte MIS À JOUR avant l'ajout de skipDc
				nz += skipDc; // eob=1 signifie alors "DC seul" (venu de Y2)
				out.eobs[i] = (uint8)nz;
				eobTotal += nz;
			}

			for (int32 i = 16; i < 24; ++i) { // 4 blocs U puis 4 blocs V (grilles 2×2)
				uint8 &a = above.v[4 + ((i > 19) ? 2 : 0) + (i & 1)];
				uint8 &l = left.v[4 + ((i > 19) ? 2 : 0) + (((i & 3) > 1) ? 1 : 0)];
				const int32 nz =
					Vp8GetCoeffs(bd, fc.coefProbs[2], (int32)a + (int32)l, 0, out.coeffs[i]);
				a = l = (nz > 0) ? 1 : 0;
				out.eobs[i] = (uint8)nz;
				eobTotal += nz;
			}
			return eobTotal;
		}

		bool NkVp8DecodeKeyFrameResiduals(NkVp8BoolDecoder &bd, const NkVp8FrameContext &fc,
										   const NkVector<NkVp8MbModeInfo> &mbInfo, int32 mbCols,
										   int32 mbRows, NkVp8ResidualStats &stats) {
			if (mbCols <= 0 || mbRows <= 0)
				return false;
			const int32 stride = mbCols + 1;
			if (mbInfo.Size() < (uint64)((mbRows + 1) * stride))
				return false;

			stats = NkVp8ResidualStats();
			// Un contexte « au-dessus » PAR COLONNE (persistant sur toute l'image), remis à
			// zéro au début de l'image ; un contexte « à gauche » unique, remis à zéro au
			// début de CHAQUE ligne de macroblocs.
			NkVector<NkVp8EntropyContext> aboveCtx;
			aboveCtx.Resize((uint64)mbCols);
			for (uint64 i = 0; i < aboveCtx.Size(); ++i)
				aboveCtx[i] = NkVp8EntropyContext();

			NkVp8MbCoeffs coeffs;
			for (int32 r = 0; r < mbRows; ++r) {
				NkVp8EntropyContext leftCtx;
				for (int32 c = 0; c < mbCols; ++c) {
					const NkVp8MbModeInfo &mi = mbInfo[(uint64)(r + 1) * stride + (c + 1)];
					const bool isBPred = (mi.yMode == kVp8MbBPred);
					if (mi.skipCoeff) {
						NkVp8ResetMbTokenContext(isBPred, aboveCtx[(uint64)c], leftCtx);
						++stats.skippedMbs;
						continue;
					}
					stats.eobTotal +=
						NkVp8DecodeMbTokens(bd, fc, isBPred, aboveCtx[(uint64)c], leftCtx, coeffs);
					++stats.decodedMbs;
					for (int32 b = 0; b < 25; ++b) {
						for (int32 k = 0; k < 16; ++k) {
							const int32 v = coeffs.coeffs[b][k];
							if (v != 0)
								++stats.nonZeroCoeffs;
							if (v < stats.minCoeff)
								stats.minCoeff = v;
							if (v > stats.maxCoeff)
								stats.maxCoeff = v;
						}
					}
				}
			}
			return true;
		}

		// =====================================================================
		//  Reconstruction : déquantification, transformées inverses, prédiction intra
		// =====================================================================
		namespace {

			inline uint8 Clip255(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}
			inline int32 ClampQIndex(int32 q) {
				return q < 0 ? 0 : (q > 127 ? 127 : q);
			}
			inline int32 Avg2(int32 a, int32 b) {
				return (a + b + 1) >> 1;
			}
			inline int32 Avg3(int32 a, int32 b, int32 c) {
				return (a + 2 * b + c + 2) >> 2;
			}

			// §14.3 : transformée inverse de Walsh-Hadamard 4×4. Reconstruit les 16 DC des
			// blocs luma à partir du bloc Y2, et les écrit DIRECTEMENT à la position 0 de
			// chacun des 16 blocs (d'où le pas de 16 dans `mbCoeffs`).
			void Vp8InverseWalsh(const int16 *input, int16 *mbCoeffs) {
				int16 output[16];
				for (int32 i = 0; i < 4; ++i) {
					const int32 a1 = input[i + 0] + input[i + 12];
					const int32 b1 = input[i + 4] + input[i + 8];
					const int32 c1 = input[i + 4] - input[i + 8];
					const int32 d1 = input[i + 0] - input[i + 12];
					output[i + 0] = (int16)(a1 + b1);
					output[i + 4] = (int16)(c1 + d1);
					output[i + 8] = (int16)(a1 - b1);
					output[i + 12] = (int16)(d1 - c1);
				}
				for (int32 i = 0; i < 4; ++i) {
					const int32 o = i * 4;
					const int32 a1 = output[o + 0] + output[o + 3];
					const int32 b1 = output[o + 1] + output[o + 2];
					const int32 c1 = output[o + 1] - output[o + 2];
					const int32 d1 = output[o + 0] - output[o + 3];
					output[o + 0] = (int16)((a1 + b1 + 3) >> 3);
					output[o + 1] = (int16)((c1 + d1 + 3) >> 3);
					output[o + 2] = (int16)((a1 - b1 + 3) >> 3);
					output[o + 3] = (int16)((d1 - c1 + 3) >> 3);
				}
				for (int32 i = 0; i < 16; ++i)
					mbCoeffs[i * 16] = output[i];
			}

			// Variante « DC seul » : quand le bloc Y2 n'a qu'un coefficient, les 16 DC sont
			// tous identiques. Doit donner EXACTEMENT le même résultat que la version
			// générale sur une entrée DC-seul (propriété vérifiée en self-test).
			void Vp8InverseWalshDcOnly(int32 dc, int16 *mbCoeffs) {
				const int16 a1 = (int16)((dc + 3) >> 3);
				for (int32 i = 0; i < 16; ++i)
					mbCoeffs[i * 16] = a1;
			}

			// §14.4 : IDCT 4×4 « llm ». Les deux constantes sont des approximations 16 bits
			// en virgule fixe de sqrt(2)·cos(pi/8) et sqrt(2)·sin(pi/8) ; la première étant
			// > 1, elle est codée sous la forme x·a = x + x·(a−1), d'où le « minus1 ».
			const int32 kCosPi8Sqrt2Minus1 = 20091;
			const int32 kSinPi8Sqrt2 = 35468;

			// Applique l'IDCT à `input` (déjà déquantifié) et ADDITIONNE le résultat au
			// prédicteur déjà présent dans `dst`, avec saturation.
			void Vp8IdctAdd(const int16 *input, uint8 *dst, int32 stride) {
				int16 tmp[16];
				for (int32 i = 0; i < 4; ++i) {
					const int32 i0 = input[i + 0], i4 = input[i + 4];
					const int32 i8 = input[i + 8], i12 = input[i + 12];
					const int32 a1 = i0 + i8;
					const int32 b1 = i0 - i8;
					const int32 c1 = ((i4 * kSinPi8Sqrt2) >> 16) - (i12 + ((i12 * kCosPi8Sqrt2Minus1) >> 16));
					const int32 d1 = (i4 + ((i4 * kCosPi8Sqrt2Minus1) >> 16)) + ((i12 * kSinPi8Sqrt2) >> 16);
					tmp[i + 0] = (int16)(a1 + d1);
					tmp[i + 12] = (int16)(a1 - d1);
					tmp[i + 4] = (int16)(b1 + c1);
					tmp[i + 8] = (int16)(b1 - c1);
				}
				for (int32 i = 0; i < 4; ++i) {
					const int32 o = i * 4;
					const int32 t0 = tmp[o + 0], t1 = tmp[o + 1], t2 = tmp[o + 2], t3 = tmp[o + 3];
					const int32 a1 = t0 + t2;
					const int32 b1 = t0 - t2;
					const int32 c1 = ((t1 * kSinPi8Sqrt2) >> 16) - (t3 + ((t3 * kCosPi8Sqrt2Minus1) >> 16));
					const int32 d1 = (t1 + ((t1 * kCosPi8Sqrt2Minus1) >> 16)) + ((t3 * kSinPi8Sqrt2) >> 16);
					tmp[o + 0] = (int16)((a1 + d1 + 4) >> 3);
					tmp[o + 3] = (int16)((a1 - d1 + 4) >> 3);
					tmp[o + 1] = (int16)((b1 + c1 + 4) >> 3);
					tmp[o + 2] = (int16)((b1 - c1 + 4) >> 3);
				}
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c)
						dst[r * stride + c] = Clip255(tmp[r * 4 + c] + dst[r * stride + c]);
			}

			// Raccourci « DC seul » (eob == 1) : toute la matrice vaut la même constante.
			void Vp8IdctAddDcOnly(int32 dc, uint8 *dst, int32 stride) {
				const int32 a1 = (dc + 4) >> 3;
				for (int32 r = 0; r < 4; ++r)
					for (int32 c = 0; c < 4; ++c)
						dst[r * stride + c] = Clip255(a1 + dst[r * stride + c]);
			}

			// ── Prédicteurs intra pleine taille (16×16 luma, 8×8 chroma) ─────────────
			// `above` pointe sur la ligne au-dessus du bloc, `left` sur la colonne à gauche
			// (pas `leftStride` : on la copie d'abord dans un tableau contigu).
			void Vp8PredictBlockDcVhTm(uint8 *dst, int32 stride, int32 bs, const uint8 *above,
									   const uint8 *leftCol, int32 mode, bool upAvail,
									   bool leftAvail) {
				if (mode == kVp8MbVPred) {
					for (int32 r = 0; r < bs; ++r)
						for (int32 c = 0; c < bs; ++c)
							dst[r * stride + c] = above[c];
					return;
				}
				if (mode == kVp8MbHPred) {
					for (int32 r = 0; r < bs; ++r)
						for (int32 c = 0; c < bs; ++c)
							dst[r * stride + c] = leftCol[r];
					return;
				}
				if (mode == kVp8MbTmPred) {
					const int32 topLeft = above[-1];
					for (int32 r = 0; r < bs; ++r)
						for (int32 c = 0; c < bs; ++c)
							dst[r * stride + c] = Clip255(leftCol[r] + above[c] - topLeft);
					return;
				}
				// DC_PRED : la moyenne porte sur les voisins RÉELLEMENT disponibles ; si
				// aucun ne l'est (macrobloc en haut à gauche), la valeur normative est 128.
				int32 expected;
				if (upAvail && leftAvail) {
					int32 sum = 0;
					for (int32 i = 0; i < bs; ++i)
						sum += above[i] + leftCol[i];
					expected = (sum + bs) / (2 * bs);
				} else if (upAvail) {
					int32 sum = 0;
					for (int32 i = 0; i < bs; ++i)
						sum += above[i];
					expected = (sum + (bs >> 1)) / bs;
				} else if (leftAvail) {
					int32 sum = 0;
					for (int32 i = 0; i < bs; ++i)
						sum += leftCol[i];
					expected = (sum + (bs >> 1)) / bs;
				} else {
					expected = 128;
				}
				for (int32 r = 0; r < bs; ++r)
					for (int32 c = 0; c < bs; ++c)
						dst[r * stride + c] = (uint8)expected;
			}

			// ── Prédicteurs intra 4×4 (B_PRED, 10 modes) ─────────────────────────────
			// `above` doit exposer 8 échantillons (A..H) ET `above[-1]` (coin haut-gauche) :
			// les modes diagonaux lisent jusqu'à 4 pixels EN HAUT À DROITE du bloc.
			void Vp8Predict4x4(uint8 *dst, int32 stride, int32 mode, const uint8 *above,
							   const uint8 *left) {
				const int32 X = above[-1];
				const int32 A = above[0], B = above[1], C = above[2], D = above[3];
				const int32 E = above[4], F = above[5], G = above[6], H = above[7];
				const int32 I = left[0], J = left[1], K = left[2], L = left[3];
				auto D_ = [&](int32 x, int32 y) -> uint8 & { return dst[(x) + (y)*stride]; };

				switch (mode) {
					case kVp8BDcPred: {
						int32 sum = A + B + C + D + I + J + K + L;
						const uint8 v = (uint8)((sum + 4) >> 3);
						for (int32 r = 0; r < 4; ++r)
							for (int32 c = 0; c < 4; ++c)
								D_(c, r) = v;
						break;
					}
					case kVp8BTmPred:
						for (int32 r = 0; r < 4; ++r)
							for (int32 c = 0; c < 4; ++c)
								D_(c, r) = Clip255(left[r] + above[c] - X);
						break;
					case kVp8BVePred: {
						const uint8 v0 = (uint8)Avg3(X, A, B), v1 = (uint8)Avg3(A, B, C);
						const uint8 v2 = (uint8)Avg3(B, C, D), v3 = (uint8)Avg3(C, D, E);
						for (int32 r = 0; r < 4; ++r) {
							D_(0, r) = v0;
							D_(1, r) = v1;
							D_(2, r) = v2;
							D_(3, r) = v3;
						}
						break;
					}
					case kVp8BHePred: {
						const uint8 r0 = (uint8)Avg3(X, I, J), r1 = (uint8)Avg3(I, J, K);
						const uint8 r2 = (uint8)Avg3(J, K, L), r3 = (uint8)Avg3(K, L, L);
						for (int32 c = 0; c < 4; ++c) {
							D_(c, 0) = r0;
							D_(c, 1) = r1;
							D_(c, 2) = r2;
							D_(c, 3) = r3;
						}
						break;
					}
					case kVp8BLdPred: // "down-left" (d45e)
						D_(0, 0) = (uint8)Avg3(A, B, C);
						D_(1, 0) = D_(0, 1) = (uint8)Avg3(B, C, D);
						D_(2, 0) = D_(1, 1) = D_(0, 2) = (uint8)Avg3(C, D, E);
						D_(3, 0) = D_(2, 1) = D_(1, 2) = D_(0, 3) = (uint8)Avg3(D, E, F);
						D_(3, 1) = D_(2, 2) = D_(1, 3) = (uint8)Avg3(E, F, G);
						D_(3, 2) = D_(2, 3) = (uint8)Avg3(F, G, H);
						D_(3, 3) = (uint8)Avg3(G, H, H);
						break;
					case kVp8BRdPred: // "down-right" (d135)
						D_(0, 3) = (uint8)Avg3(J, K, L);
						D_(1, 3) = D_(0, 2) = (uint8)Avg3(I, J, K);
						D_(2, 3) = D_(1, 2) = D_(0, 1) = (uint8)Avg3(X, I, J);
						D_(3, 3) = D_(2, 2) = D_(1, 1) = D_(0, 0) = (uint8)Avg3(A, X, I);
						D_(3, 2) = D_(2, 1) = D_(1, 0) = (uint8)Avg3(B, A, X);
						D_(3, 1) = D_(2, 0) = (uint8)Avg3(C, B, A);
						D_(3, 0) = (uint8)Avg3(D, C, B);
						break;
					case kVp8BVrPred: // "vertical-right" (d117)
						D_(0, 0) = D_(1, 2) = (uint8)Avg2(X, A);
						D_(1, 0) = D_(2, 2) = (uint8)Avg2(A, B);
						D_(2, 0) = D_(3, 2) = (uint8)Avg2(B, C);
						D_(3, 0) = (uint8)Avg2(C, D);
						D_(0, 3) = (uint8)Avg3(K, J, I);
						D_(0, 2) = (uint8)Avg3(J, I, X);
						D_(0, 1) = D_(1, 3) = (uint8)Avg3(I, X, A);
						D_(1, 1) = D_(2, 3) = (uint8)Avg3(X, A, B);
						D_(2, 1) = D_(3, 3) = (uint8)Avg3(A, B, C);
						D_(3, 1) = (uint8)Avg3(B, C, D);
						break;
					case kVp8BVlPred: // "vertical-left" (d63e)
						D_(0, 0) = (uint8)Avg2(A, B);
						D_(1, 0) = D_(0, 2) = (uint8)Avg2(B, C);
						D_(2, 0) = D_(1, 2) = (uint8)Avg2(C, D);
						D_(3, 0) = D_(2, 2) = (uint8)Avg2(D, E);
						D_(3, 2) = (uint8)Avg3(E, F, G);
						D_(0, 1) = (uint8)Avg3(A, B, C);
						D_(1, 1) = D_(0, 3) = (uint8)Avg3(B, C, D);
						D_(2, 1) = D_(1, 3) = (uint8)Avg3(C, D, E);
						D_(3, 1) = D_(2, 3) = (uint8)Avg3(D, E, F);
						D_(3, 3) = (uint8)Avg3(F, G, H);
						break;
					case kVp8BHdPred: // "horizontal-down" (d153)
						D_(0, 0) = D_(2, 1) = (uint8)Avg2(I, X);
						D_(0, 1) = D_(2, 2) = (uint8)Avg2(J, I);
						D_(0, 2) = D_(2, 3) = (uint8)Avg2(K, J);
						D_(0, 3) = (uint8)Avg2(L, K);
						D_(3, 0) = (uint8)Avg3(A, B, C);
						D_(2, 0) = (uint8)Avg3(X, A, B);
						D_(1, 0) = D_(3, 1) = (uint8)Avg3(I, X, A);
						D_(1, 1) = D_(3, 2) = (uint8)Avg3(J, I, X);
						D_(1, 2) = D_(3, 3) = (uint8)Avg3(K, J, I);
						D_(1, 3) = (uint8)Avg3(L, K, J);
						break;
					default: // kVp8BHuPred, "horizontal-up" (d207)
						D_(0, 0) = (uint8)Avg2(I, J);
						D_(2, 0) = D_(0, 1) = (uint8)Avg2(J, K);
						D_(2, 1) = D_(0, 2) = (uint8)Avg2(K, L);
						D_(1, 0) = (uint8)Avg3(I, J, K);
						D_(3, 0) = D_(1, 1) = (uint8)Avg3(J, K, L);
						D_(3, 1) = D_(1, 2) = (uint8)Avg3(K, L, L);
						D_(3, 2) = D_(2, 2) = D_(0, 3) = D_(1, 3) = D_(2, 3) = D_(3, 3) = (uint8)L;
						break;
				}
			}

			// ── Filtre de boucle (§15) ───────────────────────────────────────────────
			// Toutes les primitives travaillent sur des valeurs recentrées (`^ 0x80`,
			// c'est-à-dire décalées de -128) et saturées en signed char — le style de la
			// référence, conservé à l'identique pour l'exactitude bit à bit.

			inline int8 SignedClamp(int32 t) {
				return (int8)(t < -128 ? -128 : (t > 127 ? 127 : t));
			}
			inline int32 IAbs(int32 v) {
				return v < 0 ? -v : v;
			}

			// Faut-il filtrer cette position ? (masque -1 = oui, 0 = non)
			inline int32 FilterMask(int32 limit, int32 blimit, int32 p3, int32 p2, int32 p1,
									int32 p0, int32 q0, int32 q1, int32 q2, int32 q3) {
				int32 mask = 0;
				mask |= (IAbs(p3 - p2) > limit);
				mask |= (IAbs(p2 - p1) > limit);
				mask |= (IAbs(p1 - p0) > limit);
				mask |= (IAbs(q1 - q0) > limit);
				mask |= (IAbs(q2 - q1) > limit);
				mask |= (IAbs(q3 - q2) > limit);
				mask |= (IAbs(p0 - q0) * 2 + IAbs(p1 - q1) / 2 > blimit);
				return mask - 1;
			}

			// Variance d'arête élevée ? (-1 = oui, 0 = non)
			inline int32 HevMask(int32 thresh, int32 p1, int32 p0, int32 q0, int32 q1) {
				int32 hev = 0;
				hev |= (IAbs(p1 - p0) > thresh) * -1;
				hev |= (IAbs(q1 - q0) > thresh) * -1;
				return hev;
			}

			// Filtre « intérieur » (arêtes de sous-bloc) : ajuste p1/p0/q0/q1.
			void Vp8Filter4(int32 mask, int32 hev, uint8 *op1, uint8 *op0, uint8 *oq0,
							uint8 *oq1) {
				const int8 ps1 = (int8)(*op1 ^ 0x80), ps0 = (int8)(*op0 ^ 0x80);
				const int8 qs0 = (int8)(*oq0 ^ 0x80), qs1 = (int8)(*oq1 ^ 0x80);
				int32 f = SignedClamp(ps1 - qs1);
				f &= hev; // les taps externes ne participent qu'en cas de forte variance
				f = SignedClamp(f + 3 * (qs0 - ps0));
				f &= mask;
				// +4 d'un côté, +3 de l'autre : arrondi asymétrique voulu par la norme.
				const int8 f1 = (int8)(SignedClamp(f + 4) >> 3);
				const int8 f2 = (int8)(SignedClamp(f + 3) >> 3);
				*oq0 = (uint8)(SignedClamp(qs0 - f1) ^ 0x80);
				*op0 = (uint8)(SignedClamp(ps0 + f2) ^ 0x80);
				int32 outer = f1;
				outer = (outer + 1) >> 1;
				outer &= ~hev; // p1/q1 ne bougent QUE si la variance est faible
				*oq1 = (uint8)(SignedClamp(qs1 - outer) ^ 0x80);
				*op1 = (uint8)(SignedClamp(ps1 + outer) ^ 0x80);
			}

			// Filtre « fort » (arêtes de macrobloc) : ajuste p2..q2 (6 pixels).
			void Vp8MbFilter6(int32 mask, int32 hev, uint8 *op2, uint8 *op1, uint8 *op0,
							  uint8 *oq0, uint8 *oq1, uint8 *oq2) {
				int8 ps2 = (int8)(*op2 ^ 0x80), ps1 = (int8)(*op1 ^ 0x80), ps0 = (int8)(*op0 ^ 0x80);
				int8 qs0 = (int8)(*oq0 ^ 0x80), qs1 = (int8)(*oq1 ^ 0x80), qs2 = (int8)(*oq2 ^ 0x80);
				int32 f = SignedClamp(ps1 - qs1);
				f = SignedClamp(f + 3 * (qs0 - ps0));
				f &= mask;
				// Cas forte variance : filtre court classique sur p0/q0 seulement.
				int32 f2 = f & hev;
				const int8 f1c = (int8)(SignedClamp(f2 + 4) >> 3);
				const int8 f2c = (int8)(SignedClamp(f2 + 3) >> 3);
				qs0 = SignedClamp(qs0 - f1c);
				ps0 = SignedClamp(ps0 + f2c);
				// Cas variance faible : filtre large ~3/7, 2/7, 1/7 de la différence.
				f &= ~hev;
				const int32 w = f;
				int32 u = SignedClamp((63 + w * 27) >> 7);
				*oq0 = (uint8)(SignedClamp(qs0 - u) ^ 0x80);
				*op0 = (uint8)(SignedClamp(ps0 + u) ^ 0x80);
				u = SignedClamp((63 + w * 18) >> 7);
				*oq1 = (uint8)(SignedClamp(qs1 - u) ^ 0x80);
				*op1 = (uint8)(SignedClamp(ps1 + u) ^ 0x80);
				u = SignedClamp((63 + w * 9) >> 7);
				*oq2 = (uint8)(SignedClamp(qs2 - u) ^ 0x80);
				*op2 = (uint8)(SignedClamp(ps2 + u) ^ 0x80);
			}

			// Arête HORIZONTALE (pixels au-dessus/au-dessous, on avance en colonne).
			void Vp8FilterHorizEdge(uint8 *s, int32 p, int32 blimit, int32 limit, int32 thresh,
									int32 count, bool mbEdge) {
				for (int32 i = 0; i < count * 8; ++i, ++s) {
					const int32 mask = FilterMask(limit, blimit, s[-4 * p], s[-3 * p], s[-2 * p],
												  s[-1 * p], s[0], s[1 * p], s[2 * p], s[3 * p]);
					const int32 hev = HevMask(thresh, s[-2 * p], s[-1 * p], s[0], s[1 * p]);
					if (mbEdge)
						Vp8MbFilter6(mask, hev, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p,
									 s + 2 * p);
					else
						Vp8Filter4(mask, hev, s - 2 * p, s - 1 * p, s, s + 1 * p);
				}
			}

			// Arête VERTICALE (pixels à gauche/à droite, on avance en ligne).
			void Vp8FilterVertEdge(uint8 *s, int32 p, int32 blimit, int32 limit, int32 thresh,
								   int32 count, bool mbEdge) {
				for (int32 i = 0; i < count * 8; ++i, s += p) {
					const int32 mask = FilterMask(limit, blimit, s[-4], s[-3], s[-2], s[-1], s[0],
												  s[1], s[2], s[3]);
					const int32 hev = HevMask(thresh, s[-2], s[-1], s[0], s[1]);
					if (mbEdge)
						Vp8MbFilter6(mask, hev, s - 3, s - 2, s - 1, s, s + 1, s + 2);
					else
						Vp8Filter4(mask, hev, s - 2, s - 1, s, s + 1);
				}
			}

			// Variante SIMPLE (filterType == 1) : luma seulement, masque réduit, 2 pixels.
			void Vp8SimpleFilterEdge(uint8 *s, int32 stride, int32 blimit, bool vertical) {
				const int32 stepPix = vertical ? stride : 1; // d'un pixel au suivant LE LONG de l'arête
				const int32 stepTap = vertical ? 1 : stride; // d'un tap au suivant EN TRAVERS de l'arête
				for (int32 i = 0; i < 16; ++i, s += stepPix) {
					const int32 p1 = s[-2 * stepTap], p0 = s[-1 * stepTap];
					const int32 q0 = s[0], q1 = s[1 * stepTap];
					const int32 mask = (IAbs(p0 - q0) * 2 + IAbs(p1 - q1) / 2 <= blimit) ? -1 : 0;
					const int8 sp1 = (int8)(p1 ^ 0x80), sp0 = (int8)(p0 ^ 0x80);
					const int8 sq0 = (int8)(q0 ^ 0x80), sq1 = (int8)(q1 ^ 0x80);
					int32 f = SignedClamp(sp1 - sq1);
					f = SignedClamp(f + 3 * (sq0 - sp0));
					f &= mask;
					s[0] = (uint8)(SignedClamp(sq0 - (SignedClamp(f + 4) >> 3)) ^ 0x80);
					s[-1 * stepTap] = (uint8)(SignedClamp(sp0 + (SignedClamp(f + 3) >> 3)) ^ 0x80);
				}
			}

		} // namespace

		void NkVp8ComputeDequant(const NkVp8FrameHeader &hdr, int32 qIndex, NkVp8Dequant &out) {
			const int32 q = ClampQIndex(qIndex);
			out.y1[0] = kVp8DcQLookup[ClampQIndex(q + hdr.y1dcDeltaQ)];
			out.y1[1] = kVp8AcQLookup[q];
			// Y2 : le DC est DOUBLÉ et l'AC multiplié par 155/100 (borné à 8) — c'est la
			// compensation d'échelle de la transformée de Walsh appliquée aux DC.
			out.y2[0] = (int16)(kVp8DcQLookup[ClampQIndex(q + hdr.y2dcDeltaQ)] * 2);
			int32 ac2 = (kVp8AcQLookup[ClampQIndex(q + hdr.y2acDeltaQ)] * 101581) >> 16;
			out.y2[1] = (int16)(ac2 < 8 ? 8 : ac2);
			int32 dcUv = kVp8DcQLookup[ClampQIndex(q + hdr.uvdcDeltaQ)];
			out.uv[0] = (int16)(dcUv > 132 ? 132 : dcUv); // plafond normatif du DC chroma
			out.uv[1] = kVp8AcQLookup[ClampQIndex(q + hdr.uvacDeltaQ)];
		}

		bool NkVp8ReconstructKeyFrame(NkVp8BoolDecoder &tokenBd, const NkVp8FrameContext &fc,
									   const NkVp8FrameHeader &hdr,
									   const NkVp8LoopFilterDeltas &lfDeltas,
									   const NkVector<NkVp8MbModeInfo> &mbInfo, int32 width,
									   int32 height, NkVp8Image &out) {
			const int32 mbCols = (width + 15) / 16;
			const int32 mbRows = (height + 15) / 16;
			if (mbCols <= 0 || mbRows <= 0)
				return false;
			const int32 miStride = mbCols + 1;
			if (mbInfo.Size() < (uint64)((mbRows + 1) * miStride))
				return false;
			// Segmentation par macrobloc non gérée à ce stade (aucun de nos flux de test ne
			// l'active) : on refuse proprement plutôt que de produire des pixels faux.
			if (hdr.segmentationEnabled)
				return false;

			// Buffers avec bordure : 1 pixel à gauche, 1 ligne au-dessus, marge à droite
			// (les prédicteurs 4×4 diagonaux lisent jusqu'à 4 pixels au-delà du bord droit).
			const int32 yW = mbCols * 16, yH = mbRows * 16;
			const int32 cW = mbCols * 8, cH = mbRows * 8;
			out.width = width;
			out.height = height;
			out.mbCols = mbCols;
			out.mbRows = mbRows;
			out.yStride = yW + 1 + 8;
			out.uvStride = cW + 1 + 8;
			out.yOrigin = out.yStride + 1;
			out.uvOrigin = out.uvStride + 1;
			out.y.Resize((uint64)(out.yStride * (yH + 1)));
			out.u.Resize((uint64)(out.uvStride * (cH + 1)));
			out.v.Resize((uint64)(out.uvStride * (cH + 1)));
			for (uint64 i = 0; i < out.y.Size(); ++i)
				out.y[i] = 0;
			for (uint64 i = 0; i < out.u.Size(); ++i)
				out.u[i] = 0;
			for (uint64 i = 0; i < out.v.Size(); ++i)
				out.v[i] = 0;

			// §12.2 : bordures normatives — 127 sur la ligne AU-DESSUS de l'image (y compris
			// le coin), 129 sur la colonne À GAUCHE. Ces constantes ne sont pas arbitraires :
			// elles font que les prédicteurs V/H/TM donnent le résultat attendu sur les bords
			// SANS cas particulier.
			auto initBorders = [](uint8 *plane, int32 stride, int32 w, int32 h) {
				uint8 *topLeft = plane - stride - 1;
				for (int32 i = 0; i < w + 5; ++i)
					topLeft[i] = 127;
				for (int32 r = 0; r < h; ++r)
					plane[(int64)r * stride - 1] = 129;
			};
			initBorders(out.Y(), out.yStride, yW, yH);
			initBorders(out.U(), out.uvStride, cW, cH);
			initBorders(out.V(), out.uvStride, cW, cH);

			NkVp8Dequant dq;
			NkVp8ComputeDequant(hdr, hdr.baseQIndex, dq);

			NkVector<NkVp8EntropyContext> aboveCtx;
			aboveCtx.Resize((uint64)mbCols);
			for (uint64 i = 0; i < aboveCtx.Size(); ++i)
				aboveCtx[i] = NkVp8EntropyContext();

			NkVp8MbCoeffs mb;
			int16 dqBlock[16];

			// Skip EFFECTIF par macrobloc, pour le filtre de boucle : un MB non sauté au
			// bitstream mais dont TOUS les coefficients sont nuls (eobTotal == 0) est
			// considéré sauté par le filtre (miroir de la référence, qui force
			// `mb_skip_coeff = (eobtotal == 0)` après le décodage des tokens).
			NkVector<uint8> effSkip;
			effSkip.Resize((uint64)(mbRows * mbCols));

			for (int32 r = 0; r < mbRows; ++r) {
				NkVp8EntropyContext leftCtx;
				for (int32 c = 0; c < mbCols; ++c) {
					const NkVp8MbModeInfo &mi = mbInfo[(uint64)(r + 1) * miStride + (c + 1)];
					const bool isBPred = (mi.yMode == kVp8MbBPred);
					const bool upAvail = (r != 0);
					const bool leftAvail = (c != 0);

					if (mi.skipCoeff) {
						NkVp8ResetMbTokenContext(isBPred, aboveCtx[(uint64)c], leftCtx);
						mb = NkVp8MbCoeffs(); // aucun résidu : coefficients tous nuls
						effSkip[(uint64)(r * mbCols + c)] = 1;
					} else {
						const int32 eobTotal =
							NkVp8DecodeMbTokens(tokenBd, fc, isBPred, aboveCtx[(uint64)c], leftCtx, mb);
						effSkip[(uint64)(r * mbCols + c)] = (eobTotal == 0) ? 1 : 0;
					}

					uint8 *yMb = out.Y() + (int64)(r * 16) * out.yStride + c * 16;
					uint8 *uMb = out.U() + (int64)(r * 8) * out.uvStride + c * 8;
					uint8 *vMb = out.V() + (int64)(r * 8) * out.uvStride + c * 8;

					// ── Luma ────────────────────────────────────────────────────────
					// `dcFactor` : quand un bloc Y2 existe, ses DC déjà déquantifiés sont
					// écrits dans les blocs luma, donc il ne faut PAS les re-multiplier —
					// d'où un facteur DC de 1 (astuce de la référence).
					int16 yDc = dq.y1[0];
					if (!isBPred) {
						// Bloc Y2 → IWHT → DC des 16 blocs luma.
						if (mb.eobs[24] > 1) {
							for (int32 i = 0; i < 16; ++i)
								dqBlock[i] = (int16)(mb.coeffs[24][i] * (i == 0 ? dq.y2[0] : dq.y2[1]));
							Vp8InverseWalsh(dqBlock, &mb.coeffs[0][0]);
						} else {
							Vp8InverseWalshDcOnly(mb.coeffs[24][0] * dq.y2[0], &mb.coeffs[0][0]);
						}
						yDc = 1; // les DC sont déjà déquantifiés par le chemin Y2
					}

					if (isBPred) {
						// B_PRED : chaque sous-bloc est prédit PUIS reconstruit avant le
						// suivant (cascade), car ses voisins servent de référence.
						// « Down-copy » : les 4 pixels en haut à droite du macrobloc sont
						// recopiés sur les lignes 3/7/11 afin que les sous-blocs des rangées
						// inférieures disposent eux aussi d'un « au-dessus à droite ».
						const uint8 *aboveRight = yMb - out.yStride + 16;
						for (int32 k = 0; k < 3; ++k)
							for (int32 i = 0; i < 4; ++i)
								yMb[(int64)(3 + 4 * k) * out.yStride + 16 + i] = aboveRight[i];

						for (int32 b = 0; b < 16; ++b) {
							uint8 *dst = yMb + (int64)((b >> 2) * 4) * out.yStride + (b & 3) * 4;
							const uint8 *above = dst - out.yStride;
							uint8 leftCol[4];
							for (int32 i = 0; i < 4; ++i)
								leftCol[i] = dst[(int64)i * out.yStride - 1];
							Vp8Predict4x4(dst, out.yStride, mi.bModes[b], above, leftCol);
							if (mb.eobs[b] > 1) {
								for (int32 i = 0; i < 16; ++i)
									dqBlock[i] = (int16)(mb.coeffs[b][i] * (i == 0 ? yDc : dq.y1[1]));
								Vp8IdctAdd(dqBlock, dst, out.yStride);
							} else if (mb.eobs[b] == 1) {
								Vp8IdctAddDcOnly(mb.coeffs[b][0] * yDc, dst, out.yStride);
							}
						}
					} else {
						uint8 leftCol[16];
						for (int32 i = 0; i < 16; ++i)
							leftCol[i] = yMb[(int64)i * out.yStride - 1];
						Vp8PredictBlockDcVhTm(yMb, out.yStride, 16, yMb - out.yStride, leftCol,
											  mi.yMode, upAvail, leftAvail);
						for (int32 b = 0; b < 16; ++b) {
							uint8 *dst = yMb + (int64)((b >> 2) * 4) * out.yStride + (b & 3) * 4;
							if (mb.eobs[b] > 1) {
								for (int32 i = 0; i < 16; ++i)
									dqBlock[i] = (int16)(mb.coeffs[b][i] * (i == 0 ? yDc : dq.y1[1]));
								Vp8IdctAdd(dqBlock, dst, out.yStride);
							} else if (mb.eobs[b] == 1) {
								Vp8IdctAddDcOnly(mb.coeffs[b][0] * yDc, dst, out.yStride);
							}
						}
					}

					// ── Chroma (U puis V, 8×8 prédits en un bloc, résidu par 4×4) ────
					for (int32 plane = 0; plane < 2; ++plane) {
						uint8 *cMb = (plane == 0) ? uMb : vMb;
						uint8 leftCol[8];
						for (int32 i = 0; i < 8; ++i)
							leftCol[i] = cMb[(int64)i * out.uvStride - 1];
						Vp8PredictBlockDcVhTm(cMb, out.uvStride, 8, cMb - out.uvStride, leftCol,
											  mi.uvMode, upAvail, leftAvail);
						for (int32 k = 0; k < 4; ++k) {
							const int32 b = 16 + plane * 4 + k;
							uint8 *dst = cMb + (int64)((k >> 1) * 4) * out.uvStride + (k & 1) * 4;
							if (mb.eobs[b] > 1) {
								for (int32 i = 0; i < 16; ++i)
									dqBlock[i] = (int16)(mb.coeffs[b][i] * (i == 0 ? dq.uv[0] : dq.uv[1]));
								Vp8IdctAdd(dqBlock, dst, out.uvStride);
							} else if (mb.eobs[b] == 1) {
								Vp8IdctAddDcOnly(mb.coeffs[b][0] * dq.uv[0], dst, out.uvStride);
							}
						}
					}
				}

				// ⚠️ Extension du bord DROIT en fin de ligne de macroblocs (§reconstruction).
				// Les modes 4×4 diagonaux (LD, VE, VL…) lisent 4 échantillons EN HAUT À DROITE
				// du sous-bloc ; pour le DERNIER macrobloc d'une ligne, ces échantillons sont
				// hors image. La référence y réplique le dernier pixel réel de la ligne — et
				// UNIQUEMENT sur les DEUX DERNIÈRES lignes du macrobloc, car seule la ligne 15
				// sera lue comme « au-dessus » par la ligne de macroblocs suivante.
				// Sans ça : 17 pixels faux, tous dans la dernière colonne de macroblocs, sur
				// les seuls sous-blocs utilisant un mode à référence haut-droite (bug réel).
				for (int32 k = 14; k <= 15; ++k) {
					uint8 *row = out.Y() + (int64)(r * 16 + k) * out.yStride + yW;
					for (int32 i = 0; i < 4; ++i)
						row[i] = row[-1];
				}
				for (int32 k = 6; k <= 7; ++k) {
					uint8 *ru = out.U() + (int64)(r * 8 + k) * out.uvStride + cW;
					uint8 *rv = out.V() + (int64)(r * 8 + k) * out.uvStride + cW;
					for (int32 i = 0; i < 4; ++i) {
						ru[i] = ru[-1];
						rv[i] = rv[-1];
					}
				}
			}

			// ── Filtre de boucle (§15) : passe finale, ordre raster ─────────────────
			if (hdr.filterLevel > 0) {
				for (int32 r = 0; r < mbRows; ++r) {
					for (int32 c = 0; c < mbCols; ++c) {
						const NkVp8MbModeInfo &mi = mbInfo[(uint64)(r + 1) * miStride + (c + 1)];
						const bool isBPred = (mi.yMode == kVp8MbBPred);

						// Niveau par MB (image clé, sans segmentation) : niveau de base +
						// ajustements par référence/mode si activés — B_PRED a son propre
						// delta de mode, les autres modes intra n'en ont pas.
						int32 lvl = hdr.filterLevel;
						if (hdr.lfDeltaEnabled) {
							int32 lvlRef = hdr.filterLevel + lfDeltas.refLfDeltas[0]; // [0] = INTRA
							if (isBPred)
								lvlRef += lfDeltas.modeLfDeltas[0]; // [0] = B_PRED
							lvl = lvlRef < 0 ? 0 : (lvlRef > 63 ? 63 : lvlRef);
						}
						if (lvl == 0)
							continue;

						// Seuils dérivés du niveau et de la netteté (§15.2, miroir de
						// vp8_loop_filter_update_sharpness).
						int32 interior = lvl >> ((hdr.sharpnessLevel > 0) ? 1 : 0);
						interior >>= (hdr.sharpnessLevel > 4) ? 1 : 0;
						if (hdr.sharpnessLevel > 0 && interior > 9 - hdr.sharpnessLevel)
							interior = 9 - hdr.sharpnessLevel;
						if (interior < 1)
							interior = 1;
						const int32 lim = interior;
						const int32 blim = 2 * lvl + interior;
						const int32 mblim = 2 * (lvl + 2) + interior;
						// Seuil de variance d'arête, table IMAGE CLÉ (différente des inter).
						const int32 hev = (lvl >= 40) ? 2 : ((lvl >= 15) ? 1 : 0);

						// Un MB sans coefficient (et pas en B_PRED) saute ses arêtes INTERNES,
						// mais filtre quand même ses bords de macrobloc.
						const bool skipInternal = !isBPred && effSkip[(uint64)(r * mbCols + c)] != 0;

						uint8 *yMb = out.Y() + (int64)(r * 16) * out.yStride + c * 16;
						uint8 *uMb = out.U() + (int64)(r * 8) * out.uvStride + c * 8;
						uint8 *vMb = out.V() + (int64)(r * 8) * out.uvStride + c * 8;

						if (hdr.filterType == 0) { // filtre NORMAL (luma + chroma)
							if (c > 0) {
								Vp8FilterVertEdge(yMb, out.yStride, mblim, lim, hev, 2, true);
								Vp8FilterVertEdge(uMb, out.uvStride, mblim, lim, hev, 1, true);
								Vp8FilterVertEdge(vMb, out.uvStride, mblim, lim, hev, 1, true);
							}
							if (!skipInternal) {
								for (int32 e = 4; e <= 12; e += 4)
									Vp8FilterVertEdge(yMb + e, out.yStride, blim, lim, hev, 2, false);
								Vp8FilterVertEdge(uMb + 4, out.uvStride, blim, lim, hev, 1, false);
								Vp8FilterVertEdge(vMb + 4, out.uvStride, blim, lim, hev, 1, false);
							}
							if (r > 0) {
								Vp8FilterHorizEdge(yMb, out.yStride, mblim, lim, hev, 2, true);
								Vp8FilterHorizEdge(uMb, out.uvStride, mblim, lim, hev, 1, true);
								Vp8FilterHorizEdge(vMb, out.uvStride, mblim, lim, hev, 1, true);
							}
							if (!skipInternal) {
								for (int32 e = 4; e <= 12; e += 4)
									Vp8FilterHorizEdge(yMb + (int64)e * out.yStride, out.yStride,
													   blim, lim, hev, 2, false);
								Vp8FilterHorizEdge(uMb + (int64)4 * out.uvStride, out.uvStride,
												   blim, lim, hev, 1, false);
								Vp8FilterHorizEdge(vMb + (int64)4 * out.uvStride, out.uvStride,
												   blim, lim, hev, 1, false);
							}
						} else { // filtre SIMPLE (luma uniquement)
							if (c > 0)
								Vp8SimpleFilterEdge(yMb, out.yStride, mblim, true);
							if (!skipInternal)
								for (int32 e = 4; e <= 12; e += 4)
									Vp8SimpleFilterEdge(yMb + e, out.yStride, blim, true);
							if (r > 0)
								Vp8SimpleFilterEdge(yMb, out.yStride, mblim, false);
							if (!skipInternal)
								for (int32 e = 4; e <= 12; e += 4)
									Vp8SimpleFilterEdge(yMb + (int64)e * out.yStride, out.yStride,
														blim, false);
						}
					}
				}
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
