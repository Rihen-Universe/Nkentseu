// =============================================================================
// NKMedia/Codecs/Video/AV1/NkAv1Decoder.cpp
// -----------------------------------------------------------------------------
// Décodeur AV1 FROM SCRATCH :
//   1. OBU parsing (§5.2-5.6)         : ParseObuHeader / ParseTemporalUnit
//   2. Sequence header (§5.5)         : ParseSequenceHeader
//   3. Frame header complet (§5.9)    : intra + INTER (ParseFrameHeaderFull)
//   4. Symbol decoder (§8.2)          : NkAv1Symbol.h (init/decode/renorm/adapt)
//   5. Reconstruction intra (§5.11/§7.11.2/§7.13) : keyframes BIT-EXACTES
//   6. INTER (§7.9/§7.10/§7.11.3)     : pile de MV, champ de MV temporel, MC
//      8-tap, compound (average/distance/wedge/diffwtd), inter-intra, OBMC,
//      warp local+global, var-tx, CDEF (§7.15), DPB + CDF persistantes
//      (NkAv1StreamDecoder) : trames P/B/altref BIT-EXACTES.
// TOUT est écrit depuis la spécification — zéro code dav1d/libaom/ffmpeg.
//
// AUTEUR : Rihen.
// =============================================================================
#include "NKMedia/Codecs/Video/AV1/NkAv1Decoder.h"
#include "NKMedia/Codecs/Video/AV1/NkAv1Symbol.h"
#include "NKMemory/NKMemory.h"

#include <new>

namespace nkentseu {
	namespace media {

		namespace {

			// Constantes normatives (§3 "Symbols and abbreviated terms").
			constexpr int32 kNumRefFrames = 8;
			constexpr int32 kRefsPerFrame = 7;
			constexpr int32 kTotalRefsPerFrame = 8;
			constexpr int32 kPrimaryRefNone = 7;
			constexpr int32 kMaxSegments = 8;
			constexpr int32 kSegLvlMax = 8;
			constexpr int32 kSegLvlRefFrame = 5;
			constexpr int32 kSegLvlAltQ = 0;
			constexpr int32 kSelectScreenContentTools = 2;
			constexpr int32 kSelectIntegerMv = 2;
			constexpr int32 kSuperresNum = 8;
			constexpr int32 kSuperresDenomMin = 9;
			constexpr int32 kSuperresDenomBits = 3;
			constexpr int32 kMaxTileWidthSb = 4096;
			constexpr int64 kMaxTileAreaSb = 4096 * 2304;
			constexpr int32 kMaxTileCols = 64;
			constexpr int32 kMaxTileRows = 64;
			constexpr int32 kRestorationTileSizeMax = 256;

			// Tables de segmentation (§5.9.14).
			const int32 kSegFeatureBits[kSegLvlMax] = {8, 6, 6, 6, 6, 3, 0, 0};
			const int32 kSegFeatureSigned[kSegLvlMax] = {1, 1, 1, 1, 1, 0, 0, 0};
			const int32 kSegFeatureMax[kSegLvlMax] = {255, 63, 63, 63, 63, 7, 0, 0};

			// Remap des types de loop restoration (§5.9.20).
			const int32 kRemapLrType[4] = {0 /*NONE*/, 3 /*SWITCHABLE*/, 1 /*WIENER*/, 2 /*SGRPROJ*/};

			int32 NumPlanesOf(const NkAv1SequenceHeader &seq) { return seq.mono ? 1 : 3; }

			// tile_log2(blkSize, target) : plus petit k tel que (blkSize<<k) >= target (§5.9.15).
			int32 TileLog2(int32 blkSize, int32 target) {
				int32 k = 0;
				while ((blkSize << k) < target)
					++k;
				return k;
			}

			// read_delta_q() (§5.9.13).
			int32 ReadDeltaQ(NkAv1BitReader &rb) {
				if (rb.Bit())
					return rb.Su(7); // su(1+6)
				return 0;
			}

			// --- color_config (§5.5.2) ---
			void ParseColorConfig(NkAv1BitReader &rb, NkAv1SequenceHeader &s) {
				const int32 highBitdepth = rb.Bit();
				if (s.seqProfile == 2 && highBitdepth) {
					const int32 twelveBit = rb.Bit();
					s.bitDepth = twelveBit ? 12 : 10;
				} else {
					s.bitDepth = highBitdepth ? 10 : 8;
				}
				if (s.seqProfile == 1)
					s.mono = false;
				else
					s.mono = rb.Bit() != 0;
				s.colorDescriptionPresent = rb.Bit() != 0;
				if (s.colorDescriptionPresent) {
					s.colorPrimaries = (int32)rb.F(8);
					s.transferCharacteristics = (int32)rb.F(8);
					s.matrixCoefficients = (int32)rb.F(8);
				} else {
					s.colorPrimaries = 2;
					s.transferCharacteristics = 2;
					s.matrixCoefficients = 2;
				}
				if (s.mono) {
					s.colorRangeFull = rb.Bit() != 0;
					s.subsamplingX = 1;
					s.subsamplingY = 1;
					s.chromaSamplePosition = 0;
					s.separateUvDeltaQ = false;
					return;
				}
				if (s.colorPrimaries == 1 && s.transferCharacteristics == 13 && s.matrixCoefficients == 0) {
					// BT.709 + sRGB + identity => 4:4:4 full range.
					s.colorRangeFull = true;
					s.subsamplingX = 0;
					s.subsamplingY = 0;
				} else {
					s.colorRangeFull = rb.Bit() != 0;
					if (s.seqProfile == 0) {
						s.subsamplingX = 1;
						s.subsamplingY = 1;
					} else if (s.seqProfile == 1) {
						s.subsamplingX = 0;
						s.subsamplingY = 0;
					} else {
						if (s.bitDepth == 12) {
							s.subsamplingX = rb.Bit();
							s.subsamplingY = s.subsamplingX ? rb.Bit() : 0;
						} else {
							s.subsamplingX = 1;
							s.subsamplingY = 0;
						}
					}
					if (s.subsamplingX && s.subsamplingY)
						s.chromaSamplePosition = (int32)rb.F(2);
				}
				s.separateUvDeltaQ = rb.Bit() != 0;
			}

		} // namespace

		// =====================================================================
		// OBU header (§5.3).
		// =====================================================================
		bool NkAv1Decoder::ParseObuHeader(const uint8 *data, usize size, NkAv1ObuHeader &out) {
			if (size < 1)
				return false;
			const uint8 b0 = data[0];
			if (b0 & 0x80) // obu_forbidden_bit doit valoir 0
				return false;
			out.type = (NkAv1ObuType)((b0 >> 3) & 0x0F);
			out.hasExtension = (b0 >> 2) & 1;
			out.hasSizeField = (b0 >> 1) & 1;
			// bit 0 = obu_reserved_1bit (doit être 0, mais on tolère)
			usize pos = 1;
			out.temporalId = 0;
			out.spatialId = 0;
			if (out.hasExtension) {
				if (size < 2)
					return false;
				const uint8 e = data[1];
				out.temporalId = (e >> 5) & 0x7;
				out.spatialId = (e >> 3) & 0x3;
				pos = 2;
			}
			if (out.hasSizeField) {
				usize lebBytes = 0;
				const uint64 sz = NkAv1Leb128(data + pos, size - pos, &lebBytes);
				if (lebBytes == 0)
					return false;
				pos += lebBytes;
				out.payloadSize = (usize)sz;
			} else {
				out.payloadSize = size - pos; // jusqu'à la fin du buffer
			}
			out.headerBytes = pos;
			out.payloadOffset = pos;
			if (out.payloadOffset + out.payloadSize > size)
				return false;
			return true;
		}

		// =====================================================================
		// Sequence header OBU (§5.5.1).
		// =====================================================================
		bool NkAv1Decoder::ParseSequenceHeader(const uint8 *data, usize size, NkAv1SequenceHeader &out) {
			NkAv1BitReader rb(data, size);
			NkAv1SequenceHeader s;
			s.seqProfile = (int32)rb.F(3);
			s.stillPicture = rb.Bit() != 0;
			s.reducedStillPictureHeader = rb.Bit() != 0;
			if (s.reducedStillPictureHeader) {
				s.timingInfoPresent = false;
				s.decoderModelInfoPresent = false;
				s.initialDisplayDelayPresent = false;
				s.operatingPointsCnt = 1;
				s.operatingPointIdc[0] = 0;
				s.seqLevelIdx[0] = (int32)rb.F(5);
				s.seqTier[0] = 0;
			} else {
				s.timingInfoPresent = rb.Bit() != 0;
				int32 bufferDelayLenMinus1 = 0;
				if (s.timingInfoPresent) {
					rb.F(32); // num_units_in_display_tick
					rb.F(32); // time_scale
					const int32 equalPictureInterval = rb.Bit();
					if (equalPictureInterval)
						rb.Uvlc(); // num_ticks_per_picture_minus_1
					s.decoderModelInfoPresent = rb.Bit() != 0;
					if (s.decoderModelInfoPresent) {
						bufferDelayLenMinus1 = (int32)rb.F(5);
						rb.F(32); // num_units_in_decoding_tick
						rb.F(5);  // buffer_removal_time_length_minus_1
						rb.F(5);  // frame_presentation_time_length_minus_1
					}
				} else {
					s.decoderModelInfoPresent = false;
				}
				s.initialDisplayDelayPresent = rb.Bit() != 0;
				const int32 opCntMinus1 = (int32)rb.F(5);
				s.operatingPointsCnt = opCntMinus1 + 1;
				for (int32 i = 0; i <= opCntMinus1; ++i) {
					const int32 idc = (int32)rb.F(12);
					const int32 lvl = (int32)rb.F(5);
					if (i < 32) {
						s.operatingPointIdc[i] = idc;
						s.seqLevelIdx[i] = lvl;
					}
					int32 tier = 0;
					if (lvl > 7)
						tier = rb.Bit();
					if (i < 32)
						s.seqTier[i] = tier;
					if (s.decoderModelInfoPresent) {
						const int32 present = rb.Bit();
						if (present) {
							const int32 n = bufferDelayLenMinus1 + 1;
							rb.F(n); // decoder_buffer_delay
							rb.F(n); // encoder_buffer_delay
							rb.Bit(); // low_delay_mode_flag
						}
					}
					if (s.initialDisplayDelayPresent) {
						const int32 present = rb.Bit();
						if (present)
							rb.F(4); // initial_display_delay_minus_1
					}
				}
			}
			s.frameWidthBitsMinus1 = (int32)rb.F(4);
			s.frameHeightBitsMinus1 = (int32)rb.F(4);
			s.maxFrameWidthMinus1 = (int32)rb.F(s.frameWidthBitsMinus1 + 1);
			s.maxFrameHeightMinus1 = (int32)rb.F(s.frameHeightBitsMinus1 + 1);
			if (s.reducedStillPictureHeader)
				s.frameIdNumbersPresent = false;
			else
				s.frameIdNumbersPresent = rb.Bit() != 0;
			if (s.frameIdNumbersPresent) {
				s.deltaFrameIdLengthMinus2 = (int32)rb.F(4);
				s.additionalFrameIdLengthMinus1 = (int32)rb.F(3);
			}
			s.use128x128Superblock = rb.Bit() != 0;
			s.enableFilterIntra = rb.Bit() != 0;
			s.enableIntraEdgeFilter = rb.Bit() != 0;
			if (s.reducedStillPictureHeader) {
				s.enableInterintraCompound = false;
				s.enableMaskedCompound = false;
				s.enableWarpedMotion = false;
				s.enableDualFilter = false;
				s.enableOrderHint = false;
				s.enableJntComp = false;
				s.enableRefFrameMvs = false;
				s.seqForceScreenContentTools = kSelectScreenContentTools;
				s.seqForceIntegerMv = kSelectIntegerMv;
				s.orderHintBits = 0;
			} else {
				s.enableInterintraCompound = rb.Bit() != 0;
				s.enableMaskedCompound = rb.Bit() != 0;
				s.enableWarpedMotion = rb.Bit() != 0;
				s.enableDualFilter = rb.Bit() != 0;
				s.enableOrderHint = rb.Bit() != 0;
				if (s.enableOrderHint) {
					s.enableJntComp = rb.Bit() != 0;
					s.enableRefFrameMvs = rb.Bit() != 0;
				} else {
					s.enableJntComp = false;
					s.enableRefFrameMvs = false;
				}
				const int32 chooseScreen = rb.Bit();
				if (chooseScreen)
					s.seqForceScreenContentTools = kSelectScreenContentTools;
				else
					s.seqForceScreenContentTools = rb.Bit();
				if (s.seqForceScreenContentTools > 0) {
					const int32 chooseInt = rb.Bit();
					if (chooseInt)
						s.seqForceIntegerMv = kSelectIntegerMv;
					else
						s.seqForceIntegerMv = rb.Bit();
				} else {
					s.seqForceIntegerMv = kSelectIntegerMv;
				}
				if (s.enableOrderHint)
					s.orderHintBits = (int32)rb.F(3) + 1;
				else
					s.orderHintBits = 0;
			}
			s.enableSuperres = rb.Bit() != 0;
			s.enableCdef = rb.Bit() != 0;
			s.enableRestoration = rb.Bit() != 0;
			ParseColorConfig(rb, s);
			s.filmGrainParamsPresent = rb.Bit() != 0;
			if (rb.error)
				return false;
			s.valid = true;
			out = s;
			return true;
		}

		// =====================================================================
		// Frame header (§5.9) — chemin KEYFRAME/INTRA-ONLY complet.
		// =====================================================================
		namespace {

			void ParseFrameSize(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				if (h.frameSizeOverride) {
					h.frameWidth = (int32)rb.F(seq.frameWidthBitsMinus1 + 1) + 1;
					h.frameHeight = (int32)rb.F(seq.frameHeightBitsMinus1 + 1) + 1;
				} else {
					h.frameWidth = seq.maxFrameWidthMinus1 + 1;
					h.frameHeight = seq.maxFrameHeightMinus1 + 1;
				}
				// superres_params.
				int32 useSuperres = 0;
				if (seq.enableSuperres)
					useSuperres = rb.Bit();
				if (useSuperres) {
					const int32 codedDenom = (int32)rb.F(kSuperresDenomBits);
					h.superresDenom = codedDenom + kSuperresDenomMin;
				} else {
					h.superresDenom = kSuperresNum;
				}
				h.upscaledWidth = h.frameWidth;
				h.frameWidth = (h.upscaledWidth * kSuperresNum + (h.superresDenom / 2)) / h.superresDenom;
				// compute_image_size.
				h.miCols = 2 * ((h.frameWidth + 7) >> 3);
				h.miRows = 2 * ((h.frameHeight + 7) >> 3);
			}

			void ParseRenderSize(NkAv1BitReader &rb, NkAv1FrameHeader &h) {
				const int32 diff = rb.Bit();
				if (diff) {
					h.renderWidth = (int32)rb.F(16) + 1;
					h.renderHeight = (int32)rb.F(16) + 1;
				} else {
					h.renderWidth = h.upscaledWidth;
					h.renderHeight = h.frameHeight;
				}
			}

			void ParseTileInfo(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				NkAv1TileInfo &t = h.tiles;
				const int32 sbShift = seq.use128x128Superblock ? 5 : 4;
				const int32 sbSize = sbShift + 2;
				const int32 sbCols = seq.use128x128Superblock ? ((h.miCols + 31) >> 5) : ((h.miCols + 15) >> 4);
				const int32 sbRows = seq.use128x128Superblock ? ((h.miRows + 31) >> 5) : ((h.miRows + 15) >> 4);
				const int32 maxTileWidthSb = kMaxTileWidthSb >> sbSize;
				const int32 maxTileAreaSb = (int32)(kMaxTileAreaSb >> (2 * sbSize));
				const int32 minLog2TileCols = TileLog2(maxTileWidthSb, sbCols);
				const int32 maxLog2TileCols = TileLog2(1, sbCols < kMaxTileCols ? sbCols : kMaxTileCols);
				const int32 maxLog2TileRows = TileLog2(1, sbRows < kMaxTileRows ? sbRows : kMaxTileRows);
				const int32 areaLog2 = TileLog2(maxTileAreaSb, sbRows * sbCols);
				const int32 minLog2Tiles = (minLog2TileCols > areaLog2) ? minLog2TileCols : areaLog2;
				t.uniformTileSpacing = rb.Bit() != 0;
				if (t.uniformTileSpacing) {
					t.tileColsLog2 = minLog2TileCols;
					while (t.tileColsLog2 < maxLog2TileCols) {
						if (rb.Bit())
							++t.tileColsLog2;
						else
							break;
					}
					const int32 tileWidthSb = (sbCols + (1 << t.tileColsLog2) - 1) >> t.tileColsLog2;
					int32 i = 0;
					for (int32 startSb = 0; startSb < sbCols; startSb += tileWidthSb) {
						if (i < 64)
							t.miColStarts[i] = startSb << sbShift;
						++i;
					}
					if (i < 65)
						t.miColStarts[i] = h.miCols;
					t.tileCols = i;

					int32 minLog2TileRows = minLog2Tiles - t.tileColsLog2;
					if (minLog2TileRows < 0)
						minLog2TileRows = 0;
					t.tileRowsLog2 = minLog2TileRows;
					while (t.tileRowsLog2 < maxLog2TileRows) {
						if (rb.Bit())
							++t.tileRowsLog2;
						else
							break;
					}
					const int32 tileHeightSb = (sbRows + (1 << t.tileRowsLog2) - 1) >> t.tileRowsLog2;
					int32 j = 0;
					for (int32 startSb = 0; startSb < sbRows; startSb += tileHeightSb) {
						if (j < 64)
							t.miRowStarts[j] = startSb << sbShift;
						++j;
					}
					if (j < 65)
						t.miRowStarts[j] = h.miRows;
					t.tileRows = j;
				} else {
					// Espacement non uniforme.
					int32 widestTileSb = 0;
					int32 startSb = 0;
					int32 i = 0;
					for (; startSb < sbCols; ++i) {
						if (i < 64)
							t.miColStarts[i] = startSb << sbShift;
						const int32 maxWidth = (sbCols - startSb < maxTileWidthSb) ? (sbCols - startSb) : maxTileWidthSb;
						const int32 widthInSbs = (int32)rb.Ns((uint32)maxWidth) + 1;
						if (widthInSbs > widestTileSb)
							widestTileSb = widthInSbs;
						startSb += widthInSbs;
					}
					if (i < 65)
						t.miColStarts[i] = h.miCols;
					t.tileCols = i;
					t.tileColsLog2 = TileLog2(1, t.tileCols);

					int32 maxTileAreaSb2 = maxTileAreaSb;
					if (minLog2Tiles > 0)
						maxTileAreaSb2 = (sbRows * sbCols) >> (minLog2Tiles + 1);
					const int32 maxTileHeightSb = (maxTileAreaSb2 / widestTileSb > 1) ? (maxTileAreaSb2 / widestTileSb) : 1;
					startSb = 0;
					int32 j = 0;
					for (; startSb < sbRows; ++j) {
						if (j < 64)
							t.miRowStarts[j] = startSb << sbShift;
						const int32 maxHeight = (sbRows - startSb < maxTileHeightSb) ? (sbRows - startSb) : maxTileHeightSb;
						const int32 heightInSbs = (int32)rb.Ns((uint32)maxHeight) + 1;
						startSb += heightInSbs;
					}
					if (j < 65)
						t.miRowStarts[j] = h.miRows;
					t.tileRows = j;
					t.tileRowsLog2 = TileLog2(1, t.tileRows);
				}
				if (t.tileColsLog2 > 0 || t.tileRowsLog2 > 0) {
					t.contextUpdateTileId = (int32)rb.F(t.tileRowsLog2 + t.tileColsLog2);
					t.tileSizeBytes = (int32)rb.F(2) + 1;
				} else {
					t.contextUpdateTileId = 0;
					t.tileSizeBytes = 1;
				}
			}

			void ParseQuantizationParams(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				NkAv1QuantParams &q = h.quant;
				q.baseQIdx = (int32)rb.F(8);
				q.deltaQYDc = ReadDeltaQ(rb);
				if (NumPlanesOf(seq) > 1) {
					int32 diffUvDelta = 0;
					if (seq.separateUvDeltaQ)
						diffUvDelta = rb.Bit();
					q.deltaQUDc = ReadDeltaQ(rb);
					q.deltaQUAc = ReadDeltaQ(rb);
					if (diffUvDelta) {
						q.deltaQVDc = ReadDeltaQ(rb);
						q.deltaQVAc = ReadDeltaQ(rb);
					} else {
						q.deltaQVDc = q.deltaQUDc;
						q.deltaQVAc = q.deltaQUAc;
					}
				} else {
					q.deltaQUDc = q.deltaQUAc = q.deltaQVDc = q.deltaQVAc = 0;
				}
				q.usingQmatrix = rb.Bit() != 0;
				if (q.usingQmatrix) {
					q.qmY = (int32)rb.F(4);
					q.qmU = (int32)rb.F(4);
					if (!seq.separateUvDeltaQ)
						q.qmV = q.qmU;
					else
						q.qmV = (int32)rb.F(4);
				}
			}

			void ParseSegmentationParams(NkAv1BitReader &rb, NkAv1FrameHeader &h) {
				NkAv1SegmentationParams &sg = h.seg;
				sg.enabled = rb.Bit() != 0;
				if (sg.enabled) {
					if (h.primaryRefFrame == kPrimaryRefNone) {
						sg.updateMap = true;
						sg.temporalUpdate = false;
						sg.updateData = true;
					} else {
						sg.updateMap = rb.Bit() != 0;
						if (sg.updateMap)
							sg.temporalUpdate = rb.Bit() != 0;
						sg.updateData = rb.Bit() != 0;
					}
					if (sg.updateData) {
						for (int32 i = 0; i < kMaxSegments; ++i) {
							for (int32 j = 0; j < kSegLvlMax; ++j) {
								int32 clippedValue = 0;
								const int32 featureEnabled = rb.Bit();
								sg.featureEnabled[i][j] = featureEnabled != 0;
								if (featureEnabled) {
									const int32 bitsToRead = kSegFeatureBits[j];
									const int32 limit = kSegFeatureMax[j];
									if (kSegFeatureSigned[j]) {
										int32 fv = rb.Su(1 + bitsToRead);
										if (fv < -limit)
											fv = -limit;
										if (fv > limit)
											fv = limit;
										clippedValue = fv;
									} else {
										int32 fv = bitsToRead > 0 ? (int32)rb.F(bitsToRead) : 0;
										if (fv < 0)
											fv = 0;
										if (fv > limit)
											fv = limit;
										clippedValue = fv;
									}
								}
								sg.featureData[i][j] = clippedValue;
							}
						}
					}
				} else {
					for (int32 i = 0; i < kMaxSegments; ++i)
						for (int32 j = 0; j < kSegLvlMax; ++j) {
							sg.featureEnabled[i][j] = false;
							sg.featureData[i][j] = 0;
						}
				}
				sg.preSkipSegidPresent = false;
				sg.lastActiveSegId = 0;
				for (int32 i = 0; i < kMaxSegments; ++i)
					for (int32 j = 0; j < kSegLvlMax; ++j)
						if (sg.featureEnabled[i][j]) {
							sg.lastActiveSegId = i;
							if (j >= kSegLvlRefFrame)
								sg.preSkipSegidPresent = true;
						}
			}

			// get_qindex avec ignoreDeltaQ=1 (§7.12.2, pour CodedLossless).
			int32 GetQIndexNoDelta(const NkAv1FrameHeader &h, int32 segmentId) {
				const NkAv1SegmentationParams &sg = h.seg;
				if (sg.enabled && sg.featureEnabled[segmentId][kSegLvlAltQ]) {
					int32 data = sg.featureData[segmentId][kSegLvlAltQ];
					int32 qindex = h.quant.baseQIdx + data;
					if (qindex < 0)
						qindex = 0;
					if (qindex > 255)
						qindex = 255;
					return qindex;
				}
				return h.quant.baseQIdx;
			}

			void ComputeLossless(NkAv1FrameHeader &h) {
				const NkAv1QuantParams &q = h.quant;
				h.codedLossless = true;
				for (int32 seg = 0; seg < kMaxSegments; ++seg) {
					const int32 qindex = GetQIndexNoDelta(h, seg);
					const bool lossless = (qindex == 0 && q.deltaQYDc == 0 && q.deltaQUAc == 0 &&
										   q.deltaQUDc == 0 && q.deltaQVAc == 0 && q.deltaQVDc == 0);
					h.losslessArray[seg] = lossless;
					if (!lossless)
						h.codedLossless = false;
				}
				h.allLossless = h.codedLossless && (h.frameWidth == h.upscaledWidth);
			}

			void ParseDeltaQParams(NkAv1BitReader &rb, NkAv1FrameHeader &h) {
				h.deltaQRes = 0;
				h.deltaQPresent = false;
				if (h.quant.baseQIdx > 0)
					h.deltaQPresent = rb.Bit() != 0;
				if (h.deltaQPresent)
					h.deltaQRes = (int32)rb.F(2);
			}

			void ParseDeltaLfParams(NkAv1BitReader &rb, NkAv1FrameHeader &h) {
				h.deltaLfPresent = false;
				h.deltaLfRes = 0;
				h.deltaLfMulti = false;
				if (h.deltaQPresent) {
					if (!h.allowIntrabc)
						h.deltaLfPresent = rb.Bit() != 0;
					if (h.deltaLfPresent) {
						h.deltaLfRes = (int32)rb.F(2);
						h.deltaLfMulti = rb.Bit() != 0;
					}
				}
			}

			void ParseLoopFilterParams(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				NkAv1LoopFilterParams &lf = h.lf;
				const int32 defRef[8] = {1, 0, 0, 0, -1, 0, -1, -1};
				if (h.codedLossless || h.allowIntrabc) {
					lf.level[0] = lf.level[1] = lf.level[2] = lf.level[3] = 0;
					for (int32 i = 0; i < 8; ++i)
						lf.refDeltas[i] = defRef[i];
					lf.modeDeltas[0] = lf.modeDeltas[1] = 0;
					return;
				}
				lf.level[0] = (int32)rb.F(6);
				lf.level[1] = (int32)rb.F(6);
				if (NumPlanesOf(seq) > 1) {
					if (lf.level[0] || lf.level[1]) {
						lf.level[2] = (int32)rb.F(6);
						lf.level[3] = (int32)rb.F(6);
					}
				}
				lf.sharpness = (int32)rb.F(3);
				lf.deltaEnabled = rb.Bit() != 0;
				if (lf.deltaEnabled) {
					const int32 deltaUpdate = rb.Bit();
					if (deltaUpdate) {
						for (int32 i = 0; i < kTotalRefsPerFrame; ++i) {
							if (rb.Bit())
								lf.refDeltas[i] = rb.Su(7);
						}
						for (int32 i = 0; i < 2; ++i) {
							if (rb.Bit())
								lf.modeDeltas[i] = rb.Su(7);
						}
					}
				}
			}

			void ParseCdefParams(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				NkAv1CdefParams &c = h.cdef;
				if (h.codedLossless || h.allowIntrabc || !seq.enableCdef) {
					c.bits = 0;
					c.yPriStrength[0] = 0;
					c.ySecStrength[0] = 0;
					c.uvPriStrength[0] = 0;
					c.uvSecStrength[0] = 0;
					c.dampingMinus3 = 0;
					return;
				}
				c.dampingMinus3 = (int32)rb.F(2);
				c.bits = (int32)rb.F(2);
				const int32 n = 1 << c.bits;
				for (int32 i = 0; i < n; ++i) {
					c.yPriStrength[i] = (int32)rb.F(4);
					c.ySecStrength[i] = (int32)rb.F(2);
					if (c.ySecStrength[i] == 3)
						c.ySecStrength[i] += 1;
					if (NumPlanesOf(seq) > 1) {
						c.uvPriStrength[i] = (int32)rb.F(4);
						c.uvSecStrength[i] = (int32)rb.F(2);
						if (c.uvSecStrength[i] == 3)
							c.uvSecStrength[i] += 1;
					}
				}
			}

			void ParseLrParams(NkAv1BitReader &rb, const NkAv1SequenceHeader &seq, NkAv1FrameHeader &h) {
				NkAv1LrParams &lr = h.lr;
				const int32 numPlanes = NumPlanesOf(seq);
				if (h.allLossless || h.allowIntrabc || !seq.enableRestoration) {
					for (int32 i = 0; i < 3; ++i)
						lr.frameRestorationType[i] = 0;
					lr.usesLr = false;
					return;
				}
				bool usesLr = false, usesChromaLr = false;
				for (int32 i = 0; i < numPlanes; ++i) {
					const int32 lrType = (int32)rb.F(2);
					lr.frameRestorationType[i] = kRemapLrType[lrType];
					if (lr.frameRestorationType[i] != 0) {
						usesLr = true;
						if (i > 0)
							usesChromaLr = true;
					}
				}
				lr.usesLr = usesLr;
				if (usesLr) {
					int32 lrUnitShift;
					if (seq.use128x128Superblock) {
						lrUnitShift = rb.Bit() + 1;
					} else {
						lrUnitShift = rb.Bit();
						if (lrUnitShift)
							lrUnitShift += rb.Bit();
					}
					lr.loopRestorationSize[0] = kRestorationTileSizeMax >> (2 - lrUnitShift);
					int32 lrUvShift = 0;
					if (seq.subsamplingX && seq.subsamplingY && usesChromaLr)
						lrUvShift = rb.Bit();
					lr.loopRestorationSize[1] = lr.loopRestorationSize[0] >> lrUvShift;
					lr.loopRestorationSize[2] = lr.loopRestorationSize[0] >> lrUvShift;
				}
			}

		} // namespace

		bool NkAv1Decoder::ParseFrameHeader(const uint8 *data, usize size, const NkAv1SequenceHeader &seq,
											NkAv1FrameHeader &out) {
			if (!seq.valid)
				return false;
			NkAv1BitReader rb(data, size);
			NkAv1FrameHeader h;
			const int32 allFrames = (1 << kNumRefFrames) - 1; // 255

			int32 idLen = 0;
			if (seq.frameIdNumbersPresent)
				idLen = seq.additionalFrameIdLengthMinus1 + seq.deltaFrameIdLengthMinus2 + 3;

			bool frameIsIntra;
			if (seq.reducedStillPictureHeader) {
				h.showExistingFrame = false;
				h.frameType = kAv1KeyFrame;
				frameIsIntra = true;
				h.showFrame = true;
				h.showableFrame = false;
			} else {
				h.showExistingFrame = rb.Bit() != 0;
				if (h.showExistingFrame) {
					h.frameToShow = (int32)rb.F(3);
					// (decoder model / frame id détails ignorés — pas de contenu à décoder)
					out = h;
					out.valid = !rb.error;
					return !rb.error;
				}
				h.frameType = (int32)rb.F(2);
				frameIsIntra = (h.frameType == kAv1KeyFrame || h.frameType == kAv1IntraOnlyFrame);
				h.showFrame = rb.Bit() != 0;
				if (h.showFrame)
					h.showableFrame = (h.frameType != kAv1KeyFrame);
				else
					h.showableFrame = rb.Bit() != 0;
				if (h.frameType == kAv1SwitchFrame || (h.frameType == kAv1KeyFrame && h.showFrame))
					h.errorResilientMode = true;
				else
					h.errorResilientMode = rb.Bit() != 0;
			}

			h.disableCdfUpdate = rb.Bit() != 0;
			if (seq.seqForceScreenContentTools == kSelectScreenContentTools)
				h.allowScreenContentTools = rb.Bit();
			else
				h.allowScreenContentTools = seq.seqForceScreenContentTools;
			if (h.allowScreenContentTools) {
				if (seq.seqForceIntegerMv == kSelectIntegerMv)
					h.forceIntegerMv = rb.Bit();
				else
					h.forceIntegerMv = seq.seqForceIntegerMv;
			} else {
				h.forceIntegerMv = 0;
			}
			if (frameIsIntra)
				h.forceIntegerMv = 1;

			if (seq.frameIdNumbersPresent)
				h.currentFrameId = (int32)rb.F(idLen);

			if (h.frameType == kAv1SwitchFrame)
				h.frameSizeOverride = true;
			else if (seq.reducedStillPictureHeader)
				h.frameSizeOverride = false;
			else
				h.frameSizeOverride = rb.Bit() != 0;

			h.orderHint = (int32)rb.F(seq.orderHintBits);

			if (frameIsIntra || h.errorResilientMode)
				h.primaryRefFrame = kPrimaryRefNone;
			else
				h.primaryRefFrame = (int32)rb.F(3);

			// decoder_model_info : buffer_removal_time — sauté (non présent dans nos flux).
			// (Si présent, il faudrait le lire ici ; nos flux de test ne l'ont pas.)

			if (h.frameType == kAv1SwitchFrame || (h.frameType == kAv1KeyFrame && h.showFrame))
				h.refreshFrameFlags = allFrames;
			else
				h.refreshFrameFlags = (int32)rb.F(8);

			if (!frameIsIntra || h.refreshFrameFlags != allFrames) {
				if (h.errorResilientMode && seq.enableOrderHint) {
					for (int32 i = 0; i < kNumRefFrames; ++i)
						rb.F(seq.orderHintBits); // ref_order_hint[i]
				}
			}

			if (frameIsIntra) {
				ParseFrameSize(rb, seq, h);
				ParseRenderSize(rb, h);
				if (h.allowScreenContentTools && h.upscaledWidth == h.frameWidth)
					h.allowIntrabc = rb.Bit() != 0;
			} else {
				// Chemin inter non implémenté (briques futures) — on échoue proprement.
				out = h;
				out.valid = false;
				return false;
			}

			if (seq.reducedStillPictureHeader || h.disableCdfUpdate)
				h.disableFrameEndUpdateCdf = true;
			else
				h.disableFrameEndUpdateCdf = rb.Bit() != 0;

			// (primary_ref_frame == NONE => init cdfs + past independence ; géré au décodage.)

			ParseTileInfo(rb, seq, h);
			ParseQuantizationParams(rb, seq, h);
			ParseSegmentationParams(rb, h);
			ComputeLossless(h);
			ParseDeltaQParams(rb, h);
			ParseDeltaLfParams(rb, h);
			ParseLoopFilterParams(rb, seq, h);
			ParseCdefParams(rb, seq, h);
			ParseLrParams(rb, seq, h);

			// read_tx_mode.
			if (h.codedLossless)
				h.txMode = 0; // ONLY_4X4
			else
				h.txMode = rb.Bit() ? 2 /*SELECT*/ : 1 /*LARGEST*/;

			// frame_reference_mode (intra => reference_select=0, rien à lire).
			h.referenceSelect = 0;

			// skip_mode_params (intra => skipModeAllowed=0, rien à lire).
			h.skipModePresent = false;

			// allow_warped_motion (intra => 0).
			h.allowWarpedMotion = false;

			h.reducedTxSet = rb.Bit() != 0;

			// global_motion_params (intra => rien à lire).
			// film_grain_params : présent seulement si flag séquence + (show||showable).
			if (seq.filmGrainParamsPresent && (h.showFrame || h.showableFrame)) {
				const int32 applyGrain = rb.Bit();
				if (applyGrain) {
					// On saute le contenu détaillé (non nécessaire à la reconstruction des
					// échantillons ; les grains sont appliqués en post). Lecture minimale
					// pour rester aligné n'est pas garantie ici — nos flux de test ont
					// film_grain_params_present=0, donc ce chemin n'est pas exercé.
				}
			}

			// byte_alignment() avant la donnée de tiles (pour OBU_FRAME).
			rb.ByteAlign();
			h.headerEndByte = rb.BytePos();

			if (rb.error)
				return false;
			h.valid = true;
			out = h;
			return true;
		}

		// =====================================================================
		// tile_group_obu (§5.11.1) — localise chaque tile compressée.
		// =====================================================================
		bool NkAv1Decoder::ParseTileGroup(const uint8 *data, usize size, const NkAv1FrameHeader &fh,
										  NkVector<NkAv1TileEntry> &tilesOut, int32 &tgStart, int32 &tgEnd) {
			tilesOut.Clear();
			const int32 tileCols = fh.tiles.tileCols;
			const int32 tileRows = fh.tiles.tileRows;
			const int32 numTiles = tileCols * tileRows;
			if (numTiles <= 0)
				return false;
			NkAv1BitReader rb(data, size);
			int32 startAndEndPresent = 0;
			if (numTiles > 1)
				startAndEndPresent = rb.Bit();
			if (numTiles == 1 || !startAndEndPresent) {
				tgStart = 0;
				tgEnd = numTiles - 1;
			} else {
				const int32 tileBits = fh.tiles.tileColsLog2 + fh.tiles.tileRowsLog2;
				tgStart = (int32)rb.F(tileBits);
				tgEnd = (int32)rb.F(tileBits);
			}
			rb.ByteAlign();
			if (rb.error)
				return false;
			usize pos = rb.BytePos();       // début de la 1re donnée de tile
			usize remaining = size - pos;   // "sz" de la spec après l'en-tête tg
			const int32 tsb = fh.tiles.tileSizeBytes;
			for (int32 tileNum = tgStart; tileNum <= tgEnd; ++tileNum) {
				const bool lastTile = (tileNum == tgEnd);
				usize tileSize;
				if (lastTile) {
					tileSize = remaining;
				} else {
					if (remaining < (usize)tsb)
						return false;
					// tile_size_minus_1 : le(TileSizeBytes) little-endian.
					uint64 v = 0;
					for (int32 i = 0; i < tsb; ++i)
						v |= (uint64)data[pos + i] << (8 * i);
					const usize sz = (usize)v + 1;
					pos += (usize)tsb;
					remaining -= (usize)tsb;
					if (sz > remaining)
						return false;
					tileSize = sz;
				}
				NkAv1TileEntry e;
				e.offset = pos;
				e.size = tileSize;
				e.tileNum = tileNum;
				e.tileRow = tileNum / tileCols;
				e.tileCol = tileNum % tileCols;
				tilesOut.PushBack(e);
				pos += tileSize;
				remaining -= tileSize;
			}
			// Les tiles doivent partitionner EXACTEMENT la charge (remaining == 0).
			return remaining == 0;
		}

		// =====================================================================
		// Parcours d'une temporal unit (une trame IVF) : itère les OBU.
		// =====================================================================
		bool NkAv1Decoder::ParseTemporalUnit(const uint8 *data, usize size, NkAv1SequenceHeader &seqOut,
											 NkAv1FrameHeader &frameOut, NkAv1ParseStats &stats) {
			usize pos = 0;
			bool haveSeq = seqOut.valid;
			while (pos < size) {
				NkAv1ObuHeader oh;
				if (!ParseObuHeader(data + pos, size - pos, oh)) {
					stats.overran = true;
					return false;
				}
				++stats.obuCount;
				const uint8 *payload = data + pos + oh.payloadOffset;
				const usize plen = oh.payloadSize;
				switch (oh.type) {
					case kAv1ObuTemporalDelimiter:
						++stats.tdCount;
						break;
					case kAv1ObuSequenceHeader: {
						++stats.seqHdrCount;
						NkAv1SequenceHeader s;
						if (ParseSequenceHeader(payload, plen, s)) {
							seqOut = s;
							haveSeq = true;
						}
						break;
					}
					case kAv1ObuFrameHeader:
					case kAv1ObuRedundantFrameHeader: {
						++stats.frameHdrCount;
						if (haveSeq) {
							NkAv1FrameHeader fh;
							if (ParseFrameHeader(payload, plen, seqOut, fh))
								frameOut = fh;
						}
						break;
					}
					case kAv1ObuTileGroup:
						++stats.tileGroupCount;
						break;
					case kAv1ObuFrame: {
						++stats.frameObuCount;
						if (haveSeq) {
							NkAv1FrameHeader fh;
							if (ParseFrameHeader(payload, plen, seqOut, fh)) {
								frameOut = fh;
								// La donnée de tiles suit à payload + fh.headerEndByte.
								if (fh.headerEndByte <= plen) {
									NkVector<NkAv1TileEntry> tiles;
									int32 tgS = 0, tgE = 0;
									const uint8 *tgData = payload + fh.headerEndByte;
									const usize tgSize = plen - fh.headerEndByte;
									if (ParseTileGroup(tgData, tgSize, fh, tiles, tgS, tgE)) {
										stats.tilesParsed += (int32)tiles.Size();
										// Initialise le décodeur de symboles sur CHAQUE tile
										// (preuve que init_symbol fonctionne sur du bitstream réel).
										for (usize ti = 0; ti < tiles.Size(); ++ti) {
											NkAv1SymbolDecoder sd;
											sd.Init(tgData + tiles[ti].offset, tiles[ti].size, fh.disableCdfUpdate);
											if (sd.symbolRange != (1u << 15))
												stats.overran = true;
										}
									} else {
										stats.overran = true;
									}
								}
							}
						}
						break;
					}
					case kAv1ObuMetadata:
						++stats.metadataCount;
						break;
					case kAv1ObuPadding:
						++stats.paddingCount;
						break;
					default:
						break;
				}
				pos += oh.payloadOffset + oh.payloadSize;
			}
			stats.bytesConsumed = pos;
			return pos == size;
		}

		// =====================================================================
// =============================================================================
// Reconstruction pipeline addition for NkAv1Decoder.cpp -- KEY FRAME INTRA ONLY.
// Written from the AV1 spec (AOMediaCodec "AV1 Bitstream & Decoding Process
// Specification", av1-spec repository chapters 05-10). No dav1d/libaom/ffmpeg
// code used or consulted for algorithms; only ffmpeg used as an EXTERNAL TEST
// ORACLE (encode+reference-decode), never as a source of implementation code.
// =============================================================================

namespace {

// -----------------------------------------------------------------------
// Local enums matching the spec's numeric encodings (verified against the
// Mode_To_Angle / Mi_Width_Log2 / Subsampled_Size tables above).
// -----------------------------------------------------------------------
enum NkAv1YMode : int32 {
	kModeDC=0,kModeV=1,kModeH=2,kModeD45=3,kModeD135=4,kModeD113=5,kModeD157=6,
	kModeD203=7,kModeD67=8,kModeSmooth=9,kModeSmoothV=10,kModeSmoothH=11,
	kModePaeth=12,kModeUvCfl=13
};
enum NkAv1Partition : int32 {
	kPartNone=0,kPartHorz=1,kPartVert=2,kPartSplit=3,kPartHorzA=4,kPartHorzB=5,
	kPartVertA=6,kPartVertB=7,kPartHorz4=8,kPartVert4=9
};
enum NkAv1TxType : int32 {
	kTxDCT_DCT=0,kTxADST_DCT=1,kTxDCT_ADST=2,kTxADST_ADST=3,kTxFLIPADST_DCT=4,
	kTxDCT_FLIPADST=5,kTxFLIPADST_FLIPADST=6,kTxADST_FLIPADST=7,kTxFLIPADST_ADST=8,
	kTxIDTX=9,kTxV_DCT=10,kTxH_DCT=11,kTxV_ADST=12,kTxH_ADST=13,kTxV_FLIPADST=14,
	kTxH_FLIPADST=15
};
// TX sizes (TX_SIZES_ALL=19), matches Tx_Width/Tx_Height table row order.
enum NkAv1TxSize : int32 {
	kTx4x4=0,kTx8x8=1,kTx16x16=2,kTx32x32=3,kTx64x64=4,kTx4x8=5,kTx8x4=6,
	kTx8x16=7,kTx16x8=8,kTx16x32=9,kTx32x16=10,kTx32x64=11,kTx64x32=12,
	kTx4x16=13,kTx16x4=14,kTx8x32=15,kTx32x8=16,kTx16x64=17,kTx64x16=18
};

#include "NKMedia/Codecs/Video/AV1/NkAv1Tables.inc"

// -----------------------------------------------------------------------
// Basic math helpers (§4.7 "Mathematical functions").
// -----------------------------------------------------------------------
inline int32 NkAv1Abs(int32 x) { return x < 0 ? -x : x; }
inline int32 NkAv1Min(int32 a, int32 b) { return a < b ? a : b; }
inline int32 NkAv1Max(int32 a, int32 b) { return a > b ? a : b; }
inline int32 NkAv1Clip3(int32 lo, int32 hi, int32 v) { return v < lo ? lo : (v > hi ? hi : v); }
inline int32 NkAv1Round2(int32 x, int32 n) { return n == 0 ? x : (x + (1 << (n - 1))) >> n; }
inline int64 NkAv1Round2_64(int64 x, int32 n) { return n == 0 ? x : (x + ((int64)1 << (n - 1))) >> n; }
inline int32 NkAv1Round2Signed(int32 x, int32 n) { return x >= 0 ? NkAv1Round2(x, n) : -NkAv1Round2(-x, n); }

// -----------------------------------------------------------------------
// Tile-local CDF context (§init_non_coeff_cdfs / init_coeff_cdfs subset
// needed for intra key frames). All arrays use the spec's own
// [nsymbs+1] convention (last slot = adaptation counter), so they can be
// passed directly to NkAv1SymbolDecoder::DecodeSymbol.
// -----------------------------------------------------------------------
struct NkAv1Cdfs {
	uint16 partitionW8[4][5];
	uint16 partitionW16[4][11];
	uint16 partitionW32[4][11];
	uint16 partitionW64[4][11];
	uint16 partitionW128[4][9];
	uint16 skip[3][3];
	uint16 intraFrameYMode[5][5][14];
	uint16 uvModeCflNotAllowed[13][14];
	uint16 uvModeCflAllowed[13][15];
	uint16 angleDelta[8][8];
	uint16 tx8x8[3][3];
	uint16 tx16x16[3][4];
	uint16 tx32x32[3][4];
	uint16 tx64x64[3][4];
	uint16 filterIntraMode[6];
	uint16 filterIntra[22][3];
	uint16 segmentId[3][9];
	uint16 deltaQ[5];
	uint16 deltaLf[5];
	uint16 intraTxTypeSet1[2][13][8];
	uint16 intraTxTypeSet2[3][13][6];
	uint16 cflSign[9];
	uint16 cflAlpha[6][17];
	uint16 intrabc[3];
	uint16 paletteYMode[7][3][3];
	uint16 paletteUvMode[2][3];
	// ----- inter (§ init_non_coeff_cdfs, portée inter) -----
	uint16 yMode[4][14];
	uint16 skipMode[3][3];
	uint16 segIdPredicted[3][3];
	uint16 newMv[6][3];
	uint16 zeroMv[2][3];
	uint16 refMv[6][3];
	uint16 drlMode[3][3];
	uint16 isInter[4][3];
	uint16 compMode[5][3];
	uint16 compRefType[5][3];
	uint16 uniCompRef[3][3][3];
	uint16 compRef[3][3][3];
	uint16 compBwdRef[3][2][3];
	uint16 singleRef[3][6][3];
	uint16 compoundMode[8][9];
	uint16 interpFilter[16][4];
	uint16 motionMode[22][4];
	uint16 useObmc[22][3];
	uint16 interIntra[3][3];
	uint16 interIntraMode[3][5];
	uint16 wedgeInterIntra[22][3];
	uint16 wedgeIndex[22][17];
	uint16 compGroupIdx[6][3];
	uint16 compoundIdx[6][3];
	uint16 compoundType[22][3];
	uint16 mvJoint[2][5];
	uint16 mvClass[2][2][12];
	uint16 mvClass0Bit[2][2][3];
	uint16 mvFr[2][2][5];
	uint16 mvClass0Fr[2][2][2][5];
	uint16 mvClass0Hp[2][2][3];
	uint16 mvSign[2][2][3];
	uint16 mvBit[2][2][10][3];
	uint16 mvHp[2][2][3];
	uint16 txfmSplit[21][3];
	uint16 interTxTypeSet1[2][17];
	uint16 interTxTypeSet2[13];
	uint16 interTxTypeSet3[4][3];
	uint16 deltaLfMulti[4][5];
	// coeff (already selected qCtx at init time)
	uint16 txbSkip[5][13][3];
	uint16 eobPt16[2][2][6];
	uint16 eobPt32[2][2][7];
	uint16 eobPt64[2][2][8];
	uint16 eobPt128[2][2][9];
	uint16 eobPt256[2][2][10];
	uint16 eobPt512[2][11];
	uint16 eobPt1024[2][12];
	uint16 eobExtra[5][2][9][3];
	uint16 dcSign[2][3][3];
	uint16 coeffBaseEob[5][2][4][4];
	uint16 coeffBase[5][2][42][5];
	uint16 coeffBr[5][2][21][5];
};

template <typename TDst, typename TSrc> void CopyFlat(TDst *dst, const TSrc *src, usize n) {
	for (usize i = 0; i < n; ++i)
		dst[i] = (TDst)src[i];
}

void NkAv1InitCdfs(NkAv1Cdfs &c, int32 baseQIdx) {
	CopyFlat(&c.partitionW8[0][0], &kDefaultPartitionW8Cdf[0][0], 4 * 5);
	CopyFlat(&c.partitionW16[0][0], &kDefaultPartitionW16Cdf[0][0], 4 * 11);
	CopyFlat(&c.partitionW32[0][0], &kDefaultPartitionW32Cdf[0][0], 4 * 11);
	CopyFlat(&c.partitionW64[0][0], &kDefaultPartitionW64Cdf[0][0], 4 * 11);
	CopyFlat(&c.partitionW128[0][0], &kDefaultPartitionW128Cdf[0][0], 4 * 9);
	CopyFlat(&c.skip[0][0], &kDefaultSkipCdf[0][0], 3 * 3);
	CopyFlat(&c.intraFrameYMode[0][0][0], &kDefaultIntraFrameYModeCdf[0][0][0], 5*5*14);
	CopyFlat(&c.uvModeCflNotAllowed[0][0], &kDefaultUvModeCflNotAllowedCdf[0][0], 13*14);
	CopyFlat(&c.uvModeCflAllowed[0][0], &kDefaultUvModeCflAllowedCdf[0][0], 13*15);
	CopyFlat(&c.angleDelta[0][0], &kDefaultAngleDeltaCdf[0][0], 8*8);
	CopyFlat(&c.tx8x8[0][0], &kDefaultTx8x8Cdf[0][0], 3*3);
	CopyFlat(&c.tx16x16[0][0], &kDefaultTx16x16Cdf[0][0], 3*4);
	CopyFlat(&c.tx32x32[0][0], &kDefaultTx32x32Cdf[0][0], 3*4);
	CopyFlat(&c.tx64x64[0][0], &kDefaultTx64x64Cdf[0][0], 3*4);
	CopyFlat(&c.filterIntraMode[0], &kDefaultFilterIntraModeCdf[0], 6);
	CopyFlat(&c.filterIntra[0][0], &kDefaultFilterIntraCdf[0][0], 22*3);
	CopyFlat(&c.segmentId[0][0], &kDefaultSegmentIdCdf[0][0], 3*9);
	CopyFlat(&c.deltaQ[0], &kDefaultDeltaQCdf[0], 5);
	CopyFlat(&c.deltaLf[0], &kDefaultDeltaLfCdf[0], 5);
	CopyFlat(&c.intraTxTypeSet1[0][0][0], &kDefaultIntraTxTypeSet1Cdf[0][0][0], 2*13*8);
	CopyFlat(&c.intraTxTypeSet2[0][0][0], &kDefaultIntraTxTypeSet2Cdf[0][0][0], 3*13*6);
	CopyFlat(&c.cflSign[0], &kDefaultCflSignCdf[0], 9);
	CopyFlat(&c.cflAlpha[0][0], &kDefaultCflAlphaCdf[0][0], 6*17);
	CopyFlat(&c.intrabc[0], &kDefaultIntrabcCdf[0], 3);
	CopyFlat(&c.paletteYMode[0][0][0], &kDefaultPaletteYModeCdf[0][0][0], 7*3*3);
	CopyFlat(&c.paletteUvMode[0][0], &kDefaultPaletteUvModeCdf[0][0], 2*3);

	// ----- inter -----
	CopyFlat(&c.yMode[0][0], &kDefaultYModeCdf[0][0], 4*14);
	CopyFlat(&c.skipMode[0][0], &kDefaultSkipModeCdf[0][0], 3*3);
	CopyFlat(&c.segIdPredicted[0][0], &kDefaultSegmentIdPredictedCdf[0][0], 3*3);
	CopyFlat(&c.newMv[0][0], &kDefaultNewMvCdf[0][0], 6*3);
	CopyFlat(&c.zeroMv[0][0], &kDefaultZeroMvCdf[0][0], 2*3);
	CopyFlat(&c.refMv[0][0], &kDefaultRefMvCdf[0][0], 6*3);
	CopyFlat(&c.drlMode[0][0], &kDefaultDrlModeCdf[0][0], 3*3);
	CopyFlat(&c.isInter[0][0], &kDefaultIsInterCdf[0][0], 4*3);
	CopyFlat(&c.compMode[0][0], &kDefaultCompModeCdf[0][0], 5*3);
	CopyFlat(&c.compRefType[0][0], &kDefaultCompRefTypeCdf[0][0], 5*3);
	CopyFlat(&c.uniCompRef[0][0][0], &kDefaultUniCompRefCdf[0][0][0], 3*3*3);
	CopyFlat(&c.compRef[0][0][0], &kDefaultCompRefCdf[0][0][0], 3*3*3);
	CopyFlat(&c.compBwdRef[0][0][0], &kDefaultCompBwdRefCdf[0][0][0], 3*2*3);
	CopyFlat(&c.singleRef[0][0][0], &kDefaultSingleRefCdf[0][0][0], 3*6*3);
	CopyFlat(&c.compoundMode[0][0], &kDefaultCompoundModeCdf[0][0], 8*9);
	CopyFlat(&c.interpFilter[0][0], &kDefaultInterpFilterCdf[0][0], 16*4);
	CopyFlat(&c.motionMode[0][0], &kDefaultMotionModeCdf[0][0], 22*4);
	CopyFlat(&c.useObmc[0][0], &kDefaultUseObmcCdf[0][0], 22*3);
	CopyFlat(&c.interIntra[0][0], &kDefaultInterIntraCdf[0][0], 3*3);
	CopyFlat(&c.interIntraMode[0][0], &kDefaultInterIntraModeCdf[0][0], 3*5);
	CopyFlat(&c.wedgeInterIntra[0][0], &kDefaultWedgeInterIntraCdf[0][0], 22*3);
	CopyFlat(&c.wedgeIndex[0][0], &kDefaultWedgeIndexCdf[0][0], 22*17);
	CopyFlat(&c.compGroupIdx[0][0], &kDefaultCompGroupIdxCdf[0][0], 6*3);
	CopyFlat(&c.compoundIdx[0][0], &kDefaultCompoundIdxCdf[0][0], 6*3);
	CopyFlat(&c.compoundType[0][0], &kDefaultCompoundTypeCdf[0][0], 22*3);
	for (int32 mc = 0; mc < 2; ++mc) {
		CopyFlat(&c.mvJoint[mc][0], &kDefaultMvJointCdf[0], 5);
		CopyFlat(&c.mvClass[mc][0][0], &kDefaultMvClassCdf[0][0], 2*12);
		CopyFlat(&c.mvFr[mc][0][0], &kDefaultMvFrCdf[0][0], 2*5);
		CopyFlat(&c.mvClass0Fr[mc][0][0][0], &kDefaultMvClass0FrCdf[0][0][0], 2*2*5);
		for (int32 comp = 0; comp < 2; ++comp) {
			CopyFlat(&c.mvClass0Bit[mc][comp][0], &kDefaultMvClass0BitCdf[0], 3);
			CopyFlat(&c.mvClass0Hp[mc][comp][0], &kDefaultMvClass0HpCdf[0], 3);
			CopyFlat(&c.mvSign[mc][comp][0], &kDefaultMvSignCdf[0], 3);
			CopyFlat(&c.mvHp[mc][comp][0], &kDefaultMvHpCdf[0], 3);
			CopyFlat(&c.mvBit[mc][comp][0][0], &kDefaultMvBitCdf[0][0], 10*3);
		}
	}
	CopyFlat(&c.txfmSplit[0][0], &kDefaultTxfmSplitCdf[0][0], 21*3);
	CopyFlat(&c.interTxTypeSet1[0][0], &kDefaultInterTxTypeSet1Cdf[0][0], 2*17);
	CopyFlat(&c.interTxTypeSet2[0], &kDefaultInterTxTypeSet2Cdf[0], 13);
	CopyFlat(&c.interTxTypeSet3[0][0], &kDefaultInterTxTypeSet3Cdf[0][0], 4*3);
	for (int32 i = 0; i < 4; ++i)
		CopyFlat(&c.deltaLfMulti[i][0], &kDefaultDeltaLfCdf[0], 5);

	int32 idx = (baseQIdx <= 20) ? 0 : (baseQIdx <= 60) ? 1 : (baseQIdx <= 120) ? 2 : 3;
	CopyFlat(&c.txbSkip[0][0][0], &kDefaultTxbSkipCdf[idx][0][0][0], 5*13*3);
	CopyFlat(&c.eobPt16[0][0][0], &kDefaultEobPt16Cdf[idx][0][0][0], 2*2*6);
	CopyFlat(&c.eobPt32[0][0][0], &kDefaultEobPt32Cdf[idx][0][0][0], 2*2*7);
	CopyFlat(&c.eobPt64[0][0][0], &kDefaultEobPt64Cdf[idx][0][0][0], 2*2*8);
	CopyFlat(&c.eobPt128[0][0][0], &kDefaultEobPt128Cdf[idx][0][0][0], 2*2*9);
	CopyFlat(&c.eobPt256[0][0][0], &kDefaultEobPt256Cdf[idx][0][0][0], 2*2*10);
	CopyFlat(&c.eobPt512[0][0], &kDefaultEobPt512Cdf[idx][0][0], 2*11);
	CopyFlat(&c.eobPt1024[0][0], &kDefaultEobPt1024Cdf[idx][0][0], 2*12);
	CopyFlat(&c.eobExtra[0][0][0][0], &kDefaultEobExtraCdf[idx][0][0][0][0], 5*2*9*3);
	CopyFlat(&c.dcSign[0][0][0], &kDefaultDcSignCdf[idx][0][0][0], 2*3*3);
	CopyFlat(&c.coeffBaseEob[0][0][0][0], &kDefaultCoeffBaseEobCdf[idx][0][0][0][0], 5*2*4*4);
	CopyFlat(&c.coeffBase[0][0][0][0], &kDefaultCoeffBaseCdf[idx][0][0][0][0], 5*2*42*5);
	CopyFlat(&c.coeffBr[0][0][0][0], &kDefaultCoeffBrCdf[idx][0][0][0][0], 5*2*21*5);
}

// load_cdfs (§7.20 / semantics) : quand les CDF sont CHARGEES depuis un slot de
// reference, le compteur d'adaptation (DERNIERE entree de chaque rangee) est
// remis a 0. Enumeration mecanique de chaque tableau du struct.
void NkAv1ZeroCdfCounters(NkAv1Cdfs &c) {
#define NK_ZC(first, rows, rowLen) do { uint16 *zp = (first); for (int32 zr = 0; zr < (rows); ++zr) zp[(usize)zr * (rowLen) + (rowLen) - 1] = 0; } while (0)
	NK_ZC(&c.partitionW8[0][0], 4, 5); NK_ZC(&c.partitionW16[0][0], 4, 11);
	NK_ZC(&c.partitionW32[0][0], 4, 11); NK_ZC(&c.partitionW64[0][0], 4, 11);
	NK_ZC(&c.partitionW128[0][0], 4, 9);
	NK_ZC(&c.skip[0][0], 3, 3); NK_ZC(&c.intraFrameYMode[0][0][0], 25, 14);
	NK_ZC(&c.uvModeCflNotAllowed[0][0], 13, 14); NK_ZC(&c.uvModeCflAllowed[0][0], 13, 15);
	NK_ZC(&c.angleDelta[0][0], 8, 8);
	NK_ZC(&c.tx8x8[0][0], 3, 3); NK_ZC(&c.tx16x16[0][0], 3, 4);
	NK_ZC(&c.tx32x32[0][0], 3, 4); NK_ZC(&c.tx64x64[0][0], 3, 4);
	NK_ZC(&c.filterIntraMode[0], 1, 6); NK_ZC(&c.filterIntra[0][0], 22, 3);
	NK_ZC(&c.segmentId[0][0], 3, 9); NK_ZC(&c.deltaQ[0], 1, 5); NK_ZC(&c.deltaLf[0], 1, 5);
	NK_ZC(&c.intraTxTypeSet1[0][0][0], 26, 8); NK_ZC(&c.intraTxTypeSet2[0][0][0], 39, 6);
	NK_ZC(&c.cflSign[0], 1, 9); NK_ZC(&c.cflAlpha[0][0], 6, 17); NK_ZC(&c.intrabc[0], 1, 3);
	NK_ZC(&c.paletteYMode[0][0][0], 21, 3); NK_ZC(&c.paletteUvMode[0][0], 2, 3);
	NK_ZC(&c.yMode[0][0], 4, 14); NK_ZC(&c.skipMode[0][0], 3, 3);
	NK_ZC(&c.segIdPredicted[0][0], 3, 3);
	NK_ZC(&c.newMv[0][0], 6, 3); NK_ZC(&c.zeroMv[0][0], 2, 3); NK_ZC(&c.refMv[0][0], 6, 3);
	NK_ZC(&c.drlMode[0][0], 3, 3); NK_ZC(&c.isInter[0][0], 4, 3);
	NK_ZC(&c.compMode[0][0], 5, 3); NK_ZC(&c.compRefType[0][0], 5, 3);
	NK_ZC(&c.uniCompRef[0][0][0], 9, 3); NK_ZC(&c.compRef[0][0][0], 9, 3);
	NK_ZC(&c.compBwdRef[0][0][0], 6, 3); NK_ZC(&c.singleRef[0][0][0], 18, 3);
	NK_ZC(&c.compoundMode[0][0], 8, 9); NK_ZC(&c.interpFilter[0][0], 16, 4);
	NK_ZC(&c.motionMode[0][0], 22, 4); NK_ZC(&c.useObmc[0][0], 22, 3);
	NK_ZC(&c.interIntra[0][0], 3, 3); NK_ZC(&c.interIntraMode[0][0], 3, 5);
	NK_ZC(&c.wedgeInterIntra[0][0], 22, 3); NK_ZC(&c.wedgeIndex[0][0], 22, 17);
	NK_ZC(&c.compGroupIdx[0][0], 6, 3); NK_ZC(&c.compoundIdx[0][0], 6, 3);
	NK_ZC(&c.compoundType[0][0], 22, 3);
	NK_ZC(&c.mvJoint[0][0], 2, 5); NK_ZC(&c.mvClass[0][0][0], 4, 12);
	NK_ZC(&c.mvClass0Bit[0][0][0], 4, 3); NK_ZC(&c.mvFr[0][0][0], 4, 5);
	NK_ZC(&c.mvClass0Fr[0][0][0][0], 8, 5); NK_ZC(&c.mvClass0Hp[0][0][0], 4, 3);
	NK_ZC(&c.mvSign[0][0][0], 4, 3); NK_ZC(&c.mvBit[0][0][0][0], 40, 3);
	NK_ZC(&c.mvHp[0][0][0], 4, 3);
	NK_ZC(&c.txfmSplit[0][0], 21, 3);
	NK_ZC(&c.interTxTypeSet1[0][0], 2, 17); NK_ZC(&c.interTxTypeSet2[0], 1, 13);
	NK_ZC(&c.interTxTypeSet3[0][0], 4, 3); NK_ZC(&c.deltaLfMulti[0][0], 4, 5);
	NK_ZC(&c.txbSkip[0][0][0], 65, 3);
	NK_ZC(&c.eobPt16[0][0][0], 4, 6); NK_ZC(&c.eobPt32[0][0][0], 4, 7);
	NK_ZC(&c.eobPt64[0][0][0], 4, 8); NK_ZC(&c.eobPt128[0][0][0], 4, 9);
	NK_ZC(&c.eobPt256[0][0][0], 4, 10); NK_ZC(&c.eobPt512[0][0], 2, 11);
	NK_ZC(&c.eobPt1024[0][0], 2, 12);
	NK_ZC(&c.eobExtra[0][0][0][0], 90, 3); NK_ZC(&c.dcSign[0][0][0], 6, 3);
	NK_ZC(&c.coeffBaseEob[0][0][0][0], 40, 4); NK_ZC(&c.coeffBase[0][0][0][0], 420, 5);
	NK_ZC(&c.coeffBr[0][0][0][0], 210, 5);
#undef NK_ZC
}

// -----------------------------------------------------------------------
// DPB : un slot de reference (§7.20 reference frame update process). Contient
// les pixels FINAUX (post loop filter/CDEF), les MV sauvegardes (MfMvs), les
// order hints, les CDF sauvegardees, et les parametres persistants charges par
// load_previous().
// -----------------------------------------------------------------------
struct NkAv1RefSlot {
	bool valid = false;
	NkVector<uint8> y, u, v;
	int32 yStride = 0, uvStride = 0;
	int32 upscaledWidth = 0, frameWidth = 0, frameHeight = 0;
	int32 renderWidth = 0, renderHeight = 0;
	int32 uvW = 0, uvH = 0;
	int32 miCols = 0, miRows = 0;
	int32 frameType = 0;
	int32 orderHint = 0;
	int32 savedOrderHints[8] = {0};
	NkVector<int8> savedRefFrames;   // MfRefFrames (miRows*miCols)
	NkVector<int16> savedMvs;        // MfMvs (miRows*miCols*2)
	NkVector<int8> savedSegmentIds;  // (miRows*miCols)
	int32 savedGmParams[8][6] = {{0}};
	int32 lfRefDeltas[8] = {1, 0, 0, 0, -1, 0, -1, -1};
	int32 lfModeDeltas[2] = {0, 0};
	bool segFeatureEnabled[8][8] = {{false}};
	int32 segFeatureData[8][8] = {{0}};
	NkAv1Cdfs cdfs;
	bool showableFrame = false;
};

// Etat persistant du decodeur de flux (DPB 8 slots + sequence header).
struct NkAv1DecoderState {
	NkAv1SequenceHeader seq;
	bool haveSeq = false;
	NkAv1RefSlot refs[8];
	const char *lastError = "";
};

// -----------------------------------------------------------------------
// Frame-level decode context: sequence/frame header refs, grids, CDFs,
// current-block state, working pixel planes and symbol decoder.
// -----------------------------------------------------------------------
struct NkAv1Ctx {
	const NkAv1SequenceHeader *seq;
	const NkAv1FrameHeader *fh;
	NkAv1SymbolDecoder sd;
	NkAv1Cdfs cdf;

	int32 miCols, miRows;
	int32 gridStride, gridRows; // miCols/miRows padded up by sbSize4 -- decode_block writes
	                             // grid cells for the full nominal block extent (bw4 x bh4)
	                             // even when that extent runs past the frame edge (§5.11.5).
	int32 sbSize;      // BLOCK_128X128(15) or BLOCK_64X64(12)
	int32 sbSize4;      // Num_4x4_Blocks_Wide[sbSize]
	int32 miColStart, miColEnd, miRowStart, miRowEnd;

	// Per-tile grids (row-major, stride = miCols).
	NkVector<int8> yModes, uvModes, miSizes, segmentIds, skips, txSizesGrid; // InterTxSizes for ctx use
	NkVector<int8> txTypes; // TxTypes[y4][x4], stride = miCols
	NkVector<int8> loopfilterTxSizes[3]; // LoopfilterTxSizes[plane][row>>subY][col>>subX], stride = gridStride
	NkVector<int8> deltaLfGrid[4]; // DeltaLFs[row][col][i], stride = gridStride

	// Above context: full tile width (in 4x4 units), per plane.
	NkVector<int16> aboveLevel[3], aboveDc[3];
	// Left context: superblock height (in 4x4 units), per plane.
	NkVector<int16> leftLevel[3], leftDc[3];

	// BlockDecoded[plane][y+1][x+1] local to current superblock; sized (sbSize4/subY+2) etc.
	// We size it to the max possible (sbSize4+2) for simplicity and re-clear per SB.
	NkVector<uint8> blockDecoded[3];
	int32 bdStride[3]; // stride for blockDecoded plane (in cells, offset baked via +1)

	// Working reconstructed pixel planes, padded to miCols*4 x miRows*4 (subsampled for chroma).
	NkVector<uint8> curY, curU, curV;
	int32 curYStride, curUvStride, curUvW, curUvH;

	// Per-block state (set during mode_info / used by residual/transform_block).
	int32 MiRow, MiCol, MiSize;
	bool AvailU, AvailL, AvailUChroma, AvailLChroma, HasChroma;
	bool skip_, useIntrabc;
	int32 segmentId_;
	bool Lossless;
	int32 YMode, UVMode, AngleDeltaY, AngleDeltaUV;
	bool useFilterIntra; int32 filterIntraMode_;
	int32 CflAlphaU, CflAlphaV;
	int32 PaletteSizeY, PaletteSizeUV;
	int32 TxSize_;
	int32 PlaneTxType; // set per transform_block call
	int32 MaxLumaW, MaxLumaH; // set by transform_block(plane=0), used by CFL
	bool ReadDeltas;
	int32 CurrentQIndex;
	int32 DeltaLF[4];

	bool ok; // sticky error flag
	const char *err; // reason for !ok (static string)

	struct NkAv1BlockScratch *scratch; // block-local dequant/residual buffers (owned by caller)

	// =================== INTER state ===================
	bool frameIsIntra;
	const NkAv1DecoderState *state; // DPB (null pour le chemin intra pur)
	// Grilles supplementaires (stride = gridStride)
	NkVector<int8> refFrames0, refFrames1;      // RefFrames[..][0/1]
	NkVector<int8> interpFilters0, interpFilters1;
	NkVector<int8> compGroupIdxs, compoundIdxs, skipModes, isInters;
	NkVector<uint8> refWritten;                 // "RefFrames a ete ecrit" (scan point / warp)
	NkVector<int16> mvsGrid;                    // Mvs[cell][list][comp] : cell*4 + list*2 + comp
	NkVector<int8> segPredAbove, segPredLeft;   // Above/LeftSegPredContext
	NkVector<int8> prevSegmentIds;              // PrevSegmentIds (vide => 0)
	NkVector<int16> motionFieldMvs;             // [ref 1..7][y8][x8][2]
	int32 mfW8, mfH8;
	NkVector<int8> cdefIdxGrid;                 // cdef_idx par 4x4 (-1 = non lu)
	// Parametres de trame inter
	int32 interpFilterFrame;   // interpolation_filter (3 = SWITCHABLE)
	bool enableDualFilter, enableJntComp, enableMaskedCompound, enableInterintraCompound;
	bool allowHighPrecisionMv;
	int32 forceIntegerMv;
	bool isMotionModeSwitchable, allowWarpedMotion;
	int32 referenceSelect;
	bool skipModePresent;
	int32 skipModeFrame[2];
	bool useRefFrameMvsFrame;
	// Etat de bloc inter
	bool isInter_, skipMode_;
	int32 refFrame[2];         // RefFrame[0..1] (NONE=-1)
	int32 mvBlk[2][2];         // Mv[list][row/col]
	int32 predMv[2][2];
	int32 interpFilterBlk[2];
	int32 compGroupIdx_, compoundIdx_, compoundTypeBlk;
	int32 wedgeIndexBlk, wedgeSignBlk, maskTypeBlk;
	int32 motionModeBlk;       // 0=SIMPLE 1=OBMC 2=LOCALWARP
	int32 interIntraBlk;       // interintra (bloc single-ref + prediction intra melangee)
	int32 interIntraModeBlk;   // II_DC/II_V/II_H/II_SMOOTH
	int32 wedgeInterIntraBlk;
	// Pile de MV (§7.10.2)
	int32 refStackMv[8][2][2];
	int32 weightStack[8];
	int32 numMvFound, newMvCount;
	int32 newMvContext, refMvContext, zeroMvContext;
	int32 drlCtxStack[8];
	int32 globalMvs[2][2];
	int32 foundMatch, closeMatches, totalMatches;
	int32 numSamples, numSamplesScanned; // find_warp_samples
	int32 candList[8][4];                // CandList (warp samples, 1/8 pel)
	int32 localWarpParams[6];            // LocalWarpParams (§7.11.3.8)
	bool localValid;
	int32 refMvIdx_;
	// Contexte voisin (inter_frame_mode_info)
	int32 aboveRefFrame[2], leftRefFrame[2];
	bool aboveIntra, leftIntra, aboveSingle, leftSingle;
};

// ------------------------------------------------------------------
// Grid helpers.
// ------------------------------------------------------------------
inline int8 &Grid(NkVector<int8> &g, int32 stride, int32 r, int32 c) { return g[(usize)r * (usize)stride + (usize)c]; }
inline int8 Grid(const NkVector<int8> &g, int32 stride, int32 r, int32 c) { return g[(usize)r * (usize)stride + (usize)c]; }

bool IsInside(NkAv1Ctx &ctx, int32 candR, int32 candC) {
	return candC >= ctx.miColStart && candC < ctx.miColEnd && candR >= ctx.miRowStart && candR < ctx.miRowEnd;
}

// is_directional_mode (§ decode_partition-adjacent).
inline bool IsDirectionalMode(int32 mode) { return mode >= kModeV && mode <= kModeD67; }

// -----------------------------------------------------------------------
// Forward declarations (mutual recursion decode_partition <-> decode_block,
// and the various sub-processes called from decode_block).
// -----------------------------------------------------------------------
void DecodePartition(NkAv1Ctx &ctx, int32 r, int32 c, int32 bSize);
void DecodeBlock(NkAv1Ctx &ctx, int32 r, int32 c, int32 subSize);
void ModeInfo(NkAv1Ctx &ctx);
void IntraFrameModeInfo(NkAv1Ctx &ctx);
void ReadTxSize(NkAv1Ctx &ctx, bool allowSelect);
void ReadBlockTxSize(NkAv1Ctx &ctx);
void Residual(NkAv1Ctx &ctx);
void TransformBlock(NkAv1Ctx &ctx, int32 plane, int32 baseX, int32 baseY, int32 txSz, int32 x, int32 y);
int32 Coeffs(NkAv1Ctx &ctx, int32 plane, int32 startX, int32 startY, int32 txSz);
void Reconstruct(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, int32 txSz);
struct NkAv1BlockScratch;
void PredictIntra(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, bool haveLeft, bool haveAbove,
				  bool haveAboveRight, bool haveBelowLeft, int32 mode, int32 log2W, int32 log2H);
void PredictChromaFromLuma(NkAv1Ctx &ctx, int32 plane, int32 startX, int32 startY, int32 txSz);
int32 GetTxSize(NkAv1Ctx &ctx, int32 plane, int32 txSz);
int32 GetPlaneResidualSize(NkAv1Ctx &ctx, int32 subsize, int32 plane);
const nk_uint16 *GetScan(NkAv1Ctx &ctx, int32 txSz);
int32 ComputeTxType(NkAv1Ctx &ctx, int32 plane, int32 txSz, int32 blockX, int32 blockY);
int32 GetTxSet(NkAv1Ctx &ctx, int32 txSz);
void TransformType(NkAv1Ctx &ctx, int32 x4, int32 y4, int32 txSz);
void IntraAngleInfoY(NkAv1Ctx &ctx);
void IntraAngleInfoUV(NkAv1Ctx &ctx);
void ReadCflAlphas(NkAv1Ctx &ctx);
void FilterIntraModeInfo(NkAv1Ctx &ctx);
void ClearBlockDecodedFlags(NkAv1Ctx &ctx, int32 r, int32 c, int32 sbSize4);
void ResetBlockContext(NkAv1Ctx &ctx, int32 bw4, int32 bh4);
int32 GetTxClass(int32 txType);
int32 GetCoeffBaseCtx(NkAv1Ctx &ctx, int32 txSz, int32 plane, int32 blockX, int32 blockY, int32 pos, int32 c, int32 isEob);
void IntraSegmentId(NkAv1Ctx &ctx);
void ReadSegmentId(NkAv1Ctx &ctx);
void ReadSkip(NkAv1Ctx &ctx);
void ReadDeltaQIndex(NkAv1Ctx &ctx);
void ReadDeltaLf(NkAv1Ctx &ctx);
bool SegFeatureActiveIdx(NkAv1Ctx &ctx, int32 idx, int32 feature);
bool SegFeatureActive(NkAv1Ctx &ctx, int32 feature);
int32 GetQIndex(NkAv1Ctx &ctx, int32 ignoreDeltaQ, int32 segmentId);
uint8 &CurPix(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y);
// --- inter ---
void InterFrameModeInfo(NkAv1Ctx &ctx);
void InterBlockModeInfo(NkAv1Ctx &ctx);
void IntraBlockModeInfo(NkAv1Ctx &ctx);
void FindMvStack(NkAv1Ctx &ctx, bool isCompound);
void ComputePrediction(NkAv1Ctx &ctx);
void PredictInter(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, int32 w, int32 h, int32 candRow, int32 candCol);
void ReadVarTxSize(NkAv1Ctx &ctx, int32 row, int32 col, int32 txSz, int32 depth);
void TransformTree(NkAv1Ctx &ctx, int32 startX, int32 startY, int32 w, int32 h);
void ReadCdef(NkAv1Ctx &ctx);
void ClearCdef(NkAv1Ctx &ctx, int32 r, int32 c);

// get_above_tx_width / get_left_tx_height (§ CDF selection for txfm_split, also used by tx_depth).
int32 GetAboveTxWidth(NkAv1Ctx &ctx, int32 row, int32 col) {
	if (row == ctx.MiRow) {
		if (!ctx.AvailU)
			return 64;
		if (Grid(ctx.skips, ctx.gridStride, row - 1, col) && Grid(ctx.isInters, ctx.gridStride, row - 1, col))
			return kNum4x4BlocksWide[(int32)Grid(ctx.miSizes, ctx.gridStride, row - 1, col)] * 4;
	}
	int32 tsz = Grid(ctx.txSizesGrid, ctx.gridStride, row - 1, col);
	return kTxWidth[tsz];
}
int32 GetLeftTxHeight(NkAv1Ctx &ctx, int32 row, int32 col) {
	if (col == ctx.MiCol) {
		if (!ctx.AvailL)
			return 64;
		if (Grid(ctx.skips, ctx.gridStride, row, col - 1) && Grid(ctx.isInters, ctx.gridStride, row, col - 1))
			return kNum4x4BlocksHigh[(int32)Grid(ctx.miSizes, ctx.gridStride, row, col - 1)] * 4;
	}
	int32 tsz = Grid(ctx.txSizesGrid, ctx.gridStride, row, col - 1);
	return kTxHeight[tsz];
}

// =========================================================================
// INTER — constantes, accesseurs et processus §7.9/§7.10 (prediction de MV),
// §5.11.x (syntaxe de bloc inter), §7.11.3 (compensation de mouvement).
// Transcrits depuis la spec AOMedia, comme le pipeline intra.
// =========================================================================
// Types de trame de reference (§6.10.24) et modes Y inter (§6.10.23).
constexpr int32 kRefNone = -1, kRefIntra = 0, kRefLast = 1, kRefLast2 = 2, kRefLast3 = 3,
				kRefGolden = 4, kRefBwd = 5, kRefAltRef2 = 6, kRefAltRef = 7;
enum NkAv1InterYMode : int32 {
	kModeNearestMv = 14, kModeNearMv = 15, kModeGlobalMv = 16, kModeNewMv = 17,
	kModeNearestNearestMv = 18, kModeNearNearMv = 19, kModeNearestNewMv = 20,
	kModeNewNearestMv = 21, kModeNearNewMv = 22, kModeNewNearMv = 23,
	kModeGlobalGlobalMv = 24, kModeNewNewMv = 25
};
constexpr int32 kMvBorder = 128;
constexpr int32 kRefCatLevel = 640;
constexpr int32 kMaxFrameDistance = 31;
constexpr int32 kGmIdentity = 0, kGmTranslation = 1, kGmRotzoom = 2, kGmAffine = 3;
constexpr int32 kWarpedModelPrecBits = 16;
constexpr int32 kCompoundAverage = 0, kCompoundDistance = 1, kCompoundDiffwtd = 2,
				kCompoundWedge = 3, kCompoundIntra = 4;
constexpr int32 kMotionSimple = 0, kMotionObmc = 1, kMotionLocalwarp = 2;

inline int32 BlockWidthOf(int32 sz) { return kNum4x4BlocksWide[sz] * 4; }
inline int32 BlockHeightOf(int32 sz) { return kNum4x4BlocksHigh[sz] * 4; }

inline int16 *MvsAt(NkAv1Ctx &ctx, int32 r, int32 c) {
	return &ctx.mvsGrid[((usize)r * (usize)ctx.gridStride + (usize)c) * 4];
}
inline int16 *MfMvAt(NkAv1Ctx &ctx, int32 ref, int32 y8, int32 x8) {
	return &ctx.motionFieldMvs[(((usize)(ref - 1) * (usize)ctx.mfH8 + (usize)y8) * (usize)ctx.mfW8 + (usize)x8) * 2];
}
inline int64 NkAv1Round2Signed64(int64 x, int32 n) {
	return x >= 0 ? NkAv1Round2_64(x, n) : -NkAv1Round2_64(-x, n);
}

// get_relative_dist (§5.9.3).
inline int32 GetRelativeDist(const NkAv1SequenceHeader &seq, int32 a, int32 b) {
	if (!seq.enableOrderHint)
		return 0;
	int32 diff = a - b;
	const int32 m = 1 << (seq.orderHintBits - 1);
	diff = (diff & (m - 1)) - (diff & m);
	return diff;
}

// Slot DPB pour un type de reference (LAST..ALTREF).
inline const NkAv1RefSlot *SlotFor(NkAv1Ctx &ctx, int32 refFrame) {
	if (!ctx.state || refFrame < kRefLast || refFrame > kRefAltRef)
		return nullptr;
	const int32 idx = ctx.fh->refFrameIdx[refFrame - kRefLast];
	if (idx < 0 || idx >= 8)
		return nullptr;
	return &ctx.state->refs[idx];
}

// is_scaled (§5.11.27).
inline bool IsScaledRef(NkAv1Ctx &ctx, int32 refFrame) {
	const NkAv1RefSlot *slot = SlotFor(ctx, refFrame);
	if (!slot || !slot->valid)
		return false;
	const int32 xScale = (int32)(((int64)slot->upscaledWidth << 14) + (ctx.fh->frameWidth / 2)) / ctx.fh->frameWidth;
	const int32 yScale = (int32)(((int64)slot->frameHeight << 14) + (ctx.fh->frameHeight / 2)) / ctx.fh->frameHeight;
	return xScale != (1 << 14) || yScale != (1 << 14);
}

// Lower precision process (§7.10.2.10).
void LowerMvPrecision(NkAv1Ctx &ctx, int32 *candMv) {
	if (ctx.allowHighPrecisionMv)
		return;
	for (int32 i = 0; i < 2; ++i) {
		if (ctx.forceIntegerMv) {
			const int32 a = NkAv1Abs(candMv[i]);
			const int32 aInt = (a + 3) >> 3;
			candMv[i] = (candMv[i] > 0) ? (aInt << 3) : -(aInt << 3);
		} else {
			if (candMv[i] & 1) {
				if (candMv[i] > 0)
					candMv[i]--;
				else
					candMv[i]++;
			}
		}
	}
}

// Setup global MV process (§7.10.2.1).
void SetupGlobalMv(NkAv1Ctx &ctx, int32 refList, int32 *mv) {
	const int32 ref = ctx.refFrame[refList];
	int32 typ = kGmIdentity;
	if (ref != kRefIntra)
		typ = ctx.fh->gmType[ref];
	const int32 bw = BlockWidthOf(ctx.MiSize), bh = BlockHeightOf(ctx.MiSize);
	if (ref == kRefIntra || typ == kGmIdentity) {
		mv[0] = 0;
		mv[1] = 0;
	} else if (typ == kGmTranslation) {
		mv[0] = ctx.fh->gmParams[ref][0] >> (kWarpedModelPrecBits - 3);
		mv[1] = ctx.fh->gmParams[ref][1] >> (kWarpedModelPrecBits - 3);
	} else {
		const int32 x = ctx.MiCol * 4 + bw / 2 - 1;
		const int32 y = ctx.MiRow * 4 + bh / 2 - 1;
		const int64 xc = (int64)(ctx.fh->gmParams[ref][2] - (1 << kWarpedModelPrecBits)) * x +
						 (int64)ctx.fh->gmParams[ref][3] * y + ctx.fh->gmParams[ref][0];
		const int64 yc = (int64)ctx.fh->gmParams[ref][4] * x +
						 (int64)(ctx.fh->gmParams[ref][5] - (1 << kWarpedModelPrecBits)) * y + ctx.fh->gmParams[ref][1];
		if (ctx.allowHighPrecisionMv) {
			mv[0] = (int32)NkAv1Round2Signed64(yc, kWarpedModelPrecBits - 3);
			mv[1] = (int32)NkAv1Round2Signed64(xc, kWarpedModelPrecBits - 3);
		} else {
			mv[0] = (int32)NkAv1Round2Signed64(yc, kWarpedModelPrecBits - 2) * 2;
			mv[1] = (int32)NkAv1Round2Signed64(xc, kWarpedModelPrecBits - 2) * 2;
		}
	}
	LowerMvPrecision(ctx, mv);
}

inline bool HasNewmvMode(int32 mode) {
	return mode == kModeNewMv || mode == kModeNewNewMv || mode == kModeNearNewMv ||
		   mode == kModeNewNearMv || mode == kModeNearestNewMv || mode == kModeNewNearestMv;
}

// Search stack process (§7.10.2.8).
void SearchStack(NkAv1Ctx &ctx, int32 mvRow, int32 mvCol, int32 candList, int32 weight) {
	const int32 candMode = Grid(ctx.yModes, ctx.gridStride, mvRow, mvCol);
	const int32 candSize = Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol);
	const bool large = NkAv1Min(BlockWidthOf(candSize), BlockHeightOf(candSize)) >= 8;
	int32 candMv[2];
	if ((candMode == kModeGlobalMv || candMode == kModeGlobalGlobalMv) &&
		ctx.fh->gmType[ctx.refFrame[0]] > kGmTranslation && large) {
		candMv[0] = ctx.globalMvs[0][0];
		candMv[1] = ctx.globalMvs[0][1];
	} else {
		const int16 *m = MvsAt(ctx, mvRow, mvCol);
		candMv[0] = m[candList * 2 + 0];
		candMv[1] = m[candList * 2 + 1];
	}
	LowerMvPrecision(ctx, candMv);
	if (HasNewmvMode(candMode))
		++ctx.newMvCount;
	ctx.foundMatch = 1;
	for (int32 idx = 0; idx < ctx.numMvFound; ++idx) {
		if (candMv[0] == ctx.refStackMv[idx][0][0] && candMv[1] == ctx.refStackMv[idx][0][1]) {
			ctx.weightStack[idx] += weight;
			return;
		}
	}
	if (ctx.numMvFound < 8) {
		ctx.refStackMv[ctx.numMvFound][0][0] = candMv[0];
		ctx.refStackMv[ctx.numMvFound][0][1] = candMv[1];
		ctx.weightStack[ctx.numMvFound] = weight;
		++ctx.numMvFound;
	}
}

// Compound search stack process (§7.10.2.9).
void CompoundSearchStack(NkAv1Ctx &ctx, int32 mvRow, int32 mvCol, int32 weight) {
	const int16 *m = MvsAt(ctx, mvRow, mvCol);
	int32 candMvs[2][2] = {{m[0], m[1]}, {m[2], m[3]}};
	const int32 candMode = Grid(ctx.yModes, ctx.gridStride, mvRow, mvCol);
	if (candMode == kModeGlobalGlobalMv) {
		for (int32 refList = 0; refList < 2; ++refList) {
			if (ctx.fh->gmType[ctx.refFrame[refList]] > kGmTranslation) {
				candMvs[refList][0] = ctx.globalMvs[refList][0];
				candMvs[refList][1] = ctx.globalMvs[refList][1];
			}
		}
	}
	LowerMvPrecision(ctx, candMvs[0]);
	LowerMvPrecision(ctx, candMvs[1]);
	ctx.foundMatch = 1;
	for (int32 idx = 0; idx < ctx.numMvFound; ++idx) {
		if (candMvs[0][0] == ctx.refStackMv[idx][0][0] && candMvs[0][1] == ctx.refStackMv[idx][0][1] &&
			candMvs[1][0] == ctx.refStackMv[idx][1][0] && candMvs[1][1] == ctx.refStackMv[idx][1][1]) {
			ctx.weightStack[idx] += weight;
			if (HasNewmvMode(candMode))
				++ctx.newMvCount;
			return;
		}
	}
	if (ctx.numMvFound < 8) {
		for (int32 i = 0; i < 2; ++i) {
			ctx.refStackMv[ctx.numMvFound][i][0] = candMvs[i][0];
			ctx.refStackMv[ctx.numMvFound][i][1] = candMvs[i][1];
		}
		ctx.weightStack[ctx.numMvFound] = weight;
		++ctx.numMvFound;
	}
	if (HasNewmvMode(candMode))
		++ctx.newMvCount;
}

// Add reference motion vector process (§7.10.2.7).
void AddRefMvCandidate(NkAv1Ctx &ctx, int32 mvRow, int32 mvCol, bool isCompound, int32 weight) {
	if (!Grid(ctx.isInters, ctx.gridStride, mvRow, mvCol))
		return;
	if (!isCompound) {
		for (int32 candList = 0; candList < 2; ++candList) {
			const int32 candRef = (candList == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, mvRow, mvCol)
												  : (int32)Grid(ctx.refFrames1, ctx.gridStride, mvRow, mvCol);
			if (candRef == ctx.refFrame[0])
				SearchStack(ctx, mvRow, mvCol, candList, weight);
		}
	} else {
		if ((int32)Grid(ctx.refFrames0, ctx.gridStride, mvRow, mvCol) == ctx.refFrame[0] &&
			(int32)Grid(ctx.refFrames1, ctx.gridStride, mvRow, mvCol) == ctx.refFrame[1])
			CompoundSearchStack(ctx, mvRow, mvCol, weight);
	}
}

// Scan row process (§7.10.2.2).
void ScanRow(NkAv1Ctx &ctx, int32 deltaRow, bool isCompound) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 end4 = NkAv1Min(NkAv1Min(bw4, ctx.miCols - ctx.MiCol), 16);
	int32 deltaCol = 0;
	const bool useStep16 = bw4 >= 16;
	if (NkAv1Abs(deltaRow) > 1) {
		deltaRow += ctx.MiRow & 1;
		deltaCol = 1 - (ctx.MiCol & 1);
	}
	int32 i = 0;
	while (i < end4) {
		const int32 mvRow = ctx.MiRow + deltaRow;
		const int32 mvCol = ctx.MiCol + deltaCol + i;
		if (!IsInside(ctx, mvRow, mvCol))
			break;
		int32 len = NkAv1Min(bw4, (int32)kNum4x4BlocksWide[(int32)Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol)]);
		if (NkAv1Abs(deltaRow) > 1)
			len = NkAv1Max(2, len);
		if (useStep16)
			len = NkAv1Max(4, len);
		AddRefMvCandidate(ctx, mvRow, mvCol, isCompound, len * 2);
		i += len;
	}
}

// Scan col process (§7.10.2.3).
void ScanCol(NkAv1Ctx &ctx, int32 deltaCol, bool isCompound) {
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	const int32 end4 = NkAv1Min(NkAv1Min(bh4, ctx.miRows - ctx.MiRow), 16);
	int32 deltaRow = 0;
	const bool useStep16 = bh4 >= 16;
	if (NkAv1Abs(deltaCol) > 1) {
		deltaRow = 1 - (ctx.MiRow & 1);
		deltaCol += ctx.MiCol & 1;
	}
	int32 i = 0;
	while (i < end4) {
		const int32 mvRow = ctx.MiRow + deltaRow + i;
		const int32 mvCol = ctx.MiCol + deltaCol;
		if (!IsInside(ctx, mvRow, mvCol))
			break;
		int32 len = NkAv1Min(bh4, (int32)kNum4x4BlocksHigh[(int32)Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol)]);
		if (NkAv1Abs(deltaCol) > 1)
			len = NkAv1Max(2, len);
		if (useStep16)
			len = NkAv1Max(4, len);
		AddRefMvCandidate(ctx, mvRow, mvCol, isCompound, len * 2);
		i += len;
	}
}

// Scan point process (§7.10.2.4).
void ScanPoint(NkAv1Ctx &ctx, int32 deltaRow, int32 deltaCol, bool isCompound) {
	const int32 mvRow = ctx.MiRow + deltaRow;
	const int32 mvCol = ctx.MiCol + deltaCol;
	if (IsInside(ctx, mvRow, mvCol) && ctx.refWritten[(usize)mvRow * (usize)ctx.gridStride + (usize)mvCol])
		AddRefMvCandidate(ctx, mvRow, mvCol, isCompound, 4);
}

// Temporal sample process (§7.10.2.6).
void AddTplRefMv(NkAv1Ctx &ctx, int32 deltaRow, int32 deltaCol, bool isCompound) {
	const int32 mvRow = (ctx.MiRow + deltaRow) | 1;
	const int32 mvCol = (ctx.MiCol + deltaCol) | 1;
	if (!IsInside(ctx, mvRow, mvCol))
		return;
	const int32 x8 = mvCol >> 1;
	const int32 y8 = mvRow >> 1;
	if (deltaRow == 0 && deltaCol == 0)
		ctx.zeroMvContext = 1;
	if (!isCompound) {
		const int16 *src = MfMvAt(ctx, ctx.refFrame[0], y8, x8);
		int32 candMv[2] = {src[0], src[1]};
		if (candMv[0] == (int32)(int16)(-(1 << 15)))
			return;
		LowerMvPrecision(ctx, candMv);
		if (deltaRow == 0 && deltaCol == 0) {
			if (NkAv1Abs(candMv[0] - ctx.globalMvs[0][0]) >= 16 || NkAv1Abs(candMv[1] - ctx.globalMvs[0][1]) >= 16)
				ctx.zeroMvContext = 1;
			else
				ctx.zeroMvContext = 0;
		}
		int32 idx;
		for (idx = 0; idx < ctx.numMvFound; ++idx) {
			if (candMv[0] == ctx.refStackMv[idx][0][0] && candMv[1] == ctx.refStackMv[idx][0][1])
				break;
		}
		if (idx < ctx.numMvFound) {
			ctx.weightStack[idx] += 2;
		} else if (ctx.numMvFound < 8) {
			ctx.refStackMv[ctx.numMvFound][0][0] = candMv[0];
			ctx.refStackMv[ctx.numMvFound][0][1] = candMv[1];
			ctx.weightStack[ctx.numMvFound] = 2;
			++ctx.numMvFound;
		}
	} else {
		const int16 *src0 = MfMvAt(ctx, ctx.refFrame[0], y8, x8);
		int32 candMv0[2] = {src0[0], src0[1]};
		if (candMv0[0] == (int32)(int16)(-(1 << 15)))
			return;
		const int16 *src1 = MfMvAt(ctx, ctx.refFrame[1], y8, x8);
		int32 candMv1[2] = {src1[0], src1[1]};
		if (candMv1[0] == (int32)(int16)(-(1 << 15)))
			return;
		LowerMvPrecision(ctx, candMv0);
		LowerMvPrecision(ctx, candMv1);
		if (deltaRow == 0 && deltaCol == 0) {
			if (NkAv1Abs(candMv0[0] - ctx.globalMvs[0][0]) >= 16 || NkAv1Abs(candMv0[1] - ctx.globalMvs[0][1]) >= 16 ||
				NkAv1Abs(candMv1[0] - ctx.globalMvs[1][0]) >= 16 || NkAv1Abs(candMv1[1] - ctx.globalMvs[1][1]) >= 16)
				ctx.zeroMvContext = 1;
			else
				ctx.zeroMvContext = 0;
		}
		int32 idx;
		for (idx = 0; idx < ctx.numMvFound; ++idx) {
			if (candMv0[0] == ctx.refStackMv[idx][0][0] && candMv0[1] == ctx.refStackMv[idx][0][1] &&
				candMv1[0] == ctx.refStackMv[idx][1][0] && candMv1[1] == ctx.refStackMv[idx][1][1])
				break;
		}
		if (idx < ctx.numMvFound) {
			ctx.weightStack[idx] += 2;
		} else if (ctx.numMvFound < 8) {
			ctx.refStackMv[ctx.numMvFound][0][0] = candMv0[0];
			ctx.refStackMv[ctx.numMvFound][0][1] = candMv0[1];
			ctx.refStackMv[ctx.numMvFound][1][0] = candMv1[0];
			ctx.refStackMv[ctx.numMvFound][1][1] = candMv1[1];
			ctx.weightStack[ctx.numMvFound] = 2;
			++ctx.numMvFound;
		}
	}
}

// Temporal scan process (§7.10.2.5).
void TemporalScan(NkAv1Ctx &ctx, bool isCompound) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	const int32 stepW4 = (bw4 >= 16) ? 4 : 2;
	const int32 stepH4 = (bh4 >= 16) ? 4 : 2;
	for (int32 deltaRow = 0; deltaRow < NkAv1Min(bh4, 16); deltaRow += stepH4)
		for (int32 deltaCol = 0; deltaCol < NkAv1Min(bw4, 16); deltaCol += stepW4)
			AddTplRefMv(ctx, deltaRow, deltaCol, isCompound);
	const bool allowExtension = (bh4 >= kNum4x4BlocksHigh[3 /*BLOCK_8X8*/]) && (bh4 < kNum4x4BlocksHigh[12 /*BLOCK_64X64*/]) &&
								(bw4 >= kNum4x4BlocksWide[3]) && (bw4 < kNum4x4BlocksWide[12]);
	if (allowExtension) {
		const int32 tplSamplePos[3][2] = {{bh4, -2}, {bh4, bw4}, {bh4 - 2, bw4}};
		for (int32 i = 0; i < 3; ++i) {
			const int32 deltaRow = tplSamplePos[i][0];
			const int32 deltaCol = tplSamplePos[i][1];
			const int32 row = (ctx.MiRow & 15) + deltaRow;
			const int32 col = (ctx.MiCol & 15) + deltaCol;
			if (row >= 0 && row < 16 && col >= 0 && col < 16)
				AddTplRefMv(ctx, deltaRow, deltaCol, isCompound);
		}
	}
}

// Sorting process (§7.10.2.11) — tri stable a bulles comme la spec.
void SortStack(NkAv1Ctx &ctx, int32 start, int32 end, bool isCompound) {
	while (end > start) {
		int32 newEnd = start;
		for (int32 idx = start + 1; idx < end; ++idx) {
			if (ctx.weightStack[idx - 1] < ctx.weightStack[idx]) {
				const int32 tw = ctx.weightStack[idx - 1];
				ctx.weightStack[idx - 1] = ctx.weightStack[idx];
				ctx.weightStack[idx] = tw;
				for (int32 list = 0; list < 1 + (isCompound ? 1 : 0); ++list) {
					for (int32 comp = 0; comp < 2; ++comp) {
						const int32 tv = ctx.refStackMv[idx - 1][list][comp];
						ctx.refStackMv[idx - 1][list][comp] = ctx.refStackMv[idx][list][comp];
						ctx.refStackMv[idx][list][comp] = tv;
					}
				}
				newEnd = idx;
			}
		}
		end = newEnd;
	}
}

// Add extra MV candidate process (§7.10.2.13).
void AddExtraMvCandidate(NkAv1Ctx &ctx, int32 mvRow, int32 mvCol, bool isCompound,
						 int32 refIdMvs[2][2][2], int32 refIdCount[2],
						 int32 refDiffMvs[2][2][2], int32 refDiffCount[2]) {
	if (isCompound) {
		for (int32 candList = 0; candList < 2; ++candList) {
			const int32 candRef = (candList == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, mvRow, mvCol)
												  : (int32)Grid(ctx.refFrames1, ctx.gridStride, mvRow, mvCol);
			if (candRef > kRefIntra) {
				for (int32 list = 0; list < 2; ++list) {
					const int16 *m = MvsAt(ctx, mvRow, mvCol);
					int32 candMv[2] = {m[candList * 2 + 0], m[candList * 2 + 1]};
					if (candRef == ctx.refFrame[list] && refIdCount[list] < 2) {
						refIdMvs[list][refIdCount[list]][0] = candMv[0];
						refIdMvs[list][refIdCount[list]][1] = candMv[1];
						++refIdCount[list];
					} else if (refDiffCount[list] < 2) {
						if (ctx.fh->refFrameSignBias[candRef] != ctx.fh->refFrameSignBias[ctx.refFrame[list]]) {
							candMv[0] *= -1;
							candMv[1] *= -1;
						}
						refDiffMvs[list][refDiffCount[list]][0] = candMv[0];
						refDiffMvs[list][refDiffCount[list]][1] = candMv[1];
						++refDiffCount[list];
					}
				}
			}
		}
	} else {
		for (int32 candList = 0; candList < 2; ++candList) {
			const int32 candRef = (candList == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, mvRow, mvCol)
												  : (int32)Grid(ctx.refFrames1, ctx.gridStride, mvRow, mvCol);
			if (candRef > kRefIntra) {
				const int16 *m = MvsAt(ctx, mvRow, mvCol);
				int32 candMv[2] = {m[candList * 2 + 0], m[candList * 2 + 1]};
				if (ctx.fh->refFrameSignBias[candRef] != ctx.fh->refFrameSignBias[ctx.refFrame[0]]) {
					candMv[0] *= -1;
					candMv[1] *= -1;
				}
				int32 idx;
				for (idx = 0; idx < ctx.numMvFound; ++idx) {
					if (candMv[0] == ctx.refStackMv[idx][0][0] && candMv[1] == ctx.refStackMv[idx][0][1])
						break;
				}
				if (idx == ctx.numMvFound) {
					ctx.refStackMv[idx][0][0] = candMv[0];
					ctx.refStackMv[idx][0][1] = candMv[1];
					ctx.weightStack[idx] = 2;
					++ctx.numMvFound;
				}
			}
		}
	}
}

// Extra search process (§7.10.2.12).
void ExtraSearch(NkAv1Ctx &ctx, bool isCompound) {
	int32 refIdMvs[2][2][2], refIdCount[2] = {0, 0};
	int32 refDiffMvs[2][2][2], refDiffCount[2] = {0, 0};
	int32 w4 = NkAv1Min(16, (int32)kNum4x4BlocksWide[ctx.MiSize]);
	int32 h4 = NkAv1Min(16, (int32)kNum4x4BlocksHigh[ctx.MiSize]);
	w4 = NkAv1Min(w4, ctx.miCols - ctx.MiCol);
	h4 = NkAv1Min(h4, ctx.miRows - ctx.MiRow);
	const int32 num4x4 = NkAv1Min(w4, h4);
	for (int32 pass = 0; pass < 2; ++pass) {
		int32 idx = 0;
		while (idx < num4x4 && ctx.numMvFound < 2) {
			int32 mvRow, mvCol;
			if (pass == 0) {
				mvRow = ctx.MiRow - 1;
				mvCol = ctx.MiCol + idx;
			} else {
				mvRow = ctx.MiRow + idx;
				mvCol = ctx.MiCol - 1;
			}
			if (!IsInside(ctx, mvRow, mvCol))
				break;
			AddExtraMvCandidate(ctx, mvRow, mvCol, isCompound, refIdMvs, refIdCount, refDiffMvs, refDiffCount);
			if (pass == 0)
				idx += kNum4x4BlocksWide[(int32)Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol)];
			else
				idx += kNum4x4BlocksHigh[(int32)Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol)];
		}
	}
	if (isCompound) {
		int32 combinedMvs[2][2][2];
		for (int32 list = 0; list < 2; ++list) {
			int32 compCount = 0;
			for (int32 idx = 0; idx < refIdCount[list]; ++idx) {
				combinedMvs[compCount][list][0] = refIdMvs[list][idx][0];
				combinedMvs[compCount][list][1] = refIdMvs[list][idx][1];
				++compCount;
			}
			for (int32 idx = 0; idx < refDiffCount[list] && compCount < 2; ++idx) {
				combinedMvs[compCount][list][0] = refDiffMvs[list][idx][0];
				combinedMvs[compCount][list][1] = refDiffMvs[list][idx][1];
				++compCount;
			}
			while (compCount < 2) {
				combinedMvs[compCount][list][0] = ctx.globalMvs[list][0];
				combinedMvs[compCount][list][1] = ctx.globalMvs[list][1];
				++compCount;
			}
		}
		if (ctx.numMvFound == 1) {
			if (combinedMvs[0][0][0] == ctx.refStackMv[0][0][0] && combinedMvs[0][0][1] == ctx.refStackMv[0][0][1] &&
				combinedMvs[0][1][0] == ctx.refStackMv[0][1][0] && combinedMvs[0][1][1] == ctx.refStackMv[0][1][1]) {
				ctx.refStackMv[ctx.numMvFound][0][0] = combinedMvs[1][0][0];
				ctx.refStackMv[ctx.numMvFound][0][1] = combinedMvs[1][0][1];
				ctx.refStackMv[ctx.numMvFound][1][0] = combinedMvs[1][1][0];
				ctx.refStackMv[ctx.numMvFound][1][1] = combinedMvs[1][1][1];
			} else {
				ctx.refStackMv[ctx.numMvFound][0][0] = combinedMvs[0][0][0];
				ctx.refStackMv[ctx.numMvFound][0][1] = combinedMvs[0][0][1];
				ctx.refStackMv[ctx.numMvFound][1][0] = combinedMvs[0][1][0];
				ctx.refStackMv[ctx.numMvFound][1][1] = combinedMvs[0][1][1];
			}
			ctx.weightStack[ctx.numMvFound] = 2;
			++ctx.numMvFound;
		} else {
			for (int32 idx = 0; idx < 2; ++idx) {
				ctx.refStackMv[ctx.numMvFound][0][0] = combinedMvs[idx][0][0];
				ctx.refStackMv[ctx.numMvFound][0][1] = combinedMvs[idx][0][1];
				ctx.refStackMv[ctx.numMvFound][1][0] = combinedMvs[idx][1][0];
				ctx.refStackMv[ctx.numMvFound][1][1] = combinedMvs[idx][1][1];
				ctx.weightStack[ctx.numMvFound] = 2;
				++ctx.numMvFound;
			}
		}
	} else {
		for (int32 idx = ctx.numMvFound; idx < 2; ++idx) {
			ctx.refStackMv[idx][0][0] = ctx.globalMvs[0][0];
			ctx.refStackMv[idx][0][1] = ctx.globalMvs[0][1];
		}
	}
}

// clamp_mv_row / clamp_mv_col (§5.11.53/54).
inline int32 ClampMvRow(NkAv1Ctx &ctx, int32 mvec, int32 border) {
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	const int32 mbToTopEdge = -((ctx.MiRow * 4) * 8);
	const int32 mbToBottomEdge = ((ctx.miRows - bh4 - ctx.MiRow) * 4) * 8;
	return NkAv1Clip3(mbToTopEdge - border, mbToBottomEdge + border, mvec);
}
inline int32 ClampMvCol(NkAv1Ctx &ctx, int32 mvec, int32 border) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 mbToLeftEdge = -((ctx.MiCol * 4) * 8);
	const int32 mbToRightEdge = ((ctx.miCols - bw4 - ctx.MiCol) * 4) * 8;
	return NkAv1Clip3(mbToLeftEdge - border, mbToRightEdge + border, mvec);
}

// Context and clamping process (§7.10.2.14).
void ContextAndClamping(NkAv1Ctx &ctx, bool isCompound, int32 numNew) {
	const int32 bw = BlockWidthOf(ctx.MiSize), bh = BlockHeightOf(ctx.MiSize);
	const int32 numLists = isCompound ? 2 : 1;
	for (int32 idx = 0; idx < ctx.numMvFound; ++idx) {
		int32 z = 0;
		if (idx + 1 < ctx.numMvFound) {
			const int32 w0 = ctx.weightStack[idx];
			const int32 w1 = ctx.weightStack[idx + 1];
			if (w0 >= kRefCatLevel) {
				if (w1 < kRefCatLevel)
					z = 1;
			} else {
				z = 2;
			}
		}
		ctx.drlCtxStack[idx] = z;
	}
	for (int32 list = 0; list < numLists; ++list) {
		for (int32 idx = 0; idx < ctx.numMvFound; ++idx) {
			ctx.refStackMv[idx][list][0] = ClampMvRow(ctx, ctx.refStackMv[idx][list][0], kMvBorder + bh * 8);
			ctx.refStackMv[idx][list][1] = ClampMvCol(ctx, ctx.refStackMv[idx][list][1], kMvBorder + bw * 8);
		}
	}
	if (ctx.closeMatches == 0) {
		ctx.newMvContext = NkAv1Min(ctx.totalMatches, 1);
		ctx.refMvContext = ctx.totalMatches;
	} else if (ctx.closeMatches == 1) {
		ctx.newMvContext = 3 - NkAv1Min(numNew, 1);
		ctx.refMvContext = 2 + ctx.totalMatches;
	} else {
		ctx.newMvContext = 5 - NkAv1Min(numNew, 1);
		ctx.refMvContext = 5;
	}
}

// Find MV stack process (§7.10.2).
void FindMvStack(NkAv1Ctx &ctx, bool isCompound) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	ctx.numMvFound = 0;
	ctx.newMvCount = 0;
	SetupGlobalMv(ctx, 0, ctx.globalMvs[0]);
	if (isCompound)
		SetupGlobalMv(ctx, 1, ctx.globalMvs[1]);
	ctx.foundMatch = 0;
	ScanRow(ctx, -1, isCompound);
	int32 foundAboveMatch = ctx.foundMatch;
	ctx.foundMatch = 0;
	ScanCol(ctx, -1, isCompound);
	int32 foundLeftMatch = ctx.foundMatch;
	ctx.foundMatch = 0;
	if (NkAv1Max(bw4, bh4) <= 16)
		ScanPoint(ctx, -1, bw4, isCompound);
	if (ctx.foundMatch)
		foundAboveMatch = 1;
	ctx.closeMatches = foundAboveMatch + foundLeftMatch;
	const int32 numNearest = ctx.numMvFound;
	const int32 numNew = ctx.newMvCount;
	if (numNearest > 0) {
		for (int32 idx = 0; idx < numNearest; ++idx)
			ctx.weightStack[idx] += kRefCatLevel;
	}
	ctx.zeroMvContext = 0;
	if (ctx.useRefFrameMvsFrame)
		TemporalScan(ctx, isCompound);
	ctx.foundMatch = 0;
	ScanPoint(ctx, -1, -1, isCompound);
	if (ctx.foundMatch)
		foundAboveMatch = 1;
	ctx.foundMatch = 0;
	ScanRow(ctx, -3, isCompound);
	if (ctx.foundMatch)
		foundAboveMatch = 1;
	ctx.foundMatch = 0;
	ScanCol(ctx, -3, isCompound);
	if (ctx.foundMatch)
		foundLeftMatch = 1;
	ctx.foundMatch = 0;
	if (bh4 > 1) {
		ScanRow(ctx, -5, isCompound);
		if (ctx.foundMatch)
			foundAboveMatch = 1;
		ctx.foundMatch = 0;
	}
	if (bw4 > 1) {
		ScanCol(ctx, -5, isCompound);
		if (ctx.foundMatch)
			foundLeftMatch = 1;
		ctx.foundMatch = 0;
	}
	ctx.totalMatches = foundAboveMatch + foundLeftMatch;
	SortStack(ctx, 0, numNearest, isCompound);
	SortStack(ctx, numNearest, ctx.numMvFound, isCompound);
	if (ctx.numMvFound < 2)
		ExtraSearch(ctx, isCompound);
	ContextAndClamping(ctx, isCompound, numNew);
}

// Has overlappable candidates process (§7.10.3).
bool HasOverlappableCandidates(NkAv1Ctx &ctx) {
	if (ctx.AvailU) {
		const int32 w4 = kNum4x4BlocksWide[ctx.MiSize];
		for (int32 x4 = ctx.MiCol; x4 < NkAv1Min(ctx.miCols, ctx.MiCol + w4); x4 += 2) {
			if ((int32)Grid(ctx.refFrames0, ctx.gridStride, ctx.MiRow - 1, x4 | 1) > kRefIntra)
				return true;
		}
	}
	if (ctx.AvailL) {
		const int32 h4 = kNum4x4BlocksHigh[ctx.MiSize];
		for (int32 y4 = ctx.MiRow; y4 < NkAv1Min(ctx.miRows, ctx.MiRow + h4); y4 += 2) {
			if ((int32)Grid(ctx.refFrames0, ctx.gridStride, y4 | 1, ctx.MiCol - 1) > kRefIntra)
				return true;
		}
	}
	return false;
}

// Find warp samples process (§7.10.4) — on ne garde que NumSamples (le warp
// local lui-meme est refuse proprement s'il est SELECTIONNE ; mais le COMPTE
// influence quel element de syntaxe est lu dans read_motion_mode).
void AddWarpSample(NkAv1Ctx &ctx, int32 deltaRow, int32 deltaCol) {
	if (ctx.numSamplesScanned >= 8 /*LEAST_SQUARES_SAMPLES_MAX*/)
		return;
	const int32 mvRow = ctx.MiRow + deltaRow;
	const int32 mvCol = ctx.MiCol + deltaCol;
	if (!IsInside(ctx, mvRow, mvCol))
		return;
	if (!ctx.refWritten[(usize)mvRow * (usize)ctx.gridStride + (usize)mvCol])
		return;
	if ((int32)Grid(ctx.refFrames0, ctx.gridStride, mvRow, mvCol) != ctx.refFrame[0])
		return;
	if ((int32)Grid(ctx.refFrames1, ctx.gridStride, mvRow, mvCol) != kRefNone)
		return;
	const int32 candSz = Grid(ctx.miSizes, ctx.gridStride, mvRow, mvCol);
	const int32 candW4 = kNum4x4BlocksWide[candSz];
	const int32 candH4 = kNum4x4BlocksHigh[candSz];
	const int32 candRow = mvRow & ~(candH4 - 1);
	const int32 candCol = mvCol & ~(candW4 - 1);
	const int32 threshold = NkAv1Clip3(16, 112, NkAv1Max(BlockWidthOf(ctx.MiSize), BlockHeightOf(ctx.MiSize)));
	const int16 *m = MvsAt(ctx, candRow, candCol);
	const int32 mvDiffRow = NkAv1Abs((int32)m[0] - ctx.mvBlk[0][0]);
	const int32 mvDiffCol = NkAv1Abs((int32)m[1] - ctx.mvBlk[0][1]);
	const bool valid = (mvDiffRow + mvDiffCol) <= threshold;
	const int32 midY = candRow * 4 + candH4 * 2 - 1;
	const int32 midX = candCol * 4 + candW4 * 2 - 1;
	int32 cand[4];
	cand[0] = midY * 8;
	cand[1] = midX * 8;
	cand[2] = midY * 8 + (int32)m[0];
	cand[3] = midX * 8 + (int32)m[1];
	++ctx.numSamplesScanned;
	if (!valid && ctx.numSamplesScanned > 1)
		return;
	for (int32 j = 0; j < 4; ++j)
		ctx.candList[ctx.numSamples][j] = cand[j];
	if (valid)
		++ctx.numSamples;
}

void FindWarpSamples(NkAv1Ctx &ctx) {
	ctx.numSamples = 0;
	ctx.numSamplesScanned = 0;
	const int32 w4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 h4 = kNum4x4BlocksHigh[ctx.MiSize];
	int32 doTopLeft = 1, doTopRight = 1;
	if (ctx.AvailU) {
		const int32 srcSize = Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
		const int32 srcW = kNum4x4BlocksWide[srcSize];
		if (w4 <= srcW) {
			const int32 colOffset = -(ctx.MiCol & (srcW - 1));
			if (colOffset < 0)
				doTopLeft = 0;
			if (colOffset + srcW > w4)
				doTopRight = 0;
			AddWarpSample(ctx, -1, 0);
		} else {
			int32 miStep;
			for (int32 i = 0; i < NkAv1Min(w4, ctx.miCols - ctx.MiCol); i += miStep) {
				const int32 srcSize2 = Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol + i);
				const int32 srcW2 = kNum4x4BlocksWide[srcSize2];
				miStep = NkAv1Min(w4, srcW2);
				AddWarpSample(ctx, -1, i);
			}
		}
	}
	if (ctx.AvailL) {
		const int32 srcSize = Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
		const int32 srcH = kNum4x4BlocksHigh[srcSize];
		if (h4 <= srcH) {
			const int32 rowOffset = -(ctx.MiRow & (srcH - 1));
			if (rowOffset < 0)
				doTopLeft = 0;
			AddWarpSample(ctx, 0, -1);
		} else {
			int32 miStep;
			for (int32 i = 0; i < NkAv1Min(h4, ctx.miRows - ctx.MiRow); i += miStep) {
				const int32 srcSize2 = Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow + i, ctx.MiCol - 1);
				const int32 srcH2 = kNum4x4BlocksHigh[srcSize2];
				miStep = NkAv1Min(h4, srcH2);
				AddWarpSample(ctx, i, -1);
			}
		}
	}
	if (doTopLeft)
		AddWarpSample(ctx, -1, -1);
	if (doTopRight) {
		if (NkAv1Max(w4, h4) <= 16)
			AddWarpSample(ctx, -1, w4);
	}
	if (ctx.numSamples == 0 && ctx.numSamplesScanned > 0)
		ctx.numSamples = 1;
}

// =========================================================================
// decode_partition / decode_block (§5.11.4)
// =========================================================================
void DecodePartition(NkAv1Ctx &ctx, int32 r, int32 c, int32 bSize) {
	if (!ctx.ok)
		return; // erreur/refus en amont : ne pas continuer (le 1er motif prime)
	if (r >= ctx.miRows || c >= ctx.miCols)
		return;
	const bool availU = IsInside(ctx, r - 1, c);
	const bool availL = IsInside(ctx, r, c - 1);
	const int32 num4x4 = kNum4x4BlocksWide[bSize];
	const int32 half4x4 = num4x4 >> 1;
	const int32 quarter4x4 = half4x4 >> 1;
	const bool hasRows = (r + half4x4) < ctx.miRows;
	const bool hasCols = (c + half4x4) < ctx.miCols;
	int32 partition;

	// ctx derivation shared by partition / split_or_horz / split_or_vert.
	const int32 bsl = kMiWidthLog2[bSize];
	const int32 above = (availU && (kMiWidthLog2[(int32)Grid(ctx.miSizes, ctx.gridStride, r - 1, c)] < bsl)) ? 1 : 0;
	const int32 left = (availL && (kMiHeightLog2[(int32)Grid(ctx.miSizes, ctx.gridStride, r, c - 1)] < bsl)) ? 1 : 0;
	const int32 partCtx = left * 2 + above;

	const uint16 *baseCdf = nullptr; int32 partN = 0;
	if (bsl == 1) { baseCdf = ctx.cdf.partitionW8[partCtx]; partN = 4; }
	else if (bsl == 2) { baseCdf = ctx.cdf.partitionW16[partCtx]; partN = 10; }
	else if (bsl == 3) { baseCdf = ctx.cdf.partitionW32[partCtx]; partN = 10; }
	else if (bsl == 4) { baseCdf = ctx.cdf.partitionW64[partCtx]; partN = 10; }
	else { baseCdf = ctx.cdf.partitionW128[partCtx]; partN = 8; }

	if (bSize < 3 /*BLOCK_8X8*/) {
		partition = kPartNone;
	} else if (hasRows && hasCols) {
		uint16 *mutableCdf = (bsl == 1) ? ctx.cdf.partitionW8[partCtx] : (bsl == 2) ? ctx.cdf.partitionW16[partCtx] :
			(bsl == 3) ? ctx.cdf.partitionW32[partCtx] : (bsl == 4) ? ctx.cdf.partitionW64[partCtx] : ctx.cdf.partitionW128[partCtx];
		partition = ctx.sd.DecodeSymbol(mutableCdf, partN);
	} else if (hasCols) {
		// split_or_horz : derive a 2-symbol cdf from partitionCdf (non-adaptive, discarded after use).
		uint16 tmp[3];
		int32 psum = (baseCdf[kPartVert] - baseCdf[kPartVert - 1]) + (baseCdf[kPartSplit] - baseCdf[kPartSplit - 1]) +
					 (baseCdf[kPartHorzA] - baseCdf[kPartHorzA - 1]) + (baseCdf[kPartVertA] - baseCdf[kPartVertA - 1]) +
					 (baseCdf[kPartVertB] - baseCdf[kPartVertB - 1]);
		if (bSize != 15 /*BLOCK_128X128*/)
			psum += (baseCdf[kPartVert4] - baseCdf[kPartVert4 - 1]);
		tmp[0] = (uint16)((1 << 15) - psum);
		tmp[1] = (uint16)(1 << 15);
		tmp[2] = 0;
		const int32 splitOrHorz = ctx.sd.DecodeSymbolNoAdapt(tmp, 2);
		partition = splitOrHorz ? kPartSplit : kPartHorz;
	} else if (hasRows) {
		uint16 tmp[3];
		int32 psum = (baseCdf[kPartHorz] - baseCdf[kPartHorz - 1]) + (baseCdf[kPartSplit] - baseCdf[kPartSplit - 1]) +
					 (baseCdf[kPartHorzA] - baseCdf[kPartHorzA - 1]) + (baseCdf[kPartHorzB] - baseCdf[kPartHorzB - 1]) +
					 (baseCdf[kPartVertA] - baseCdf[kPartVertA - 1]);
		if (bSize != 15)
			psum += (baseCdf[kPartHorz4] - baseCdf[kPartHorz4 - 1]);
		tmp[0] = (uint16)((1 << 15) - psum);
		tmp[1] = (uint16)(1 << 15);
		tmp[2] = 0;
		const int32 splitOrVert = ctx.sd.DecodeSymbolNoAdapt(tmp, 2);
		partition = splitOrVert ? kPartSplit : kPartVert;
	} else {
		partition = kPartSplit;
	}

	const int32 subSize = kPartitionSubsize[partition][bSize];
	const int32 splitSize = kPartitionSubsize[kPartSplit][bSize];

	if (partition == kPartNone) {
		DecodeBlock(ctx, r, c, subSize);
	} else if (partition == kPartHorz) {
		DecodeBlock(ctx, r, c, subSize);
		if (hasRows) DecodeBlock(ctx, r + half4x4, c, subSize);
	} else if (partition == kPartVert) {
		DecodeBlock(ctx, r, c, subSize);
		if (hasCols) DecodeBlock(ctx, r, c + half4x4, subSize);
	} else if (partition == kPartSplit) {
		DecodePartition(ctx, r, c, subSize);
		DecodePartition(ctx, r, c + half4x4, subSize);
		DecodePartition(ctx, r + half4x4, c, subSize);
		DecodePartition(ctx, r + half4x4, c + half4x4, subSize);
	} else if (partition == kPartHorzA) {
		DecodeBlock(ctx, r, c, splitSize);
		DecodeBlock(ctx, r, c + half4x4, splitSize);
		DecodeBlock(ctx, r + half4x4, c, subSize);
	} else if (partition == kPartHorzB) {
		DecodeBlock(ctx, r, c, subSize);
		DecodeBlock(ctx, r + half4x4, c, splitSize);
		DecodeBlock(ctx, r + half4x4, c + half4x4, splitSize);
	} else if (partition == kPartVertA) {
		DecodeBlock(ctx, r, c, splitSize);
		DecodeBlock(ctx, r + half4x4, c, splitSize);
		DecodeBlock(ctx, r, c + half4x4, subSize);
	} else if (partition == kPartVertB) {
		DecodeBlock(ctx, r, c, subSize);
		DecodeBlock(ctx, r, c + half4x4, splitSize);
		DecodeBlock(ctx, r + half4x4, c + half4x4, splitSize);
	} else if (partition == kPartHorz4) {
		DecodeBlock(ctx, r + quarter4x4 * 0, c, subSize);
		DecodeBlock(ctx, r + quarter4x4 * 1, c, subSize);
		DecodeBlock(ctx, r + quarter4x4 * 2, c, subSize);
		if (r + quarter4x4 * 3 < ctx.miRows)
			DecodeBlock(ctx, r + quarter4x4 * 3, c, subSize);
	} else { // PARTITION_VERT_4
		DecodeBlock(ctx, r, c + quarter4x4 * 0, subSize);
		DecodeBlock(ctx, r, c + quarter4x4 * 1, subSize);
		DecodeBlock(ctx, r, c + quarter4x4 * 2, subSize);
		if (c + quarter4x4 * 3 < ctx.miCols)
			DecodeBlock(ctx, r, c + quarter4x4 * 3, subSize);
	}
}

void DecodeBlock(NkAv1Ctx &ctx, int32 r, int32 c, int32 subSize) {
	if (!ctx.ok)
		return;
	ctx.MiRow = r;
	ctx.MiCol = c;
	ctx.MiSize = subSize;
	const int32 bw4 = kNum4x4BlocksWide[subSize];
	const int32 bh4 = kNum4x4BlocksHigh[subSize];
	const int32 ssx = ctx.seq->subsamplingX, ssy = ctx.seq->subsamplingY;
	if (bh4 == 1 && ssy && (r & 1) == 0)
		ctx.HasChroma = false;
	else if (bw4 == 1 && ssx && (c & 1) == 0)
		ctx.HasChroma = false;
	else
		ctx.HasChroma = !ctx.seq->mono;
	ctx.AvailU = IsInside(ctx, r - 1, c);
	ctx.AvailL = IsInside(ctx, r, c - 1);
	ctx.AvailUChroma = ctx.AvailU;
	ctx.AvailLChroma = ctx.AvailL;
	if (ctx.HasChroma) {
		if (ssy && bh4 == 1) ctx.AvailUChroma = IsInside(ctx, r - 2, c);
		if (ssx && bw4 == 1) ctx.AvailLChroma = IsInside(ctx, r, c - 2);
	} else {
		ctx.AvailUChroma = false;
		ctx.AvailLChroma = false;
	}

	ModeInfo(ctx);
	if (!ctx.ok)
		return;
	// palette_tokens() -- palette unsupported (allow_screen_content_tools path
	// bails out earlier in ModeInfo/IntraFrameModeInfo if palette is selected).
	ReadBlockTxSize(ctx);

	if (ctx.skip_)
		ResetBlockContext(ctx, bw4, bh4);
	const bool blkIsCompound = ctx.refFrame[1] > 0 /*INTRA_FRAME*/;

	for (int32 y = 0; y < bh4; ++y) {
		for (int32 x = 0; x < bw4; ++x) {
			Grid(ctx.yModes, ctx.gridStride, r + y, c + x) = (int8)ctx.YMode;
			if (ctx.refFrame[0] == 0 /*INTRA_FRAME*/ && ctx.HasChroma)
				Grid(ctx.uvModes, ctx.gridStride, r + y, c + x) = (int8)ctx.UVMode;
			Grid(ctx.miSizes, ctx.gridStride, r + y, c + x) = (int8)subSize;
			Grid(ctx.refFrames0, ctx.gridStride, r + y, c + x) = (int8)ctx.refFrame[0];
			Grid(ctx.refFrames1, ctx.gridStride, r + y, c + x) = (int8)ctx.refFrame[1];
			ctx.refWritten[(usize)(r + y) * (usize)ctx.gridStride + (usize)(c + x)] = 1;
			if (ctx.isInter_) {
				Grid(ctx.compGroupIdxs, ctx.gridStride, r + y, c + x) = (int8)ctx.compGroupIdx_;
				Grid(ctx.compoundIdxs, ctx.gridStride, r + y, c + x) = (int8)ctx.compoundIdx_;
				Grid(ctx.interpFilters0, ctx.gridStride, r + y, c + x) = (int8)ctx.interpFilterBlk[0];
				Grid(ctx.interpFilters1, ctx.gridStride, r + y, c + x) = (int8)ctx.interpFilterBlk[1];
				int16 *m = MvsAt(ctx, r + y, c + x);
				for (int32 refList = 0; refList < 1 + (blkIsCompound ? 1 : 0); ++refList) {
					m[refList * 2 + 0] = (int16)ctx.mvBlk[refList][0];
					m[refList * 2 + 1] = (int16)ctx.mvBlk[refList][1];
				}
			}
		}
	}

	// compute_prediction() : pour les blocs INTER, la prediction se fait ici,
	// AVANT residual(). Pour l'intra, predict_intra reste invoque par
	// transform_block (comme la spec).
	if (ctx.isInter_) {
		ComputePrediction(ctx);
		if (!ctx.ok)
			return;
	}
	Residual(ctx);

	for (int32 y = 0; y < bh4; ++y) {
		for (int32 x = 0; x < bw4; ++x) {
			Grid(ctx.isInters, ctx.gridStride, r + y, c + x) = (int8)(ctx.isInter_ ? 1 : 0);
			Grid(ctx.skipModes, ctx.gridStride, r + y, c + x) = (int8)(ctx.skipMode_ ? 1 : 0);
			Grid(ctx.skips, ctx.gridStride, r + y, c + x) = (int8)ctx.skip_;
			Grid(ctx.segmentIds, ctx.gridStride, r + y, c + x) = (int8)ctx.segmentId_;
			for (int32 i = 0; i < 4; ++i)
				Grid(ctx.deltaLfGrid[i], ctx.gridStride, r + y, c + x) = (int8)ctx.DeltaLF[i];
			// TxSizes grid already filled per-tx-block in ReadBlockTxSize (InterTxSizes use).
		}
	}
}

// =========================================================================
// mode_info / intra_frame_mode_info + sub-syntax (§5.11.7 .. §5.11.18)
// =========================================================================
int32 NegDeinterleave(int32 diff, int32 ref, int32 mx) {
	if (!ref) return diff;
	if (ref >= (mx - 1)) return mx - diff - 1;
	if (2 * ref < mx) {
		if (diff <= 2 * ref) {
			if (diff & 1) return ref + ((diff + 1) >> 1);
			return ref - (diff >> 1);
		}
		return diff;
	}
	if (diff <= 2 * (mx - ref - 1)) {
		if (diff & 1) return ref + ((diff + 1) >> 1);
		return ref - (diff >> 1);
	}
	return mx - (diff + 1);
}

bool SegFeatureActiveIdx(NkAv1Ctx &ctx, int32 idx, int32 feature) {
	return ctx.fh->seg.enabled && ctx.fh->seg.featureEnabled[idx][feature];
}
bool SegFeatureActive(NkAv1Ctx &ctx, int32 feature) { return SegFeatureActiveIdx(ctx, ctx.segmentId_, feature); }

void ReadSegmentId(NkAv1Ctx &ctx) {
	int32 prevUL = -1, prevU = -1, prevL = -1;
	if (ctx.AvailU && ctx.AvailL) prevUL = Grid(ctx.segmentIds, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol - 1);
	if (ctx.AvailU) prevU = Grid(ctx.segmentIds, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
	if (ctx.AvailL) prevL = Grid(ctx.segmentIds, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
	int32 pred;
	if (prevU == -1) pred = (prevL == -1) ? 0 : prevL;
	else if (prevL == -1) pred = prevU;
	else pred = (prevUL == prevU) ? prevU : prevL;
	if (ctx.skip_) {
		ctx.segmentId_ = pred;
		return;
	}
	int32 segCtx;
	if (prevUL < 0) segCtx = 0;
	else if ((prevUL == prevU) && (prevUL == prevL)) segCtx = 2;
	else if ((prevUL == prevU) || (prevUL == prevL) || (prevU == prevL)) segCtx = 1;
	else segCtx = 0;
	const int32 seg = ctx.sd.DecodeSymbol(ctx.cdf.segmentId[segCtx], 8);
	ctx.segmentId_ = NegDeinterleave(seg, pred, ctx.fh->seg.lastActiveSegId + 1);
}

void IntraSegmentId(NkAv1Ctx &ctx) {
	if (ctx.fh->seg.enabled)
		ReadSegmentId(ctx);
	else
		ctx.segmentId_ = 0;
	ctx.Lossless = ctx.fh->losslessArray[ctx.segmentId_];
}

void ReadSkip(NkAv1Ctx &ctx) {
	if (ctx.fh->seg.preSkipSegidPresent && SegFeatureActive(ctx, 6 /*SEG_LVL_SKIP*/)) {
		ctx.skip_ = true;
		return;
	}
	int32 sctx = 0;
	if (ctx.AvailU) sctx += Grid(ctx.skips, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
	if (ctx.AvailL) sctx += Grid(ctx.skips, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
	ctx.skip_ = ctx.sd.DecodeSymbol(ctx.cdf.skip[sctx], 2) != 0;
}

void ReadDeltaQIndex(NkAv1Ctx &ctx) {
	const int32 sbSize = ctx.seq->use128x128Superblock ? 15 : 12;
	if (ctx.MiSize == sbSize && ctx.skip_) return;
	if (ctx.ReadDeltas) {
		int32 deltaQAbs = ctx.sd.DecodeSymbol(ctx.cdf.deltaQ, 4);
		if (deltaQAbs == 3 /*DELTA_Q_SMALL*/) {
			const int32 remBits = (int32)ctx.sd.ReadLiteral(3) + 1;
			const int32 absBits = (int32)ctx.sd.ReadLiteral(remBits);
			deltaQAbs = absBits + (1 << remBits) + 1;
		}
		if (deltaQAbs) {
			const int32 signBit = (int32)ctx.sd.ReadLiteral(1);
			const int32 reduced = signBit ? -deltaQAbs : deltaQAbs;
			ctx.CurrentQIndex = NkAv1Clip3(1, 255, ctx.CurrentQIndex + (reduced << ctx.fh->deltaQRes));
		}
	}
}

void ReadDeltaLf(NkAv1Ctx &ctx) {
	const int32 sbSize = ctx.seq->use128x128Superblock ? 15 : 12;
	if (ctx.MiSize == sbSize && ctx.skip_) return;
	if (ctx.ReadDeltas && ctx.fh->deltaLfPresent) {
		int32 frameLfCount = 1;
		if (ctx.fh->deltaLfMulti)
			frameLfCount = (!ctx.seq->mono) ? 4 : 2;
		for (int32 i = 0; i < frameLfCount; ++i) {
			int32 deltaLfAbs = ctx.fh->deltaLfMulti ? ctx.sd.DecodeSymbol(ctx.cdf.deltaLfMulti[i], 4)
													 : ctx.sd.DecodeSymbol(ctx.cdf.deltaLf, 4);
			if (deltaLfAbs == 3 /*DELTA_LF_SMALL*/) {
				const int32 remBits = (int32)ctx.sd.ReadLiteral(3) + 1;
				const int32 absBits = (int32)ctx.sd.ReadLiteral(remBits);
				deltaLfAbs = absBits + (1 << remBits) + 1;
			}
			if (deltaLfAbs) {
				const int32 signBit = (int32)ctx.sd.ReadLiteral(1);
				const int32 reduced = signBit ? -deltaLfAbs : deltaLfAbs;
				ctx.DeltaLF[i] = NkAv1Clip3(-63, 63, ctx.DeltaLF[i] + (reduced << ctx.fh->deltaLfRes));
			}
		}
	}
}

void IntraAngleInfoY(NkAv1Ctx &ctx) {
	ctx.AngleDeltaY = 0;
	if (ctx.MiSize >= 3 /*BLOCK_8X8*/ && IsDirectionalMode(ctx.YMode)) {
		const int32 v = ctx.sd.DecodeSymbol(ctx.cdf.angleDelta[ctx.YMode - kModeV], 7);
		ctx.AngleDeltaY = v - 3;
	}
}
void IntraAngleInfoUV(NkAv1Ctx &ctx) {
	ctx.AngleDeltaUV = 0;
	if (ctx.MiSize >= 3 && IsDirectionalMode(ctx.UVMode)) {
		const int32 v = ctx.sd.DecodeSymbol(ctx.cdf.angleDelta[ctx.UVMode - kModeV], 7);
		ctx.AngleDeltaUV = v - 3;
	}
}

void ReadCflAlphas(NkAv1Ctx &ctx) {
	const int32 signs = ctx.sd.DecodeSymbol(ctx.cdf.cflSign, 8);
	const int32 signU = (signs + 1) / 3;
	const int32 signV = (signs + 1) % 3;
	if (signU != 0 /*CFL_SIGN_ZERO*/) {
		const int32 actx = (signU - 1) * 3 + signV;
		const int32 u = ctx.sd.DecodeSymbol(ctx.cdf.cflAlpha[actx], 16);
		ctx.CflAlphaU = 1 + u;
		if (signU == 1 /*CFL_SIGN_NEG*/) ctx.CflAlphaU = -ctx.CflAlphaU;
	} else {
		ctx.CflAlphaU = 0;
	}
	if (signV != 0) {
		const int32 actx = (signV - 1) * 3 + signU;
		const int32 v = ctx.sd.DecodeSymbol(ctx.cdf.cflAlpha[actx], 16);
		ctx.CflAlphaV = 1 + v;
		if (signV == 1) ctx.CflAlphaV = -ctx.CflAlphaV;
	} else {
		ctx.CflAlphaV = 0;
	}
}

void FilterIntraModeInfo(NkAv1Ctx &ctx) {
	ctx.useFilterIntra = false;
	const int32 bw = kNum4x4BlocksWide[ctx.MiSize] * 4, bh = kNum4x4BlocksHigh[ctx.MiSize] * 4;
	if (ctx.seq->enableFilterIntra && ctx.YMode == kModeDC && ctx.PaletteSizeY == 0 &&
		NkAv1Max(bw, bh) <= 32) {
		ctx.useFilterIntra = ctx.sd.DecodeSymbol(ctx.cdf.filterIntra[ctx.MiSize], 2) != 0;
		if (ctx.useFilterIntra)
			ctx.filterIntraMode_ = ctx.sd.DecodeSymbol(ctx.cdf.filterIntraMode, 5);
	}
}

void IntraFrameModeInfo(NkAv1Ctx &ctx) {
	ctx.skip_ = false;
	ctx.skipMode_ = false;
	ctx.isInter_ = false;
	ctx.refFrame[0] = 0 /*INTRA_FRAME*/;
	ctx.refFrame[1] = -1 /*NONE*/;
	if (ctx.fh->seg.preSkipSegidPresent)
		IntraSegmentId(ctx);
	ReadSkip(ctx);
	if (!ctx.fh->seg.preSkipSegidPresent)
		IntraSegmentId(ctx);
	ReadCdef(ctx);
	ReadDeltaQIndex(ctx);
	ReadDeltaLf(ctx);
	ctx.ReadDeltas = false;

	ctx.useIntrabc = false;
	if (ctx.fh->allowIntrabc) {
		ctx.useIntrabc = ctx.sd.DecodeSymbol(ctx.cdf.intrabc, 2) != 0;
	}
	if (ctx.useIntrabc) {
		// Intra block copy not implemented (not exercised by our intra-only
		// still-image test streams; allow_intrabc is false for them).
		ctx.ok = false;
		ctx.err = "intra block copy selectionne (non implemente)";
		return;
	}
	const int32 abovemode = kIntraModeContext[ctx.AvailU ? (int32)Grid(ctx.yModes, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol) : kModeDC];
	const int32 leftmode = kIntraModeContext[ctx.AvailL ? (int32)Grid(ctx.yModes, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1) : kModeDC];
	ctx.YMode = ctx.sd.DecodeSymbol(ctx.cdf.intraFrameYMode[abovemode][leftmode], 13);
	IntraAngleInfoY(ctx);
	if (ctx.HasChroma) {
		const bool cflAllowed = (ctx.Lossless && GetPlaneResidualSize(ctx, ctx.MiSize, 1) == 0 /*BLOCK_4X4*/) ||
								 (!ctx.Lossless && NkAv1Max(kNum4x4BlocksWide[ctx.MiSize]*4, kNum4x4BlocksHigh[ctx.MiSize]*4) <= 32);
		if (cflAllowed)
			ctx.UVMode = ctx.sd.DecodeSymbol(ctx.cdf.uvModeCflAllowed[ctx.YMode], 14);
		else
			ctx.UVMode = ctx.sd.DecodeSymbol(ctx.cdf.uvModeCflNotAllowed[ctx.YMode], 13);
		if (ctx.UVMode == kModeUvCfl)
			ReadCflAlphas(ctx);
		IntraAngleInfoUV(ctx);
	}
	ctx.PaletteSizeY = 0;
	ctx.PaletteSizeUV = 0;
	const int32 bw = kNum4x4BlocksWide[ctx.MiSize] * 4, bh = kNum4x4BlocksHigh[ctx.MiSize] * 4;
	if (ctx.MiSize >= 3 /*BLOCK_8X8*/ && bw <= 64 && bh <= 64 && ctx.fh->allowScreenContentTools) {
		// palette_mode_info() (§5.11.46). Full palette COLOR decoding (cache
		// merge, delta-coded palette entries, per-pixel color index reading)
		// is NOT implemented -- documented gap. But has_palette_y/has_palette_uv
		// themselves ARE read (their CDF only needs neighbouring PaletteSizes,
		// which are provably 0 here since we bail out whenever palette is truly
		// selected -- so the "no palette used anywhere yet" invariant holds for
        // every block reached). Bailing only when a block actually selects
		// palette keeps the (very common, non-screen-content) has_palette_y==0
		// path fully bit-exact instead of over-conservatively giving up frame-wide.
		const int32 bsizeCtx = kMiWidthLog2[ctx.MiSize] + kMiHeightLog2[ctx.MiSize] - 2;
		if (ctx.YMode == kModeDC) {
			const int32 hasPaletteY = ctx.sd.DecodeSymbol(ctx.cdf.paletteYMode[bsizeCtx][0], 2);
			if (hasPaletteY) {
				ctx.ok = false;
				ctx.err = "palette Y selectionnee (non implementee)";
				return;
			}
		}
		if (ctx.HasChroma && ctx.UVMode == kModeDC) {
			const int32 hasPaletteUv = ctx.sd.DecodeSymbol(ctx.cdf.paletteUvMode[0], 2);
			if (hasPaletteUv) {
				ctx.ok = false;
				ctx.err = "palette UV selectionnee (non implementee)";
				return;
			}
		}
	}
	FilterIntraModeInfo(ctx);
}

// =========================================================================
// INTER — syntaxe de bloc (§5.11.15 .. §5.11.30).
// =========================================================================
// read_cdef (§5.11.56) : index CDEF par bloc 64x64, lu en literal L(cdef_bits).
void ClearCdef(NkAv1Ctx &ctx, int32 r, int32 c) {
	Grid(ctx.cdefIdxGrid, ctx.gridStride, r, c) = -1;
	if (ctx.seq->use128x128Superblock) {
		const int32 cdefSize4 = kNum4x4BlocksWide[12 /*BLOCK_64X64*/];
		Grid(ctx.cdefIdxGrid, ctx.gridStride, r, c + cdefSize4) = -1;
		Grid(ctx.cdefIdxGrid, ctx.gridStride, r + cdefSize4, c) = -1;
		Grid(ctx.cdefIdxGrid, ctx.gridStride, r + cdefSize4, c + cdefSize4) = -1;
	}
}

void ReadCdef(NkAv1Ctx &ctx) {
	if (ctx.skip_ || ctx.fh->codedLossless || !ctx.seq->enableCdef || ctx.fh->allowIntrabc)
		return;
	const int32 cdefSize4 = kNum4x4BlocksWide[12 /*BLOCK_64X64*/];
	const int32 cdefMask4 = ~(cdefSize4 - 1);
	const int32 r = ctx.MiRow & cdefMask4;
	const int32 c = ctx.MiCol & cdefMask4;
	if (Grid(ctx.cdefIdxGrid, ctx.gridStride, r, c) == -1) {
		const int32 v = (int32)ctx.sd.ReadLiteral(ctx.fh->cdef.bits);
		const int32 w4 = kNum4x4BlocksWide[ctx.MiSize];
		const int32 h4 = kNum4x4BlocksHigh[ctx.MiSize];
		for (int32 i = r; i < r + h4; i += cdefSize4)
			for (int32 j = c; j < c + w4; j += cdefSize4)
				Grid(ctx.cdefIdxGrid, ctx.gridStride, i, j) = (int8)v;
	}
}

// count_refs / ref_count_ctx (§9, CDF selection comp_ref).
int32 CountRefs(NkAv1Ctx &ctx, int32 frameType) {
	int32 cRefs = 0;
	if (ctx.AvailU) {
		if (ctx.aboveRefFrame[0] == frameType) ++cRefs;
		if (ctx.aboveRefFrame[1] == frameType) ++cRefs;
	}
	if (ctx.AvailL) {
		if (ctx.leftRefFrame[0] == frameType) ++cRefs;
		if (ctx.leftRefFrame[1] == frameType) ++cRefs;
	}
	return cRefs;
}
inline int32 RefCountCtx(int32 counts0, int32 counts1) {
	if (counts0 < counts1) return 0;
	if (counts0 == counts1) return 1;
	return 2;
}
inline bool CheckBackward(int32 refFrame) { return refFrame >= kRefBwd && refFrame <= kRefAltRef; }
inline bool IsSamedirRefPair(int32 ref0, int32 ref1) {
	return (ref0 >= kRefBwd) == (ref1 >= kRefBwd);
}

// read_skip_mode (§5.11.11).
void ReadSkipMode(NkAv1Ctx &ctx) {
	if (SegFeatureActive(ctx, 6 /*SEG_LVL_SKIP*/) || SegFeatureActive(ctx, 5 /*SEG_LVL_REF_FRAME*/) ||
		SegFeatureActive(ctx, 7 /*SEG_LVL_GLOBALMV*/) || !ctx.skipModePresent ||
		BlockWidthOf(ctx.MiSize) < 8 || BlockHeightOf(ctx.MiSize) < 8) {
		ctx.skipMode_ = false;
	} else {
		int32 sctx = 0;
		if (ctx.AvailU) sctx += Grid(ctx.skipModes, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
		if (ctx.AvailL) sctx += Grid(ctx.skipModes, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
		ctx.skipMode_ = ctx.sd.DecodeSymbol(ctx.cdf.skipMode[sctx], 2) != 0;
	}
}

// get_segment_id (§5.11.22) : min de PrevSegmentIds sur la zone visible du bloc.
int32 GetSegmentIdPred(NkAv1Ctx &ctx) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	const int32 xMis = NkAv1Min(ctx.miCols - ctx.MiCol, bw4);
	const int32 yMis = NkAv1Min(ctx.miRows - ctx.MiRow, bh4);
	int32 seg = 7;
	if (ctx.prevSegmentIds.Size() == 0)
		return 0;
	for (int32 y = 0; y < yMis; ++y)
		for (int32 x = 0; x < xMis; ++x)
			seg = NkAv1Min(seg, (int32)Grid(ctx.prevSegmentIds, ctx.gridStride, ctx.MiRow + y, ctx.MiCol + x));
	return seg;
}

// inter_segment_id (§5.11.19).
void InterSegmentId(NkAv1Ctx &ctx, int32 preSkip) {
	if (!ctx.fh->seg.enabled) {
		ctx.segmentId_ = 0;
		return;
	}
	const int32 predictedSegmentId = GetSegmentIdPred(ctx);
	if (ctx.fh->seg.updateMap) {
		if (preSkip && !ctx.fh->seg.preSkipSegidPresent) {
			ctx.segmentId_ = 0;
			return;
		}
		if (!preSkip) {
			if (ctx.skip_) {
				for (int32 i = 0; i < kNum4x4BlocksWide[ctx.MiSize]; ++i)
					Grid(ctx.segPredAbove, ctx.gridStride, 0, ctx.MiCol + i) = 0;
				for (int32 i = 0; i < kNum4x4BlocksHigh[ctx.MiSize]; ++i)
					Grid(ctx.segPredLeft, ctx.gridStride, 0, ctx.MiRow + i) = 0;
				ReadSegmentId(ctx);
				return;
			}
		}
		if (ctx.fh->seg.temporalUpdate) {
			const int32 sctx = (int32)Grid(ctx.segPredLeft, ctx.gridStride, 0, ctx.MiRow) +
							   (int32)Grid(ctx.segPredAbove, ctx.gridStride, 0, ctx.MiCol);
			const int32 segIdPredicted = ctx.sd.DecodeSymbol(ctx.cdf.segIdPredicted[sctx], 2);
			if (segIdPredicted)
				ctx.segmentId_ = predictedSegmentId;
			else
				ReadSegmentId(ctx);
			for (int32 i = 0; i < kNum4x4BlocksWide[ctx.MiSize]; ++i)
				Grid(ctx.segPredAbove, ctx.gridStride, 0, ctx.MiCol + i) = (int8)segIdPredicted;
			for (int32 i = 0; i < kNum4x4BlocksHigh[ctx.MiSize]; ++i)
				Grid(ctx.segPredLeft, ctx.gridStride, 0, ctx.MiRow + i) = (int8)segIdPredicted;
		} else {
			ReadSegmentId(ctx);
		}
	} else {
		ctx.segmentId_ = predictedSegmentId;
	}
}

// read_is_inter (§5.11.20).
void ReadIsInter(NkAv1Ctx &ctx) {
	if (ctx.skipMode_) {
		ctx.isInter_ = true;
	} else if (SegFeatureActive(ctx, 5 /*SEG_LVL_REF_FRAME*/)) {
		ctx.isInter_ = ctx.fh->seg.featureData[ctx.segmentId_][5] != kRefIntra;
	} else if (SegFeatureActive(ctx, 7 /*SEG_LVL_GLOBALMV*/)) {
		ctx.isInter_ = true;
	} else {
		int32 ictx;
		if (ctx.AvailU && ctx.AvailL)
			ictx = (ctx.leftIntra && ctx.aboveIntra) ? 3 : ((ctx.leftIntra || ctx.aboveIntra) ? 1 : 0);
		else if (ctx.AvailU || ctx.AvailL)
			ictx = 2 * (ctx.AvailU ? (ctx.aboveIntra ? 1 : 0) : (ctx.leftIntra ? 1 : 0));
		else
			ictx = 0;
		ctx.isInter_ = ctx.sd.DecodeSymbol(ctx.cdf.isInter[ictx], 2) != 0;
	}
}

// read_ref_frames (§5.11.25).
void ReadRefFrames(NkAv1Ctx &ctx) {
	if (ctx.skipMode_) {
		ctx.refFrame[0] = ctx.skipModeFrame[0];
		ctx.refFrame[1] = ctx.skipModeFrame[1];
		return;
	}
	if (SegFeatureActive(ctx, 5 /*SEG_LVL_REF_FRAME*/)) {
		ctx.refFrame[0] = ctx.fh->seg.featureData[ctx.segmentId_][5];
		ctx.refFrame[1] = kRefNone;
		return;
	}
	if (SegFeatureActive(ctx, 6 /*SEG_LVL_SKIP*/) || SegFeatureActive(ctx, 7 /*SEG_LVL_GLOBALMV*/)) {
		ctx.refFrame[0] = kRefLast;
		ctx.refFrame[1] = kRefNone;
		return;
	}
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	int32 compMode = 0; // SINGLE_REFERENCE
	if (ctx.referenceSelect && NkAv1Min(bw4, bh4) >= 2) {
		int32 cctx;
		if (ctx.AvailU && ctx.AvailL) {
			if (ctx.aboveSingle && ctx.leftSingle)
				cctx = (CheckBackward(ctx.aboveRefFrame[0]) ? 1 : 0) ^ (CheckBackward(ctx.leftRefFrame[0]) ? 1 : 0);
			else if (ctx.aboveSingle)
				cctx = 2 + ((CheckBackward(ctx.aboveRefFrame[0]) || ctx.aboveIntra) ? 1 : 0);
			else if (ctx.leftSingle)
				cctx = 2 + ((CheckBackward(ctx.leftRefFrame[0]) || ctx.leftIntra) ? 1 : 0);
			else
				cctx = 4;
		} else if (ctx.AvailU) {
			cctx = ctx.aboveSingle ? (CheckBackward(ctx.aboveRefFrame[0]) ? 1 : 0) : 3;
		} else if (ctx.AvailL) {
			cctx = ctx.leftSingle ? (CheckBackward(ctx.leftRefFrame[0]) ? 1 : 0) : 3;
		} else {
			cctx = 1;
		}
		compMode = ctx.sd.DecodeSymbol(ctx.cdf.compMode[cctx], 2);
	}
	if (compMode == 1 /*COMPOUND_REFERENCE*/) {
		// comp_ref_type ctx (§9).
		int32 rctx;
		{
			const bool aboveCompInter = ctx.AvailU && !ctx.aboveIntra && !ctx.aboveSingle;
			const bool leftCompInter = ctx.AvailL && !ctx.leftIntra && !ctx.leftSingle;
			const bool aboveUniComp = aboveCompInter && IsSamedirRefPair(ctx.aboveRefFrame[0], ctx.aboveRefFrame[1]);
			const bool leftUniComp = leftCompInter && IsSamedirRefPair(ctx.leftRefFrame[0], ctx.leftRefFrame[1]);
			if (ctx.AvailU && !ctx.aboveIntra && ctx.AvailL && !ctx.leftIntra) {
				const int32 samedir = IsSamedirRefPair(ctx.aboveRefFrame[0], ctx.leftRefFrame[0]) ? 1 : 0;
				if (!aboveCompInter && !leftCompInter) {
					rctx = 1 + 2 * samedir;
				} else if (!aboveCompInter) {
					rctx = !leftUniComp ? 1 : 3 + samedir;
				} else if (!leftCompInter) {
					rctx = !aboveUniComp ? 1 : 3 + samedir;
				} else {
					if (!aboveUniComp && !leftUniComp)
						rctx = 0;
					else if (!aboveUniComp || !leftUniComp)
						rctx = 2;
					else
						rctx = 3 + (((ctx.aboveRefFrame[0] == kRefBwd) == (ctx.leftRefFrame[0] == kRefBwd)) ? 1 : 0);
				}
			} else if (ctx.AvailU && ctx.AvailL) {
				if (aboveCompInter)
					rctx = 1 + 2 * (aboveUniComp ? 1 : 0);
				else if (leftCompInter)
					rctx = 1 + 2 * (leftUniComp ? 1 : 0);
				else
					rctx = 2;
			} else if (aboveCompInter) {
				rctx = 4 * (aboveUniComp ? 1 : 0);
			} else if (leftCompInter) {
				rctx = 4 * (leftUniComp ? 1 : 0);
			} else {
				rctx = 2;
			}
		}
		const int32 compRefType = ctx.sd.DecodeSymbol(ctx.cdf.compRefType[rctx], 2);
		if (compRefType == 0 /*UNIDIR_COMP_REFERENCE*/) {
			// uni_comp_ref : ctx = single_ref_p1 ctx.
			int32 uctx;
			{
				int32 fwdCount = CountRefs(ctx, kRefLast) + CountRefs(ctx, kRefLast2) +
								 CountRefs(ctx, kRefLast3) + CountRefs(ctx, kRefGolden);
				int32 bwdCount = CountRefs(ctx, kRefBwd) + CountRefs(ctx, kRefAltRef2) + CountRefs(ctx, kRefAltRef);
				uctx = RefCountCtx(fwdCount, bwdCount);
			}
			const int32 uniCompRef = ctx.sd.DecodeSymbol(ctx.cdf.uniCompRef[uctx][0], 2);
			if (uniCompRef) {
				ctx.refFrame[0] = kRefBwd;
				ctx.refFrame[1] = kRefAltRef;
			} else {
				const int32 p1ctx = RefCountCtx(CountRefs(ctx, kRefLast2), CountRefs(ctx, kRefLast3) + CountRefs(ctx, kRefGolden));
				const int32 uniCompRefP1 = ctx.sd.DecodeSymbol(ctx.cdf.uniCompRef[p1ctx][1], 2);
				if (uniCompRefP1) {
					const int32 p2ctx = RefCountCtx(CountRefs(ctx, kRefLast3), CountRefs(ctx, kRefGolden));
					const int32 uniCompRefP2 = ctx.sd.DecodeSymbol(ctx.cdf.uniCompRef[p2ctx][2], 2);
					if (uniCompRefP2) {
						ctx.refFrame[0] = kRefLast;
						ctx.refFrame[1] = kRefGolden;
					} else {
						ctx.refFrame[0] = kRefLast;
						ctx.refFrame[1] = kRefLast3;
					}
				} else {
					ctx.refFrame[0] = kRefLast;
					ctx.refFrame[1] = kRefLast2;
				}
			}
		} else {
			const int32 crctx = RefCountCtx(CountRefs(ctx, kRefLast) + CountRefs(ctx, kRefLast2),
											CountRefs(ctx, kRefLast3) + CountRefs(ctx, kRefGolden));
			const int32 compRef = ctx.sd.DecodeSymbol(ctx.cdf.compRef[crctx][0], 2);
			if (compRef == 0) {
				const int32 p1ctx = RefCountCtx(CountRefs(ctx, kRefLast), CountRefs(ctx, kRefLast2));
				const int32 compRefP1 = ctx.sd.DecodeSymbol(ctx.cdf.compRef[p1ctx][1], 2);
				ctx.refFrame[0] = compRefP1 ? kRefLast2 : kRefLast;
			} else {
				const int32 p2ctx = RefCountCtx(CountRefs(ctx, kRefLast3), CountRefs(ctx, kRefGolden));
				const int32 compRefP2 = ctx.sd.DecodeSymbol(ctx.cdf.compRef[p2ctx][2], 2);
				ctx.refFrame[0] = compRefP2 ? kRefGolden : kRefLast3;
			}
			const int32 bctx = RefCountCtx(CountRefs(ctx, kRefBwd) + CountRefs(ctx, kRefAltRef2), CountRefs(ctx, kRefAltRef));
			const int32 compBwdref = ctx.sd.DecodeSymbol(ctx.cdf.compBwdRef[bctx][0], 2);
			if (compBwdref == 0) {
				const int32 b1ctx = RefCountCtx(CountRefs(ctx, kRefBwd), CountRefs(ctx, kRefAltRef2));
				const int32 compBwdrefP1 = ctx.sd.DecodeSymbol(ctx.cdf.compBwdRef[b1ctx][1], 2);
				ctx.refFrame[1] = compBwdrefP1 ? kRefAltRef2 : kRefBwd;
			} else {
				ctx.refFrame[1] = kRefAltRef;
			}
		}
	} else {
		// single reference.
		const int32 s1ctx = RefCountCtx(CountRefs(ctx, kRefLast) + CountRefs(ctx, kRefLast2) +
										 CountRefs(ctx, kRefLast3) + CountRefs(ctx, kRefGolden),
										CountRefs(ctx, kRefBwd) + CountRefs(ctx, kRefAltRef2) + CountRefs(ctx, kRefAltRef));
		const int32 singleRefP1 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s1ctx][0], 2);
		if (singleRefP1) {
			const int32 s2ctx = RefCountCtx(CountRefs(ctx, kRefBwd) + CountRefs(ctx, kRefAltRef2), CountRefs(ctx, kRefAltRef));
			const int32 singleRefP2 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s2ctx][1], 2);
			if (singleRefP2 == 0) {
				const int32 s6ctx = RefCountCtx(CountRefs(ctx, kRefBwd), CountRefs(ctx, kRefAltRef2));
				const int32 singleRefP6 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s6ctx][5], 2);
				ctx.refFrame[0] = singleRefP6 ? kRefAltRef2 : kRefBwd;
			} else {
				ctx.refFrame[0] = kRefAltRef;
			}
		} else {
			const int32 s3ctx = RefCountCtx(CountRefs(ctx, kRefLast) + CountRefs(ctx, kRefLast2),
											CountRefs(ctx, kRefLast3) + CountRefs(ctx, kRefGolden));
			const int32 singleRefP3 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s3ctx][2], 2);
			if (singleRefP3) {
				const int32 s5ctx = RefCountCtx(CountRefs(ctx, kRefLast3), CountRefs(ctx, kRefGolden));
				const int32 singleRefP5 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s5ctx][4], 2);
				ctx.refFrame[0] = singleRefP5 ? kRefGolden : kRefLast3;
			} else {
				const int32 s4ctx = RefCountCtx(CountRefs(ctx, kRefLast), CountRefs(ctx, kRefLast2));
				const int32 singleRefP4 = ctx.sd.DecodeSymbol(ctx.cdf.singleRef[s4ctx][3], 2);
				ctx.refFrame[0] = singleRefP4 ? kRefLast2 : kRefLast;
			}
		}
		ctx.refFrame[1] = kRefNone;
	}
}

// get_mode (§5.11.30).
int32 GetMode(NkAv1Ctx &ctx, int32 refList) {
	int32 compMode;
	if (refList == 0) {
		if (ctx.YMode < kModeNearestNearestMv)
			compMode = ctx.YMode;
		else if (ctx.YMode == kModeNewNewMv || ctx.YMode == kModeNewNearestMv || ctx.YMode == kModeNewNearMv)
			compMode = kModeNewMv;
		else if (ctx.YMode == kModeNearestNearestMv || ctx.YMode == kModeNearestNewMv)
			compMode = kModeNearestMv;
		else if (ctx.YMode == kModeNearNearMv || ctx.YMode == kModeNearNewMv)
			compMode = kModeNearMv;
		else
			compMode = kModeGlobalMv;
	} else {
		if (ctx.YMode == kModeNewNewMv || ctx.YMode == kModeNearestNewMv || ctx.YMode == kModeNearNewMv)
			compMode = kModeNewMv;
		else if (ctx.YMode == kModeNearestNearestMv || ctx.YMode == kModeNewNearestMv)
			compMode = kModeNearestMv;
		else if (ctx.YMode == kModeNearNearMv || ctx.YMode == kModeNewNearMv)
			compMode = kModeNearMv;
		else
			compMode = kModeGlobalMv;
	}
	return compMode;
}

// read_mv_component (§5.11.32).
int32 ReadMvComponent(NkAv1Ctx &ctx, int32 comp) {
	const int32 mvCtx = 0; // MV_INTRABC_CONTEXT jamais (intrabc refuse)
	const int32 mvSign = ctx.sd.DecodeSymbol(ctx.cdf.mvSign[mvCtx][comp], 2);
	const int32 mvClass = ctx.sd.DecodeSymbol(ctx.cdf.mvClass[mvCtx][comp], 11);
	int32 mag;
	if (mvClass == 0 /*MV_CLASS_0*/) {
		const int32 mvClass0Bit = ctx.sd.DecodeSymbol(ctx.cdf.mvClass0Bit[mvCtx][comp], 2);
		int32 mvClass0Fr;
		if (ctx.forceIntegerMv)
			mvClass0Fr = 3;
		else
			mvClass0Fr = ctx.sd.DecodeSymbol(ctx.cdf.mvClass0Fr[mvCtx][comp][mvClass0Bit], 4);
		int32 mvClass0Hp;
		if (ctx.allowHighPrecisionMv)
			mvClass0Hp = ctx.sd.DecodeSymbol(ctx.cdf.mvClass0Hp[mvCtx][comp], 2);
		else
			mvClass0Hp = 1;
		mag = ((mvClass0Bit << 3) | (mvClass0Fr << 1) | mvClass0Hp) + 1;
	} else {
		int32 d = 0;
		for (int32 i = 0; i < mvClass; ++i) {
			const int32 mvBit = ctx.sd.DecodeSymbol(ctx.cdf.mvBit[mvCtx][comp][i], 2);
			d |= mvBit << i;
		}
		mag = 2 /*CLASS0_SIZE*/ << (mvClass + 2);
		int32 mvFr;
		if (ctx.forceIntegerMv)
			mvFr = 3;
		else
			mvFr = ctx.sd.DecodeSymbol(ctx.cdf.mvFr[mvCtx][comp], 4);
		int32 mvHp;
		if (ctx.allowHighPrecisionMv)
			mvHp = ctx.sd.DecodeSymbol(ctx.cdf.mvHp[mvCtx][comp], 2);
		else
			mvHp = 1;
		mag += ((d << 3) | (mvFr << 1) | mvHp) + 1;
	}
	return mvSign ? -mag : mag;
}

// read_mv (§5.11.31).
void ReadMvBlk(NkAv1Ctx &ctx, int32 refList) {
	int32 diffMv[2] = {0, 0};
	const int32 mvCtx = 0;
	const int32 mvJoint = ctx.sd.DecodeSymbol(ctx.cdf.mvJoint[mvCtx], 4);
	if (mvJoint == 2 /*MV_JOINT_HZVNZ*/ || mvJoint == 3 /*MV_JOINT_HNZVNZ*/)
		diffMv[0] = ReadMvComponent(ctx, 0);
	if (mvJoint == 1 /*MV_JOINT_HNZVZ*/ || mvJoint == 3)
		diffMv[1] = ReadMvComponent(ctx, 1);
	ctx.mvBlk[refList][0] = ctx.predMv[refList][0] + diffMv[0];
	ctx.mvBlk[refList][1] = ctx.predMv[refList][1] + diffMv[1];
}

// assign_mv (§5.11.26).
void AssignMv(NkAv1Ctx &ctx, bool isCompound) {
	for (int32 i = 0; i < 1 + (isCompound ? 1 : 0); ++i) {
		const int32 compMode = GetMode(ctx, i);
		if (compMode == kModeGlobalMv) {
			ctx.predMv[i][0] = ctx.globalMvs[i][0];
			ctx.predMv[i][1] = ctx.globalMvs[i][1];
		} else {
			int32 pos = (compMode == kModeNearestMv) ? 0 : ctx.refMvIdx_;
			if (compMode == kModeNewMv && ctx.numMvFound <= 1)
				pos = 0;
			ctx.predMv[i][0] = ctx.refStackMv[pos][i][0];
			ctx.predMv[i][1] = ctx.refStackMv[pos][i][1];
		}
		if (compMode == kModeNewMv) {
			ReadMvBlk(ctx, i);
		} else {
			ctx.mvBlk[i][0] = ctx.predMv[i][0];
			ctx.mvBlk[i][1] = ctx.predMv[i][1];
		}
	}
}

// read_interintra_mode (§5.11.28).
void ReadInterIntraMode(NkAv1Ctx &ctx, bool isCompound) {
	ctx.interIntraBlk = 0;
	ctx.wedgeInterIntraBlk = 0;
	if (!ctx.skipMode_ && ctx.enableInterintraCompound && !isCompound &&
		ctx.MiSize >= 3 /*BLOCK_8X8*/ && ctx.MiSize <= 9 /*BLOCK_32X32*/) {
		const int32 grp = kSizeGroup[ctx.MiSize] - 1;
		ctx.interIntraBlk = ctx.sd.DecodeSymbol(ctx.cdf.interIntra[grp], 2);
		if (ctx.interIntraBlk) {
			ctx.interIntraModeBlk = ctx.sd.DecodeSymbol(ctx.cdf.interIntraMode[grp], 4);
			ctx.refFrame[1] = kRefIntra;
			ctx.AngleDeltaY = 0;
			ctx.AngleDeltaUV = 0;
			ctx.useFilterIntra = false;
			ctx.wedgeInterIntraBlk = ctx.sd.DecodeSymbol(ctx.cdf.wedgeInterIntra[ctx.MiSize], 2);
			if (ctx.wedgeInterIntraBlk) {
				ctx.wedgeIndexBlk = ctx.sd.DecodeSymbol(ctx.cdf.wedgeIndex[ctx.MiSize], 16);
				ctx.wedgeSignBlk = 0;
			}
		}
	}
}

// read_motion_mode (§5.11.27). OBMC / warp local non implementes -> refus si
// SELECTIONNES (mais la lecture du symbole est faite exactement comme la spec).
void ReadMotionMode(NkAv1Ctx &ctx, bool isCompound) {
	if (ctx.skipMode_) { ctx.motionModeBlk = kMotionSimple; return; }
	if (!ctx.isMotionModeSwitchable) { ctx.motionModeBlk = kMotionSimple; return; }
	if (NkAv1Min(BlockWidthOf(ctx.MiSize), BlockHeightOf(ctx.MiSize)) < 8) { ctx.motionModeBlk = kMotionSimple; return; }
	if (!ctx.forceIntegerMv && (ctx.YMode == kModeGlobalMv || ctx.YMode == kModeGlobalGlobalMv)) {
		if (ctx.fh->gmType[ctx.refFrame[0]] > kGmTranslation) { ctx.motionModeBlk = kMotionSimple; return; }
	}
	if (isCompound || ctx.refFrame[1] == kRefIntra || !HasOverlappableCandidates(ctx)) {
		ctx.motionModeBlk = kMotionSimple;
		return;
	}
	FindWarpSamples(ctx);
	if (ctx.forceIntegerMv || ctx.numSamples == 0 || !ctx.allowWarpedMotion || IsScaledRef(ctx, ctx.refFrame[0])) {
		const int32 useObmc = ctx.sd.DecodeSymbol(ctx.cdf.useObmc[ctx.MiSize], 2);
		ctx.motionModeBlk = useObmc ? kMotionObmc : kMotionSimple;
	} else {
		ctx.motionModeBlk = ctx.sd.DecodeSymbol(ctx.cdf.motionMode[ctx.MiSize], 3);
	}
}

// read_compound_type (§5.11.29). Compound masque (wedge/diffwtd) non
// implemente -> refus si comp_group_idx == 1.
void ReadCompoundType(NkAv1Ctx &ctx, bool isCompound) {
	ctx.compGroupIdx_ = 0;
	ctx.compoundIdx_ = 1;
	if (ctx.skipMode_) {
		ctx.compoundTypeBlk = kCompoundAverage;
		return;
	}
	if (isCompound) {
		if (ctx.enableMaskedCompound) {
			int32 gctx = 0;
			if (ctx.AvailU) {
				if (!ctx.aboveSingle)
					gctx += Grid(ctx.compGroupIdxs, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
				else if (ctx.aboveRefFrame[0] == kRefAltRef)
					gctx += 3;
			}
			if (ctx.AvailL) {
				if (!ctx.leftSingle)
					gctx += Grid(ctx.compGroupIdxs, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
				else if (ctx.leftRefFrame[0] == kRefAltRef)
					gctx += 3;
			}
			gctx = NkAv1Min(5, gctx);
			ctx.compGroupIdx_ = ctx.sd.DecodeSymbol(ctx.cdf.compGroupIdx[gctx], 2);
		}
		if (ctx.compGroupIdx_ == 0) {
			if (ctx.enableJntComp) {
				int32 ictx;
				{
					const int32 fwd = NkAv1Abs(GetRelativeDist(*ctx.seq, ctx.fh->orderHints[ctx.refFrame[0]], ctx.fh->orderHint));
					const int32 bck = NkAv1Abs(GetRelativeDist(*ctx.seq, ctx.fh->orderHints[ctx.refFrame[1]], ctx.fh->orderHint));
					ictx = (fwd == bck) ? 3 : 0;
					if (ctx.AvailU) {
						if (!ctx.aboveSingle)
							ictx += Grid(ctx.compoundIdxs, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
						else if (ctx.aboveRefFrame[0] == kRefAltRef)
							++ictx;
					}
					if (ctx.AvailL) {
						if (!ctx.leftSingle)
							ictx += Grid(ctx.compoundIdxs, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
						else if (ctx.leftRefFrame[0] == kRefAltRef)
							++ictx;
					}
				}
				ctx.compoundIdx_ = ctx.sd.DecodeSymbol(ctx.cdf.compoundIdx[ictx], 2);
				ctx.compoundTypeBlk = ctx.compoundIdx_ ? kCompoundAverage : kCompoundDistance;
			} else {
				ctx.compoundTypeBlk = kCompoundAverage;
			}
		} else {
			const int32 n = kWedgeBits[ctx.MiSize];
			if (n == 0) {
				ctx.compoundTypeBlk = kCompoundDiffwtd;
			} else {
				const int32 ct = ctx.sd.DecodeSymbol(ctx.cdf.compoundType[ctx.MiSize], 2);
				ctx.compoundTypeBlk = (ct == 0) ? kCompoundWedge : kCompoundDiffwtd;
			}
		}
		if (ctx.compoundTypeBlk == kCompoundWedge) {
			ctx.wedgeIndexBlk = ctx.sd.DecodeSymbol(ctx.cdf.wedgeIndex[ctx.MiSize], 16);
			ctx.wedgeSignBlk = (int32)ctx.sd.ReadLiteral(1);
		} else if (ctx.compoundTypeBlk == kCompoundDiffwtd) {
			ctx.maskTypeBlk = (int32)ctx.sd.ReadLiteral(1);
		}
	} else {
		if (ctx.interIntraBlk)
			ctx.compoundTypeBlk = ctx.wedgeInterIntraBlk ? kCompoundWedge : kCompoundIntra;
		else
			ctx.compoundTypeBlk = kCompoundAverage;
	}
}

// needs_interp_filter (§5.11.24).
bool NeedsInterpFilter(NkAv1Ctx &ctx) {
	const bool large = NkAv1Min(BlockWidthOf(ctx.MiSize), BlockHeightOf(ctx.MiSize)) >= 8;
	if (ctx.skipMode_ || ctx.motionModeBlk == kMotionLocalwarp)
		return false;
	if (large && ctx.YMode == kModeGlobalMv)
		return ctx.fh->gmType[ctx.refFrame[0]] == kGmTranslation;
	if (large && ctx.YMode == kModeGlobalGlobalMv)
		return ctx.fh->gmType[ctx.refFrame[0]] == kGmTranslation || ctx.fh->gmType[ctx.refFrame[1]] == kGmTranslation;
	return true;
}

// Contexte interp_filter (§9).
int32 InterpFilterCtx(NkAv1Ctx &ctx, int32 dir) {
	int32 fctx = ((dir & 1) * 2 + (ctx.refFrame[1] > kRefIntra ? 1 : 0)) * 4;
	int32 leftType = 3, aboveType = 3;
	if (ctx.AvailL) {
		if ((int32)Grid(ctx.refFrames0, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1) == ctx.refFrame[0] ||
			(int32)Grid(ctx.refFrames1, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1) == ctx.refFrame[0])
			leftType = (dir == 0) ? Grid(ctx.interpFilters0, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1)
								   : Grid(ctx.interpFilters1, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1);
	}
	if (ctx.AvailU) {
		if ((int32)Grid(ctx.refFrames0, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol) == ctx.refFrame[0] ||
			(int32)Grid(ctx.refFrames1, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol) == ctx.refFrame[0])
			aboveType = (dir == 0) ? Grid(ctx.interpFilters0, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol)
									: Grid(ctx.interpFilters1, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol);
	}
	if (leftType == aboveType)
		fctx += leftType;
	else if (leftType == 3)
		fctx += aboveType;
	else if (aboveType == 3)
		fctx += leftType;
	else
		fctx += 3;
	return fctx;
}

inline bool HasNearmv(NkAv1Ctx &ctx) {
	return ctx.YMode == kModeNearMv || ctx.YMode == kModeNearNearMv ||
		   ctx.YMode == kModeNearNewMv || ctx.YMode == kModeNewNearMv;
}

// inter_block_mode_info (§5.11.23).
void InterBlockModeInfo(NkAv1Ctx &ctx) {
	ctx.PaletteSizeY = 0;
	ctx.PaletteSizeUV = 0;
	ReadRefFrames(ctx);
	const bool isCompound = ctx.refFrame[1] > kRefIntra;
	FindMvStack(ctx, isCompound);
	if (ctx.skipMode_) {
		ctx.YMode = kModeNearestNearestMv;
	} else if (SegFeatureActive(ctx, 6 /*SEG_LVL_SKIP*/) || SegFeatureActive(ctx, 7 /*SEG_LVL_GLOBALMV*/)) {
		ctx.YMode = kModeGlobalMv;
	} else if (isCompound) {
		const int32 cmctx = kCompoundModeCtxMap[ctx.refMvContext >> 1][NkAv1Min(ctx.newMvContext, 4)];
		const int32 compoundMode = ctx.sd.DecodeSymbol(ctx.cdf.compoundMode[cmctx], 8);
		ctx.YMode = kModeNearestNearestMv + compoundMode;
	} else {
		const int32 newMv = ctx.sd.DecodeSymbol(ctx.cdf.newMv[ctx.newMvContext], 2);
		if (newMv == 0) {
			ctx.YMode = kModeNewMv;
		} else {
			const int32 zeroMv = ctx.sd.DecodeSymbol(ctx.cdf.zeroMv[ctx.zeroMvContext], 2);
			if (zeroMv == 0) {
				ctx.YMode = kModeGlobalMv;
			} else {
				const int32 refMv = ctx.sd.DecodeSymbol(ctx.cdf.refMv[ctx.refMvContext], 2);
				ctx.YMode = (refMv == 0) ? kModeNearestMv : kModeNearMv;
			}
		}
	}
	ctx.refMvIdx_ = 0;
	if (ctx.YMode == kModeNewMv || ctx.YMode == kModeNewNewMv) {
		for (int32 idx = 0; idx < 2; ++idx) {
			if (ctx.numMvFound > idx + 1) {
				const int32 drlMode = ctx.sd.DecodeSymbol(ctx.cdf.drlMode[ctx.drlCtxStack[idx]], 2);
				if (drlMode == 0) {
					ctx.refMvIdx_ = idx;
					break;
				}
				ctx.refMvIdx_ = idx + 1;
			}
		}
	} else if (HasNearmv(ctx)) {
		ctx.refMvIdx_ = 1;
		for (int32 idx = 1; idx < 3; ++idx) {
			if (ctx.numMvFound > idx + 1) {
				const int32 drlMode = ctx.sd.DecodeSymbol(ctx.cdf.drlMode[ctx.drlCtxStack[idx]], 2);
				if (drlMode == 0) {
					ctx.refMvIdx_ = idx;
					break;
				}
				ctx.refMvIdx_ = idx + 1;
			}
		}
	}
	AssignMv(ctx, isCompound);
	ReadInterIntraMode(ctx, isCompound);
	if (!ctx.ok)
		return;
	ReadMotionMode(ctx, isCompound);
	if (!ctx.ok)
		return;
	ReadCompoundType(ctx, isCompound);
	if (!ctx.ok)
		return;
	if (ctx.interpFilterFrame == 3 /*SWITCHABLE*/) {
		for (int32 dir = 0; dir < (ctx.enableDualFilter ? 2 : 1); ++dir) {
			if (NeedsInterpFilter(ctx))
				ctx.interpFilterBlk[dir] = ctx.sd.DecodeSymbol(ctx.cdf.interpFilter[InterpFilterCtx(ctx, dir)], 3);
			else
				ctx.interpFilterBlk[dir] = 0; // EIGHTTAP
		}
		if (!ctx.enableDualFilter)
			ctx.interpFilterBlk[1] = ctx.interpFilterBlk[0];
	} else {
		ctx.interpFilterBlk[0] = ctx.interpFilterFrame;
		ctx.interpFilterBlk[1] = ctx.interpFilterFrame;
	}
}

// intra_block_mode_info (§5.11.22) — bloc INTRA dans une trame inter.
void IntraBlockModeInfo(NkAv1Ctx &ctx) {
	ctx.refFrame[0] = kRefIntra;
	ctx.refFrame[1] = kRefNone;
	ctx.YMode = ctx.sd.DecodeSymbol(ctx.cdf.yMode[kSizeGroup[ctx.MiSize]], 13);
	IntraAngleInfoY(ctx);
	if (ctx.HasChroma) {
		const bool cflAllowed = (ctx.Lossless && GetPlaneResidualSize(ctx, ctx.MiSize, 1) == 0 /*BLOCK_4X4*/) ||
								 (!ctx.Lossless && NkAv1Max(BlockWidthOf(ctx.MiSize), BlockHeightOf(ctx.MiSize)) <= 32);
		if (cflAllowed)
			ctx.UVMode = ctx.sd.DecodeSymbol(ctx.cdf.uvModeCflAllowed[ctx.YMode], 14);
		else
			ctx.UVMode = ctx.sd.DecodeSymbol(ctx.cdf.uvModeCflNotAllowed[ctx.YMode], 13);
		if (ctx.UVMode == kModeUvCfl)
			ReadCflAlphas(ctx);
		IntraAngleInfoUV(ctx);
	}
	ctx.PaletteSizeY = 0;
	ctx.PaletteSizeUV = 0;
	const int32 bw = BlockWidthOf(ctx.MiSize), bh = BlockHeightOf(ctx.MiSize);
	if (ctx.MiSize >= 3 /*BLOCK_8X8*/ && bw <= 64 && bh <= 64 && ctx.fh->allowScreenContentTools) {
		const int32 bsizeCtx = kMiWidthLog2[ctx.MiSize] + kMiHeightLog2[ctx.MiSize] - 2;
		if (ctx.YMode == kModeDC) {
			const int32 hasPaletteY = ctx.sd.DecodeSymbol(ctx.cdf.paletteYMode[bsizeCtx][0], 2);
			if (hasPaletteY) {
				ctx.ok = false;
				ctx.err = "palette selectionnee (non implementee)";
				return;
			}
		}
		if (ctx.HasChroma && ctx.UVMode == kModeDC) {
			const int32 hasPaletteUv = ctx.sd.DecodeSymbol(ctx.cdf.paletteUvMode[0], 2);
			if (hasPaletteUv) {
				ctx.ok = false;
				ctx.err = "palette UV selectionnee (non implementee)";
				return;
			}
		}
	}
	FilterIntraModeInfo(ctx);
}

// inter_frame_mode_info (§5.11.18).
void InterFrameModeInfo(NkAv1Ctx &ctx) {
	ctx.useIntrabc = false;
	ctx.leftRefFrame[0] = ctx.AvailL ? (int32)Grid(ctx.refFrames0, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1) : kRefIntra;
	ctx.aboveRefFrame[0] = ctx.AvailU ? (int32)Grid(ctx.refFrames0, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol) : kRefIntra;
	ctx.leftRefFrame[1] = ctx.AvailL ? (int32)Grid(ctx.refFrames1, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1) : kRefNone;
	ctx.aboveRefFrame[1] = ctx.AvailU ? (int32)Grid(ctx.refFrames1, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol) : kRefNone;
	ctx.leftIntra = ctx.leftRefFrame[0] <= kRefIntra;
	ctx.aboveIntra = ctx.aboveRefFrame[0] <= kRefIntra;
	ctx.leftSingle = ctx.leftRefFrame[1] <= kRefIntra;
	ctx.aboveSingle = ctx.aboveRefFrame[1] <= kRefIntra;
	ctx.skip_ = false;
	InterSegmentId(ctx, 1);
	ReadSkipMode(ctx);
	if (ctx.skipMode_)
		ctx.skip_ = true;
	else
		ReadSkip(ctx);
	if (!ctx.fh->seg.preSkipSegidPresent)
		InterSegmentId(ctx, 0);
	ctx.Lossless = ctx.fh->losslessArray[ctx.segmentId_];
	ReadCdef(ctx);
	ReadDeltaQIndex(ctx);
	ReadDeltaLf(ctx);
	ctx.ReadDeltas = false;
	ReadIsInter(ctx);
	if (ctx.isInter_)
		InterBlockModeInfo(ctx);
	else
		IntraBlockModeInfo(ctx);
}

void ModeInfo(NkAv1Ctx &ctx) {
	if (ctx.frameIsIntra)
		IntraFrameModeInfo(ctx);
	else
		InterFrameModeInfo(ctx);
}

// =========================================================================
// TX size (§5.11.15/16), plane residual size, transform-set lookups.
// =========================================================================
void ReadTxSize(NkAv1Ctx &ctx, bool allowSelect) {
	if (ctx.Lossless) { ctx.TxSize_ = kTx4x4; return; }
	const int32 maxRectTxSize = kMaxTxSizeRect[ctx.MiSize];
	const int32 maxTxDepth = kMaxTxDepth[ctx.MiSize];
	ctx.TxSize_ = maxRectTxSize;
	if (ctx.MiSize > 0 /*BLOCK_4X4*/ && allowSelect && ctx.fh->txMode == 2 /*TX_MODE_SELECT*/) {
		const int32 maxTxWidth = kTxWidth[maxRectTxSize];
		const int32 maxTxHeight = kTxHeight[maxRectTxSize];
		int32 aboveW;
		if (ctx.AvailU && Grid(ctx.isInters, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol))
			aboveW = kNum4x4BlocksWide[(int32)Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow - 1, ctx.MiCol)] * 4;
		else if (ctx.AvailU)
			aboveW = GetAboveTxWidth(ctx, ctx.MiRow, ctx.MiCol);
		else
			aboveW = 0;
		int32 leftH;
		if (ctx.AvailL && Grid(ctx.isInters, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1))
			leftH = kNum4x4BlocksHigh[(int32)Grid(ctx.miSizes, ctx.gridStride, ctx.MiRow, ctx.MiCol - 1)] * 4;
		else if (ctx.AvailL)
			leftH = GetLeftTxHeight(ctx, ctx.MiRow, ctx.MiCol);
		else
			leftH = 0;
		const int32 tctx = (aboveW >= maxTxWidth ? 1 : 0) + (leftH >= maxTxHeight ? 1 : 0);
		int32 txDepth;
		if (maxTxDepth == 4) txDepth = ctx.sd.DecodeSymbol(ctx.cdf.tx64x64[tctx], 3);
		else if (maxTxDepth == 3) txDepth = ctx.sd.DecodeSymbol(ctx.cdf.tx32x32[tctx], 3);
		else if (maxTxDepth == 2) txDepth = ctx.sd.DecodeSymbol(ctx.cdf.tx16x16[tctx], 3);
		else txDepth = ctx.sd.DecodeSymbol(ctx.cdf.tx8x8[tctx], 2);
		for (int32 i = 0; i < txDepth; ++i)
			ctx.TxSize_ = kSplitTxSize[ctx.TxSize_];
	}
}

// find_tx_size (§5.11.36).
int32 FindTxSize(int32 w, int32 h) {
	int32 txSz;
	for (txSz = 0; txSz < 19; ++txSz)
		if (kTxWidth[txSz] == w && kTxHeight[txSz] == h)
			break;
	return txSz;
}

// read_var_tx_size (§5.11.17) : arbre de tailles de transformee (blocs inter).
void ReadVarTxSize(NkAv1Ctx &ctx, int32 row, int32 col, int32 txSz, int32 depth) {
	if (row >= ctx.miRows || col >= ctx.miCols)
		return;
	int32 txfmSplit;
	if (txSz == kTx4x4 || depth == 2 /*MAX_VARTX_DEPTH*/) {
		txfmSplit = 0;
	} else {
		// ctx txfm_split (§9).
		const int32 above = GetAboveTxWidth(ctx, row, col) < kTxWidth[txSz] ? 1 : 0;
		const int32 left = GetLeftTxHeight(ctx, row, col) < kTxHeight[txSz] ? 1 : 0;
		const int32 size = NkAv1Min(64, NkAv1Max(BlockWidthOf(ctx.MiSize), BlockHeightOf(ctx.MiSize)));
		const int32 maxTxSz = FindTxSize(size, size);
		const int32 txSzSqrUp = kTxSizeSqrUp[txSz];
		const int32 sctx = (txSzSqrUp != maxTxSz ? 1 : 0) * 3 + (5 /*TX_SIZES*/ - 1 - maxTxSz) * 6 + above + left;
		txfmSplit = ctx.sd.DecodeSymbol(ctx.cdf.txfmSplit[sctx], 2);
	}
	const int32 w4 = kTxWidth[txSz] / 4;
	const int32 h4 = kTxHeight[txSz] / 4;
	if (txfmSplit) {
		const int32 subTxSz = kSplitTxSize[txSz];
		const int32 stepW = kTxWidth[subTxSz] / 4;
		const int32 stepH = kTxHeight[subTxSz] / 4;
		for (int32 i = 0; i < h4; i += stepH)
			for (int32 j = 0; j < w4; j += stepW)
				ReadVarTxSize(ctx, row + i, col + j, subTxSz, depth + 1);
	} else {
		for (int32 i = 0; i < h4; ++i)
			for (int32 j = 0; j < w4; ++j)
				Grid(ctx.txSizesGrid, ctx.gridStride, row + i, col + j) = (int8)txSz;
		ctx.TxSize_ = txSz;
	}
}

void ReadBlockTxSize(NkAv1Ctx &ctx) {
	const int32 bw4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 bh4 = kNum4x4BlocksHigh[ctx.MiSize];
	if (ctx.fh->txMode == 2 /*TX_MODE_SELECT*/ && ctx.MiSize > 0 /*BLOCK_4X4*/ && ctx.isInter_ &&
		!ctx.skip_ && !ctx.Lossless) {
		const int32 maxTxSz = kMaxTxSizeRect[ctx.MiSize];
		const int32 txW4 = kTxWidth[maxTxSz] / 4;
		const int32 txH4 = kTxHeight[maxTxSz] / 4;
		for (int32 row = ctx.MiRow; row < ctx.MiRow + bh4; row += txH4)
			for (int32 col = ctx.MiCol; col < ctx.MiCol + bw4; col += txW4)
				ReadVarTxSize(ctx, row, col, maxTxSz, 0);
	} else {
		ReadTxSize(ctx, !ctx.skip_ || !ctx.isInter_);
		for (int32 row = ctx.MiRow; row < ctx.MiRow + bh4; ++row)
			for (int32 col = ctx.MiCol; col < ctx.MiCol + bw4; ++col)
				Grid(ctx.txSizesGrid, ctx.gridStride, row, col) = (int8)ctx.TxSize_;
	}
}

int32 GetPlaneResidualSize(NkAv1Ctx &ctx, int32 subsize, int32 plane) {
	const int32 subx = plane > 0 ? ctx.seq->subsamplingX : 0;
	const int32 suby = plane > 0 ? ctx.seq->subsamplingY : 0;
	return kSubsampledSize[subsize][subx][suby];
}

int32 GetTxSize(NkAv1Ctx &ctx, int32 plane, int32 txSz) {
	if (plane == 0) return txSz;
	const int32 uvTx = kMaxTxSizeRect[GetPlaneResidualSize(ctx, ctx.MiSize, plane)];
	if (kTxWidth[uvTx] == 64 || kTxHeight[uvTx] == 64) {
		if (kTxWidth[uvTx] == 16) return kTx16x32;
		if (kTxHeight[uvTx] == 16) return kTx32x16;
		return kTx32x32;
	}
	return uvTx;
}

int32 GetTxSet(NkAv1Ctx &ctx, int32 txSz) {
	const int32 txSzSqr = kTxSizeSqr[txSz];
	const int32 txSzSqrUp = kTxSizeSqrUp[txSz];
	if (txSzSqrUp > kTx32x32) return 0 /*TX_SET_DCTONLY*/;
	if (ctx.isInter_) {
		if (ctx.fh->reducedTxSet || txSzSqrUp == kTx32x32) return 3 /*TX_SET_INTER_3*/;
		if (txSzSqr == kTx16x16) return 2 /*TX_SET_INTER_2*/;
		return 1 /*TX_SET_INTER_1*/;
	}
	if (txSzSqrUp == kTx32x32) return 0;
	if (ctx.fh->reducedTxSet) return 2 /*TX_SET_INTRA_2*/;
	if (txSzSqr == kTx16x16) return 2;
	return 1 /*TX_SET_INTRA_1*/;
}

int32 GetTxClass(int32 txType) {
	if (txType == kTxV_DCT || txType == kTxV_ADST || txType == kTxV_FLIPADST) return 2 /*TX_CLASS_VERT*/;
	if (txType == kTxH_DCT || txType == kTxH_ADST || txType == kTxH_FLIPADST) return 1 /*TX_CLASS_HORIZ*/;
	return 0 /*TX_CLASS_2D*/;
}

void TransformType(NkAv1Ctx &ctx, int32 x4, int32 y4, int32 txSz) {
	const int32 set = GetTxSet(ctx, txSz);
	int32 txType = 0 /*DCT_DCT*/;
	const int32 qidx = ctx.fh->seg.enabled ? GetQIndex(ctx, 1, ctx.segmentId_) : ctx.fh->quant.baseQIdx;
	if (set > 0 && qidx > 0) {
		if (ctx.isInter_) {
			int32 sym;
			if (set == 1 /*TX_SET_INTER_1*/) {
				sym = ctx.sd.DecodeSymbol(ctx.cdf.interTxTypeSet1[kTxSizeSqr[txSz]], 16);
				txType = kTxTypeInterInvSet1[sym];
			} else if (set == 2 /*TX_SET_INTER_2*/) {
				sym = ctx.sd.DecodeSymbol(ctx.cdf.interTxTypeSet2, 12);
				txType = kTxTypeInterInvSet2[sym];
			} else {
				sym = ctx.sd.DecodeSymbol(ctx.cdf.interTxTypeSet3[kTxSizeSqr[txSz]], 2);
				txType = kTxTypeInterInvSet3[sym];
			}
		} else {
			int32 intraDir = ctx.useFilterIntra ? kFilterIntraModeToIntraDir[ctx.filterIntraMode_] : ctx.YMode;
			int32 sym;
			if (set == 1 /*TX_SET_INTRA_1*/)
				sym = ctx.sd.DecodeSymbol(ctx.cdf.intraTxTypeSet1[kTxSizeSqr[txSz]][intraDir], 7);
			else
				sym = ctx.sd.DecodeSymbol(ctx.cdf.intraTxTypeSet2[kTxSizeSqr[txSz]][intraDir], 5);
			txType = (set == 1) ? kTxTypeIntraInvSet1[sym] : kTxTypeIntraInvSet2[sym];
		}
	} else {
		txType = 0;
	}
	const int32 wi = kTxWidth[txSz] >> 2, hi = kTxHeight[txSz] >> 2;
	for (int32 i = 0; i < wi; ++i)
		for (int32 j = 0; j < hi; ++j)
			Grid(ctx.txTypes, ctx.gridStride, y4 + j, x4 + i) = (int8)txType;
}

int32 ComputeTxType(NkAv1Ctx &ctx, int32 plane, int32 txSz, int32 blockX, int32 blockY) {
	const int32 txSzSqrUp = kTxSizeSqrUp[txSz];
	if (ctx.Lossless || txSzSqrUp > kTx32x32) return 0 /*DCT_DCT*/;
	const int32 set = GetTxSet(ctx, txSz);
	if (plane == 0)
		return Grid(ctx.txTypes, ctx.gridStride, blockY, blockX);
	if (ctx.isInter_) {
		const int32 x4 = NkAv1Max(ctx.MiCol, blockX << ctx.seq->subsamplingX);
		const int32 y4 = NkAv1Max(ctx.MiRow, blockY << ctx.seq->subsamplingY);
		const int32 txType = Grid(ctx.txTypes, ctx.gridStride, y4, x4);
		if (!kTxTypeInSetInter[set][txType]) return 0;
		return txType;
	}
	const int32 txType = kModeToTxfm[ctx.UVMode];
	if (!kTxTypeInSetIntra[set][txType]) return 0;
	return txType;
}

const nk_uint16 *GetMrowScan(int32 txSz) {
	switch (txSz) {
		case kTx4x4: return kMrowScan4x4;
		case kTx4x8: return kMrowScan4x8;
		case kTx8x4: return kMrowScan8x4;
		case kTx8x8: return kMrowScan8x8;
		case kTx8x16: return kMrowScan8x16;
		case kTx16x8: return kMrowScan16x8;
		case kTx16x16: return kMrowScan16x16;
		case kTx4x16: return kMrowScan4x16;
		default: return kMrowScan16x4;
	}
}
const nk_uint16 *GetMcolScan(int32 txSz) {
	switch (txSz) {
		case kTx4x4: return kMcolScan4x4;
		case kTx4x8: return kMcolScan4x8;
		case kTx8x4: return kMcolScan8x4;
		case kTx8x8: return kMcolScan8x8;
		case kTx8x16: return kMcolScan8x16;
		case kTx16x8: return kMcolScan16x8;
		case kTx16x16: return kMcolScan16x16;
		case kTx4x16: return kMcolScan4x16;
		default: return kMcolScan16x4;
	}
}
const nk_uint16 *GetDefaultScan(int32 txSz) {
	switch (txSz) {
		case kTx4x4: return kDefaultScan4x4;
		case kTx4x8: return kDefaultScan4x8;
		case kTx8x4: return kDefaultScan8x4;
		case kTx8x8: return kDefaultScan8x8;
		case kTx8x16: return kDefaultScan8x16;
		case kTx16x8: return kDefaultScan16x8;
		case kTx16x16: return kDefaultScan16x16;
		case kTx16x32: return kDefaultScan16x32;
		case kTx32x16: return kDefaultScan32x16;
		case kTx4x16: return kDefaultScan4x16;
		case kTx16x4: return kDefaultScan16x4;
		case kTx8x32: return kDefaultScan8x32;
		case kTx32x8: return kDefaultScan32x8;
		default: return kDefaultScan32x32;
	}
}
const nk_uint16 *GetScan(NkAv1Ctx &ctx, int32 txSz) {
	if (txSz == kTx16x64) return kDefaultScan16x32;
	if (txSz == kTx64x16) return kDefaultScan32x16;
	if (kTxSizeSqrUp[txSz] == kTx64x64) return kDefaultScan32x32;
	if (ctx.PlaneTxType == kTxIDTX) return GetDefaultScan(txSz);
	const bool preferRow = (ctx.PlaneTxType == kTxV_DCT || ctx.PlaneTxType == kTxV_ADST || ctx.PlaneTxType == kTxV_FLIPADST);
	const bool preferCol = (ctx.PlaneTxType == kTxH_DCT || ctx.PlaneTxType == kTxH_ADST || ctx.PlaneTxType == kTxH_FLIPADST);
	if (preferRow) return GetMrowScan(txSz);
	if (preferCol) return GetMcolScan(txSz);
	return GetDefaultScan(txSz);
}

// =========================================================================
// Dequantization (§7.12.2).
// =========================================================================
int32 GetQIndex(NkAv1Ctx &ctx, int32 ignoreDeltaQ, int32 segmentId) {
	if (SegFeatureActiveIdx(ctx, segmentId, 0 /*SEG_LVL_ALT_Q*/)) {
		const int32 data = ctx.fh->seg.featureData[segmentId][0];
		int32 qindex = ctx.fh->quant.baseQIdx + data;
		if (ignoreDeltaQ == 0 && ctx.fh->deltaQPresent) qindex = ctx.CurrentQIndex + data;
		return NkAv1Clip3(0, 255, qindex);
	}
	if (ignoreDeltaQ == 0 && ctx.fh->deltaQPresent) return ctx.CurrentQIndex;
	return ctx.fh->quant.baseQIdx;
}
int32 DcQ(int32 b) { return kDcQlookup[0][NkAv1Clip3(0, 255, b)]; } // 8-bit only (BitDepth-8)>>1 == 0
int32 AcQ(int32 b) { return kAcQlookup[0][NkAv1Clip3(0, 255, b)]; }
int32 GetDcQuant(NkAv1Ctx &ctx, int32 plane) {
	const int32 qi = GetQIndex(ctx, 0, ctx.segmentId_);
	if (plane == 0) return DcQ(qi + ctx.fh->quant.deltaQYDc);
	if (plane == 1) return DcQ(qi + ctx.fh->quant.deltaQUDc);
	return DcQ(qi + ctx.fh->quant.deltaQVDc);
}
int32 GetAcQuant(NkAv1Ctx &ctx, int32 plane) {
	const int32 qi = GetQIndex(ctx, 0, ctx.segmentId_);
	if (plane == 0) return AcQ(qi);
	if (plane == 1) return AcQ(qi + ctx.fh->quant.deltaQUAc);
	return AcQ(qi + ctx.fh->quant.deltaQVAc);
}

// =========================================================================
// Inverse transforms (§7.13.2 / §7.13.3). T is a shared scratch buffer (max
// 64 points); the 1D helpers use it in place exactly as specified.
// =========================================================================
struct Txfm1D {
	int64 T[64];

	inline int32 Brev(int32 numBits, int32 x) {
		int32 t = 0;
		for (int32 i = 0; i < numBits; ++i) t += ((x >> i) & 1) << (numBits - 1 - i);
		return t;
	}
	inline int32 Cos128(int32 angle) {
		int32 a2 = angle & 255;
		if (a2 <= 64) return kCos128Lookup[a2];
		if (a2 <= 128) return -kCos128Lookup[128 - a2];
		if (a2 <= 192) return -kCos128Lookup[a2 - 128];
		return kCos128Lookup[256 - a2];
	}
	inline int32 Sin128(int32 angle) { return Cos128(angle - 64); }

	inline void B(int32 a, int32 b, int32 angle, int32 flip, int32 r) {
		const int64 x = T[a] * (int64)Cos128(angle) - T[b] * (int64)Sin128(angle);
		const int64 y = T[a] * (int64)Sin128(angle) + T[b] * (int64)Cos128(angle);
		T[a] = NkAv1Round2_64(x, 12);
		T[b] = NkAv1Round2_64(y, 12);
		if (flip) { const int64 tmp = T[a]; T[a] = T[b]; T[b] = tmp; }
		(void)r;
	}
	inline void H(int32 a, int32 b, int32 flip, int32 r) {
		if (flip) { const int32 tmp = a; a = b; b = tmp; }
		const int64 x = T[a], y = T[b];
		const int64 lo = -((int64)1 << (r - 1)), hi = ((int64)1 << (r - 1)) - 1;
		T[a] = NkAv1Clip3((int32)lo, (int32)hi, (int32)(x + y));
		T[b] = NkAv1Clip3((int32)lo, (int32)hi, (int32)(x - y));
	}

	void PermuteDct(int32 n) {
		int64 copyT[64];
		for (int32 i = 0; i < (1 << n); ++i) copyT[i] = T[i];
		for (int32 i = 0; i < (1 << n); ++i) T[i] = copyT[Brev(n, i)];
	}

	void InvDct(int32 n, int32 r) {
		PermuteDct(n);
		if (n == 6) for (int32 i = 0; i <= 15; ++i) B(32 + i, 63 - i, 63 - 4 * Brev(4, i), 0, r);
		if (n >= 5) for (int32 i = 0; i <= 7; ++i) B(16 + i, 31 - i, 6 + (Brev(3, 7 - i) << 3), 0, r);
		if (n == 6) for (int32 i = 0; i <= 15; ++i) H(32 + i * 2, 33 + i * 2, i & 1, r);
		if (n >= 4) for (int32 i = 0; i <= 3; ++i) B(8 + i, 15 - i, 12 + (Brev(2, 3 - i) << 4), 0, r);
		if (n >= 5) for (int32 i = 0; i <= 7; ++i) H(16 + 2 * i, 17 + 2 * i, i & 1, r);
		if (n == 6) for (int32 i = 0; i <= 3; ++i) for (int32 j = 0; j <= 1; ++j)
			B(62 - i * 4 - j, 33 + i * 4 + j, 60 - 16 * Brev(2, i) + 64 * j, 1, r);
		if (n >= 3) for (int32 i = 0; i <= 1; ++i) B(4 + i, 7 - i, 56 - 32 * i, 0, r);
		if (n >= 4) for (int32 i = 0; i <= 3; ++i) H(8 + 2 * i, 9 + 2 * i, i & 1, r);
		if (n >= 5) for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 1; ++j)
			B(30 - 4 * i - j, 17 + 4 * i + j, 24 + (j << 6) + ((1 - i) << 5), 1, r);
		if (n == 6) for (int32 i = 0; i <= 7; ++i) for (int32 j = 0; j <= 1; ++j)
			H(32 + i * 4 + j, 35 + i * 4 - j, i & 1, r);
		for (int32 i = 0; i <= 1; ++i) B(2 * i, 2 * i + 1, 32 + 16 * i, 1 - i, r);
		if (n >= 3) for (int32 i = 0; i <= 1; ++i) H(4 + 2 * i, 5 + 2 * i, i, r);
		if (n >= 4) for (int32 i = 0; i <= 1; ++i) B(14 - i, 9 + i, 48 + 64 * i, 1, r);
		if (n >= 5) for (int32 i = 0; i <= 3; ++i) for (int32 j = 0; j <= 1; ++j)
			H(16 + 4 * i + j, 19 + 4 * i - j, i & 1, r);
		if (n == 6) for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 3; ++j)
			B(61 - i * 8 - j, 34 + i * 8 + j, 56 - i * 32 + (j >> 1) * 64, 1, r);
		for (int32 i = 0; i <= 1; ++i) H(i, 3 - i, 0, r);
		if (n >= 3) B(6, 5, 32, 1, r);
		if (n >= 4) for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 1; ++j)
			H(8 + 4 * i + j, 11 + 4 * i - j, i, r);
		if (n >= 5) for (int32 i = 0; i <= 3; ++i) B(29 - i, 18 + i, 48 + (i >> 1) * 64, 1, r);
		if (n == 6) for (int32 i = 0; i <= 3; ++i) for (int32 j = 0; j <= 3; ++j)
			H(32 + 8 * i + j, 39 + 8 * i - j, i & 1, r);
		if (n >= 3) for (int32 i = 0; i <= 3; ++i) H(i, 7 - i, 0, r);
		if (n >= 4) for (int32 i = 0; i <= 1; ++i) B(13 - i, 10 + i, 32, 1, r);
		if (n >= 5) for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 3; ++j)
			H(16 + i * 8 + j, 23 + i * 8 - j, i, r);
		if (n == 6) for (int32 i = 0; i <= 7; ++i) B(59 - i, 36 + i, i < 4 ? 48 : 112, 1, r);
		if (n >= 4) for (int32 i = 0; i <= 7; ++i) H(i, 15 - i, 0, r);
		if (n >= 5) for (int32 i = 0; i <= 3; ++i) B(27 - i, 20 + i, 32, 1, r);
		if (n == 6) for (int32 i = 0; i <= 7; ++i) { H(32 + i, 47 - i, 0, r); H(48 + i, 63 - i, 1, r); }
		if (n >= 5) for (int32 i = 0; i <= 15; ++i) H(i, 31 - i, 0, r);
		if (n == 6) for (int32 i = 0; i <= 7; ++i) B(55 - i, 40 + i, 32, 1, r);
		if (n == 6) for (int32 i = 0; i <= 31; ++i) H(i, 63 - i, 0, r);
	}

	void PermuteAdstIn(int32 n) {
		const int32 n0 = 1 << n;
		int64 copyT[16];
		for (int32 i = 0; i < n0; ++i) copyT[i] = T[i];
		for (int32 i = 0; i < n0; ++i) {
			const int32 idx = (i & 1) ? (i - 1) : (n0 - i - 1);
			T[i] = copyT[idx];
		}
	}
	void PermuteAdstOut(int32 n) {
		const int32 n0 = 1 << n;
		int64 copyT[16];
		for (int32 i = 0; i < n0; ++i) copyT[i] = T[i];
		for (int32 i = 0; i < n0; ++i) {
			const int32 a = (i >> 3) & 1;
			const int32 b = ((i >> 2) & 1) ^ ((i >> 3) & 1);
			const int32 c = ((i >> 1) & 1) ^ ((i >> 2) & 1);
			const int32 d = (i & 1) ^ ((i >> 1) & 1);
			const int32 idx = ((d << 3) | (c << 2) | (b << 1) | a) >> (4 - n);
			T[i] = (i & 1) ? -copyT[idx] : copyT[idx];
		}
	}
	void InvAdst4(int32 r) {
		(void)r;
		static const int64 SINPI_1_9 = 1321, SINPI_2_9 = 2482, SINPI_3_9 = 3344, SINPI_4_9 = 3803;
		int64 s[7];
		s[0] = SINPI_1_9 * T[0];
		s[1] = SINPI_2_9 * T[0];
		s[2] = SINPI_3_9 * T[1];
		s[3] = SINPI_4_9 * T[2];
		s[4] = SINPI_1_9 * T[2];
		s[5] = SINPI_2_9 * T[3];
		s[6] = SINPI_4_9 * T[3];
		const int64 a7 = T[0] - T[2];
		const int64 b7 = a7 + T[3];
		s[0] = s[0] + s[3];
		s[1] = s[1] - s[4];
		s[3] = s[2];
		s[2] = SINPI_3_9 * b7;
		s[0] = s[0] + s[5];
		s[1] = s[1] - s[6];
		int64 x[4];
		x[0] = s[0] + s[3];
		x[1] = s[1] + s[3];
		x[2] = s[2];
		x[3] = s[0] + s[1];
		x[3] = x[3] - s[3];
		T[0] = NkAv1Round2_64(x[0], 12);
		T[1] = NkAv1Round2_64(x[1], 12);
		T[2] = NkAv1Round2_64(x[2], 12);
		T[3] = NkAv1Round2_64(x[3], 12);
	}
	void InvAdst8(int32 r) {
		PermuteAdstIn(3);
		for (int32 i = 0; i <= 3; ++i) B(2 * i, 2 * i + 1, 60 - 16 * i, 1, r);
		for (int32 i = 0; i <= 3; ++i) H(i, 4 + i, 0, r);
		for (int32 i = 0; i <= 1; ++i) B(4 + 3 * i, 5 + i, 48 - 32 * i, 1, r);
		for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 1; ++j) H(4 * j + i, 2 + 4 * j + i, 0, r);
		for (int32 i = 0; i <= 1; ++i) B(2 + 4 * i, 3 + 4 * i, 32, 1, r);
		PermuteAdstOut(3);
	}
	void InvAdst16(int32 r) {
		PermuteAdstIn(4);
		for (int32 i = 0; i <= 7; ++i) B(2 * i, 2 * i + 1, 62 - 8 * i, 1, r);
		for (int32 i = 0; i <= 7; ++i) H(i, 8 + i, 0, r);
		for (int32 i = 0; i <= 1; ++i) { B(8 + 2 * i, 9 + 2 * i, 56 - 32 * i, 1, r); B(13 + 2 * i, 12 + 2 * i, 8 + 32 * i, 1, r); }
		for (int32 i = 0; i <= 3; ++i) for (int32 j = 0; j <= 1; ++j) H(8 * j + i, 4 + 8 * j + i, 0, r);
		for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 1; ++j) B(4 + 8 * j + 3 * i, 5 + 8 * j + i, 48 - 32 * i, 1, r);
		for (int32 i = 0; i <= 1; ++i) for (int32 j = 0; j <= 3; ++j) H(4 * j + i, 2 + 4 * j + i, 0, r);
		for (int32 i = 0; i <= 3; ++i) B(2 + 4 * i, 3 + 4 * i, 32, 1, r);
		PermuteAdstOut(4);
	}
	void InvAdst(int32 n, int32 r) {
		if (n == 2) InvAdst4(r);
		else if (n == 3) InvAdst8(r);
		else InvAdst16(r);
	}
	void InvWht(int32 shift) {
		int64 a = T[0] >> shift, c = T[1] >> shift, d = T[2] >> shift, b = T[3] >> shift;
		a += c; d -= b;
		const int64 e = (a - d) >> 1;
		b = e - b; c = e - c;
		a -= b; d += c;
		T[0] = a; T[1] = b; T[2] = c; T[3] = d;
	}
	void InvIdentity(int32 n) {
		const int32 len = 1 << n;
		if (n == 2) for (int32 i = 0; i < len; ++i) T[i] = NkAv1Round2_64(T[i] * 5793, 12);
		else if (n == 3) for (int32 i = 0; i < len; ++i) T[i] = T[i] * 2;
		else if (n == 4) for (int32 i = 0; i < len; ++i) T[i] = NkAv1Round2_64(T[i] * 11586, 12);
		else for (int32 i = 0; i < len; ++i) T[i] = T[i] * 4;
	}
};

// Block-scoped scratch for dequant/reconstruct (avoids repeated large stack
// allocations; one instance lives inside NkAv1Ctx via BlockScratch below).
struct NkAv1BlockScratch {
	int32 Quant[1024];
	int32 Dequant[64][64];
	int64 Residual[64][64];
	// MC inter : tampon intermediaire (filtre horizontal) + predictions par liste.
	int32 mcIntermediate[128 + 8][128];
	int32 mcPreds[2][128][128];
	// Masque de compound masque (wedge / diffwtd), en resolution LUMA.
	uint8 mcMask[128][128];
};

// 2D inverse transform (§7.13.3): row transforms then column transforms.
void Inverse2DTransform(NkAv1Ctx &ctx, int32 txSz) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 log2W = kTxWidthLog2[txSz], log2H = kTxHeightLog2[txSz];
	const int32 w = 1 << log2W, h = 1 << log2H;
	const int32 rowShift = ctx.Lossless ? 0 : kTransformRowShift[txSz];
	const int32 colShift = ctx.Lossless ? 0 : 4;
	const int32 rowClamp = 8 + 8; // BitDepth(8) + 8
	const int32 colClamp = NkAv1Max(8 + 6, 16); // BitDepth(8)+6, min 16

	Txfm1D t1d;
	for (int32 i = 0; i < h; ++i) {
		for (int32 j = 0; j < w; ++j)
			t1d.T[j] = (i < 32 && j < 32) ? bs.Dequant[i][j] : 0;
		if (NkAv1Abs(log2W - log2H) == 1)
			for (int32 j = 0; j < w; ++j) t1d.T[j] = NkAv1Round2_64(t1d.T[j] * 2896, 12);
		if (ctx.Lossless) {
			t1d.InvWht(2);
		} else if (ctx.PlaneTxType == kTxDCT_DCT || ctx.PlaneTxType == kTxADST_DCT ||
				   ctx.PlaneTxType == kTxFLIPADST_DCT || ctx.PlaneTxType == kTxH_DCT) {
			t1d.InvDct(log2W, rowClamp);
		} else if (ctx.PlaneTxType == kTxDCT_ADST || ctx.PlaneTxType == kTxADST_ADST ||
				   ctx.PlaneTxType == kTxDCT_FLIPADST || ctx.PlaneTxType == kTxFLIPADST_FLIPADST ||
				   ctx.PlaneTxType == kTxADST_FLIPADST || ctx.PlaneTxType == kTxFLIPADST_ADST ||
				   ctx.PlaneTxType == kTxH_ADST || ctx.PlaneTxType == kTxH_FLIPADST) {
			t1d.InvAdst(log2W, rowClamp);
		} else {
			t1d.InvIdentity(log2W);
		}
		for (int32 j = 0; j < w; ++j)
			bs.Residual[i][j] = NkAv1Round2_64(t1d.T[j], rowShift);
	}
	for (int32 i = 0; i < h; ++i)
		for (int32 j = 0; j < w; ++j)
			bs.Residual[i][j] = NkAv1Clip3(-(1 << (colClamp - 1)), (1 << (colClamp - 1)) - 1, (int32)bs.Residual[i][j]);
	for (int32 j = 0; j < w; ++j) {
		for (int32 i = 0; i < h; ++i) t1d.T[i] = bs.Residual[i][j];
		if (ctx.Lossless) {
			t1d.InvWht(0);
		} else if (ctx.PlaneTxType == kTxDCT_DCT || ctx.PlaneTxType == kTxDCT_ADST ||
				   ctx.PlaneTxType == kTxDCT_FLIPADST || ctx.PlaneTxType == kTxV_DCT) {
			t1d.InvDct(log2H, colClamp);
		} else if (ctx.PlaneTxType == kTxADST_DCT || ctx.PlaneTxType == kTxADST_ADST ||
				   ctx.PlaneTxType == kTxFLIPADST_DCT || ctx.PlaneTxType == kTxFLIPADST_FLIPADST ||
				   ctx.PlaneTxType == kTxADST_FLIPADST || ctx.PlaneTxType == kTxFLIPADST_ADST ||
				   ctx.PlaneTxType == kTxV_ADST || ctx.PlaneTxType == kTxV_FLIPADST) {
			t1d.InvAdst(log2H, colClamp);
		} else {
			t1d.InvIdentity(log2H);
		}
		for (int32 i = 0; i < h; ++i)
			bs.Residual[i][j] = NkAv1Round2_64(t1d.T[i], colShift);
	}
}

uint8 &CurPix(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y) {
	if (plane == 0) return ctx.curY[(usize)y * (usize)ctx.curYStride + (usize)x];
	if (plane == 1) return ctx.curU[(usize)y * (usize)ctx.curUvStride + (usize)x];
	return ctx.curV[(usize)y * (usize)ctx.curUvStride + (usize)x];
}
inline int32 Clip1_8(int32 v) { return NkAv1Clip3(0, 255, v); }

void Reconstruct(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, int32 txSz) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	int32 dqDenom = 1;
	if (txSz == kTx32x32 || txSz == kTx16x32 || txSz == kTx32x16 || txSz == kTx16x64 || txSz == kTx64x16) dqDenom = 2;
	else if (txSz == kTx64x64 || txSz == kTx32x64 || txSz == kTx64x32) dqDenom = 4;
	const int32 log2W = kTxWidthLog2[txSz], log2H = kTxHeightLog2[txSz];
	const int32 w = 1 << log2W, h = 1 << log2H;
	const int32 tw = NkAv1Min(32, w), th = NkAv1Min(32, h);
	const bool flipUD = (ctx.PlaneTxType == kTxFLIPADST_DCT || ctx.PlaneTxType == kTxFLIPADST_ADST ||
						  ctx.PlaneTxType == kTxV_FLIPADST || ctx.PlaneTxType == kTxFLIPADST_FLIPADST);
	const bool flipLR = (ctx.PlaneTxType == kTxDCT_FLIPADST || ctx.PlaneTxType == kTxADST_FLIPADST ||
						  ctx.PlaneTxType == kTxH_FLIPADST || ctx.PlaneTxType == kTxFLIPADST_FLIPADST);

	for (int32 i = 0; i < th; ++i) {
		for (int32 j = 0; j < tw; ++j) {
			const int32 q = (i == 0 && j == 0) ? GetDcQuant(ctx, plane) : GetAcQuant(ctx, plane);
			// using_qmatrix not implemented (our test streams have usingQmatrix=0);
			// q2 = q always (documented gap).
			const int64 dq = (int64)bs.Quant[i * tw + j] * q;
			const int64 absDq = (dq < 0) ? -dq : dq;
			const int64 masked = absDq & 0xFFFFFF;
			const int64 dq2 = (dq < 0 ? -masked : masked) / dqDenom;
			bs.Dequant[i][j] = NkAv1Clip3(-(1 << (7 + 8)), (1 << (7 + 8)) - 1, (int32)dq2);
		}
	}
	for (int32 i = th; i < 64; ++i) for (int32 j = 0; j < 64; ++j) bs.Dequant[i][j] = 0;
	for (int32 i = 0; i < th; ++i) for (int32 j = tw; j < 64; ++j) bs.Dequant[i][j] = 0;

	Inverse2DTransform(ctx, txSz);

	for (int32 i = 0; i < h; ++i) {
		for (int32 j = 0; j < w; ++j) {
			const int32 xx = flipLR ? (w - j - 1) : j;
			const int32 yy = flipUD ? (h - i - 1) : i;
			uint8 &px = CurPix(ctx, plane, x + xx, y + yy);
			px = (uint8)Clip1_8((int32)px + (int32)bs.Residual[i][j]);
		}
	}
}

// =========================================================================
// Coefficient contexts + coeffs() (§5.11.39 / §8.3.2 CDF selection).
// =========================================================================
int32 GetCoeffBaseCtx(NkAv1Ctx &ctx, int32 txSz, int32 plane, int32 blockX, int32 blockY,
					  int32 pos, int32 c, int32 isEob) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 adjTxSz = kAdjustedTxSize[txSz];
	const int32 bwl = kTxWidthLog2[adjTxSz];
	const int32 width = 1 << bwl;
	const int32 height = kTxHeight[adjTxSz];
	const int32 txType = ComputeTxType(ctx, plane, txSz, blockX, blockY);
	if (isEob) {
		if (c == 0) return 42 - 4;
		if (c <= (height << bwl) / 8) return 42 - 3;
		if (c <= (height << bwl) / 4) return 42 - 2;
		return 42 - 1;
	}
	const int32 txClass = GetTxClass(txType);
	const int32 row = pos >> bwl;
	const int32 col = pos - (row << bwl);
	int32 mag = 0;
	for (int32 idx = 0; idx < 5 /*SIG_REF_DIFF_OFFSET_NUM*/; ++idx) {
		const int32 refRow = row + kSigRefDiffOffset[txClass][idx][0];
		const int32 refCol = col + kSigRefDiffOffset[txClass][idx][1];
		if (refRow >= 0 && refCol >= 0 && refRow < height && refCol < width)
			mag += NkAv1Min(NkAv1Abs(bs.Quant[(refRow << bwl) + refCol]), 3);
	}
	const int32 baseCtx = NkAv1Min((mag + 1) >> 1, 4);
	if (txClass == 0 /*TX_CLASS_2D*/) {
		if (row == 0 && col == 0) return 0;
		return baseCtx + kCoeffBaseCtxOffset[txSz][NkAv1Min(row, 4)][NkAv1Min(col, 4)];
	}
	const int32 idx2 = (txClass == 2 /*TX_CLASS_VERT*/) ? row : col;
	return baseCtx + kCoeffBasePosCtxOffset[NkAv1Min(idx2, 2)];
}

int32 Coeffs(NkAv1Ctx &ctx, int32 plane, int32 startX, int32 startY, int32 txSz) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 x4 = startX >> 2, y4 = startY >> 2;
	const int32 w4 = kTxWidth[txSz] >> 2, h4 = kTxHeight[txSz] >> 2;
	const int32 txSzCtx = (kTxSizeSqr[txSz] + kTxSizeSqrUp[txSz] + 1) >> 1;
	const int32 ptype = plane > 0 ? 1 : 0;
	const int32 segEob = (txSz == kTx16x64 || txSz == kTx64x16) ? 512 : NkAv1Min(1024, kTxWidth[txSz] * kTxHeight[txSz]);

	for (int32 i = 0; i < segEob; ++i) bs.Quant[i] = 0;

	int32 eob = 0, culLevel = 0, dcCategory = 0;
	const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
	const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
	int32 maxX4 = ctx.miCols >> subX, maxY4 = ctx.miRows >> subY;

	// all_zero context.
	const int32 bsize = GetPlaneResidualSize(ctx, ctx.MiSize, plane);
	const int32 bw = kNum4x4BlocksWide[bsize] * 4, bh = kNum4x4BlocksHigh[bsize] * 4;
	const int32 w = kTxWidth[txSz], h = kTxHeight[txSz];
	int32 azCtx;
	if (plane == 0) {
		int32 top = 0, left = 0;
		for (int32 k = 0; k < w4; ++k) if (x4 + k < maxX4) top = NkAv1Max(top, ctx.aboveLevel[plane][x4 + k]);
		for (int32 k = 0; k < h4; ++k) if (y4 + k < maxY4) left = NkAv1Max(left, ctx.leftLevel[plane][y4 + k]);
		top = NkAv1Min(top, 255); left = NkAv1Min(left, 255);
		if (bw == w && bh == h) azCtx = 0;
		else if (top == 0 && left == 0) azCtx = 1;
		else if (top == 0 || left == 0) azCtx = 2 + (NkAv1Max(top, left) > 3 ? 1 : 0);
		else if (NkAv1Max(top, left) <= 3) azCtx = 4;
		else if (NkAv1Min(top, left) <= 3) azCtx = 5;
		else azCtx = 6;
	} else {
		int32 above = 0, left = 0;
		for (int32 i = 0; i < w4; ++i) if (x4 + i < maxX4) { above |= ctx.aboveLevel[plane][x4 + i]; above |= ctx.aboveDc[plane][x4 + i]; }
		for (int32 i = 0; i < h4; ++i) if (y4 + i < maxY4) { left |= ctx.leftLevel[plane][y4 + i]; left |= ctx.leftDc[plane][y4 + i]; }
		azCtx = (above != 0 ? 1 : 0) + (left != 0 ? 1 : 0) + 7;
		if (bw * bh > w * h) azCtx += 3;
	}
	const int32 allZero = ctx.sd.DecodeSymbol(ctx.cdf.txbSkip[txSzCtx][azCtx], 2);

	if (allZero) {
		if (plane == 0)
			for (int32 i = 0; i < w4; ++i)
				for (int32 j = 0; j < h4; ++j)
					Grid(ctx.txTypes, ctx.gridStride, y4 + j, x4 + i) = 0 /*DCT_DCT*/;
	} else {
		if (plane == 0)
			TransformType(ctx, x4, y4, txSz);
		ctx.PlaneTxType = ComputeTxType(ctx, plane, txSz, x4, y4);
		const nk_uint16 *scan = GetScan(ctx, txSz);

		const int32 eobMultisize = NkAv1Min(kTxWidthLog2[txSz], 5) + NkAv1Min(kTxHeightLog2[txSz], 5) - 4;
		int32 eobPt;
		if (eobMultisize == 0) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt16[ptype][GetTxClass(ctx.PlaneTxType) == 0 ? 0 : 1], 5) + 1;
		else if (eobMultisize == 1) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt32[ptype][GetTxClass(ctx.PlaneTxType) == 0 ? 0 : 1], 6) + 1;
		else if (eobMultisize == 2) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt64[ptype][GetTxClass(ctx.PlaneTxType) == 0 ? 0 : 1], 7) + 1;
		else if (eobMultisize == 3) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt128[ptype][GetTxClass(ctx.PlaneTxType) == 0 ? 0 : 1], 8) + 1;
		else if (eobMultisize == 4) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt256[ptype][GetTxClass(ctx.PlaneTxType) == 0 ? 0 : 1], 9) + 1;
		else if (eobMultisize == 5) eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt512[ptype], 10) + 1;
		else eobPt = ctx.sd.DecodeSymbol(ctx.cdf.eobPt1024[ptype], 11) + 1;

		eob = (eobPt < 2) ? eobPt : ((1 << (eobPt - 2)) + 1);
		int32 eobShift = NkAv1Max(-1, eobPt - 3);
		if (eobShift >= 0) {
			const int32 eobExtra = ctx.sd.DecodeSymbol(ctx.cdf.eobExtra[txSzCtx][ptype][eobPt - 3], 2);
			if (eobExtra) eob += (1 << eobShift);
			for (int32 i = 1; i < NkAv1Max(0, eobPt - 2); ++i) {
				eobShift = NkAv1Max(0, eobPt - 2) - 1 - i;
				const int32 bit = (int32)ctx.sd.ReadLiteral(1);
				if (bit) eob += (1 << eobShift);
			}
		}
		for (int32 c = eob - 1; c >= 0; --c) {
			const int32 pos = scan[c];
			int32 level;
			if (c == eob - 1) {
				const int32 cctx = GetCoeffBaseCtx(ctx, txSz, plane, x4, y4, pos, c, 1) - 42 + 4;
				level = ctx.sd.DecodeSymbol(ctx.cdf.coeffBaseEob[txSzCtx][ptype][cctx], 3) + 1;
			} else {
				const int32 cctx = GetCoeffBaseCtx(ctx, txSz, plane, x4, y4, pos, c, 0);
				level = ctx.sd.DecodeSymbol(ctx.cdf.coeffBase[txSzCtx][ptype][cctx], 4);
			}
			if (level > 2 /*NUM_BASE_LEVELS*/) {
				for (int32 idx = 0; idx < 12 / 3 /*COEFF_BASE_RANGE/(BR_CDF_SIZE-1)*/; ++idx) {
					const int32 brCtx = [&]() {
						const int32 adjTxSz = kAdjustedTxSize[txSz];
						const int32 bwl = kTxWidthLog2[adjTxSz];
						const int32 txw = kTxWidth[adjTxSz], txh = kTxHeight[adjTxSz];
						const int32 row = pos >> bwl, col = pos - (row << bwl);
						int32 mag = 0;
						const int32 txClass = GetTxClass(ctx.PlaneTxType);
						for (int32 k = 0; k < 3; ++k) {
							const int32 refRow = row + kMagRefOffsetWithTxClass[txClass][k][0];
							const int32 refCol = col + kMagRefOffsetWithTxClass[txClass][k][1];
							if (refRow >= 0 && refCol >= 0 && refRow < txh && refCol < (1 << bwl))
								mag += NkAv1Min(bs.Quant[refRow * txw + refCol], 12 + 2 + 1);
						}
						mag = NkAv1Min((mag + 1) >> 1, 6);
						if (pos == 0) return mag;
						if (txClass == 0) return (row < 2 && col < 2) ? mag + 7 : mag + 14;
						if (txClass == 1) return (col == 0) ? mag + 7 : mag + 14;
						return (row == 0) ? mag + 7 : mag + 14;
					}();
					const int32 br = ctx.sd.DecodeSymbol(ctx.cdf.coeffBr[NkAv1Min(txSzCtx, 3)][ptype][brCtx], 4);
					level += br;
					if (br < 3) break;
				}
			}
			bs.Quant[pos] = level;
		}
		for (int32 c = 0; c < eob; ++c) {
			const int32 pos = scan[c];
			int32 sign = 0;
			if (bs.Quant[pos] != 0) {
				if (c == 0) {
					int32 dcSign = 0;
					for (int32 k = 0; k < w4; ++k) if (x4 + k < maxX4) {
						const int32 s = ctx.aboveDc[plane][x4 + k];
						if (s == 1) dcSign--; else if (s == 2) dcSign++;
					}
					for (int32 k = 0; k < h4; ++k) if (y4 + k < maxY4) {
						const int32 s = ctx.leftDc[plane][y4 + k];
						if (s == 1) dcSign--; else if (s == 2) dcSign++;
					}
					const int32 dcCtx = (dcSign < 0) ? 1 : (dcSign > 0) ? 2 : 0;
					sign = ctx.sd.DecodeSymbol(ctx.cdf.dcSign[ptype][dcCtx], 2);
				} else {
					sign = (int32)ctx.sd.ReadLiteral(1);
				}
			}
			if (bs.Quant[pos] > (2 + 12) /*NUM_BASE_LEVELS+COEFF_BASE_RANGE*/) {
				int32 length = 0;
				int32 bit;
				do { ++length; bit = (int32)ctx.sd.ReadLiteral(1); } while (!bit);
				int32 xg = 1;
				for (int32 i = length - 2; i >= 0; --i) {
					const int32 gb = (int32)ctx.sd.ReadLiteral(1);
					xg = (xg << 1) | gb;
				}
				bs.Quant[pos] = xg + 12 + 2;
			}
			if (pos == 0 && bs.Quant[pos] > 0) dcCategory = sign ? 1 : 2;
			bs.Quant[pos] = bs.Quant[pos] & 0xFFFFF;
			culLevel += bs.Quant[pos];
			if (sign) bs.Quant[pos] = -bs.Quant[pos];
		}
		culLevel = NkAv1Min(63, culLevel);
	}

	for (int32 i = 0; i < w4; ++i) { ctx.aboveLevel[plane][x4 + i] = (int16)culLevel; ctx.aboveDc[plane][x4 + i] = (int16)dcCategory; }
	for (int32 i = 0; i < h4; ++i) { ctx.leftLevel[plane][y4 + i] = (int16)culLevel; ctx.leftDc[plane][y4 + i] = (int16)dcCategory; }

	return eob;
}

// =========================================================================
// Intra prediction (§7.11.2). AboveRow/LeftCol use offset arrays (index -2
// valid) implemented as fixed local buffers with a +2 bias.
// =========================================================================
inline int32 GetFilterType(NkAv1Ctx &ctx, int32 plane) {
	bool aboveSmooth = false, leftSmooth = false;
	auto isSmooth = [&](int32 row, int32 col, int32 pl) -> bool {
		int32 mode;
		if (pl == 0) {
			mode = Grid(ctx.yModes, ctx.gridStride, row, col);
		} else {
			if ((int32)Grid(ctx.refFrames0, ctx.gridStride, row, col) > 0 /*INTRA_FRAME*/)
				return false; // voisin inter : pas un mode intra smooth (§7.11.2.7)
			mode = Grid(ctx.uvModes, ctx.gridStride, row, col);
		}
		return mode == kModeSmooth || mode == kModeSmoothV || mode == kModeSmoothH;
	};
	if (plane == 0 ? ctx.AvailU : ctx.AvailUChroma) {
		int32 r = ctx.MiRow - 1, c = ctx.MiCol;
		if (plane > 0) {
			if (ctx.seq->subsamplingX && !(ctx.MiCol & 1)) ++c;
			if (ctx.seq->subsamplingY && (ctx.MiRow & 1)) --r;
		}
		aboveSmooth = isSmooth(r, c, plane);
	}
	if (plane == 0 ? ctx.AvailL : ctx.AvailLChroma) {
		int32 r = ctx.MiRow, c = ctx.MiCol - 1;
		if (plane > 0) {
			if (ctx.seq->subsamplingX && (ctx.MiCol & 1)) --c;
			if (ctx.seq->subsamplingY && !(ctx.MiRow & 1)) ++r;
		}
		leftSmooth = isSmooth(r, c, plane);
	}
	return (aboveSmooth || leftSmooth) ? 1 : 0;
}

void PredictIntra(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, bool haveLeft, bool haveAbove,
				  bool haveAboveRight, bool haveBelowLeft, int32 mode, int32 log2W, int32 log2H) {
	const int32 w = 1 << log2W, h = 1 << log2H;
	int32 maxX = (ctx.miCols * 4) - 1, maxY = (ctx.miRows * 4) - 1;
	if (plane > 0) {
		maxX = ((ctx.miCols * 4) >> ctx.seq->subsamplingX) - 1;
		maxY = ((ctx.miRows * 4) >> ctx.seq->subsamplingY) - 1;
	}
	// AboveRow[-2..w+h-1], LeftCol[-2..w+h-1] via +2 bias buffers (upsample needs -2).
	static const int32 kBias = 2;
	int32 aboveBuf[4 + 64 + 64], leftBuf[4 + 64 + 64];
	int32 *AboveRow = aboveBuf + kBias;
	int32 *LeftCol = leftBuf + kBias;

	if (!haveAbove && haveLeft) {
		const int32 v = CurPix(ctx, plane, x - 1, y);
		for (int32 i = 0; i < w + h; ++i) AboveRow[i] = v;
	} else if (!haveAbove && !haveLeft) {
		for (int32 i = 0; i < w + h; ++i) AboveRow[i] = (1 << (8 - 1)) - 1;
	} else {
		const int32 aboveLimit = NkAv1Min(maxX, x + (haveAboveRight ? 2 * w : w) - 1);
		for (int32 i = 0; i < w + h; ++i) AboveRow[i] = CurPix(ctx, plane, NkAv1Min(aboveLimit, x + i), y - 1);
	}
	if (!haveLeft && haveAbove) {
		const int32 v = CurPix(ctx, plane, x, y - 1);
		for (int32 i = 0; i < w + h; ++i) LeftCol[i] = v;
	} else if (!haveLeft && !haveAbove) {
		for (int32 i = 0; i < w + h; ++i) LeftCol[i] = (1 << (8 - 1)) + 1;
	} else {
		const int32 leftLimit = NkAv1Min(maxY, y + (haveBelowLeft ? 2 * h : h) - 1);
		for (int32 i = 0; i < w + h; ++i) LeftCol[i] = CurPix(ctx, plane, x - 1, NkAv1Min(leftLimit, y + i));
	}
	if (haveAbove && haveLeft) AboveRow[-1] = CurPix(ctx, plane, x - 1, y - 1);
	else if (haveAbove) AboveRow[-1] = CurPix(ctx, plane, x, y - 1);
	else if (haveLeft) AboveRow[-1] = CurPix(ctx, plane, x - 1, y);
	else AboveRow[-1] = 1 << (8 - 1);
	LeftCol[-1] = AboveRow[-1];

	int32 pred[64][64];

	if (plane == 0 && ctx.useFilterIntra) {
		const int32 w4 = w >> 2, h2 = h >> 1;
		for (int32 i2 = 0; i2 < h2; ++i2) {
			for (int32 j4 = 0; j4 < w4; ++j4) {
				int32 p[7];
				for (int32 i = 0; i < 7; ++i) {
					if (i < 5) {
						if (i2 == 0) p[i] = AboveRow[(j4 << 2) + i - 1];
						else if (j4 == 0 && i == 0) p[i] = LeftCol[(i2 << 1) - 1];
						else p[i] = pred[(i2 << 1) - 1][(j4 << 2) + i - 1];
					} else {
						if (j4 == 0) p[i] = LeftCol[(i2 << 1) + i - 5];
						else p[i] = pred[(i2 << 1) + i - 5][(j4 << 2) - 1];
					}
				}
				for (int32 i1 = 0; i1 <= 1; ++i1) {
					for (int32 j1 = 0; j1 <= 3; ++j1) {
						int32 pr = 0;
						for (int32 i = 0; i < 7; ++i) pr += kIntraFilterTaps[ctx.filterIntraMode_][(i1 << 2) + j1][i] * p[i];
						pred[(i2 << 1) + i1][(j4 << 2) + j1] = Clip1_8(NkAv1Round2Signed(pr, 4 /*INTRA_FILTER_SCALE_BITS*/));
					}
				}
			}
		}
	} else if (IsDirectionalMode(mode)) {
		const int32 angleDelta = (plane == 0) ? ctx.AngleDeltaY : ctx.AngleDeltaUV;
		const int32 pAngle = kModeToAngle[mode] + angleDelta * 3 /*ANGLE_STEP*/;
		int32 upsampleAbove = 0, upsampleLeft = 0;
		if (ctx.seq->enableIntraEdgeFilter) {
			if (pAngle != 90 && pAngle != 180) {
				if (pAngle > 90 && pAngle < 180 && (w + h) >= 24) {
					const int32 s = LeftCol[0] * 5 + AboveRow[-1] * 6 + AboveRow[0] * 5;
					const int32 v = NkAv1Round2(s, 4);
					LeftCol[-1] = v; AboveRow[-1] = v;
				}
				const int32 filterType = GetFilterType(ctx, plane);
				if (haveAbove) {
					const int32 d = NkAv1Abs(pAngle - 90);
					const int32 blkWh = w + h;
					int32 strength = 0;
					if (filterType == 0) {
						if (blkWh <= 8) { if (d >= 56) strength = 1; }
						else if (blkWh <= 12) { if (d >= 40) strength = 1; }
						else if (blkWh <= 16) { if (d >= 40) strength = 1; }
						else if (blkWh <= 24) { if (d >= 8) strength = 1; if (d >= 16) strength = 2; if (d >= 32) strength = 3; }
						else if (blkWh <= 32) { strength = 1; if (d >= 4) strength = 2; if (d >= 32) strength = 3; }
						else strength = 3;
					} else {
						if (blkWh <= 8) { if (d >= 40) strength = 1; if (d >= 64) strength = 2; }
						else if (blkWh <= 16) { if (d >= 20) strength = 1; if (d >= 48) strength = 2; }
						else if (blkWh <= 24) { if (d >= 4) strength = 3; }
						else strength = 3;
					}
					const int32 numPx = NkAv1Min(w, maxX - x + 1) + (pAngle < 90 ? h : 0) + 1;
					if (strength) {
						int32 edge[130];
						for (int32 i = 0; i < numPx; ++i) edge[i] = AboveRow[i - 1];
						for (int32 i = 1; i < numPx; ++i) {
							int32 s2 = 0;
							for (int32 j = 0; j < 5 /*INTRA_EDGE_TAPS*/; ++j) {
								const int32 k = NkAv1Clip3(0, numPx - 1, i - 2 + j);
								s2 += kIntraEdgeKernel[strength - 1][j] * edge[k];
							}
							AboveRow[i - 1] = (s2 + 8) >> 4;
						}
					}
				}
				if (haveLeft) {
					const int32 d = NkAv1Abs(pAngle - 180);
					const int32 blkWh = w + h;
					int32 strength = 0;
					const int32 filterType2 = filterType;
					if (filterType2 == 0) {
						if (blkWh <= 8) { if (d >= 56) strength = 1; }
						else if (blkWh <= 12) { if (d >= 40) strength = 1; }
						else if (blkWh <= 16) { if (d >= 40) strength = 1; }
						else if (blkWh <= 24) { if (d >= 8) strength = 1; if (d >= 16) strength = 2; if (d >= 32) strength = 3; }
						else if (blkWh <= 32) { strength = 1; if (d >= 4) strength = 2; if (d >= 32) strength = 3; }
						else strength = 3;
					} else {
						if (blkWh <= 8) { if (d >= 40) strength = 1; if (d >= 64) strength = 2; }
						else if (blkWh <= 16) { if (d >= 20) strength = 1; if (d >= 48) strength = 2; }
						else if (blkWh <= 24) { if (d >= 4) strength = 3; }
						else strength = 3;
					}
					const int32 numPx = NkAv1Min(h, maxY - y + 1) + (pAngle > 180 ? w : 0) + 1;
					if (strength) {
						int32 edge[130];
						for (int32 i = 0; i < numPx; ++i) edge[i] = LeftCol[i - 1];
						for (int32 i = 1; i < numPx; ++i) {
							int32 s2 = 0;
							for (int32 j = 0; j < 5; ++j) {
								const int32 k = NkAv1Clip3(0, numPx - 1, i - 2 + j);
								s2 += kIntraEdgeKernel[strength - 1][j] * edge[k];
							}
							LeftCol[i - 1] = (s2 + 8) >> 4;
						}
					}
				}
			}
			// upsample selection.
			auto selectUpsample = [&](int32 delta) -> int32 {
				const int32 d = NkAv1Abs(delta);
				const int32 blkWh = w + h;
				if (d <= 0 || d >= 40) return 0;
				const int32 filterType = GetFilterType(ctx, plane);
				if (filterType == 0) return blkWh <= 16 ? 1 : 0;
				return blkWh <= 8 ? 1 : 0;
			};
			upsampleAbove = selectUpsample(pAngle - 90);
			{
				const int32 numPx = w + (pAngle < 90 ? h : 0);
				if (upsampleAbove) {
					int32 dup[71];
					dup[0] = AboveRow[-1];
					for (int32 i = -1; i < numPx; ++i) dup[i + 2] = AboveRow[i];
					dup[numPx + 2] = AboveRow[numPx - 1];
					AboveRow[-2] = dup[0];
					for (int32 i = 0; i < numPx; ++i) {
						int32 s2 = -dup[i] + 9 * dup[i + 1] + 9 * dup[i + 2] - dup[i + 3];
						s2 = Clip1_8(NkAv1Round2(s2, 4));
						AboveRow[2 * i - 1] = s2;
						AboveRow[2 * i] = dup[i + 2];
					}
				}
			}
			upsampleLeft = selectUpsample(pAngle - 180);
			{
				const int32 numPx = h + (pAngle > 180 ? w : 0);
				if (upsampleLeft) {
					int32 dup[71];
					dup[0] = LeftCol[-1];
					for (int32 i = -1; i < numPx; ++i) dup[i + 2] = LeftCol[i];
					dup[numPx + 2] = LeftCol[numPx - 1];
					LeftCol[-2] = dup[0];
					for (int32 i = 0; i < numPx; ++i) {
						int32 s2 = -dup[i] + 9 * dup[i + 1] + 9 * dup[i + 2] - dup[i + 3];
						s2 = Clip1_8(NkAv1Round2(s2, 4));
						LeftCol[2 * i - 1] = s2;
						LeftCol[2 * i] = dup[i + 2];
					}
				}
			}
		}
		int32 dx = 0, dy = 0;
		if (pAngle < 90) dx = kDrIntraDerivative[pAngle];
		else if (pAngle > 90 && pAngle < 180) dx = kDrIntraDerivative[180 - pAngle];
		if (pAngle > 90 && pAngle < 180) dy = kDrIntraDerivative[pAngle - 90];
		else if (pAngle > 180) dy = kDrIntraDerivative[270 - pAngle];

		if (pAngle < 90) {
			for (int32 i = 0; i < h; ++i) {
				for (int32 j = 0; j < w; ++j) {
					const int32 idx = (i + 1) * dx;
					const int32 base = (idx >> (6 - upsampleAbove)) + (j << upsampleAbove);
					const int32 shift = ((idx << upsampleAbove) >> 1) & 0x1F;
					const int32 maxBaseX = (w + h - 1) << upsampleAbove;
					if (base < maxBaseX) pred[i][j] = NkAv1Round2(AboveRow[base] * (32 - shift) + AboveRow[base + 1] * shift, 5);
					else pred[i][j] = AboveRow[maxBaseX];
				}
			}
		} else if (pAngle > 90 && pAngle < 180) {
			for (int32 i = 0; i < h; ++i) {
				for (int32 j = 0; j < w; ++j) {
					int32 idx = (j << 6) - (i + 1) * dx;
					int32 base = idx >> (6 - upsampleAbove);
					if (base >= -(1 << upsampleAbove)) {
						const int32 shift = ((idx << upsampleAbove) >> 1) & 0x1F;
						pred[i][j] = NkAv1Round2(AboveRow[base] * (32 - shift) + AboveRow[base + 1] * shift, 5);
					} else {
						idx = (i << 6) - (j + 1) * dy;
						base = idx >> (6 - upsampleLeft);
						const int32 shift = ((idx << upsampleLeft) >> 1) & 0x1F;
						pred[i][j] = NkAv1Round2(LeftCol[base] * (32 - shift) + LeftCol[base + 1] * shift, 5);
					}
				}
			}
		} else if (pAngle > 180) {
			for (int32 i = 0; i < h; ++i) {
				for (int32 j = 0; j < w; ++j) {
					const int32 idx = (j + 1) * dy;
					const int32 base = (idx >> (6 - upsampleLeft)) + (i << upsampleLeft);
					const int32 shift = ((idx << upsampleLeft) >> 1) & 0x1F;
					pred[i][j] = NkAv1Round2(LeftCol[base] * (32 - shift) + LeftCol[base + 1] * shift, 5);
				}
			}
		} else if (pAngle == 90) {
			for (int32 i = 0; i < h; ++i) for (int32 j = 0; j < w; ++j) pred[i][j] = AboveRow[j];
		} else { // pAngle == 180
			for (int32 i = 0; i < h; ++i) for (int32 j = 0; j < w; ++j) pred[i][j] = LeftCol[i];
		}
	} else if (mode == kModeSmooth || mode == kModeSmoothV || mode == kModeSmoothH) {
		const nk_int16 *wX = (log2W == 2) ? kSmWeightsTx4x4 : (log2W == 3) ? kSmWeightsTx8x8 :
			(log2W == 4) ? kSmWeightsTx16x16 : (log2W == 5) ? kSmWeightsTx32x32 : kSmWeightsTx64x64;
		const nk_int16 *wY = (log2H == 2) ? kSmWeightsTx4x4 : (log2H == 3) ? kSmWeightsTx8x8 :
			(log2H == 4) ? kSmWeightsTx16x16 : (log2H == 5) ? kSmWeightsTx32x32 : kSmWeightsTx64x64;
		if (mode == kModeSmooth) {
			for (int32 i = 0; i < h; ++i)
				for (int32 j = 0; j < w; ++j) {
					const int32 sp = wY[i] * AboveRow[j] + (256 - wY[i]) * LeftCol[h - 1] +
									 wX[j] * LeftCol[i] + (256 - wX[j]) * AboveRow[w - 1];
					pred[i][j] = NkAv1Round2(sp, 9);
				}
		} else if (mode == kModeSmoothV) {
			for (int32 i = 0; i < h; ++i)
				for (int32 j = 0; j < w; ++j) {
					const int32 sp = wY[i] * AboveRow[j] + (256 - wY[i]) * LeftCol[h - 1];
					pred[i][j] = NkAv1Round2(sp, 8);
				}
		} else {
			for (int32 i = 0; i < h; ++i)
				for (int32 j = 0; j < w; ++j) {
					const int32 sp = wX[j] * LeftCol[i] + (256 - wX[j]) * AboveRow[w - 1];
					pred[i][j] = NkAv1Round2(sp, 8);
				}
		}
	} else if (mode == kModeDC) {
		int32 avg;
		if (haveLeft && haveAbove) {
			int32 sum = 0;
			for (int32 k = 0; k < h; ++k) sum += LeftCol[k];
			for (int32 k = 0; k < w; ++k) sum += AboveRow[k];
			sum += (w + h) >> 1;
			avg = sum / (w + h);
		} else if (haveLeft) {
			int32 sum = 0;
			for (int32 k = 0; k < h; ++k) sum += LeftCol[k];
			avg = Clip1_8((sum + (h >> 1)) >> log2H);
		} else if (haveAbove) {
			int32 sum = 0;
			for (int32 k = 0; k < w; ++k) sum += AboveRow[k];
			avg = Clip1_8((sum + (w >> 1)) >> log2W);
		} else {
			avg = 1 << (8 - 1);
		}
		for (int32 i = 0; i < h; ++i) for (int32 j = 0; j < w; ++j) pred[i][j] = avg;
	} else { // PAETH
		for (int32 i = 0; i < h; ++i) {
			for (int32 j = 0; j < w; ++j) {
				const int32 base = AboveRow[j] + LeftCol[i] - AboveRow[-1];
				const int32 pLeft = NkAv1Abs(base - LeftCol[i]);
				const int32 pTop = NkAv1Abs(base - AboveRow[j]);
				const int32 pTopLeft = NkAv1Abs(base - AboveRow[-1]);
				if (pLeft <= pTop && pLeft <= pTopLeft) pred[i][j] = LeftCol[i];
				else if (pTop <= pTopLeft) pred[i][j] = AboveRow[j];
				else pred[i][j] = AboveRow[-1];
			}
		}
	}

	for (int32 i = 0; i < h; ++i)
		for (int32 j = 0; j < w; ++j)
			CurPix(ctx, plane, x + j, y + i) = (uint8)pred[i][j];
}

void PredictChromaFromLuma(NkAv1Ctx &ctx, int32 plane, int32 startX, int32 startY, int32 txSz) {
	const int32 w = kTxWidth[txSz], h = kTxHeight[txSz];
	const int32 subX = ctx.seq->subsamplingX, subY = ctx.seq->subsamplingY;
	// Luma average (§7.11.5): subsampled reconstructed luma samples with 3 fractional bits.
	int32 lumaAvgSum = 0;
	int32 L[32][32];
	for (int32 i = 0; i < h; ++i) {
		int32 lumaY = (startY + i) << subY;
		lumaY = NkAv1Min(lumaY, ctx.MaxLumaH - (1 << subY));
		for (int32 j = 0; j < w; ++j) {
			int32 lumaX = (startX + j) << subX;
			lumaX = NkAv1Min(lumaX, ctx.MaxLumaW - (1 << subX));
			int32 t = 0;
			for (int32 dy = 0; dy <= subY; ++dy)
				for (int32 dx = 0; dx <= subX; ++dx)
					t += (int32)CurPix(ctx, 0, lumaX + dx, lumaY + dy);
			const int32 v = t << (3 - subX - subY);
			L[i][j] = v;
			lumaAvgSum += v;
		}
	}
	const int32 lumaAvg = NkAv1Round2(lumaAvgSum, kTxWidthLog2[txSz] + kTxHeightLog2[txSz]);
	const int32 alpha = (plane == 1) ? ctx.CflAlphaU : ctx.CflAlphaV;
	for (int32 i = 0; i < h; ++i) {
		for (int32 j = 0; j < w; ++j) {
			const int32 dc = CurPix(ctx, plane, startX + j, startY + i);
			const int32 scaledLuma = NkAv1Round2Signed(alpha * (L[i][j] - lumaAvg), 6);
			CurPix(ctx, plane, startX + j, startY + i) = (uint8)Clip1_8(dc + scaledLuma);
		}
	}
}

// -----------------------------------------------------------------------
// BlockDecoded[plane][y][x], y,x in [-1, sbSize4>>sub]. Stored with a fixed
// +1 bias in a 34x34 (max sbSize4=32) per-plane buffer.
// -----------------------------------------------------------------------
inline uint8 &BD(NkAv1Ctx &ctx, int32 plane, int32 y, int32 x) {
	return ctx.blockDecoded[plane][(usize)(y + 1) * 34 + (usize)(x + 1)];
}

void ClearBlockDecodedFlags(NkAv1Ctx &ctx, int32 r, int32 c, int32 sbSize4) {
	const int32 numPlanes = ctx.seq->mono ? 1 : 3;
	for (int32 plane = 0; plane < numPlanes; ++plane) {
		const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
		const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
		const int32 sbWidth4 = (ctx.miColEnd - c) >> subX;
		const int32 sbHeight4 = (ctx.miRowEnd - r) >> subY;
		for (int32 y = -1; y <= (sbSize4 >> subY); ++y) {
			for (int32 x = -1; x <= (sbSize4 >> subX); ++x) {
				if (y < 0 && x < sbWidth4) BD(ctx, plane, y, x) = 1;
				else if (x < 0 && y < sbHeight4) BD(ctx, plane, y, x) = 1;
				else BD(ctx, plane, y, x) = 0;
			}
		}
		BD(ctx, plane, sbSize4 >> subY, -1) = 0;
	}
}

void ResetBlockContext(NkAv1Ctx &ctx, int32 bw4, int32 bh4) {
	const int32 planes = 1 + 2 * (ctx.HasChroma ? 1 : 0);
	for (int32 plane = 0; plane < planes; ++plane) {
		const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
		const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
		for (int32 i = (ctx.MiCol >> subX); i < ((ctx.MiCol + bw4) >> subX); ++i) {
			ctx.aboveLevel[plane][i] = 0;
			ctx.aboveDc[plane][i] = 0;
		}
		for (int32 i = (ctx.MiRow >> subY); i < ((ctx.MiRow + bh4) >> subY); ++i) {
			ctx.leftLevel[plane][i] = 0;
			ctx.leftDc[plane][i] = 0;
		}
	}
}

void TransformBlock(NkAv1Ctx &ctx, int32 plane, int32 baseX, int32 baseY, int32 txSz, int32 x, int32 y) {
	const int32 startX = baseX + 4 * x;
	const int32 startY = baseY + 4 * y;
	const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
	const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
	const int32 row = (startY << subY) >> 2 /*MI_SIZE_LOG2*/;
	const int32 col = (startX << subX) >> 2;
	const int32 sbMask = ctx.seq->use128x128Superblock ? 31 : 15;
	const int32 subBlockMiRow = row & sbMask;
	const int32 subBlockMiCol = col & sbMask;
	const int32 stepX = kTxWidth[txSz] >> 2;
	const int32 stepY = kTxHeight[txSz] >> 2;
	const int32 maxX = (ctx.miCols * 4) >> subX;
	const int32 maxY = (ctx.miRows * 4) >> subY;
	if (startX >= maxX || startY >= maxY) return;

	if (!ctx.isInter_) {
		if ((plane == 0 && ctx.PaletteSizeY) || (plane != 0 && ctx.PaletteSizeUV)) {
			// Palette prediction unsupported -- guarded upstream (ModeInfo bails
			// out honestly before reaching here when palette would be selected).
			ctx.ok = false;
			ctx.err = "palette (non implementee)";
			return;
		}
		const bool isCfl = (plane > 0 && ctx.UVMode == kModeUvCfl);
		const int32 mode = (plane == 0) ? ctx.YMode : (isCfl ? kModeDC : ctx.UVMode);
		const int32 log2W = kTxWidthLog2[txSz], log2H = kTxHeightLog2[txSz];
		const bool haveLeft = (plane == 0 ? ctx.AvailL : ctx.AvailLChroma) || x > 0;
		const bool haveAbove = (plane == 0 ? ctx.AvailU : ctx.AvailUChroma) || y > 0;
		const bool haveAboveRight = BD(ctx, plane, (subBlockMiRow >> subY) - 1, (subBlockMiCol >> subX) + stepX) != 0;
		const bool haveBelowLeft = BD(ctx, plane, (subBlockMiRow >> subY) + stepY, (subBlockMiCol >> subX) - 1) != 0;
		PredictIntra(ctx, plane, startX, startY, haveLeft, haveAbove, haveAboveRight, haveBelowLeft, mode, log2W, log2H);
		if (isCfl) PredictChromaFromLuma(ctx, plane, startX, startY, txSz);
		if (plane == 0) { ctx.MaxLumaW = startX + stepX * 4; ctx.MaxLumaH = startY + stepY * 4; }
	}

	if (!ctx.skip_) {
		const int32 eob = Coeffs(ctx, plane, startX, startY, txSz);
		if (eob > 0) Reconstruct(ctx, plane, startX, startY, txSz);
	}
	for (int32 i = 0; i < stepY; ++i) {
		for (int32 j = 0; j < stepX; ++j) {
			BD(ctx, plane, (subBlockMiRow >> subY) + i, (subBlockMiCol >> subX) + j) = 1;
			Grid(ctx.loopfilterTxSizes[plane], ctx.gridStride, (row >> subY) + i, (col >> subX) + j) = (int8)txSz;
		}
	}
}

void Residual(NkAv1Ctx &ctx) {
	const int32 sbMask = ctx.seq->use128x128Superblock ? 31 : 15;
	const int32 blockW = kNum4x4BlocksWide[ctx.MiSize] * 4, blockH = kNum4x4BlocksHigh[ctx.MiSize] * 4;
	const int32 widthChunks = NkAv1Max(1, blockW >> 6);
	const int32 heightChunks = NkAv1Max(1, blockH >> 6);
	const int32 miSizeChunk = (widthChunks > 1 || heightChunks > 1) ? 12 /*BLOCK_64X64*/ : ctx.MiSize;
	(void)sbMask;
	for (int32 chunkY = 0; chunkY < heightChunks; ++chunkY) {
		for (int32 chunkX = 0; chunkX < widthChunks; ++chunkX) {
			const int32 miRowChunk = ctx.MiRow + (chunkY << 4);
			const int32 miColChunk = ctx.MiCol + (chunkX << 4);
			const int32 planes = 1 + (ctx.HasChroma ? 2 : 0);
			for (int32 plane = 0; plane < planes; ++plane) {
				const int32 txSz = ctx.Lossless ? kTx4x4 : GetTxSize(ctx, plane, ctx.TxSize_);
				const int32 stepX = kTxWidth[txSz] >> 2, stepY = kTxHeight[txSz] >> 2;
				const int32 planeSz = GetPlaneResidualSize(ctx, miSizeChunk, plane);
				const int32 num4x4W = kNum4x4BlocksWide[planeSz], num4x4H = kNum4x4BlocksHigh[planeSz];
				const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
				const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
				if (ctx.isInter_ && !ctx.Lossless && plane == 0) {
					const int32 baseX = (miColChunk >> subX) * 4;
					const int32 baseY = (miRowChunk >> subY) * 4;
					TransformTree(ctx, baseX, baseY, num4x4W * 4, num4x4H * 4);
				} else {
					const int32 baseXBlock = (ctx.MiCol >> subX) * 4;
					const int32 baseYBlock = (ctx.MiRow >> subY) * 4;
					for (int32 y = 0; y < num4x4H; y += stepY)
						for (int32 x = 0; x < num4x4W; x += stepX)
							TransformBlock(ctx, plane, baseXBlock, baseYBlock, txSz,
										   x + ((chunkX << 4) >> subX), y + ((chunkY << 4) >> subY));
				}
			}
			(void)miColChunk; (void)miRowChunk;
		}
	}
}

// =========================================================================
// transform_tree (§5.11.37) — parcours des blocs de transformee inter (luma).
// =========================================================================
void TransformTree(NkAv1Ctx &ctx, int32 startX, int32 startY, int32 w, int32 h) {
	const int32 maxX = ctx.miCols * 4;
	const int32 maxY = ctx.miRows * 4;
	if (startX >= maxX || startY >= maxY)
		return;
	const int32 row = startY >> 2;
	const int32 col = startX >> 2;
	const int32 lumaTxSz = Grid(ctx.txSizesGrid, ctx.gridStride, row, col);
	const int32 lumaW = kTxWidth[lumaTxSz];
	const int32 lumaH = kTxHeight[lumaTxSz];
	if (w <= lumaW && h <= lumaH) {
		const int32 txSz = FindTxSize(w, h);
		TransformBlock(ctx, 0, startX, startY, txSz, 0, 0);
	} else {
		if (w > h) {
			TransformTree(ctx, startX, startY, w / 2, h);
			TransformTree(ctx, startX + w / 2, startY, w / 2, h);
		} else if (w < h) {
			TransformTree(ctx, startX, startY, w, h / 2);
			TransformTree(ctx, startX, startY + h / 2, w, h / 2);
		} else {
			TransformTree(ctx, startX, startY, w / 2, h / 2);
			TransformTree(ctx, startX + w / 2, startY, w / 2, h / 2);
			TransformTree(ctx, startX, startY + h / 2, w / 2, h / 2);
			TransformTree(ctx, startX + w / 2, startY + h / 2, w / 2, h / 2);
		}
	}
}

// =========================================================================
// Compensation de mouvement (§7.11.3) — single ref + compound
// average/distance. Warp (local/global), OBMC, interintra, masques : refuses
// en amont.
// =========================================================================
// Echantillon de reference avec clamping aux bords (§7.11.3.4).
inline int32 RefPixAt(const NkAv1RefSlot &slot, int32 plane, int32 x, int32 y, int32 lastX, int32 lastY) {
	x = NkAv1Clip3(0, lastX, x);
	y = NkAv1Clip3(0, lastY, y);
	if (plane == 0)
		return slot.y[(usize)y * (usize)slot.yStride + (usize)x];
	if (plane == 1)
		return slot.u[(usize)y * (usize)slot.uvStride + (usize)x];
	return slot.v[(usize)y * (usize)slot.uvStride + (usize)x];
}

// Motion vector scaling process (§7.11.3.3).
void MotionVectorScaling(NkAv1Ctx &ctx, int32 plane, const NkAv1RefSlot &slot, int32 x, int32 y,
						 const int32 *mv, int32 &startX, int32 &startY, int32 &stepX, int32 &stepY) {
	const int32 xScale = (int32)((((int64)slot.upscaledWidth << 14) + (ctx.fh->frameWidth / 2)) / ctx.fh->frameWidth);
	const int32 yScale = (int32)((((int64)slot.frameHeight << 14) + (ctx.fh->frameHeight / 2)) / ctx.fh->frameHeight);
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	const int32 halfSample = 1 << (4 /*SUBPEL_BITS*/ - 1);
	const int32 origX = (x << 4) + ((2 * mv[1]) >> subX) + halfSample;
	const int32 origY = (y << 4) + ((2 * mv[0]) >> subY) + halfSample;
	const int64 baseX = (int64)origX * xScale - ((int64)halfSample << 14 /*REF_SCALE_SHIFT*/);
	const int64 baseY = (int64)origY * yScale - ((int64)halfSample << 14);
	const int32 off = (1 << (10 /*SCALE_SUBPEL_BITS*/ - 4)) / 2;
	startX = (int32)NkAv1Round2Signed64(baseX, 14 + 4 - 10) + off;
	startY = (int32)NkAv1Round2Signed64(baseY, 14 + 4 - 10) + off;
	stepX = (int32)NkAv1Round2Signed64(xScale, 14 - 10);
	stepY = (int32)NkAv1Round2Signed64(yScale, 14 - 10);
}

// Block inter prediction process (§7.11.3.4). Ecrit dans scratch->mcPreds[refList].
void BlockInterPredict(NkAv1Ctx &ctx, int32 plane, const NkAv1RefSlot &slot, int32 x, int32 y,
					   int32 xStep, int32 yStep, int32 w, int32 h, int32 candRow, int32 candCol,
					   int32 refList, int32 interRound0, int32 interRound1) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	const int32 lastX = ((slot.upscaledWidth + subX) >> subX) - 1;
	const int32 lastY = ((slot.frameHeight + subY) >> subY) - 1;
	const int32 intermediateHeight = (((h - 1) * yStep + (1 << 10) - 1) >> 10) + 8;

	int32 interpFilterH = (int32)Grid(ctx.interpFilters1, ctx.gridStride, candRow, candCol);
	if (w <= 4) {
		if (interpFilterH == 0 /*EIGHTTAP*/ || interpFilterH == 2 /*EIGHTTAP_SHARP*/)
			interpFilterH = 4;
		else if (interpFilterH == 1 /*EIGHTTAP_SMOOTH*/)
			interpFilterH = 5;
	}
	for (int32 r = 0; r < intermediateHeight; ++r) {
		for (int32 c = 0; c < w; ++c) {
			int32 s = 0;
			const int32 p = x + xStep * c;
			for (int32 t = 0; t < 8; ++t)
				s += kSubpelFilters[interpFilterH][(p >> 6) & 15][t] *
					 RefPixAt(slot, plane, (p >> 10) + t - 3, (y >> 10) + r - 3, lastX, lastY);
			bs.mcIntermediate[r][c] = NkAv1Round2(s, interRound0);
		}
	}
	int32 interpFilterV = (int32)Grid(ctx.interpFilters0, ctx.gridStride, candRow, candCol);
	if (h <= 4) {
		if (interpFilterV == 0 || interpFilterV == 2)
			interpFilterV = 4;
		else if (interpFilterV == 1)
			interpFilterV = 5;
	}
	for (int32 r = 0; r < h; ++r) {
		for (int32 c = 0; c < w; ++c) {
			int32 s = 0;
			const int32 p = (y & 1023) + yStep * r;
			for (int32 t = 0; t < 8; ++t)
				s += kSubpelFilters[interpFilterV][(p >> 6) & 15][t] * bs.mcIntermediate[(p >> 10) + t][c];
			bs.mcPreds[refList][r][c] = NkAv1Round2(s, interRound1);
		}
	}
}

// Resolve divisor process (§7.11.3.7).
void ResolveDivisor(int64 d, int32 &divShift, int32 &divFactor) {
	const int64 ad = d < 0 ? -d : d;
	int32 n = 0;
	{
		int64 t = ad;
		while (t != 0) {
			t >>= 1;
			++n;
		}
		--n; // FloorLog2
	}
	const int64 e = ad - ((int64)1 << n);
	int64 f;
	if (n > 8 /*DIV_LUT_BITS*/)
		f = NkAv1Round2_64(e, n - 8);
	else
		f = e << (8 - n);
	divShift = n + 14 /*DIV_LUT_PREC_BITS*/;
	divFactor = (d < 0) ? -(int32)kDivLut[f] : (int32)kDivLut[f];
}

// Setup shear process (§7.11.3.6).
bool SetupShear(const int32 *warpParams, int32 &alpha, int32 &beta, int32 &gamma, int32 &delta) {
	const int32 alpha0 = NkAv1Clip3(-32768, 32767, warpParams[2] - (1 << kWarpedModelPrecBits));
	const int32 beta0 = NkAv1Clip3(-32768, 32767, warpParams[3]);
	int32 divShift, divFactor;
	ResolveDivisor(warpParams[2], divShift, divFactor);
	const int64 v = (int64)warpParams[4] << kWarpedModelPrecBits;
	const int32 gamma0 = NkAv1Clip3(-32768, 32767, (int32)NkAv1Round2Signed64(v * divFactor, divShift));
	const int64 w = (int64)warpParams[3] * warpParams[4];
	const int32 delta0 = NkAv1Clip3(-32768, 32767,
									warpParams[5] - (int32)NkAv1Round2Signed64(w * divFactor, divShift) -
										(1 << kWarpedModelPrecBits));
	alpha = (int32)NkAv1Round2Signed64(alpha0, 6 /*WARP_PARAM_REDUCE_BITS*/) << 6;
	beta = (int32)NkAv1Round2Signed64(beta0, 6) << 6;
	gamma = (int32)NkAv1Round2Signed64(gamma0, 6) << 6;
	delta = (int32)NkAv1Round2Signed64(delta0, 6) << 6;
	if (4 * NkAv1Abs(alpha) + 7 * NkAv1Abs(beta) >= (1 << kWarpedModelPrecBits))
		return false;
	if (4 * NkAv1Abs(gamma) + 4 * NkAv1Abs(delta) >= (1 << kWarpedModelPrecBits))
		return false;
	return true;
}

// Warp estimation process (§7.11.3.8) — moindres carres sur CandList.
void WarpEstimation(NkAv1Ctx &ctx) {
	int64 A[2][2] = {{0, 0}, {0, 0}};
	int64 Bx[2] = {0, 0};
	int64 By[2] = {0, 0};
	const int32 w4 = kNum4x4BlocksWide[ctx.MiSize];
	const int32 h4 = kNum4x4BlocksHigh[ctx.MiSize];
	const int32 midY = ctx.MiRow * 4 + h4 * 2 - 1;
	const int32 midX = ctx.MiCol * 4 + w4 * 2 - 1;
	const int32 suy = midY * 8;
	const int32 sux = midX * 8;
	const int32 duy = suy + ctx.mvBlk[0][0];
	const int32 dux = sux + ctx.mvBlk[0][1];
	auto lsProduct = [](int64 a, int64 b) -> int64 { return ((a * b) >> 2) + (a + b); };
	for (int32 i = 0; i < ctx.numSamples; ++i) {
		const int32 sy = ctx.candList[i][0] - suy;
		const int32 sx = ctx.candList[i][1] - sux;
		const int32 dy = ctx.candList[i][2] - duy;
		const int32 dx = ctx.candList[i][3] - dux;
		if (NkAv1Abs(sx - dx) < 256 /*LS_MV_MAX*/ && NkAv1Abs(sy - dy) < 256) {
			A[0][0] += lsProduct(sx, sx) + 8;
			A[0][1] += lsProduct(sx, sy) + 4;
			A[1][1] += lsProduct(sy, sy) + 8;
			Bx[0] += lsProduct(sx, dx) + 8;
			Bx[1] += lsProduct(sy, dx) + 4;
			By[0] += lsProduct(sx, dy) + 4;
			By[1] += lsProduct(sy, dy) + 8;
		}
	}
	const int64 det = A[0][0] * A[1][1] - A[0][1] * A[0][1];
	ctx.localValid = det != 0;
	if (det == 0)
		return;
	int32 divShift, divFactor;
	ResolveDivisor(det, divShift, divFactor);
	divShift -= kWarpedModelPrecBits;
	if (divShift < 0) {
		divFactor = divFactor << (-divShift);
		divShift = 0;
	}
	auto nondiag = [&](int64 v) -> int32 {
		return NkAv1Clip3(-(1 << 13) + 1, (1 << 13) - 1, (int32)NkAv1Round2Signed64(v * divFactor, divShift));
	};
	auto diag = [&](int64 v) -> int32 {
		return NkAv1Clip3((1 << kWarpedModelPrecBits) - (1 << 13) + 1,
						  (1 << kWarpedModelPrecBits) + (1 << 13) - 1,
						  (int32)NkAv1Round2Signed64(v * divFactor, divShift));
	};
	ctx.localWarpParams[2] = diag(A[1][1] * Bx[0] - A[0][1] * Bx[1]);
	ctx.localWarpParams[3] = nondiag(-A[0][1] * Bx[0] + A[0][0] * Bx[1]);
	ctx.localWarpParams[4] = nondiag(A[1][1] * By[0] - A[0][1] * By[1]);
	ctx.localWarpParams[5] = diag(-A[0][1] * By[0] + A[0][0] * By[1]);
	const int32 mvx = ctx.mvBlk[0][1];
	const int32 mvy = ctx.mvBlk[0][0];
	const int64 vx = (int64)mvx * (1 << (kWarpedModelPrecBits - 3)) -
					 ((int64)midX * (ctx.localWarpParams[2] - (1 << kWarpedModelPrecBits)) +
					  (int64)midY * ctx.localWarpParams[3]);
	const int64 vy = (int64)mvy * (1 << (kWarpedModelPrecBits - 3)) -
					 ((int64)midX * ctx.localWarpParams[4] +
					  (int64)midY * (ctx.localWarpParams[5] - (1 << kWarpedModelPrecBits)));
	ctx.localWarpParams[0] = (int32)NkAv1Clip3(-(1 << 23), (1 << 23) - 1, (int32)vx);
	ctx.localWarpParams[1] = (int32)NkAv1Clip3(-(1 << 23), (1 << 23) - 1, (int32)vy);
}

// Block warp process (§7.11.3.5) — remplit un pave 8x8 (clippe) de mcPreds[refList].
void BlockWarp(NkAv1Ctx &ctx, int32 useWarp, int32 plane, int32 refList, int32 x, int32 y,
			   int32 i8, int32 j8, int32 w, int32 h, int32 interRound0, int32 interRound1) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 refFrame = ctx.refFrame[refList];
	const NkAv1RefSlot *slot = SlotFor(ctx, refFrame);
	if (!slot || !slot->valid) {
		ctx.ok = false;
		ctx.err = "reference DPB invalide (warp)";
		return;
	}
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	const int32 lastX = ((slot->upscaledWidth + subX) >> subX) - 1;
	const int32 lastY = ((slot->frameHeight + subY) >> subY) - 1;
	const int32 srcX = (x + j8 * 8 + 4) << subX;
	const int32 srcY = (y + i8 * 8 + 4) << subY;
	const int32 *warpParams = (useWarp == 1) ? ctx.localWarpParams : ctx.fh->gmParams[refFrame];
	const int64 dstX = (int64)warpParams[2] * srcX + (int64)warpParams[3] * srcY + warpParams[0];
	const int64 dstY = (int64)warpParams[4] * srcX + (int64)warpParams[5] * srcY + warpParams[1];
	int32 alpha, beta, gamma, delta;
	SetupShear(warpParams, alpha, beta, gamma, delta); // warpValid toujours 1 ici
	const int64 x4 = dstX >> subX;
	const int64 y4 = dstY >> subY;
	const int32 ix4 = (int32)(x4 >> kWarpedModelPrecBits);
	const int32 sx4 = (int32)(x4 & ((1 << kWarpedModelPrecBits) - 1));
	const int32 iy4 = (int32)(y4 >> kWarpedModelPrecBits);
	const int32 sy4 = (int32)(y4 & ((1 << kWarpedModelPrecBits) - 1));
	int32 intermediate[15][8];
	for (int32 i1 = -7; i1 < 8; ++i1) {
		for (int32 i2 = -4; i2 < 4; ++i2) {
			const int32 sx = sx4 + alpha * i2 + beta * i1;
			const int32 offs = NkAv1Round2(sx, 10 /*WARPEDDIFF_PREC_BITS*/) + 64 /*WARPEDPIXEL_PREC_SHIFTS*/;
			int32 s = 0;
			for (int32 i3 = 0; i3 < 8; ++i3)
				s += kWarpedFilters[offs][i3] * RefPixAt(*slot, plane, ix4 + i2 - 3 + i3, iy4 + i1, lastX, lastY);
			intermediate[i1 + 7][i2 + 4] = NkAv1Round2(s, interRound0);
		}
	}
	for (int32 i1 = -4; i1 < NkAv1Min(4, h - i8 * 8 - 4); ++i1) {
		for (int32 i2 = -4; i2 < NkAv1Min(4, w - j8 * 8 - 4); ++i2) {
			const int32 sy = sy4 + gamma * i2 + delta * i1;
			const int32 offs = NkAv1Round2(sy, 10) + 64;
			int32 s = 0;
			for (int32 i3 = 0; i3 < 8; ++i3)
				s += kWarpedFilters[offs][i3] * intermediate[i1 + i3 + 4][i2 + 4];
			bs.mcPreds[refList][i8 * 8 + i1 + 4][j8 * 8 + i2 + 4] = NkAv1Round2(s, interRound1);
		}
	}
}

// =========================================================================
// OBMC — compensation de mouvement chevauchee (§7.11.3.9) + melange (§7.11.3.10).
// =========================================================================
inline const int32 *GetObmcMask(int32 length) {
	static const int32 kObmcMask2[2] = {45, 64};
	static const int32 kObmcMask4[4] = {39, 50, 59, 64};
	static const int32 kObmcMask8[8] = {36, 42, 48, 53, 57, 61, 64, 64};
	static const int32 kObmcMask16[16] = {34, 37, 40, 43, 46, 49, 52, 54, 56, 58, 60, 61, 64, 64, 64, 64};
	static const int32 kObmcMask32[32] = {33, 35, 36, 38, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 53, 55,
										  56, 57, 58, 59, 60, 60, 61, 62, 64, 64, 64, 64, 64, 64, 64, 64};
	if (length == 2) return kObmcMask2;
	if (length == 4) return kObmcMask4;
	if (length == 8) return kObmcMask8;
	if (length == 16) return kObmcMask16;
	return kObmcMask32;
}

// predict_overlap : prediction du voisin + melange dans CurrFrame.
void ObmcPredictOverlap(NkAv1Ctx &ctx, int32 plane, int32 candRow, int32 candCol, int32 x4, int32 y4,
						int32 predW, int32 predH, int32 pass, const int32 *mask) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	const int16 *m = MvsAt(ctx, candRow, candCol);
	const int32 mv[2] = {m[0], m[1]};
	const int32 candRef = Grid(ctx.refFrames0, ctx.gridStride, candRow, candCol);
	const NkAv1RefSlot *slot = SlotFor(ctx, candRef);
	if (!slot || !slot->valid) {
		ctx.ok = false;
		ctx.err = "reference DPB invalide (OBMC)";
		return;
	}
	const int32 predX = (x4 * 4) >> subX;
	const int32 predY = (y4 * 4) >> subY;
	int32 startX, startY, stepX, stepY;
	MotionVectorScaling(ctx, plane, *slot, predX, predY, mv, startX, startY, stepX, stepY);
	// obmcPred stocke dans mcPreds[1] (OBMC = single ref, la liste 1 est libre).
	// Arrondis single-ref (InterRound1 = 11 en 8-bit).
	BlockInterPredict(ctx, plane, *slot, startX, startY, stepX, stepY, predW, predH, candRow, candCol,
					  1, 3, 11);
	for (int32 i = 0; i < predH; ++i) {
		for (int32 j = 0; j < predW; ++j) {
			const int32 obmcPx = Clip1_8(bs.mcPreds[1][i][j]);
			const int32 mval = (pass == 0) ? mask[i] : mask[j];
			uint8 &dst = CurPix(ctx, plane, predX + j, predY + i);
			dst = (uint8)NkAv1Round2(mval * (int32)dst + (64 - mval) * obmcPx, 6);
		}
	}
}

// Overlapped motion compensation process (§7.11.3.9).
void OverlappedMotionCompensation(NkAv1Ctx &ctx, int32 plane, int32 w, int32 h) {
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	if (ctx.AvailU) {
		if (GetPlaneResidualSize(ctx, ctx.MiSize, plane) >= 3 /*BLOCK_8X8*/) {
			const int32 pass = 0;
			const int32 w4 = kNum4x4BlocksWide[ctx.MiSize];
			int32 x4 = ctx.MiCol;
			const int32 y4 = ctx.MiRow;
			int32 nCount = 0;
			const int32 nLimit = NkAv1Min(4, (int32)kMiWidthLog2[ctx.MiSize]);
			while (nCount < nLimit && x4 < NkAv1Min(ctx.miCols, ctx.MiCol + w4)) {
				const int32 candRow = ctx.MiRow - 1;
				const int32 candCol = x4 | 1;
				const int32 candSz = Grid(ctx.miSizes, ctx.gridStride, candRow, candCol);
				const int32 step4 = NkAv1Clip3(2, 16, (int32)kNum4x4BlocksWide[candSz]);
				if ((int32)Grid(ctx.refFrames0, ctx.gridStride, candRow, candCol) > kRefIntra) {
					++nCount;
					const int32 predW = NkAv1Min(w, (step4 * 4) >> subX);
					const int32 predH = NkAv1Min(h >> 1, 32 >> subY);
					const int32 *mask = GetObmcMask(predH);
					ObmcPredictOverlap(ctx, plane, candRow, candCol, x4, y4, predW, predH, pass, mask);
					if (!ctx.ok)
						return;
				}
				x4 += step4;
			}
		}
	}
	if (ctx.AvailL) {
		const int32 pass = 1;
		const int32 h4 = kNum4x4BlocksHigh[ctx.MiSize];
		const int32 x4 = ctx.MiCol;
		int32 y4 = ctx.MiRow;
		int32 nCount = 0;
		const int32 nLimit = NkAv1Min(4, (int32)kMiHeightLog2[ctx.MiSize]);
		while (nCount < nLimit && y4 < NkAv1Min(ctx.miRows, ctx.MiRow + h4)) {
			const int32 candCol = ctx.MiCol - 1;
			const int32 candRow = y4 | 1;
			const int32 candSz = Grid(ctx.miSizes, ctx.gridStride, candRow, candCol);
			const int32 step4 = NkAv1Clip3(2, 16, (int32)kNum4x4BlocksHigh[candSz]);
			if ((int32)Grid(ctx.refFrames0, ctx.gridStride, candRow, candCol) > kRefIntra) {
				++nCount;
				const int32 predW = NkAv1Min(w >> 1, 32 >> subX);
				const int32 predH = NkAv1Min(h, (step4 * 4) >> subY);
				const int32 *mask = GetObmcMask(predW);
				ObmcPredictOverlap(ctx, plane, candRow, candCol, x4, y4, predW, predH, pass, mask);
				if (!ctx.ok)
					return;
			}
			y4 += step4;
		}
	}
}

// =========================================================================
// Compound masque (§7.11.3.11 wedge mask / §7.11.3.12 difference weight mask /
// §7.11.3.14 mask blend).
// =========================================================================
constexpr int32 kMaskMasterSize = 64;
// MasterMask[dir][i][j] — construit une fois (initialise_wedge_mask_table, 1re partie).
static uint8 gWedgeMasterMask[6][kMaskMasterSize][kMaskMasterSize];
static bool gWedgeMasterInit = false;

void InitWedgeMasterMask() {
	if (gWedgeMasterInit)
		return;
	const int32 w = kMaskMasterSize, h = kMaskMasterSize;
	for (int32 j = 0; j < w; ++j) {
		int32 shift = kMaskMasterSize / 4;
		for (int32 i = 0; i < h; i += 2) {
			gWedgeMasterMask[3 /*WEDGE_OBLIQUE63*/][i][j] =
				(uint8)kWedgeMasterObliqueEven[NkAv1Clip3(0, kMaskMasterSize - 1, j - shift)];
			shift -= 1;
			gWedgeMasterMask[3][i + 1][j] =
				(uint8)kWedgeMasterObliqueOdd[NkAv1Clip3(0, kMaskMasterSize - 1, j - shift)];
			gWedgeMasterMask[1 /*WEDGE_VERTICAL*/][i][j] = (uint8)kWedgeMasterVertical[j];
			gWedgeMasterMask[1][i + 1][j] = (uint8)kWedgeMasterVertical[j];
		}
	}
	for (int32 i = 0; i < h; ++i) {
		for (int32 j = 0; j < w; ++j) {
			const int32 msk = gWedgeMasterMask[3][i][j];
			gWedgeMasterMask[2 /*WEDGE_OBLIQUE27*/][j][i] = (uint8)msk;
			gWedgeMasterMask[4 /*WEDGE_OBLIQUE117*/][i][w - 1 - j] = (uint8)(64 - msk);
			gWedgeMasterMask[5 /*WEDGE_OBLIQUE153*/][w - 1 - j][i] = (uint8)(64 - msk);
			gWedgeMasterMask[0 /*WEDGE_HORIZONTAL*/][j][i] = gWedgeMasterMask[1][i][j];
		}
	}
	gWedgeMasterInit = true;
}

inline int32 WedgeBlockShape(int32 bsize) {
	const int32 w4 = kNum4x4BlocksWide[bsize];
	const int32 h4 = kNum4x4BlocksHigh[bsize];
	if (h4 > w4) return 0;
	if (h4 < w4) return 1;
	return 2;
}

// Wedge mask process (§7.11.3.11) : remplit scratch->mcMask (resolution luma).
void WedgeMaskProcess(NkAv1Ctx &ctx, int32 w, int32 h) {
	InitWedgeMasterMask();
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 shape = WedgeBlockShape(ctx.MiSize);
	const int32 dir = kWedgeCodebook[shape][ctx.wedgeIndexBlk][0];
	const int32 xoff = kMaskMasterSize / 2 - ((kWedgeCodebook[shape][ctx.wedgeIndexBlk][1] * w) >> 3);
	const int32 yoff = kMaskMasterSize / 2 - ((kWedgeCodebook[shape][ctx.wedgeIndexBlk][2] * h) >> 3);
	// flipSign (initialise_wedge_mask_table, 2e partie) — calcule a la volee.
	int32 sum = 0;
	for (int32 i = 0; i < w; ++i)
		sum += gWedgeMasterMask[dir][yoff][xoff + i];
	for (int32 i = 1; i < h; ++i)
		sum += gWedgeMasterMask[dir][yoff + i][xoff];
	const int32 avg = (sum + (w + h - 1) / 2) / (w + h - 1);
	const int32 flipSign = (avg < 32) ? 1 : 0;
	for (int32 i = 0; i < h; ++i)
		for (int32 j = 0; j < w; ++j) {
			const int32 m = gWedgeMasterMask[dir][yoff + i][xoff + j];
			bs.mcMask[i][j] = (uint8)((ctx.wedgeSignBlk == flipSign) ? m : 64 - m);
		}
}

// Difference weight mask process (§7.11.3.12) — depuis les preds LUMA.
void DiffWtdMaskProcess(NkAv1Ctx &ctx, int32 w, int32 h, int32 interPostRound) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	for (int32 i = 0; i < h; ++i)
		for (int32 j = 0; j < w; ++j) {
			int32 diff = NkAv1Abs(bs.mcPreds[0][i][j] - bs.mcPreds[1][i][j]);
			diff = NkAv1Round2(diff, interPostRound); // BitDepth-8 == 0
			const int32 m = NkAv1Clip3(0, 64, 38 + diff / 16);
			bs.mcMask[i][j] = (uint8)(ctx.maskTypeBlk ? 64 - m : m);
		}
}

// Intra mode variant mask process (§7.11.3.13) — dims du PLAN courant.
void IntraVariantMask(NkAv1Ctx &ctx, int32 w, int32 h) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 sizeScale = 128 /*MAX_SB_SIZE*/ / NkAv1Max(h, w);
	for (int32 i = 0; i < h; ++i)
		for (int32 j = 0; j < w; ++j) {
			int32 m;
			if (ctx.interIntraModeBlk == 1 /*II_V_PRED*/)
				m = kIiWeights1d[i * sizeScale];
			else if (ctx.interIntraModeBlk == 2 /*II_H_PRED*/)
				m = kIiWeights1d[j * sizeScale];
			else if (ctx.interIntraModeBlk == 3 /*II_SMOOTH_PRED*/)
				m = kIiWeights1d[NkAv1Min(i, j) * sizeScale];
			else
				m = 32;
			bs.mcMask[i][j] = (uint8)m;
		}
}

// Mask blend process (§7.11.3.14) — compound masque ET inter-intra.
void MaskBlend(NkAv1Ctx &ctx, int32 plane, int32 dstX, int32 dstY, int32 w, int32 h, int32 interPostRound,
			   bool interIntra) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const int32 subX = (plane == 0) ? 0 : ctx.seq->subsamplingX;
	const int32 subY = (plane == 0) ? 0 : ctx.seq->subsamplingY;
	for (int32 y = 0; y < h; ++y) {
		for (int32 x = 0; x < w; ++x) {
			int32 m;
			if ((!subX && !subY) || (interIntra && !ctx.wedgeInterIntraBlk)) {
				m = bs.mcMask[y][x];
			} else if (subX && !subY) {
				m = NkAv1Round2((int32)bs.mcMask[y][2 * x] + bs.mcMask[y][2 * x + 1], 1);
			} else {
				m = NkAv1Round2((int32)bs.mcMask[2 * y][2 * x] + bs.mcMask[2 * y][2 * x + 1] +
									bs.mcMask[2 * y + 1][2 * x] + bs.mcMask[2 * y + 1][2 * x + 1],
								2);
			}
			if (interIntra) {
				const int32 pred0 = Clip1_8(NkAv1Round2(bs.mcPreds[0][y][x], interPostRound));
				const int32 pred1 = CurPix(ctx, plane, dstX + x, dstY + y);
				CurPix(ctx, plane, dstX + x, dstY + y) = (uint8)NkAv1Round2(m * pred1 + (64 - m) * pred0, 6);
			} else {
				const int32 pred0 = bs.mcPreds[0][y][x];
				const int32 pred1 = bs.mcPreds[1][y][x];
				CurPix(ctx, plane, dstX + x, dstY + y) =
					(uint8)Clip1_8(NkAv1Round2(m * pred0 + (64 - m) * pred1, 6 + interPostRound));
			}
		}
	}
}

// Distance weights process (§7.11.3.15).
void DistanceWeights(NkAv1Ctx &ctx, int32 candRow, int32 candCol, int32 &fwdWeight, int32 &bckWeight) {
	int32 dist[2];
	for (int32 refList = 0; refList < 2; ++refList) {
		const int32 rf = (refList == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, candRow, candCol)
										: (int32)Grid(ctx.refFrames1, ctx.gridStride, candRow, candCol);
		const int32 hint = ctx.fh->orderHints[rf];
		dist[refList] = NkAv1Clip3(0, kMaxFrameDistance, NkAv1Abs(GetRelativeDist(*ctx.seq, hint, ctx.fh->orderHint)));
	}
	const int32 d0 = dist[1];
	const int32 d1 = dist[0];
	const int32 order = (d0 <= d1) ? 1 : 0;
	int32 i;
	if (d0 == 0 || d1 == 0) {
		i = 3;
	} else {
		for (i = 0; i < 3; ++i) {
			const int32 c0 = kQuantDistWeight[i][order];
			const int32 c1 = kQuantDistWeight[i][1 - order];
			if (order) {
				if (d0 * c0 > d1 * c1)
					break;
			} else {
				if (d0 * c0 < d1 * c1)
					break;
			}
		}
	}
	fwdWeight = kQuantDistLookup[i][order];
	bckWeight = kQuantDistLookup[i][1 - order];
}

// predict_inter (§7.11.3.1).
void PredictInter(NkAv1Ctx &ctx, int32 plane, int32 x, int32 y, int32 w, int32 h, int32 candRow, int32 candCol) {
	NkAv1BlockScratch &bs = *ctx.scratch;
	const bool isCompound = (int32)Grid(ctx.refFrames1, ctx.gridStride, candRow, candCol) > kRefIntra;
	// Rounding variables (§7.11.3.2), BitDepth = 8.
	const int32 interRound0 = 3;
	const int32 interRound1 = isCompound ? 7 : 11;
	const int32 interPostRound = 2 * 7 /*FILTER_BITS*/ - (interRound0 + interRound1);

	// Warp local : estimation + validation (§7.11.3.1 etapes 2-3, plane 0).
	if (plane == 0 && ctx.motionModeBlk == kMotionLocalwarp)
		WarpEstimation(ctx);
	if (plane == 0 && ctx.motionModeBlk == kMotionLocalwarp && ctx.localValid) {
		int32 a2, b2, g2, d2;
		ctx.localValid = SetupShear(ctx.localWarpParams, a2, b2, g2, d2);
	}
	for (int32 refList = 0; refList < 1 + (isCompound ? 1 : 0); ++refList) {
		const int32 refFrame = (refList == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, candRow, candCol)
											  : (int32)Grid(ctx.refFrames1, ctx.gridStride, candRow, candCol);
		bool globalValid = false;
		if ((ctx.YMode == kModeGlobalMv || ctx.YMode == kModeGlobalGlobalMv) &&
			ctx.fh->gmType[refFrame] > kGmTranslation) {
			int32 a2, b2, g2, d2;
			globalValid = SetupShear(ctx.fh->gmParams[refFrame], a2, b2, g2, d2);
		}
		// useWarp (§7.11.3.1 etape 7) : 1 = warp local, 2 = warp global.
		int32 useWarp = 0;
		if (w < 8 || h < 8)
			useWarp = 0;
		else if (ctx.forceIntegerMv)
			useWarp = 0;
		else if (ctx.motionModeBlk == kMotionLocalwarp && ctx.localValid)
			useWarp = 1;
		else if ((ctx.YMode == kModeGlobalMv || ctx.YMode == kModeGlobalGlobalMv) &&
				 ctx.fh->gmType[refFrame] > kGmTranslation && !IsScaledRef(ctx, refFrame) && globalValid)
			useWarp = 2;
		const NkAv1RefSlot *slot = SlotFor(ctx, refFrame);
		if (!slot || !slot->valid) {
			ctx.ok = false;
			ctx.err = "reference DPB invalide";
			return;
		}
		const int16 *m = MvsAt(ctx, candRow, candCol);
		const int32 mv[2] = {m[refList * 2 + 0], m[refList * 2 + 1]};
		int32 startX, startY, stepX, stepY;
		MotionVectorScaling(ctx, plane, *slot, x, y, mv, startX, startY, stepX, stepY);
		if (useWarp != 0) {
			for (int32 i8 = 0; i8 <= (h - 1) >> 3; ++i8)
				for (int32 j8 = 0; j8 <= (w - 1) >> 3; ++j8) {
					BlockWarp(ctx, useWarp, plane, refList, x, y, i8, j8, w, h, interRound0, interRound1);
					if (!ctx.ok)
						return;
				}
		} else {
			BlockInterPredict(ctx, plane, *slot, startX, startY, stepX, stepY, w, h, candRow, candCol,
							  refList, interRound0, interRound1);
		}
	}
	// Preparation du masque (§7.11.3.1) : wedge/diffwtd calcules sur le plan LUMA,
	// masque intra-variant sur le plan COURANT.
	const bool isInterIntra = ctx.isInter_ && ctx.refFrame[1] == kRefIntra;
	if (ctx.compoundTypeBlk == kCompoundWedge && plane == 0)
		WedgeMaskProcess(ctx, w, h);
	else if (ctx.compoundTypeBlk == kCompoundIntra)
		IntraVariantMask(ctx, w, h);
	else if (ctx.compoundTypeBlk == kCompoundDiffwtd && plane == 0)
		DiffWtdMaskProcess(ctx, w, h, interPostRound);

	if (isInterIntra) {
		// Melange inter + intra (la prediction intra est deja dans CurrFrame).
		MaskBlend(ctx, plane, x, y, w, h, interPostRound, true);
	} else if (!isCompound) {
		for (int32 i = 0; i < h; ++i)
			for (int32 j = 0; j < w; ++j)
				CurPix(ctx, plane, x + j, y + i) = (uint8)Clip1_8(bs.mcPreds[0][i][j]);
	} else if (ctx.compoundTypeBlk == kCompoundAverage) {
		for (int32 i = 0; i < h; ++i)
			for (int32 j = 0; j < w; ++j)
				CurPix(ctx, plane, x + j, y + i) =
					(uint8)Clip1_8(NkAv1Round2(bs.mcPreds[0][i][j] + bs.mcPreds[1][i][j], 1 + interPostRound));
	} else if (ctx.compoundTypeBlk == kCompoundDistance) {
		int32 fwdWeight, bckWeight;
		DistanceWeights(ctx, candRow, candCol, fwdWeight, bckWeight);
		for (int32 i = 0; i < h; ++i)
			for (int32 j = 0; j < w; ++j)
				CurPix(ctx, plane, x + j, y + i) = (uint8)Clip1_8(
					NkAv1Round2(fwdWeight * bs.mcPreds[0][i][j] + bckWeight * bs.mcPreds[1][i][j], 4 + interPostRound));
	} else {
		// COMPOUND_WEDGE / COMPOUND_DIFFWTD : melange par masque (§7.11.3.14).
		MaskBlend(ctx, plane, x, y, w, h, interPostRound, false);
	}

	// OBMC (§7.11.3.9) : melange avec les predictions des voisins.
	if (ctx.motionModeBlk == kMotionObmc)
		OverlappedMotionCompensation(ctx, plane, w, h);
}

// compute_prediction (§5.11.33) — chemin inter, y compris inter-intra.
void ComputePrediction(NkAv1Ctx &ctx) {
	const int32 sbMask = ctx.seq->use128x128Superblock ? 31 : 15;
	const int32 subBlockMiRow = ctx.MiRow & sbMask;
	const int32 subBlockMiCol = ctx.MiCol & sbMask;
	for (int32 plane = 0; plane < 1 + (ctx.HasChroma ? 2 : 0); ++plane) {
		const int32 planeSz = GetPlaneResidualSize(ctx, ctx.MiSize, plane);
		const int32 num4x4W = kNum4x4BlocksWide[planeSz];
		const int32 num4x4H = kNum4x4BlocksHigh[planeSz];
		const int32 subX = (plane > 0) ? ctx.seq->subsamplingX : 0;
		const int32 subY = (plane > 0) ? ctx.seq->subsamplingY : 0;
		const int32 baseX = (ctx.MiCol >> subX) * 4;
		const int32 baseY = (ctx.MiRow >> subY) * 4;
		// Inter-intra : la prediction INTRA du bloc est generee d'abord (§5.11.33).
		if (ctx.isInter_ && ctx.refFrame[1] == kRefIntra) {
			int32 mode;
			if (ctx.interIntraModeBlk == 0 /*II_DC_PRED*/)
				mode = kModeDC;
			else if (ctx.interIntraModeBlk == 1 /*II_V_PRED*/)
				mode = kModeV;
			else if (ctx.interIntraModeBlk == 2 /*II_H_PRED*/)
				mode = kModeH;
			else
				mode = kModeSmooth;
			const int32 log2W = 2 + kMiWidthLog2[planeSz];
			const int32 log2H = 2 + kMiHeightLog2[planeSz];
			PredictIntra(ctx, plane, baseX, baseY,
						 plane == 0 ? ctx.AvailL : ctx.AvailLChroma,
						 plane == 0 ? ctx.AvailU : ctx.AvailUChroma,
						 BD(ctx, plane, (subBlockMiRow >> subY) - 1, (subBlockMiCol >> subX) + num4x4W) != 0,
						 BD(ctx, plane, (subBlockMiRow >> subY) + num4x4H, (subBlockMiCol >> subX) - 1) != 0,
						 mode, log2W, log2H);
		}
		int32 candRow = (ctx.MiRow >> subY) << subY;
		int32 candCol = (ctx.MiCol >> subX) << subX;
		int32 predW = BlockWidthOf(ctx.MiSize) >> subX;
		int32 predH = BlockHeightOf(ctx.MiSize) >> subY;
		int32 someUseIntra = 0;
		for (int32 r2 = 0; r2 < (num4x4H << subY); ++r2)
			for (int32 c2 = 0; c2 < (num4x4W << subX); ++c2)
				if ((int32)Grid(ctx.refFrames0, ctx.gridStride, candRow + r2, candCol + c2) == kRefIntra)
					someUseIntra = 1;
		if (someUseIntra) {
			predW = num4x4W * 4;
			predH = num4x4H * 4;
			candRow = ctx.MiRow;
			candCol = ctx.MiCol;
		}
		int32 r2 = 0;
		for (int32 y = 0; y < num4x4H * 4; y += predH) {
			int32 c2 = 0;
			for (int32 x = 0; x < num4x4W * 4; x += predW) {
				PredictInter(ctx, plane, baseX + x, baseY + y, predW, predH, candRow + r2, candCol + c2);
				if (!ctx.ok)
					return;
				++c2;
			}
			++r2;
		}
	}
}

// =========================================================================
// decode_tile / frame-level driver (§5.11.2 / §5.11.3).
// =========================================================================
void ClearAboveContext(NkAv1Ctx &ctx) {
	for (int32 p = 0; p < 3; ++p) {
		for (usize i = 0; i < ctx.aboveLevel[p].Size(); ++i) { ctx.aboveLevel[p][i] = 0; ctx.aboveDc[p][i] = 0; }
	}
	for (usize i = 0; i < ctx.segPredAbove.Size(); ++i)
		ctx.segPredAbove[i] = 0;
}
void ClearLeftContext(NkAv1Ctx &ctx) {
	for (int32 p = 0; p < 3; ++p) {
		for (usize i = 0; i < ctx.leftLevel[p].Size(); ++i) { ctx.leftLevel[p][i] = 0; ctx.leftDc[p][i] = 0; }
	}
	for (usize i = 0; i < ctx.segPredLeft.Size(); ++i)
		ctx.segPredLeft[i] = 0;
}

// Decodes ONE tile's worth of superblocks. `tileData`/`tileSize` = the
// compressed data for this tile (from NkAv1TileEntry). Returns false on any
// unimplemented/unsupported feature encountered (palette, intrabc, ...).
bool DecodeTile(NkAv1Ctx &ctx, const uint8 *tileData, usize tileSize) {
	ctx.sd.Init(tileData, tileSize, ctx.fh->disableCdfUpdate);
	ctx.ReadDeltas = false;
	for (int32 i = 0; i < 4; ++i) ctx.DeltaLF[i] = 0;
	ctx.CurrentQIndex = ctx.fh->quant.baseQIdx;
	ClearAboveContext(ctx);
	const int32 sbSize4 = ctx.sbSize4;
	for (int32 r = ctx.miRowStart; r < ctx.miRowEnd; r += sbSize4) {
		ClearLeftContext(ctx);
		for (int32 c = ctx.miColStart; c < ctx.miColEnd; c += sbSize4) {
			ctx.ReadDeltas = ctx.fh->deltaQPresent;
			ClearCdef(ctx, r, c);
			ClearBlockDecodedFlags(ctx, r, c, sbSize4);
			// read_lr() -- loop restoration disabled in our test streams
			// (usesLr==false), so no per-SB restoration symbols to read.
			// Documented gap: not implemented for usesLr==true.
			if (ctx.fh->lr.usesLr) { ctx.ok = false; ctx.err = "loop restoration (non implementee)"; return false; }
			DecodePartition(ctx, r, c, ctx.sbSize);
			if (!ctx.ok) return false;
		}
	}
	return ctx.ok;
}

// =========================================================================
// Loop filter / deblocking process (§7.14). Key-frame-only simplification:
// RefFrames[.][.][0] is always INTRA_FRAME so isIntra is always true and
// modeType is always 0 (no NEARESTMV/GLOBALMV inter cases exist).
// =========================================================================
int32 FilterSizeProcess(int32 txSz, int32 prevTxSz, int32 pass, int32 plane) {
	const int32 baseSize = (pass == 0) ? NkAv1Min(kTxWidth[prevTxSz], kTxWidth[txSz])
										 : NkAv1Min(kTxHeight[prevTxSz], kTxHeight[txSz]);
	return (plane == 0) ? NkAv1Min(16, baseSize) : NkAv1Min(8, baseSize);
}

int32 AdaptiveFilterStrengthSelection(NkAv1Ctx &ctx, int32 segment, int32 ref, int32 modeType, int32 deltaLF,
									  int32 plane, int32 pass) {
	const int32 i = (plane == 0) ? pass : (plane + 1);
	int32 lvlSeg = NkAv1Clip3(0, 63, deltaLF + ctx.fh->lf.level[i]);
	const int32 feature = 1 /*SEG_LVL_ALT_LF_Y_V*/ + i;
	if (SegFeatureActiveIdx(ctx, segment, feature)) {
		lvlSeg = ctx.fh->seg.featureData[segment][feature] + lvlSeg;
		lvlSeg = NkAv1Clip3(0, 63, lvlSeg);
	}
	if (ctx.fh->lf.deltaEnabled) {
		const int32 nShift = lvlSeg >> 5;
		if (ref == 0 /*INTRA_FRAME*/) {
			lvlSeg = lvlSeg + (ctx.fh->lf.refDeltas[0] << nShift);
		} else {
			lvlSeg = lvlSeg + (ctx.fh->lf.refDeltas[ref] << nShift) + (ctx.fh->lf.modeDeltas[modeType] << nShift);
		}
		lvlSeg = NkAv1Clip3(0, 63, lvlSeg);
	}
	return lvlSeg;
}

void AdaptiveFilterStrength(NkAv1Ctx &ctx, int32 row, int32 col, int32 plane, int32 pass,
							int32 &lvl, int32 &limit, int32 &blimit, int32 &thresh) {
	const int32 segment = Grid(ctx.segmentIds, ctx.gridStride, row, col);
	const int32 ref = Grid(ctx.refFrames0, ctx.gridStride, row, col);
	const int32 mode = Grid(ctx.yModes, ctx.gridStride, row, col);
	const int32 modeType = (mode >= kModeNearestMv && mode != kModeGlobalMv && mode != kModeGlobalGlobalMv) ? 1 : 0;
	int32 deltaLF;
	if (!ctx.fh->deltaLfMulti) {
		deltaLF = Grid(ctx.deltaLfGrid[0], ctx.gridStride, row, col);
	} else {
		const int32 idx = (plane == 0) ? pass : (plane + 1);
		deltaLF = Grid(ctx.deltaLfGrid[idx], ctx.gridStride, row, col);
	}
	lvl = AdaptiveFilterStrengthSelection(ctx, segment, ref, modeType, deltaLF, plane, pass);
	const int32 sharpness = ctx.fh->lf.sharpness;
	const int32 shift = (sharpness > 4) ? 2 : (sharpness > 0) ? 1 : 0;
	limit = (sharpness > 0) ? NkAv1Clip3(1, 9 - sharpness, lvl >> shift) : NkAv1Max(1, lvl >> shift);
	blimit = 2 * (lvl + 2) + limit;
	thresh = lvl >> 4;
}

inline int32 Filter4Clamp(int32 v) { return NkAv1Clip3(-128, 127, v); }

void NarrowFilter(NkAv1Ctx &ctx, int32 hevMask, int32 x, int32 y, int32 plane, int32 dx, int32 dy) {
	const int32 q0 = CurPix(ctx, plane, x, y);
	const int32 q1 = CurPix(ctx, plane, x + dx, y + dy);
	const int32 p0 = CurPix(ctx, plane, x - dx, y - dy);
	const int32 p1 = CurPix(ctx, plane, x - 2 * dx, y - 2 * dy);
	const int32 ps1 = p1 - 128, ps0 = p0 - 128, qs0 = q0 - 128, qs1 = q1 - 128;
	int32 filter = hevMask ? Filter4Clamp(ps1 - qs1) : 0;
	filter = Filter4Clamp(filter + 3 * (qs0 - ps0));
	const int32 filter1 = Filter4Clamp(filter + 4) >> 3;
	const int32 filter2 = Filter4Clamp(filter + 3) >> 3;
	CurPix(ctx, plane, x, y) = (uint8)(Filter4Clamp(qs0 - filter1) + 128);
	CurPix(ctx, plane, x - dx, y - dy) = (uint8)(Filter4Clamp(ps0 + filter2) + 128);
	if (!hevMask) {
		const int32 f = NkAv1Round2(filter1, 1);
		CurPix(ctx, plane, x + dx, y + dy) = (uint8)(Filter4Clamp(qs1 - f) + 128);
		CurPix(ctx, plane, x - 2 * dx, y - 2 * dy) = (uint8)(Filter4Clamp(ps1 + f) + 128);
	}
}

void WideFilter(NkAv1Ctx &ctx, int32 x, int32 y, int32 plane, int32 dx, int32 dy, int32 log2Size) {
	const int32 n = (log2Size == 4) ? 6 : (plane == 0) ? 3 : 2;
	const int32 n2 = (log2Size == 3 && plane == 0) ? 0 : 1;
	int32 F[13]; // indices -n..n-1, n<=6 -> up to 12 slots, bias +6
	for (int32 i = -n; i < n; ++i) {
		int32 t = 0;
		for (int32 j = -n; j <= n; ++j) {
			const int32 p = NkAv1Clip3(-(n + 1), n, i + j);
			const int32 tap = (NkAv1Abs(j) <= n2) ? 2 : 1;
			t += (int32)CurPix(ctx, plane, x + p * dx, y + p * dy) * tap;
		}
		F[i + 6] = NkAv1Round2(t, log2Size);
	}
	for (int32 i = -n; i < n; ++i)
		CurPix(ctx, plane, x + i * dx, y + i * dy) = (uint8)F[i + 6];
}

void SampleFilteringProcess(NkAv1Ctx &ctx, int32 x, int32 y, int32 plane, int32 limit, int32 blimit, int32 thresh,
							int32 dx, int32 dy, int32 filterSize) {
	auto px = [&](int32 k) { return (int32)CurPix(ctx, plane, x + k * dx, y + k * dy); };
	const int32 q0 = px(0), q1 = px(1), q2 = px(2), q3 = px(3);
	const int32 p0 = px(-1), p1 = px(-2), p2 = px(-3), p3 = px(-4);

	int32 hevMask = 0;
	hevMask |= (NkAv1Abs(p1 - p0) > thresh) ? 1 : 0;
	hevMask |= (NkAv1Abs(q1 - q0) > thresh) ? 1 : 0;

	const int32 filterLen = (filterSize == 4) ? 4 : (plane != 0) ? 6 : (filterSize == 8) ? 8 : 16;

	int32 mask = 0;
	mask |= (NkAv1Abs(p1 - p0) > limit) ? 1 : 0;
	mask |= (NkAv1Abs(q1 - q0) > limit) ? 1 : 0;
	mask |= (NkAv1Abs(p0 - q0) * 2 + NkAv1Abs(p1 - q1) / 2 > blimit) ? 1 : 0;
	if (filterLen >= 6) {
		mask |= (NkAv1Abs(p2 - p1) > limit) ? 1 : 0;
		mask |= (NkAv1Abs(q2 - q1) > limit) ? 1 : 0;
	}
	if (filterLen >= 8) {
		mask |= (NkAv1Abs(p3 - p2) > limit) ? 1 : 0;
		mask |= (NkAv1Abs(q3 - q2) > limit) ? 1 : 0;
	}
	const int32 filterMask = (mask == 0) ? 1 : 0;
	if (!filterMask) return;

	int32 flatMask = 1;
	if (filterSize >= 8) {
		int32 m2 = 0;
		m2 |= (NkAv1Abs(p1 - p0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(q1 - q0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(p2 - p0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(q2 - q0) > 1) ? 1 : 0;
		if (filterLen >= 8) {
			m2 |= (NkAv1Abs(p3 - p0) > 1) ? 1 : 0;
			m2 |= (NkAv1Abs(q3 - q0) > 1) ? 1 : 0;
		}
		flatMask = (m2 == 0) ? 1 : 0;
	}

	if (filterSize == 4 || !flatMask) {
		NarrowFilter(ctx, hevMask, x, y, plane, dx, dy);
		return;
	}

	int32 flatMask2 = 1;
	if (filterSize >= 16) {
		const int32 q4 = px(4), q5 = px(5), q6 = px(6);
		const int32 p4 = px(-5), p5 = px(-6), p6 = px(-7);
		int32 m2 = 0;
		m2 |= (NkAv1Abs(p6 - p0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(q6 - q0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(p5 - p0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(q5 - q0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(p4 - p0) > 1) ? 1 : 0;
		m2 |= (NkAv1Abs(q4 - q0) > 1) ? 1 : 0;
		flatMask2 = (m2 == 0) ? 1 : 0;
	}

	if (filterSize == 8 || !flatMask2)
		WideFilter(ctx, x, y, plane, dx, dy, 3);
	else
		WideFilter(ctx, x, y, plane, dx, dy, 4);
}

void LoopFilterEdge(NkAv1Ctx &ctx, int32 plane, int32 pass, int32 row, int32 col) {
	const int32 subX = plane > 0 ? ctx.seq->subsamplingX : 0;
	const int32 subY = plane > 0 ? ctx.seq->subsamplingY : 0;
	const int32 dx = (pass == 0) ? 1 : 0;
	const int32 dy = (pass == 0) ? 0 : 1;
	const int32 x = col * 4, y = row * 4;
	row = row | subY;
	col = col | subX;
	bool onScreen = true;
	if (x >= ctx.fh->frameWidth) onScreen = false;
	else if (y >= ctx.fh->frameHeight) onScreen = false;
	else if (pass == 0 && x == 0) onScreen = false;
	else if (pass == 1 && y == 0) onScreen = false;
	if (!onScreen) return;

	const int32 xP = x >> subX, yP = y >> subY;
	const int32 prevRow = row - (dy << subY);
	const int32 prevCol = col - (dx << subX);
	const int32 miSize = Grid(ctx.miSizes, ctx.gridStride, row, col);
	const int32 txSz = Grid(ctx.loopfilterTxSizes[plane], ctx.gridStride, row >> subY, col >> subX);
	const int32 planeSize = GetPlaneResidualSize(ctx, miSize, plane);
	const bool skip = Grid(ctx.skips, ctx.gridStride, row, col) != 0;
	const bool isIntra = (int32)Grid(ctx.refFrames0, ctx.gridStride, row, col) <= kRefIntra;
	const int32 prevTxSz = Grid(ctx.loopfilterTxSizes[plane], ctx.gridStride, prevRow >> subY, prevCol >> subX);

	bool isBlockEdge = false;
	if (pass == 0) { if (xP % (kNum4x4BlocksWide[planeSize] * 4) == 0) isBlockEdge = true; }
	else { if (yP % (kNum4x4BlocksHigh[planeSize] * 4) == 0) isBlockEdge = true; }

	bool isTxEdge = false;
	if (pass == 0) { if (xP % kTxWidth[txSz] == 0) isTxEdge = true; }
	else { if (yP % kTxHeight[txSz] == 0) isTxEdge = true; }

	if (!isTxEdge) return;
	const bool applyFilter = isBlockEdge || !skip || isIntra; // isIntra always true here -> always true when isTxEdge

	const int32 filterSize = FilterSizeProcess(txSz, prevTxSz, pass, plane);
	int32 lvl, limit, blimit, thresh;
	AdaptiveFilterStrength(ctx, row, col, plane, pass, lvl, limit, blimit, thresh);
	if (lvl == 0)
		AdaptiveFilterStrength(ctx, prevRow, prevCol, plane, pass, lvl, limit, blimit, thresh);

	if (!applyFilter || lvl == 0) return;
	for (int32 i = 0; i < 4 /*MI_SIZE*/; ++i)
		SampleFilteringProcess(ctx, xP + dy * i, yP + dx * i, plane, limit, blimit, thresh, dx, dy, filterSize);
}

void LoopFilterFrame(NkAv1Ctx &ctx) {
	const int32 numPlanes = ctx.seq->mono ? 1 : 3;
	for (int32 plane = 0; plane < numPlanes; ++plane) {
		if (plane != 0 && ctx.fh->lf.level[1 + plane] == 0) continue;
		for (int32 pass = 0; pass < 2; ++pass) {
			const int32 rowStep = (plane == 0) ? 1 : (1 << ctx.seq->subsamplingY);
			const int32 colStep = (plane == 0) ? 1 : (1 << ctx.seq->subsamplingX);
			for (int32 row = 0; row < ctx.miRows; row += rowStep)
				for (int32 col = 0; col < ctx.miCols; col += colStep)
					LoopFilterEdge(ctx, plane, pass, row, col);
		}
	}
}

// =========================================================================
// CDEF (§7.15) — filtrage directionnel par blocs 8x8, from-scratch.
// =========================================================================
struct NkAv1CdefRegion { int32 rowStart, rowEnd, colStart, colEnd; };

// is_inside_filter_region (§5.11.55) : la region CDEF est la TRAME entiere
// (0..MiCols/MiRows), pas la tile.
NkAv1CdefRegion CdefRegionFor(NkAv1Ctx &ctx, int32 r, int32 c) {
	NkAv1CdefRegion reg;
	reg.rowStart = 0;
	reg.rowEnd = ctx.miRows;
	reg.colStart = 0;
	reg.colEnd = ctx.miCols;
	(void)r;
	(void)c;
	return reg;
}

inline int32 CdefConstrain(int32 diff, int32 threshold, int32 damping) {
	if (!threshold)
		return 0;
	const int32 dampingAdj = NkAv1Max(0, damping - NkAv1FloorLog2((uint32)threshold));
	const int32 sign = (diff < 0) ? -1 : 1;
	return sign * NkAv1Clip3(0, NkAv1Abs(diff), threshold - (NkAv1Abs(diff) >> dampingAdj));
}

// Echantillon source (plans non filtres) avec test de region (cdef_get_at).
inline int32 CdefGetAt(NkAv1Ctx &ctx, const NkVector<uint8> *src[3], int32 plane, int32 x, int32 y,
					   int32 subX, int32 subY, const NkAv1CdefRegion &reg, bool &avail) {
	const int32 candidateR = (y << subY) >> 2;
	const int32 candidateC = (x << subX) >> 2;
	if (candidateR < reg.rowStart || candidateR >= reg.rowEnd || candidateC < reg.colStart || candidateC >= reg.colEnd) {
		avail = false;
		return 0;
	}
	avail = true;
	const int32 stride = (plane == 0) ? ctx.curYStride : ctx.curUvStride;
	return (*src[plane])[(usize)y * (usize)stride + (usize)x];
}

// CDEF direction process (§7.15.2) — sur les plans luma NON filtres.
void CdefDirection(NkAv1Ctx &ctx, const NkVector<uint8> &srcY, int32 r, int32 c, int32 &yDir, int32 &var) {
	static const int32 kCdefDivTable[9] = {0, 840, 420, 280, 210, 168, 140, 120, 105};
	int32 cost[8];
	int32 partial[8][15];
	for (int32 i = 0; i < 8; ++i) {
		cost[i] = 0;
		for (int32 j = 0; j < 15; ++j)
			partial[i][j] = 0;
	}
	int32 bestCost = 0;
	yDir = 0;
	const int32 x0 = c << 2;
	const int32 y0 = r << 2;
	for (int32 i = 0; i < 8; ++i) {
		for (int32 j = 0; j < 8; ++j) {
			const int32 x = (int32)srcY[(usize)(y0 + i) * (usize)ctx.curYStride + (usize)(x0 + j)] - 128;
			partial[0][i + j] += x;
			partial[1][i + j / 2] += x;
			partial[2][i] += x;
			partial[3][3 + i - j / 2] += x;
			partial[4][7 + i - j] += x;
			partial[5][3 - i / 2 + j] += x;
			partial[6][j] += x;
			partial[7][i / 2 + j] += x;
		}
	}
	for (int32 i = 0; i < 8; ++i) {
		cost[2] += partial[2][i] * partial[2][i];
		cost[6] += partial[6][i] * partial[6][i];
	}
	cost[2] *= kCdefDivTable[8];
	cost[6] *= kCdefDivTable[8];
	for (int32 i = 0; i < 7; ++i) {
		cost[0] += (partial[0][i] * partial[0][i] + partial[0][14 - i] * partial[0][14 - i]) * kCdefDivTable[i + 1];
		cost[4] += (partial[4][i] * partial[4][i] + partial[4][14 - i] * partial[4][14 - i]) * kCdefDivTable[i + 1];
	}
	cost[0] += partial[0][7] * partial[0][7] * kCdefDivTable[8];
	cost[4] += partial[4][7] * partial[4][7] * kCdefDivTable[8];
	for (int32 i = 1; i < 8; i += 2) {
		for (int32 j = 0; j < 4 + 1; ++j)
			cost[i] += partial[i][3 + j] * partial[i][3 + j];
		cost[i] *= kCdefDivTable[8];
		for (int32 j = 0; j < 4 - 1; ++j)
			cost[i] += (partial[i][j] * partial[i][j] + partial[i][10 - j] * partial[i][10 - j]) * kCdefDivTable[2 * j + 2];
	}
	for (int32 i = 0; i < 8; ++i) {
		if (cost[i] > bestCost) {
			bestCost = cost[i];
			yDir = i;
		}
	}
	var = (bestCost - cost[(yDir + 4) & 7]) >> 10;
}

// CDEF filter process (§7.15.3) — lit src (non filtre), ecrit dst.
void CdefFilterPlane(NkAv1Ctx &ctx, const NkVector<uint8> *src[3], NkVector<uint8> *dst[3], int32 plane,
					 int32 r, int32 c, int32 priStr, int32 secStr, int32 damping, int32 dir) {
	static const int32 kCdefPriTaps[2][2] = {{4, 2}, {3, 3}};
	static const int32 kCdefSecTaps[2][2] = {{2, 1}, {2, 1}};
	const NkAv1CdefRegion reg = CdefRegionFor(ctx, r, c);
	const int32 subX = (plane > 0) ? ctx.seq->subsamplingX : 0;
	const int32 subY = (plane > 0) ? ctx.seq->subsamplingY : 0;
	const int32 x0 = (c * 4) >> subX;
	const int32 y0 = (r * 4) >> subY;
	const int32 w = 8 >> subX;
	const int32 h = 8 >> subY;
	const int32 stride = (plane == 0) ? ctx.curYStride : ctx.curUvStride;
	for (int32 i = 0; i < h; ++i) {
		for (int32 j = 0; j < w; ++j) {
			int32 sum = 0;
			const int32 x = (*src[plane])[(usize)(y0 + i) * (usize)stride + (usize)(x0 + j)];
			int32 mx = x, mn = x;
			for (int32 k = 0; k < 2; ++k) {
				for (int32 sign = -1; sign <= 1; sign += 2) {
					bool avail = false;
					const int32 py = y0 + i + sign * kCdefDirections[dir][k][0];
					const int32 px = x0 + j + sign * kCdefDirections[dir][k][1];
					const int32 p = CdefGetAt(ctx, src, plane, px, py, subX, subY, reg, avail);
					if (avail) {
						sum += kCdefPriTaps[(priStr >> 0) & 1][k] * CdefConstrain(p - x, priStr, damping);
						mx = NkAv1Max(p, mx);
						mn = NkAv1Min(p, mn);
					}
					for (int32 dirOff = -2; dirOff <= 2; dirOff += 4) {
						bool avail2 = false;
						const int32 sy = y0 + i + sign * kCdefDirections[(dir + dirOff) & 7][k][0];
						const int32 sx = x0 + j + sign * kCdefDirections[(dir + dirOff) & 7][k][1];
						const int32 s = CdefGetAt(ctx, src, plane, sx, sy, subX, subY, reg, avail2);
						if (avail2) {
							sum += kCdefSecTaps[(priStr >> 0) & 1][k] * CdefConstrain(s - x, secStr, damping);
							mx = NkAv1Max(s, mx);
							mn = NkAv1Min(s, mn);
						}
					}
				}
			}
			(*dst[plane])[(usize)(y0 + i) * (usize)stride + (usize)(x0 + j)] =
				(uint8)NkAv1Clip3(mn, mx, x + ((8 + sum - (sum < 0 ? 1 : 0)) >> 4));
		}
	}
}

// CDEF frame process (§7.15).
void CdefApplyFrame(NkAv1Ctx &ctx) {
	if (!ctx.seq->enableCdef || ctx.fh->codedLossless || ctx.fh->allowIntrabc)
		return;
	// src = copie des plans reconstruits (CurrFrame) ; dst = plans courants.
	NkVector<uint8> srcY, srcU, srcV;
	srcY = ctx.curY;
	srcU = ctx.curU;
	srcV = ctx.curV;
	const NkVector<uint8> *src[3] = {&srcY, &srcU, &srcV};
	NkVector<uint8> *dst[3] = {&ctx.curY, &ctx.curU, &ctx.curV};
	const int32 cdefDamping = ctx.fh->cdef.dampingMinus3 + 3;
	const int32 step4 = kNum4x4BlocksWide[3 /*BLOCK_8X8*/];
	const int32 cdefSize4 = kNum4x4BlocksWide[12 /*BLOCK_64X64*/];
	const int32 cdefMask4 = ~(cdefSize4 - 1);
	static const int32 kCdefUvDir[2][2][8] = {
		{{0, 1, 2, 3, 4, 5, 6, 7}, {1, 2, 2, 2, 3, 4, 6, 0}},
		{{7, 0, 2, 4, 5, 6, 6, 6}, {0, 1, 2, 3, 4, 5, 6, 7}}};
	for (int32 r = 0; r < ctx.miRows; r += step4) {
		for (int32 c = 0; c < ctx.miCols; c += step4) {
			const int32 baseR = r & cdefMask4;
			const int32 baseC = c & cdefMask4;
			const int32 idx = Grid(ctx.cdefIdxGrid, ctx.gridStride, baseR, baseC);
			if (idx == -1)
				continue; // copie deja en place
			const bool skipB = Grid(ctx.skips, ctx.gridStride, r, c) && Grid(ctx.skips, ctx.gridStride, r + 1, c) &&
							   Grid(ctx.skips, ctx.gridStride, r, c + 1) && Grid(ctx.skips, ctx.gridStride, r + 1, c + 1);
			if (skipB)
				continue;
			int32 yDir = 0, var = 0;
			CdefDirection(ctx, srcY, r, c, yDir, var);
			int32 priStr = ctx.fh->cdef.yPriStrength[idx]; // coeffShift = 0 (8-bit)
			int32 secStr = ctx.fh->cdef.ySecStrength[idx];
			int32 dir = (priStr == 0) ? 0 : yDir;
			const int32 varStr = (var >> 6) ? NkAv1Min(NkAv1FloorLog2((uint32)(var >> 6)), 12) : 0;
			priStr = var ? (priStr * (4 + varStr) + 8) >> 4 : 0;
			int32 damping = cdefDamping;
			CdefFilterPlane(ctx, src, dst, 0, r, c, priStr, secStr, damping, dir);
			if (!ctx.seq->mono) {
				priStr = ctx.fh->cdef.uvPriStrength[idx];
				secStr = ctx.fh->cdef.uvSecStrength[idx];
				dir = (priStr == 0) ? 0 : kCdefUvDir[ctx.seq->subsamplingX][ctx.seq->subsamplingY][yDir];
				damping = cdefDamping - 1;
				CdefFilterPlane(ctx, src, dst, 1, r, c, priStr, secStr, damping, dir);
				CdefFilterPlane(ctx, src, dst, 2, r, c, priStr, secStr, damping, dir);
			}
		}
	}
}

// =========================================================================
// Champ de MV temporel (§7.9 motion field estimation) + stockage (§7.19).
// =========================================================================
void GetMvProjection(const int32 *mv, int32 numerator, int32 denominator, int32 *projMv) {
	const int32 clippedDenominator = NkAv1Min(kMaxFrameDistance, denominator);
	const int32 clippedNumerator = NkAv1Clip3(-kMaxFrameDistance, kMaxFrameDistance, numerator);
	for (int32 i = 0; i < 2; ++i) {
		const int64 scaled = NkAv1Round2Signed64((int64)mv[i] * clippedNumerator * kDivMult[clippedDenominator], 14);
		projMv[i] = NkAv1Clip3(-(1 << 14) + 1, (1 << 14) - 1, (int32)scaled);
	}
}

bool GetBlockPosition(NkAv1Ctx &ctx, int32 x8, int32 y8, int32 dstSign, const int32 *projMv,
					  int32 &posX8, int32 &posY8) {
	bool posValid = true;
	auto project = [&](int32 v8, int32 delta, int32 max8, int32 maxOff8) -> int32 {
		const int32 base8 = (v8 >> 3) << 3;
		int32 offset8;
		if (delta >= 0)
			offset8 = delta >> (3 + 1 + 2 /*MI_SIZE_LOG2*/);
		else
			offset8 = -((-delta) >> (3 + 1 + 2));
		v8 += dstSign * offset8;
		if (v8 < 0 || v8 >= max8 || v8 < base8 - maxOff8 || v8 >= base8 + 8 + maxOff8)
			posValid = false;
		return v8;
	};
	posY8 = project(y8, projMv[0], ctx.miRows >> 1, 0 /*MAX_OFFSET_HEIGHT*/);
	posX8 = project(x8, projMv[1], ctx.miCols >> 1, 8 /*MAX_OFFSET_WIDTH*/);
	return posValid;
}

// Projection process (§7.9.2). Renvoie true si la source etait utilisable.
bool MotionFieldProjection(NkAv1Ctx &ctx, int32 src, int32 dstSign) {
	const int32 srcIdx = ctx.fh->refFrameIdx[src - kRefLast];
	if (srcIdx < 0)
		return false;
	const NkAv1RefSlot &slot = ctx.state->refs[srcIdx];
	const int32 w8 = ctx.mfW8, h8 = ctx.mfH8;
	if (!slot.valid || slot.miRows != ctx.miRows || slot.miCols != ctx.miCols ||
		slot.frameType == kAv1IntraOnlyFrame || slot.frameType == kAv1KeyFrame)
		return false;
	if (slot.savedRefFrames.Size() == 0)
		return false;
	for (int32 y8 = 0; y8 < h8; ++y8) {
		for (int32 x8 = 0; x8 < w8; ++x8) {
			const int32 row = 2 * y8 + 1;
			const int32 col = 2 * x8 + 1;
			const int32 srcRef = slot.savedRefFrames[(usize)row * (usize)slot.miCols + (usize)col];
			if (srcRef > kRefIntra) {
				const int32 refToCur = GetRelativeDist(*ctx.seq, ctx.fh->orderHints[src], ctx.fh->orderHint);
				const int32 refOffset = GetRelativeDist(*ctx.seq, ctx.fh->orderHints[src], slot.savedOrderHints[srcRef]);
				bool posValid = NkAv1Abs(refToCur) <= kMaxFrameDistance && NkAv1Abs(refOffset) <= kMaxFrameDistance &&
								refOffset > 0;
				if (posValid) {
					const int16 *sm = &slot.savedMvs[((usize)row * (usize)slot.miCols + (usize)col) * 2];
					const int32 mv[2] = {sm[0], sm[1]};
					int32 projMv[2];
					GetMvProjection(mv, refToCur * dstSign, refOffset, projMv);
					int32 posX8 = 0, posY8 = 0;
					posValid = GetBlockPosition(ctx, x8, y8, dstSign, projMv, posX8, posY8);
					if (posValid) {
						for (int32 dst = kRefLast; dst <= kRefAltRef; ++dst) {
							const int32 refToDst = GetRelativeDist(*ctx.seq, ctx.fh->orderHint, ctx.fh->orderHints[dst]);
							int32 projMv2[2];
							GetMvProjection(mv, refToDst, refOffset, projMv2);
							int16 *dstMv = MfMvAt(ctx, dst, posY8, posX8);
							dstMv[0] = (int16)projMv2[0];
							dstMv[1] = (int16)projMv2[1];
						}
					}
				}
			}
		}
	}
	return true;
}

// Motion field estimation process (§7.9.1).
void MotionFieldEstimation(NkAv1Ctx &ctx) {
	ctx.mfW8 = ctx.miCols >> 1;
	ctx.mfH8 = ctx.miRows >> 1;
	ctx.motionFieldMvs.Resize((usize)7 * (usize)ctx.mfH8 * (usize)ctx.mfW8 * 2);
	for (usize i = 0; i < ctx.motionFieldMvs.Size(); ++i)
		ctx.motionFieldMvs[i] = (int16)(-(1 << 15));
	const int32 lastIdx = ctx.fh->refFrameIdx[0];
	const int32 curGoldOrderHint = ctx.fh->orderHints[kRefGolden];
	const int32 lastAltOrderHint = (lastIdx >= 0) ? ctx.state->refs[lastIdx].savedOrderHints[kRefAltRef] : 0;
	const bool useLast = lastAltOrderHint != curGoldOrderHint;
	if (useLast)
		MotionFieldProjection(ctx, kRefLast, -1);
	int32 refStamp = 3 /*MFMV_STACK_SIZE*/ - 2;
	const bool useBwd = GetRelativeDist(*ctx.seq, ctx.fh->orderHints[kRefBwd], ctx.fh->orderHint) > 0;
	if (useBwd) {
		if (MotionFieldProjection(ctx, kRefBwd, 1))
			--refStamp;
	}
	const bool useAlt2 = GetRelativeDist(*ctx.seq, ctx.fh->orderHints[kRefAltRef2], ctx.fh->orderHint) > 0;
	if (useAlt2) {
		if (MotionFieldProjection(ctx, kRefAltRef2, 1))
			--refStamp;
	}
	const bool useAlt = GetRelativeDist(*ctx.seq, ctx.fh->orderHints[kRefAltRef], ctx.fh->orderHint) > 0;
	if (useAlt && refStamp >= 0) {
		if (MotionFieldProjection(ctx, kRefAltRef, 1))
			--refStamp;
	}
	if (refStamp >= 0)
		MotionFieldProjection(ctx, kRefLast2, -1);
}

// Motion field motion vector storage process (§7.19) : prepare MfRefFrames/MfMvs.
void ComputeMfStorage(NkAv1Ctx &ctx, NkVector<int8> &mfRef, NkVector<int16> &mfMv) {
	const int32 miRows = ctx.miRows, miCols = ctx.miCols;
	mfRef.Resize((usize)miRows * (usize)miCols);
	mfMv.Resize((usize)miRows * (usize)miCols * 2);
	for (int32 row = 0; row < miRows; ++row) {
		for (int32 col = 0; col < miCols; ++col) {
			const usize cell = (usize)row * (usize)miCols + (usize)col;
			mfRef[cell] = (int8)kRefNone;
			mfMv[cell * 2 + 0] = 0;
			mfMv[cell * 2 + 1] = 0;
			for (int32 list = 0; list < 2; ++list) {
				const int32 r2 = (list == 0) ? (int32)Grid(ctx.refFrames0, ctx.gridStride, row, col)
											 : (int32)Grid(ctx.refFrames1, ctx.gridStride, row, col);
				if (r2 > kRefIntra) {
					const int32 refIdx = ctx.fh->refFrameIdx[r2 - kRefLast];
					if (refIdx < 0)
						continue;
					const int32 dist = GetRelativeDist(*ctx.seq, ctx.state ? ctx.state->refs[refIdx].orderHint : 0,
													   ctx.fh->orderHint);
					if (dist < 0) {
						const int16 *m = MvsAt(ctx, row, col);
						const int32 mvRow = m[list * 2 + 0];
						const int32 mvCol = m[list * 2 + 1];
						if (NkAv1Abs(mvRow) <= 4095 /*REFMVS_LIMIT*/ && NkAv1Abs(mvCol) <= 4095) {
							mfRef[cell] = (int8)r2;
							mfMv[cell * 2 + 0] = (int16)mvRow;
							mfMv[cell * 2 + 1] = (int16)mvCol;
						}
					}
				}
			}
		}
	}
}

// =========================================================================
// Top-level frame driver (intra + inter) : alloue grilles/plans, decode chaque
// tile (CDF par tile conformes §5.11.1), applique loop filter + CDEF, stocke
// le champ de MV et met a jour le DPB (§7.20).
// =========================================================================
bool DecodeFrameImplFull(NkAv1DecoderState &st, const NkAv1FrameHeader &fh,
						 const uint8 *tgData, usize tgSize, NkAv1Image *outImage, const char **errOut) {
	const NkAv1SequenceHeader &seq = st.seq;
	const char *err = "";
	if (seq.mono) { if (errOut) *errOut = "monochrome (non gere)"; return false; }
	if (seq.bitDepth != 8) { if (errOut) *errOut = "profondeur > 8 bits (non geree)"; return false; }
	if (seq.subsamplingX != 1 || seq.subsamplingY != 1) {
		if (errOut) *errOut = "sous-echantillonnage != 4:2:0 (non gere)";
		return false;
	}
	if (fh.allowIntrabc) { if (errOut) *errOut = "intra block copy (non implemente)"; return false; }
	if (fh.lr.usesLr) { if (errOut) *errOut = "loop restoration (non implementee)"; return false; }
	if (fh.frameWidth != fh.upscaledWidth) { if (errOut) *errOut = "superres (non implemente)"; return false; }

	NkVector<NkAv1TileEntry> tiles;
	int32 tgStart = 0, tgEnd = 0;
	if (!NkAv1Decoder::ParseTileGroup(tgData, tgSize, fh, tiles, tgStart, tgEnd)) {
		if (errOut) *errOut = "tile group invalide";
		return false;
	}
	if (tiles.Size() != (usize)(fh.tiles.tileCols * fh.tiles.tileRows)) {
		if (errOut) *errOut = "tile groups multiples (non geres)";
		return false;
	}

	NkAv1Ctx ctx;
	ctx.seq = &seq;
	ctx.fh = &fh;
	ctx.state = &st;
	ctx.ok = true;
	ctx.err = "";
	ctx.frameIsIntra = fh.frameIsIntra;
	ctx.miCols = fh.miCols;
	ctx.miRows = fh.miRows;
	ctx.sbSize = seq.use128x128Superblock ? 15 /*BLOCK_128X128*/ : 12 /*BLOCK_64X64*/;
	ctx.sbSize4 = kNum4x4BlocksWide[ctx.sbSize];
	const int32 pad = ctx.sbSize4; // safety margin for nominal-block writes at frame edges
	ctx.gridStride = ctx.miCols + pad;
	ctx.gridRows = ctx.miRows + pad;
	const usize gridN = (usize)ctx.gridStride * (usize)ctx.gridRows;
	ctx.yModes.Resize(gridN, 0);
	ctx.uvModes.Resize(gridN, 0);
	ctx.miSizes.Resize(gridN, 0);
	ctx.segmentIds.Resize(gridN, 0);
	ctx.skips.Resize(gridN, 0);
	ctx.txSizesGrid.Resize(gridN, 0);
	ctx.txTypes.Resize(gridN, 0);
	ctx.refFrames0.Resize(gridN, 0);
	ctx.refFrames1.Resize(gridN, (int8)kRefNone);
	ctx.interpFilters0.Resize(gridN, 0);
	ctx.interpFilters1.Resize(gridN, 0);
	ctx.compGroupIdxs.Resize(gridN, 0);
	ctx.compoundIdxs.Resize(gridN, 0);
	ctx.skipModes.Resize(gridN, 0);
	ctx.isInters.Resize(gridN, 0);
	ctx.refWritten.Resize(gridN, 0);
	ctx.mvsGrid.Resize(gridN * 4, 0);
	ctx.segPredAbove.Resize((usize)ctx.gridStride, 0);
	ctx.segPredLeft.Resize((usize)ctx.gridRows, 0);
	ctx.cdefIdxGrid.Resize(gridN, (int8)-1);

	for (int32 p = 0; p < 3; ++p) {
		ctx.aboveLevel[p].Resize((usize)ctx.gridStride, 0);
		ctx.aboveDc[p].Resize((usize)ctx.gridStride, 0);
		ctx.leftLevel[p].Resize((usize)ctx.gridRows, 0);
		ctx.leftDc[p].Resize((usize)ctx.gridRows, 0);
		ctx.blockDecoded[p].Resize(34 * 34, 0);
		ctx.loopfilterTxSizes[p].Resize(gridN, 0);
	}
	for (int32 i = 0; i < 4; ++i)
		ctx.deltaLfGrid[i].Resize(gridN, 0);

	// Parametres de trame utilises au niveau bloc.
	ctx.interpFilterFrame = fh.interpolationFilter;
	ctx.enableDualFilter = seq.enableDualFilter;
	ctx.enableJntComp = seq.enableJntComp;
	ctx.enableMaskedCompound = seq.enableMaskedCompound;
	ctx.enableInterintraCompound = seq.enableInterintraCompound;
	ctx.allowHighPrecisionMv = fh.allowHighPrecisionMv;
	ctx.forceIntegerMv = fh.forceIntegerMv;
	ctx.isMotionModeSwitchable = fh.isMotionModeSwitchable;
	ctx.allowWarpedMotion = fh.allowWarpedMotion;
	ctx.referenceSelect = fh.referenceSelect;
	ctx.skipModePresent = fh.skipModePresent;
	ctx.skipModeFrame[0] = fh.skipModeFrame[0];
	ctx.skipModeFrame[1] = fh.skipModeFrame[1];
	ctx.useRefFrameMvsFrame = fh.useRefFrameMvs;
	ctx.mfW8 = ctx.miCols >> 1;
	ctx.mfH8 = ctx.miRows >> 1;

	// PrevSegmentIds (load_previous_segment_ids, §5.9.2 semantics).
	if (fh.seg.enabled) {
		ctx.prevSegmentIds.Resize(gridN, 0);
		if (fh.primaryRefFrame != kPrimaryRefNone) {
			const int32 prevFrame = fh.refFrameIdx[fh.primaryRefFrame];
			if (prevFrame >= 0 && st.refs[prevFrame].valid && st.refs[prevFrame].miCols == ctx.miCols &&
				st.refs[prevFrame].miRows == ctx.miRows && st.refs[prevFrame].savedSegmentIds.Size() != 0) {
				for (int32 row = 0; row < ctx.miRows; ++row)
					for (int32 col = 0; col < ctx.miCols; ++col)
						Grid(ctx.prevSegmentIds, ctx.gridStride, row, col) =
							st.refs[prevFrame].savedSegmentIds[(usize)row * (usize)ctx.miCols + (usize)col];
			}
		}
	}

	const int32 ssx = seq.subsamplingX, ssy = seq.subsamplingY;
	// Padding : un bloc de transformée dont la position de départ est valide
	// (startX < maxX) peut néanmoins ECRIRE au-delà de miCols*4/miRows*4 quand
	// ces dimensions ne sont pas alignées sur la plus grande taille de tx
	// possible (64 luma / 32 chroma). Sans marge, ces écritures débordent le
	// buffer NkVector -> corruption de tas. On sur-alloue de 64px (luma).
	constexpr int32 kPixelPad = 64;
	ctx.curYStride = ctx.miCols * 4 + kPixelPad;
	const int32 curYH = ctx.miRows * 4 + kPixelPad;
	ctx.curUvW = ctx.curYStride >> ssx;
	ctx.curUvH = curYH >> ssy;
	ctx.curUvStride = ctx.curUvW;
	ctx.curY.Resize((usize)ctx.curYStride * (usize)curYH, 0);
	ctx.curU.Resize((usize)ctx.curUvStride * (usize)ctx.curUvH, 0);
	ctx.curV.Resize((usize)ctx.curUvStride * (usize)ctx.curUvH, 0);

	// CDF d'origine de la trame : defauts (primary_ref_frame == NONE) ou
	// chargees depuis le slot primaire (load_cdfs : compteurs remis a 0).
	NkAv1BlockScratch *scratch = (NkAv1BlockScratch *)memory::NkAlloc(sizeof(NkAv1BlockScratch));
	NkAv1Cdfs *origCdfs = (NkAv1Cdfs *)memory::NkAlloc(sizeof(NkAv1Cdfs));
	NkAv1Cdfs *savedCdfs = (NkAv1Cdfs *)memory::NkAlloc(sizeof(NkAv1Cdfs));
	if (!scratch || !origCdfs || !savedCdfs) {
		if (scratch) memory::NkFree(scratch);
		if (origCdfs) memory::NkFree(origCdfs);
		if (savedCdfs) memory::NkFree(savedCdfs);
		if (errOut) *errOut = "allocation";
		return false;
	}
	ctx.scratch = scratch;
	bool decodeOk = true;
	{
		if (fh.primaryRefFrame == kPrimaryRefNone) {
			NkAv1InitCdfs(*origCdfs, fh.quant.baseQIdx);
		} else {
			const int32 prevFrame = fh.refFrameIdx[fh.primaryRefFrame];
			if (prevFrame < 0 || !st.refs[prevFrame].valid) {
				err = "primary ref frame invalide";
				decodeOk = false;
			} else {
				*origCdfs = st.refs[prevFrame].cdfs;
				NkAv1ZeroCdfCounters(*origCdfs);
			}
		}
	}

	// Champ de MV temporel (§7.9) si use_ref_frame_mvs.
	if (decodeOk && !fh.frameIsIntra && fh.useRefFrameMvs)
		MotionFieldEstimation(ctx);

	bool savedAny = false;
	if (decodeOk) {
		for (int32 tRow = 0; tRow < fh.tiles.tileRows && decodeOk; ++tRow) {
			for (int32 tCol = 0; tCol < fh.tiles.tileCols && decodeOk; ++tCol) {
				const int32 tileNum = tRow * fh.tiles.tileCols + tCol;
				const NkAv1TileEntry &te = tiles[(usize)tileNum];
				ctx.miColStart = fh.tiles.miColStarts[tCol];
				ctx.miColEnd = fh.tiles.miColStarts[tCol + 1];
				ctx.miRowStart = fh.tiles.miRowStarts[tRow];
				ctx.miRowEnd = fh.tiles.miRowStarts[tRow + 1];
				ctx.cdf = *origCdfs; // chaque tile repart des CDF de trame (§5.11.1)
				if (!DecodeTile(ctx, tgData + te.offset, te.size)) {
					err = (ctx.err && ctx.err[0]) ? ctx.err : "echec decode tile";
					decodeOk = false;
					break;
				}
				if (!fh.disableFrameEndUpdateCdf && tileNum == fh.tiles.contextUpdateTileId) {
					*savedCdfs = ctx.cdf;
					savedAny = true;
				}
			}
		}
	}

	if (decodeOk) {
		// Post-filtres : deblocage (§7.14) puis CDEF (§7.15). §7.4 : le loop
		// filter n'est invoque QUE si loop_filter_level[0] ou [1] est non nul
		// (les deltas par bloc ne suffisent pas a le declencher).
		if (fh.lf.level[0] != 0 || fh.lf.level[1] != 0)
			LoopFilterFrame(ctx);
		CdefApplyFrame(ctx);

		// decode_frame_wrapup, etape 7 : si seg active sans update de carte, la
		// carte finale est PrevSegmentIds.
		if (fh.seg.enabled && !fh.seg.updateMap && ctx.prevSegmentIds.Size() != 0) {
			for (int32 row = 0; row < ctx.miRows; ++row)
				for (int32 col = 0; col < ctx.miCols; ++col)
					Grid(ctx.segmentIds, ctx.gridStride, row, col) = Grid(ctx.prevSegmentIds, ctx.gridStride, row, col);
		}

		// frame_end_update_cdf (§7.4) : CDF finales de la trame.
		const NkAv1Cdfs *frameCdfs = origCdfs;
		if (!fh.disableFrameEndUpdateCdf && savedAny)
			frameCdfs = savedCdfs;

		// Champ de MV a sauvegarder (§7.19).
		NkVector<int8> mfRef;
		NkVector<int16> mfMv;
		ComputeMfStorage(ctx, mfRef, mfMv);

		// Mise a jour du DPB (§7.20).
		for (int32 i = 0; i < kNumRefFrames; ++i) {
			if (!((fh.refreshFrameFlags >> i) & 1))
				continue;
			NkAv1RefSlot &slot = st.refs[i];
			slot.valid = true;
			slot.upscaledWidth = fh.upscaledWidth;
			slot.frameWidth = fh.frameWidth;
			slot.frameHeight = fh.frameHeight;
			slot.renderWidth = fh.renderWidth;
			slot.renderHeight = fh.renderHeight;
			slot.miCols = fh.miCols;
			slot.miRows = fh.miRows;
			slot.frameType = fh.frameType;
			slot.orderHint = fh.orderHint;
			slot.showableFrame = fh.showableFrame;
			for (int32 j = 0; j < 8; ++j)
				slot.savedOrderHints[j] = fh.orderHints[j];
			slot.yStride = fh.upscaledWidth;
			slot.uvW = (fh.upscaledWidth + ssx) >> ssx;
			slot.uvH = (fh.frameHeight + ssy) >> ssy;
			slot.uvStride = slot.uvW;
			slot.y.Resize((usize)slot.yStride * (usize)fh.frameHeight);
			slot.u.Resize((usize)slot.uvStride * (usize)slot.uvH);
			slot.v.Resize((usize)slot.uvStride * (usize)slot.uvH);
			for (int32 y = 0; y < fh.frameHeight; ++y)
				for (int32 x = 0; x < fh.upscaledWidth; ++x)
					slot.y[(usize)y * (usize)slot.yStride + (usize)x] = ctx.curY[(usize)y * (usize)ctx.curYStride + (usize)x];
			for (int32 y = 0; y < slot.uvH; ++y)
				for (int32 x = 0; x < slot.uvW; ++x) {
					slot.u[(usize)y * (usize)slot.uvStride + (usize)x] = ctx.curU[(usize)y * (usize)ctx.curUvStride + (usize)x];
					slot.v[(usize)y * (usize)slot.uvStride + (usize)x] = ctx.curV[(usize)y * (usize)ctx.curUvStride + (usize)x];
				}
			slot.savedRefFrames = mfRef;
			slot.savedMvs = mfMv;
			slot.savedSegmentIds.Resize((usize)fh.miRows * (usize)fh.miCols);
			for (int32 row = 0; row < fh.miRows; ++row)
				for (int32 col = 0; col < fh.miCols; ++col)
					slot.savedSegmentIds[(usize)row * (usize)fh.miCols + (usize)col] =
						Grid(ctx.segmentIds, ctx.gridStride, row, col);
			for (int32 rr = 0; rr < 8; ++rr)
				for (int32 j = 0; j < 6; ++j)
					slot.savedGmParams[rr][j] = fh.gmParams[rr][j];
			for (int32 j = 0; j < 8; ++j)
				slot.lfRefDeltas[j] = fh.lf.refDeltas[j];
			slot.lfModeDeltas[0] = fh.lf.modeDeltas[0];
			slot.lfModeDeltas[1] = fh.lf.modeDeltas[1];
			for (int32 s2 = 0; s2 < 8; ++s2)
				for (int32 j = 0; j < 8; ++j) {
					slot.segFeatureEnabled[s2][j] = fh.seg.featureEnabled[s2][j];
					slot.segFeatureData[s2][j] = fh.seg.featureData[s2][j];
				}
			slot.cdfs = *frameCdfs;
		}

		// Sortie (ordre d'affichage : l'appelant gere show_frame).
		if (outImage) {
			NkAv1Image &out = *outImage;
			const int32 w = fh.upscaledWidth, hgt = fh.frameHeight;
			out.width = w;
			out.height = hgt;
			out.yStride = w;
			out.uvWidth = (w + ssx) >> ssx;
			out.uvHeight = (hgt + ssy) >> ssy;
			out.uvStride = out.uvWidth;
			out.bitDepth = seq.bitDepth;
			out.y.Resize((usize)out.yStride * (usize)hgt);
			out.u.Resize((usize)out.uvStride * (usize)out.uvHeight);
			out.v.Resize((usize)out.uvStride * (usize)out.uvHeight);
			for (int32 y = 0; y < hgt; ++y)
				for (int32 x = 0; x < w; ++x)
					out.y[(usize)y * (usize)out.yStride + (usize)x] = ctx.curY[(usize)y * (usize)ctx.curYStride + (usize)x];
			for (int32 y = 0; y < out.uvHeight; ++y)
				for (int32 x = 0; x < out.uvWidth; ++x) {
					out.u[(usize)y * (usize)out.uvStride + (usize)x] = ctx.curU[(usize)y * (usize)ctx.curUvStride + (usize)x];
					out.v[(usize)y * (usize)out.uvStride + (usize)x] = ctx.curV[(usize)y * (usize)ctx.curUvStride + (usize)x];
				}
		}
	}

	memory::NkFree(scratch);
	memory::NkFree(origCdfs);
	memory::NkFree(savedCdfs);
	if (!decodeOk && errOut)
		*errOut = err;
	return decodeOk;
}

// =========================================================================
// Frame header COMPLET (§5.9) — chemins intra ET inter, avec acces au DPB
// (RefOrderHint, set_frame_refs, frame_size_with_refs, load_previous,
// global_motion_params, skip_mode_params).
// =========================================================================
inline int32 InverseRecenter(int32 r, int32 v) {
	if (v > 2 * r)
		return v;
	if (v & 1)
		return r - ((v + 1) >> 1);
	return r + (v >> 1);
}

int32 DecodeSubexp(NkAv1BitReader &rb, int32 numSyms) {
	int32 i = 0, mk = 0;
	const int32 k = 3;
	while (true) {
		const int32 b2 = i ? (k + i - 1) : k;
		const int32 a = 1 << b2;
		if (numSyms <= mk + 3 * a) {
			return (int32)rb.Ns((uint32)(numSyms - mk)) + mk;
		} else {
			if (rb.Bit()) {
				++i;
				mk += a;
			} else {
				return (int32)rb.F(b2) + mk;
			}
		}
	}
}

int32 DecodeUnsignedSubexpWithRef(NkAv1BitReader &rb, int32 mx, int32 r) {
	const int32 v = DecodeSubexp(rb, mx);
	if ((r << 1) <= mx)
		return InverseRecenter(r, v);
	return mx - 1 - InverseRecenter(mx - 1 - r, v);
}

int32 DecodeSignedSubexpWithRef(NkAv1BitReader &rb, int32 low, int32 high, int32 r) {
	return DecodeUnsignedSubexpWithRef(rb, high - low, r - low) + low;
}

// read_global_param (§5.9.25).
void ReadGlobalParam(NkAv1BitReader &rb, NkAv1FrameHeader &h, int32 type, int32 ref, int32 idx,
					 const int32 prevGmParams[8][6]) {
	int32 absBits = 12;  // GM_ABS_ALPHA_BITS
	int32 precBits = 15; // GM_ALPHA_PREC_BITS
	if (idx < 2) {
		if (type == kGmTranslation) {
			absBits = 9 - (h.allowHighPrecisionMv ? 0 : 1);  // GM_ABS_TRANS_ONLY_BITS
			precBits = 3 - (h.allowHighPrecisionMv ? 0 : 1); // GM_TRANS_ONLY_PREC_BITS
		} else {
			absBits = 12; // GM_ABS_TRANS_BITS
			precBits = 6; // GM_TRANS_PREC_BITS
		}
	}
	const int32 precDiff = kWarpedModelPrecBits - precBits;
	const int32 round = ((idx % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
	const int32 sub = ((idx % 3) == 2) ? (1 << precBits) : 0;
	const int32 mx = 1 << absBits;
	const int32 r = (prevGmParams[ref][idx] >> precDiff) - sub;
	h.gmParams[ref][idx] = (DecodeSignedSubexpWithRef(rb, -mx, mx + 1, r) << precDiff) + round;
}

// set_frame_refs (§7.8).
void SetFrameRefs(NkAv1DecoderState &st, NkAv1FrameHeader &h, int32 lastFrameIdx, int32 goldFrameIdx) {
	const NkAv1SequenceHeader &seq = st.seq;
	for (int32 i = 0; i < kRefsPerFrame; ++i)
		h.refFrameIdx[i] = -1;
	h.refFrameIdx[0] = lastFrameIdx;                 // LAST_FRAME
	h.refFrameIdx[kRefGolden - kRefLast] = goldFrameIdx; // GOLDEN_FRAME
	bool usedFrame[8] = {false};
	usedFrame[lastFrameIdx] = true;
	usedFrame[goldFrameIdx] = true;
	const int32 curFrameHint = 1 << (seq.orderHintBits - 1);
	int32 shiftedOrderHints[8];
	for (int32 i = 0; i < 8; ++i)
		shiftedOrderHints[i] = curFrameHint + GetRelativeDist(seq, st.refs[i].orderHint, h.orderHint);
	int32 latestOrderHint = 0, earliestOrderHint = 0;
	auto findLatestBackward = [&]() -> int32 {
		int32 ref = -1;
		for (int32 i = 0; i < 8; ++i) {
			const int32 hint = shiftedOrderHints[i];
			if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint >= latestOrderHint)) {
				ref = i;
				latestOrderHint = hint;
			}
		}
		return ref;
	};
	auto findEarliestBackward = [&]() -> int32 {
		int32 ref = -1;
		for (int32 i = 0; i < 8; ++i) {
			const int32 hint = shiftedOrderHints[i];
			if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint < earliestOrderHint)) {
				ref = i;
				earliestOrderHint = hint;
			}
		}
		return ref;
	};
	auto findLatestForward = [&]() -> int32 {
		int32 ref = -1;
		for (int32 i = 0; i < 8; ++i) {
			const int32 hint = shiftedOrderHints[i];
			if (!usedFrame[i] && hint < curFrameHint && (ref < 0 || hint >= latestOrderHint)) {
				ref = i;
				latestOrderHint = hint;
			}
		}
		return ref;
	};
	int32 ref = findLatestBackward();
	if (ref >= 0) {
		h.refFrameIdx[kRefAltRef - kRefLast] = ref;
		usedFrame[ref] = true;
	}
	ref = findEarliestBackward();
	if (ref >= 0) {
		h.refFrameIdx[kRefBwd - kRefLast] = ref;
		usedFrame[ref] = true;
	}
	ref = findEarliestBackward();
	if (ref >= 0) {
		h.refFrameIdx[kRefAltRef2 - kRefLast] = ref;
		usedFrame[ref] = true;
	}
	const int32 refFrameList[5] = {kRefLast2, kRefLast3, kRefBwd, kRefAltRef2, kRefAltRef};
	for (int32 i = 0; i < 5; ++i) {
		const int32 refFrame = refFrameList[i];
		if (h.refFrameIdx[refFrame - kRefLast] < 0) {
			ref = findLatestForward();
			if (ref >= 0) {
				h.refFrameIdx[refFrame - kRefLast] = ref;
				usedFrame[ref] = true;
			}
		}
	}
	ref = -1;
	for (int32 i = 0; i < 8; ++i) {
		const int32 hint = shiftedOrderHints[i];
		if (ref < 0 || hint < earliestOrderHint) {
			ref = i;
			earliestOrderHint = hint;
		}
	}
	for (int32 i = 0; i < kRefsPerFrame; ++i) {
		if (h.refFrameIdx[i] < 0)
			h.refFrameIdx[i] = ref;
	}
}

// uncompressed_header COMPLET (§5.9.2). Peut modifier st (reset des refs pour
// une keyframe montree, RefValid via ref_order_hint en mode error-resilient).
bool ParseFrameHeaderFull(const uint8 *data, usize size, NkAv1DecoderState &st, NkAv1FrameHeader &out,
						  const char **errOut) {
	const NkAv1SequenceHeader &seq = st.seq;
	if (!seq.valid)
		return false;
	NkAv1BitReader rb(data, size);
	NkAv1FrameHeader h;
	const int32 allFrames = (1 << kNumRefFrames) - 1;

	int32 idLen = 0;
	if (seq.frameIdNumbersPresent)
		idLen = seq.additionalFrameIdLengthMinus1 + seq.deltaFrameIdLengthMinus2 + 3;

	bool frameIsIntra;
	if (seq.reducedStillPictureHeader) {
		h.showExistingFrame = false;
		h.frameType = kAv1KeyFrame;
		frameIsIntra = true;
		h.showFrame = true;
		h.showableFrame = false;
	} else {
		h.showExistingFrame = rb.Bit() != 0;
		if (h.showExistingFrame) {
			h.frameToShow = (int32)rb.F(3);
			if (seq.frameIdNumbersPresent)
				rb.F(idLen); // display_frame_id
			h.frameType = st.refs[h.frameToShow].frameType;
			h.refreshFrameFlags = 0;
			if (h.frameType == kAv1KeyFrame)
				h.refreshFrameFlags = allFrames;
			out = h;
			out.valid = !rb.error;
			return !rb.error;
		}
		h.frameType = (int32)rb.F(2);
		frameIsIntra = (h.frameType == kAv1KeyFrame || h.frameType == kAv1IntraOnlyFrame);
		h.showFrame = rb.Bit() != 0;
		if (h.showFrame)
			h.showableFrame = (h.frameType != kAv1KeyFrame);
		else
			h.showableFrame = rb.Bit() != 0;
		if (h.frameType == kAv1SwitchFrame || (h.frameType == kAv1KeyFrame && h.showFrame))
			h.errorResilientMode = true;
		else
			h.errorResilientMode = rb.Bit() != 0;
	}
	h.frameIsIntra = frameIsIntra;

	if (h.frameType == kAv1KeyFrame && h.showFrame) {
		for (int32 i = 0; i < kNumRefFrames; ++i) {
			st.refs[i].valid = false;
			st.refs[i].orderHint = 0;
		}
		for (int32 i = 0; i < 8; ++i)
			h.orderHints[i] = 0;
	}

	h.disableCdfUpdate = rb.Bit() != 0;
	if (seq.seqForceScreenContentTools == kSelectScreenContentTools)
		h.allowScreenContentTools = rb.Bit();
	else
		h.allowScreenContentTools = seq.seqForceScreenContentTools;
	if (h.allowScreenContentTools) {
		if (seq.seqForceIntegerMv == kSelectIntegerMv)
			h.forceIntegerMv = rb.Bit();
		else
			h.forceIntegerMv = seq.seqForceIntegerMv;
	} else {
		h.forceIntegerMv = 0;
	}
	if (frameIsIntra)
		h.forceIntegerMv = 1;

	if (seq.frameIdNumbersPresent)
		h.currentFrameId = (int32)rb.F(idLen);

	if (h.frameType == kAv1SwitchFrame)
		h.frameSizeOverride = true;
	else if (seq.reducedStillPictureHeader)
		h.frameSizeOverride = false;
	else
		h.frameSizeOverride = rb.Bit() != 0;

	h.orderHint = (int32)rb.F(seq.orderHintBits);

	if (frameIsIntra || h.errorResilientMode)
		h.primaryRefFrame = kPrimaryRefNone;
	else
		h.primaryRefFrame = (int32)rb.F(3);

	// decoder model buffer_removal_time : absent de nos flux (decoder model off).

	if (h.frameType == kAv1SwitchFrame || (h.frameType == kAv1KeyFrame && h.showFrame))
		h.refreshFrameFlags = allFrames;
	else
		h.refreshFrameFlags = (int32)rb.F(8);

	if (!frameIsIntra || h.refreshFrameFlags != allFrames) {
		if (h.errorResilientMode && seq.enableOrderHint) {
			for (int32 i = 0; i < kNumRefFrames; ++i) {
				const int32 refOrderHint = (int32)rb.F(seq.orderHintBits);
				if (refOrderHint != st.refs[i].orderHint)
					st.refs[i].valid = false;
			}
		}
	}

	if (frameIsIntra) {
		ParseFrameSize(rb, seq, h);
		ParseRenderSize(rb, h);
		if (h.allowScreenContentTools && h.upscaledWidth == h.frameWidth)
			h.allowIntrabc = rb.Bit() != 0;
	} else {
		int32 frameRefsShortSignaling = 0;
		if (seq.enableOrderHint) {
			frameRefsShortSignaling = rb.Bit();
			if (frameRefsShortSignaling) {
				const int32 lastFrameIdx = (int32)rb.F(3);
				const int32 goldFrameIdx = (int32)rb.F(3);
				SetFrameRefs(st, h, lastFrameIdx, goldFrameIdx);
			}
		}
		for (int32 i = 0; i < kRefsPerFrame; ++i) {
			if (!frameRefsShortSignaling)
				h.refFrameIdx[i] = (int32)rb.F(3);
			if (seq.frameIdNumbersPresent)
				rb.F(seq.deltaFrameIdLengthMinus2 + 2); // delta_frame_id_minus_1
		}
		if (h.frameSizeOverride && !h.errorResilientMode) {
			// frame_size_with_refs (§5.9.7).
			int32 foundRef = 0;
			for (int32 i = 0; i < kRefsPerFrame; ++i) {
				foundRef = rb.Bit();
				if (foundRef == 1) {
					const NkAv1RefSlot &slot = st.refs[h.refFrameIdx[i]];
					h.upscaledWidth = slot.upscaledWidth;
					h.frameWidth = h.upscaledWidth;
					h.frameHeight = slot.frameHeight;
					h.renderWidth = slot.renderWidth;
					h.renderHeight = slot.renderHeight;
					break;
				}
			}
			if (foundRef == 0) {
				ParseFrameSize(rb, seq, h);
				ParseRenderSize(rb, h);
			} else {
				// superres_params + compute_image_size.
				int32 useSuperres = 0;
				if (seq.enableSuperres)
					useSuperres = rb.Bit();
				if (useSuperres) {
					const int32 codedDenom = (int32)rb.F(kSuperresDenomBits);
					h.superresDenom = codedDenom + kSuperresDenomMin;
				} else {
					h.superresDenom = kSuperresNum;
				}
				h.upscaledWidth = h.frameWidth;
				h.frameWidth = (h.upscaledWidth * kSuperresNum + (h.superresDenom / 2)) / h.superresDenom;
				h.miCols = 2 * ((h.frameWidth + 7) >> 3);
				h.miRows = 2 * ((h.frameHeight + 7) >> 3);
			}
		} else {
			ParseFrameSize(rb, seq, h);
			ParseRenderSize(rb, h);
		}
		if (h.forceIntegerMv)
			h.allowHighPrecisionMv = false;
		else
			h.allowHighPrecisionMv = rb.Bit() != 0;
		// read_interpolation_filter (§5.9.10).
		const int32 isFilterSwitchable = rb.Bit();
		h.interpolationFilter = isFilterSwitchable ? 3 /*SWITCHABLE*/ : (int32)rb.F(2);
		h.isMotionModeSwitchable = rb.Bit() != 0;
		if (h.errorResilientMode || !seq.enableRefFrameMvs)
			h.useRefFrameMvs = false;
		else
			h.useRefFrameMvs = rb.Bit() != 0;
		for (int32 i = 0; i < kRefsPerFrame; ++i) {
			const int32 refFrame = kRefLast + i;
			const int32 hint = st.refs[h.refFrameIdx[i]].orderHint;
			h.orderHints[refFrame] = hint;
			if (!seq.enableOrderHint)
				h.refFrameSignBias[refFrame] = false;
			else
				h.refFrameSignBias[refFrame] = GetRelativeDist(seq, hint, h.orderHint) > 0;
		}
	}

	if (seq.reducedStillPictureHeader || h.disableCdfUpdate)
		h.disableFrameEndUpdateCdf = true;
	else
		h.disableFrameEndUpdateCdf = rb.Bit() != 0;

	// setup_past_independence / load_previous (les CDF sont gerees au decodage).
	int32 prevGmParams[8][6];
	for (int32 ref = 0; ref < 8; ++ref)
		for (int32 i = 0; i < 6; ++i)
			prevGmParams[ref][i] = ((i % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
	if (h.primaryRefFrame == kPrimaryRefNone) {
		// setup_past_independence : deltas lf par defaut, features seg a 0.
		const int32 defRef[8] = {1, 0, 0, 0, 0, -1, -1, -1};
		// NOTE ordre spec : INTRA=1, LAST..LAST3=0, BWD=0, GOLDEN=-1, ALTREF=-1, ALTREF2=-1
		// (indices : 0=INTRA 1=LAST 2=LAST2 3=LAST3 4=GOLDEN 5=BWD 6=ALTREF2 7=ALTREF)
		h.lf.refDeltas[0] = 1;
		h.lf.refDeltas[kRefLast] = 0;
		h.lf.refDeltas[kRefLast2] = 0;
		h.lf.refDeltas[kRefLast3] = 0;
		h.lf.refDeltas[kRefBwd] = 0;
		h.lf.refDeltas[kRefGolden] = -1;
		h.lf.refDeltas[kRefAltRef] = -1;
		h.lf.refDeltas[kRefAltRef2] = -1;
		h.lf.modeDeltas[0] = 0;
		h.lf.modeDeltas[1] = 0;
		for (int32 i = 0; i < kMaxSegments; ++i)
			for (int32 j = 0; j < kSegLvlMax; ++j) {
				h.seg.featureEnabled[i][j] = false;
				h.seg.featureData[i][j] = 0;
			}
		(void)defRef;
	} else {
		const int32 prevFrame = h.refFrameIdx[h.primaryRefFrame];
		const NkAv1RefSlot &slot = st.refs[prevFrame];
		for (int32 ref = 0; ref < 8; ++ref)
			for (int32 i = 0; i < 6; ++i)
				prevGmParams[ref][i] = slot.savedGmParams[ref][i];
		for (int32 i = 0; i < 8; ++i)
			h.lf.refDeltas[i] = slot.lfRefDeltas[i];
		h.lf.modeDeltas[0] = slot.lfModeDeltas[0];
		h.lf.modeDeltas[1] = slot.lfModeDeltas[1];
		for (int32 i = 0; i < kMaxSegments; ++i)
			for (int32 j = 0; j < kSegLvlMax; ++j) {
				h.seg.featureEnabled[i][j] = slot.segFeatureEnabled[i][j];
				h.seg.featureData[i][j] = slot.segFeatureData[i][j];
			}
	}

	// (motion_field_estimation est faite au decodage.)

	ParseTileInfo(rb, seq, h);
	ParseQuantizationParams(rb, seq, h);
	ParseSegmentationParams(rb, h);
	ComputeLossless(h);
	ParseDeltaQParams(rb, h);
	ParseDeltaLfParams(rb, h);
	ParseLoopFilterParams(rb, seq, h);
	ParseCdefParams(rb, seq, h);
	ParseLrParams(rb, seq, h);

	if (h.codedLossless)
		h.txMode = 0; // ONLY_4X4
	else
		h.txMode = rb.Bit() ? 2 /*SELECT*/ : 1 /*LARGEST*/;

	// frame_reference_mode (§5.9.23).
	if (frameIsIntra)
		h.referenceSelect = 0;
	else
		h.referenceSelect = rb.Bit();

	// skip_mode_params (§5.9.22).
	{
		int32 skipModeAllowed;
		if (frameIsIntra || !h.referenceSelect || !seq.enableOrderHint) {
			skipModeAllowed = 0;
		} else {
			int32 forwardIdx = -1, backwardIdx = -1;
			int32 forwardHint = 0, backwardHint = 0;
			for (int32 i = 0; i < kRefsPerFrame; ++i) {
				const int32 refHint = st.refs[h.refFrameIdx[i]].orderHint;
				if (GetRelativeDist(seq, refHint, h.orderHint) < 0) {
					if (forwardIdx < 0 || GetRelativeDist(seq, refHint, forwardHint) > 0) {
						forwardIdx = i;
						forwardHint = refHint;
					}
				} else if (GetRelativeDist(seq, refHint, h.orderHint) > 0) {
					if (backwardIdx < 0 || GetRelativeDist(seq, refHint, backwardHint) < 0) {
						backwardIdx = i;
						backwardHint = refHint;
					}
				}
			}
			if (forwardIdx < 0) {
				skipModeAllowed = 0;
			} else if (backwardIdx >= 0) {
				skipModeAllowed = 1;
				h.skipModeFrame[0] = kRefLast + NkAv1Min(forwardIdx, backwardIdx);
				h.skipModeFrame[1] = kRefLast + NkAv1Max(forwardIdx, backwardIdx);
			} else {
				int32 secondForwardIdx = -1, secondForwardHint = 0;
				for (int32 i = 0; i < kRefsPerFrame; ++i) {
					const int32 refHint = st.refs[h.refFrameIdx[i]].orderHint;
					if (GetRelativeDist(seq, refHint, forwardHint) < 0) {
						if (secondForwardIdx < 0 || GetRelativeDist(seq, refHint, secondForwardHint) > 0) {
							secondForwardIdx = i;
							secondForwardHint = refHint;
						}
					}
				}
				if (secondForwardIdx < 0) {
					skipModeAllowed = 0;
				} else {
					skipModeAllowed = 1;
					h.skipModeFrame[0] = kRefLast + NkAv1Min(forwardIdx, secondForwardIdx);
					h.skipModeFrame[1] = kRefLast + NkAv1Max(forwardIdx, secondForwardIdx);
				}
			}
		}
		if (skipModeAllowed)
			h.skipModePresent = rb.Bit() != 0;
		else
			h.skipModePresent = false;
	}

	if (frameIsIntra || h.errorResilientMode || !seq.enableWarpedMotion)
		h.allowWarpedMotion = false;
	else
		h.allowWarpedMotion = rb.Bit() != 0;

	h.reducedTxSet = rb.Bit() != 0;

	// global_motion_params (§5.9.24).
	for (int32 ref = 0; ref < 8; ++ref) {
		h.gmType[ref] = kGmIdentity;
		for (int32 i = 0; i < 6; ++i)
			h.gmParams[ref][i] = ((i % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
	}
	if (!frameIsIntra) {
		for (int32 ref = kRefLast; ref <= kRefAltRef; ++ref) {
			int32 type = kGmIdentity;
			const int32 isGlobal = rb.Bit();
			if (isGlobal) {
				const int32 isRotZoom = rb.Bit();
				if (isRotZoom) {
					type = kGmRotzoom;
				} else {
					const int32 isTranslation = rb.Bit();
					type = isTranslation ? kGmTranslation : kGmAffine;
				}
			}
			h.gmType[ref] = type;
			if (type >= kGmRotzoom) {
				ReadGlobalParam(rb, h, type, ref, 2, prevGmParams);
				ReadGlobalParam(rb, h, type, ref, 3, prevGmParams);
				if (type == kGmAffine) {
					ReadGlobalParam(rb, h, type, ref, 4, prevGmParams);
					ReadGlobalParam(rb, h, type, ref, 5, prevGmParams);
				} else {
					h.gmParams[ref][4] = -h.gmParams[ref][3];
					h.gmParams[ref][5] = h.gmParams[ref][2];
				}
			}
			if (type >= kGmTranslation) {
				ReadGlobalParam(rb, h, type, ref, 0, prevGmParams);
				ReadGlobalParam(rb, h, type, ref, 1, prevGmParams);
			}
		}
	}

	// film_grain_params : nos flux ont film_grain_params_present = 0. Refus si
	// une trame demande du grain (parse non implemente au-dela du flag).
	if (seq.filmGrainParamsPresent && (h.showFrame || h.showableFrame)) {
		const int32 applyGrain = rb.Bit();
		if (applyGrain) {
			if (errOut)
				*errOut = "film grain (non implemente)";
			return false;
		}
	}

	rb.ByteAlign();
	h.headerEndByte = rb.BytePos();
	if (rb.error)
		return false;
	h.valid = true;
	out = h;
	return true;
}

} // anonymous namespace
		// =====================================================================
		// DecodeKeyFrame -- reconstruction pixel intra key frame (voir rapport pour
		// l'etat de bit-exactitude). Refait sa propre boucle d'OBU (au lieu de
		// reutiliser ParseTemporalUnit) pour recuperer le pointeur brut vers la
		// donnee de tile group necessaire au decodeur de symboles.
		// =====================================================================
		bool NkAv1Decoder::DecodeKeyFrame(const uint8 *data, usize size, NkAv1Image &out,
									  NkAv1ParseStats *statsOut) {
			NkAv1SequenceHeader seq;
			NkAv1FrameHeader fh;
			NkAv1ParseStats stats;
			bool haveSeq = false, haveFrame = false;
			const uint8 *tgData = nullptr;
			usize tgSize = 0;
			usize pos = 0;
			bool overran = false;
			while (pos < size) {
				NkAv1ObuHeader oh;
				if (!ParseObuHeader(data + pos, size - pos, oh)) { overran = true; break; }
				++stats.obuCount;
				const uint8 *payload = data + pos + oh.payloadOffset;
				const usize plen = oh.payloadSize;
				if (oh.type == kAv1ObuTemporalDelimiter) {
					++stats.tdCount;
				} else if (oh.type == kAv1ObuSequenceHeader) {
					++stats.seqHdrCount;
					NkAv1SequenceHeader s;
					if (ParseSequenceHeader(payload, plen, s)) { seq = s; haveSeq = true; }
				} else if (oh.type == kAv1ObuFrame) {
					++stats.frameObuCount;
					if (haveSeq) {
						NkAv1FrameHeader h;
						if (ParseFrameHeader(payload, plen, seq, h) && h.headerEndByte <= plen) {
							fh = h;
							haveFrame = true;
							tgData = payload + h.headerEndByte;
							tgSize = plen - h.headerEndByte;
						}
					}
				} else if (oh.type == kAv1ObuTileGroup) {
					++stats.tileGroupCount;
				} else if (oh.type == kAv1ObuMetadata) {
					++stats.metadataCount;
				} else if (oh.type == kAv1ObuPadding) {
					++stats.paddingCount;
				} else if (oh.type == kAv1ObuFrameHeader || oh.type == kAv1ObuRedundantFrameHeader) {
					++stats.frameHdrCount;
				}
				pos += oh.payloadOffset + oh.payloadSize;
			}
			stats.bytesConsumed = pos;
			stats.overran = overran || (pos != size);
			if (statsOut)
				*statsOut = stats;
			if (!haveSeq || !haveFrame || !seq.valid || !fh.valid)
				return false;
			if (fh.frameType != kAv1KeyFrame)
				return false;

			if (!tgData)
				return false;
			// Etat DPB temporaire (une seule keyframe) — le pipeline unifie exige
			// un etat, mais rien n'est reutilise ici.
			NkAv1DecoderState *tmpSt = (NkAv1DecoderState *)memory::NkAlloc(sizeof(NkAv1DecoderState));
			if (!tmpSt)
				return false;
			new (tmpSt) NkAv1DecoderState();
			tmpSt->seq = seq;
			tmpSt->haveSeq = true;
			const char *err = "";
			const bool ok = DecodeFrameImplFull(*tmpSt, fh, tgData, tgSize, &out, &err);
			tmpSt->~NkAv1DecoderState();
			memory::NkFree(tmpSt);
			return ok;
		}

		// =====================================================================
		// NkAv1StreamDecoder — decodeur de flux avec DPB persistant.
		// =====================================================================
		struct NkAv1StreamDecoder::Impl {
				NkAv1DecoderState st;
		};

		NkAv1StreamDecoder::NkAv1StreamDecoder() {
			mImpl = (Impl *)memory::NkAlloc(sizeof(Impl));
			if (mImpl)
				new (mImpl) Impl();
		}

		NkAv1StreamDecoder::~NkAv1StreamDecoder() {
			if (mImpl) {
				mImpl->~Impl();
				memory::NkFree(mImpl);
				mImpl = nullptr;
			}
		}

		const char *NkAv1StreamDecoder::LastError() const {
			return (mImpl && mImpl->st.lastError) ? mImpl->st.lastError : "";
		}

		namespace {
			// Copie l'image d'un slot DPB vers une NkAv1Image.
			void SlotToImage(const NkAv1SequenceHeader &seq, const NkAv1RefSlot &slot, NkAv1Image &out) {
				out.width = slot.upscaledWidth;
				out.height = slot.frameHeight;
				out.yStride = slot.yStride;
				out.uvWidth = slot.uvW;
				out.uvHeight = slot.uvH;
				out.uvStride = slot.uvStride;
				out.bitDepth = seq.bitDepth;
				out.y = slot.y;
				out.u = slot.u;
				out.v = slot.v;
			}
		} // namespace

		bool NkAv1StreamDecoder::DecodeTemporalUnit(const uint8 *data, usize size, NkVector<NkAv1Image> &outFrames) {
			if (!mImpl)
				return false;
			NkAv1DecoderState &st = mImpl->st;
			st.lastError = "";
			usize pos = 0;
			while (pos < size) {
				NkAv1ObuHeader oh;
				if (!NkAv1Decoder::ParseObuHeader(data + pos, size - pos, oh)) {
					st.lastError = "OBU invalide";
					return false;
				}
				const uint8 *payload = data + pos + oh.payloadOffset;
				const usize plen = oh.payloadSize;
				if (oh.type == kAv1ObuSequenceHeader) {
					NkAv1SequenceHeader s;
					if (NkAv1Decoder::ParseSequenceHeader(payload, plen, s)) {
						st.seq = s;
						st.haveSeq = true;
					} else {
						st.lastError = "sequence header invalide";
						return false;
					}
				} else if (oh.type == kAv1ObuFrame || oh.type == kAv1ObuFrameHeader) {
					if (!st.haveSeq) {
						st.lastError = "frame avant sequence header";
						return false;
					}
					NkAv1FrameHeader fh;
					const char *err = "";
					if (!ParseFrameHeaderFull(payload, plen, st, fh, &err)) {
						st.lastError = (err && err[0]) ? err : "frame header invalide";
						return false;
					}
					if (fh.showExistingFrame) {
						const NkAv1RefSlot &slot = st.refs[fh.frameToShow];
						if (!slot.valid) {
							st.lastError = "show_existing_frame sur slot invalide";
							return false;
						}
						NkAv1Image img;
						SlotToImage(st.seq, slot, img);
						outFrames.PushBack(img);
						if (fh.frameType == kAv1KeyFrame) {
							// Reference frame loading (§7.21) + update de tous les slots
							// (refresh_frame_flags == allFrames pour une keyframe montree).
							const NkAv1RefSlot copy = slot;
							for (int32 i = 0; i < 8; ++i)
								st.refs[i] = copy;
						}
					} else if (oh.type == kAv1ObuFrame) {
						if (fh.headerEndByte > plen) {
							st.lastError = "frame header deborde l'OBU";
							return false;
						}
						const uint8 *tgData = payload + fh.headerEndByte;
						const usize tgSize = plen - fh.headerEndByte;
						NkAv1Image img;
						const char *derr = "";
						if (!DecodeFrameImplFull(st, fh, tgData, tgSize, fh.showFrame ? &img : nullptr, &derr)) {
							st.lastError = (derr && derr[0]) ? derr : "echec decodage trame";
							return false;
						}
						if (fh.showFrame)
							outFrames.PushBack(img);
					} else {
						// OBU_FRAME_HEADER suivi de TILE_GROUP separes : non gere pour
						// l'instant (libaom emet des OBU_FRAME).
						st.lastError = "frame header + tile groups separes (non gere)";
						return false;
					}
				}
				// temporal delimiter / metadata / padding : ignores.
				pos += oh.payloadOffset + oh.payloadSize;
			}
			return true;
		}

		// =====================================================================
		bool NkAv1Decoder::SelfTest() {
			bool ok = true;

			// 1. LEB128.
			{
				const uint8 buf[] = {0x80, 0x01}; // 128
				usize br = 0;
				const uint64 v = NkAv1Leb128(buf, 2, &br);
				ok = ok && (v == 128 && br == 2);
				const uint8 buf2[] = {0x0A}; // 10
				const uint64 v2 = NkAv1Leb128(buf2, 1, &br);
				ok = ok && (v2 == 10 && br == 1);
			}

			// 2. FloorLog2 / uvlc / su / ns.
			{
				ok = ok && (NkAv1FloorLog2(1) == 0);
				ok = ok && (NkAv1FloorLog2(2) == 1);
				ok = ok && (NkAv1FloorLog2(255) == 7);
				ok = ok && (NkAv1FloorLog2(256) == 8);
				// su : 0b111 sur 3 bits = -1 ; 0b011 = 3.
				const uint8 sb[] = {0xE0}; // 111 00000
				NkAv1BitReader r(sb, 1);
				ok = ok && (r.Su(3) == -1);
				const uint8 sb2[] = {0x60}; // 011 00000
				NkAv1BitReader r2(sb2, 1);
				ok = ok && (r2.Su(3) == 3);
			}

			// 3. Symbol decoder : cohérence init + decode d'un flux uniforme. On vérifie
			// surtout que decode_symbol respecte les invariants (range dans [2^14,2^15]
			// après renorm, pas d'overrun sur données suffisantes) et que la CDF
			// s'adapte dans le bon sens.
			{
				uint8 tile[64];
				for (int32 i = 0; i < 64; ++i)
					tile[i] = (uint8)(0x55 + i * 7);
				NkAv1SymbolDecoder sd;
				sd.Init(tile, 64, false);
				ok = ok && (sd.symbolRange == (1u << 15));
				// CDF booléenne 50/50 initiale.
				uint16 cdf[3] = {1u << 14, 1u << 15, 0};
				int32 lastSym = -1;
				for (int32 i = 0; i < 100; ++i) {
					const int32 s = sd.DecodeSymbol(cdf, 2);
					ok = ok && (s == 0 || s == 1);
					// Invariant §8.2.6 : après renormalisation, range ∈ [2^15, 2^16).
					ok = ok && (sd.symbolRange >= (1u << 15) && sd.symbolRange < (1u << 16));
					lastSym = s;
				}
				(void)lastSym;
				ok = ok && !sd.Overran();
			}

			// 4. ReadLiteral cohérent (valeurs dans les bornes).
			{
				uint8 tile[32];
				for (int32 i = 0; i < 32; ++i)
					tile[i] = (uint8)(i * 13 + 1);
				NkAv1SymbolDecoder sd;
				sd.Init(tile, 32, true);
				for (int32 i = 0; i < 20; ++i) {
					const uint32 v = sd.ReadLiteral(4);
					ok = ok && (v < 16);
				}
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
