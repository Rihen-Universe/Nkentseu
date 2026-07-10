// =============================================================================
// NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.h
// -----------------------------------------------------------------------------
// Encodeur MPEG-1 Video (ISO/IEC 11172-2) FROM-SCRATCH (sans ffmpeg) — VRAI codec
// vidéo compressé DCT. Étape 1 : images INTRA (I-pictures) → flux élémentaire MPEG-1
// (.m1v) valide, lisible par ffmpeg/VLC/lecteurs standard. Pipeline : RGB → YCbCr
// 4:2:0 → DCT 8×8 → quantification intra → balayage zig-zag → VLC (run/level) →
// bitstream. Étape 2 (à venir) : P-pictures (estimation/compensation de mouvement)
// pour la vraie compression inter-frame.
//
// Algorithmes réécrits à la sauce Nkentseu (tables du format uniquement réutilisées).
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Video/NkBitWriter.h"
#include "NKMedia/Video/NkVideoWriter.h" // NkVideoInputFormat
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkMpeg1Encoder {
			public:
				// Ouvre `path` (.m1v/.mpg élémentaire). `qscale` 1..31 (petit = meilleure qualité).
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen, int32 qscale = 8);

				// Encode une trame (pixels haut-en-bas) comme I-picture et l'écrit.
				bool WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt);

				// Écrit le sequence_end_code et ferme.
				bool Close();

				bool IsOpen() const {
					return mOpen;
				}
				int32 FrameCount() const {
					return mFrame;
				}

			private:
				NkFile mFile;
				int32 mWidth = 0, mHeight = 0;	 // dimensions réelles
				int32 mMbW = 0, mMbH = 0;		 // en macroblocs (padding multiple de 16)
				int32 mFpsNum = 25, mFpsDen = 1;
				int32 mQScale = 8;
				int32 mFrame = 0;
				bool mOpen = false;
				bool mWroteSeq = false;

				// Plans YCbCr (padded aux multiples de 16 / 8 pour la chroma).
				uint8 *mY = nullptr;   // mMbW*16 × mMbH*16
				uint8 *mCb = nullptr;  // moitié
				uint8 *mCr = nullptr;
				int32 mLumaW = 0, mLumaH = 0, mChromaW = 0, mChromaH = 0;

				void WriteSequenceHeader(NkBitWriter &bw);
				void WriteGopHeader(NkBitWriter &bw);
				void EncodePicture(NkBitWriter &bw, int32 temporalRef);
				void EncodeIntraBlock(NkBitWriter &bw, const uint8 *plane, int32 planeW, int32 x0, int32 y0,
									  bool luma, int32 &dcPred);
				void ConvertToYuv(const uint8 *pixels, NkVideoInputFormat fmt);
				void Flush(NkBitWriter &bw);
		};

	} // namespace media
} // namespace nkentseu
