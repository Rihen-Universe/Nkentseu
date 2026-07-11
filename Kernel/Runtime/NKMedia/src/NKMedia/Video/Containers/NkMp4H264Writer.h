// =============================================================================
// NKMedia/Video/Containers/NkMp4H264Writer.h
// -----------------------------------------------------------------------------
// Muxer MP4 (ISOBMFF) from-scratch pour une piste vidéo H.264 (AVC). Écrit ftyp
// (isom/avc1) + mdat (échantillons = NAL longueur-préfixées 4 octets) + moov avec
// l'entrée d'échantillon 'avc1' + la box 'avcC' (AVCDecoderConfigurationRecord :
// SPS/PPS) + tables stbl (stsd/stts/stsc/stsz/stco/stss). Produit un `.mp4` cliquable
// lisible par VLC, QuickTime, navigateurs, éditeurs. Boxes big-endian. Zero-STL.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkMp4H264Writer {
			public:
				// Ouvre `path` (.mp4/.mov). Piste vidéo H.264 (avc1). fps = num/den.
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen);

				// Fixe SPS/PPS (octets du NAL, en-tête compris, SANS start code) pour la box avcC.
				void SetSps(const uint8 *data, uint32 size);
				void SetPps(const uint8 *data, uint32 size);

				// Écrit un échantillon (données = NAL VCL longueur-préfixées 4 octets big-endian).
				// `sync` = image clé (IDR) → listée dans stss.
				bool WriteSample(const uint8 *data, uint32 size, bool sync);

				bool Close();

				bool IsOpen() const {
					return mFile.IsOpen();
				}
				int32 FrameCount() const {
					return (int32)mSizes.Size();
				}
				bool HasParameterSets() const {
					return mSps.Size() > 0 && mPps.Size() > 0;
				}

			private:
				NkFile mFile;
				NkVector<uint32> mSizes;   // taille de chaque échantillon
				NkVector<uint32> mOffsets; // offset absolu de chaque échantillon
				NkVector<uint8> mSync;	   // 1 = échantillon synchro (IDR)
				NkVector<uint8> mSps, mPps;
				int32 mWidth = 0, mHeight = 0, mFpsNum = 30, mFpsDen = 1;
				nk_int64 mMdatSizePos = 0;
				nk_int64 mMdatStart = 0;
		};

	} // namespace media
} // namespace nkentseu
