// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Encoder.h
// -----------------------------------------------------------------------------
// Encodeur H.264 baseline (ISO/IEC 14496-10) FROM-SCRATCH (sans ffmpeg) — brique 2b.
// Produit un flux élémentaire Annex-B (.h264 / .264) RÉELLEMENT DÉCODABLE par ffmpeg,
// VLC, navigateurs. Pipeline par trame (toutes IDR / I-slice) :
//   RGB → YCbCr 4:2:0 → macroblocs I_16×16 → prédiction intra (V/H/DC) → résidu
//   → transformée entière 4×4 + Hadamard DC → quantification QP → CAVLC → NAL.
// SPS + PPS + slice header I + couche macrobloc + reconstruction (pour la prédiction
// des voisins, cohérente au bit près avec le décodeur). Chroma : prédiction seule
// (CBP chroma = 0) — le résidu chroma arrive en brique 2c/3.
//
// Algorithmes réécrits à la sauce Nkentseu (réf lue : spec H.264, minih264 lieff).
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/Video/NkVideoTypes.h" // NkVideoInputFormat
#include "NKMedia/Codecs/Video/H264/NkH264BitWriter.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		struct NkH264Encoder {
			public:
				// Ouvre `path` (.h264/.264 élémentaire Annex-B). `qp` 0..51 (petit = meilleure qualité).
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum = 25, int32 fpsDen = 1,
						  int32 qp = 26);

				// Encode une trame (pixels haut-en-bas) comme image IDR (I-slice) et l'écrit.
				bool WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt);

				bool Close();

				bool IsOpen() const {
					return mOpen;
				}
				int32 FrameCount() const {
					return mFrame;
				}

				// Auto-test : encode un dégradé en mémoire, vérifie que le flux commence par SPS/PPS/IDR
				// et que le round-trip prédiction+résidu reconstruit l'image à un PSNR élevé.
				static bool SelfTest();

			private:
				NkFile mFile;
				int32 mWidth = 0, mHeight = 0;
				int32 mMbW = 0, mMbH = 0;
				int32 mLumaW = 0, mLumaH = 0, mChromaW = 0, mChromaH = 0;
				int32 mFpsNum = 25, mFpsDen = 1;
				int32 mQp = 26;
				int32 mFrame = 0;
				bool mOpen = false;

				// Plans source (padding au multiple de 16 luma / 8 chroma).
				uint8 *mY = nullptr, *mCb = nullptr, *mCr = nullptr;
				// Reconstruction de la trame (référence de prédiction intra des voisins).
				uint8 *mRecY = nullptr, *mRecCb = nullptr, *mRecCr = nullptr;
				// total_coeff des blocs luma 4×4 (contexte nC CAVLC), grille (mMbW*4) × (mMbH*4).
				int32 *mLumaNz = nullptr;
				// idem chroma par composante (Cb,Cr), grille (mMbW*2) × (mMbH*2).
				int32 *mChromaNz[2] = {nullptr, nullptr};

				void ConvertToYuv(const uint8 *pixels, NkVideoInputFormat fmt);
				void EncodeFrame(NkVector<uint8> &out);
				void EncodeMacroblock(NkH264BitWriter &bs, int32 mbX, int32 mbY);
				void Free();
		};

	} // namespace media
} // namespace nkentseu
