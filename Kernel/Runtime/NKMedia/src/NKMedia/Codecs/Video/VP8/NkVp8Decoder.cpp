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

	} // namespace media
} // namespace nkentseu
