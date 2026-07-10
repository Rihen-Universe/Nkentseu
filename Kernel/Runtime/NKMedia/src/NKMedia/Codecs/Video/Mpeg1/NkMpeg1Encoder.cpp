// =============================================================================
// NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.cpp — encodeur MPEG-1 Video I-picture.
// =============================================================================
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.h"
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // cos

namespace nkentseu {
	namespace media {

		namespace {
			inline uint8 ClampU8(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}
			inline int32 Clamp(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}

			// DCT 8×8 directe (séparable, O(N^3)) — précalcule la table de cosinus.
			struct DctCos {
					float32 c[8][8];
					DctCos() {
						for (int32 u = 0; u < 8; ++u)
							for (int32 x = 0; x < 8; ++x)
								c[u][x] = (float32)::cos((2.0 * x + 1.0) * u * 3.14159265358979323846 / 16.0);
					}
			};
			const DctCos &Cos() {
				static DctCos d;
				return d;
			}

			// Forward DCT sur un bloc 8×8 (valeurs 0..255) → coefficients (raster).
			void Fdct8x8(const float32 *in, float32 *out) {
				const DctCos &T = Cos();
				float32 tmp[64];
				// lignes
				for (int32 y = 0; y < 8; ++y) {
					for (int32 u = 0; u < 8; ++u) {
						float32 s = 0.0f;
						for (int32 x = 0; x < 8; ++x)
							s += in[y * 8 + x] * T.c[u][x];
						const float32 cu = (u == 0) ? 0.70710678f : 1.0f;
						tmp[y * 8 + u] = 0.5f * cu * s;
					}
				}
				// colonnes
				for (int32 x = 0; x < 8; ++x) {
					for (int32 v = 0; v < 8; ++v) {
						float32 s = 0.0f;
						for (int32 y = 0; y < 8; ++y)
							s += tmp[y * 8 + x] * T.c[v][y];
						const float32 cv = (v == 0) ? 0.70710678f : 1.0f;
						out[v * 8 + x] = 0.5f * cv * s;
					}
				}
			}
		} // namespace

		bool NkMpeg1Encoder::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
								  int32 qscale) {
			if (width <= 0 || height <= 0 || fpsDen <= 0 || fpsNum <= 0)
				return false;
			mWidth = width;
			mHeight = height;
			mFpsNum = fpsNum;
			mFpsDen = fpsDen;
			mQScale = Clamp(qscale, 1, 31);
			mMbW = (width + 15) / 16;
			mMbH = (height + 15) / 16;
			mLumaW = mMbW * 16;
			mLumaH = mMbH * 16;
			mChromaW = mLumaW / 2;
			mChromaH = mLumaH / 2;
			mFrame = 0;
			mWroteSeq = false;

			mY = (uint8 *)memory::NkAlloc((usize)mLumaW * mLumaH);
			mCb = (uint8 *)memory::NkAlloc((usize)mChromaW * mChromaH);
			mCr = (uint8 *)memory::NkAlloc((usize)mChromaW * mChromaH);
			if (!mY || !mCb || !mCr)
				return false;

			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode))
				return false;
			mOpen = true;
			return true;
		}

		void NkMpeg1Encoder::ConvertToYuv(const uint8 *pixels, NkVideoInputFormat fmt) {
			const int32 ibpp = (fmt == NkVideoInputFormat::RGBA32) ? 4 : 3;
			// Luma plein + accumulateurs chroma (4:2:0).
			for (int32 y = 0; y < mLumaH; ++y) {
				const int32 sy = Clamp(y, 0, mHeight - 1);
				for (int32 x = 0; x < mLumaW; ++x) {
					const int32 sx = Clamp(x, 0, mWidth - 1);
					const uint8 *p = pixels + ((usize)sy * mWidth + sx) * ibpp;
					int32 r, g, b;
					if (fmt == NkVideoInputFormat::BGR24) {
						b = p[0];
						g = p[1];
						r = p[2];
					} else {
						r = p[0];
						g = p[1];
						b = p[2];
					}
					const int32 Y = (77 * r + 150 * g + 29 * b) >> 8; // 0.299/0.587/0.114
					mY[(usize)y * mLumaW + x] = ClampU8(Y);
				}
			}
			// Chroma : sous-échantillonnage 2×2 (moyenne).
			for (int32 cy = 0; cy < mChromaH; ++cy) {
				for (int32 cx = 0; cx < mChromaW; ++cx) {
					int32 sr = 0, sg = 0, sb = 0;
					for (int32 dy = 0; dy < 2; ++dy)
						for (int32 dx = 0; dx < 2; ++dx) {
							const int32 yy = Clamp(cy * 2 + dy, 0, mHeight - 1);
							const int32 xx = Clamp(cx * 2 + dx, 0, mWidth - 1);
							const uint8 *p = pixels + ((usize)yy * mWidth + xx) * ibpp;
							if (fmt == NkVideoInputFormat::BGR24) {
								sb += p[0];
								sg += p[1];
								sr += p[2];
							} else {
								sr += p[0];
								sg += p[1];
								sb += p[2];
							}
						}
					const int32 r = sr / 4, g = sg / 4, b = sb / 4;
					const int32 Cb = 128 + ((-43 * r - 85 * g + 128 * b) >> 8);
					const int32 Cr = 128 + ((128 * r - 107 * g - 21 * b) >> 8);
					mCb[(usize)cy * mChromaW + cx] = ClampU8(Cb);
					mCr[(usize)cy * mChromaW + cx] = ClampU8(Cr);
				}
			}
		}

		void NkMpeg1Encoder::WriteSequenceHeader(NkBitWriter &bw) {
			bw.PutStartCode(0xB3);
			bw.PutBits((uint32)mWidth, 12);
			bw.PutBits((uint32)mHeight, 12);
			bw.PutBits(1, 4); // aspect_ratio (1 = 1:1)
			// frame_rate_code
			uint32 frc = 5; // 30
			const double fps = (double)mFpsNum / (double)mFpsDen;
			if (fps > 23.5 && fps < 24.5)
				frc = 2;
			else if (fps > 24.5 && fps < 25.5)
				frc = 3;
			else if (fps > 29.0 && fps < 29.98)
				frc = 4;
			else if (fps >= 29.98 && fps < 30.5)
				frc = 5;
			else if (fps > 49.0 && fps < 50.5)
				frc = 6;
			else if (fps >= 59.0 && fps < 60.5)
				frc = 8;
			bw.PutBits(frc, 4);
			bw.PutBits(0x3FFFF, 18); // bit_rate (variable)
			bw.PutBits(1, 1);		 // marker
			bw.PutBits(112, 10);	 // vbv_buffer_size
			bw.PutBits(0, 1);		 // constrained_parameters_flag
			bw.PutBits(0, 1);		 // load_intra_quantizer_matrix (défaut)
			bw.PutBits(0, 1);		 // load_non_intra_quantizer_matrix (défaut)
		}

		void NkMpeg1Encoder::WriteGopHeader(NkBitWriter &bw) {
			bw.PutStartCode(0xB8);
			// time_code : drop(1)+h(5)+m(6)+marker(1)+s(6)+f(6) = 25 bits.
			const int32 totalFrames = mFrame;
			const int32 fpsI = (mFpsNum + mFpsDen / 2) / mFpsDen;
			const int32 f = fpsI > 0 ? totalFrames % fpsI : 0;
			const int32 s = fpsI > 0 ? (totalFrames / fpsI) % 60 : 0;
			const int32 m = fpsI > 0 ? (totalFrames / fpsI / 60) % 60 : 0;
			const int32 h = fpsI > 0 ? (totalFrames / fpsI / 3600) % 24 : 0;
			bw.PutBits(0, 1);			  // drop_frame
			bw.PutBits((uint32)h, 5);
			bw.PutBits((uint32)m, 6);
			bw.PutBits(1, 1);			  // marker
			bw.PutBits((uint32)s, 6);
			bw.PutBits((uint32)f, 6);
			bw.PutBits(1, 1); // closed_gop
			bw.PutBits(0, 1); // broken_link
		}

		void NkMpeg1Encoder::EncodeIntraBlock(NkBitWriter &bw, const uint8 *plane, int32 planeW, int32 x0, int32 y0,
											  bool luma, int32 &dcPred) {
			// Charge le bloc 8×8.
			float32 in[64];
			for (int32 y = 0; y < 8; ++y)
				for (int32 x = 0; x < 8; ++x)
					in[y * 8 + x] = (float32)plane[(usize)(y0 + y) * planeW + (x0 + x)];
			float32 coef[64];
			Fdct8x8(in, coef);

			const uint16 *W = NkMpeg1Tables::IntraMatrix();
			const uint8 *zz = NkMpeg1Tables::ZigZag();

			// --- DC ---
			int32 dc = (int32)::floor(coef[0] / 8.0f + 0.5f); // quant DC (intra_dc_precision 8)
			dc = Clamp(dc, 0, 255);
			int32 diff = dc - dcPred;
			dcPred = dc;
			// taille = nombre de bits pour |diff|
			int32 absd = diff < 0 ? -diff : diff;
			int32 size = 0;
			while ((1 << size) <= absd)
				++size;
			const uint16 *dcCode = luma ? NkMpeg1Tables::DcLumCode() : NkMpeg1Tables::DcChromaCode();
			const uint8 *dcBits = luma ? NkMpeg1Tables::DcLumBits() : NkMpeg1Tables::DcChromaBits();
			bw.PutBits(dcCode[size], dcBits[size]);
			if (size > 0) {
				uint32 val = (diff >= 0) ? (uint32)diff : (uint32)(diff + (1 << size) - 1);
				bw.PutBits(val, size);
			}

			// --- AC ---
			int32 quant[64];
			for (int32 i = 1; i < 64; ++i) {
				const int32 pos = zz[i]; // position raster du i-ème coeff en zig-zag
				const float32 q = coef[pos] * 8.0f / (float32)(mQScale * W[pos]);
				int32 lv = (int32)(q < 0 ? ::ceil(q - 0.5f) : ::floor(q + 0.5f));
				quant[i] = Clamp(lv, -255, 255);
			}

			const uint16(*ac)[2] = NkMpeg1Tables::AcVlc();
			int32 run = 0;
			for (int32 i = 1; i < 64; ++i) {
				const int32 lv = quant[i];
				if (lv == 0) {
					++run;
					continue;
				}
				const int32 absLevel = lv < 0 ? -lv : lv;
				const int32 idx = NkMpeg1Tables::FindAc(run, absLevel);
				if (idx >= 0) {
					bw.PutBits(ac[idx][0], ac[idx][1]);
					bw.PutBits(lv < 0 ? 1u : 0u, 1); // signe
				} else {
					// escape : 000001 + run(6) + level.
					bw.PutBits(ac[NkMpeg1Tables::kAcEscape][0], ac[NkMpeg1Tables::kAcEscape][1]);
					bw.PutBits((uint32)run, 6);
					if (lv >= 1 && lv <= 127)
						bw.PutBits((uint32)lv, 8);
					else if (lv >= -127 && lv <= -1)
						bw.PutBits((uint32)(lv & 0xFF), 8);
					else if (lv >= 128 && lv <= 255) {
						bw.PutBits(0x00, 8);
						bw.PutBits((uint32)lv, 8);
					} else { // -255..-128
						bw.PutBits(0x80, 8);
						bw.PutBits((uint32)((lv + 256) & 0xFF), 8);
					}
				}
				run = 0;
			}
			// EOB
			bw.PutBits(ac[NkMpeg1Tables::kAcEob][0], ac[NkMpeg1Tables::kAcEob][1]);
		}

		void NkMpeg1Encoder::EncodePicture(NkBitWriter &bw, int32 temporalRef) {
			// --- picture header ---
			bw.PutStartCode(0x00);
			bw.PutBits((uint32)(temporalRef & 0x3FF), 10);
			bw.PutBits(1, 3);	   // picture_coding_type = I
			bw.PutBits(0xFFFF, 16); // vbv_delay
			bw.PutBits(0, 1);	   // extra_bit_picture

			// --- slices : une par rangée de macroblocs ---
			for (int32 mbRow = 0; mbRow < mMbH; ++mbRow) {
				bw.PutStartCode((uint8)(mbRow + 1)); // slice_vertical_position
				bw.PutBits((uint32)mQScale, 5);		 // quantizer_scale_code
				bw.PutBits(0, 1);					 // extra_bit_slice

				int32 dcY = 128, dcCb = 128, dcCr = 128; // prédicteurs DC (reset par slice)
				for (int32 mbCol = 0; mbCol < mMbW; ++mbCol) {
					bw.PutBits(1, 1); // macroblock_address_increment = 1
					bw.PutBits(1, 1); // macroblock_type = intra (I-picture)

					const int32 lx = mbCol * 16, ly = mbRow * 16;
					// 4 blocs Y
					EncodeIntraBlock(bw, mY, mLumaW, lx + 0, ly + 0, true, dcY);
					EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly + 0, true, dcY);
					EncodeIntraBlock(bw, mY, mLumaW, lx + 0, ly + 8, true, dcY);
					EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly + 8, true, dcY);
					// Cb, Cr (8×8, position moitié)
					const int32 cx = mbCol * 8, cy = mbRow * 8;
					EncodeIntraBlock(bw, mCb, mChromaW, cx, cy, false, dcCb);
					EncodeIntraBlock(bw, mCr, mChromaW, cx, cy, false, dcCr);
				}
			}
		}

		void NkMpeg1Encoder::Flush(NkBitWriter &bw) {
			bw.AlignByteZero();
			if (bw.Size() > 0)
				mFile.Write(bw.Data(), bw.Size());
			bw.Clear();
		}

		bool NkMpeg1Encoder::WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt) {
			if (!mOpen || pixels == nullptr)
				return false;
			ConvertToYuv(pixels, fmt);

			NkBitWriter bw;
			if (!mWroteSeq) {
				WriteSequenceHeader(bw);
				mWroteSeq = true;
			}
			// Un GOP toutes les 12 images (tout-I : chaque image est sa propre référence).
			int32 temporalRef = mFrame % 12;
			if (temporalRef == 0)
				WriteGopHeader(bw);
			EncodePicture(bw, temporalRef);
			Flush(bw);

			mFrame++;
			return true;
		}

		bool NkMpeg1Encoder::Close() {
			if (!mOpen)
				return false;
			// sequence_end_code
			NkBitWriter bw;
			bw.PutStartCode(0xB7);
			Flush(bw);
			mFile.Close();
			if (mY)
				memory::NkFree(mY);
			if (mCb)
				memory::NkFree(mCb);
			if (mCr)
				memory::NkFree(mCr);
			mY = mCb = mCr = nullptr;
			mOpen = false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
