// =============================================================================
// NKMedia/Video/Containers/NkMovWriter.h
// -----------------------------------------------------------------------------
// Muxer conteneur QuickTime/MP4 (ISOBMFF) from-scratch — écrit un .mov/.mp4 avec
// une piste vidéo MJPEG (codec 'jpeg', chaque échantillon = une image JPEG). Gère
// ftyp + mdat (données) + moov (tables stbl : stsd/stts/stsc/stsz/stco). C'est la
// même famille de conteneur que MP4 (boxes big-endian) — lisible par VLC, QuickTime,
// navigateurs, éditeurs. Complément du démux ISOBMFF déjà présent (NkMediaDemux).
//
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkMovWriter {
			public:
				// Ouvre `path` (.mov/.mp4). Piste vidéo MJPEG. fps = num/den.
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen);

				// Active une piste AUDIO PCM s16 little-endian (sample entry 'sowt'). À appeler
				// avant le premier WriteAudio (Open ou après — la piste part dans moov à Close).
				void SetAudio(int32 sampleRate, int32 channels);

				// Écrit une image JPEG déjà encodée (`data`/`size`) comme un échantillon vidéo.
				bool WriteFrame(const uint8 *data, uint32 size, bool keyframe);

				// Écrit `frames` trames PCM s16 entrelacées comme UN chunk audio dans mdat
				// (entrelacé avec les WriteFrame : ordre du fichier = ordre d'appel). Requiert SetAudio.
				bool WriteAudio(const int16 *interleaved, uint32 frames);

				// Écrit moov + tables et ferme.
				bool Close();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				int32 FrameCount() const {
					return (int32)mSizes.Size();
				}

			private:
				NkFile mFile;
				NkVector<uint32> mSizes;   // taille de chaque échantillon (JPEG)
				NkVector<uint32> mOffsets; // offset absolu de chaque échantillon dans le fichier
				int32 mWidth = 0, mHeight = 0, mFpsNum = 30, mFpsDen = 1;
				nk_int64 mMdatSizePos = 0;
				nk_int64 mMdatStart = 0;

				// Piste audio PCM 'sowt' optionnelle (chunks entrelacés dans mdat).
				int32 mAudioRate = 0; // 0 = pas d'audio
				int32 mAudioChannels = 0;
				NkVector<uint32> mAudioChunkOff;	// offset absolu de chaque chunk audio
				NkVector<uint32> mAudioChunkFrames; // trames PCM par chunk
				uint64 mAudioFrames = 0;			// total de trames PCM
		};

	} // namespace media
} // namespace nkentseu
