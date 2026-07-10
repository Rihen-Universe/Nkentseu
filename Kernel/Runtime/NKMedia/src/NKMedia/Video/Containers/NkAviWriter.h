// =============================================================================
// NKMedia/Video/Containers/NkAviWriter.h
// -----------------------------------------------------------------------------
// Muxer conteneur AVI (RIFF) — écrit un fichier .avi from-scratch (sans ffmpeg).
// Accepte des trames vidéo DÉJÀ encodées (données brutes BGR DIB, ou JPEG pour
// MJPEG, ou autre codec) + le FourCC du codec. Gère l'en-tête RIFF/hdrl/strl, la
// liste `movi`, l'index `idx1`, et le rapiéçage des tailles à la fermeture.
//
// Réf : spec AVI/RIFF (Microsoft) + OpenDML. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		// Un « FourCC » = 4 octets ASCII empaquetés en little-endian ('M','J','P','G' → 'GPJM').
		constexpr uint32 NkFourCC(char a, char b, char c, char d) {
			return (uint32)(uint8)a | ((uint32)(uint8)b << 8) | ((uint32)(uint8)c << 16) | ((uint32)(uint8)d << 24);
		}

		// biCompression courants.
		constexpr uint32 kAviCompressionRGB = 0;						 // BI_RGB (BGR brut, bas-en-haut)
		inline uint32 NkAviCompressionMJPG() { return NkFourCC('M', 'J', 'P', 'G'); }

		struct NkAviWriter {
			public:
				// Ouvre `path` et écrit l'en-tête. `fourccHandler`/`biCompression` = codec (0 = brut DIB,
				// 'MJPG' = MJPEG). `bitCount` = bits par pixel (24 pour BGR/MJPEG). fps = num/den.
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
						  uint32 fourccHandler, uint32 biCompression, int32 bitCount);

				// Écrit une trame déjà encodée (`data`/`size`). `keyframe` = image-clé (toutes le sont en
				// MJPEG/brut). `chunkId` = '00dc' (compressé) ou '00db' (brut DIB).
				bool WriteFrame(const uint8 *data, uint32 size, bool keyframe);

				// Termine : écrit l'index `idx1` et rapièce toutes les tailles. Ferme le fichier.
				bool Close();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				int32 FrameCount() const {
					return mFrameCount;
				}

			private:
				struct IndexEntry {
						uint32 ckid;
						uint32 flags;
						uint32 offset; // relatif au FourCC 'movi'
						uint32 length;
				};

				NkFile mFile;
				NkVector<IndexEntry> mIndex;
				int32 mWidth = 0, mHeight = 0;
				int32 mFrameCount = 0;
				uint32 mMaxChunk = 0;
				uint32 mChunkId = 0;

				// Positions à rapiécer.
				nk_int64 mRiffSizePos = 0;
				nk_int64 mAvihTotalFramesPos = 0;
				nk_int64 mAvihBufSizePos = 0;
				nk_int64 mStrhLengthPos = 0;
				nk_int64 mStrhBufSizePos = 0;
				nk_int64 mMoviListSizePos = 0;
				nk_int64 mMoviBasePos = 0; // position du FourCC 'movi'

				void PutU32(uint32 v);
				void PutU16(uint16 v);
				void PutBytes(const void *p, usize n);
				void PatchU32(nk_int64 pos, uint32 v);
		};

	} // namespace media
} // namespace nkentseu
