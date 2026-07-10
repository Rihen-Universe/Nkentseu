// =============================================================================
// NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.cpp — encodeur MPEG-1 Video.
// I-pictures (intra) + P-pictures (compensation de mouvement full-pel) → vraie
// compression inter-frame. Reconstruction (dequant + IDCT) pour la trame de
// référence. Algorithme réécrit à la sauce Nkentseu.
// =============================================================================
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.h"
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // cos, floor

namespace nkentseu {
	namespace media {

		namespace {
			inline uint8 ClampU8(int32 v) {
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}
			inline int32 Clamp(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			inline int32 Sign(int32 v) {
				return v > 0 ? 1 : (v < 0 ? -1 : 0);
			}

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

			// DCT 8×8 directe (orthonormale : facteur 0.5·C).
			void Fdct8x8(const float32 *in, float32 *out) {
				const DctCos &T = Cos();
				float32 tmp[64];
				for (int32 y = 0; y < 8; ++y)
					for (int32 u = 0; u < 8; ++u) {
						float32 s = 0.0f;
						for (int32 x = 0; x < 8; ++x)
							s += in[y * 8 + x] * T.c[u][x];
						tmp[y * 8 + u] = 0.5f * ((u == 0) ? 0.70710678f : 1.0f) * s;
					}
				for (int32 x = 0; x < 8; ++x)
					for (int32 v = 0; v < 8; ++v) {
						float32 s = 0.0f;
						for (int32 y = 0; y < 8; ++y)
							s += tmp[y * 8 + x] * T.c[v][y];
						out[v * 8 + x] = 0.5f * ((v == 0) ? 0.70710678f : 1.0f) * s;
					}
			}

			// IDCT 8×8 (inverse orthonormale).
			void Idct8x8(const float32 *in, float32 *out) {
				const DctCos &T = Cos();
				float32 mid[64];
				for (int32 x = 0; x < 8; ++x)
					for (int32 y = 0; y < 8; ++y) {
						float32 s = 0.0f;
						for (int32 v = 0; v < 8; ++v)
							s += in[v * 8 + x] * (0.5f * ((v == 0) ? 0.70710678f : 1.0f)) * T.c[v][y];
						mid[y * 8 + x] = s;
					}
				for (int32 y = 0; y < 8; ++y)
					for (int32 x = 0; x < 8; ++x) {
						float32 s = 0.0f;
						for (int32 u = 0; u < 8; ++u)
							s += mid[y * 8 + u] * (0.5f * ((u == 0) ? 0.70710678f : 1.0f)) * T.c[u][x];
						out[y * 8 + x] = s;
					}
			}

			// Oddification MPEG-1 (mismatch) : force chaque coefficient non nul à être impair.
			inline int32 Oddify(int32 v) {
				if (v != 0 && (v & 1) == 0)
					v -= Sign(v);
				return v;
			}
		} // namespace

		bool NkMpeg1Encoder::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
								  int32 qscale, int32 gop) {
			if (width <= 0 || height <= 0 || fpsDen <= 0 || fpsNum <= 0)
				return false;
			mWidth = width;
			mHeight = height;
			mFpsNum = fpsNum;
			mFpsDen = fpsDen;
			mQScale = Clamp(qscale, 1, 31);
			mGop = gop < 1 ? 1 : gop;
			mMbW = (width + 15) / 16;
			mMbH = (height + 15) / 16;
			mLumaW = mMbW * 16;
			mLumaH = mMbH * 16;
			mChromaW = mLumaW / 2;
			mChromaH = mLumaH / 2;
			mFrame = 0;
			mWroteSeq = false;

			const usize lumaN = (usize)mLumaW * mLumaH, chromaN = (usize)mChromaW * mChromaH;
			mY = (uint8 *)memory::NkAlloc(lumaN);
			mCb = (uint8 *)memory::NkAlloc(chromaN);
			mCr = (uint8 *)memory::NkAlloc(chromaN);
			mRecY = (uint8 *)memory::NkAlloc(lumaN);
			mRecCb = (uint8 *)memory::NkAlloc(chromaN);
			mRecCr = (uint8 *)memory::NkAlloc(chromaN);
			mRefY = (uint8 *)memory::NkAlloc(lumaN);
			mRefCb = (uint8 *)memory::NkAlloc(chromaN);
			mRefCr = (uint8 *)memory::NkAlloc(chromaN);
			if (!mY || !mCb || !mCr || !mRecY || !mRecCb || !mRecCr || !mRefY || !mRefCb || !mRefCr)
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
					mY[(usize)y * mLumaW + x] = ClampU8((77 * r + 150 * g + 29 * b) >> 8);
				}
			}
			for (int32 cy = 0; cy < mChromaH; ++cy)
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
					mCb[(usize)cy * mChromaW + cx] = ClampU8(128 + ((-43 * r - 85 * g + 128 * b) >> 8));
					mCr[(usize)cy * mChromaW + cx] = ClampU8(128 + ((128 * r - 107 * g - 21 * b) >> 8));
				}
		}

		void NkMpeg1Encoder::WriteSequenceHeader(NkBitWriter &bw) {
			bw.PutStartCode(0xB3);
			bw.PutBits((uint32)mWidth, 12);
			bw.PutBits((uint32)mHeight, 12);
			bw.PutBits(1, 4);
			uint32 frc = 5;
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
			bw.PutBits(0x3FFFF, 18);
			bw.PutBits(1, 1);
			bw.PutBits(112, 10);
			bw.PutBits(0, 1);
			bw.PutBits(0, 1);
			bw.PutBits(0, 1);
		}

		void NkMpeg1Encoder::WriteGopHeader(NkBitWriter &bw) {
			bw.PutStartCode(0xB8);
			const int32 fpsI = (mFpsNum + mFpsDen / 2) / mFpsDen;
			const int32 tf = mFrame;
			const int32 f = fpsI > 0 ? tf % fpsI : 0, s = fpsI > 0 ? (tf / fpsI) % 60 : 0;
			const int32 m = fpsI > 0 ? (tf / fpsI / 60) % 60 : 0, h = fpsI > 0 ? (tf / fpsI / 3600) % 24 : 0;
			bw.PutBits(0, 1);
			bw.PutBits((uint32)h, 5);
			bw.PutBits((uint32)m, 6);
			bw.PutBits(1, 1);
			bw.PutBits((uint32)s, 6);
			bw.PutBits((uint32)f, 6);
			bw.PutBits(1, 1);
			bw.PutBits(0, 1);
		}

		// Encode le DC d'un bloc intra.
		static void EncodeDc(NkBitWriter &bw, int32 diff, bool luma) {
			const int32 absd = diff < 0 ? -diff : diff;
			int32 size = 0;
			while ((1 << size) <= absd)
				++size;
			const uint16 *code = luma ? NkMpeg1Tables::DcLumCode() : NkMpeg1Tables::DcChromaCode();
			const uint8 *bits = luma ? NkMpeg1Tables::DcLumBits() : NkMpeg1Tables::DcChromaBits();
			bw.PutBits(code[size], bits[size]);
			if (size > 0)
				bw.PutBits((diff >= 0) ? (uint32)diff : (uint32)(diff + (1 << size) - 1), size);
		}

		// Encode un couple (run, level) AC ou l'escape.
		static void EncodeAcCoef(NkBitWriter &bw, int32 run, int32 level) {
			const uint16(*ac)[2] = NkMpeg1Tables::AcVlc();
			const int32 absLevel = level < 0 ? -level : level;
			const int32 idx = NkMpeg1Tables::FindAc(run, absLevel);
			if (idx >= 0) {
				bw.PutBits((ac[idx][0] << 1) | (level < 0 ? 1u : 0u), ac[idx][1] + 1);
			} else {
				bw.PutBits((0x1 << 6) | (uint32)run, 12); // escape(6) + run(6)
				if (absLevel < 128)
					bw.PutBits((uint32)(level & 0xFF), 8);
				else if (level >= 128)
					bw.PutBits((uint32)level, 16);
				else
					bw.PutBits((uint32)(0x8001 + level + 255) & 0xFFFF, 16);
			}
		}

		void NkMpeg1Encoder::EncodeIntraBlock(NkBitWriter &bw, const uint8 *plane, int32 planeW, int32 x0, int32 y0,
											  bool luma, int32 &dcPred, uint8 *rec) {
			float32 in[64], coef[64];
			for (int32 y = 0; y < 8; ++y)
				for (int32 x = 0; x < 8; ++x)
					in[y * 8 + x] = (float32)plane[(usize)(y0 + y) * planeW + (x0 + x)];
			Fdct8x8(in, coef);

			const uint16 *W = NkMpeg1Tables::IntraMatrix();
			const uint8 *zz = NkMpeg1Tables::ZigZag();

			int32 q[64];
			int32 dc = Clamp((int32)::floor(coef[0] / 8.0f + 0.5f), 0, 255);
			q[0] = dc;
			for (int32 i = 1; i < 64; ++i)
				q[i] = 0;
			for (int32 i = 1; i < 64; ++i) {
				const int32 pos = zz[i];
				const float32 v = coef[pos] * 8.0f / (float32)(mQScale * W[pos]);
				q[pos] = Clamp((int32)(v < 0 ? ::ceil(v - 0.5f) : ::floor(v + 0.5f)), -255, 255);
			}

			// --- bitstream : DC diff + AC (scan zig-zag) + EOB ---
			EncodeDc(bw, dc - dcPred, luma);
			dcPred = dc;
			int32 run = 0;
			for (int32 i = 1; i < 64; ++i) {
				const int32 lv = q[zz[i]];
				if (lv == 0) {
					++run;
					continue;
				}
				EncodeAcCoef(bw, run, lv);
				run = 0;
			}
			bw.PutBits(NkMpeg1Tables::AcVlc()[NkMpeg1Tables::kAcEob][0],
					   NkMpeg1Tables::AcVlc()[NkMpeg1Tables::kAcEob][1]);

			// --- reconstruction : dequant intra + IDCT ---
			float32 dq[64];
			dq[0] = (float32)(q[0] * 8);
			for (int32 i = 1; i < 64; ++i) {
				const int32 pos = zz[i];
				const int32 lv = q[pos];
				int32 r = (lv == 0) ? 0 : Sign(lv) * ((2 * (lv < 0 ? -lv : lv) * mQScale * (int32)W[pos]) / 16);
				r = Oddify(Clamp(r, -2048, 2047));
				dq[pos] = (float32)r;
			}
			float32 out[64];
			Idct8x8(dq, out);
			for (int32 y = 0; y < 8; ++y)
				for (int32 x = 0; x < 8; ++x)
					rec[(usize)(y0 + y) * planeW + (x0 + x)] = ClampU8((int32)::floor(out[y * 8 + x] + 0.5f));
		}

		void NkMpeg1Encoder::EncodePictureI(NkBitWriter &bw, int32 temporalRef) {
			bw.PutStartCode(0x00);
			bw.PutBits((uint32)(temporalRef & 0x3FF), 10);
			bw.PutBits(1, 3); // I
			bw.PutBits(0xFFFF, 16);
			bw.PutBits(0, 1);

			for (int32 mbRow = 0; mbRow < mMbH; ++mbRow) {
				bw.PutStartCode((uint8)(mbRow + 1));
				bw.PutBits((uint32)mQScale, 5);
				bw.PutBits(0, 1);
				int32 dcY = 128, dcCb = 128, dcCr = 128;
				for (int32 mbCol = 0; mbCol < mMbW; ++mbCol) {
					bw.PutBits(1, 1); // addr increment
					bw.PutBits(1, 1); // type intra
					const int32 lx = mbCol * 16, ly = mbRow * 16, cx = mbCol * 8, cy = mbRow * 8;
					EncodeIntraBlock(bw, mY, mLumaW, lx, ly, true, dcY, mRecY);
					EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly, true, dcY, mRecY);
					EncodeIntraBlock(bw, mY, mLumaW, lx, ly + 8, true, dcY, mRecY);
					EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly + 8, true, dcY, mRecY);
					EncodeIntraBlock(bw, mCb, mChromaW, cx, cy, false, dcCb, mRecCb);
					EncodeIntraBlock(bw, mCr, mChromaW, cx, cy, false, dcCr, mRecCr);
				}
			}
		}

		// --- P-picture ---
		namespace {
			// SAD 16×16 luma entre source (à sx,sy) et référence (à sx+dx, sy+dy), clampé.
			int32 Sad16(const uint8 *src, const uint8 *ref, int32 w, int32 h, int32 sx, int32 sy, int32 dx,
						int32 dy) {
				int32 sad = 0;
				for (int32 y = 0; y < 16; ++y) {
					const int32 ry = Clamp(sy + dy + y, 0, h - 1);
					const int32 syy = sy + y;
					for (int32 x = 0; x < 16; ++x) {
						const int32 rx = Clamp(sx + dx + x, 0, w - 1);
						const int32 a = src[(usize)syy * w + (sx + x)];
						const int32 b = ref[(usize)ry * w + rx];
						sad += a > b ? a - b : b - a;
					}
				}
				return sad;
			}

			// Prédiction half-pel bilinéaire d'une région dstW×dstH depuis `ref`. MV en demi-pel.
			void InterpRegion(const uint8 *ref, int32 w, int32 h, int32 x0, int32 y0, int32 mvxHp, int32 mvyHp,
							  int32 dstW, int32 dstH, float32 *out) {
				const int32 ix = mvxHp >> 1, iy = mvyHp >> 1; // partie entière (floor)
				const int32 fx = mvxHp & 1, fy = mvyHp & 1;	  // demi-pel
				for (int32 y = 0; y < dstH; ++y) {
					const int32 ry = Clamp(y0 + iy + y, 0, h - 1);
					const int32 ry1 = Clamp(y0 + iy + y + 1, 0, h - 1);
					for (int32 x = 0; x < dstW; ++x) {
						const int32 rx = Clamp(x0 + ix + x, 0, w - 1);
						const int32 rx1 = Clamp(x0 + ix + x + 1, 0, w - 1);
						const int32 a = ref[(usize)ry * w + rx];
						int32 v;
						if (fx == 0 && fy == 0)
							v = a;
						else if (fx == 1 && fy == 0)
							v = (a + ref[(usize)ry * w + rx1] + 1) >> 1;
						else if (fx == 0 && fy == 1)
							v = (a + ref[(usize)ry1 * w + rx] + 1) >> 1;
						else
							v = (a + ref[(usize)ry * w + rx1] + ref[(usize)ry1 * w + rx] + ref[(usize)ry1 * w + rx1] +
								 2) >>
								2;
						out[y * dstW + x] = (float32)v;
					}
				}
			}

			// SAD 16×16 luma à un MV demi-pel (interpolé).
			int32 Sad16Hp(const uint8 *src, int32 sw, int32 sx, int32 sy, const uint8 *ref, int32 w, int32 h,
						  int32 mvxHp, int32 mvyHp) {
				float32 pred[256];
				InterpRegion(ref, w, h, sx, sy, mvxHp, mvyHp, 16, 16, pred);
				int32 sad = 0;
				for (int32 y = 0; y < 16; ++y)
					for (int32 x = 0; x < 16; ++x) {
						const int32 d = (int32)src[(usize)(sy + y) * sw + (sx + x)] - (int32)pred[y * 16 + x];
						sad += d < 0 ? -d : d;
					}
				return sad;
			}

			// Encode une composante de MV (demi-pel) avec f_code (sign_extend + résidu), algo ISO 11172-2.
			void EncodeMotionComp(NkBitWriter &bw, int32 val, int32 fCode) {
				if (val == 0) {
					bw.PutBits(0x1, 1);
					return;
				}
				const int32 bitSize = fCode - 1;
				const int32 range = 1 << bitSize;
				const int32 n = 5 + bitSize;
				int32 v = val & ((1 << n) - 1);
				if (v & (1 << (n - 1)))
					v -= (1 << n);
				int32 code, bits, sign;
				if (v >= 0) {
					const int32 t = v - 1;
					code = (t >> bitSize) + 1;
					bits = t & (range - 1);
					sign = 0;
				} else {
					const int32 t = -v - 1;
					code = (t >> bitSize) + 1;
					bits = t & (range - 1);
					sign = 1;
				}
				const uint8(*mv)[2] = NkMpeg1Tables::MotionVlc();
				bw.PutBits(mv[code][0], mv[code][1]);
				bw.PutBits((uint32)sign, 1);
				if (bitSize > 0)
					bw.PutBits((uint32)bits, bitSize);
			}
		} // namespace

		void NkMpeg1Encoder::EncodePictureP(NkBitWriter &bw, int32 temporalRef) {
			bw.PutStartCode(0x00);
			bw.PutBits((uint32)(temporalRef & 0x3FF), 10);
			bw.PutBits(2, 3);		// P
			bw.PutBits(0xFFFF, 16); // vbv_delay
			bw.PutBits(0, 1);		// full_pel_forward_vector = 0 (half-pel)
			bw.PutBits(2, 3);		// forward_f_code = 2 (plage ±31 demi-pel)
			bw.PutBits(0, 1);		// extra_bit_picture
			const int32 kFCode = 2;

			const uint16(*acEob)[2] = NkMpeg1Tables::AcVlc();
			const uint8(*cbpVlc)[2] = NkMpeg1Tables::CbpVlc();
			const int32 SR = 15; // rayon de recherche full-pel

			for (int32 mbRow = 0; mbRow < mMbH; ++mbRow) {
				bw.PutStartCode((uint8)(mbRow + 1));
				bw.PutBits((uint32)mQScale, 5);
				bw.PutBits(0, 1);
				int32 pmvx = 0, pmvy = 0;			 // prédicteur MV (reset slice)
				int32 dcY = 128, dcCb = 128, dcCr = 128;
				for (int32 mbCol = 0; mbCol < mMbW; ++mbCol) {
					const int32 lx = mbCol * 16, ly = mbRow * 16, cx = mbCol * 8, cy = mbRow * 8;

					// --- estimation de mouvement (SAD + coût lagrangien du MV) ---
					// Le biais vers MV=0 évite les vecteurs parasites sur les fonds lisses
					// (un dégradé décalé « matche » par hasard) → sinon dérive/artefacts.
					const int32 lambda = 2 * mQScale;
					// Bornes entières : bloc 16×16 DANS la trame (MPEG-1 n'étend pas les bords).
					const int32 dxMin = (-SR > -lx) ? -SR : -lx;
					const int32 dxMax = (SR < mLumaW - 16 - lx) ? SR : mLumaW - 16 - lx;
					const int32 dyMin = (-SR > -ly) ? -SR : -ly;
					const int32 dyMax = (SR < mLumaH - 16 - ly) ? SR : mLumaH - 16 - ly;
					// 1) recherche entière (full-pel).
					int32 bestSad = Sad16(mY, mRefY, mLumaW, mLumaH, lx, ly, 0, 0);
					int32 bestCost = bestSad;
					int32 idx = 0, idy = 0;
					for (int32 dy = dyMin; dy <= dyMax; ++dy)
						for (int32 dx = dxMin; dx <= dxMax; ++dx) {
							if (dx == 0 && dy == 0)
								continue;
							const int32 sad = Sad16(mY, mRefY, mLumaW, mLumaH, lx, ly, dx, dy);
							const int32 cost = sad + lambda * ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
							if (cost < bestCost) {
								bestCost = cost;
								bestSad = sad;
								idx = dx;
								idy = dy;
							}
						}
					// 2) raffinement demi-pel autour du meilleur entier (MV final en demi-pel).
					int32 mvx = idx * 2, mvy = idy * 2; // demi-pel
					{
						const int32 baseHx = idx * 2, baseHy = idy * 2;
						int32 refCost = bestSad + lambda * ((idx < 0 ? -idx : idx) + (idy < 0 ? -idy : idy)) * 2;
						for (int32 hy = -1; hy <= 1; ++hy)
							for (int32 hx = -1; hx <= 1; ++hx) {
								if (hx == 0 && hy == 0)
									continue;
								const int32 mhx = baseHx + hx, mhy = baseHy + hy;
								// Le demi-pel lit +1 px : ne garder que les positions DANS la trame.
								const int32 ix = mhx >> 1, iy = mhy >> 1, fxp = mhx & 1, fyp = mhy & 1;
								if (lx + ix < 0 || ly + iy < 0 || lx + ix + 15 + fxp > mLumaW - 1 ||
									ly + iy + 15 + fyp > mLumaH - 1)
									continue;
								const int32 sad = Sad16Hp(mY, mLumaW, lx, ly, mRefY, mLumaW, mLumaH, mhx, mhy);
								const int32 cost =
									sad + lambda * ((mhx < 0 ? -mhx : mhx) + (mhy < 0 ? -mhy : mhy));
								if (cost < refCost) {
									refCost = cost;
									bestSad = sad;
									mvx = mhx;
									mvy = mhy;
								}
							}
					}

					// --- coût intra (SAD vs moyenne) pour décider intra/inter ---
					int32 mean = 0;
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x)
							mean += mY[(usize)(ly + y) * mLumaW + (lx + x)];
					mean /= 256;
					int32 intraSad = 0;
					for (int32 y = 0; y < 16; ++y)
						for (int32 x = 0; x < 16; ++x) {
							const int32 d = (int32)mY[(usize)(ly + y) * mLumaW + (lx + x)] - mean;
							intraSad += d < 0 ? -d : d;
						}

					const bool useIntra = (intraSad + 256 < bestSad);

					bw.PutBits(1, 1); // macroblock_address_increment = 1

					if (useIntra) {
						bw.PutBits(0x3, 5); // macroblock_type = Intra ('00011')
						EncodeIntraBlock(bw, mY, mLumaW, lx, ly, true, dcY, mRecY);
						EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly, true, dcY, mRecY);
						EncodeIntraBlock(bw, mY, mLumaW, lx, ly + 8, true, dcY, mRecY);
						EncodeIntraBlock(bw, mY, mLumaW, lx + 8, ly + 8, true, dcY, mRecY);
						EncodeIntraBlock(bw, mCb, mChromaW, cx, cy, false, dcCb, mRecCb);
						EncodeIntraBlock(bw, mCr, mChromaW, cx, cy, false, dcCr, mRecCr);
						pmvx = 0;
						pmvy = 0;
						continue;
					}

					// INTER : le prédicteur DC intra est réinitialisé à chaque macrobloc non-intra
					// (comme le fait le décodeur) → sinon un futur MB intra prédit son DC de travers.
					dcY = 128;
					dcCb = 128;
					dcCr = 128;

					// --- INTER : résidu, quantification non-intra, CBP ---
					// Chroma : MV demi-pel = MV luma demi-pel / 2 (chroma en demi-résolution).
					const int32 chmvx = mvx >> 1;
					const int32 chmvy = mvy >> 1;

					// Prépare les 6 blocs : (plane, ref, planeW/H, x0, y0, mvx, mvy, recPlane).
					struct BlkRef {
							const uint8 *src;
							const uint8 *ref;
							uint8 *rec;
							int32 w, h, x0, y0, mvx, mvy;
					};
					BlkRef blk[6] = {
						{mY, mRefY, mRecY, mLumaW, mLumaH, lx, ly, mvx, mvy},
						{mY, mRefY, mRecY, mLumaW, mLumaH, lx + 8, ly, mvx, mvy},
						{mY, mRefY, mRecY, mLumaW, mLumaH, lx, ly + 8, mvx, mvy},
						{mY, mRefY, mRecY, mLumaW, mLumaH, lx + 8, ly + 8, mvx, mvy},
						{mCb, mRefCb, mRecCb, mChromaW, mChromaH, cx, cy, chmvx, chmvy},
						{mCr, mRefCr, mRecCr, mChromaW, mChromaH, cx, cy, chmvx, chmvy},
					};

					int32 qcoef[6][64];
					float32 predAll[6][64];
					int32 cbp = 0;
					for (int32 bi = 0; bi < 6; ++bi) {
						float32 pred[64], resid[64], coef[64];
						InterpRegion(blk[bi].ref, blk[bi].w, blk[bi].h, blk[bi].x0, blk[bi].y0, blk[bi].mvx,
									 blk[bi].mvy, 8, 8, pred);
						for (int32 k = 0; k < 64; ++k) {
							const int32 py = blk[bi].y0 + k / 8, px = blk[bi].x0 + k % 8;
							resid[k] = (float32)blk[bi].src[(usize)py * blk[bi].w + px] - pred[k];
							predAll[bi][k] = pred[k];
						}
						Fdct8x8(resid, coef);
						bool nz = false;
						for (int32 k = 0; k < 64; ++k) {
							// quant non-intra (W=16, dead-zone par troncature) : level = coef/(2*q)
							const float32 v = coef[k] / (float32)(2 * mQScale);
							int32 lv = (int32)(v); // troncature vers zéro
							lv = Clamp(lv, -255, 255);
							qcoef[bi][k] = lv;
							if (lv != 0)
								nz = true;
						}
						if (nz)
							cbp |= (1 << (5 - bi));
					}

					// --- type de macrobloc ---
					const bool hasMv = (mvx != 0 || mvy != 0);
					if (hasMv && cbp) {
						bw.PutBits(1, 1); // MC, Coded
					} else if (!hasMv && cbp) {
						bw.PutBits(1, 2); // No-MC, Coded ('01')
					} else {			  // pas de résidu → MC not-coded ('001'), MV éventuellement nul
						bw.PutBits(1, 3);
					}

					// --- vecteur de mouvement (types avec motion_forward : MC Coded, MC not-coded) ---
					const bool codeMv = (hasMv && cbp) || (!cbp);
					if (codeMv) {
						EncodeMotionComp(bw, mvx - pmvx, kFCode);
						EncodeMotionComp(bw, mvy - pmvy, kFCode);
						pmvx = mvx;
						pmvy = mvy;
					} else {
						pmvx = 0;
						pmvy = 0; // No-MC : predicteur reset
					}

					// --- CBP + blocs codés (non-intra) ---
					if (cbp) {
						bw.PutBits(cbpVlc[cbp][0], cbpVlc[cbp][1]);
						for (int32 bi = 0; bi < 6; ++bi) {
							if (!(cbp & (1 << (5 - bi))))
								continue;
							// premier coefficient : cas spécial |level|==1 → '1s'
							int32 last = -1;
							for (int32 i = 63; i >= 0; --i)
								if (qcoef[bi][i] != 0) {
									last = i;
									break;
								}
							int32 i = 0;
							if (qcoef[bi][0] != 0 && (qcoef[bi][0] == 1 || qcoef[bi][0] == -1)) {
								bw.PutBits((qcoef[bi][0] < 0) ? 0x3u : 0x2u, 2); // '1'+sign
								i = 1;
							}
							int32 lastNz = i - 1;
							for (; i <= last; ++i) {
								const int32 lv = qcoef[bi][i];
								if (lv == 0)
									continue;
								EncodeAcCoef(bw, i - lastNz - 1, lv);
								lastNz = i;
							}
							bw.PutBits(acEob[NkMpeg1Tables::kAcEob][0], acEob[NkMpeg1Tables::kAcEob][1]);
						}
					}

					// --- reconstruction inter : pred + IDCT(dequant non-intra(residu)) ---
					for (int32 bi = 0; bi < 6; ++bi) {
						float32 dq[64], out[64];
						for (int32 k = 0; k < 64; ++k) {
							const int32 lv = qcoef[bi][k];
							int32 r = (lv == 0) ? 0 : ((2 * lv + Sign(lv)) * mQScale);
							r = Oddify(Clamp(r, -2048, 2047));
							dq[k] = (float32)r;
						}
						Idct8x8(dq, out);
						for (int32 y = 0; y < 8; ++y)
							for (int32 x = 0; x < 8; ++x) {
								const int32 py = blk[bi].y0 + y, px = blk[bi].x0 + x;
								const int32 val = (int32)::floor(predAll[bi][y * 8 + x] + out[y * 8 + x] + 0.5f);
								blk[bi].rec[(usize)py * blk[bi].w + px] = ClampU8(val);
							}
					}
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
			const bool isI = (mFrame % mGop) == 0;
			const int32 temporalRef = mFrame % mGop;
			if (temporalRef == 0)
				WriteGopHeader(bw);
			if (isI)
				EncodePictureI(bw, temporalRef);
			else
				EncodePictureP(bw, temporalRef);
			Flush(bw);

			// La reconstruction devient la référence de la trame suivante.
			uint8 *tY = mRefY, *tCb = mRefCb, *tCr = mRefCr;
			mRefY = mRecY;
			mRefCb = mRecCb;
			mRefCr = mRecCr;
			mRecY = tY;
			mRecCb = tCb;
			mRecCr = tCr;

			mFrame++;
			return true;
		}

		bool NkMpeg1Encoder::Close() {
			if (!mOpen)
				return false;
			NkBitWriter bw;
			bw.PutStartCode(0xB7);
			Flush(bw);
			mFile.Close();
			uint8 *bufs[9] = {mY, mCb, mCr, mRecY, mRecCb, mRecCr, mRefY, mRefCb, mRefCr};
			for (int32 i = 0; i < 9; ++i)
				if (bufs[i])
					memory::NkFree(bufs[i]);
			mY = mCb = mCr = mRecY = mRecCb = mRecCr = mRefY = mRefCb = mRefCr = nullptr;
			mOpen = false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
