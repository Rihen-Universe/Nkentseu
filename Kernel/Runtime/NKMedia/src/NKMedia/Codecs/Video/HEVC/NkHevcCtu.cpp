// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcCtu.cpp — brique 5 : slice_segment_data() INTRA.
// -----------------------------------------------------------------------------
// Parse la syntaxe CTU complète d'une slice I au-dessus du CABAC (brique 4) :
// sao() (§7.3.8.3), coding_quadtree (§7.3.8.4), coding_unit intra (§7.3.8.5,
// modes luma via MPM §8.4.2 — nécessaires car le SCAN des résidus dépend du mode
// intra), transform_tree (§7.3.8.8), transform_unit + cu_qp_delta (§7.3.8.10),
// residual_coding (§7.3.8.11 — coefficients COMPLETS : last_sig, sous-blocs,
// sig_coeff avec cartes de contexte, greater1/greater2, restes Rice/EGk, signes
// avec sign hiding), et WPP (§9.3.1 : nouvelle init moteur par rangée aux entry
// points + restauration des contextes sauvés après le 2e CTB de la rangée
// au-dessus).
//
// Les coefficients ne sont PAS reconstruits en pixels ici (briques suivantes :
// déquant/transformées/prédiction intra) — la VALIDATION de cette brique est
// STRUCTURELLE, comme les tiles VP9 : si un seul bin était mal décodé quelque
// part, la position des terminaisons (end_of_slice_segment_flag,
// end_of_subset_one_bit) et le compte de CTU seraient faux avec une probabilité
// écrasante. Dérivations de contexte alignées sur la référence ffmpeg
// (libavcodec/hevc/cabac.c, validée bit-exacte) ; tables de scan GÉNÉRÉES par le
// procédé normatif §6.5.3 (vérifié identique aux tables ffmpeg par script).
//
// Restrictions (refus propre, briques suivantes) : slices P/B, tuiles, PCM,
// 4:2:2/4:4:4, transform_skip rotation/contexts (extensions range).
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h"
#include "NKMedia/Codecs/Video/HEVC/NkHevcCabac.h"

namespace nkentseu {
	namespace media {

		namespace {

			int32 Min32(int32 a, int32 b) {
				return a < b ? a : b;
			}
			int32 Max32(int32 a, int32 b) {
				return a > b ? a : b;
			}

			// ---- Tables de scan (générées, procédé normatif §6.5.3) ----------------
			struct ScanTables {
					uint8 diag4x4X[16], diag4x4Y[16];
					uint8 diag4x4Inv[4][4];
					uint8 diag2x2X[4], diag2x2Y[4];
					uint8 diag2x2Inv[2][2];
					uint8 diag8x8X[64], diag8x8Y[64];
					uint8 diag8x8Inv[8][8];
					uint8 rasterX[16], rasterY[16]; // scan horizontal 4x4 (raster)
					uint8 horiz2x2X[4], horiz2x2Y[4];
			};

			void BuildDiag(int32 n, uint8 *sx, uint8 *sy, uint8 *inv /*n*n, [y][x]*/) {
				int32 i = 0, x = 0, y = 0;
				while (true) {
					while (y >= 0) {
						if (x < n && y < n) {
							sx[i] = (uint8)x;
							sy[i] = (uint8)y;
							inv[y * n + x] = (uint8)i;
							++i;
						}
						--y;
						++x;
					}
					y = x;
					x = 0;
					if (i >= n * n)
						break;
				}
			}

			const ScanTables &Scans() {
				static ScanTables t;
				static bool built = false;
				if (!built) {
					BuildDiag(4, t.diag4x4X, t.diag4x4Y, &t.diag4x4Inv[0][0]);
					BuildDiag(2, t.diag2x2X, t.diag2x2Y, &t.diag2x2Inv[0][0]);
					BuildDiag(8, t.diag8x8X, t.diag8x8Y, &t.diag8x8Inv[0][0]);
					for (int32 i = 0; i < 16; ++i) {
						t.rasterX[i] = (uint8)(i & 3);
						t.rasterY[i] = (uint8)(i >> 2);
					}
					t.horiz2x2X[0] = 0;
					t.horiz2x2X[1] = 1;
					t.horiz2x2X[2] = 0;
					t.horiz2x2X[3] = 1;
					t.horiz2x2Y[0] = 0;
					t.horiz2x2Y[1] = 0;
					t.horiz2x2Y[2] = 1;
					t.horiz2x2Y[3] = 1;
					built = true;
				}
				return t;
			}

			// Index composé du scan horizontal sur 8x8 (sous-bloc raster puis intérieur
			// raster) — sert à num_coeff pour les scans horizontal/vertical (trafo 4 et 8).
			int32 HorizComposedIdx(int32 y, int32 x) {
				return (((y >> 2) * 2 + (x >> 2)) << 4) + (y & 3) * 4 + (x & 3);
			}

			// Carte de contexte sig_coeff_flag (§9.3.4.2.5, composée avec le scan —
			// disposition identique à la référence ffmpeg). [scanIdx][5*16].
			const uint8 kSigCtxIdxMap[3][5 * 16] = {
				{
					// SCAN_DIAG
					0, 2, 1, 6, 3, 4, 7, 6, 4, 5, 7, 8, 5, 8, 8, 8, // log2TrafoSize == 2
					1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // prevCsbf == 0
					2, 1, 2, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 0, 0, 0, // prevCsbf == 1
					2, 2, 1, 2, 1, 0, 2, 1, 0, 0, 1, 0, 0, 0, 0, 0, // prevCsbf == 2
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // prevCsbf == 3
				},
				{
					// SCAN_HORIZ
					0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8,
					1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
					2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
					2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0,
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				},
				{
					// SCAN_VERT
					0, 2, 6, 7, 1, 3, 6, 7, 4, 4, 8, 8, 5, 5, 8, 8,
					1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
					2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0,
					2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
					2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				},
			};

			enum { kScanDiag = 0, kScanHoriz = 1, kScanVert = 2 };

			// ---- État du parseur -----------------------------------------------------
			struct CtuParser {
					const NkHevcSps *sps = nullptr;
					const NkHevcPps *pps = nullptr;
					const NkHevcSliceHeader *sh = nullptr;
					NkCabacEngine eng;
					NkHevcCabacState st;
					NkHevcCabacState wppSaved; // état après le 2e CTB de la rangée courante
					bool wppSavedValid = false;

					// Géométrie.
					int32 ctbLog2 = 6, minCbLog2 = 3, minTbLog2 = 2, maxTbLog2 = 5;
					int32 picW = 0, picH = 0;
					int32 picWidthInCtbs = 0, picHeightInCtbs = 0;
					int32 minCbWidth = 0, minCbHeight = 0;
					int32 minPuWidth = 0, minPuHeight = 0; // grille 4x4
					int32 maxTrafoDepthIntra = 0;
					int32 log2MinCuQpDeltaSize = 6;

					// Voisinage.
					NkVector<uint8> ctDepth;	   // par min-CB
					NkVector<uint8> intraModeY;	   // par bloc 4x4
					bool isCuQpDeltaCoded = false; // par groupe de quantification

					// CU courant.
					bool cuTransquantBypass = false;
					bool intraSplit = false;	// part NxN
					int32 curIntraModeY[4] = {1, 1, 1, 1};
					int32 curIntraModeC = 1;

					NkHevcSliceDataStats *stats = nullptr;
					bool okFlag = true; // passe à false sur incohérence/dépassement

					// ---- Primitives CABAC --------------------------------------------
					uint32 Bin(int32 ctxIdx) {
						return eng.DecodeDecision(st.ctx[ctxIdx]);
					}
					uint32 Bypass() {
						return eng.DecodeBypass();
					}
					uint32 BypassBits(int32 n) {
						uint32 v = 0;
						for (int32 i = 0; i < n; ++i)
							v = (v << 1) | Bypass();
						return v;
					}
					uint32 Terminate() {
						return eng.DecodeTerminate();
					}

					// ---- sao() (§7.3.8.3) ---------------------------------------------
					void ParseSao(int32 rx, int32 ry) {
						bool mergeLeft = false, mergeUp = false;
						if (rx > 0)
							mergeLeft = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (!mergeLeft && ry > 0)
							mergeUp = Bin(kHevcCtxSaoMergeFlag) != 0;
						if (mergeLeft || mergeUp)
							return;
						int32 typeLuma = 0, typeChroma = 0;
						for (int32 cIdx = 0; cIdx < 3; ++cIdx) {
							const bool present = (cIdx == 0) ? sh->saoLuma : sh->saoChroma;
							if (!present)
								continue;
							int32 type;
							if (cIdx == 0 || cIdx == 1) {
								if (Bin(kHevcCtxSaoTypeIdx) == 0)
									type = 0;
								else
									type = Bypass() ? 2 : 1; // 1 = bande, 2 = contour
								if (cIdx == 0)
									typeLuma = type;
								else
									typeChroma = type;
							} else {
								type = typeChroma; // cIdx 2 hérite de cIdx 1
							}
							if (type == 0)
								continue;
							const int32 cMax = (1 << (Min32(sps->bitDepthLuma, 10) - 5)) - 1;
							int32 offsetAbs[4];
							for (int32 i = 0; i < 4; ++i) {
								int32 v = 0;
								while (v < cMax && Bypass())
									++v;
								offsetAbs[i] = v;
							}
							if (type == 1) { // bande
								for (int32 i = 0; i < 4; ++i)
									if (offsetAbs[i] != 0)
										Bypass(); // sao_offset_sign
								BypassBits(5);	  // sao_band_position
							} else {
								if (cIdx == 0)
									BypassBits(2); // sao_eo_class_luma
								if (cIdx == 1)
									BypassBits(2); // sao_eo_class_chroma (cIdx 2 hérite)
							}
						}
					}

					// ---- Modes intra (§8.4.2) -----------------------------------------
					int32 GetStoredMode(int32 x, int32 y) const {
						return intraModeY[(usize)((y >> 2) * minPuWidth + (x >> 2))];
					}
					void StoreModes(int32 x, int32 y, int32 size, int32 mode) {
						for (int32 j = 0; j < size; j += 4)
							for (int32 i = 0; i < size; i += 4) {
								const int32 px = (x + i) >> 2, py = (y + j) >> 2;
								if (px < minPuWidth && py < minPuHeight)
									intraModeY[(usize)(py * minPuWidth + px)] = (uint8)mode;
							}
					}

					int32 DeriveIntraMode(int32 xPb, int32 yPb, bool prevFlag, int32 mpmIdx, int32 rem) {
						// candA (gauche) / candB (dessus) — DC si indisponible ; le voisin du
						// dessus hors de la rangée de CTB courante est remplacé par DC (§8.4.2).
						int32 candA = 1, candB = 1;
						if (xPb > 0)
							candA = GetStoredMode(xPb - 1, yPb);
						const int32 ctbTop = (yPb >> ctbLog2) << ctbLog2;
						if (yPb > 0 && (yPb - 1) >= ctbTop)
							candB = GetStoredMode(xPb, yPb - 1);
						int32 mpm[3];
						if (candA == candB) {
							if (candA < 2) {
								mpm[0] = 0;
								mpm[1] = 1;
								mpm[2] = 26;
							} else {
								mpm[0] = candA;
								mpm[1] = 2 + ((candA + 29) % 32);
								mpm[2] = 2 + ((candA - 2 + 1) % 32);
							}
						} else {
							mpm[0] = candA;
							mpm[1] = candB;
							if (candA != 0 && candB != 0)
								mpm[2] = 0;
							else
								mpm[2] = (candA + candB) < 2 ? 26 : 1;
						}
						if (prevFlag)
							return mpm[mpmIdx];
						// tri croissant des 3 MPM puis réinsertion du reste.
						if (mpm[0] > mpm[1]) {
							const int32 t = mpm[0];
							mpm[0] = mpm[1];
							mpm[1] = t;
						}
						if (mpm[0] > mpm[2]) {
							const int32 t = mpm[0];
							mpm[0] = mpm[2];
							mpm[2] = t;
						}
						if (mpm[1] > mpm[2]) {
							const int32 t = mpm[1];
							mpm[1] = mpm[2];
							mpm[2] = t;
						}
						int32 mode = rem;
						for (int32 i = 0; i < 3; ++i)
							if (mode >= mpm[i])
								++mode;
						return mode;
					}

					// ---- residual_coding (§7.3.8.11) ----------------------------------
					// coeff_abs_level_remaining (§9.3.3.13) : préfixe unaire bypass puis
					// suffixe Rice/EGk. La mise à jour du paramètre Rice se fait chez
					// l'APPELANT sur le NIVEAU COMPLET (base + reste), pas sur le reste seul.
					int32 CoeffAbsLevelRemaining(int32 riceParam) {
						int32 prefix = 0;
						while (prefix < 31 && Bypass())
							++prefix;
						if (prefix < 3)
							return (prefix << riceParam) + (int32)BypassBits(riceParam);
						if (prefix >= 31 || (prefix - 3 + riceParam) > 22) {
							okFlag = false;
							return 0;
						}
						const int32 k = prefix - 3 + riceParam;
						return (((1 << (prefix - 3)) + 3 - 1) << riceParam) + (int32)BypassBits(k);
					}

					void ParseResidual(int32 log2TrafoSize, int32 cIdx, int32 predModeIntra) {
						const ScanTables &sc = Scans();
						++stats->tuCount;

						if (pps->transformSkipEnabled && log2TrafoSize == 2 && !cuTransquantBypass)
							Bin(kHevcCtxTransformSkip + (cIdx ? 1 : 0));

						// scanIdx (§7.4.9.11) — intra seulement ici.
						int32 scanIdx = kScanDiag;
						if (log2TrafoSize == 2 || (log2TrafoSize == 3 && cIdx == 0)) {
							if (predModeIntra >= 6 && predModeIntra <= 14)
								scanIdx = kScanVert;
							else if (predModeIntra >= 22 && predModeIntra <= 30)
								scanIdx = kScanHoriz;
						}

						// last_sig_coeff prefixes (TR contextés) + suffixes (bypass).
						const int32 maxPre = (log2TrafoSize << 1) - 1;
						int32 ctxOffset, ctxShift;
						if (cIdx == 0) {
							ctxOffset = 3 * (log2TrafoSize - 2) + ((log2TrafoSize - 1) >> 2);
							ctxShift = (log2TrafoSize + 1) >> 2;
						} else {
							ctxOffset = 15;
							ctxShift = log2TrafoSize - 2;
						}
						int32 lastX = 0, lastY = 0;
						while (lastX < maxPre && Bin(kHevcCtxLastSigCoeffXPrefix + ctxOffset + (lastX >> ctxShift)))
							++lastX;
						while (lastY < maxPre && Bin(kHevcCtxLastSigCoeffYPrefix + ctxOffset + (lastY >> ctxShift)))
							++lastY;
						if (lastX > 3) {
							const int32 nbits = (lastX >> 1) - 1;
							lastX = (1 << nbits) * (2 + (lastX & 1)) + (int32)BypassBits(nbits);
						}
						if (lastY > 3) {
							const int32 nbits = (lastY >> 1) - 1;
							lastY = (1 << nbits) * (2 + (lastY & 1)) + (int32)BypassBits(nbits);
						}
						if (scanIdx == kScanVert) {
							const int32 t = lastX;
							lastX = lastY;
							lastY = t;
						}

						// Sélection des scans + index composé du dernier coefficient.
						const uint8 *scanXOff, *scanYOff, *scanXCg, *scanYCg;
						int32 numCoeff;
						const int32 xCgLast = lastX >> 2, yCgLast = lastY >> 2;
						if (scanIdx == kScanDiag) {
							scanXOff = sc.diag4x4X;
							scanYOff = sc.diag4x4Y;
							numCoeff = sc.diag4x4Inv[lastY & 3][lastX & 3];
							if (log2TrafoSize == 2) {
								static const uint8 one[1] = {0};
								scanXCg = one;
								scanYCg = one;
							} else if (log2TrafoSize == 3) {
								numCoeff += sc.diag2x2Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag2x2X;
								scanYCg = sc.diag2x2Y;
							} else if (log2TrafoSize == 4) {
								numCoeff += sc.diag4x4Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag4x4X;
								scanYCg = sc.diag4x4Y;
							} else {
								numCoeff += sc.diag8x8Inv[yCgLast][xCgLast] << 4;
								scanXCg = sc.diag8x8X;
								scanYCg = sc.diag8x8Y;
							}
						} else if (scanIdx == kScanHoriz) {
							scanXOff = sc.rasterX;
							scanYOff = sc.rasterY;
							scanXCg = sc.horiz2x2X;
							scanYCg = sc.horiz2x2Y;
							numCoeff = HorizComposedIdx(lastY, lastX);
						} else { // vertical = horizontal transposé
							scanXOff = sc.rasterY;
							scanYOff = sc.rasterX;
							scanXCg = sc.horiz2x2Y;
							scanYCg = sc.horiz2x2X;
							numCoeff = HorizComposedIdx(lastX, lastY);
						}
						++numCoeff;
						const int32 numLastSubset = (numCoeff - 1) >> 4;

						uint8 sbFlag[8][8] = {{0}};
						int32 greater1Ctx = 1;

						for (int32 i = numLastSubset; i >= 0; --i) {
							const int32 xCg = scanXCg[i], yCg = scanYCg[i];
							int32 implicitNonZero = 0;
							uint8 sigIdx[16];
							int32 nbSig = 0;
							const int32 offset = i << 4;

							if (i < numLastSubset && i > 0) {
								int32 ctxCg = 0;
								if (xCg < (1 << (log2TrafoSize - 2)) - 1)
									ctxCg += sbFlag[xCg + 1][yCg];
								if (yCg < (1 << (log2TrafoSize - 2)) - 1)
									ctxCg += sbFlag[xCg][yCg + 1];
								sbFlag[xCg][yCg] = (uint8)Bin(kHevcCtxSigCoeffGroupFlag + Min32(ctxCg, 1) +
															 (cIdx ? 2 : 0));
								implicitNonZero = 1;
							} else {
								sbFlag[xCg][yCg] =
									(uint8)((xCg == xCgLast && yCg == yCgLast) || (xCg == 0 && yCg == 0));
							}

							const int32 lastScanPos = numCoeff - offset - 1;
							int32 nEnd;
							if (i == numLastSubset) {
								nEnd = lastScanPos - 1;
								sigIdx[0] = (uint8)lastScanPos;
								nbSig = 1;
							} else {
								nEnd = 15;
							}

							int32 prevCsbf = 0;
							if (xCg < ((1 << log2TrafoSize) - 1) >> 2)
								prevCsbf = sbFlag[xCg + 1][yCg] ? 1 : 0;
							if (yCg < ((1 << log2TrafoSize) - 1) >> 2)
								prevCsbf += sbFlag[xCg][yCg + 1] ? 2 : 0;

							if (sbFlag[xCg][yCg] && nEnd >= 0) {
								// sig_coeff_flag — même dérivation que la référence ffmpeg.
								const uint8 *map;
								int32 scfOffset = (cIdx != 0) ? 27 : 0;
								if (log2TrafoSize == 2) {
									map = &kSigCtxIdxMap[scanIdx][0];
								} else {
									map = &kSigCtxIdxMap[scanIdx][(prevCsbf + 1) << 4];
									if (cIdx == 0) {
										if (xCg > 0 || yCg > 0)
											scfOffset += 3;
										if (log2TrafoSize == 3)
											scfOffset += (scanIdx == kScanDiag) ? 9 : 15;
										else
											scfOffset += 21;
									} else {
										scfOffset += (log2TrafoSize == 3) ? 9 : 12;
									}
								}
								const int32 nb0 = nbSig;
								for (int32 n = nEnd; n > 0; --n) {
									const int32 sig = (int32)Bin(kHevcCtxSigCoeffFlag + map[n] + scfOffset);
									sigIdx[nbSig] = (uint8)n;
									nbSig += sig;
								}
								if (nbSig != nb0)
									implicitNonZero = 0;
								if (implicitNonZero == 0) {
									int32 dcOffset;
									if (i == 0)
										dcOffset = (cIdx == 0) ? 0 : 27;
									else
										dcOffset = 2 + scfOffset;
									sigIdx[nbSig] = 0;
									nbSig += (int32)Bin(kHevcCtxSigCoeffFlag + dcOffset);
								} else {
									sigIdx[nbSig] = 0;
									++nbSig;
								}
							}

							if (nbSig <= 0)
								continue;
							stats->nonZeroCoeffs += nbSig;

							// greater1 (max 8), greater2 (1er greater1), signes, restes Rice.
							int32 ctxSet = (i > 0 && cIdx == 0) ? 2 : 0;
							if (i != numLastSubset && greater1Ctx == 0)
								++ctxSet;
							greater1Ctx = 1;
							uint8 g1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
							int32 firstG1 = -1;
							const int32 nG1 = Min32(nbSig, 8);
							for (int32 m = 0; m < nG1; ++m) {
								const int32 inc = (ctxSet << 2) + greater1Ctx + (cIdx ? 16 : 0);
								const int32 flag = (int32)Bin(kHevcCtxCoeffAbsGreater1 + inc);
								g1[m] = (uint8)flag;
								if (flag) {
									if (firstG1 < 0)
										firstG1 = m;
									greater1Ctx = 0;
								} else if (greater1Ctx >= 1 && greater1Ctx < 3) {
									++greater1Ctx;
								}
							}
							const int32 lastNzPos = sigIdx[0];
							const int32 firstNzPos = sigIdx[nbSig - 1];
							const bool signHidden = !cuTransquantBypass && (lastNzPos - firstNzPos >= 4);
							if (firstG1 >= 0)
								g1[firstG1] = (uint8)(g1[firstG1] +
													  Bin(kHevcCtxCoeffAbsGreater2 + ctxSet + (cIdx ? 4 : 0)));
							const int32 nbSigns =
								(pps->signDataHiding && signHidden) ? (nbSig - 1) : nbSig;
							uint32 signBits = BypassBits(nbSigns);
							(void)signBits;

							int32 riceParam = 0;
							int32 sumAbs = 0;
							for (int32 m = 0; m < nbSig; ++m) {
								nk_int64 level;
								if (m < 8) {
									level = 1 + g1[m];
									const nk_int64 escape = (m == firstG1) ? 3 : 2;
									if (level == escape) {
										level += CoeffAbsLevelRemaining(riceParam);
										if (level > (3 << riceParam))
											riceParam = Min32(riceParam + 1, 4);
									}
								} else {
									level = 1 + CoeffAbsLevelRemaining(riceParam);
									if (level > (3 << riceParam))
										riceParam = Min32(riceParam + 1, 4);
								}
								sumAbs += (int32)level;
								if (!okFlag)
									return;
							}
							(void)sumAbs; // parité utilisée par le sign hiding à la brique reconstruction
						}
					}

					// ---- transform_tree / transform_unit (§7.3.8.8/10) ----------------
					void ParseTransformTree(int32 x0, int32 y0, int32 xBase, int32 yBase, int32 log2Size,
											int32 depth, int32 blkIdx, bool parentCbfCb, bool parentCbfCr) {
						if (!okFlag)
							return;
						bool split;
						const int32 maxDepth = maxTrafoDepthIntra + (intraSplit ? 1 : 0);
						if (log2Size <= maxTbLog2 && log2Size > minTbLog2 && depth < maxDepth &&
							!(intraSplit && depth == 0)) {
							split = Bin(kHevcCtxSplitTransform + 5 - log2Size) != 0;
						} else {
							split = (log2Size > maxTbLog2) || (intraSplit && depth == 0);
						}
						bool cbfCb = parentCbfCb, cbfCr = parentCbfCr;
						if (log2Size > 2) { // 4:2:0
							if (depth == 0 || parentCbfCb)
								cbfCb = Bin(kHevcCtxCbfCbCr + depth) != 0;
							else
								cbfCb = false;
							if (depth == 0 || parentCbfCr)
								cbfCr = Bin(kHevcCtxCbfCbCr + depth) != 0;
							else
								cbfCr = false;
						}
						if (split) {
							const int32 h = 1 << (log2Size - 1);
							ParseTransformTree(x0, y0, x0, y0, log2Size - 1, depth + 1, 0, cbfCb, cbfCr);
							ParseTransformTree(x0 + h, y0, x0, y0, log2Size - 1, depth + 1, 1, cbfCb, cbfCr);
							ParseTransformTree(x0, y0 + h, x0, y0, log2Size - 1, depth + 1, 2, cbfCb, cbfCr);
							ParseTransformTree(x0 + h, y0 + h, x0, y0, log2Size - 1, depth + 1, 3, cbfCb,
											   cbfCr);
							return;
						}
						// Feuille : cbf_luma (toujours signalé en intra) puis transform_unit.
						const bool cbfLuma = Bin(kHevcCtxCbfLuma + (depth == 0 ? 1 : 0)) != 0;
						if (cbfLuma || cbfCb || cbfCr) {
							if (pps->cuQpDeltaEnabled && !isCuQpDeltaCoded) {
								// cu_qp_delta_abs : préfixe TR contexté (bin0 ctx+0, suite ctx+1,
								// cMax 5) + suffixe EG0 bypass, puis signe bypass si non nul.
								int32 prefix = 0, inc = 0;
								while (prefix < 5 && Bin(kHevcCtxCuQpDelta + inc)) {
									++prefix;
									inc = 1;
								}
								int32 v = prefix;
								if (prefix >= 5) {
									int32 k = 0;
									int32 suffix = 0;
									while (k < 7 && Bypass()) {
										suffix += 1 << k;
										++k;
									}
									if (k == 7) {
										okFlag = false;
										return;
									}
									int32 kk = k;
									while (kk--)
										suffix += (int32)Bypass() << kk;
									v += suffix;
								}
								if (v != 0)
									Bypass(); // cu_qp_delta_sign_flag
								isCuQpDeltaCoded = true;
								++stats->qpDeltaCount;
							}
							// Mode intra du bloc luma courant (pour le scan) : partition
							// couvrante (NxN -> quadrant de xBase/x0).
							int32 lumaMode = curIntraModeY[0];
							if (intraSplit)
								lumaMode = curIntraModeY[blkIdx];
							if (cbfLuma)
								ParseResidual(log2Size, 0, lumaMode);
							if (log2Size > 2) {
								if (cbfCb)
									ParseResidual(log2Size - 1, 1, curIntraModeC);
								if (cbfCr)
									ParseResidual(log2Size - 1, 2, curIntraModeC);
							} else if (blkIdx == 3) {
								if (parentCbfCb)
									ParseResidual(2, 1, curIntraModeC);
								if (parentCbfCr)
									ParseResidual(2, 2, curIntraModeC);
							}
						}
					}

					// ---- coding_unit intra (§7.3.8.5) ---------------------------------
					void ParseCodingUnit(int32 x0, int32 y0, int32 log2CbSize) {
						if (!okFlag)
							return;
						++stats->cuCount;
						cuTransquantBypass = false;
						if (pps->transquantBypassEnabled)
							cuTransquantBypass = Bin(kHevcCtxCuTransquantBypass) != 0;
						intraSplit = false;
						if (log2CbSize == minCbLog2) {
							// part_mode intra : 1 bin (1 = 2Nx2N, 0 = NxN).
							if (Bin(kHevcCtxPartMode) == 0)
								intraSplit = true;
						}
						const int32 nParts = intraSplit ? 4 : 1;
						const int32 pbSize = (1 << log2CbSize) >> (intraSplit ? 1 : 0);
						bool prevFlag[4] = {false, false, false, false};
						int32 mpmIdx[4] = {0, 0, 0, 0};
						int32 rem[4] = {0, 0, 0, 0};
						for (int32 p = 0; p < nParts; ++p)
							prevFlag[p] = Bin(kHevcCtxPrevIntraLumaPred) != 0;
						for (int32 p = 0; p < nParts; ++p) {
							if (prevFlag[p]) {
								int32 v = 0;
								while (v < 2 && Bypass())
									++v;
								mpmIdx[p] = v;
							} else {
								rem[p] = (int32)BypassBits(5);
							}
						}
						for (int32 p = 0; p < nParts; ++p) {
							const int32 xPb = x0 + (p & 1) * pbSize;
							const int32 yPb = y0 + (p >> 1) * pbSize;
							const int32 mode = DeriveIntraMode(xPb, yPb, prevFlag[p], mpmIdx[p], rem[p]);
							curIntraModeY[p] = mode;
							StoreModes(xPb, yPb, pbSize, mode);
						}
						// intra_chroma_pred_mode (4:2:0 : un seul pour le CU).
						if (Bin(kHevcCtxIntraChromaPredMode) == 0) {
							curIntraModeC = curIntraModeY[0]; // DM
						} else {
							static const int32 kChromaModes[4] = {0, 26, 10, 1};
							const int32 idx = (int32)BypassBits(2);
							curIntraModeC = kChromaModes[idx];
							if (curIntraModeC == curIntraModeY[0])
								curIntraModeC = 34;
						}
						ParseTransformTree(x0, y0, x0, y0, log2CbSize, 0, 0, false, false);
					}

					// ---- coding_quadtree (§7.3.8.4) -----------------------------------
					void ParseCodingQuadtree(int32 x0, int32 y0, int32 log2CbSize, int32 depth) {
						if (!okFlag)
							return;
						const int32 size = 1 << log2CbSize;
						bool split;
						if (x0 + size <= picW && y0 + size <= picH && log2CbSize > minCbLog2) {
							int32 inc = 0;
							const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
							if (x0 > 0 && ctDepth[(usize)(yCb * minCbWidth + xCb - 1)] > depth)
								++inc;
							if (y0 > 0 && ctDepth[(usize)((yCb - 1) * minCbWidth + xCb)] > depth)
								++inc;
							split = Bin(kHevcCtxSplitCuFlag + inc) != 0;
						} else {
							split = log2CbSize > minCbLog2; // hors image : split forcé si possible
						}
						if (pps->cuQpDeltaEnabled && log2CbSize >= log2MinCuQpDeltaSize)
							isCuQpDeltaCoded = false; // nouveau groupe de quantification
						if (split) {
							const int32 h = size >> 1;
							if (x0 < picW && y0 < picH)
								ParseCodingQuadtree(x0, y0, log2CbSize - 1, depth + 1);
							if (x0 + h < picW && y0 < picH)
								ParseCodingQuadtree(x0 + h, y0, log2CbSize - 1, depth + 1);
							if (x0 < picW && y0 + h < picH)
								ParseCodingQuadtree(x0, y0 + h, log2CbSize - 1, depth + 1);
							if (x0 + h < picW && y0 + h < picH)
								ParseCodingQuadtree(x0 + h, y0 + h, log2CbSize - 1, depth + 1);
							return;
						}
						// Feuille : mémorise la profondeur pour le contexte des voisins.
						const int32 xCb = x0 >> minCbLog2, yCb = y0 >> minCbLog2;
						const int32 nMin = size >> minCbLog2;
						for (int32 j = 0; j < nMin; ++j)
							for (int32 i = 0; i < nMin; ++i) {
								const int32 px = xCb + i, py = yCb + j;
								if (px < minCbWidth && py < minCbHeight)
									ctDepth[(usize)(py * minCbWidth + px)] = (uint8)depth;
							}
						ParseCodingUnit(x0, y0, log2CbSize);
					}
			};

		} // namespace

		bool NkHevcDecoder::ParseSliceDataIntra(const uint8 *nal, usize size, const NkHevcSps &sps,
												const NkHevcPps &pps, const NkHevcSliceHeader &sh,
												NkHevcSliceDataStats &out) {
			out = NkHevcSliceDataStats{};
			if (!nal || size < 4 || !sps.valid || !pps.valid || !sh.valid)
				return false;
			if (sh.sliceType != kHevcSliceI || sh.dependentSliceSegment || !sh.firstSliceSegmentInPic)
				return false; // P/B et slices multiples : briques suivantes
			if (pps.tilesEnabled || sps.pcmEnabled || sps.chromaFormatIdc != 1 || sps.separateColourPlane)
				return false;

			// RBSP dé-émulé + positions des octets retirés (pour convertir les entry
			// points, exprimés en octets du flux ÉMULÉ, §7.4.7.1).
			NkVector<uint8> rbsp;
			NkVector<uint32> removed; // indices (domaine émulé, après en-tête NAL 2 octets)
			{
				const uint8 *src = nal + 2;
				const usize n = size - 2;
				for (usize i = 0; i < n; ++i) {
					if (i + 2 < n && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
						rbsp.PushBack(0);
						rbsp.PushBack(0);
						removed.PushBack((uint32)(i + 2));
						i += 2;
					} else {
						rbsp.PushBack(src[i]);
					}
				}
			}
			// deemulé -> émulé : décale d'un octet par 0x03 retiré situé avant.
			auto deemToEm = [&](usize d) -> usize {
				usize em = d;
				for (uint64 k = 0; k < removed.Size(); ++k)
					if ((usize)removed[k] <= em)
						++em;
					else
						break;
				return em;
			};
			auto emToDeem = [&](usize em) -> usize {
				usize cnt = 0;
				for (uint64 k = 0; k < removed.Size(); ++k)
					if ((usize)removed[k] < em)
						++cnt;
					else
						break;
				return em - cnt;
			};

			CtuParser p;
			p.sps = &sps;
			p.pps = &pps;
			p.sh = &sh;
			p.stats = &out;
			p.ctbLog2 = sps.log2MinCbSizeY + sps.log2DiffMaxMinCbSizeY;
			p.minCbLog2 = sps.log2MinCbSizeY;
			p.minTbLog2 = sps.log2MinTbSizeY;
			p.maxTbLog2 = sps.log2MinTbSizeY + sps.log2DiffMaxMinTbSizeY;
			p.maxTrafoDepthIntra = sps.maxTransformHierarchyDepthIntra;
			p.picW = sps.width;
			p.picH = sps.height;
			const int32 ctbSize = 1 << p.ctbLog2;
			p.picWidthInCtbs = (p.picW + ctbSize - 1) >> p.ctbLog2;
			p.picHeightInCtbs = (p.picH + ctbSize - 1) >> p.ctbLog2;
			p.minCbWidth = (p.picW + (1 << p.minCbLog2) - 1) >> p.minCbLog2;
			p.minCbHeight = (p.picH + (1 << p.minCbLog2) - 1) >> p.minCbLog2;
			p.minPuWidth = (p.picW + 3) >> 2;
			p.minPuHeight = (p.picH + 3) >> 2;
			p.log2MinCuQpDeltaSize = p.ctbLog2 - pps.diffCuQpDeltaDepth;
			p.ctDepth.Resize((usize)(p.minCbWidth * p.minCbHeight));
			p.intraModeY.Resize((usize)(p.minPuWidth * p.minPuHeight));
			for (uint64 i = 0; i < p.intraModeY.Size(); ++i)
				p.intraModeY[i] = 1; // DC par défaut
			const int32 picSizeInCtbs = p.picWidthInCtbs * p.picHeightInCtbs;

			// WPP : rangées attendues et offsets de sous-ensembles (dé-émulés).
			const bool wpp = pps.entropyCodingSyncEnabled;
			if (wpp && sh.numEntryPointOffsets != p.picHeightInCtbs - 1)
				return false;
			const usize emDataStart = deemToEm(sh.dataByteOffset);

			// Init CABAC de la 1re rangée.
			p.st.Init(sh.sliceQp, NkHevcCabacState::InitTypeFor(sh.sliceType, sh.cabacInit));
			p.eng.InitEngine(rbsp.Data(), (usize)rbsp.Size(), sh.dataByteOffset);
			if (p.eng.codIOffset >= p.eng.codIRange)
				return false;

			int32 ctbAddr = 0;
			usize emSubsetStart = emDataStart;
			while (ctbAddr < picSizeInCtbs) {
				const int32 rx = ctbAddr % p.picWidthInCtbs;
				const int32 ry = ctbAddr / p.picWidthInCtbs;

				if (sh.saoLuma || sh.saoChroma)
					p.ParseSao(rx, ry);
				p.ParseCodingQuadtree(rx << p.ctbLog2, ry << p.ctbLog2, p.ctbLog2, 0);
				if (!p.okFlag)
					return false;

				// WPP : sauvegarde de l'état des contextes après le 2e CTB de la rangée.
				if (wpp && rx == Min32(1, p.picWidthInCtbs - 1)) {
					p.wppSaved = p.st;
					p.wppSavedValid = true;
				}

				const uint32 endOfSlice = p.Terminate();
				++out.ctusParsed;
				++ctbAddr;
				const bool lastCtb = (ctbAddr == picSizeInCtbs);
				if (endOfSlice != (lastCtb ? 1u : 0u))
					return false; // terminaison au mauvais endroit = désynchronisation

				if (!lastCtb && wpp && (ctbAddr % p.picWidthInCtbs) == 0) {
					// Fin de rangée : end_of_subset_one_bit (obligatoirement 1), puis
					// nouvelle init moteur au point d'entrée suivant + contextes restaurés.
					if (p.Terminate() != 1)
						return false;
					const int32 row = ctbAddr / p.picWidthInCtbs; // rangée qui COMMENCE
					emSubsetStart += (usize)sh.entryPointOffsets[(usize)(row - 1)];
					const usize deemStart = emToDeem(emSubsetStart);
					// Écart consommation vs entry point (le moteur peut avoir prélu ≤2 octets).
					nk_int64 dev = (nk_int64)deemStart - (nk_int64)p.eng.bytePos;
					if (dev < 0)
						dev = -dev;
					if ((int32)dev > out.maxSubsetDeviation)
						out.maxSubsetDeviation = (int32)dev;
					if (deemStart >= (usize)rbsp.Size())
						return false;
					if (p.wppSavedValid)
						p.st = p.wppSaved;
					else
						p.st.Init(sh.sliceQp, NkHevcCabacState::InitTypeFor(sh.sliceType, sh.cabacInit));
					p.eng.InitEngine(rbsp.Data(), (usize)rbsp.Size(), deemStart);
					if (p.eng.codIOffset >= p.eng.codIRange)
						return false;
					++out.rows;
				}
			}
			out.rows += 1; // la dernière rangée (pas de end_of_subset après elle)
			return out.ctusParsed == picSizeInCtbs;
		}

	} // namespace media
} // namespace nkentseu
