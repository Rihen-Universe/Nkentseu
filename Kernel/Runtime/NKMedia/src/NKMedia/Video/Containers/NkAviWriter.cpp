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

		void NkAviWriter::SetAudio(int32 sampleRate, int32 channels) {
			if (sampleRate > 0 && channels > 0) {
				mAudioRate = sampleRate;
				mAudioChannels = channels;
			} else {
				mAudioRate = 0;
				mAudioChannels = 0;
			}
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
			const bool hasAudio = (mAudioRate > 0 && mAudioChannels > 0);
			mAudioChunkId = NkFourCC('0', '1', 'w', 'b');
			mAudioBytes = 0;
			mAudioMaxChunk = 0;
			mAudioChunkCount = 0;
			mAudioStrhLengthPos = 0;
			mAudioStrhBufSizePos = 0;

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
			PutU32(hasAudio ? 2u : 1u);		 // dwStreams
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

			// Rapièce la taille du strl vidéo (connue maintenant).
			const nk_int64 afterStrl = mFile.Tell();
			PatchU32(strlSizePos, (uint32)(afterStrl - strlStart));

			// ---- LIST 'strl' audio (PCM s16) — flux 1, chunks '01wb' ----
			if (hasAudio) {
				const uint32 blockAlign = (uint32)(mAudioChannels * 2); // s16
				const uint32 avgBytesPerSec = (uint32)mAudioRate * blockAlign;

				PutU32(NkFourCC('L', 'I', 'S', 'T'));
				const nk_int64 aStrlSizePos = mFile.Tell();
				PutU32(0);
				const nk_int64 aStrlStart = mFile.Tell();
				PutU32(NkFourCC('s', 't', 'r', 'l'));

				// strh (56 octets). Convention PCM (comme ffmpeg) : Rate/Scale = octets/s sur
				// blockAlign → Rate = avgBytesPerSec, Scale = blockAlign ; dwSampleSize = blockAlign.
				PutU32(NkFourCC('s', 't', 'r', 'h'));
				PutU32(56);
				PutU32(NkFourCC('a', 'u', 'd', 's')); // fccType
				PutU32(0);							  // fccHandler
				PutU32(0);							  // dwFlags
				PutU16(0);							  // wPriority
				PutU16(0);							  // wLanguage
				PutU32(0);							  // dwInitialFrames
				PutU32(blockAlign);					  // dwScale
				PutU32(avgBytesPerSec);				  // dwRate → Rate/Scale = échantillons/s
				PutU32(0);							  // dwStart
				mAudioStrhLengthPos = mFile.Tell();
				PutU32(0);							  // dwLength (rapiécé = blocs PCM)
				mAudioStrhBufSizePos = mFile.Tell();
				PutU32(0);							  // dwSuggestedBufferSize (rapiécé)
				PutU32(0xFFFFFFFFu);				  // dwQuality (-1 = défaut)
				PutU32(blockAlign);					  // dwSampleSize (PCM = taille d'un bloc)
				PutU16(0);
				PutU16(0);
				PutU16(0);
				PutU16(0); // rcFrame (inutilisé pour l'audio)

				// strf = WAVEFORMAT PCM (16 octets).
				PutU32(NkFourCC('s', 't', 'r', 'f'));
				PutU32(16);
				PutU16(1);						// wFormatTag = WAVE_FORMAT_PCM
				PutU16((uint16)mAudioChannels); // nChannels
				PutU32((uint32)mAudioRate);		// nSamplesPerSec
				PutU32(avgBytesPerSec);			// nAvgBytesPerSec
				PutU16((uint16)blockAlign);		// nBlockAlign
				PutU16(16);						// wBitsPerSample

				const nk_int64 afterAStrl = mFile.Tell();
				PatchU32(aStrlSizePos, (uint32)(afterAStrl - aStrlStart));
			}

			// Rapièce la taille de hdrl (couvre avih + tous les strl).
			const nk_int64 afterHdrl = mFile.Tell();
			PatchU32(hdrlSizePos, (uint32)(afterHdrl - hdrlStart));

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

		bool NkAviWriter::WriteAudio(const uint8 *data, uint32 size) {
			if (!mFile.IsOpen() || data == nullptr || size == 0)
				return false;
			if (mAudioRate <= 0 || mAudioChannels <= 0)
				return false; // SetAudio non appelé avant Open

			const nk_int64 ckidPos = mFile.Tell();
			PutU32(mAudioChunkId);
			PutU32(size);
			PutBytes(data, size);
			// Alignement pair (padding requis par RIFF).
			if (size & 1) {
				const uint8 pad = 0;
				mFile.Write(&pad, 1);
			}

			IndexEntry e;
			e.ckid = mAudioChunkId;
			e.flags = kAviifKeyframe; // les chunks PCM sont tous « clés »
			e.offset = (uint32)(ckidPos - mMoviBasePos);
			e.length = size;
			mIndex.PushBack(e);

			mAudioBytes += size;
			if (size > mAudioMaxChunk)
				mAudioMaxChunk = size;
			mAudioChunkCount++;
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
			if (mAudioStrhLengthPos != 0) {
				const uint32 blockAlign = (uint32)(mAudioChannels * 2);
				PatchU32(mAudioStrhLengthPos, (uint32)(mAudioBytes / blockAlign)); // blocs PCM
				PatchU32(mAudioStrhBufSizePos, mAudioMaxChunk);
			}

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
