// =============================================================================
// NKMedia/Video/Containers/NkAviWriter.cpp — muxer AVI/RIFF from-scratch.
// =============================================================================
#include "NKMedia/Video/Containers/NkAviWriter.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr uint32 kAvifHasIndex = 0x00000010;   // AVIF_HASINDEX
			constexpr uint32 kAviifKeyframe = 0x00000010;  // AVIIF_KEYFRAME
		} // namespace

		void NkAviWriter::PutU32(uint32 v) {
			uint8 b[4] = {(uint8)(v & 0xFF), (uint8)((v >> 8) & 0xFF), (uint8)((v >> 16) & 0xFF),
						  (uint8)((v >> 24) & 0xFF)};
			mFile.Write(b, 4);
		}

		void NkAviWriter::PutU16(uint16 v) {
			uint8 b[2] = {(uint8)(v & 0xFF), (uint8)((v >> 8) & 0xFF)};
			mFile.Write(b, 2);
		}

		void NkAviWriter::PutBytes(const void *p, usize n) {
			mFile.Write(p, n);
		}

		void NkAviWriter::PatchU32(nk_int64 pos, uint32 v) {
			const nk_int64 cur = mFile.Tell();
			mFile.Seek(pos, NkSeekOrigin::NK_BEGIN);
			PutU32(v);
			mFile.Seek(cur, NkSeekOrigin::NK_BEGIN);
		}

		bool NkAviWriter::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
							   uint32 fourccHandler, uint32 biCompression, int32 bitCount) {
			if (width <= 0 || height <= 0 || fpsDen <= 0)
				return false;
			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode))
				return false;

			mWidth = width;
			mHeight = height;
			mFrameCount = 0;
			mMaxChunk = 0;
			mIndex.Clear();
			mChunkId = (biCompression == kAviCompressionRGB) ? NkFourCC('0', '0', 'd', 'b')
															 : NkFourCC('0', '0', 'd', 'c');

			const uint32 microSecPerFrame = (uint32)((1000000ull * (uint64)fpsDen) / (uint64)fpsNum);

			// ---- RIFF 'AVI ' ----
			PutU32(NkFourCC('R', 'I', 'F', 'F'));
			mRiffSizePos = mFile.Tell();
			PutU32(0); // taille RIFF (rapiécée)
			PutU32(NkFourCC('A', 'V', 'I', ' '));

			// ---- LIST 'hdrl' ----
			PutU32(NkFourCC('L', 'I', 'S', 'T'));
			const nk_int64 hdrlSizePos = mFile.Tell();
			PutU32(0);
			const nk_int64 hdrlStart = mFile.Tell();
			PutU32(NkFourCC('h', 'd', 'r', 'l'));

			// avih (56 octets de données).
			PutU32(NkFourCC('a', 'v', 'i', 'h'));
			PutU32(56);
			PutU32(microSecPerFrame);		 // dwMicroSecPerFrame
			mAvihBufSizePos = mFile.Tell();	 // dwMaxBytesPerSec (rapiécé approx)
			PutU32(0);
			PutU32(0); // dwPaddingGranularity
			PutU32(kAvifHasIndex);			 // dwFlags
			mAvihTotalFramesPos = mFile.Tell();
			PutU32(0);						 // dwTotalFrames (rapiécé)
			PutU32(0);						 // dwInitialFrames
			PutU32(1);						 // dwStreams
			PutU32(0);						 // dwSuggestedBufferSize (rapiécé plus bas via BufSize)
			PutU32((uint32)width);			 // dwWidth
			PutU32((uint32)height);			 // dwHeight
			PutU32(0);
			PutU32(0);
			PutU32(0);
			PutU32(0); // dwReserved[4]

			// ---- LIST 'strl' ----
			PutU32(NkFourCC('L', 'I', 'S', 'T'));
			const nk_int64 strlSizePos = mFile.Tell();
			PutU32(0);
			const nk_int64 strlStart = mFile.Tell();
			PutU32(NkFourCC('s', 't', 'r', 'l'));

			// strh (56 octets).
			PutU32(NkFourCC('s', 't', 'r', 'h'));
			PutU32(56);
			PutU32(NkFourCC('v', 'i', 'd', 's')); // fccType
			PutU32(fourccHandler);				  // fccHandler
			PutU32(0);							  // dwFlags
			PutU16(0);							  // wPriority
			PutU16(0);							  // wLanguage
			PutU32(0);							  // dwInitialFrames
			PutU32((uint32)fpsDen);				  // dwScale
			PutU32((uint32)fpsNum);				  // dwRate → fps = Rate/Scale
			PutU32(0);							  // dwStart
			mStrhLengthPos = mFile.Tell();
			PutU32(0);							  // dwLength (rapiécé = frames)
			mStrhBufSizePos = mFile.Tell();
			PutU32(0);							  // dwSuggestedBufferSize (rapiécé)
			PutU32(0xFFFFFFFFu);				  // dwQuality (-1 = défaut)
			PutU32(0);							  // dwSampleSize
			PutU16(0);
			PutU16(0);
			PutU16((uint16)width);
			PutU16((uint16)height); // rcFrame

			// strf = BITMAPINFOHEADER (40 octets).
			PutU32(NkFourCC('s', 't', 'r', 'f'));
			PutU32(40);
			PutU32(40);						  // biSize
			PutU32((uint32)width);			  // biWidth
			PutU32((uint32)height);			  // biHeight (>0 = bas-en-haut pour BGR ; MJPEG l'ignore)
			PutU16(1);						  // biPlanes
			PutU16((uint16)bitCount);		  // biBitCount
			PutU32(biCompression);			  // biCompression
			PutU32((uint32)(width * height * (bitCount / 8))); // biSizeImage
			PutU32(0);
			PutU32(0);
			PutU32(0);
			PutU32(0); // pels/clr

			// Rapièce les tailles hdrl/strl (connues maintenant).
			const nk_int64 afterStrl = mFile.Tell();
			PatchU32(strlSizePos, (uint32)(afterStrl - strlStart));
			PatchU32(hdrlSizePos, (uint32)(afterStrl - hdrlStart));

			// ---- LIST 'movi' ----
			PutU32(NkFourCC('L', 'I', 'S', 'T'));
			mMoviListSizePos = mFile.Tell();
			PutU32(0);
			mMoviBasePos = mFile.Tell(); // position du FourCC 'movi'
			PutU32(NkFourCC('m', 'o', 'v', 'i'));

			return mFile.IsOpen();
		}

		bool NkAviWriter::WriteFrame(const uint8 *data, uint32 size, bool keyframe) {
			if (!mFile.IsOpen() || data == nullptr)
				return false;

			const nk_int64 ckidPos = mFile.Tell();
			PutU32(mChunkId);
			PutU32(size);
			PutBytes(data, size);
			// Alignement pair (padding requis par RIFF).
			if (size & 1) {
				const uint8 pad = 0;
				mFile.Write(&pad, 1);
			}

			IndexEntry e;
			e.ckid = mChunkId;
			e.flags = keyframe ? kAviifKeyframe : 0;
			e.offset = (uint32)(ckidPos - mMoviBasePos);
			e.length = size;
			mIndex.PushBack(e);

			if (size > mMaxChunk)
				mMaxChunk = size;
			mFrameCount++;
			return true;
		}

		bool NkAviWriter::Close() {
			if (!mFile.IsOpen())
				return false;

			const nk_int64 moviEnd = mFile.Tell();
			// Taille de la LIST 'movi' = tout depuis le FourCC 'movi' jusqu'ici.
			PatchU32(mMoviListSizePos, (uint32)(moviEnd - mMoviBasePos));

			// ---- idx1 ----
			PutU32(NkFourCC('i', 'd', 'x', '1'));
			PutU32((uint32)(mIndex.Size() * 16));
			for (uint64 i = 0; i < mIndex.Size(); ++i) {
				const IndexEntry &e = mIndex[i];
				PutU32(e.ckid);
				PutU32(e.flags);
				PutU32(e.offset);
				PutU32(e.length);
			}

			const nk_int64 fileEnd = mFile.Tell();

			// Rapiéçage final.
			PatchU32(mRiffSizePos, (uint32)(fileEnd - (mRiffSizePos + 4)));
			PatchU32(mAvihTotalFramesPos, (uint32)mFrameCount);
			PatchU32(mStrhLengthPos, (uint32)mFrameCount);
			PatchU32(mAvihBufSizePos, mMaxChunk);
			PatchU32(mStrhBufSizePos, mMaxChunk);

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
