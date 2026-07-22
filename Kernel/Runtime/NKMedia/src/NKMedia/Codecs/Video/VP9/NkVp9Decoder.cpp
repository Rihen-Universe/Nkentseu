// =============================================================================
// NKMedia/Codecs/Video/VP9/NkVp9Decoder.cpp — brique 1 : superframes + en-tête
// de trame non compressé (spec VP9 §6.2 / Annexe B, vérifié contre l'ordre exact
// de vp9_decodeframe.c read_uncompressed_header — réécrit, zéro code importé).
// =============================================================================
#include "NKMedia/Codecs/Video/VP9/NkVp9Decoder.h"

namespace nkentseu {
	namespace media {

		namespace {

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
				if (rb.Bit()) { // update_data
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

		bool NkVp9Decoder::ParseUncompressedHeader(const uint8 *data, usize size, NkVp9FrameHeader &out) {
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
					// taille explicite. La taille héritée exige l'état des refs (brique
					// du décodeur complet) — ICI on note juste le flag choisi.
					bool found = false;
					for (int32 i = 0; i < 3; ++i) {
						if (rb.Bit()) {
							found = true;
							// Taille = celle de la référence i (résolue plus tard par le
							// décodeur complet ; le harnais la considère inchangée).
							out.width = -(out.refFrameIdx[i] + 1); // sentinelle : réf i
							out.height = out.width;
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
