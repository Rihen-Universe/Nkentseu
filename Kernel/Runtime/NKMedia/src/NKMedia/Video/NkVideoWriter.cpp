// =============================================================================
// NKMedia/Video/NkVideoWriter.cpp — création vidéo from-scratch (AVI: brut + MJPEG).
// =============================================================================
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Récupère (r,g,b) d'un pixel d'entrée selon le format.
			inline void FetchRGB(const uint8 *p, NkVideoInputFormat fmt, uint8 &r, uint8 &g, uint8 &b) {
				switch (fmt) {
					case NkVideoInputFormat::RGB24:
						r = p[0];
						g = p[1];
						b = p[2];
						break;
					case NkVideoInputFormat::RGBA32:
						r = p[0];
						g = p[1];
						b = p[2];
						break;
					case NkVideoInputFormat::BGR24:
						b = p[0];
						g = p[1];
						r = p[2];
						break;
					default:
						r = g = b = 0;
						break;
				}
			}

			inline int32 InputBpp(NkVideoInputFormat fmt) {
				return (fmt == NkVideoInputFormat::RGBA32) ? 4 : 3;
			}
		} // namespace

		bool NkVideoWriter::EnsureScratch(usize n) {
			if (mScratchCap >= n && mScratch)
				return true;
			if (mScratch)
				memory::NkFree(mScratch);
			mScratch = (uint8 *)memory::NkAlloc(n);
			mScratchCap = mScratch ? n : 0;
			return mScratch != nullptr;
		}

		bool NkVideoWriter::Open(const char *path, const NkVideoConfig &cfg) {
			if (mOpen || cfg.width <= 0 || cfg.height <= 0)
				return false;
			mCfg = cfg;

			if (cfg.codec == NkVideoCodec::MPEG1) {
				// MPEG-1 : flux élémentaire (.m1v). `quality` 1..100 → qscale 1..31 (inversé).
				int32 qscale = 32 - (cfg.quality * 31) / 100;
				if (qscale < 1)
					qscale = 1;
				if (qscale > 31)
					qscale = 31;
				if (!mMpeg1.Open(path, cfg.width, cfg.height, cfg.fpsNum, cfg.fpsDen, qscale))
					return false;
				mOpen = true;
				return true;
			}

			if (cfg.container == NkVideoContainer::MOV) {
				// MOV/MP4 : MJPEG uniquement (pas de RAW dans ce conteneur pour l'instant).
				if (cfg.codec != NkVideoCodec::MJPEG)
					return false;
				if (!mMov.Open(path, cfg.width, cfg.height, cfg.fpsNum, cfg.fpsDen))
					return false;
				mOpen = true;
				return true;
			}

			// --- AVI ---
			uint32 fourcc = 0, compression = 0;
			int32 bitCount = 24;
			if (cfg.codec == NkVideoCodec::MJPEG) {
				fourcc = NkAviCompressionMJPG();
				compression = NkAviCompressionMJPG();
			} else { // RAW_BGR
				fourcc = 0;
				compression = kAviCompressionRGB;
			}
			if (!mAvi.Open(path, cfg.width, cfg.height, cfg.fpsNum, cfg.fpsDen, fourcc, compression, bitCount))
				return false;
			mOpen = true;
			return true;
		}

		bool NkVideoWriter::WriteEncoded(const uint8 *data, uint32 size, uint32 /*chunkKind*/) {
			if (mCfg.container == NkVideoContainer::MOV)
				return mMov.WriteFrame(data, size, true);
			return mAvi.WriteFrame(data, size, true);
		}

		bool NkVideoWriter::WriteFrameRaw(const uint8 *pixels, NkVideoInputFormat fmt) {
			const int32 w = mCfg.width, h = mCfg.height;
			const int32 ibpp = InputBpp(fmt);
			// DIB 24 bits : lignes alignées sur 4 octets, ordre BGR, bas-en-haut.
			const int32 rowBytes = w * 3;
			const int32 padRow = (rowBytes + 3) & ~3;
			const usize frameSize = (usize)padRow * (usize)h;
			if (!EnsureScratch(frameSize))
				return false;

			for (int32 y = 0; y < h; ++y) {
				const int32 srcY = h - 1 - y; // bas-en-haut
				const uint8 *src = pixels + (usize)srcY * w * ibpp;
				uint8 *dst = mScratch + (usize)y * padRow;
				for (int32 x = 0; x < w; ++x) {
					uint8 r, g, b;
					FetchRGB(src + x * ibpp, fmt, r, g, b);
					dst[x * 3 + 0] = b;
					dst[x * 3 + 1] = g;
					dst[x * 3 + 2] = r;
				}
				for (int32 p = rowBytes; p < padRow; ++p)
					dst[p] = 0;
			}
			return WriteEncoded(mScratch, (uint32)frameSize, 0);
		}

		bool NkVideoWriter::WriteFrameMjpeg(const uint8 *pixels, NkVideoInputFormat fmt) {
			const int32 w = mCfg.width, h = mCfg.height;
			const int32 ibpp = InputBpp(fmt);

			NkImage *img = NkImage::Alloc(w, h, NkImagePixelFormat::NK_RGB24);
			if (!img)
				return false;
			uint8 *dstBase = img->Pixels();
			const int32 stride = img->Stride();
			for (int32 y = 0; y < h; ++y) {
				const uint8 *src = pixels + (usize)y * w * ibpp;
				uint8 *dst = dstBase + (usize)y * stride;
				for (int32 x = 0; x < w; ++x) {
					uint8 r, g, b;
					FetchRGB(src + x * ibpp, fmt, r, g, b);
					dst[x * 3 + 0] = r;
					dst[x * 3 + 1] = g;
					dst[x * 3 + 2] = b;
				}
			}

			uint8 *jpg = nullptr;
			usize jsz = 0;
			const bool okEnc = img->EncodeJPEG(jpg, jsz, mCfg.quality);
			img->Free();
			if (!okEnc || !jpg || jsz == 0) {
				if (jpg)
					memory::NkFree(jpg);
				return false;
			}
			const bool okWrite = WriteEncoded(jpg, (uint32)jsz, 0);
			memory::NkFree(jpg);
			return okWrite;
		}

		bool NkVideoWriter::WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt) {
			if (!mOpen || pixels == nullptr)
				return false;
			if (mCfg.codec == NkVideoCodec::MPEG1)
				return mMpeg1.WriteFrame(pixels, fmt);
			if (mCfg.codec == NkVideoCodec::MJPEG)
				return WriteFrameMjpeg(pixels, fmt);
			return WriteFrameRaw(pixels, fmt);
		}

		bool NkVideoWriter::Close() {
			if (!mOpen)
				return false;
			bool ok;
			if (mCfg.codec == NkVideoCodec::MPEG1)
				ok = mMpeg1.Close();
			else
				ok = (mCfg.container == NkVideoContainer::MOV) ? mMov.Close() : mAvi.Close();
			if (mScratch) {
				memory::NkFree(mScratch);
				mScratch = nullptr;
				mScratchCap = 0;
			}
			mOpen = false;
			return ok;
		}

	} // namespace media
} // namespace nkentseu
