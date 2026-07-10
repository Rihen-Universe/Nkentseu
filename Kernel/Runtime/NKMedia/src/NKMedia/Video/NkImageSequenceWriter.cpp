// =============================================================================
// NKMedia/Video/NkImageSequenceWriter.cpp — export séquence d'images (PNG/JPEG/…).
// =============================================================================
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"

#include <cstdio> // snprintf

namespace nkentseu {
	namespace media {

		namespace {
			void CopyStr(char *dst, usize cap, const char *src) {
				usize i = 0;
				if (src)
					for (; src[i] && i + 1 < cap; ++i)
						dst[i] = src[i];
				dst[i] = 0;
			}
			inline int32 InBpp(NkVideoInputFormat f) {
				return (f == NkVideoInputFormat::RGBA32) ? 4 : 3;
			}
		} // namespace

		bool NkImageSequenceWriter::Open(const char *dir, const char *basename, int32 width, int32 height,
										 NkImageSeqFormat fmt, int32 padding, int32 quality) {
			if (width <= 0 || height <= 0)
				return false;
			CopyStr(mDir, sizeof(mDir), dir ? dir : ".");
			CopyStr(mBase, sizeof(mBase), basename ? basename : "frame");
			mWidth = width;
			mHeight = height;
			mFmt = fmt;
			mPadding = (padding < 1) ? 1 : (padding > 9 ? 9 : padding);
			mQuality = quality;
			mFrame = 0;
			switch (fmt) {
				case NkImageSeqFormat::PNG:
					mExt = "png";
					break;
				case NkImageSeqFormat::JPEG:
					mExt = "jpg";
					break;
				case NkImageSeqFormat::BMP:
					mExt = "bmp";
					break;
				case NkImageSeqFormat::TGA:
					mExt = "tga";
					break;
				case NkImageSeqFormat::QOI:
					mExt = "qoi";
					break;
			}
			mOpen = true;
			return true;
		}

		bool NkImageSequenceWriter::WriteFrame(const uint8 *pixels, NkVideoInputFormat inFmt) {
			if (!mOpen || pixels == nullptr)
				return false;
			const int32 w = mWidth, h = mHeight, ibpp = InBpp(inFmt);

			// Construit une NkImage (RGB24 ou RGBA32 selon l'entrée) à partir des pixels (haut-en-bas).
			const bool hasAlpha = (inFmt == NkVideoInputFormat::RGBA32);
			NkImage *img = NkImage::Alloc(w, h, hasAlpha ? NkImagePixelFormat::NK_RGBA32 : NkImagePixelFormat::NK_RGB24);
			if (!img)
				return false;
			uint8 *base = img->Pixels();
			const int32 stride = img->Stride();
			const int32 obpp = hasAlpha ? 4 : 3;
			for (int32 y = 0; y < h; ++y) {
				const uint8 *src = pixels + (usize)y * w * ibpp;
				uint8 *dst = base + (usize)y * stride;
				for (int32 x = 0; x < w; ++x) {
					const uint8 *s = src + x * ibpp;
					uint8 r, g, b;
					if (inFmt == NkVideoInputFormat::BGR24) {
						b = s[0];
						g = s[1];
						r = s[2];
					} else {
						r = s[0];
						g = s[1];
						b = s[2];
					}
					uint8 *d = dst + x * obpp;
					d[0] = r;
					d[1] = g;
					d[2] = b;
					if (hasAlpha)
						d[3] = (ibpp == 4) ? s[3] : 255;
				}
			}

			// Encode selon le format.
			uint8 *out = nullptr;
			usize outSize = 0;
			bool okEnc = false;
			switch (mFmt) {
				case NkImageSeqFormat::PNG:
					okEnc = img->EncodePNG(out, outSize);
					break;
				case NkImageSeqFormat::JPEG:
					okEnc = img->EncodeJPEG(out, outSize, mQuality);
					break;
				case NkImageSeqFormat::BMP:
					okEnc = img->EncodeBMP(out, outSize);
					break;
				case NkImageSeqFormat::TGA:
					okEnc = img->EncodeTGA(out, outSize);
					break;
				case NkImageSeqFormat::QOI:
					okEnc = img->EncodeQOI(out, outSize);
					break;
			}
			img->Free();
			if (!okEnc || !out || outSize == 0) {
				if (out)
					memory::NkFree(out);
				return false;
			}

			// Nom de fichier numéroté : dir/base_0001.ext
			char path[1024];
			::snprintf(path, sizeof(path), "%s/%s_%0*d.%s", mDir, mBase, mPadding, mFrame + 1, mExt);

			NkFile f;
			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			bool okWrite = false;
			if (f.Open(path, (NkFileMode)mode)) {
				okWrite = (f.Write(out, outSize) == outSize);
				f.Close();
			}
			memory::NkFree(out);
			if (okWrite)
				mFrame++;
			return okWrite;
		}

	} // namespace media
} // namespace nkentseu
