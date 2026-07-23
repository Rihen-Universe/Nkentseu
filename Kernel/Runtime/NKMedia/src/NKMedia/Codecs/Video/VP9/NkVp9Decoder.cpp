// =============================================================================
// NKMedia/Codecs/Video/VP9/NkVp9Decoder.cpp — briques 1-3 :
//   1) superframes + en-tête de trame non compressé (§6.2 / Annexe B) ;
//   2) en-tête COMPRESSÉ (§6.3) : bool decoder (identique VP8, MAIS avec un bit
//      marqueur initial) + contexte d'entropie + mises à jour subexp ;
//   3) CONTENU de trame clé : tiles → partitions récursives 64→4 → modes intra
//      kf → tx_size/skip/segment → tokens résiduels (sans reconstruction).
// Ordres vérifiés contre vp9_decodeframe.c / vp9_decodemv.c / vp9_detokenize.c
// (réécrit, zéro code importé). Tables : NkVp9Tables.inc GÉNÉRÉ (vp9ref/extract.py).
// =============================================================================
#include "NKMedia/Codecs/Video/VP9/NkVp9Decoder.h"
#include "NKMedia/Codecs/Video/VP9/NkVp9Itxfm.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8BoolDecoder.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {

#include "NkVp9Tables.inc"

			// --- Lecteur de bits MSB-first (vpx_read_bit_buffer) pour l'en-tête ---
			// non compressé. L'en-tête compressé (briques suivantes) utilisera le
			// bool decoder VP8/VP9 (identique à NkVp8BoolDecoder).
			struct BitReader {
					const uint8 *data = nullptr;
					usize size = 0;
					usize bitPos = 0;
					bool error = false;

					int32 Bit() {
						const usize byteIdx = bitPos >> 3;
						if (byteIdx >= size) {
							error = true;
							return 0;
						}
						const int32 b = (data[byteIdx] >> (7 - (bitPos & 7))) & 1;
						++bitPos;
						return b;
					}
					int32 Literal(int32 nBits) {
						int32 v = 0;
						for (int32 i = 0; i < nBits; ++i)
							v = (v << 1) | Bit();
						return v;
					}
					// Littéral signé : magnitude puis bit de signe (vpx_rb_read_signed_literal).
					int32 SignedLiteral(int32 nBits) {
						const int32 v = Literal(nBits);
						return Bit() ? -v : v;
					}
			};

			// Nombre de bits nécessaires pour coder [0, max] (get_unsigned_bits).
			int32 UnsignedBits(int32 max) {
				int32 n = 0;
				while (max > 0) {
					++n;
					max >>= 1;
				}
				return n;
			}

			// width_minus_1/height_minus_1 sur 16 bits (vp9_read_frame_size).
			void ReadFrameSize(BitReader &rb, int32 &w, int32 &h) {
				w = rb.Literal(16) + 1;
				h = rb.Literal(16) + 1;
			}

			void ReadRenderSize(BitReader &rb, NkVp9FrameHeader &hdr) {
				hdr.renderWidth = hdr.width;
				hdr.renderHeight = hdr.height;
				if (rb.Bit())
					ReadFrameSize(rb, hdr.renderWidth, hdr.renderHeight);
			}

			// frame_sync_code = 0x49 0x83 0x42 (§6.2.2).
			bool ReadSyncCode(BitReader &rb) {
				const int32 a = rb.Literal(8);
				const int32 b = rb.Literal(8);
				const int32 c = rb.Literal(8);
				return a == 0x49 && b == 0x83 && c == 0x42;
			}

			// color_config (§6.2.3, read_bitdepth_colorspace_sampling).
			bool ReadColorConfig(BitReader &rb, NkVp9FrameHeader &hdr) {
				if (hdr.profile >= 2)
					hdr.bitDepth = rb.Bit() ? 12 : 10;
				else
					hdr.bitDepth = 8;
				hdr.colorSpace = rb.Literal(3);
				if (hdr.colorSpace != 7) { // != sRGB
					hdr.colorRangeFull = rb.Bit() != 0;
					if (hdr.profile == 1 || hdr.profile == 3) {
						hdr.subsamplingX = rb.Bit();
						hdr.subsamplingY = rb.Bit();
						if (hdr.subsamplingX == 1 && hdr.subsamplingY == 1)
							return false; // 4:2:0 interdit en profil 1/3
						if (rb.Bit())
							return false; // reserved != 0
					} else {
						hdr.subsamplingX = 1;
						hdr.subsamplingY = 1;
					}
				} else {
					// sRGB : profil 1/3 seulement, 4:4:4.
					if (hdr.profile == 1 || hdr.profile == 3) {
						hdr.subsamplingX = 0;
						hdr.subsamplingY = 0;
						if (rb.Bit())
							return false; // reserved != 0
					} else {
						return false;
					}
				}
				return true;
			}

			// setup_loopfilter (§6.2.8).
			void ReadLoopFilter(BitReader &rb, NkVp9FrameHeader &hdr) {
				hdr.lfLevel = rb.Literal(6);
				hdr.lfSharpness = rb.Literal(3);
				hdr.lfDeltaEnabled = rb.Bit() != 0;
				if (hdr.lfDeltaEnabled) {
					if (rb.Bit()) { // delta update
						for (int32 i = 0; i < 4; ++i)
							if (rb.Bit())
								hdr.lfRefDeltas[i] = rb.SignedLiteral(6);
						for (int32 i = 0; i < 2; ++i)
							if (rb.Bit())
								hdr.lfModeDeltas[i] = rb.SignedLiteral(6);
					}
				}
			}

			// read_delta_q : flag + littéral signé 4 bits.
			int32 ReadDeltaQ(BitReader &rb) {
				return rb.Bit() ? rb.SignedLiteral(4) : 0;
			}

			// setup_quantization (§6.2.9).
			void ReadQuantization(BitReader &rb, NkVp9FrameHeader &hdr) {
				hdr.baseQIdx = rb.Literal(8);
				hdr.deltaQYDc = ReadDeltaQ(rb);
				hdr.deltaQUvDc = ReadDeltaQ(rb);
				hdr.deltaQUvAc = ReadDeltaQ(rb);
				hdr.lossless = (hdr.baseQIdx == 0 && hdr.deltaQYDc == 0 && hdr.deltaQUvDc == 0 &&
								hdr.deltaQUvAc == 0);
			}

			// setup_segmentation (§6.2.10). Bornes/signe par feature :
			// ALT_Q max 255 signé, ALT_LF max 63 signé, REF_FRAME max 3 non signé,
			// SKIP max 0 non signé (0 bit de donnée).
			void ReadSegmentation(BitReader &rb, NkVp9FrameHeader &hdr) {
				static const int32 kFeatureMax[4] = {255, 63, 3, 0};
				static const bool kFeatureSigned[4] = {true, true, false, false};
				hdr.segEnabled = rb.Bit() != 0;
				if (!hdr.segEnabled)
					return;
				hdr.segUpdateMap = rb.Bit() != 0;
				if (hdr.segUpdateMap) {
					for (int32 i = 0; i < 7; ++i)
						hdr.segTreeProbs[i] = rb.Bit() ? (uint8)rb.Literal(8) : (uint8)255;
					hdr.segTemporalUpdate = rb.Bit() != 0;
					for (int32 i = 0; i < 3; ++i)
						hdr.segPredProbs[i] =
							(hdr.segTemporalUpdate && rb.Bit()) ? (uint8)rb.Literal(8) : (uint8)255;
				}
				hdr.segUpdateData = rb.Bit() != 0;
				if (hdr.segUpdateData) {
					hdr.segAbsDelta = rb.Bit() != 0;
					for (int32 i = 0; i < 8; ++i) {
						for (int32 j = 0; j < 4; ++j) {
							int32 data = 0;
							const bool enabled = rb.Bit() != 0;
							if (enabled) {
								data = rb.Literal(UnsignedBits(kFeatureMax[j]));
								if (kFeatureSigned[j] && rb.Bit())
									data = -data;
							}
							hdr.segFeatureEnabled[i][j] = enabled;
							hdr.segFeatureData[i][j] = data;
						}
					}
				}
			}

			// setup_tile_info (§6.2.11) : bornes dérivées de la largeur en superblocs 64.
			void ReadTileInfo(BitReader &rb, NkVp9FrameHeader &hdr) {
				const int32 miCols = (hdr.width + 7) >> 3;
				const int32 sb64Cols = ((miCols + 7) & ~7) >> 3;
				int32 minLog2 = 0;
				while ((64 << minLog2) < sb64Cols)
					++minLog2;
				int32 maxLog2 = 1;
				while ((sb64Cols >> maxLog2) >= 4)
					++maxLog2;
				--maxLog2;

				hdr.tileColsLog2 = minLog2;
				int32 ones = maxLog2 - minLog2;
				while (ones-- > 0 && rb.Bit())
					++hdr.tileColsLog2;

				hdr.tileRowsLog2 = rb.Bit();
				if (hdr.tileRowsLog2)
					hdr.tileRowsLog2 += rb.Bit();
			}

			// --- Brique 2 : primitives de mise à jour de probabilités (§6.3) ---

			constexpr int32 kDiffUpdateProb = 252; // DIFF_UPDATE_PROB
			constexpr int32 kMvUpdateProb = 252;   // MV_UPDATE_PROB

			// inv_recenter_nonneg (vp9_dsubexp.c).
			int32 InvRecenterNonneg(int32 v, int32 m) {
				if (v > 2 * m)
					return v;
				return (v & 1) ? m - ((v + 1) >> 1) : m + (v >> 1);
			}

			// decode_uniform : littéral 7 bits, étendu d'un bit au-delà de m=65.
			int32 DecodeUniform(NkVp8BoolDecoder &bd) {
				const int32 m = (1 << 8) - 191; // 65
				const int32 v = (int32)bd.GetLiteral(7);
				return v < m ? v : (v << 1) - m + bd.GetFlag();
			}

			// decode_term_subexp : delta en 4 tranches (4b, 4b+16, 5b+32, uniforme+64).
			int32 DecodeTermSubexp(NkVp8BoolDecoder &bd) {
				if (!bd.GetFlag())
					return (int32)bd.GetLiteral(4);
				if (!bd.GetFlag())
					return (int32)bd.GetLiteral(4) + 16;
				if (!bd.GetFlag())
					return (int32)bd.GetLiteral(5) + 32;
				return DecodeUniform(bd) + 64;
			}

			// inv_remap_prob : delta décodé + proba courante → nouvelle proba.
			uint8 InvRemapProb(int32 v, int32 m) {
				v = (int32)kVp9InvMapTable[v];
				--m;
				if ((m << 1) <= 255)
					return (uint8)(1 + InvRecenterNonneg(v, m));
				return (uint8)(255 - InvRecenterNonneg(v, 255 - 1 - m));
			}

			// vp9_diff_update_prob : flag (proba 252) puis delta subexp remappé.
			void DiffUpdateProb(NkVp8BoolDecoder &bd, uint8 &p) {
				if (bd.GetBool(kDiffUpdateProb)) {
					const int32 delp = DecodeTermSubexp(bd);
					p = InvRemapProb(delp, (int32)p);
				}
			}

			// update_mv_probs : flag (proba 252) puis 7 bits → proba impaire.
			void MvUpdateProbs(NkVp8BoolDecoder &bd, uint8 *p, int32 n) {
				for (int32 i = 0; i < n; ++i)
					if (bd.GetBool(kMvUpdateProb))
						p[i] = (uint8)(((int32)bd.GetLiteral(7) << 1) | 1);
			}

			// =================================================================
			// BRIQUE 3 — contenu de trame clé (partitions, modes, tokens).
			// =================================================================

			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}

			constexpr int32 kBlock8x8 = 3;	 // BLOCK_8X8
			constexpr int32 kBlock64x64 = 12;
			constexpr int32 kMiBlockSize = 8; // unités mi (8x8) par superbloc 64

			// Une cellule de la grille MODE_INFO (par unité mi 8x8). Les blocs plus
			// grands COPIENT leur cellule sur toute leur emprise (équivalent de la
			// propagation de pointeurs de libvpx) — les voisins la consultent.
			// MV entier (1/8 pel) — pas de int_mv 32 bits packé, deux int16 explicites.
			struct SimpleMv {
					int16 row = 0, col = 0;
					bool operator==(const SimpleMv &o) const {
						return row == o.row && col == o.col;
					}
					bool operator!=(const SimpleMv &o) const {
						return !(*this == o);
					}
			};

			struct MiCell {
					uint8 sbType = 0;  // BLOCK_SIZE
					uint8 skip = 0;
					uint8 txSize = 0;
					uint8 mode = 0;	   // mode Y (bloc >= 8x8) ou bmi[3]
					uint8 uvMode = 0;
					uint8 segId = 0;
					uint8 segIdPredicted = 0; // valide seulement si segTemporalUpdate (voisin pour le contexte)
					uint8 bmi[4] = {0, 0, 0, 0}; // sous-modes 4x4 (ou modes b_mode sub8x8 inter)
					// --- Inter (brique 6) ---
					int8 refFrame[2] = {0, -1}; // 0=INTRA_FRAME (défaut : bloc intra), -1=NONE
					uint8 interpFilter = 3;		// SWITCHABLE_FILTERS (sentinelle "non dispo")
					SimpleMv mv[2];				// MV du bloc (>=8x8) par référence
					SimpleMv bmiMv[4][2];		// MV par sous-bloc 4x4 (sb_type<8x8) par référence

					bool IsInter() const {
						return refFrame[0] > 0; // > INTRA_FRAME(0)
					}
					bool HasSecondRef() const {
						return refFrame[1] > 0;
					}
			};

			// État de décodage du contenu (une trame).
			struct FrameParseState {
					const NkVp9FrameHeader *hdr = nullptr;
					const NkVp9FrameContext *fc = nullptr;
					int32 miCols = 0, miRows = 0;
					int32 sbCols = 0; // superblocs (alignement des contextes above)
					MiCell *mi = nullptr;			  // grille miRows × miCols
					uint8 *aboveSegCtx = nullptr;	  // par colonne mi (alignée sb) — contexte de PARTITION
					uint8 leftSegCtx[8] = {0};		  // par rangée mi dans le SB — contexte de PARTITION
					// Carte de SEGMENTATION (segment_id par unité mi, miRows*miCols) — sans
					// rapport avec aboveSegCtx/leftSegCtx ci-dessus (contexte de partition).
					uint8 *currentSegMap = nullptr;  // écrite pendant CETTE trame (set_segment_id)
					const uint8 *lastSegMap = nullptr; // lue depuis la trame PRÉCÉDENTE (dec_get_segment_id) ; nullptr = indisponible (→ 0)
					uint8 *aboveEntCtx[3] = {nullptr, nullptr, nullptr}; // par plane, 2/mi
					uint8 leftEntCtx[3][16] = {{0}};  // par plane, unités 4x4 dans le SB
					// Tile courante.
					int32 tileMiColStart = 0, tileMiColEnd = 0;
					int32 txMode = 0; // du header compressé (TX_MODE_SELECT = 4)
					// Reconstruction (brique 4) — nullptr = parse seul (brique 3).
					uint8 *planes[3] = {nullptr, nullptr, nullptr};
					int32 planeStride[3] = {0, 0, 0};
					int32 planeW[3] = {0, 0, 0}, planeH[3] = {0, 0, 0};
					int32 mbToLeftEdge = 0, mbToTopEdge = 0; // 1/8 pel (négatifs)
					int16 yDequant[8][2] = {{0}}, uvDequant[8][2] = {{0}};
					// Bloc courant.
					MiCell cur;
					const MiCell *aboveMi = nullptr;
					const MiCell *leftMi = nullptr;
					int32 mbToRightEdge = 0, mbToBottomEdge = 0; // en 1/8 pel (×8)
					int32 n4w[3] = {0}, n4h[3] = {0};			 // unités 4x4 du bloc par plane
					int32 aboveOff[3] = {0}, leftOff[3] = {0};	 // offsets contextes entropie
					NkVp9Decoder::NkTileParseStats *stats = nullptr;
					// Compteurs d'adaptation backward (nullptr = pas de comptage, ex. brique 3
					// parse-seul via ParseKeyFrameContent).
					NkVp9FrameCounts *counts = nullptr;

					// --- Inter (brique 6) : nullptr/false = trame clé (comportement inchangé). ---
					bool isInterFrame = false;
					int32 signBias[4] = {0, 0, 0, 0};	 // indexé MV_REFERENCE_FRAME (0=intra inutilisé)
					int32 compFixedRef = 0, compVarRef[2] = {0, 0};
					int32 referenceMode = 0; // NkVp9ReferenceMode
					bool allowHighPrecisionMv = false;
					int32 interpFilterHdr = 4; // SWITCHABLE par défaut
					// Images de référence DÉJÀ décodées (bordure étendue), indexées par
					// MV_REFERENCE_FRAME - 1 (0=LAST,1=GOLDEN,2=ALTREF). Plans Y/U/V avec
					// marge kBorder de chaque côté (origine réelle = plane + border*stride+border).
					const uint8 *refPlanes[3][3] = {{nullptr}}; // [refIdx][plane]
					int32 refStride[3][3] = {{0}};
					// Grille MV de la trame PRÉCÉDENTE (use_prev_frame_mvs) ; nullptr = inutilisée.
					const NkVp9MvRef *prevMvs = nullptr;
					bool usePrevFrameMvs = false;
			};

			constexpr int32 kMcBorder = 96; // marge des plans de référence (>= 16 (UMV) + 64 (bloc max))

			// Scans/voisins par (txSize, txType→variante) : 0=default, 1=row, 2=col.
			void GetScan(int32 txSize, int32 txType, const int16 **scan, const int16 **nb) {
				// tx_type → variante (vp9_scan_orders) : DCT_DCT→def, ADST_DCT→row,
				// DCT_ADST→col, ADST_ADST→def ; 32x32 : toujours default.
				const int32 variant = (txSize == 3) ? 0 : ((txType == 1) ? 1 : (txType == 2) ? 2 : 0);
				switch (txSize) {
					case 0:
						*scan = (variant == 1) ? kVp9RowScan4x4 : (variant == 2) ? kVp9ColScan4x4 : kVp9DefaultScan4x4;
						*nb = (variant == 1) ? kVp9RowScan4x4Neighbors
							  : (variant == 2) ? kVp9ColScan4x4Neighbors
											   : kVp9DefaultScan4x4Neighbors;
						break;
					case 1:
						*scan = (variant == 1) ? kVp9RowScan8x8 : (variant == 2) ? kVp9ColScan8x8 : kVp9DefaultScan8x8;
						*nb = (variant == 1) ? kVp9RowScan8x8Neighbors
							  : (variant == 2) ? kVp9ColScan8x8Neighbors
											   : kVp9DefaultScan8x8Neighbors;
						break;
					case 2:
						*scan = (variant == 1)	 ? kVp9RowScan16x16
								: (variant == 2) ? kVp9ColScan16x16
												 : kVp9DefaultScan16x16;
						*nb = (variant == 1)   ? kVp9RowScan16x16Neighbors
							  : (variant == 2) ? kVp9ColScan16x16Neighbors
											   : kVp9DefaultScan16x16Neighbors;
						break;
					default:
						*scan = kVp9DefaultScan32x32;
						*nb = kVp9DefaultScan32x32Neighbors;
						break;
				}
			}

			// read_coeff : n bits pondérés par probas.
			int32 ReadCoeff(NkVp8BoolDecoder &bd, const uint8 *probs, int32 n) {
				int32 val = 0;
				for (int32 i = 0; i < n; ++i)
					val = (val << 1) | bd.GetBool(probs[i]);
				return val;
			}

			// decode_coefs (vp9_detokenize.c) : décode les tokens d'UN bloc de
			// transformée et écrit les coefficients DÉQUANTIFIÉS dans `dqcoeff`
			// (ordre du scan). dq = {DC, AC} ; dqShift = 1 pour les 32x32. Renvoie l'EOB.
			int32 DecodeCoefs(NkVp8BoolDecoder &bd, const NkVp9FrameContext &fc, int32 planeType,
							  int32 txSize, int32 ctx, const int16 *scan, const int16 *nb,
							  const int16 *dq, int16 *dqcoeff, int32 refIdx,
							  NkVp9FrameCounts *counts) {
				const int32 maxEob = 16 << (txSize << 1);
				const int32 dqShift = (txSize == 3) ? 1 : 0;
				const uint8(*coefProbs)[6][3] = fc.coefProbs[txSize][planeType][refIdx]; // 0=intra,1=inter
				uint32(*coefCounts)[6][4] = counts ? counts->coef[txSize][planeType][refIdx] : nullptr;
				uint32(*eobCounts)[6] = counts ? counts->eobBranch[txSize][planeType][refIdx] : nullptr;
				uint8 tokenCache[32 * 32];
				const uint8 *bandTranslate = (txSize == 0) ? kVp9CoefbandTrans4x4 : kVp9CoefbandTrans8x8Plus;
				int32 dqv = dq[0];
				int32 c = 0;
				while (c < maxEob) {
					int32 band = (int32)bandTranslate[c];
					const uint8 *prob = coefProbs[band][ctx];
					if (eobCounts)
						++eobCounts[band][ctx];
					if (!bd.GetBool(prob[0])) { // EOB
						if (coefCounts)
							++coefCounts[band][ctx][3]; // EOB_MODEL_TOKEN
						break;
					}
					// Suite de zéros.
					while (!bd.GetBool(prob[1])) {
						if (coefCounts)
							++coefCounts[band][ctx][0]; // ZERO_TOKEN
						tokenCache[scan[c]] = 0;
						dqv = dq[1];
						++c;
						if (c >= maxEob)
							return c; // zéros jusqu'au bout (pas de token EOB)
						ctx = (1 + tokenCache[nb[2 * c]] + tokenCache[nb[2 * c + 1]]) >> 1;
						band = (int32)bandTranslate[c];
						prob = coefProbs[band][ctx];
					}
					// Valeur.
					int32 v;
					if (bd.GetBool(prob[2])) {
						if (coefCounts)
							++coefCounts[band][ctx][2]; // TWO_TOKEN
						const uint8 *p = kVp9Pareto8Full[prob[2] - 1];
						int32 val;
						if (bd.GetBool(p[0])) {
							if (bd.GetBool(p[3])) {
								tokenCache[scan[c]] = 5;
								if (bd.GetBool(p[5])) {
									if (bd.GetBool(p[7]))
										val = 67 + ReadCoeff(bd, kVp9Cat6Prob, 14); // CAT6
									else
										val = 35 + ReadCoeff(bd, kVp9Cat5Prob, 5); // CAT5
								} else if (bd.GetBool(p[6])) {
									val = 19 + ReadCoeff(bd, kVp9Cat4Prob, 4); // CAT4
								} else {
									val = 11 + ReadCoeff(bd, kVp9Cat3Prob, 3); // CAT3
								}
							} else {
								tokenCache[scan[c]] = 4;
								if (bd.GetBool(p[4]))
									val = 7 + ReadCoeff(bd, kVp9Cat2Prob, 2); // CAT2
								else
									val = 5 + ReadCoeff(bd, kVp9Cat1Prob, 1); // CAT1
							}
							v = (val * dqv) >> dqShift;
						} else {
							if (bd.GetBool(p[1])) {
								tokenCache[scan[c]] = 3;
								v = ((3 + bd.GetBool(p[2])) * dqv) >> dqShift; // THREE/FOUR
							} else {
								tokenCache[scan[c]] = 2; // TWO
								v = (2 * dqv) >> dqShift;
							}
						}
					} else {
						if (coefCounts)
							++coefCounts[band][ctx][1]; // ONE_TOKEN
						tokenCache[scan[c]] = 1; // ONE
						v = dqv >> dqShift;
					}
					dqcoeff[scan[c]] = (int16)(bd.GetFlag() ? -v : v); // signe
					++c;
					if (c < maxEob)
						ctx = (1 + tokenCache[nb[2 * c]] + tokenCache[nb[2 * c + 1]]) >> 1;
					dqv = dq[1];
				}
				return c;
			}

			// get_entropy_context : combinaison des contextes above/left du bloc tx.
			int32 GetEntropyContext(int32 txSize, const uint8 *a, const uint8 *l) {
				const int32 n = 1 << txSize;
				int32 aec = 0, lec = 0;
				for (int32 i = 0; i < n; ++i) {
					aec |= a[i];
					lec |= l[i];
				}
				return (aec != 0) + (lec != 0);
			}

			// vp9_set_contexts : propage has_eob dans les contextes A/L, avec clip aux
			// bords de l'image (les unités au-delà reçoivent 0).
			void SetEntropyContexts(FrameParseState &st, int32 plane, int32 txSize, int32 hasEob,
									int32 aoff, int32 loff, int32 ssx, int32 ssy) {
				uint8 *a = st.aboveEntCtx[plane] + st.aboveOff[plane] + aoff;
				uint8 *l = st.leftEntCtx[plane] + st.leftOff[plane] + loff;
				const int32 n = 1 << txSize;
				if (st.mbToRightEdge < 0) {
					const int32 blocksWide = st.n4w[plane] + (st.mbToRightEdge >> (5 + ssx));
					int32 aboveN = n;
					if (aboveN + aoff > blocksWide)
						aboveN = blocksWide - aoff;
					for (int32 i = 0; i < aboveN; ++i)
						a[i] = (uint8)hasEob;
					for (int32 i = aboveN; i < n; ++i)
						a[i] = 0;
				} else {
					for (int32 i = 0; i < n; ++i)
						a[i] = (uint8)hasEob;
				}
				if (st.mbToBottomEdge < 0) {
					const int32 blocksHigh = st.n4h[plane] + (st.mbToBottomEdge >> (5 + ssy));
					int32 leftN = n;
					if (leftN + loff > blocksHigh)
						leftN = blocksHigh - loff;
					for (int32 i = 0; i < leftN; ++i)
						l[i] = (uint8)hasEob;
					for (int32 i = leftN; i < n; ++i)
						l[i] = 0;
				} else {
					for (int32 i = 0; i < n; ++i)
						l[i] = (uint8)hasEob;
				}
			}

			// Modes des voisins pour les arbres kf (vp9_above/left_block_mode).
			uint8 AboveBlockMode(const MiCell &cur, const MiCell *above, int32 b) {
				if (b == 0 || b == 1) {
					if (above == nullptr)
						return 0; // DC_PRED
					return (above->sbType < kBlock8x8) ? above->bmi[b + 2] : above->mode;
				}
				return cur.bmi[b - 2];
			}
			uint8 LeftBlockMode(const MiCell &cur, const MiCell *left, int32 b) {
				if (b == 0 || b == 2) {
					if (left == nullptr)
						return 0; // DC_PRED
					return (left->sbType < kBlock8x8) ? left->bmi[b + 1] : left->mode;
				}
				return cur.bmi[b - 1];
			}

			// read_selected_tx_size + contexte (get_tx_size_context).
			int32 ReadTxSize(NkVp8BoolDecoder &bd, const FrameParseState &st, int32 maxTxSize) {
				// Contexte : tx des voisins non-skip, sinon maxTxSize.
				const bool hasAbove = st.aboveMi != nullptr;
				const bool hasLeft = st.leftMi != nullptr;
				int32 aCtx = (hasAbove && !st.aboveMi->skip) ? st.aboveMi->txSize : maxTxSize;
				int32 lCtx = (hasLeft && !st.leftMi->skip) ? st.leftMi->txSize : maxTxSize;
				if (!hasLeft)
					lCtx = aCtx;
				if (!hasAbove)
					aCtx = lCtx;
				const int32 ctx = (aCtx + lCtx) > maxTxSize ? 1 : 0;
				const uint8 *probs = (maxTxSize == 1)	? st.fc->txProbs8[ctx]
									 : (maxTxSize == 2) ? st.fc->txProbs16[ctx]
														: st.fc->txProbs32[ctx];
				int32 txSize = bd.GetBool(probs[0]);
				if (txSize != 0 && maxTxSize >= 2) {
					txSize += bd.GetBool(probs[1]);
					if (txSize != 1 && maxTxSize >= 3)
						txSize += bd.GetBool(probs[2]);
				}
				if (st.counts) {
					if (maxTxSize == 1)
						++st.counts->txP8x8[ctx][txSize];
					else if (maxTxSize == 2)
						++st.counts->txP16x16[ctx][txSize];
					else
						++st.counts->txP32x32[ctx][txSize];
				}
				return txSize;
			}

			// (défini plus bas, avec les prédicteurs)
			void PredictIntra(const FrameParseState &st, int32 plane, int32 mode, int32 txSize,
							  uint8 *dst, int32 stride, int32 aoff, int32 loff, int32 frameW,
							  int32 frameH, bool haveTop, bool haveLeft, bool haveRight);

			// Blocs de transformée d'un bloc de prédiction : prédiction intra (si
			// reconstruction active) + tokens + transformée inverse ajoutée.
			// `skip` : pas de résidus — prédiction seule. `isInter` : le bloc est déjà
			// prédit par compensation de mouvement (appelant) — ne pas appeler
			// PredictIntra ; tx_type toujours DCT_DCT ; coefProbs[ref=1].
			void DecodeBlockTokens(NkVp8BoolDecoder &bd, FrameParseState &st, bool skip, bool isInter = false) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				const bool recon = (st.planes[0] != nullptr);
				int16 dqcoeff[32 * 32];
				for (int32 plane = 0; plane < 3; ++plane) {
					const int32 ssx = plane ? hdr.subsamplingX : 0;
					const int32 ssy = plane ? hdr.subsamplingY : 0;
					int32 txSize;
					if (plane == 0) {
						txSize = st.cur.txSize;
					} else {
						// get_uv_tx_size : min(tx luma, max tx du bloc plane).
						const int32 planeBsize = kVp9SsSizeLookup[st.cur.sbType][ssx][ssy];
						txSize = IMin((int32)st.cur.txSize, (int32)kVp9MaxTxsizeLookup[planeBsize]);
					}
					if (hdr.lossless)
						txSize = 0;
					const int32 step = 1 << txSize;
					const int32 num4w = st.n4w[plane];
					const int32 num4h = st.n4h[plane];
					const int32 maxW = num4w + ((st.mbToRightEdge >= 0) ? 0 : (st.mbToRightEdge >> (5 + ssx)));
					const int32 maxH = num4h + ((st.mbToBottomEdge >= 0) ? 0 : (st.mbToBottomEdge >> (5 + ssy)));
					// n4wl : log2 des unités 4x4 du bloc (largeur) pour have_right.
					int32 n4wl = 0;
					while ((1 << n4wl) < num4w)
						++n4wl;
					const int16 *dq = plane ? st.uvDequant[st.cur.segId] : st.yDequant[st.cur.segId];
					for (int32 row = 0; row < maxH; row += step) {
						for (int32 col = 0; col < maxW; col += step) {
							// Mode du bloc tx (luma <8x8 : sous-mode ; sinon mode/uv).
							const uint8 mode = (plane == 0)
												   ? ((st.cur.sbType < kBlock8x8)
														  ? st.cur.bmi[(row << 1) + col]
														  : st.cur.mode)
												   : st.cur.uvMode;
							uint8 *dst = nullptr;
							if (recon) {
								const int32 px = (((-st.mbToLeftEdge) >> (3 + ssx)) + col * 4);
								const int32 py = (((-st.mbToTopEdge) >> (3 + ssy)) + row * 4);
								dst = st.planes[plane] + (usize)py * (usize)st.planeStride[plane] + px;
								if (!isInter) {
									const bool haveTop = (row != 0) || (st.aboveMi != nullptr);
									const bool haveLeft = (col != 0) || (st.leftMi != nullptr);
									const bool haveRight = (col + step) < (1 << n4wl);
									PredictIntra(st, plane, mode, txSize, dst, st.planeStride[plane],
												 col, row, st.planeW[plane], st.planeH[plane], haveTop,
												 haveLeft, haveRight);
								}
							}
							if (skip)
								continue;
							// Type de transformée : DCT_DCT pour l'inter, sinon selon le mode intra.
							int32 txType = 0; // DCT_DCT
							if (plane == 0 && !hdr.lossless && !isInter)
								txType = (int32)kVp9IntraModeToTxType[mode];
							const int16 *scan = nullptr, *nb = nullptr;
							GetScan(txSize, txType, &scan, &nb);
							const int32 ctx = GetEntropyContext(
								txSize, st.aboveEntCtx[plane] + st.aboveOff[plane] + col,
								st.leftEntCtx[plane] + st.leftOff[plane] + row);
							if (recon) {
								const int32 n = 16 << (txSize << 1);
								for (int32 i = 0; i < n; ++i)
									dqcoeff[i] = 0;
							}
							const int32 eob = DecodeCoefs(bd, *st.fc, plane ? 1 : 0, txSize, ctx, scan, nb, dq,
														  dqcoeff, isInter ? 1 : 0, st.counts);
							st.stats->eobTotal += eob;
							if (recon && eob > 0) {
								if (hdr.lossless) {
									NkVp9Itxfm::Iwht4x4Add(dqcoeff, dst, st.planeStride[plane]);
								} else {
									switch (txSize) {
										case 0:
											NkVp9Itxfm::Iht4x4Add(dqcoeff, dst, st.planeStride[plane], txType);
											break;
										case 1:
											NkVp9Itxfm::Iht8x8Add(dqcoeff, dst, st.planeStride[plane], txType);
											break;
										case 2:
											NkVp9Itxfm::Iht16x16Add(dqcoeff, dst, st.planeStride[plane], txType);
											break;
										default:
											NkVp9Itxfm::Idct32x32Add(dqcoeff, dst, st.planeStride[plane]);
											break;
									}
								}
							}
							SetEntropyContexts(st, plane, txSize, eob > 0, col, row, ssx, ssy);
						}
					}
				}
			}

			// Déclarations anticipées (définies plus bas, brique 6 modes/MV) — utilisées
			// par ReadInterFrameModeInfo/PredictInterRegion définis avant dans le fichier.
			int32 ClampI(int32 v, int32 lo, int32 hi);
			int32 ReadIsInterBlock(NkVp8BoolDecoder &bd, FrameParseState &st, int32 segId);
			void ReadInterBlockModeInfo(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol);
			int32 ReadIntraSegmentId(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
									 int32 xMis, int32 yMis);
			int32 ReadInterSegmentId(NkVp8BoolDecoder &bd, FrameParseState &st, MiCell &mi, int32 miRow,
									 int32 miCol, int32 xMis, int32 yMis);

			// read_intra_block_mode_info (non-kf) : modes intra dans une trame INTER —
			// contexte size_group_lookup (pas d'arbre à voisins comme en kf).
			void ReadIntraBlockModeInfoNonKf(NkVp8BoolDecoder &bd, FrameParseState &st) {
				MiCell &mi = st.cur;
				const NkVp9FrameContext &fc = *st.fc;
				switch (mi.sbType) {
					case 0: // BLOCK_4X4
						for (int32 i = 0; i < 4; ++i) {
							mi.bmi[i] = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[0]);
							if (st.counts)
								++st.counts->yMode[0][mi.bmi[i]];
						}
						mi.mode = mi.bmi[3];
						break;
					case 1: // BLOCK_4X8
						mi.bmi[0] = mi.bmi[2] = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[0]);
						if (st.counts)
							++st.counts->yMode[0][mi.bmi[0]];
						mi.bmi[1] = mi.bmi[3] = mi.mode = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[0]);
						if (st.counts)
							++st.counts->yMode[0][mi.mode];
						break;
					case 2: // BLOCK_8X4
						mi.bmi[0] = mi.bmi[1] = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[0]);
						if (st.counts)
							++st.counts->yMode[0][mi.bmi[0]];
						mi.bmi[2] = mi.bmi[3] = mi.mode = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[0]);
						if (st.counts)
							++st.counts->yMode[0][mi.mode];
						break;
					default: {
						const int32 sg = kVp9SizeGroupLookup[mi.sbType];
						mi.mode = (uint8)bd.GetTree(kVp9IntraModeTree, fc.yModeProb[sg]);
						if (st.counts)
							++st.counts->yMode[sg][mi.mode];
						mi.bmi[0] = mi.bmi[1] = mi.bmi[2] = mi.bmi[3] = mi.mode;
						break;
					}
				}
				mi.uvMode = (uint8)bd.GetTree(kVp9IntraModeTree, fc.uvModeProb[mi.mode]);
				if (st.counts)
					++st.counts->uvMode[mi.mode][mi.uvMode];
				mi.interpFilter = 3; // SWITCHABLE_FILTERS (sentinelle)
				mi.refFrame[0] = 0;	 // INTRA_FRAME
				mi.refFrame[1] = -1; // NO_REF_FRAME
			}

			// read_inter_frame_mode_info : segment id + skip + is_inter + tx_size, puis
			// délègue au chemin intra (non-kf) ou inter selon read_is_inter_block.
			void ReadInterFrameModeInfo(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
									   int32 xMis, int32 yMis) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				MiCell &mi = st.cur;
				// Segment id : prédiction temporelle depuis la carte de la trame précédente
				// (read_inter_segment_id — voir ReadInterSegmentId).
				mi.segId = (uint8)ReadInterSegmentId(bd, st, mi, miRow, miCol, xMis, yMis);
				// Skip.
				const bool segSkip = hdr.segEnabled && hdr.segFeatureEnabled[mi.segId][3];
				if (segSkip) {
					mi.skip = 1;
				} else {
					const int32 aSkip = st.aboveMi ? st.aboveMi->skip : 0;
					const int32 lSkip = st.leftMi ? st.leftMi->skip : 0;
					const int32 skipCtx = aSkip + lSkip;
					mi.skip = (uint8)bd.GetBool(st.fc->skipProbs[skipCtx]);
					if (st.counts)
						++st.counts->skip[skipCtx][mi.skip];
					if (mi.skip)
						++st.stats->skipBlocks;
				}
				const int32 isInterBlock = ReadIsInterBlock(bd, st, mi.segId);
				// tx_size : select_tx_size si (!skip || !inter_block).
				const int32 maxTx = (int32)kVp9MaxTxsizeLookup[mi.sbType];
				const int32 txModeBiggest[5] = {0, 1, 2, 3, 3};
				if (st.txMode == 4 && mi.sbType >= kBlock8x8 && (!mi.skip || !isInterBlock)) {
					mi.txSize = (uint8)ReadTxSize(bd, st, maxTx);
				} else {
					mi.txSize = (uint8)IMin(maxTx, txModeBiggest[st.txMode]);
				}
				if (isInterBlock)
					ReadInterBlockModeInfo(bd, st, miRow, miCol);
				else
					ReadIntraBlockModeInfoNonKf(bd, st);
			}

			// =================================================================
			// BRIQUE 6c — COMPENSATION DE MOUVEMENT (port fidèle vpx_convolve.c +
			// vp9_reconinter.c, cas NON-SCALÉ uniquement : x_step_q4=y_step_q4=16).
			// =================================================================

			inline uint8 ClipPixelU8(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}
			inline int32 RoundPow2(int32 v, int32 n) {
				return (v + (1 << (n - 1))) >> n;
			}

			// vpx_convolve8_c (non scalé) : horizontal puis vertical, 8 taps Q7.
			void Vp9Convolve8(const uint8 *src, int32 srcStride, uint8 *dst, int32 dstStride,
							  const int16 kernel[16][8], int32 x0q4, int32 y0q4, int32 w, int32 h, bool avgMode) {
				uint8 temp[64 * 80];
				const int32 interH = (((h - 1) * 16 + y0q4) >> 4) + 8;
				const uint8 *s = src - srcStride * 3 - 3;
				for (int32 y = 0; y < interH; ++y) {
					int32 xq4 = x0q4;
					for (int32 x = 0; x < w; ++x) {
						const uint8 *sx = s + (xq4 >> 4);
						const int16 *f = kernel[xq4 & 15];
						int32 sum = 0;
						for (int32 k = 0; k < 8; ++k)
							sum += sx[k] * f[k];
						temp[y * 64 + x] = ClipPixelU8(RoundPow2(sum, 7));
						xq4 += 16;
					}
					s += srcStride;
				}
				for (int32 x = 0; x < w; ++x) {
					int32 yq4 = y0q4;
					for (int32 y = 0; y < h; ++y) {
						const uint8 *ty = temp + (yq4 >> 4) * 64 + x;
						const int16 *f = kernel[yq4 & 15];
						int32 sum = 0;
						for (int32 k = 0; k < 8; ++k)
							sum += ty[k * 64] * f[k];
						const uint8 v = ClipPixelU8(RoundPow2(sum, 7));
						dst[y * dstStride + x] = avgMode ? (uint8)((dst[y * dstStride + x] + v + 1) >> 1) : v;
						yq4 += 16;
					}
				}
			}

			int32 RoundMvCompQ2(int32 v) {
				return (v < 0 ? v - 1 : v + 1) / 2;
			}
			int32 RoundMvCompQ4(int32 v) {
				return (v < 0 ? v - 2 : v + 2) / 4;
			}
			// average_split_mvs (reconinter.c) : MV chroma sous-échantillonnée depuis
			// les MV des sous-blocs 4x4 luma couverts.
			SimpleMv AverageSplitMvs(const MiCell &mi, int32 ref, int32 block, int32 ssx, int32 ssy) {
				const int32 ssIdx = ((ssx > 0) ? 2 : 0) | ((ssy > 0) ? 1 : 0);
				switch (ssIdx) {
					case 0:
						return mi.bmiMv[block][ref];
					case 1:
						return SimpleMv{(int16)RoundMvCompQ2(mi.bmiMv[block][ref].row + mi.bmiMv[block + 2][ref].row),
										(int16)RoundMvCompQ2(mi.bmiMv[block][ref].col + mi.bmiMv[block + 2][ref].col)};
					case 2:
						return SimpleMv{(int16)RoundMvCompQ2(mi.bmiMv[block][ref].row + mi.bmiMv[block + 1][ref].row),
										(int16)RoundMvCompQ2(mi.bmiMv[block][ref].col + mi.bmiMv[block + 1][ref].col)};
					default: {
						int32 r = 0, c = 0;
						for (int32 k = 0; k < 4; ++k) {
							r += mi.bmiMv[k][ref].row;
							c += mi.bmiMv[k][ref].col;
						}
						return SimpleMv{(int16)RoundMvCompQ4(r), (int16)RoundMvCompQ4(c)};
					}
				}
			}

			// build_inter_predictors (une région rectangulaire d'UN plan, UN sous-bloc
			// si sbType<8x8). `x,y,w,h` en pixels du PLAN ; `dst` déjà positionné.
			void PredictInterRegion(const FrameParseState &st, int32 plane, int32 bw, int32 bh, int32 x, int32 y,
									int32 w, int32 h, int32 block, uint8 *dst, int32 dstStride) {
				const MiCell &mi = st.cur;
				const bool isCompound = mi.HasSecondRef();
				const int32 ssx = plane ? st.hdr->subsamplingX : 0;
				const int32 ssy = plane ? st.hdr->subsamplingY : 0;
				// Origine GLOBALE (dans le plan) du bloc courant — `x,y` ci-dessus ne sont
				// que des offsets LOCAUX au bloc (0 pour un bloc entier, 4*sous-bloc pour
				// sbType<8x8). Sans cette addition, la lecture référence restait bloquée
				// à l'origine (0,0) du plan de référence pour TOUT bloc (fonctionnait par
				// coïncidence en (0,0), corrompait tout le reste — bug diagnostiqué en
				// comparant la prédiction MV=0 au pixel de la trame clé référencée).
				const int32 blockX = (-st.mbToLeftEdge) >> (3 + ssx);
				const int32 blockY = (-st.mbToTopEdge) >> (3 + ssy);
				for (int32 ref = 0; ref < (isCompound ? 2 : 1); ++ref) {
					const SimpleMv mv =
						(mi.sbType < kBlock8x8) ? AverageSplitMvs(mi, ref, block, ssx, ssy) : mi.mv[ref];
					// clamp_mv_to_umv_border_sb : MV → Q4 (échelle luma/chroma) + clamp UMV.
					SimpleMv mvQ4{(int16)(mv.row * (1 << (1 - ssy))), (int16)(mv.col * (1 << (1 - ssx)))};
					constexpr int32 kInterpExtend = 4;
					const int32 spelLeft = (kInterpExtend + bw) << 4, spelRight = spelLeft - 16;
					const int32 spelTop = (kInterpExtend + bh) << 4, spelBottom = spelTop - 16;
					mvQ4.col = (int16)ClampI(mvQ4.col, st.mbToLeftEdge * (1 << (1 - ssx)) - spelLeft,
											 st.mbToRightEdge * (1 << (1 - ssx)) + spelRight);
					mvQ4.row = (int16)ClampI(mvQ4.row, st.mbToTopEdge * (1 << (1 - ssy)) - spelTop,
											 st.mbToBottomEdge * (1 << (1 - ssy)) + spelBottom);
					const int32 subpelX = mvQ4.col & 15, subpelY = mvQ4.row & 15;
					const uint8 *refPlane = st.refPlanes[mi.refFrame[ref] - 1][plane];
					const int32 refStride = st.refStride[mi.refFrame[ref] - 1][plane];
					const uint8 *pre = refPlane + (usize)(blockY + y) * (usize)refStride + (usize)(blockX + x);
					pre += (mvQ4.row >> 4) * refStride + (mvQ4.col >> 4);
					Vp9Convolve8(pre, refStride, dst, dstStride, kVp9FilterKernels[mi.interpFilter], subpelX,
								subpelY, w, h, ref == 1);
				}
			}

			// build_inter_predictors_for_planes : dispatch par plan, sous-blocs 4x4 si
			// sbType<8x8 (chaque sous-bloc peut avoir un MV distinct).
			void PredictInter(const FrameParseState &st, int32 plane, uint8 *dst, int32 dstStride) {
				const MiCell &mi = st.cur;
				const int32 ssx = plane ? st.hdr->subsamplingX : 0;
				const int32 ssy = plane ? st.hdr->subsamplingY : 0;
				const int32 num4w = st.n4w[plane], num4h = st.n4h[plane];
				const int32 bw = 4 * num4w, bh = 4 * num4h;
				if (mi.sbType < kBlock8x8) {
					int32 i = 0;
					for (int32 yy = 0; yy < num4h; ++yy)
						for (int32 xx = 0; xx < num4w; ++xx)
							PredictInterRegion(st, plane, bw, bh, 4 * xx, 4 * yy, 4, 4, i++, dst + 4 * yy * dstStride + 4 * xx,
											   dstStride);
				} else {
					PredictInterRegion(st, plane, bw, bh, 0, 0, bw, bh, 0, dst, dstStride);
				}
				(void)ssx;
				(void)ssy;
			}

			void DecodeLeafBlock(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
								 int32 bsize, int32 bwl, int32 bhl) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				const NkVp9FrameContext &fc = *st.fc;
				const int32 bw = 1 << (bwl - 1); // unités mi
				const int32 bh = 1 << (bhl - 1);
				const int32 xMis = IMin(bw, st.miCols - miCol);
				const int32 yMis = IMin(bh, st.miRows - miRow);
				++st.stats->blocks;

				// set_mi_row_col : voisins + distances aux bords (unités 1/8 pel).
				st.aboveMi = (miRow > 0) ? &st.mi[(miRow - 1) * st.miCols + miCol] : nullptr;
				st.leftMi = (miCol > st.tileMiColStart) ? &st.mi[miRow * st.miCols + miCol - 1] : nullptr;
				st.mbToRightEdge = (st.miCols - bw - miCol) * 8 * 8;
				st.mbToBottomEdge = (st.miRows - bh - miRow) * 8 * 8;
				st.mbToLeftEdge = -(miCol * 8) * 8;
				st.mbToTopEdge = -(miRow * 8) * 8;
				// set_plane_n4 + set_skip_context.
				for (int32 p = 0; p < 3; ++p) {
					const int32 ssx = p ? hdr.subsamplingX : 0;
					const int32 ssy = p ? hdr.subsamplingY : 0;
					st.n4w[p] = (bw << 1) >> ssx;
					st.n4h[p] = (bh << 1) >> ssy;
					st.aboveOff[p] = (miCol * 2) >> ssx;
					st.leftOff[p] = ((miRow * 2) & 15) >> ssy;
				}

				MiCell &cell = st.cur;
				cell = MiCell{};
				cell.sbType = (uint8)bsize;

				if (st.isInterFrame) {
					// --- read_inter_frame_mode_info (brique 6) ---
					ReadInterFrameModeInfo(bd, st, miRow, miCol, xMis, yMis);
				} else {
					// --- read_intra_frame_mode_info (trame clé) ---
					// Segment id (prédiction temporelle depuis la carte précédente possible
					// même sur une trame clé si segUpdateMap est faux — read_intra_segment_id).
					cell.segId = (uint8)ReadIntraSegmentId(bd, st, miRow, miCol, xMis, yMis);
					// Skip.
					const bool segSkip = hdr.segEnabled && hdr.segFeatureEnabled[cell.segId][3];
					if (segSkip) {
						cell.skip = 1;
					} else {
						const int32 aSkip = st.aboveMi ? st.aboveMi->skip : 0;
						const int32 lSkip = st.leftMi ? st.leftMi->skip : 0;
						cell.skip = (uint8)bd.GetBool(fc.skipProbs[aSkip + lSkip]);
						if (cell.skip)
							++st.stats->skipBlocks;
					}
					// tx_size.
					const int32 maxTx = (int32)kVp9MaxTxsizeLookup[bsize];
					const int32 txModeBiggest[5] = {0, 1, 2, 3, 3};
					const int32 txMode = st.txMode;
					if (txMode == 4 && bsize >= kBlock8x8) {
						cell.txSize = (uint8)ReadTxSize(bd, st, maxTx);
					} else {
						cell.txSize = (uint8)IMin(maxTx, txModeBiggest[txMode]);
					}
					// Modes intra kf (arbres à voisins).
					switch (bsize) {
						case 0: // BLOCK_4X4 : 4 sous-modes
							for (int32 i = 0; i < 4; ++i)
								cell.bmi[i] = (uint8)bd.GetTree(
									kVp9IntraModeTree,
									kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, i)]
												   [LeftBlockMode(cell, st.leftMi, i)]);
							cell.mode = cell.bmi[3];
							break;
						case 1: // BLOCK_4X8
							cell.bmi[0] = cell.bmi[2] = (uint8)bd.GetTree(
								kVp9IntraModeTree, kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, 0)]
																  [LeftBlockMode(cell, st.leftMi, 0)]);
							cell.bmi[1] = cell.bmi[3] = cell.mode = (uint8)bd.GetTree(
								kVp9IntraModeTree, kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, 1)]
																  [LeftBlockMode(cell, st.leftMi, 1)]);
							break;
						case 2: // BLOCK_8X4
							cell.bmi[0] = cell.bmi[1] = (uint8)bd.GetTree(
								kVp9IntraModeTree, kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, 0)]
																  [LeftBlockMode(cell, st.leftMi, 0)]);
							cell.bmi[2] = cell.bmi[3] = cell.mode = (uint8)bd.GetTree(
								kVp9IntraModeTree, kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, 2)]
																  [LeftBlockMode(cell, st.leftMi, 2)]);
							break;
						default:
							cell.mode = (uint8)bd.GetTree(
								kVp9IntraModeTree, kVp9KfYModeProb[AboveBlockMode(cell, st.aboveMi, 0)]
																  [LeftBlockMode(cell, st.leftMi, 0)]);
							cell.bmi[0] = cell.bmi[1] = cell.bmi[2] = cell.bmi[3] = cell.mode;
							break;
					}
					cell.uvMode = (uint8)bd.GetTree(kVp9IntraModeTree, kVp9KfUvModeProb[cell.mode]);
					cell.refFrame[0] = 0; // INTRA_FRAME
					cell.refFrame[1] = -1;
				}

				// Propagation de la cellule sur l'emprise (voisinage des blocs suivants).
				for (int32 y = 0; y < yMis; ++y)
					for (int32 x = 0; x < xMis; ++x)
						st.mi[(miRow + y) * st.miCols + (miCol + x)] = cell;

				// --- Résidus + reconstruction ---
				if (cell.skip) {
					// dec_reset_skip_context : contextes A/L à zéro sur l'emprise.
					for (int32 p = 0; p < 3; ++p) {
						for (int32 i = 0; i < st.n4w[p]; ++i)
							st.aboveEntCtx[p][st.aboveOff[p] + i] = 0;
						for (int32 i = 0; i < st.n4h[p]; ++i)
							st.leftEntCtx[p][st.leftOff[p] + i] = 0;
					}
				}
				const int64 eobBefore = st.stats->eobTotal;
				if (cell.IsInter()) {
					// Compensation de mouvement : une fois par plan (pas par bloc de
					// transformée), AVANT le résidu — contrairement à l'intra.
					if (st.planes[0] != nullptr) {
						for (int32 p = 0; p < 3; ++p) {
							const int32 ssx = p ? hdr.subsamplingX : 0;
							const int32 ssy = p ? hdr.subsamplingY : 0;
							const int32 px = (-st.mbToLeftEdge) >> (3 + ssx);
							const int32 py = (-st.mbToTopEdge) >> (3 + ssy);
							uint8 *dst = st.planes[p] + (usize)py * (usize)st.planeStride[p] + px;
							PredictInter(st, p, dst, st.planeStride[p]);
						}
					}
					DecodeBlockTokens(bd, st, cell.skip != 0, true);
					// decode_block (vp9_decodeframe.c) : un bloc inter NON-skip >=8x8 dont
					// TOUS les résidus sont nuls (eobtotal==0) devient skip=1 A POSTERIORI
					// — pas pour l'entropie (déjà lue), mais pour le LOOP FILTER et surtout
					// pour le CONTEXTE skip des voisins SUIVANTS (vp9_get_skip_context lit
					// mi.skip depuis la grille). Sans cette mise à jour, le contexte skip
					// diverge dès le premier bloc concerné et désynchronise tout le reste
					// du tile (bug diagnostiqué via trace croisée avec vpxdec instrumenté).
					if (!cell.skip && cell.sbType >= kBlock8x8 && st.stats->eobTotal == eobBefore) {
						cell.skip = 1;
						for (int32 y = 0; y < yMis; ++y)
							for (int32 x = 0; x < xMis; ++x)
								st.mi[(miRow + y) * st.miCols + (miCol + x)].skip = 1;
					}
				} else {
					// La prédiction intra a toujours lieu ; les tokens seulement si !skip.
					DecodeBlockTokens(bd, st, cell.skip != 0, false);
				}
			}

			// =================================================================
			// BRIQUE 6 — MODES INTER + VECTEURS DE MOUVEMENT (port fidèle de
			// vp9_decodemv.c + vp9_mvref_common.c + vp9_pred_common.c).
			// =================================================================

			constexpr int32 kMvBorder = 16 << 3;	// MV_BORDER
			constexpr int32 kMvRefThresh = 64;		// use_mv_hp
			constexpr int32 kMvUpp = (1 << 14) - 1; // MV_UPP
			constexpr int32 kMvLow = -(1 << 14);	 // MV_LOW

			int32 ClampI(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			bool UseMvHp(const SimpleMv &ref) {
				return (ref.row < kMvRefThresh && ref.row > -kMvRefThresh) &&
					   (ref.col < kMvRefThresh && ref.col > -kMvRefThresh);
			}
			void ClampMvRef(SimpleMv &mv, const FrameParseState &st) {
				mv.col = (int16)ClampI(mv.col, st.mbToLeftEdge - kMvBorder, st.mbToRightEdge + kMvBorder);
				mv.row = (int16)ClampI(mv.row, st.mbToTopEdge - kMvBorder, st.mbToBottomEdge + kMvBorder);
			}

			// idx_n_column_to_subblock (mvref_common.c) : sous-bloc source des 2
			// voisins immédiats (dessus/gauche) selon block_idx et search_col==0.
			const int32 kIdxNColumnToSubblock[4][2] = {{1, 2}, {1, 3}, {3, 2}, {3, 3}};

			// get_sub_block_mv : MV du voisin, en tenant compte des sous-blocs 4x4.
			SimpleMv GetSubBlockMv(const MiCell &cand, int32 which, int32 searchCol, int32 block) {
				if (block >= 0 && cand.sbType < kBlock8x8)
					return cand.bmiMv[kIdxNColumnToSubblock[block][searchCol == 0 ? 1 : 0]][which];
				return cand.mv[which];
			}
			// scale_mv : inverse le signe si le sign bias diffère entre la réf du
			// candidat et la réf recherchée.
			SimpleMv ScaleMv(const MiCell &mi, int32 ref, int32 thisRefFrame, const int32 *signBias) {
				SimpleMv mv = mi.mv[ref];
				if (signBias[mi.refFrame[ref]] != signBias[thisRefFrame]) {
					mv.row = (int16)(-mv.row);
					mv.col = (int16)(-mv.col);
				}
				return mv;
			}

			bool IsInsideTile(const FrameParseState &st, int32 miRow, int32 miCol, int32 posRow, int32 posCol) {
				return !(miRow + posRow < 0 || miCol + posCol < st.tileMiColStart || miRow + posRow >= st.miRows ||
						 miCol + posCol >= st.tileMiColEnd);
			}

			// ADD_MV_REF_LIST / _EB : ajoute mv à la liste si absent ; renvoie true si on
			// doit s'arrêter (2e MV trouvée, ou early_break sur la 1re).
			bool AddMvRefList(const SimpleMv &mv, int32 &refmvCount, SimpleMv mvList[2], bool earlyBreak) {
				if (refmvCount) {
					if (mv != mvList[0]) {
						mvList[refmvCount] = mv;
						++refmvCount;
						return true;
					}
					return false;
				}
				mvList[refmvCount++] = mv;
				return earlyBreak;
			}

			// dec_find_mv_refs (vp9_decodemv.c) : recherche des MV candidats pour un
			// bloc/référence donnés. `block` >= 0 pour les sous-blocs 4x4 (sub8x8),
			// -1 pour un bloc >=8x8. Renvoie refmvCount (1 sauf NEARMV : 2).
			int32 DecFindMvRefs(const FrameParseState &st, int32 mode, int32 refFrame,
								const int8 mvRefSearch[8][2], SimpleMv mvList[2], int32 miRow, int32 miCol,
								int32 block) {
				int32 refmvCount = 0;
				bool differentRefFound = false;
				const bool earlyBreak = (mode != 11); // NEARMV=11 (NEARESTMV=10,ZEROMV=12,NEWMV=13)
				mvList[0] = SimpleMv{};
				mvList[1] = SimpleMv{};

				int32 i = 0;
				if (block >= 0) {
					for (; i < 2; ++i) {
						const int32 pr = mvRefSearch[i][0], pc = mvRefSearch[i][1];
						if (!IsInsideTile(st, miRow, miCol, pr, pc))
							continue;
						const MiCell &cand = st.mi[(miRow + pr) * st.miCols + (miCol + pc)];
						differentRefFound = true;
						if (cand.refFrame[0] == refFrame) {
							if (AddMvRefList(GetSubBlockMv(cand, 0, pc, block), refmvCount, mvList, earlyBreak))
								goto Done;
						} else if (cand.refFrame[1] == refFrame) {
							if (AddMvRefList(GetSubBlockMv(cand, 1, pc, block), refmvCount, mvList, earlyBreak))
								goto Done;
						}
					}
				}
				for (; i < 8; ++i) {
					const int32 pr = mvRefSearch[i][0], pc = mvRefSearch[i][1];
					if (!IsInsideTile(st, miRow, miCol, pr, pc))
						continue;
					const MiCell &cand = st.mi[(miRow + pr) * st.miCols + (miCol + pc)];
					differentRefFound = true;
					if (cand.refFrame[0] == refFrame) {
						if (AddMvRefList(cand.mv[0], refmvCount, mvList, earlyBreak))
							goto Done;
					} else if (cand.refFrame[1] == refFrame) {
						if (AddMvRefList(cand.mv[1], refmvCount, mvList, earlyBreak))
							goto Done;
					}
				}
				if (st.usePrevFrameMvs && st.prevMvs) {
					const NkVp9MvRef &pm = st.prevMvs[miRow * st.miCols + miCol];
					if (pm.refFrame[0] == refFrame) {
						if (AddMvRefList(SimpleMv{pm.mvRow[0], pm.mvCol[0]}, refmvCount, mvList, earlyBreak))
							goto Done;
					} else if (pm.refFrame[1] == refFrame) {
						if (AddMvRefList(SimpleMv{pm.mvRow[1], pm.mvCol[1]}, refmvCount, mvList, earlyBreak))
							goto Done;
					}
				}
				if (differentRefFound) {
					for (i = 0; i < 8; ++i) {
						const int32 pr = mvRefSearch[i][0], pc = mvRefSearch[i][1];
						if (!IsInsideTile(st, miRow, miCol, pr, pc))
							continue;
						const MiCell &cand = st.mi[(miRow + pr) * st.miCols + (miCol + pc)];
						if (!cand.IsInter())
							continue;
						if (cand.refFrame[0] != refFrame) {
							if (AddMvRefList(ScaleMv(cand, 0, refFrame, st.signBias), refmvCount, mvList,
											 earlyBreak))
								goto Done;
						}
						if (cand.HasSecondRef() && cand.refFrame[1] != refFrame && cand.mv[1] != cand.mv[0]) {
							if (AddMvRefList(ScaleMv(cand, 1, refFrame, st.signBias), refmvCount, mvList,
											 earlyBreak))
								goto Done;
						}
					}
				}
				if (st.usePrevFrameMvs && st.prevMvs) {
					const NkVp9MvRef &pm = st.prevMvs[miRow * st.miCols + miCol];
					if (pm.refFrame[0] != refFrame && pm.refFrame[0] > 0) {
						SimpleMv mv{pm.mvRow[0], pm.mvCol[0]};
						if (st.signBias[pm.refFrame[0]] != st.signBias[refFrame]) {
							mv.row = (int16)(-mv.row);
							mv.col = (int16)(-mv.col);
						}
						if (AddMvRefList(mv, refmvCount, mvList, earlyBreak))
							goto Done;
					}
					if (pm.refFrame[1] > 0 && pm.refFrame[1] != refFrame &&
						!(pm.mvRow[1] == pm.mvRow[0] && pm.mvCol[1] == pm.mvCol[0])) {
						SimpleMv mv{pm.mvRow[1], pm.mvCol[1]};
						if (st.signBias[pm.refFrame[1]] != st.signBias[refFrame]) {
							mv.row = (int16)(-mv.row);
							mv.col = (int16)(-mv.col);
						}
						if (AddMvRefList(mv, refmvCount, mvList, earlyBreak))
							goto Done;
					}
				}
				refmvCount = (mode == 11) ? 2 : 1; // NEARMV : 2 candidats, sinon 1 (nearestmv)
			Done:
				for (i = 0; i < refmvCount; ++i)
					ClampMvRef(mvList[i], st);
				return refmvCount;
			}

			// append_sub8x8_mvs_for_idx : MV candidate pour un sous-bloc <8x8 donné.
			void AppendSub8x8Mvs(const FrameParseState &st, const int8 mvRefSearch[8][2], int32 bMode,
								 int32 block, int32 ref, int32 miRow, int32 miCol, SimpleMv &bestSub8x8) {
				const MiCell &mi = st.cur;
				SimpleMv mvList[2];
				if (block == 0) {
					const int32 n = DecFindMvRefs(st, bMode, mi.refFrame[ref], mvRefSearch, mvList, miRow, miCol, 0);
					bestSub8x8 = mvList[n - 1];
				} else if (block == 1 || block == 2) {
					if (bMode == 10) { // NEARESTMV
						bestSub8x8 = mi.bmiMv[0][ref];
					} else {
						DecFindMvRefs(st, bMode, mi.refFrame[ref], mvRefSearch, mvList, miRow, miCol, block);
						bestSub8x8 = SimpleMv{};
						for (int32 n = 0; n < 2; ++n)
							if (mi.bmiMv[0][ref] != mvList[n]) {
								bestSub8x8 = mvList[n];
								break;
							}
					}
				} else { // block == 3
					if (bMode == 10) {
						bestSub8x8 = mi.bmiMv[2][ref];
					} else {
						bestSub8x8 = SimpleMv{};
						if (mi.bmiMv[2][ref] != mi.bmiMv[1][ref]) {
							bestSub8x8 = mi.bmiMv[1][ref];
						} else if (mi.bmiMv[2][ref] != mi.bmiMv[0][ref]) {
							bestSub8x8 = mi.bmiMv[0][ref];
						} else {
							DecFindMvRefs(st, bMode, mi.refFrame[ref], mvRefSearch, mvList, miRow, miCol, 3);
							for (int32 n = 0; n < 2; ++n)
								if (mi.bmiMv[2][ref] != mvList[n]) {
									bestSub8x8 = mvList[n];
									break;
								}
						}
					}
				}
			}

			// --- Segmentation temporelle (§ carte de segments persistante inter-trames) ---

			// vp9_get_pred_context_seg_id : somme des flags seg_id_predicted des voisins
			// above/left (0, 1 ou 2) — contexte de la proba de réutilisation temporelle.
			int32 GetPredContextSegId(const MiCell *above, const MiCell *left) {
				const int32 a = above ? above->segIdPredicted : 0;
				const int32 l = left ? left->segIdPredicted : 0;
				return a + l;
			}

			// dec_get_segment_id : MIN du segment_id sur l'emprise x_mis*y_mis du bloc,
			// lu dans la carte de la trame PRÉCÉDENTE (0 si indisponible).
			int32 DecGetSegmentId(const FrameParseState &st, int32 miRow, int32 miCol, int32 xMis, int32 yMis) {
				if (!st.lastSegMap)
					return 0;
				int32 segId = 7; // MAX_SEGMENTS-1 ; minimisé ci-dessous
				for (int32 y = 0; y < yMis; ++y)
					for (int32 x = 0; x < xMis; ++x)
						segId = IMin(segId, (int32)st.lastSegMap[(miRow + y) * st.miCols + (miCol + x)]);
				return segId;
			}

			// set_segment_id : écriture UNIFORME de segId sur l'emprise, dans la carte COURANTE.
			void SetSegmentId(FrameParseState &st, int32 miRow, int32 miCol, int32 xMis, int32 yMis, int32 segId) {
				if (!st.currentSegMap)
					return;
				for (int32 y = 0; y < yMis; ++y)
					for (int32 x = 0; x < xMis; ++x)
						st.currentSegMap[(miRow + y) * st.miCols + (miCol + x)] = (uint8)segId;
			}

			// copy_segment_id (cas !update_map) : recopie last→current cellule par cellule
			// sur l'emprise (0 si last indisponible) — AUCUN bit lu dans le bitstream.
			void CopySegmentId(FrameParseState &st, int32 miRow, int32 miCol, int32 xMis, int32 yMis) {
				if (!st.currentSegMap)
					return;
				for (int32 y = 0; y < yMis; ++y) {
					for (int32 x = 0; x < xMis; ++x) {
						const usize idx = (usize)(miRow + y) * (usize)st.miCols + (usize)(miCol + x);
						st.currentSegMap[idx] = st.lastSegMap ? st.lastSegMap[idx] : 0;
					}
				}
			}

			// read_intra_segment_id : trame clé/intra-only — pas de prédiction temporelle
			// (toujours 0 si !update_map, sinon lecture directe de l'arbre).
			int32 ReadIntraSegmentId(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
									 int32 xMis, int32 yMis) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				if (!hdr.segEnabled)
					return 0;
				if (!hdr.segUpdateMap) {
					CopySegmentId(st, miRow, miCol, xMis, yMis);
					return 0;
				}
				const int32 segId = bd.GetTree(kVp9SegmentTree, hdr.segTreeProbs);
				SetSegmentId(st, miRow, miCol, xMis, yMis, segId);
				return segId;
			}

			// read_inter_segment_id : trame inter — prédiction temporelle optionnelle
			// (1 bit "réutiliser le segment prédit" avant de retomber sur l'arbre complet).
			int32 ReadInterSegmentId(NkVp8BoolDecoder &bd, FrameParseState &st, MiCell &mi, int32 miRow,
									 int32 miCol, int32 xMis, int32 yMis) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				if (!hdr.segEnabled)
					return 0;
				const int32 predictedSegId = DecGetSegmentId(st, miRow, miCol, xMis, yMis);
				if (!hdr.segUpdateMap) {
					CopySegmentId(st, miRow, miCol, xMis, yMis);
					return predictedSegId;
				}
				int32 segId;
				if (hdr.segTemporalUpdate) {
					const int32 ctx = GetPredContextSegId(st.aboveMi, st.leftMi);
					mi.segIdPredicted = (uint8)bd.GetBool(hdr.segPredProbs[ctx]);
					segId = mi.segIdPredicted ? predictedSegId : (int32)bd.GetTree(kVp9SegmentTree, hdr.segTreeProbs);
				} else {
					segId = (int32)bd.GetTree(kVp9SegmentTree, hdr.segTreeProbs);
				}
				SetSegmentId(st, miRow, miCol, xMis, yMis, segId);
				return segId;
			}

			// --- Contextes de prédiction (vp9_pred_common.h/.c) ---

			int32 GetIntraInterContext(const MiCell *above, const MiCell *left) {
				const bool hasAbove = above != nullptr, hasLeft = left != nullptr;
				if (hasAbove && hasLeft) {
					const bool aIntra = !above->IsInter(), lIntra = !left->IsInter();
					return (lIntra && aIntra) ? 3 : (lIntra || aIntra ? 1 : 0);
				}
				if (hasAbove || hasLeft)
					return 2 * (!(hasAbove ? above : left)->IsInter() ? 1 : 0);
				return 0;
			}

			int32 GetSwitchableInterpContext(const MiCell *above, const MiCell *left) {
				const int32 leftType = left ? left->interpFilter : 3;
				const int32 aboveType = above ? above->interpFilter : 3;
				if (leftType == aboveType)
					return leftType;
				if (leftType == 3)
					return aboveType;
				if (aboveType == 3)
					return leftType;
				return 3;
			}

			int32 GetReferenceModeContext(const FrameParseState &st) {
				const MiCell *a = st.aboveMi, *l = st.leftMi;
				const bool hasA = a != nullptr, hasL = l != nullptr;
				if (hasA && hasL) {
					if (!a->HasSecondRef() && !l->HasSecondRef())
						return (a->refFrame[0] == st.compFixedRef) ^ (l->refFrame[0] == st.compFixedRef);
					if (!a->HasSecondRef())
						return 2 + ((a->refFrame[0] == st.compFixedRef || !a->IsInter()) ? 1 : 0);
					if (!l->HasSecondRef())
						return 2 + ((l->refFrame[0] == st.compFixedRef || !l->IsInter()) ? 1 : 0);
					return 4;
				}
				if (hasA || hasL) {
					const MiCell *e = hasA ? a : l;
					return e->HasSecondRef() ? 3 : (e->refFrame[0] == st.compFixedRef ? 1 : 0);
				}
				return 1;
			}

			int32 GetPredContextCompRefP(const FrameParseState &st) {
				const MiCell *a = st.aboveMi, *l = st.leftMi;
				const bool hasA = a != nullptr, hasL = l != nullptr;
				const int32 fixRefIdx = st.signBias[st.compFixedRef];
				const int32 varRefIdx = fixRefIdx ? 0 : 1;
				if (hasA && hasL) {
					const bool aIntra = !a->IsInter(), lIntra = !l->IsInter();
					if (aIntra && lIntra)
						return 2;
					if (aIntra || lIntra) {
						const MiCell *e = aIntra ? l : a;
						if (!e->HasSecondRef())
							return 1 + 2 * (e->refFrame[0] != st.compVarRef[1] ? 1 : 0);
						return 1 + 2 * (e->refFrame[varRefIdx] != st.compVarRef[1] ? 1 : 0);
					}
					const bool lSg = !l->HasSecondRef(), aSg = !a->HasSecondRef();
					const int32 vrfa = aSg ? a->refFrame[0] : a->refFrame[varRefIdx];
					const int32 vrfl = lSg ? l->refFrame[0] : l->refFrame[varRefIdx];
					if (vrfa == vrfl && st.compVarRef[1] == vrfa)
						return 0;
					if (lSg && aSg) {
						if ((vrfa == st.compFixedRef && vrfl == st.compVarRef[0]) ||
							(vrfl == st.compFixedRef && vrfa == st.compVarRef[0]))
							return 4;
						return (vrfa == vrfl) ? 3 : 1;
					}
					if (lSg || aSg) {
						const int32 vrfc = lSg ? vrfa : vrfl;
						const int32 rfs = aSg ? vrfa : vrfl;
						if (vrfc == st.compVarRef[1] && rfs != st.compVarRef[1])
							return 1;
						if (rfs == st.compVarRef[1] && vrfc != st.compVarRef[1])
							return 2;
						return 4;
					}
					return (vrfa == vrfl) ? 4 : 2;
				}
				if (hasA || hasL) {
					const MiCell *e = hasA ? a : l;
					if (!e->IsInter())
						return 2;
					if (e->HasSecondRef())
						return 4 * (e->refFrame[varRefIdx] != st.compVarRef[1] ? 1 : 0);
					return 3 * (e->refFrame[0] != st.compVarRef[1] ? 1 : 0);
				}
				return 2;
			}

			int32 GetPredContextSingleRefP1(const MiCell *above, const MiCell *left) {
				const bool hasA = above != nullptr, hasL = left != nullptr;
				const int32 kLast = 1;
				if (hasA && hasL) {
					const bool aI = !above->IsInter(), lI = !left->IsInter();
					if (aI && lI)
						return 2;
					if (aI || lI) {
						const MiCell *e = aI ? left : above;
						if (!e->HasSecondRef())
							return 4 * (e->refFrame[0] == kLast ? 1 : 0);
						return 1 + ((e->refFrame[0] == kLast || e->refFrame[1] == kLast) ? 1 : 0);
					}
					const bool aSec = above->HasSecondRef(), lSec = left->HasSecondRef();
					if (aSec && lSec) {
						return 1 + ((above->refFrame[0] == kLast || above->refFrame[1] == kLast ||
									 left->refFrame[0] == kLast || left->refFrame[1] == kLast)
										? 1
										: 0);
					}
					if (aSec || lSec) {
						const int32 rfs = !aSec ? above->refFrame[0] : left->refFrame[0];
						const int32 crf1 = aSec ? above->refFrame[0] : left->refFrame[0];
						const int32 crf2 = aSec ? above->refFrame[1] : left->refFrame[1];
						if (rfs == kLast)
							return 3 + ((crf1 == kLast || crf2 == kLast) ? 1 : 0);
						return (crf1 == kLast || crf2 == kLast) ? 1 : 0;
					}
					return 2 * (above->refFrame[0] == kLast ? 1 : 0) + 2 * (left->refFrame[0] == kLast ? 1 : 0);
				}
				if (hasA || hasL) {
					const MiCell *e = hasA ? above : left;
					if (!e->IsInter())
						return 2;
					if (!e->HasSecondRef())
						return 4 * (e->refFrame[0] == kLast ? 1 : 0);
					return 1 + ((e->refFrame[0] == kLast || e->refFrame[1] == kLast) ? 1 : 0);
				}
				return 2;
			}

			int32 GetPredContextSingleRefP2(const MiCell *above, const MiCell *left) {
				const bool hasA = above != nullptr, hasL = left != nullptr;
				const int32 kLast = 1, kGolden = 2, kAltref = 3;
				if (hasA && hasL) {
					const bool aI = !above->IsInter(), lI = !left->IsInter();
					if (aI && lI)
						return 2;
					if (aI || lI) {
						const MiCell *e = aI ? left : above;
						if (!e->HasSecondRef()) {
							if (e->refFrame[0] == kLast)
								return 3;
							return 4 * (e->refFrame[0] == kGolden ? 1 : 0);
						}
						return 1 + 2 * ((e->refFrame[0] == kGolden || e->refFrame[1] == kGolden) ? 1 : 0);
					}
					const bool aSec = above->HasSecondRef(), lSec = left->HasSecondRef();
					const int32 a0 = above->refFrame[0], a1 = above->refFrame[1];
					const int32 l0 = left->refFrame[0], l1 = left->refFrame[1];
					if (aSec && lSec) {
						if (a0 == l0 && a1 == l1)
							return 3 * ((a0 == kGolden || a1 == kGolden || l0 == kGolden || l1 == kGolden) ? 1 : 0);
						return 2;
					}
					if (aSec || lSec) {
						const int32 rfs = !aSec ? a0 : l0;
						const int32 crf1 = aSec ? a0 : l0;
						const int32 crf2 = aSec ? a1 : l1;
						if (rfs == kGolden)
							return 3 + ((crf1 == kGolden || crf2 == kGolden) ? 1 : 0);
						if (rfs == kAltref)
							return (crf1 == kGolden || crf2 == kGolden) ? 1 : 0;
						return 1 + 2 * ((crf1 == kGolden || crf2 == kGolden) ? 1 : 0);
					}
					if (a0 == kLast && l0 == kLast)
						return 3;
					if (a0 == kLast || l0 == kLast) {
						const int32 edge0 = (a0 == kLast) ? l0 : a0;
						return 4 * (edge0 == kGolden ? 1 : 0);
					}
					return 2 * (a0 == kGolden ? 1 : 0) + 2 * (l0 == kGolden ? 1 : 0);
				}
				if (hasA || hasL) {
					const MiCell *e = hasA ? above : left;
					if (!e->IsInter() || (e->refFrame[0] == kLast && !e->HasSecondRef()))
						return 2;
					if (!e->HasSecondRef())
						return 4 * (e->refFrame[0] == kGolden ? 1 : 0);
					return 3 * ((e->refFrame[0] == kGolden || e->refFrame[1] == kGolden) ? 1 : 0);
				}
				return 2;
			}

			// --- Lecture des modes/MV/refs (vp9_decodemv.c) ---

			int32 ReadInterMode(NkVp8BoolDecoder &bd, FrameParseState &st, int32 ctx) {
				const int32 idx = bd.GetTree(kVp9InterModeTree, st.fc->interModeProbs[ctx]);
				if (st.counts)
					++st.counts->interMode[ctx][idx];
				return 10 + idx; // NEARESTMV=10
			}

			void ReadRefFrames(NkVp8BoolDecoder &bd, FrameParseState &st, int32 segId, int8 refFrame[2]) {
				if (st.hdr->segEnabled && st.hdr->segFeatureEnabled[segId][2]) { // SEG_LVL_REF_FRAME
					refFrame[0] = (int8)st.hdr->segFeatureData[segId][2];
					refFrame[1] = -1;
					return;
				}
				int32 mode = st.referenceMode;
				if (mode == kVp9ReferenceModeSelect) {
					const int32 rmCtx = GetReferenceModeContext(st);
					mode = bd.GetBool(st.fc->compInterProb[rmCtx]);
					if (st.counts)
						++st.counts->compInter[rmCtx][mode];
				}
				if (mode == kVp9CompoundReference) {
					const int32 idx = st.signBias[st.compFixedRef];
					const int32 ctx = GetPredContextCompRefP(st);
					const int32 bit = bd.GetBool(st.fc->compRefProb[ctx]);
					if (st.counts)
						++st.counts->compRef[ctx][bit];
					refFrame[idx] = (int8)st.compFixedRef;
					refFrame[idx ? 0 : 1] = (int8)st.compVarRef[bit];
				} else {
					const int32 ctx0 = GetPredContextSingleRefP1(st.aboveMi, st.leftMi);
					const int32 bit0 = bd.GetBool(st.fc->singleRefProb[ctx0][0]);
					if (st.counts)
						++st.counts->singleRef[ctx0][0][bit0];
					if (bit0) {
						const int32 ctx1 = GetPredContextSingleRefP2(st.aboveMi, st.leftMi);
						const int32 bit1 = bd.GetBool(st.fc->singleRefProb[ctx1][1]);
						if (st.counts)
							++st.counts->singleRef[ctx1][1][bit1];
						refFrame[0] = (int8)(bit1 ? 3 : 2); // ALTREF : GOLDEN
					} else {
						refFrame[0] = 1; // LAST
					}
					refFrame[1] = -1;
				}
			}

			int32 ReadIsInterBlock(NkVp8BoolDecoder &bd, FrameParseState &st, int32 segId) {
				if (st.hdr->segEnabled && st.hdr->segFeatureEnabled[segId][2])
					return st.hdr->segFeatureData[segId][2] != 0; // != INTRA_FRAME(0)
				const int32 ctx = GetIntraInterContext(st.aboveMi, st.leftMi);
				const int32 isInter = bd.GetBool(st.fc->intraInterProb[ctx]);
				if (st.counts)
					++st.counts->intraInter[ctx][isInter];
				return isInter;
			}

			int32 ReadSwitchableInterpFilter(NkVp8BoolDecoder &bd, FrameParseState &st) {
				const int32 ctx = GetSwitchableInterpContext(st.aboveMi, st.leftMi);
				const int32 type = bd.GetTree(kVp9SwitchableInterpTree, st.fc->switchableInterpProb[ctx]);
				if (st.counts)
					++st.counts->switchableInterp[ctx][type];
				return type;
			}

			int32 ReadMvComponent(NkVp8BoolDecoder &bd, const NkVp9NmvComponent &c, bool useHp,
								  NkVp9NmvComponentCounts *cc) {
				const int32 sign = bd.GetBool(c.sign);
				const int32 cls = bd.GetTree(kVp9MvClassTree, c.classes);
				const bool class0 = (cls == 0);
				int32 d, mag;
				if (class0) {
					d = bd.GetBool(c.class0[0]);
					mag = 0;
				} else {
					const int32 n = cls + 1 - 1; // MV_CLASS + CLASS0_BITS - 1 = cls
					d = 0;
					for (int32 i = 0; i < n; ++i) {
						const int32 bit = bd.GetBool(c.bits[i]);
						d |= bit << i;
						if (cc)
							++cc->bits[i][bit];
					}
					mag = 2 << (cls + 2); // CLASS0_SIZE << (cls+2)
				}
				const int32 fr = bd.GetTree(kVp9MvFrTree, class0 ? c.class0Fr[d] : c.fr);
				const int32 hp = useHp ? bd.GetBool(class0 ? c.class0Hp : c.hp) : 1;
				if (cc) {
					++cc->sign[sign];
					++cc->classes[cls];
					if (class0) {
						++cc->class0[d];
						++cc->class0Fr[d][fr];
						if (useHp)
							++cc->class0Hp[hp];
					} else {
						++cc->fr[fr];
						if (useHp)
							++cc->hp[hp];
					}
				}
				mag += ((d << 3) | (fr << 1) | hp) + 1;
				return sign ? -mag : mag;
			}

			void ReadMv(NkVp8BoolDecoder &bd, FrameParseState &st, SimpleMv &mv, const SimpleMv &ref,
					   bool allowHp) {
				const NkVp9FrameContext &fc = *st.fc;
				const int32 joint = bd.GetTree(kVp9MvJointTree, fc.nmvJoints);
				if (st.counts)
					++st.counts->mvJoints[joint];
				const bool useHp = allowHp && UseMvHp(ref);
				int32 dr = 0, dc = 0;
				if (joint == 2 || joint == 3) // HZVNZ, HNZVNZ : composante verticale
					dr = ReadMvComponent(bd, fc.nmvComps[0], useHp, st.counts ? &st.counts->mvComps[0] : nullptr);
				if (joint == 1 || joint == 3) // HNZVZ, HNZVNZ : composante horizontale
					dc = ReadMvComponent(bd, fc.nmvComps[1], useHp, st.counts ? &st.counts->mvComps[1] : nullptr);
				mv.row = (int16)(ref.row + dr);
				mv.col = (int16)(ref.col + dc);
			}

			bool IsMvValid(const SimpleMv &mv) {
				return mv.row > kMvLow && mv.row < kMvUpp && mv.col > kMvLow && mv.col < kMvUpp;
			}

			// assign_mv : NEWMV lit un delta ; NEAR(EST)MV recopie le candidat ; ZEROMV = 0.
			bool AssignMv(NkVp8BoolDecoder &bd, FrameParseState &st, int32 mode, SimpleMv mv[2],
						 SimpleMv refMv[2], SimpleMv nearNearestMv[2], bool isCompound, bool allowHp) {
				switch (mode) {
					case 13: // NEWMV
						for (int32 i = 0; i < (isCompound ? 2 : 1); ++i) {
							ReadMv(bd, st, mv[i], refMv[i], allowHp);
							if (!IsMvValid(mv[i]))
								return false;
						}
						return true;
					case 11: // NEARMV
					case 10: // NEARESTMV
						mv[0] = nearNearestMv[0];
						mv[1] = nearNearestMv[1];
						return true;
					case 12: // ZEROMV
						mv[0] = SimpleMv{};
						mv[1] = SimpleMv{};
						return true;
					default:
						return false;
				}
			}

			// get_mode_context (decodemv.c) : contexte inter_mode depuis les 2 voisins
			// immédiats (mode_2_counter/counter_to_context, tables extraites).
			int32 GetModeContext(const FrameParseState &st, const int8 mvRefSearch[8][2], int32 miRow,
								 int32 miCol) {
				int32 counter = 0;
				for (int32 i = 0; i < 2; ++i) {
					const int32 pr = mvRefSearch[i][0], pc = mvRefSearch[i][1];
					if (!IsInsideTile(st, miRow, miCol, pr, pc))
						continue;
					const MiCell &cand = st.mi[(miRow + pr) * st.miCols + (miCol + pc)];
					counter += kVp9Mode2Counter[cand.mode];
				}
				return kVp9CounterToContext[counter];
			}

			// read_inter_block_mode_info : modes + MV d'un bloc inter (>=8x8 et sub8x8).
			void ReadInterBlockModeInfo(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol) {
				MiCell &mi = st.cur;
				const bool allowHp = st.allowHighPrecisionMv;
				SimpleMv bestRefMvs[2];
				ReadRefFrames(bd, st, mi.segId, mi.refFrame);
				const bool isCompound = mi.HasSecondRef();
				const int8(*mvRefSearch)[2] = kVp9MvRefBlocks[mi.sbType];
				const int32 interModeCtx = GetModeContext(st, mvRefSearch, miRow, miCol);

				const bool segSkip = st.hdr->segEnabled && st.hdr->segFeatureEnabled[mi.segId][3];
				if (segSkip) {
					mi.mode = 12; // ZEROMV
				} else if (mi.sbType >= kBlock8x8) {
					mi.mode = (uint8)ReadInterMode(bd, st, interModeCtx);
				}

				mi.interpFilter = (st.interpFilterHdr == 4) ? (uint8)ReadSwitchableInterpFilter(bd, st)
															 : (uint8)st.interpFilterHdr;

				if (mi.sbType < kBlock8x8) {
					int32 bModeLast = 0;
					bool gotMvRefsForNew = false;
					SimpleMv bestSub8x8[2] = {SimpleMv{}, SimpleMv{}};
					// Dimensions du bloc en unités 4x4 (BLOCK_4X4=0 : 1x1, BLOCK_4X8=1 : 1
					// col × 2 rangées, BLOCK_8X4=2 : 2 col × 1 rangée).
					const int32 num4x4w = (mi.sbType == 2) ? 2 : 1;
					const int32 num4x4h = (mi.sbType == 1) ? 2 : 1;
					for (int32 idy = 0; idy < 2; idy += num4x4h) {
						for (int32 idx = 0; idx < 2; idx += num4x4w) {
							const int32 j = idy * 2 + idx;
							const int32 bMode = ReadInterMode(bd, st, interModeCtx);
							bModeLast = bMode;
							if (bMode == 10 || bMode == 11) { // NEARESTMV/NEARMV
								for (int32 ref = 0; ref < (isCompound ? 2 : 1); ++ref)
									AppendSub8x8Mvs(st, mvRefSearch, bMode, j, ref, miRow, miCol, bestSub8x8[ref]);
							} else if (bMode == 13 && !gotMvRefsForNew) { // NEWMV
								for (int32 ref = 0; ref < (isCompound ? 2 : 1); ++ref) {
									SimpleMv tmp[2];
									DecFindMvRefs(st, 13, mi.refFrame[ref], mvRefSearch, tmp, miRow, miCol, -1);
									// lower_mv_precision.
									if (!(allowHp && UseMvHp(tmp[0]))) {
										if (tmp[0].row & 1)
											tmp[0].row += (tmp[0].row > 0 ? -1 : 1);
										if (tmp[0].col & 1)
											tmp[0].col += (tmp[0].col > 0 ? -1 : 1);
									}
									bestRefMvs[ref] = tmp[0];
								}
								gotMvRefsForNew = true;
							}
							SimpleMv mvOut[2];
							if (!AssignMv(bd, st, bMode, mvOut, bestRefMvs, bestSub8x8, isCompound, allowHp)) {
								mvOut[0] = SimpleMv{};
								mvOut[1] = SimpleMv{};
							}
							mi.bmiMv[j][0] = mvOut[0];
							mi.bmiMv[j][1] = mvOut[1];
							mi.bmi[j] = (uint8)bMode;
							if (num4x4h == 2) {
								mi.bmiMv[j + 2][0] = mvOut[0];
								mi.bmiMv[j + 2][1] = mvOut[1];
								mi.bmi[j + 2] = (uint8)bMode;
							}
							if (num4x4w == 2) {
								mi.bmiMv[j + 1][0] = mvOut[0];
								mi.bmiMv[j + 1][1] = mvOut[1];
								mi.bmi[j + 1] = (uint8)bMode;
							}
						}
					}
					mi.mode = (uint8)bModeLast;
					mi.mv[0] = mi.bmiMv[3][0];
					mi.mv[1] = mi.bmiMv[3][1];
				} else {
					if (mi.mode != 12) { // != ZEROMV
						for (int32 ref = 0; ref < (isCompound ? 2 : 1); ++ref) {
							SimpleMv tmp[2];
							const int32 n =
								DecFindMvRefs(st, mi.mode, mi.refFrame[ref], mvRefSearch, tmp, miRow, miCol, -1);
							SimpleMv &best = tmp[n - 1];
							if (!(allowHp && UseMvHp(best))) {
								if (best.row & 1)
									best.row += (best.row > 0 ? -1 : 1);
								if (best.col & 1)
									best.col += (best.col > 0 ? -1 : 1);
							}
							bestRefMvs[ref] = best;
						}
					}
					SimpleMv mvOut[2];
					if (!AssignMv(bd, st, mi.mode, mvOut, bestRefMvs, bestRefMvs, isCompound, allowHp)) {
						mvOut[0] = SimpleMv{};
						mvOut[1] = SimpleMv{};
					}
					mi.mv[0] = mvOut[0];
					mi.mv[1] = mvOut[1];
				}
			}

			// =================================================================
			// BRIQUE 4 — prédiction intra + reconstruction (port fidèle de
			// vpx_dsp/intrapred.c + vp9_reconintra.c).
			// =================================================================

			inline uint8 Avg2(int32 a, int32 b) {
				return (uint8)((a + b + 1) >> 1);
			}
			inline uint8 Avg3(int32 a, int32 b, int32 c) {
				return (uint8)((a + 2 * b + c + 2) >> 2);
			}
			inline uint8 ClipPixel(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}

			// Prédicteurs génériques (bs = 4/8/16/32).
			void PredV(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				(void)left;
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = above[c];
			}
			void PredH(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				(void)above;
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = left[r];
			}
			void PredTm(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				const int32 topLeft = above[-1];
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = ClipPixel(left[r] + above[c] - topLeft);
			}
			void PredDc128(uint8 *dst, int32 stride, int32 bs, const uint8 *, const uint8 *) {
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = 128;
			}
			void PredDcLeft(uint8 *dst, int32 stride, int32 bs, const uint8 *, const uint8 *left) {
				int32 sum = 0;
				for (int32 i = 0; i < bs; ++i)
					sum += left[i];
				const uint8 dc = (uint8)((sum + (bs >> 1)) / bs);
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = dc;
			}
			void PredDcTop(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *) {
				int32 sum = 0;
				for (int32 i = 0; i < bs; ++i)
					sum += above[i];
				const uint8 dc = (uint8)((sum + (bs >> 1)) / bs);
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = dc;
			}
			void PredDc(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				int32 sum = 0;
				for (int32 i = 0; i < bs; ++i)
					sum += above[i] + left[i];
				const uint8 dc = (uint8)((sum + bs) / (2 * bs));
				for (int32 r = 0; r < bs; ++r, dst += stride)
					for (int32 c = 0; c < bs; ++c)
						dst[c] = dc;
			}
			void PredD45(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				(void)left;
				const uint8 aboveRight = above[bs - 1];
				uint8 *row0 = dst;
				for (int32 x = 0; x < bs - 1; ++x)
					dst[x] = Avg3(above[x], above[x + 1], above[x + 2]);
				dst[bs - 1] = aboveRight;
				dst += stride;
				for (int32 x = 1, size = bs - 2; x < bs; ++x, --size) {
					for (int32 i = 0; i < size; ++i)
						dst[i] = row0[x + i];
					for (int32 i = 0; i < x + 1; ++i)
						dst[size + i] = aboveRight;
					dst += stride;
				}
			}
			void PredD63(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				(void)left;
				for (int32 c = 0; c < bs; ++c) {
					dst[c] = Avg2(above[c], above[c + 1]);
					dst[stride + c] = Avg3(above[c], above[c + 1], above[c + 2]);
				}
				for (int32 r = 2, size = bs - 2; r < bs; r += 2, --size) {
					for (int32 i = 0; i < size; ++i)
						dst[(r + 0) * stride + i] = dst[(r >> 1) + i];
					for (int32 i = size; i < bs; ++i)
						dst[(r + 0) * stride + i] = above[bs - 1];
					for (int32 i = 0; i < size; ++i)
						dst[(r + 1) * stride + i] = dst[stride + (r >> 1) + i];
					for (int32 i = size; i < bs; ++i)
						dst[(r + 1) * stride + i] = above[bs - 1];
				}
			}
			void PredD117(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				// 1re rangée.
				for (int32 c = 0; c < bs; ++c)
					dst[c] = Avg2(above[c - 1], above[c]);
				dst += stride;
				// 2e rangée.
				dst[0] = Avg3(left[0], above[-1], above[0]);
				for (int32 c = 1; c < bs; ++c)
					dst[c] = Avg3(above[c - 2], above[c - 1], above[c]);
				dst += stride;
				// Reste de la 1re colonne.
				dst[0] = Avg3(above[-1], left[0], left[1]);
				for (int32 r = 3; r < bs; ++r)
					dst[(r - 2) * stride] = Avg3(left[r - 3], left[r - 2], left[r - 1]);
				// Reste du bloc.
				for (int32 r = 2; r < bs; ++r) {
					for (int32 c = 1; c < bs; ++c)
						dst[c] = dst[-2 * stride + c - 1];
					dst += stride;
				}
			}
			void PredD135(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				uint8 border[32 + 32 - 1]; // bordure externe, du bas-gauche au haut-droit
				for (int32 i = 0; i < bs - 2; ++i)
					border[i] = Avg3(left[bs - 3 - i], left[bs - 2 - i], left[bs - 1 - i]);
				border[bs - 2] = Avg3(above[-1], left[0], left[1]);
				border[bs - 1] = Avg3(left[0], above[-1], above[0]);
				border[bs - 0] = Avg3(above[-1], above[0], above[1]);
				for (int32 i = 0; i < bs - 2; ++i)
					border[bs + 1 + i] = Avg3(above[i], above[i + 1], above[i + 2]);
				for (int32 i = 0; i < bs; ++i)
					for (int32 c = 0; c < bs; ++c)
						dst[i * stride + c] = border[bs - 1 - i + c];
			}
			void PredD153(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				dst[0] = Avg2(above[-1], left[0]);
				for (int32 r = 1; r < bs; ++r)
					dst[r * stride] = Avg2(left[r - 1], left[r]);
				++dst;
				dst[0] = Avg3(left[0], above[-1], above[0]);
				dst[stride] = Avg3(above[-1], left[0], left[1]);
				for (int32 r = 2; r < bs; ++r)
					dst[r * stride] = Avg3(left[r - 2], left[r - 1], left[r]);
				++dst;
				for (int32 c = 0; c < bs - 2; ++c)
					dst[c] = Avg3(above[c - 1], above[c], above[c + 1]);
				dst += stride;
				for (int32 r = 1; r < bs; ++r) {
					for (int32 c = 0; c < bs - 2; ++c)
						dst[c] = dst[-stride + c - 2];
					dst += stride;
				}
			}
			void PredD207(uint8 *dst, int32 stride, int32 bs, const uint8 *above, const uint8 *left) {
				(void)above;
				// 1re colonne.
				for (int32 r = 0; r < bs - 1; ++r)
					dst[r * stride] = Avg2(left[r], left[r + 1]);
				dst[(bs - 1) * stride] = left[bs - 1];
				++dst;
				// 2e colonne.
				for (int32 r = 0; r < bs - 2; ++r)
					dst[r * stride] = Avg3(left[r], left[r + 1], left[r + 2]);
				dst[(bs - 2) * stride] = Avg3(left[bs - 2], left[bs - 1], left[bs - 1]);
				dst[(bs - 1) * stride] = left[bs - 1];
				++dst;
				// Reste de la dernière rangée.
				for (int32 c = 0; c < bs - 2; ++c)
					dst[(bs - 1) * stride + c] = left[bs - 1];
				for (int32 r = bs - 2; r >= 0; --r)
					for (int32 c = 0; c < bs - 2; ++c)
						dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
			}

			// --- Variantes 4x4 DÉDIÉES des modes directionnels (intrapred.c) : les
			// coins utilisent l'above-right RÉEL (E..H) au lieu de la réplication du
			// générique (« differs from vp8 »). Sans elles : coins faux en 4x4.
#define NK_DST(x, y) dst[(x) + (y)*stride]
			void PredD207_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 I = left[0], J = left[1], K = left[2], L = left[3];
				(void)above;
				NK_DST(0, 0) = Avg2(I, J);
				NK_DST(2, 0) = NK_DST(0, 1) = Avg2(J, K);
				NK_DST(2, 1) = NK_DST(0, 2) = Avg2(K, L);
				NK_DST(1, 0) = Avg3(I, J, K);
				NK_DST(3, 0) = NK_DST(1, 1) = Avg3(J, K, L);
				NK_DST(3, 1) = NK_DST(1, 2) = Avg3(K, L, L);
				NK_DST(3, 2) = NK_DST(2, 2) = NK_DST(0, 3) = NK_DST(1, 3) = NK_DST(2, 3) =
					NK_DST(3, 3) = (uint8)L;
			}
			void PredD63_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 A = above[0], B = above[1], C = above[2], D = above[3];
				const int32 E = above[4], F = above[5], G = above[6];
				(void)left;
				NK_DST(0, 0) = Avg2(A, B);
				NK_DST(1, 0) = NK_DST(0, 2) = Avg2(B, C);
				NK_DST(2, 0) = NK_DST(1, 2) = Avg2(C, D);
				NK_DST(3, 0) = NK_DST(2, 2) = Avg2(D, E);
				NK_DST(3, 2) = Avg2(E, F);
				NK_DST(0, 1) = Avg3(A, B, C);
				NK_DST(1, 1) = NK_DST(0, 3) = Avg3(B, C, D);
				NK_DST(2, 1) = NK_DST(1, 3) = Avg3(C, D, E);
				NK_DST(3, 1) = NK_DST(2, 3) = Avg3(D, E, F);
				NK_DST(3, 3) = Avg3(E, F, G);
			}
			void PredD45_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 A = above[0], B = above[1], C = above[2], D = above[3];
				const int32 E = above[4], F = above[5], G = above[6], H = above[7];
				(void)left;
				NK_DST(0, 0) = Avg3(A, B, C);
				NK_DST(1, 0) = NK_DST(0, 1) = Avg3(B, C, D);
				NK_DST(2, 0) = NK_DST(1, 1) = NK_DST(0, 2) = Avg3(C, D, E);
				NK_DST(3, 0) = NK_DST(2, 1) = NK_DST(1, 2) = NK_DST(0, 3) = Avg3(D, E, F);
				NK_DST(3, 1) = NK_DST(2, 2) = NK_DST(1, 3) = Avg3(E, F, G);
				NK_DST(3, 2) = NK_DST(2, 3) = Avg3(F, G, H);
				NK_DST(3, 3) = (uint8)H;
			}
			void PredD117_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 I = left[0], J = left[1], K = left[2];
				const int32 X = above[-1], A = above[0], B = above[1], C = above[2], D = above[3];
				NK_DST(0, 0) = NK_DST(1, 2) = Avg2(X, A);
				NK_DST(1, 0) = NK_DST(2, 2) = Avg2(A, B);
				NK_DST(2, 0) = NK_DST(3, 2) = Avg2(B, C);
				NK_DST(3, 0) = Avg2(C, D);
				NK_DST(0, 3) = Avg3(K, J, I);
				NK_DST(0, 2) = Avg3(J, I, X);
				NK_DST(0, 1) = NK_DST(1, 3) = Avg3(I, X, A);
				NK_DST(1, 1) = NK_DST(2, 3) = Avg3(X, A, B);
				NK_DST(2, 1) = NK_DST(3, 3) = Avg3(A, B, C);
				NK_DST(3, 1) = Avg3(B, C, D);
			}
			void PredD135_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 I = left[0], J = left[1], K = left[2], L = left[3];
				const int32 X = above[-1], A = above[0], B = above[1], C = above[2], D = above[3];
				NK_DST(0, 3) = Avg3(J, K, L);
				NK_DST(1, 3) = NK_DST(0, 2) = Avg3(I, J, K);
				NK_DST(2, 3) = NK_DST(1, 2) = NK_DST(0, 1) = Avg3(X, I, J);
				NK_DST(3, 3) = NK_DST(2, 2) = NK_DST(1, 1) = NK_DST(0, 0) = Avg3(A, X, I);
				NK_DST(3, 2) = NK_DST(2, 1) = NK_DST(1, 0) = Avg3(B, A, X);
				NK_DST(3, 1) = NK_DST(2, 0) = Avg3(C, B, A);
				NK_DST(3, 0) = Avg3(D, C, B);
			}
			void PredD153_4x4(uint8 *dst, int32 stride, const uint8 *above, const uint8 *left) {
				const int32 I = left[0], J = left[1], K = left[2], L = left[3];
				const int32 X = above[-1], A = above[0], B = above[1], C = above[2];
				NK_DST(0, 0) = NK_DST(2, 1) = Avg2(I, X);
				NK_DST(0, 1) = NK_DST(2, 2) = Avg2(J, I);
				NK_DST(0, 2) = NK_DST(2, 3) = Avg2(K, J);
				NK_DST(0, 3) = Avg2(L, K);
				NK_DST(3, 0) = Avg3(A, B, C);
				NK_DST(2, 0) = Avg3(X, A, B);
				NK_DST(1, 0) = NK_DST(3, 1) = Avg3(I, X, A);
				NK_DST(1, 1) = NK_DST(3, 2) = Avg3(J, I, X);
				NK_DST(1, 2) = NK_DST(3, 3) = Avg3(K, J, I);
				NK_DST(1, 3) = Avg3(L, K, J);
			}
#undef NK_DST

			// Besoins de bords par mode : {left, above, aboveRight}.
			const uint8 kNeedLeft[10] = {1, 0, 1, 0, 1, 1, 1, 1, 0, 1};
			const uint8 kNeedAbove[10] = {1, 1, 0, 0, 1, 1, 1, 0, 0, 1};
			const uint8 kNeedAboveRight[10] = {0, 0, 0, 1, 0, 0, 0, 0, 1, 0};

			// build_intra_predictors (vp9_reconintra.c, port fidèle — motif 127/129).
			void PredictIntra(const FrameParseState &st, int32 plane, int32 mode, int32 txSize,
							  uint8 *dst, int32 stride, int32 aoff, int32 loff, int32 frameW,
							  int32 frameH, bool haveTop, bool haveLeft, bool haveRight) {
				uint8 leftCol[32];
				uint8 aboveData[64 + 16];
				uint8 *aboveRow = aboveData + 16;
				const uint8 *constAboveRow = aboveRow;
				const int32 bs = 4 << txSize;
				const int32 ssx = plane ? st.hdr->subsamplingX : 0;
				const int32 ssy = plane ? st.hdr->subsamplingY : 0;
				const int32 x0 = ((-st.mbToLeftEdge) >> (3 + ssx)) + aoff * 4;
				const int32 y0 = ((-st.mbToTopEdge) >> (3 + ssy)) + loff * 4;
				const uint8 *ref = dst;

				if (kNeedLeft[mode]) {
					if (haveLeft) {
						if (st.mbToBottomEdge < 0 && y0 + bs > frameH) {
							const int32 extend = frameH - y0;
							int32 i = 0;
							for (; i < extend; ++i)
								leftCol[i] = ref[i * stride - 1];
							for (; i < bs; ++i)
								leftCol[i] = ref[(extend - 1) * stride - 1];
						} else {
							for (int32 i = 0; i < bs; ++i)
								leftCol[i] = ref[i * stride - 1];
						}
					} else {
						for (int32 i = 0; i < bs; ++i)
							leftCol[i] = 129;
					}
				}

				if (kNeedAbove[mode]) {
					if (haveTop) {
						const uint8 *aboveRef = ref - stride;
						if (st.mbToRightEdge < 0 && x0 + bs > frameW) {
							if (x0 <= frameW) {
								const int32 r = frameW - x0;
								for (int32 i = 0; i < r; ++i)
									aboveRow[i] = aboveRef[i];
								for (int32 i = r; i < bs; ++i)
									aboveRow[i] = aboveRow[r - 1];
							}
						} else {
							for (int32 i = 0; i < bs; ++i)
								aboveRow[i] = aboveRef[i];
						}
						aboveRow[-1] = haveLeft ? aboveRef[-1] : 129;
					} else {
						for (int32 i = 0; i < bs; ++i)
							aboveRow[i] = 127;
						aboveRow[-1] = 127;
					}
				}

				if (kNeedAboveRight[mode]) {
					if (haveTop) {
						const uint8 *aboveRef = ref - stride;
						if (st.mbToRightEdge < 0) {
							// chemin lent : extension au bord de l'image
							if (x0 + 2 * bs <= frameW) {
								if (haveRight && bs == 4) {
									for (int32 i = 0; i < 2 * bs; ++i)
										aboveRow[i] = aboveRef[i];
								} else {
									for (int32 i = 0; i < bs; ++i)
										aboveRow[i] = aboveRef[i];
									for (int32 i = 0; i < bs; ++i)
										aboveRow[bs + i] = aboveRow[bs - 1];
								}
							} else if (x0 + bs <= frameW) {
								const int32 r = frameW - x0;
								if (haveRight && bs == 4) {
									for (int32 i = 0; i < r; ++i)
										aboveRow[i] = aboveRef[i];
									for (int32 i = r; i < 2 * bs; ++i)
										aboveRow[i] = aboveRow[r - 1];
								} else {
									for (int32 i = 0; i < bs; ++i)
										aboveRow[i] = aboveRef[i];
									for (int32 i = 0; i < bs; ++i)
										aboveRow[bs + i] = aboveRow[bs - 1];
								}
							} else if (x0 <= frameW) {
								const int32 r = frameW - x0;
								for (int32 i = 0; i < r; ++i)
									aboveRow[i] = aboveRef[i];
								for (int32 i = r; i < 2 * bs; ++i)
									aboveRow[i] = aboveRow[r - 1];
							}
						} else {
							// chemin rapide
							for (int32 i = 0; i < bs; ++i)
								aboveRow[i] = aboveRef[i];
							if (bs == 4 && haveRight)
								for (int32 i = 0; i < bs; ++i)
									aboveRow[bs + i] = aboveRef[bs + i];
							else
								for (int32 i = 0; i < bs; ++i)
									aboveRow[bs + i] = aboveRow[bs - 1];
						}
						aboveRow[-1] = haveLeft ? aboveRef[-1] : 129;
					} else {
						for (int32 i = 0; i < 2 * bs; ++i)
							aboveRow[i] = 127;
						aboveRow[-1] = 127;
					}
				}

				// Dispatch.
				if (mode == 0) { // DC : 4 variantes selon la disponibilité
					if (haveLeft && haveTop)
						PredDc(dst, stride, bs, constAboveRow, leftCol);
					else if (haveLeft)
						PredDcLeft(dst, stride, bs, constAboveRow, leftCol);
					else if (haveTop)
						PredDcTop(dst, stride, bs, constAboveRow, leftCol);
					else
						PredDc128(dst, stride, bs, constAboveRow, leftCol);
				} else if (bs == 4 && mode >= 3 && mode <= 8) {
					// Variantes 4x4 dédiées des modes directionnels.
					typedef void (*Pred4Fn)(uint8 *, int32, const uint8 *, const uint8 *);
					static const Pred4Fn kPred4[6] = {PredD45_4x4,	PredD135_4x4, PredD117_4x4,
													  PredD153_4x4, PredD207_4x4, PredD63_4x4};
					kPred4[mode - 3](dst, stride, constAboveRow, leftCol);
				} else {
					typedef void (*PredFn)(uint8 *, int32, int32, const uint8 *, const uint8 *);
					static const PredFn kPred[10] = {nullptr, PredV,	PredH,	 PredD45, PredD135,
													 PredD117, PredD153, PredD207, PredD63, PredTm};
					kPred[mode](dst, stride, bs, constAboveRow, leftCol);
				}
			}

			// =================================================================
			// BRIQUE 5 — LOOP FILTER (port fidèle vpx_dsp/loopfilter.c +
			// vp9_loopfilter.c chemin générique filter_block_plane_non420).
			// =================================================================

			inline int8 SignedCharClamp(int32 t) {
				return (int8)(t < -128 ? -128 : (t > 127 ? 127 : t));
			}
			inline int32 IAbs(int32 v) {
				return v < 0 ? -v : v;
			}

			// Faut-il filtrer du tout ? (~0 oui, 0 non)
			int8 LfFilterMask(uint8 limit, uint8 blimit, uint8 p3, uint8 p2, uint8 p1, uint8 p0,
							  uint8 q0, uint8 q1, uint8 q2, uint8 q3) {
				int8 mask = 0;
				mask |= (int8)((IAbs(p3 - p2) > limit) * -1);
				mask |= (int8)((IAbs(p2 - p1) > limit) * -1);
				mask |= (int8)((IAbs(p1 - p0) > limit) * -1);
				mask |= (int8)((IAbs(q1 - q0) > limit) * -1);
				mask |= (int8)((IAbs(q2 - q1) > limit) * -1);
				mask |= (int8)((IAbs(q3 - q2) > limit) * -1);
				mask |= (int8)((IAbs(p0 - q0) * 2 + IAbs(p1 - q1) / 2 > blimit) * -1);
				return (int8)~mask;
			}
			int8 LfFlatMask4(uint8 thresh, uint8 p3, uint8 p2, uint8 p1, uint8 p0, uint8 q0,
							 uint8 q1, uint8 q2, uint8 q3) {
				int8 mask = 0;
				mask |= (int8)((IAbs(p1 - p0) > thresh) * -1);
				mask |= (int8)((IAbs(q1 - q0) > thresh) * -1);
				mask |= (int8)((IAbs(p2 - p0) > thresh) * -1);
				mask |= (int8)((IAbs(q2 - q0) > thresh) * -1);
				mask |= (int8)((IAbs(p3 - p0) > thresh) * -1);
				mask |= (int8)((IAbs(q3 - q0) > thresh) * -1);
				return (int8)~mask;
			}
			int8 LfFlatMask5(uint8 thresh, uint8 p4, uint8 p3, uint8 p2, uint8 p1, uint8 p0,
							 uint8 q0, uint8 q1, uint8 q2, uint8 q3, uint8 q4) {
				int8 mask = (int8)~LfFlatMask4(thresh, p3, p2, p1, p0, q0, q1, q2, q3);
				mask |= (int8)((IAbs(p4 - p0) > thresh) * -1);
				mask |= (int8)((IAbs(q4 - q0) > thresh) * -1);
				return (int8)~mask;
			}
			int8 LfHevMask(uint8 thresh, uint8 p1, uint8 p0, uint8 q0, uint8 q1) {
				int8 hev = 0;
				hev |= (int8)((IAbs(p1 - p0) > thresh) * -1);
				hev |= (int8)((IAbs(q1 - q0) > thresh) * -1);
				return hev;
			}

			void LfFilter4(int8 mask, uint8 thresh, uint8 *op1, uint8 *op0, uint8 *oq0, uint8 *oq1) {
				const int8 ps1 = (int8)(*op1 ^ 0x80);
				const int8 ps0 = (int8)(*op0 ^ 0x80);
				const int8 qs0 = (int8)(*oq0 ^ 0x80);
				const int8 qs1 = (int8)(*oq1 ^ 0x80);
				const int8 hev = LfHevMask(thresh, *op1, *op0, *oq0, *oq1);
				// taps externes seulement en forte variance (hev)
				int8 filter = (int8)(SignedCharClamp(ps1 - qs1) & hev);
				filter = (int8)(SignedCharClamp(filter + 3 * (qs0 - ps0)) & mask);
				// arrondi asymétrique +4/+3
				const int8 filter1 = (int8)(SignedCharClamp(filter + 4) >> 3);
				const int8 filter2 = (int8)(SignedCharClamp(filter + 3) >> 3);
				*oq0 = (uint8)(SignedCharClamp(qs0 - filter1) ^ 0x80);
				*op0 = (uint8)(SignedCharClamp(ps0 + filter2) ^ 0x80);
				filter = (int8)(((filter1 + 1) >> 1) & (int8)~hev);
				*oq1 = (uint8)(SignedCharClamp(qs1 - filter) ^ 0x80);
				*op1 = (uint8)(SignedCharClamp(ps1 + filter) ^ 0x80);
			}

			void LfFilter8(int8 mask, uint8 thresh, int8 flat, uint8 *op3, uint8 *op2, uint8 *op1,
						   uint8 *op0, uint8 *oq0, uint8 *oq1, uint8 *oq2, uint8 *oq3) {
				if (flat && mask) {
					const uint8 p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
					const uint8 q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;
					// filtre 7 taps [1,1,1,2,1,1,1]
					*op2 = (uint8)((p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0 + 4) >> 3);
					*op1 = (uint8)((p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1 + 4) >> 3);
					*op0 = (uint8)((p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2 + 4) >> 3);
					*oq0 = (uint8)((p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3 + 4) >> 3);
					*oq1 = (uint8)((p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3 + 4) >> 3);
					*oq2 = (uint8)((p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3 + 4) >> 3);
				} else {
					LfFilter4(mask, thresh, op1, op0, oq0, oq1);
				}
			}

			void LfFilter16(int8 mask, uint8 thresh, int8 flat, int8 flat2, uint8 *op7, uint8 *op6,
							uint8 *op5, uint8 *op4, uint8 *op3, uint8 *op2, uint8 *op1, uint8 *op0,
							uint8 *oq0, uint8 *oq1, uint8 *oq2, uint8 *oq3, uint8 *oq4, uint8 *oq5,
							uint8 *oq6, uint8 *oq7) {
				if (flat2 && flat && mask) {
					const uint8 p7 = *op7, p6 = *op6, p5 = *op5, p4 = *op4, p3 = *op3, p2 = *op2,
								p1 = *op1, p0 = *op0;
					const uint8 q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3, q4 = *oq4, q5 = *oq5,
								q6 = *oq6, q7 = *oq7;
					// filtre 15 taps [1×7, 2, 1×7]
					*op6 = (uint8)((p7 * 7 + p6 * 2 + p5 + p4 + p3 + p2 + p1 + p0 + q0 + 8) >> 4);
					*op5 = (uint8)((p7 * 6 + p6 + p5 * 2 + p4 + p3 + p2 + p1 + p0 + q0 + q1 + 8) >> 4);
					*op4 = (uint8)((p7 * 5 + p6 + p5 + p4 * 2 + p3 + p2 + p1 + p0 + q0 + q1 + q2 + 8) >> 4);
					*op3 = (uint8)((p7 * 4 + p6 + p5 + p4 + p3 * 2 + p2 + p1 + p0 + q0 + q1 + q2 + q3 + 8) >> 4);
					*op2 = (uint8)((p7 * 3 + p6 + p5 + p4 + p3 + p2 * 2 + p1 + p0 + q0 + q1 + q2 + q3 + q4 + 8) >> 4);
					*op1 = (uint8)((p7 * 2 + p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 + q0 + q1 + q2 + q3 + q4 + q5 + 8) >> 4);
					*op0 = (uint8)((p7 + p6 + p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 + q1 + q2 + q3 + q4 + q5 + q6 + 8) >> 4);
					*oq0 = (uint8)((p6 + p5 + p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 + q2 + q3 + q4 + q5 + q6 + q7 + 8) >> 4);
					*oq1 = (uint8)((p5 + p4 + p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 + q3 + q4 + q5 + q6 + q7 * 2 + 8) >> 4);
					*oq2 = (uint8)((p4 + p3 + p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 + q4 + q5 + q6 + q7 * 3 + 8) >> 4);
					*oq3 = (uint8)((p3 + p2 + p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 + q5 + q6 + q7 * 4 + 8) >> 4);
					*oq4 = (uint8)((p2 + p1 + p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 + q6 + q7 * 5 + 8) >> 4);
					*oq5 = (uint8)((p1 + p0 + q0 + q1 + q2 + q3 + q4 + q5 * 2 + q6 + q7 * 6 + 8) >> 4);
					*oq6 = (uint8)((p0 + q0 + q1 + q2 + q3 + q4 + q5 + q6 * 2 + q7 * 7 + 8) >> 4);
				} else {
					LfFilter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3);
				}
			}

			// Arête HORIZONTALE (pixels au-dessus/en-dessous), sur `count` colonnes.
			void LpfHorizontal(uint8 *s, int32 pitch, uint8 blimit, uint8 limit, uint8 thresh,
							   int32 kind /*0=4taps,1=8taps,2=16taps*/, int32 count) {
				for (int32 i = 0; i < count; ++i) {
					const uint8 p3 = s[-4 * pitch], p2 = s[-3 * pitch], p1 = s[-2 * pitch], p0 = s[-pitch];
					const uint8 q0 = s[0], q1 = s[1 * pitch], q2 = s[2 * pitch], q3 = s[3 * pitch];
					const int8 mask = LfFilterMask(limit, blimit, p3, p2, p1, p0, q0, q1, q2, q3);
					if (kind == 0) {
						LfFilter4(mask, thresh, s - 2 * pitch, s - 1 * pitch, s, s + 1 * pitch);
					} else if (kind == 1) {
						const int8 flat = LfFlatMask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
						LfFilter8(mask, thresh, flat, s - 4 * pitch, s - 3 * pitch, s - 2 * pitch,
								  s - 1 * pitch, s, s + 1 * pitch, s + 2 * pitch, s + 3 * pitch);
					} else {
						const int8 flat = LfFlatMask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
						const int8 flat2 = LfFlatMask5(1, s[-8 * pitch], s[-7 * pitch], s[-6 * pitch],
													   s[-5 * pitch], p0, q0, s[4 * pitch],
													   s[5 * pitch], s[6 * pitch], s[7 * pitch]);
						LfFilter16(mask, thresh, flat, flat2, s - 8 * pitch, s - 7 * pitch,
								   s - 6 * pitch, s - 5 * pitch, s - 4 * pitch, s - 3 * pitch,
								   s - 2 * pitch, s - 1 * pitch, s, s + 1 * pitch, s + 2 * pitch,
								   s + 3 * pitch, s + 4 * pitch, s + 5 * pitch, s + 6 * pitch,
								   s + 7 * pitch);
					}
					++s;
				}
			}

			// Arête VERTICALE (pixels à gauche/droite), sur `count` rangées.
			void LpfVertical(uint8 *s, int32 pitch, uint8 blimit, uint8 limit, uint8 thresh,
							 int32 kind, int32 count) {
				for (int32 i = 0; i < count; ++i) {
					const uint8 p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
					const uint8 q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
					const int8 mask = LfFilterMask(limit, blimit, p3, p2, p1, p0, q0, q1, q2, q3);
					if (kind == 0) {
						LfFilter4(mask, thresh, s - 2, s - 1, s, s + 1);
					} else if (kind == 1) {
						const int8 flat = LfFlatMask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
						LfFilter8(mask, thresh, flat, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3);
					} else {
						const int8 flat = LfFlatMask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
						const int8 flat2 =
							LfFlatMask5(1, s[-8], s[-7], s[-6], s[-5], p0, q0, s[4], s[5], s[6], s[7]);
						LfFilter16(mask, thresh, flat, flat2, s - 8, s - 7, s - 6, s - 5, s - 4, s - 3,
								   s - 2, s - 1, s, s + 1, s + 2, s + 3, s + 4, s + 5, s + 6, s + 7);
					}
					s += pitch;
				}
			}

			// Seuils par niveau 0..63 (update_sharpness + hev_thr = lvl>>4).
			struct LfThresholds {
					uint8 mblim[64], lim[64], hev[64];
					void Build(int32 sharpness) {
						for (int32 lvl = 0; lvl <= 63; ++lvl) {
							int32 blockInsideLimit = lvl >> ((sharpness > 0) + (sharpness > 4));
							if (sharpness > 0 && blockInsideLimit > 9 - sharpness)
								blockInsideLimit = 9 - sharpness;
							if (blockInsideLimit < 1)
								blockInsideLimit = 1;
							lim[lvl] = (uint8)blockInsideLimit;
							mblim[lvl] = (uint8)(2 * (lvl + 2) + blockInsideLimit);
							hev[lvl] = (uint8)(lvl >> 4);
						}
					}
			};

			// filter_selectively_vert : arêtes verticales d'une rangée mi (8 unités 8x8).
			void FilterSelectivelyVert(uint8 *s, int32 pitch, uint32 m16, uint32 m8, uint32 m4,
									   uint32 m4i, const LfThresholds &thr, const uint8 *lfl) {
				for (uint32 mask = m16 | m8 | m4 | m4i; mask; mask >>= 1) {
					const uint8 lvl = *lfl;
					if (mask & 1) {
						if (m16 & 1)
							LpfVertical(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 2, 8);
						else if (m8 & 1)
							LpfVertical(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 1, 8);
						else if (m4 & 1)
							LpfVertical(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 0, 8);
					}
					if (m4i & 1)
						LpfVertical(s + 4, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 0, 8);
					s += 8;
					lfl += 1;
					m16 >>= 1;
					m8 >>= 1;
					m4 >>= 1;
					m4i >>= 1;
				}
			}

			// filter_selectively_horiz : arêtes horizontales (les « duals » = même
			// résultat que 2 appels adjacents ; count=2 quand 2 bits consécutifs).
			void FilterSelectivelyHoriz(uint8 *s, int32 pitch, uint32 m16, uint32 m8, uint32 m4,
										uint32 m4i, const LfThresholds &thr, const uint8 *lfl) {
				int32 count;
				for (uint32 mask = m16 | m8 | m4 | m4i; mask; mask >>= count) {
					count = 1;
					if (mask & 1) {
						const uint8 lvl = *lfl;
						if (m16 & 1) {
							if ((m16 & 3) == 3) {
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 2, 16);
								count = 2;
							} else {
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 2, 8);
							}
						} else if (m8 & 1) {
							if ((m8 & 3) == 3) {
								const uint8 ln = *(lfl + 1);
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 1, 8);
								LpfHorizontal(s + 8, pitch, thr.mblim[ln], thr.lim[ln], thr.hev[ln], 1, 8);
								if ((m4i & 3) == 3) {
									LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
												  thr.hev[lvl], 0, 8);
									LpfHorizontal(s + 8 + 4 * pitch, pitch, thr.mblim[ln], thr.lim[ln],
												  thr.hev[ln], 0, 8);
								} else {
									if (m4i & 1)
										LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
													  thr.hev[lvl], 0, 8);
									else if (m4i & 2)
										LpfHorizontal(s + 8 + 4 * pitch, pitch, thr.mblim[ln],
													  thr.lim[ln], thr.hev[ln], 0, 8);
								}
								count = 2;
							} else {
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 1, 8);
								if (m4i & 1)
									LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
												  thr.hev[lvl], 0, 8);
							}
						} else if (m4 & 1) {
							if ((m4 & 3) == 3) {
								const uint8 ln = *(lfl + 1);
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 0, 8);
								LpfHorizontal(s + 8, pitch, thr.mblim[ln], thr.lim[ln], thr.hev[ln], 0, 8);
								if ((m4i & 3) == 3) {
									LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
												  thr.hev[lvl], 0, 8);
									LpfHorizontal(s + 8 + 4 * pitch, pitch, thr.mblim[ln], thr.lim[ln],
												  thr.hev[ln], 0, 8);
								} else {
									if (m4i & 1)
										LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
													  thr.hev[lvl], 0, 8);
									else if (m4i & 2)
										LpfHorizontal(s + 8 + 4 * pitch, pitch, thr.mblim[ln],
													  thr.lim[ln], thr.hev[ln], 0, 8);
								}
								count = 2;
							} else {
								LpfHorizontal(s, pitch, thr.mblim[lvl], thr.lim[lvl], thr.hev[lvl], 0, 8);
								if (m4i & 1)
									LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
												  thr.hev[lvl], 0, 8);
							}
						} else {
							LpfHorizontal(s + 4 * pitch, pitch, thr.mblim[lvl], thr.lim[lvl],
										  thr.hev[lvl], 0, 8);
						}
					}
					s += 8 * count;
					lfl += count;
					m16 >>= count;
					m8 >>= count;
					m4 >>= count;
					m4i >>= count;
				}
			}

			// mode_lf_lut (loopfilter.c) : 0 pour les 10 modes intra + ZEROMV, 1 pour
			// NEARESTMV/NEARMV/NEWMV (modes 10,11,13).
			const uint8 kModeLfLut[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1};

			// vp9_filter_block_plane_non420 : masques à la volée par superbloc, un plane.
			// `lvlTable[seg][refFrame][modeLfLut]` : niveau par bloc (constant pour les
			// trames clés puisque refFrame=INTRA_FRAME(0)/mode intra→lut=0 partout).
			void FilterBlockPlane(const FrameParseState &st, int32 plane, const uint8 lvlTable[8][4][2],
								  const LfThresholds &thr, int32 miRow, int32 miCol) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				const int32 ssx = plane ? hdr.subsamplingX : 0;
				const int32 ssy = plane ? hdr.subsamplingY : 0;
				const int32 rowStep = 1 << ssy;
				const int32 colStep = 1 << ssx;
				const int32 stride = st.planeStride[plane];
				uint8 *const dst0 = st.planes[plane] +
									(usize)((miRow * 8) >> ssy) * (usize)stride + ((miCol * 8) >> ssx);
				uint8 *dst = dst0;
				uint32 mask16[8] = {0}, mask8[8] = {0}, mask4[8] = {0}, mask4i[8] = {0};
				uint8 lfl[64] = {0};

				for (int32 r = 0; r < 8 && miRow + r < st.miRows; r += rowStep) {
					uint32 m16c = 0, m8c = 0, m4c = 0;
					for (int32 c = 0; c < 8 && miCol + c < st.miCols; c += colStep) {
						const MiCell &mi = st.mi[(miRow + r) * st.miCols + (miCol + c)];
						const int32 sbType = mi.sbType;
						const bool skipThis = mi.skip != 0 && mi.IsInter();
						const int32 blockEdgeLeft =
							(kVp9Num4x4BlocksWide[sbType] > 1)
								? !(c & (kVp9Num8x8BlocksWide[sbType] - 1))
								: 1;
						const bool skipThisC = skipThis && !blockEdgeLeft;
						const int32 blockEdgeAbove =
							(kVp9Num4x4BlocksHigh[sbType] > 1)
								? !(r & (kVp9Num8x8BlocksHigh[sbType] - 1))
								: 1;
						const bool skipThisR = skipThis && !blockEdgeAbove;
						// tx du plane (uv : min avec le max du bloc sous-échantillonné)
						int32 txSize = mi.txSize;
						if (plane) {
							const int32 pbs = kVp9SsSizeLookup[sbType][ssx][ssy];
							txSize = IMin(txSize, (int32)kVp9MaxTxsizeLookup[pbs]);
						}
						const bool skipBorder4x4C = ssx && (miCol + c == st.miCols - 1);
						const bool skipBorder4x4R = ssy && (miRow + r == st.miRows - 1);

						const uint8 level = lvlTable[mi.segId][mi.refFrame[0]][kModeLfLut[mi.mode]];
						lfl[(r << 3) + (c >> ssx)] = level;
						if (!level)
							continue;

						const uint32 bit = 1u << (c >> ssx);
						if (txSize == 3) {
							if (!skipThisC && ((c >> ssx) & 3) == 0) {
								if (!skipBorder4x4C)
									m16c |= bit;
								else
									m8c |= bit;
							}
							if (!skipThisR && ((r >> ssy) & 3) == 0) {
								if (!skipBorder4x4R)
									mask16[r] |= bit;
								else
									mask8[r] |= bit;
							}
						} else if (txSize == 2) {
							if (!skipThisC && ((c >> ssx) & 1) == 0) {
								if (!skipBorder4x4C)
									m16c |= bit;
								else
									m8c |= bit;
							}
							if (!skipThisR && ((r >> ssy) & 1) == 0) {
								if (!skipBorder4x4R)
									mask16[r] |= bit;
								else
									mask8[r] |= bit;
							}
						} else {
							// arêtes 8x8 forcées sur les frontières 32
							if (!skipThisC) {
								if (txSize == 1 || ((c >> ssx) & 3) == 0)
									m8c |= bit;
								else
									m4c |= bit;
							}
							if (!skipThisR) {
								if (txSize == 1 || ((r >> ssy) & 3) == 0)
									mask8[r] |= bit;
								else
									mask4[r] |= bit;
							}
							if (!skipThis && txSize < 1 && !skipBorder4x4C)
								mask4i[r] |= bit;
						}
					}
					// Pas de filtrage sur la toute première colonne de l'image.
					const uint32 borderMask = ~((miCol == 0) ? 1u : 0u);
					FilterSelectivelyVert(dst, stride, m16c & borderMask, m8c & borderMask,
										  m4c & borderMask, mask4i[r], thr, &lfl[r << 3]);
					dst += 8 * stride;
				}

				// Passe horizontale.
				dst = dst0;
				for (int32 r = 0; r < 8 && miRow + r < st.miRows; r += rowStep) {
					const bool skipBorder4x4R = ssy && (miRow + r == st.miRows - 1);
					const uint32 m4ir = skipBorder4x4R ? 0 : mask4i[r];
					uint32 m16r, m8r, m4r;
					if (miRow + r == 0) {
						m16r = m8r = m4r = 0;
					} else {
						m16r = mask16[r];
						m8r = mask8[r];
						m4r = mask4[r];
					}
					FilterSelectivelyHoriz(dst, stride, m16r, m8r, m4r, m4ir, thr, &lfl[r << 3]);
					dst += 8 * stride;
				}
			}

			// vp9_loop_filter_frame_init : niveaux par [segment][ref_frame][mode_lf_lut].
			// Trame clé : seul [s][INTRA_FRAME=0][0] est jamais lu (mode intra → lut=0).
			void BuildLfLevels(const NkVp9FrameHeader &hdr, uint8 lvlTable[8][4][2]) {
				const int32 scale = 1 << (hdr.lfLevel >> 5);
				auto Clamp63 = [](int32 v) { return v < 0 ? 0 : (v > 63 ? 63 : v); };
				for (int32 s = 0; s < 8; ++s) {
					int32 lvlSeg = hdr.lfLevel;
					if (hdr.segEnabled && hdr.segFeatureEnabled[s][1]) { // ALT_LF
						const int32 data = hdr.segFeatureData[s][1];
						lvlSeg = Clamp63(hdr.segAbsDelta ? data : hdr.lfLevel + data);
					}
					if (!hdr.lfDeltaEnabled) {
						for (int32 r = 0; r < 4; ++r)
							for (int32 m = 0; m < 2; ++m)
								lvlTable[s][r][m] = (uint8)lvlSeg;
						continue;
					}
					lvlTable[s][0][0] = (uint8)Clamp63(lvlSeg + hdr.lfRefDeltas[0] * scale); // INTRA_FRAME
					lvlTable[s][0][1] = lvlTable[s][0][0];
					for (int32 ref = 1; ref < 4; ++ref)
						for (int32 m = 0; m < 2; ++m)
							lvlTable[s][ref][m] =
								(uint8)Clamp63(lvlSeg + hdr.lfRefDeltas[ref] * scale + hdr.lfModeDeltas[m] * scale);
				}
			}

			// vp9_loop_filter_rows : superbloc par superbloc, les 3 planes.
			void LoopFilterFrame(const FrameParseState &st) {
				const NkVp9FrameHeader &hdr = *st.hdr;
				if (hdr.lfLevel == 0)
					return;
				LfThresholds thr;
				thr.Build(hdr.lfSharpness);
				uint8 lvlTable[8][4][2];
				BuildLfLevels(hdr, lvlTable);
				for (int32 miRow = 0; miRow < st.miRows; miRow += 8)
					for (int32 miCol = 0; miCol < st.miCols; miCol += 8)
						for (int32 plane = 0; plane < 3; ++plane)
							FilterBlockPlane(st, plane, lvlTable, thr, miRow, miCol);
			}

			// read_partition : arbre complet ou lecture partielle aux bords.
			int32 ReadPartition(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
								bool hasRows, bool hasCols, int32 bsl) {
				// dec_partition_plane_context.
				const int32 above = (st.aboveSegCtx[miCol] >> bsl) & 1;
				const int32 left = (st.leftSegCtx[miRow & 7] >> bsl) & 1;
				const int32 ctx = (left * 2 + above) + bsl * 4; // PARTITION_PLOFFSET = 4
				const uint8 *probs = st.isInterFrame ? st.fc->partitionProb[ctx] : kVp9KfPartitionProbs[ctx];
				int32 p;
				if (hasRows && hasCols)
					p = bd.GetTree(kVp9PartitionTree, probs);
				else if (!hasRows && hasCols)
					p = bd.GetBool(probs[1]) ? 3 : 1; // SPLIT : HORZ
				else if (hasRows && !hasCols)
					p = bd.GetBool(probs[2]) ? 3 : 2; // SPLIT : VERT
				else
					p = 3; // SPLIT forcé (aucun bit lu)
				if (st.counts)
					++st.counts->partition[ctx][p];
				return p;
			}

			// decode_partition récursif (n4x4_l2 : 64x64 → 4).
			void DecodePartition(NkVp8BoolDecoder &bd, FrameParseState &st, int32 miRow, int32 miCol,
								 int32 bsize, int32 n4x4l2) {
				if (miRow >= st.miRows || miCol >= st.miCols)
					return;
				const int32 n8x8l2 = n4x4l2 - 1;
				const int32 num8x8 = 1 << n8x8l2;
				const int32 hbs = num8x8 >> 1;
				const bool hasRows = (miRow + hbs) < st.miRows;
				const bool hasCols = (miCol + hbs) < st.miCols;
				const int32 partition = ReadPartition(bd, st, miRow, miCol, hasRows, hasCols, n8x8l2);
				const int32 subsize = (int32)kVp9SubsizeLookup[partition][bsize];
				if (hbs == 0) {
					// Bloc 8x8 : les sous-tailles < 8x8 restent UN bloc feuille.
					DecodeLeafBlock(bd, st, miRow, miCol, subsize, 1, 1);
				} else {
					switch (partition) {
						case 0: // NONE
							DecodeLeafBlock(bd, st, miRow, miCol, subsize, n4x4l2, n4x4l2);
							break;
						case 1: // HORZ
							DecodeLeafBlock(bd, st, miRow, miCol, subsize, n4x4l2, n8x8l2);
							if (hasRows)
								DecodeLeafBlock(bd, st, miRow + hbs, miCol, subsize, n4x4l2, n8x8l2);
							break;
						case 2: // VERT
							DecodeLeafBlock(bd, st, miRow, miCol, subsize, n8x8l2, n4x4l2);
							if (hasCols)
								DecodeLeafBlock(bd, st, miRow, miCol + hbs, subsize, n8x8l2, n4x4l2);
							break;
						default: // SPLIT
							DecodePartition(bd, st, miRow, miCol, subsize, n8x8l2);
							DecodePartition(bd, st, miRow, miCol + hbs, subsize, n8x8l2);
							DecodePartition(bd, st, miRow + hbs, miCol, subsize, n8x8l2);
							DecodePartition(bd, st, miRow + hbs, miCol + hbs, subsize, n8x8l2);
							break;
					}
				}
				// dec_update_partition_context (8x8 : toujours ; sinon si pas SPLIT).
				// memset(above+miCol, ., num8x8) / memset(left+(miRow&7), ., num8x8) —
				// above est alloué aligné superbloc (jamais de débordement) ; la
				// récursion aligne miRow sur num8x8 donc (miRow&7)+num8x8 <= 8.
				if (bsize >= kBlock8x8 && (bsize == kBlock8x8 || partition != 3)) {
					for (int32 i = 0; i < num8x8; ++i) {
						st.aboveSegCtx[miCol + i] = kVp9PartitionCtxAbove[subsize];
						st.leftSegCtx[(miRow & 7) + i] = kVp9PartitionCtxLeft[subsize];
					}
				}
			}

		} // namespace

		bool NkVp9Decoder::ParseSuperframe(const uint8 *data, usize size, NkVp9Superframe &out) {
			out.count = 0;
			if (data == nullptr || size == 0)
				return false;

			const uint8 marker = data[size - 1];
			if ((marker & 0xE0) == 0xC0) {
				const int32 frames = (int32)(marker & 0x7) + 1;
				const int32 mag = (int32)((marker >> 3) & 0x3) + 1;
				const usize indexSz = (usize)2 + (usize)mag * (usize)frames;
				// L'index est encadré par le MÊME octet marqueur au début et à la fin.
				if (size >= indexSz && data[size - indexSz] == marker) {
					const uint8 *x = data + size - indexSz + 1;
					usize off = 0;
					for (int32 i = 0; i < frames; ++i) {
						usize sz = 0;
						for (int32 b = 0; b < mag; ++b)
							sz |= (usize)x[i * mag + b] << (8 * b); // little-endian
						if (off + sz > size - indexSz)
							return false; // index incohérent
						out.offsets[i] = off;
						out.sizes[i] = sz;
						off += sz;
					}
					out.count = frames;
					return true;
				}
			}
			// Pas d'index : la charge = une seule trame.
			out.count = 1;
			out.offsets[0] = 0;
			out.sizes[0] = size;
			return true;
		}

		bool NkVp9Decoder::ParseUncompressedHeader(const uint8 *data, usize size, NkVp9FrameHeader &out,
												   int32 refW, int32 refH) {
			out = NkVp9FrameHeader{};
			BitReader rb;
			rb.data = data;
			rb.size = size;

			if (rb.Literal(2) != 2)
				return false; // frame_marker

			// Profil : 2 bits (low puis high), + 1 bit réservé si profil 3.
			int32 profile = rb.Bit();
			profile |= rb.Bit() << 1;
			if (profile > 2)
				profile += rb.Bit();
			if (profile > 3)
				return false;
			out.profile = profile;

			out.showExistingFrame = rb.Bit() != 0;
			if (out.showExistingFrame) {
				out.frameToShowMapIdx = rb.Literal(3);
				out.showFrame = true;
				out.uncompressedBytes = (int32)((rb.bitPos + 7) >> 3);
				return !rb.error;
			}

			out.frameType = rb.Bit();
			out.showFrame = rb.Bit() != 0;
			out.errorResilient = rb.Bit() != 0;

			if (out.frameType == kVp9KeyFrame) {
				if (!ReadSyncCode(rb))
					return false;
				if (!ReadColorConfig(rb, out))
					return false;
				out.refreshFrameFlags = 0xFF;
				ReadFrameSize(rb, out.width, out.height);
				ReadRenderSize(rb, out);
			} else {
				out.intraOnly = out.showFrame ? false : (rb.Bit() != 0);
				out.resetFrameContext = out.errorResilient ? 0 : rb.Literal(2);

				if (out.intraOnly) {
					if (!ReadSyncCode(rb))
						return false;
					if (out.profile > 0) {
						if (!ReadColorConfig(rb, out))
							return false;
					} else {
						// Profil 0 intra-only : 4:2:0 8-bit BT601 normatif.
						out.colorSpace = 1;
						out.subsamplingX = 1;
						out.subsamplingY = 1;
						out.bitDepth = 8;
					}
					out.refreshFrameFlags = (uint32)rb.Literal(8);
					ReadFrameSize(rb, out.width, out.height);
					ReadRenderSize(rb, out);
				} else {
					out.refreshFrameFlags = (uint32)rb.Literal(8);
					for (int32 i = 0; i < 3; ++i) {
						out.refFrameIdx[i] = rb.Literal(3);
						out.refFrameSignBias[i] = rb.Bit() != 0;
					}
					// frame_size_with_refs : flag par référence (taille héritée), sinon
					// taille explicite. La taille héritée = celle du slot référencé —
					// fournie par l'appelant (`refW/refH`) ; sans elle, sentinelle
					// négative (tile_info/headerSize non fiables).
					bool found = false;
					for (int32 i = 0; i < 3; ++i) {
						if (rb.Bit()) {
							found = true;
							if (refW > 0 && refH > 0) {
								out.width = refW;
								out.height = refH;
							} else {
								out.width = -(out.refFrameIdx[i] + 1); // sentinelle : réf i
								out.height = out.width;
							}
							break;
						}
					}
					if (!found)
						ReadFrameSize(rb, out.width, out.height);
					ReadRenderSize(rb, out);
					out.allowHighPrecisionMv = rb.Bit() != 0;
					// read_interp_filter : 1 bit switchable, sinon 2 bits
					// {smooth, eighttap, sharp, bilinear}.
					if (rb.Bit()) {
						out.interpFilter = kVp9Switchable;
					} else {
						static const int32 kMap[4] = {kVp9EighttapSmooth, kVp9Eighttap,
													  kVp9EighttapSharp, kVp9Bilinear};
						out.interpFilter = kMap[rb.Literal(2)];
					}
				}
			}

			if (!out.errorResilient) {
				out.refreshFrameContext = rb.Bit() != 0;
				out.frameParallelDecoding = rb.Bit() != 0;
			} else {
				out.refreshFrameContext = false;
				out.frameParallelDecoding = true;
			}
			out.frameContextIdx = rb.Literal(2);

			ReadLoopFilter(rb, out);
			ReadQuantization(rb, out);
			ReadSegmentation(rb, out);
			// tile_info dépend de la largeur : indisponible pour une taille héritée
			// d'une référence (sentinelle) — le harnais s'arrête proprement avant.
			if (out.width > 0)
				ReadTileInfo(rb, out);

			out.headerSizeBytes = rb.Literal(16);
			out.uncompressedBytes = (int32)((rb.bitPos + 7) >> 3);
			return !rb.error;
		}

		void NkVp9Decoder::InitDefaultFrameContext(NkVp9FrameContext &fc) {
			const uint8 *p;
			p = &kVp9DefaultIfYProbs[0][0];
			for (int32 i = 0; i < 4 * 9; ++i)
				(&fc.yModeProb[0][0])[i] = p[i];
			p = &kVp9DefaultIfUvProbs[0][0];
			for (int32 i = 0; i < 10 * 9; ++i)
				(&fc.uvModeProb[0][0])[i] = p[i];
			p = &kVp9DefaultPartitionProbs[0][0];
			for (int32 i = 0; i < 16 * 3; ++i)
				(&fc.partitionProb[0][0])[i] = p[i];
			p = &kVp9DefaultCoefProbs[0][0][0][0][0][0];
			for (int32 i = 0; i < 4 * 2 * 2 * 6 * 6 * 3; ++i)
				(&fc.coefProbs[0][0][0][0][0][0])[i] = p[i];
			p = &kVp9DefaultSwitchableInterpProb[0][0];
			for (int32 i = 0; i < 4 * 2; ++i)
				(&fc.switchableInterpProb[0][0])[i] = p[i];
			p = &kVp9DefaultInterModeProbs[0][0];
			for (int32 i = 0; i < 7 * 3; ++i)
				(&fc.interModeProbs[0][0])[i] = p[i];
			for (int32 i = 0; i < 4; ++i)
				fc.intraInterProb[i] = kVp9DefaultIntraInterP[i];
			for (int32 i = 0; i < 5; ++i)
				fc.compInterProb[i] = kVp9DefaultCompInterP[i];
			p = &kVp9DefaultSingleRefP[0][0];
			for (int32 i = 0; i < 5 * 2; ++i)
				(&fc.singleRefProb[0][0])[i] = p[i];
			for (int32 i = 0; i < 5; ++i)
				fc.compRefProb[i] = kVp9DefaultCompRefP[i];
			p = &kVp9DefaultTxProbs32[0][0];
			for (int32 i = 0; i < 2 * 3; ++i)
				(&fc.txProbs32[0][0])[i] = p[i];
			p = &kVp9DefaultTxProbs16[0][0];
			for (int32 i = 0; i < 2 * 2; ++i)
				(&fc.txProbs16[0][0])[i] = p[i];
			p = &kVp9DefaultTxProbs8[0][0];
			for (int32 i = 0; i < 2 * 1; ++i)
				(&fc.txProbs8[0][0])[i] = p[i];
			for (int32 i = 0; i < 3; ++i)
				fc.skipProbs[i] = kVp9DefaultSkipProbs[i];
			for (int32 i = 0; i < 3; ++i)
				fc.nmvJoints[i] = kVp9DefaultNmvJoints[i];
			for (int32 c = 0; c < 2; ++c) {
				NkVp9NmvComponent &co = fc.nmvComps[c];
				co.sign = kVp9DefaultNmvSign[c];
				for (int32 i = 0; i < 10; ++i)
					co.classes[i] = kVp9DefaultNmvClasses[c][i];
				co.class0[0] = kVp9DefaultNmvClass0[c][0];
				for (int32 i = 0; i < 10; ++i)
					co.bits[i] = kVp9DefaultNmvBits[c][i];
				for (int32 i = 0; i < 2; ++i)
					for (int32 j = 0; j < 3; ++j)
						co.class0Fr[i][j] = kVp9DefaultNmvClass0Fr[c][i][j];
				for (int32 i = 0; i < 3; ++i)
					co.fr[i] = kVp9DefaultNmvFr[c][i];
				co.class0Hp = kVp9DefaultNmvClass0Hp[c];
				co.hp = kVp9DefaultNmvHp[c];
			}
		}

		bool NkVp9Decoder::ParseCompressedHeader(const uint8 *data, usize size, const NkVp9FrameHeader &hdr,
												 NkVp9FrameContext &fc, NkVp9CompressedHeader &out) {
			if (data == nullptr || size == 0)
				return false;
			NkVp8BoolDecoder bd(data, size);
			// ⚠ VP9 (vpx_reader_init) : UN BIT MARQUEUR est lu à l'init et doit valoir 0
			// — c'est LA différence d'amorçage avec le bool decoder VP8 (aucun marqueur).
			if (bd.GetFlag() != 0)
				return false;

			// tx_mode (§6.3.1) : 2 bits, +1 bit si ALLOW_32X32 (lossless → ONLY_4X4).
			int32 txMode = 0;
			if (!hdr.lossless) {
				txMode = (int32)bd.GetLiteral(2);
				if (txMode == 3)
					txMode += bd.GetFlag();
			}
			out.txMode = txMode;

			// tx probs (TX_MODE_SELECT) : p8x8 PUIS p16x16 PUIS p32x32 (read_tx_mode_probs).
			if (txMode == 4) {
				for (int32 i = 0; i < 2; ++i)
					for (int32 j = 0; j < 1; ++j)
						DiffUpdateProb(bd, fc.txProbs8[i][j]);
				for (int32 i = 0; i < 2; ++i)
					for (int32 j = 0; j < 2; ++j)
						DiffUpdateProb(bd, fc.txProbs16[i][j]);
				for (int32 i = 0; i < 2; ++i)
					for (int32 j = 0; j < 3; ++j)
						DiffUpdateProb(bd, fc.txProbs32[i][j]);
			}

			// coef probs (§6.3.2) : pour chaque taille de transformée ≤ max(txMode), un
			// flag de mise à jour puis les deltas ([plane][ref][bande][ctx][3 nœuds] —
			// la bande 0 n'a que 3 contextes).
			static const int32 kTxModeToBiggestTxSize[5] = {0, 1, 2, 3, 3};
			const int32 maxTxSize = kTxModeToBiggestTxSize[txMode];
			for (int32 ts = 0; ts <= maxTxSize; ++ts) {
				if (!bd.GetFlag())
					continue;
				for (int32 i = 0; i < 2; ++i)
					for (int32 j = 0; j < 2; ++j)
						for (int32 k = 0; k < 6; ++k) {
							const int32 nCtx = (k == 0) ? 3 : 6;
							for (int32 l = 0; l < nCtx; ++l)
								for (int32 m = 0; m < 3; ++m)
									DiffUpdateProb(bd, fc.coefProbs[ts][i][j][k][l][m]);
						}
			}

			// skip probs.
			for (int32 k = 0; k < 3; ++k)
				DiffUpdateProb(bd, fc.skipProbs[k]);

			out.referenceMode = kVp9SingleReference;
			const bool intraOnlyFrame = (hdr.frameType == kVp9KeyFrame) || hdr.intraOnly;
			if (!intraOnlyFrame) {
				// inter modes.
				for (int32 i = 0; i < 7; ++i)
					for (int32 j = 0; j < 3; ++j)
						DiffUpdateProb(bd, fc.interModeProbs[i][j]);
				// filtre switchable.
				if (hdr.interpFilter == kVp9Switchable)
					for (int32 i = 0; i < 4; ++i)
						for (int32 j = 0; j < 2; ++j)
							DiffUpdateProb(bd, fc.switchableInterpProb[i][j]);
				// intra/inter.
				for (int32 i = 0; i < 4; ++i)
					DiffUpdateProb(bd, fc.intraInterProb[i]);
				// reference mode : compound possible ssi les sign bias divergent.
				const bool compAllowed = (hdr.refFrameSignBias[1] != hdr.refFrameSignBias[0]) ||
										 (hdr.refFrameSignBias[2] != hdr.refFrameSignBias[0]);
				int32 refMode = kVp9SingleReference;
				if (compAllowed) {
					if (bd.GetFlag())
						refMode = bd.GetFlag() ? kVp9ReferenceModeSelect : kVp9CompoundReference;
				}
				out.referenceMode = refMode;
				if (refMode == kVp9ReferenceModeSelect)
					for (int32 i = 0; i < 5; ++i)
						DiffUpdateProb(bd, fc.compInterProb[i]);
				if (refMode != kVp9CompoundReference)
					for (int32 i = 0; i < 5; ++i) {
						DiffUpdateProb(bd, fc.singleRefProb[i][0]);
						DiffUpdateProb(bd, fc.singleRefProb[i][1]);
					}
				if (refMode != kVp9SingleReference)
					for (int32 i = 0; i < 5; ++i)
						DiffUpdateProb(bd, fc.compRefProb[i]);
				// modes Y.
				for (int32 j = 0; j < 4; ++j)
					for (int32 i = 0; i < 9; ++i)
						DiffUpdateProb(bd, fc.yModeProb[j][i]);
				// partitions.
				for (int32 j = 0; j < 16; ++j)
					for (int32 i = 0; i < 3; ++i)
						DiffUpdateProb(bd, fc.partitionProb[j][i]);
				// vecteurs de mouvement (read_mv_probs) : joints, puis par composante
				// sign/classes/class0/bits, puis par composante class0_fr/fr, puis hp.
				MvUpdateProbs(bd, fc.nmvJoints, 3);
				for (int32 c = 0; c < 2; ++c) {
					NkVp9NmvComponent &co = fc.nmvComps[c];
					MvUpdateProbs(bd, &co.sign, 1);
					MvUpdateProbs(bd, co.classes, 10);
					MvUpdateProbs(bd, co.class0, 1);
					MvUpdateProbs(bd, co.bits, 10);
				}
				for (int32 c = 0; c < 2; ++c) {
					NkVp9NmvComponent &co = fc.nmvComps[c];
					for (int32 j = 0; j < 2; ++j)
						MvUpdateProbs(bd, co.class0Fr[j], 3);
					MvUpdateProbs(bd, co.fr, 3);
				}
				if (hdr.allowHighPrecisionMv) {
					for (int32 c = 0; c < 2; ++c) {
						MvUpdateProbs(bd, &fc.nmvComps[c].class0Hp, 1);
						MvUpdateProbs(bd, &fc.nmvComps[c].hp, 1);
					}
				}
			}

			// Le bool decoder précharge 2 octets : un overread ≤ 2 est structurel,
			// davantage = désalignement du parse.
			return bd.overreadBytes <= 2;
		}

		namespace {
			// =============================================================
			// ADAPTATION BACKWARD DES PROBABILITÉS (§8.4) — vp9_adapt_coef_probs /
			// vp9_adapt_mode_probs / vp9_adapt_mv_probs + vp9_setup_past_independence.
			// Sans ceci, chaque trame INTER repart des probabilités PAR DÉFAUT au lieu
			// d'hériter de celles adaptées sur la trame précédente (persistées dans
			// NkVp9EntropyState::frameContexts[frame_context_idx]) — bug diagnostiqué
			// par trace croisée avec vpxdec instrumenté (désync bitstream dès la 1re
			// trame inter, sur un token de coefficient dont la proba n'était PAS
			// re-mise à jour par l'en-tête de CETTE trame).
			// =============================================================

			// get_prob/get_binary_prob/weighted_prob/merge_probs (vpx_dsp/prob.h).
			uint8 GetProb(uint32 num, uint32 den) {
				const int32 p = (int32)(((uint64)num * 256 + (den >> 1)) / den);
				const int32 clipped = p | ((255 - p) >> 23) | (p == 0);
				return (uint8)clipped;
			}
			uint8 GetBinaryProb(uint32 n0, uint32 n1) {
				const uint32 den = n0 + n1;
				if (den == 0)
					return 128;
				return GetProb(n0, den);
			}
			uint8 WeightedProb(int32 p1, int32 p2, int32 factor) {
				return (uint8)((p1 * (256 - factor) + p2 * factor + 128) >> 8);
			}
			uint8 MergeProbs(uint8 preProb, const uint32 ct[2], uint32 countSat, uint32 maxUpdateFactor) {
				const uint8 prob = GetBinaryProb(ct[0], ct[1]);
				const uint32 count = IMin(ct[0] + ct[1], countSat);
				const uint32 factor = maxUpdateFactor * count / countSat;
				return WeightedProb(preProb, prob, (int32)factor);
			}
			// mode_mv_merge_probs + count_to_update_factor (MODE_MV_COUNT_SAT=20).
			const int32 kCountToUpdateFactor[21] = {0,  6,  12, 19, 25, 32,  38,  44,  51,  57, 64,
													 70, 76, 83, 89, 96, 102, 108, 115, 121, 128};
			uint8 ModeMvMergeProbs(uint8 preProb, const uint32 ct[2]) {
				const uint32 den = ct[0] + ct[1];
				if (den == 0)
					return preProb;
				const uint32 count = IMin(den, 20u);
				const int32 factor = kCountToUpdateFactor[count];
				const uint8 prob = GetProb(ct[0], den);
				return WeightedProb(preProb, prob, factor);
			}
			// vpx_tree_merge_probs (générique, récursif sur un arbre vpx_tree).
			uint32 TreeMergeProbsImpl(int32 i, const nk_int8 *tree, const uint8 *preProbs, const uint32 *counts,
									  uint8 *probs) {
				const int32 l = tree[i];
				const uint32 leftCount =
					(l <= 0) ? counts[-l] : TreeMergeProbsImpl(l, tree, preProbs, counts, probs);
				const int32 r = tree[i + 1];
				const uint32 rightCount =
					(r <= 0) ? counts[-r] : TreeMergeProbsImpl(r, tree, preProbs, counts, probs);
				const uint32 ct[2] = {leftCount, rightCount};
				probs[i >> 1] = ModeMvMergeProbs(preProbs[i >> 1], ct);
				return leftCount + rightCount;
			}
			void TreeMergeProbs(const nk_int8 *tree, const uint8 *preProbs, const uint32 *counts, uint8 *probs) {
				TreeMergeProbsImpl(0, tree, preProbs, counts, probs);
			}

			// tx_counts_to_branch_counts_{8x8,16x16,32x32} : les comptes bruts par
			// taille sont convertis en comptes de branche binaires de la cascade de
			// bits séquentielle (read_selected_tx_size), PAS un vpx_tree générique.
			void TxCountsToBranchCounts8x8(const uint32 counts[2], uint32 branchCt[1][2]) {
				branchCt[0][0] = counts[0];
				branchCt[0][1] = counts[1];
			}
			void TxCountsToBranchCounts16x16(const uint32 counts[3], uint32 branchCt[2][2]) {
				branchCt[0][0] = counts[0];
				branchCt[0][1] = counts[1] + counts[2];
				branchCt[1][0] = counts[1];
				branchCt[1][1] = counts[2];
			}
			void TxCountsToBranchCounts32x32(const uint32 counts[4], uint32 branchCt[3][2]) {
				branchCt[0][0] = counts[0];
				branchCt[0][1] = counts[1] + counts[2] + counts[3];
				branchCt[1][0] = counts[1];
				branchCt[1][1] = counts[2] + counts[3];
				branchCt[2][0] = counts[2];
				branchCt[2][1] = counts[3];
			}

			// adapt_coef_probs (vp9_entropy.c) : une taille de transformée.
			void MergeCoefProbsForTx(NkVp9FrameContext &fc, const NkVp9FrameContext &preFc,
									 const NkVp9FrameCounts &counts, int32 txSize, uint32 countSat,
									 uint32 updateFactor) {
				for (int32 i = 0; i < 2; ++i)	  // plane type
					for (int32 j = 0; j < 2; ++j) // ref (0=intra,1=inter)
						for (int32 k = 0; k < 6; ++k) { // bande
							const int32 nCtx = (k == 0) ? 3 : 6;
							for (int32 l = 0; l < nCtx; ++l) {
								const uint32 n0 = counts.coef[txSize][i][j][k][l][0];
								const uint32 n1 = counts.coef[txSize][i][j][k][l][1];
								const uint32 n2 = counts.coef[txSize][i][j][k][l][2];
								const uint32 neob = counts.coef[txSize][i][j][k][l][3];
								const uint32 eobCount = counts.eobBranch[txSize][i][j][k][l];
								const uint32 branchCt[3][2] = {
									{neob, eobCount - neob}, {n0, n1 + n2}, {n1, n2}};
								for (int32 m = 0; m < 3; ++m)
									fc.coefProbs[txSize][i][j][k][l][m] = MergeProbs(
										preFc.coefProbs[txSize][i][j][k][l][m], branchCt[m], countSat, updateFactor);
							}
						}
			}
			// vp9_adapt_coef_probs : s'applique MÊME sur une trame intra-only (contrairement
			// à mode/mv). `intraOnly` = trame COURANTE ; `lastFrameWasKey` = trame PRÉCÉDENTE.
			void AdaptCoefProbs(NkVp9FrameContext &fc, const NkVp9FrameContext &preFc,
							   const NkVp9FrameCounts &counts, bool intraOnly, bool lastFrameWasKey) {
				const uint32 countSat = 24;
				const uint32 updateFactor = (!intraOnly && lastFrameWasKey) ? 128 : 112;
				for (int32 t = 0; t < 4; ++t)
					MergeCoefProbsForTx(fc, preFc, counts, t, countSat, updateFactor);
			}

			// vp9_adapt_mode_probs : SEULEMENT si !frame_is_intra_only(cm) (appelant).
			void AdaptModeProbs(NkVp9FrameContext &fc, const NkVp9FrameContext &preFc,
							   const NkVp9FrameCounts &counts, int32 txMode, int32 interpFilterHdr) {
				for (int32 i = 0; i < 4; ++i)
					fc.intraInterProb[i] = ModeMvMergeProbs(preFc.intraInterProb[i], counts.intraInter[i]);
				for (int32 i = 0; i < 5; ++i)
					fc.compInterProb[i] = ModeMvMergeProbs(preFc.compInterProb[i], counts.compInter[i]);
				for (int32 i = 0; i < 5; ++i)
					fc.compRefProb[i] = ModeMvMergeProbs(preFc.compRefProb[i], counts.compRef[i]);
				for (int32 i = 0; i < 5; ++i)
					for (int32 j = 0; j < 2; ++j)
						fc.singleRefProb[i][j] = ModeMvMergeProbs(preFc.singleRefProb[i][j], counts.singleRef[i][j]);

				for (int32 i = 0; i < 7; ++i)
					TreeMergeProbs(kVp9InterModeTree, preFc.interModeProbs[i], counts.interMode[i],
								   fc.interModeProbs[i]);
				for (int32 i = 0; i < 4; ++i)
					TreeMergeProbs(kVp9IntraModeTree, preFc.yModeProb[i], counts.yMode[i], fc.yModeProb[i]);
				for (int32 i = 0; i < 10; ++i)
					TreeMergeProbs(kVp9IntraModeTree, preFc.uvModeProb[i], counts.uvMode[i], fc.uvModeProb[i]);
				for (int32 i = 0; i < 16; ++i)
					TreeMergeProbs(kVp9PartitionTree, preFc.partitionProb[i], counts.partition[i],
								   fc.partitionProb[i]);

				if (interpFilterHdr == 4) { // SWITCHABLE
					for (int32 i = 0; i < 4; ++i)
						TreeMergeProbs(kVp9SwitchableInterpTree, preFc.switchableInterpProb[i],
									   counts.switchableInterp[i], fc.switchableInterpProb[i]);
				}

				if (txMode == 4) { // TX_MODE_SELECT
					for (int32 i = 0; i < 2; ++i) {
						uint32 b8[1][2];
						TxCountsToBranchCounts8x8(counts.txP8x8[i], b8);
						fc.txProbs8[i][0] = ModeMvMergeProbs(preFc.txProbs8[i][0], b8[0]);

						uint32 b16[2][2];
						TxCountsToBranchCounts16x16(counts.txP16x16[i], b16);
						for (int32 j = 0; j < 2; ++j)
							fc.txProbs16[i][j] = ModeMvMergeProbs(preFc.txProbs16[i][j], b16[j]);

						uint32 b32[3][2];
						TxCountsToBranchCounts32x32(counts.txP32x32[i], b32);
						for (int32 j = 0; j < 3; ++j)
							fc.txProbs32[i][j] = ModeMvMergeProbs(preFc.txProbs32[i][j], b32[j]);
					}
				}
				for (int32 i = 0; i < 3; ++i)
					fc.skipProbs[i] = ModeMvMergeProbs(preFc.skipProbs[i], counts.skip[i]);
			}

			// vp9_adapt_mv_probs : SEULEMENT si !frame_is_intra_only(cm) (appelant).
			void AdaptMvProbs(NkVp9FrameContext &fc, const NkVp9FrameContext &preFc,
							 const NkVp9FrameCounts &counts, bool allowHp) {
				TreeMergeProbs(kVp9MvJointTree, preFc.nmvJoints, counts.mvJoints, fc.nmvJoints);
				for (int32 i = 0; i < 2; ++i) {
					NkVp9NmvComponent &comp = fc.nmvComps[i];
					const NkVp9NmvComponent &preComp = preFc.nmvComps[i];
					const NkVp9NmvComponentCounts &c = counts.mvComps[i];

					const uint32 signCt[2] = {c.sign[0], c.sign[1]};
					comp.sign = ModeMvMergeProbs(preComp.sign, signCt);
					TreeMergeProbs(kVp9MvClassTree, preComp.classes, c.classes, comp.classes);
					TreeMergeProbs(kVp9MvClass0Tree, preComp.class0, c.class0, comp.class0);
					for (int32 j = 0; j < 10; ++j) {
						const uint32 ct[2] = {c.bits[j][0], c.bits[j][1]};
						comp.bits[j] = ModeMvMergeProbs(preComp.bits[j], ct);
					}
					for (int32 j = 0; j < 2; ++j)
						TreeMergeProbs(kVp9MvFrTree, preComp.class0Fr[j], c.class0Fr[j], comp.class0Fr[j]);
					TreeMergeProbs(kVp9MvFrTree, preComp.fr, c.fr, comp.fr);
					if (allowHp) {
						const uint32 hp0Ct[2] = {c.class0Hp[0], c.class0Hp[1]};
						comp.class0Hp = ModeMvMergeProbs(preComp.class0Hp, hp0Ct);
						const uint32 hpCt[2] = {c.hp[0], c.hp[1]};
						comp.hp = ModeMvMergeProbs(preComp.hp, hpCt);
					}
				}
			}

			// vp9_setup_past_independence (partie contexte d'entropie seulement — les
			// deltas de loop filter/segmentation sont déjà relus intégralement depuis
			// l'en-tête à chaque trame, cf. ReadLoopFilter/ReadSegmentation). Appelé
			// SEULEMENT si intra-only || error-resilient (sinon reset_frame_context
			// n'est même pas censé être pris en compte).
			void SetupPastIndependence(NkVp9EntropyState &entropy, const NkVp9FrameHeader &hdr) {
				NkVp9FrameContext def;
				NkVp9Decoder::InitDefaultFrameContext(def);
				const bool intraOnly = (hdr.frameType == kVp9KeyFrame) || hdr.intraOnly;
				if (intraOnly || hdr.errorResilient || hdr.resetFrameContext == 3) {
					for (int32 i = 0; i < 4; ++i)
						entropy.frameContexts[i] = def;
				} else if (hdr.resetFrameContext == 2) {
					entropy.frameContexts[hdr.frameContextIdx] = def;
				}
				// La carte de segmentation (last_frame_seg_map/current_frame_seg_map côté
				// libvpx) est remise à zéro INCONDITIONNELLEMENT dès que cette fonction
				// s'exécute (intra-only || error-resilient) — PAS soumise à resetFrameContext,
				// contrairement aux frame_contexts ci-dessus. Un vecteur vide = « indisponible »
				// pour DecGetSegmentId/CopySegmentId (fallback 0), équivalent au memset(0).
				if (intraOnly || hdr.errorResilient) {
					entropy.lastFrameSegMap.Resize(0);
					entropy.segAbsDelta = false;
					for (int32 i = 0; i < 8; ++i)
						for (int32 j = 0; j < 4; ++j) {
							entropy.segFeatureEnabled[i][j] = false;
							entropy.segFeatureData[i][j] = 0;
						}
				}
			}

			// setup_segmentation ne réécrit segAbsDelta/segFeatureEnabled/segFeatureData QUE
			// si segUpdateData est vrai — sinon ces champs restent aux valeurs par défaut du
			// struct fraîchement construit (0/faux), alors qu'ils doivent PERSISTER depuis la
			// dernière trame qui les a signalés (comme lfRefDeltas mais pour les features de
			// segment). À appeler après ParseUncompressedHeader + SetupPastIndependence, avant
			// tout usage de hdr.segFeature*.
			void SyncSegmentFeatures(NkVp9FrameHeader &hdr, NkVp9EntropyState &entropy) {
				if (!hdr.segEnabled)
					return;
				if (hdr.segUpdateData) {
					entropy.segAbsDelta = hdr.segAbsDelta;
					for (int32 i = 0; i < 8; ++i)
						for (int32 j = 0; j < 4; ++j) {
							entropy.segFeatureEnabled[i][j] = hdr.segFeatureEnabled[i][j];
							entropy.segFeatureData[i][j] = hdr.segFeatureData[i][j];
						}
				} else {
					hdr.segAbsDelta = entropy.segAbsDelta;
					for (int32 i = 0; i < 8; ++i)
						for (int32 j = 0; j < 4; ++j) {
							hdr.segFeatureEnabled[i][j] = entropy.segFeatureEnabled[i][j];
							hdr.segFeatureData[i][j] = entropy.segFeatureData[i][j];
						}
				}
			}
		} // namespace

		namespace {
			// Corps commun briques 3-4 : parse (img == nullptr) ou reconstruction.
			bool ParseOrDecodeKeyContent(const uint8 *tileData, usize tileSize,
										 const NkVp9FrameHeader &hdr, const NkVp9FrameContext &fc,
										 const NkVp9CompressedHeader &chdr,
										 NkVp9Decoder::NkTileParseStats &stats, NkVp9Image *img,
										 bool applyLoopFilter = true, NkVp9FrameCounts *counts = nullptr,
										 const uint8 *lastSegMap = nullptr,
										 NkVector<uint8> *outSegMap = nullptr) {
			stats = NkVp9Decoder::NkTileParseStats{};
			if (tileData == nullptr || tileSize == 0 || hdr.width <= 0 || hdr.height <= 0)
				return false;
			if (hdr.frameType != kVp9KeyFrame && !hdr.intraOnly)
				return false; // intra seulement (l'inter = briques suivantes)

			FrameParseState st;
			st.hdr = &hdr;
			st.fc = &fc;
			st.miCols = (hdr.width + 7) >> 3;
			st.miRows = (hdr.height + 7) >> 3;
			st.sbCols = (st.miCols + 7) >> 3;
			st.txMode = chdr.txMode;

			if (img != nullptr) {
				// Buffers de sortie I420 + tables de déquantification par segment.
				// ⚠ MARGE de 64 px à droite et en bas : les blocs PARTIELS des bords
				// écrivent leur emprise COMPLÈTE (prédiction pleine taille, normatif —
				// libvpx alloue pareil des bordures) ; sans marge, un bloc du bord
				// droit déborde du stride et écrase la bande gauche de la rangée
				// suivante. La marge n'est jamais LUE (les chemins de bord répliquent
				// via frameW/frameH), elle absorbe seulement les écritures.
				img->width = hdr.width;
				img->height = hdr.height;
				img->uvWidth = (hdr.width + hdr.subsamplingX) >> hdr.subsamplingX;
				img->uvHeight = (hdr.height + hdr.subsamplingY) >> hdr.subsamplingY;
				img->yStride = hdr.width + 64;
				img->uvStride = img->uvWidth + 64;
				img->y.Resize((usize)img->yStride * (usize)(img->height + 64));
				img->u.Resize((usize)img->uvStride * (usize)(img->uvHeight + 64));
				img->v.Resize((usize)img->uvStride * (usize)(img->uvHeight + 64));
				st.planes[0] = img->y.Data();
				st.planes[1] = img->u.Data();
				st.planes[2] = img->v.Data();
				st.planeStride[0] = img->yStride;
				st.planeStride[1] = st.planeStride[2] = img->uvStride;
				st.planeW[0] = img->width;
				st.planeH[0] = img->height;
				st.planeW[1] = st.planeW[2] = img->uvWidth;
				st.planeH[1] = st.planeH[2] = img->uvHeight;
				// get_qindex + vp9_dc/ac_quant par segment (ALT_Q = feature 0).
				for (int32 s = 0; s < 8; ++s) {
					int32 q = hdr.baseQIdx;
					if (hdr.segEnabled && hdr.segFeatureEnabled[s][0]) {
						const int32 data = hdr.segFeatureData[s][0];
						q = hdr.segAbsDelta ? data : hdr.baseQIdx + data;
						q = q < 0 ? 0 : (q > 255 ? 255 : q);
					}
					auto ClampQ = [](int32 v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
					st.yDequant[s][0] = kVp9DcQLookup[ClampQ(q + hdr.deltaQYDc)];
					st.yDequant[s][1] = kVp9AcQLookup[q];
					st.uvDequant[s][0] = kVp9DcQLookup[ClampQ(q + hdr.deltaQUvDc)];
					st.uvDequant[s][1] = kVp9AcQLookup[ClampQ(q + hdr.deltaQUvAc)];
				}
			}

			const int32 miAligned = st.sbCols * kMiBlockSize;
			st.mi = (MiCell *)memory::NkAlloc((size_t)st.miRows * (size_t)st.miCols * sizeof(MiCell));
			st.aboveSegCtx = (uint8 *)memory::NkAlloc((size_t)miAligned);
			st.currentSegMap = (uint8 *)memory::NkAlloc((size_t)st.miRows * (size_t)st.miCols);
			for (int32 p = 0; p < 3; ++p)
				st.aboveEntCtx[p] = (uint8 *)memory::NkAlloc((size_t)miAligned * 2);
			if (!st.mi || !st.aboveSegCtx || !st.currentSegMap || !st.aboveEntCtx[0] ||
				!st.aboveEntCtx[1] || !st.aboveEntCtx[2]) {
				memory::NkFree(st.mi);
				memory::NkFree(st.aboveSegCtx);
				memory::NkFree(st.currentSegMap);
				for (int32 p = 0; p < 3; ++p)
					memory::NkFree(st.aboveEntCtx[p]);
				return false;
			}
			for (int32 i = 0; i < st.miRows * st.miCols; ++i) {
				st.mi[i] = MiCell{};
				st.currentSegMap[i] = 0;
			}
			for (int32 i = 0; i < miAligned; ++i)
				st.aboveSegCtx[i] = 0;
			for (int32 p = 0; p < 3; ++p)
				for (int32 i = 0; i < miAligned * 2; ++i)
					st.aboveEntCtx[p][i] = 0;
			st.stats = &stats;
			st.counts = counts;
			st.lastSegMap = lastSegMap;

			const int32 tileCols = 1 << hdr.tileColsLog2;
			const int32 tileRows = 1 << hdr.tileRowsLog2;
			const int32 sbRows = (st.miRows + 7) >> 3;

			// get_tile_offset : min(((idx * sbDim) >> log2) << 3, miDim).
			auto TileOffset = [](int32 idx, int32 sbDim, int32 log2, int32 miDim) -> int32 {
				const int32 off = ((idx * sbDim) >> log2) << 3;
				return off < miDim ? off : miDim;
			};

			bool ok = true;
			const uint8 *p = tileData;
			usize remaining = tileSize;
			for (int32 tr = 0; tr < tileRows && ok; ++tr) {
				const int32 miRowStart = TileOffset(tr, sbRows, hdr.tileRowsLog2, st.miRows);
				const int32 miRowEnd = TileOffset(tr + 1, sbRows, hdr.tileRowsLog2, st.miRows);
				for (int32 tc = 0; tc < tileCols && ok; ++tc) {
					const bool last = (tr == tileRows - 1) && (tc == tileCols - 1);
					usize sz = remaining;
					if (!last) {
						if (remaining < 4) {
							ok = false;
							break;
						}
						sz = ((usize)p[0] << 24) | ((usize)p[1] << 16) | ((usize)p[2] << 8) | (usize)p[3];
						p += 4;
						remaining -= 4;
						if (sz > remaining) {
							ok = false;
							break;
						}
					}
					st.tileMiColStart = TileOffset(tc, st.sbCols, hdr.tileColsLog2, st.miCols);
					st.tileMiColEnd = TileOffset(tc + 1, st.sbCols, hdr.tileColsLog2, st.miCols);

					NkVp8BoolDecoder bd(p, sz);
					if (bd.GetFlag() != 0) { // bit marqueur (vpx_reader_init)
						ok = false;
						break;
					}
					for (int32 miRow = miRowStart; miRow < miRowEnd && ok; miRow += kMiBlockSize) {
						// Nouvelle rangée de superblocs : contextes gauche à zéro.
						for (int32 q = 0; q < 3; ++q)
							for (int32 i = 0; i < 16; ++i)
								st.leftEntCtx[q][i] = 0;
						for (int32 i = 0; i < 8; ++i)
							st.leftSegCtx[i] = 0;
						for (int32 miCol = st.tileMiColStart; miCol < st.tileMiColEnd;
							 miCol += kMiBlockSize)
							DecodePartition(bd, st, miRow, miCol, kBlock64x64, 4);
					}
					// Consommation EXACTE : pré-chargement de 2 octets toléré.
					if (bd.overreadBytes > 2 || bd.pos < sz) {
						ok = false;
					}
					if (bd.overreadBytes > stats.maxOverread)
						stats.maxOverread = bd.overreadBytes;
					++stats.tiles;
					p += sz;
					remaining -= sz;
				}
			}

			// BRIQUE 5 : loop filter sur la trame reconstruite (grille mi encore vivante).
			if (ok && img != nullptr && applyLoopFilter)
				LoopFilterFrame(st);

			if (ok && outSegMap != nullptr) {
				outSegMap->Resize((usize)st.miRows * (usize)st.miCols);
				for (int32 i = 0; i < st.miRows * st.miCols; ++i)
					(*outSegMap)[i] = st.currentSegMap[i];
			}

			memory::NkFree(st.mi);
			memory::NkFree(st.aboveSegCtx);
			memory::NkFree(st.currentSegMap);
			for (int32 q = 0; q < 3; ++q)
				memory::NkFree(st.aboveEntCtx[q]);
			return ok;
			}

			// Regroupe l'état inter préparé par DecodeInterFrame (refs + probas dérivées)
			// avant l'appel au corps commun de parcours des tiles.
			struct InterFrameCtx {
					const uint8 *refPlanes[3][3] = {{nullptr}};
					int32 refStride[3][3] = {{0}};
					int32 signBias[4] = {0, 0, 0, 0};
					int32 compFixedRef = 0, compVarRef[2] = {0, 0};
					int32 referenceMode = 0;
					bool allowHighPrecisionMv = false;
					int32 interpFilterHdr = 4;
					bool usePrevFrameMvs = false;
					const NkVp9MvRef *prevMvs = nullptr;
			};

			// Corps commun brique 6 : parse+reconstruit une trame INTER (miroir de
			// ParseOrDecodeKeyContent). `img==nullptr` : parse seul (validation
			// consommation, pas encore utilisé par un harnais dédié mais réutilisable).
			bool ParseOrDecodeInterContent(const uint8 *tileData, usize tileSize, const NkVp9FrameHeader &hdr,
										   const NkVp9FrameContext &fc, const NkVp9CompressedHeader &chdr,
										   const InterFrameCtx &ictx, NkVp9Decoder::NkTileParseStats &stats,
										   NkVp9Image *img, bool applyLoopFilter = true,
										   NkVp9FrameCounts *counts = nullptr,
										   NkVector<NkVp9MvRef> *outMvGrid = nullptr,
										   const uint8 *lastSegMap = nullptr,
										   NkVector<uint8> *outSegMap = nullptr) {
				stats = NkVp9Decoder::NkTileParseStats{};
				if (tileData == nullptr || tileSize == 0 || hdr.width <= 0 || hdr.height <= 0)
					return false;
				if (hdr.frameType == kVp9KeyFrame || hdr.intraOnly)
					return false; // trames clés/intra-only = ParseOrDecodeKeyContent

				FrameParseState st;
				st.hdr = &hdr;
				st.fc = &fc;
				st.miCols = (hdr.width + 7) >> 3;
				st.miRows = (hdr.height + 7) >> 3;
				st.sbCols = (st.miCols + 7) >> 3;
				st.txMode = chdr.txMode;
				st.isInterFrame = true;
				for (int32 i = 0; i < 3; ++i) {
					for (int32 p = 0; p < 3; ++p) {
						st.refPlanes[i][p] = ictx.refPlanes[i][p];
						st.refStride[i][p] = ictx.refStride[i][p];
					}
					st.signBias[i + 1] = ictx.signBias[i + 1];
				}
				st.compFixedRef = ictx.compFixedRef;
				st.compVarRef[0] = ictx.compVarRef[0];
				st.compVarRef[1] = ictx.compVarRef[1];
				st.referenceMode = ictx.referenceMode;
				st.allowHighPrecisionMv = ictx.allowHighPrecisionMv;
				st.interpFilterHdr = ictx.interpFilterHdr;
				st.usePrevFrameMvs = ictx.usePrevFrameMvs;
				st.prevMvs = ictx.prevMvs;

				if (img != nullptr) {
					img->width = hdr.width;
					img->height = hdr.height;
					img->uvWidth = (hdr.width + hdr.subsamplingX) >> hdr.subsamplingX;
					img->uvHeight = (hdr.height + hdr.subsamplingY) >> hdr.subsamplingY;
					img->yStride = hdr.width + 64;
					img->uvStride = img->uvWidth + 64;
					img->y.Resize((usize)img->yStride * (usize)(img->height + 64));
					img->u.Resize((usize)img->uvStride * (usize)(img->uvHeight + 64));
					img->v.Resize((usize)img->uvStride * (usize)(img->uvHeight + 64));
					st.planes[0] = img->y.Data();
					st.planes[1] = img->u.Data();
					st.planes[2] = img->v.Data();
					st.planeStride[0] = img->yStride;
					st.planeStride[1] = st.planeStride[2] = img->uvStride;
					st.planeW[0] = img->width;
					st.planeH[0] = img->height;
					st.planeW[1] = st.planeW[2] = img->uvWidth;
					st.planeH[1] = st.planeH[2] = img->uvHeight;
					for (int32 s = 0; s < 8; ++s) {
						int32 q = hdr.baseQIdx;
						if (hdr.segEnabled && hdr.segFeatureEnabled[s][0]) {
							const int32 data = hdr.segFeatureData[s][0];
							q = hdr.segAbsDelta ? data : hdr.baseQIdx + data;
							q = q < 0 ? 0 : (q > 255 ? 255 : q);
						}
						auto ClampQ = [](int32 v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
						st.yDequant[s][0] = kVp9DcQLookup[ClampQ(q + hdr.deltaQYDc)];
						st.yDequant[s][1] = kVp9AcQLookup[q];
						st.uvDequant[s][0] = kVp9DcQLookup[ClampQ(q + hdr.deltaQUvDc)];
						st.uvDequant[s][1] = kVp9AcQLookup[ClampQ(q + hdr.deltaQUvAc)];
					}
				}

				const int32 miAligned = st.sbCols * kMiBlockSize;
				st.mi = (MiCell *)memory::NkAlloc((size_t)st.miRows * (size_t)st.miCols * sizeof(MiCell));
				st.aboveSegCtx = (uint8 *)memory::NkAlloc((size_t)miAligned);
				st.currentSegMap = (uint8 *)memory::NkAlloc((size_t)st.miRows * (size_t)st.miCols);
				for (int32 p = 0; p < 3; ++p)
					st.aboveEntCtx[p] = (uint8 *)memory::NkAlloc((size_t)miAligned * 2);
				if (!st.mi || !st.aboveSegCtx || !st.currentSegMap || !st.aboveEntCtx[0] || !st.aboveEntCtx[1] ||
					!st.aboveEntCtx[2]) {
					memory::NkFree(st.mi);
					memory::NkFree(st.aboveSegCtx);
					memory::NkFree(st.currentSegMap);
					for (int32 p = 0; p < 3; ++p)
						memory::NkFree(st.aboveEntCtx[p]);
					return false;
				}
				for (int32 i = 0; i < st.miRows * st.miCols; ++i) {
					st.mi[i] = MiCell{};
					st.currentSegMap[i] = 0;
				}
				for (int32 i = 0; i < miAligned; ++i)
					st.aboveSegCtx[i] = 0;
				for (int32 p = 0; p < 3; ++p)
					for (int32 i = 0; i < miAligned * 2; ++i)
						st.aboveEntCtx[p][i] = 0;
				st.stats = &stats;
				st.counts = counts;
				st.lastSegMap = lastSegMap;

				const int32 tileCols = 1 << hdr.tileColsLog2;
				const int32 tileRows = 1 << hdr.tileRowsLog2;
				const int32 sbRows = (st.miRows + 7) >> 3;
				auto TileOffset = [](int32 idx, int32 sbDim, int32 log2, int32 miDim) -> int32 {
					const int32 off = ((idx * sbDim) >> log2) << 3;
					return off < miDim ? off : miDim;
				};

				bool ok = true;
				const uint8 *p = tileData;
				usize remaining = tileSize;
				for (int32 tr = 0; tr < tileRows && ok; ++tr) {
					const int32 miRowStart = TileOffset(tr, sbRows, hdr.tileRowsLog2, st.miRows);
					const int32 miRowEnd = TileOffset(tr + 1, sbRows, hdr.tileRowsLog2, st.miRows);
					for (int32 tc = 0; tc < tileCols && ok; ++tc) {
						const bool last = (tr == tileRows - 1) && (tc == tileCols - 1);
						usize sz = remaining;
						if (!last) {
							if (remaining < 4) {
								ok = false;
								break;
							}
							sz = ((usize)p[0] << 24) | ((usize)p[1] << 16) | ((usize)p[2] << 8) | (usize)p[3];
							p += 4;
							remaining -= 4;
							if (sz > remaining) {
								ok = false;
								break;
							}
						}
						st.tileMiColStart = TileOffset(tc, st.sbCols, hdr.tileColsLog2, st.miCols);
						st.tileMiColEnd = TileOffset(tc + 1, st.sbCols, hdr.tileColsLog2, st.miCols);

						NkVp8BoolDecoder bd(p, sz);
						if (bd.GetFlag() != 0) {
							ok = false;
							break;
						}
						for (int32 miRow = miRowStart; miRow < miRowEnd && ok; miRow += kMiBlockSize) {
							for (int32 q = 0; q < 3; ++q)
								for (int32 i = 0; i < 16; ++i)
									st.leftEntCtx[q][i] = 0;
							for (int32 i = 0; i < 8; ++i)
								st.leftSegCtx[i] = 0;
							for (int32 miCol = st.tileMiColStart; miCol < st.tileMiColEnd; miCol += kMiBlockSize)
								DecodePartition(bd, st, miRow, miCol, kBlock64x64, 4);
						}
						// Sur trame INTER, contrairement à une trame clé, l'encodeur peut
						// laisser un reliquat d'octets NON consommés en fin de tile (le
						// range coder n'a pas besoin de lire les derniers bits de bourrage
						// une fois tous les symboles décodés sans ambiguïté) — libvpx ne
						// valide d'ailleurs JAMAIS une consommation exacte par tile
						// (seul vpx_reader_has_error, équivalent à notre overreadBytes,
						// détecte une vraie corruption). Un sous-remplissage n'est donc
						// PAS une erreur ; seul un dépassement (overread) l'est.
						if (bd.overreadBytes > 2)
							ok = false;
						if (bd.overreadBytes > stats.maxOverread)
							stats.maxOverread = bd.overreadBytes;
						++stats.tiles;
						p += sz;
						remaining -= sz;
					}
				}

				if (ok && img != nullptr && applyLoopFilter)
					LoopFilterFrame(st);

				// Grille MV/ref de CETTE trame (pour use_prev_frame_mvs de la SUIVANTE) :
				// copie directe (même convention refFrame 0=intra/-1=aucune/1-3=LAST-ALTREF).
				if (ok && outMvGrid != nullptr) {
					outMvGrid->Resize((usize)st.miRows * (usize)st.miCols);
					for (int32 i = 0; i < st.miRows * st.miCols; ++i) {
						NkVp9MvRef &d = (*outMvGrid)[i];
						d.refFrame[0] = st.mi[i].refFrame[0];
						d.refFrame[1] = st.mi[i].refFrame[1];
						d.mvRow[0] = st.mi[i].mv[0].row;
						d.mvCol[0] = st.mi[i].mv[0].col;
						d.mvRow[1] = st.mi[i].mv[1].row;
						d.mvCol[1] = st.mi[i].mv[1].col;
					}
				}

				if (ok && outSegMap != nullptr) {
					outSegMap->Resize((usize)st.miRows * (usize)st.miCols);
					for (int32 i = 0; i < st.miRows * st.miCols; ++i)
						(*outSegMap)[i] = st.currentSegMap[i];
				}

				memory::NkFree(st.mi);
				memory::NkFree(st.aboveSegCtx);
				memory::NkFree(st.currentSegMap);
				for (int32 q = 0; q < 3; ++q)
					memory::NkFree(st.aboveEntCtx[q]);
				return ok;
			}
		} // namespace

		bool NkVp9Decoder::ParseKeyFrameContent(const uint8 *tileData, usize tileSize,
												const NkVp9FrameHeader &hdr, const NkVp9FrameContext &fc,
												const NkVp9CompressedHeader &chdr,
												NkTileParseStats &stats) {
			return ParseOrDecodeKeyContent(tileData, tileSize, hdr, fc, chdr, stats, nullptr);
		}

		bool NkVp9Decoder::DecodeKeyFrame(const uint8 *frame, usize size, NkVp9Image &out,
										  NkVp9EntropyState &entropy, NkTileParseStats *statsOut,
										  bool applyLoopFilter) {
			NkVp9FrameHeader hdr;
			if (!ParseUncompressedHeader(frame, size, hdr))
				return false;
			if (hdr.showExistingFrame || (hdr.frameType != kVp9KeyFrame && !hdr.intraOnly))
				return false;
			const usize hdrBytes = (usize)hdr.uncompressedBytes + (usize)hdr.headerSizeBytes;
			if (hdrBytes >= size || hdr.headerSizeBytes <= 0)
				return false;

			// vp9_setup_past_independence : trame intra (clé/intra-only) → reset
			// systématique (voir la fonction pour le détail resetFrameContext).
			SetupPastIndependence(entropy, hdr);
			SyncSegmentFeatures(hdr, entropy);
			NkVp9FrameContext fc = entropy.frameContexts[hdr.frameContextIdx];

			NkVp9CompressedHeader chdr;
			if (!ParseCompressedHeader(frame + hdr.uncompressedBytes, (usize)hdr.headerSizeBytes, hdr,
									   fc, chdr))
				return false;

			// Carte de segmentation persistée : n'est valide que si elle correspond aux
			// dimensions mi de CETTE trame (sinon indisponible → prédiction 0, cas normatif
			// d'un changement de résolution).
			const int32 miColsChk = (hdr.width + 7) >> 3, miRowsChk = (hdr.height + 7) >> 3;
			const uint8 *lastSegMap = (entropy.lastFrameSegMap.Size() == (usize)miRowsChk * (usize)miColsChk)
										  ? entropy.lastFrameSegMap.Data()
										  : nullptr;
			NkVector<uint8> outSegMap;

			NkVp9FrameCounts counts;
			NkTileParseStats stats;
			const bool ok = ParseOrDecodeKeyContent(frame + hdrBytes, size - hdrBytes, hdr, fc, chdr,
													stats, &out, applyLoopFilter, &counts, lastSegMap,
													&outSegMap);
			if (ok && hdr.segEnabled)
				entropy.lastFrameSegMap = outSegMap;

			if (ok && !hdr.errorResilient && !hdr.frameParallelDecoding) {
				// vp9_adapt_coef_probs s'applique MÊME sur les trames intra-only ; PAS
				// mode/mv (frame_is_intra_only == true ici, toujours). `preFc` = valeur
				// PERSISTÉE du slot (celle d'AVANT cette trame, inchangée par `fc` qui
				// est une copie locale) — la formule de merge ignore les mises à jour
				// forward de l'en-tête de CETTE trame (mêmes principes que libvpx).
				const NkVp9FrameContext preFc = entropy.frameContexts[hdr.frameContextIdx];
				AdaptCoefProbs(fc, preFc, counts, /*intraOnly=*/true, entropy.lastFrameWasKey);
			}
			if (hdr.refreshFrameContext)
				entropy.frameContexts[hdr.frameContextIdx] = fc;
			entropy.lastFrameWasKey = true;

			if (statsOut)
				*statsOut = stats;
			return ok;
		}

		void NkVp9RefBuffer::Build(const NkVp9Image &img, NkVp9RefBuffer &out) {
			const int32 B = kMcBorder;
			out.width = img.width;
			out.height = img.height;
			out.uvWidth = img.uvWidth;
			out.uvHeight = img.uvHeight;
			out.yStride = img.width + 2 * B;
			out.uvStride = img.uvWidth + 2 * B;
			out.yBuf.Resize((usize)out.yStride * (usize)(img.height + 2 * B));
			out.uBuf.Resize((usize)out.uvStride * (usize)(img.uvHeight + 2 * B));
			out.vBuf.Resize((usize)out.uvStride * (usize)(img.uvHeight + 2 * B));
			out.yOrigin = out.yBuf.Data() + B * out.yStride + B;
			out.uOrigin = out.uBuf.Data() + B * out.uvStride + B;
			out.vOrigin = out.vBuf.Data() + B * out.uvStride + B;

			auto Extend = [B](const uint8 *src, int32 srcStride, int32 w, int32 h, uint8 *origin,
							  int32 dstStride) {
				for (int32 y = 0; y < h; ++y)
					for (int32 x = 0; x < w; ++x)
						origin[y * dstStride + x] = src[y * srcStride + x];
				for (int32 y = 0; y < h; ++y) {
					uint8 *row = origin + y * dstStride;
					const uint8 l = row[0], r = row[w - 1];
					for (int32 x = 1; x <= B; ++x) {
						row[-x] = l;
						row[w - 1 + x] = r;
					}
				}
				const uint8 *firstRow = origin - B;
				const uint8 *lastRow = origin + (h - 1) * dstStride - B;
				const int32 fullW = w + 2 * B;
				for (int32 i = 1; i <= B; ++i) {
					uint8 *top = origin - i * dstStride - B;
					uint8 *bot = origin + (h - 1 + i) * dstStride - B;
					for (int32 x = 0; x < fullW; ++x) {
						top[x] = firstRow[x];
						bot[x] = lastRow[x];
					}
				}
			};
			Extend(img.y.Data(), img.yStride, img.width, img.height, out.yOrigin, out.yStride);
			Extend(img.u.Data(), img.uvStride, img.uvWidth, img.uvHeight, out.uOrigin, out.uvStride);
			Extend(img.v.Data(), img.uvStride, img.uvWidth, img.uvHeight, out.vOrigin, out.uvStride);
		}

		bool NkVp9Decoder::DecodeInterFrame(const uint8 *frame, usize size, const NkVp9Image *refImages[3],
											NkVp9Image &out, NkVp9EntropyState &entropy,
											NkTileParseStats *statsOut, bool applyLoopFilter,
											bool usePrevFrameMvs, const NkVp9MvRef *prevMvs,
											NkVector<NkVp9MvRef> *outMvGrid) {
			// Dimensions héritées d'une réf (frame_size_with_refs) : on suppose que les 3
			// références partagent la même résolution (cas courant — un flux qui change
			// de résolution par référence est une fonctionnalité avancée non gérée) et
			// on fournit celle de LAST (refImages[0]) comme candidate d'héritage.
			const int32 guessW = refImages[0] ? refImages[0]->width : 0;
			const int32 guessH = refImages[0] ? refImages[0]->height : 0;
			NkVp9FrameHeader hdr;
			if (!ParseUncompressedHeader(frame, size, hdr, guessW, guessH))
				return false;
			if (hdr.showExistingFrame || hdr.frameType == kVp9KeyFrame || hdr.intraOnly)
				return false;
			if (hdr.width <= 0 || hdr.height <= 0)
				return false; // taille héritée non résolue (sentinelle négative)
			const usize hdrBytes = (usize)hdr.uncompressedBytes + (usize)hdr.headerSizeBytes;
			if (hdrBytes >= size || hdr.headerSizeBytes <= 0)
				return false;

			// vp9_setup_past_independence : une trame INTER normale (ni intra-only, ni
			// error-resilient) ne déclenche AUCUN reset — le slot garde son adaptation.
			SetupPastIndependence(entropy, hdr);
			SyncSegmentFeatures(hdr, entropy);
			NkVp9FrameContext fc = entropy.frameContexts[hdr.frameContextIdx];

			NkVp9CompressedHeader chdr;
			if (!ParseCompressedHeader(frame + hdr.uncompressedBytes, (usize)hdr.headerSizeBytes, hdr, fc, chdr))
				return false;

			// Bordures étendues des 3 références (LAST/GOLDEN/ALTREF).
			NkVp9RefBuffer bufs[3];
			InterFrameCtx ictx;
			for (int32 i = 0; i < 3; ++i) {
				if (refImages[i] == nullptr || refImages[i]->width != hdr.width ||
					refImages[i]->height != hdr.height)
					return false; // référence manquante ou de taille différente (non géré)
				NkVp9RefBuffer::Build(*refImages[i], bufs[i]);
				ictx.refPlanes[i][0] = bufs[i].yOrigin;
				ictx.refPlanes[i][1] = bufs[i].uOrigin;
				ictx.refPlanes[i][2] = bufs[i].vOrigin;
				ictx.refStride[i][0] = bufs[i].yStride;
				ictx.refStride[i][1] = ictx.refStride[i][2] = bufs[i].uvStride;
				ictx.signBias[i + 1] = hdr.refFrameSignBias[i] ? 1 : 0;
			}
			// vp9_setup_compound_reference_mode.
			if (ictx.signBias[1] == ictx.signBias[2]) {
				ictx.compFixedRef = 3; // ALTREF
				ictx.compVarRef[0] = 1;
				ictx.compVarRef[1] = 2; // LAST,GOLDEN
			} else if (ictx.signBias[1] == ictx.signBias[3]) {
				ictx.compFixedRef = 2; // GOLDEN
				ictx.compVarRef[0] = 1;
				ictx.compVarRef[1] = 3; // LAST,ALTREF
			} else {
				ictx.compFixedRef = 1; // LAST
				ictx.compVarRef[0] = 2;
				ictx.compVarRef[1] = 3; // GOLDEN,ALTREF
			}
			ictx.referenceMode = chdr.referenceMode;
			ictx.allowHighPrecisionMv = hdr.allowHighPrecisionMv;
			ictx.interpFilterHdr = hdr.interpFilter;
			ictx.usePrevFrameMvs = usePrevFrameMvs;
			ictx.prevMvs = prevMvs;

			const int32 miColsChk = (hdr.width + 7) >> 3, miRowsChk = (hdr.height + 7) >> 3;
			const uint8 *lastSegMap = (entropy.lastFrameSegMap.Size() == (usize)miRowsChk * (usize)miColsChk)
										  ? entropy.lastFrameSegMap.Data()
										  : nullptr;
			NkVector<uint8> outSegMap;

			NkVp9FrameCounts counts;
			NkTileParseStats stats;
			const bool ok = ParseOrDecodeInterContent(frame + hdrBytes, size - hdrBytes, hdr, fc, chdr, ictx,
													  stats, &out, applyLoopFilter, &counts, outMvGrid,
													  lastSegMap, &outSegMap);
			if (ok && hdr.segEnabled)
				entropy.lastFrameSegMap = outSegMap;

			if (ok && !hdr.errorResilient && !hdr.frameParallelDecoding) {
				// `preFc` = valeur PERSISTÉE du slot (avant cette trame) — `fc` local
				// reste une copie mutée par les updates forward de l'en-tête, jamais
				// écrite dans `entropy` avant cette adaptation (mêmes principes que
				// libvpx : la formule de merge ignore les updates forward de CETTE trame).
				const NkVp9FrameContext preFc = entropy.frameContexts[hdr.frameContextIdx];
				AdaptCoefProbs(fc, preFc, counts, /*intraOnly=*/false, entropy.lastFrameWasKey);
				AdaptModeProbs(fc, preFc, counts, chdr.txMode, hdr.interpFilter);
				AdaptMvProbs(fc, preFc, counts, hdr.allowHighPrecisionMv);
			}
			if (hdr.refreshFrameContext)
				entropy.frameContexts[hdr.frameContextIdx] = fc;
			entropy.lastFrameWasKey = false;

			if (statsOut)
				*statsOut = stats;
			return ok;
		}

		bool NkVp9Decoder::SelfTest() {
			// 1) Superframe synthétique : 2 trames (3 + 4 octets), mag=1.
			{
				uint8 buf[3 + 4 + 4];
				for (int32 i = 0; i < 7; ++i)
					buf[i] = (uint8)(i + 1);
				const uint8 marker = (uint8)(0xC0 | (0 << 3) | (2 - 1)); // mag=1, frames=2
				buf[7] = marker;
				buf[8] = 3;
				buf[9] = 4;
				buf[10] = marker;
				NkVp9Superframe sf;
				if (!NkVp9Decoder::ParseSuperframe(buf, sizeof(buf), sf))
					return false;
				if (sf.count != 2 || sf.offsets[0] != 0 || sf.sizes[0] != 3 || sf.offsets[1] != 3 ||
					sf.sizes[1] != 4)
					return false;
			}
			// 2) Charge sans index : 1 trame.
			{
				uint8 raw[5] = {0x82, 0, 0, 0, 0}; // dernier octet != 110xxxxx
				NkVp9Superframe sf;
				if (!NkVp9Decoder::ParseSuperframe(raw, sizeof(raw), sf))
					return false;
				if (sf.count != 1 || sf.sizes[0] != 5)
					return false;
			}
			// 3) En-tête clé minimal forgé : marker=2, profil 0, show_existing=0,
			// key, show=1, err=0, sync, cs=BT601+range, 64x48, pas de render,
			// puis contexte/LF/quant/seg/tiles/size à zéro.
			{
				uint8 bits[64] = {0};
				int32 bp = 0;
				auto Put = [&](int32 v, int32 n) {
					for (int32 i = n - 1; i >= 0; --i) {
						if ((v >> i) & 1)
							bits[bp >> 3] |= (uint8)(0x80u >> (bp & 7));
						++bp;
					}
				};
				Put(2, 2); // frame_marker
				Put(0, 1); // profile low
				Put(0, 1); // profile high
				Put(0, 1); // show_existing
				Put(0, 1); // frame_type = KEY
				Put(1, 1); // show_frame
				Put(0, 1); // error_resilient
				Put(0x49, 8);
				Put(0x83, 8);
				Put(0x42, 8);	  // sync
				Put(1, 3);		  // color_space = BT601
				Put(0, 1);		  // color_range
				Put(63, 16);	  // width-1
				Put(47, 16);	  // height-1
				Put(0, 1);		  // render_size flag
				Put(1, 1);		  // refresh_frame_context
				Put(0, 1);		  // frame_parallel
				Put(0, 2);		  // frame_context_idx
				Put(0, 6);		  // lf level
				Put(0, 3);		  // lf sharpness
				Put(0, 1);		  // lf delta enabled
				Put(0, 8);		  // base_q_idx
				Put(0, 1);		  // delta_q_y_dc flag
				Put(0, 1);		  // delta_q_uv_dc flag
				Put(0, 1);		  // delta_q_uv_ac flag
				Put(0, 1);		  // seg enabled
				Put(0, 1);		  // tile rows (cols : min==max==0 → 0 bit)
				Put(123, 16);	  // header_size
				NkVp9FrameHeader h;
				if (!NkVp9Decoder::ParseUncompressedHeader(bits, sizeof(bits), h))
					return false;
				if (h.frameType != kVp9KeyFrame || !h.showFrame || h.width != 64 || h.height != 48)
					return false;
				if (h.colorSpace != 1 || h.bitDepth != 8 || h.subsamplingX != 1 || h.subsamplingY != 1)
					return false;
				if (!h.lossless || h.headerSizeBytes != 123)
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
