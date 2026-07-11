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
				// `gop` = intervalle entre images IDR (les autres sont des P inter-frame). gop=1 → tout IDR.
				bool Open(const char *path, int32 width, int32 height, int32 fpsNum = 25, int32 fpsDen = 1,
						  int32 qp = 26, int32 gop = 30);

				// Encode une trame (pixels haut-en-bas) comme image IDR (I-slice) et l'écrit.
				bool WriteFrame(const uint8 *pixels, NkVideoInputFormat fmt);

				bool Close();

				bool IsOpen() const {
					return mOpen;
				}
				int32 FrameCount() const {
					return mFrame;
				}

				// Debug/validation : dumpe la reconstruction (YUV420 planar, recadrée) de chaque trame
				// dans `path`, pour comparaison bit-à-bit avec la sortie d'un décodeur de référence.
				bool EnableReconDump(const char *path);

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
				bool mDeblock = true; // filtre de déblocage en boucle actif
				NkFile mReconFile;	  // dump de reconstruction (validation)
				bool mReconDump = false;

				// Plans source (padding au multiple de 16 luma / 8 chroma).
				uint8 *mY = nullptr, *mCb = nullptr, *mCr = nullptr;
				// Reconstruction de la trame (référence de prédiction intra des voisins).
				uint8 *mRecY = nullptr, *mRecCb = nullptr, *mRecCr = nullptr;
				// Référence inter : reconstruction déblocquée de la trame précédente (P-slices).
				uint8 *mRefY = nullptr, *mRefCb = nullptr, *mRefCr = nullptr;
				// Par macrobloc : vecteur mouvement (quart-pel) + indicateur inter (P), pour la prédiction de MV.
				int32 *mMvX = nullptr, *mMvY = nullptr, *mMbInter = nullptr;
				int32 mGop = 30, mFrameNum = 0, mPoc = 0;
				// total_coeff des blocs luma 4×4 (contexte nC CAVLC), grille (mMbW*4) × (mMbH*4).
				int32 *mLumaNz = nullptr;
				// idem chroma par composante (Cb,Cr), grille (mMbW*2) × (mMbH*2).
				int32 *mChromaNz[2] = {nullptr, nullptr};
				// mode Intra_4×4 de chaque bloc luma 4×4 (2=DC par défaut / pour non-I4×4), pour la
				// prédiction du mode des voisins. Grille (mMbW*4) × (mMbH*4).
				int32 *mI4Mode = nullptr;

				void ConvertToYuv(const uint8 *pixels, NkVideoInputFormat fmt);
				void EncodeFrame(NkVector<uint8> &out);
				// État chroma d'un macrobloc (niveaux + CBP), partagé entre I_16×16 et I_4×4.
				struct ChromaMb {
						int32 lvl[2][4][16]; // niveaux quantifiés (AC en positions 1..15) par composante/bloc
						int32 dcLvl[2][4];	 // niveaux DC chroma (Hadamard 2×2 + quant)
						int32 cbp;			 // 0=aucun, 1=DC, 2=DC+AC
				};

				void EncodeMacroblock(NkH264BitWriter &bs, int32 mbX, int32 mbY); // I_16×16
				void EncodeMbIntra4x4(NkH264BitWriter &bs, int32 mbX, int32 mbY); // I_4×4
				// P-slices (inter-frame) : trame P complète, macrobloc P_L0_16×16, détection P_Skip.
				void EncodePFrame(NkVector<uint8> &out);
				bool TryInterSkip(int32 mbX, int32 mbY); // vrai + commit si le MB est P_Skip
				void EncodeMbInter(NkH264BitWriter &bs, int32 mbX, int32 mbY); // P_L0_16×16 codé
				// Compensation de mouvement depuis la référence (quart-pel luma / 1-8-pel chroma bilinéaire).
				void McLuma(int32 px, int32 py, int32 mvx, int32 mvy, uint8 out[256]);
				void McChroma(int32 comp, int32 cpx, int32 cpy, int32 mvx, int32 mvy, uint8 out[64]);
				void PredictMv(int32 mbX, int32 mbY, int32 &pmx, int32 &pmy); // prédicteur médian (MVD)
				void SkipMv(int32 mbX, int32 mbY, int32 &smx, int32 &smy);	 // prédicteur P_Skip
				// Prédit (intra DC) + résidu + reconstruit le chroma (DC 2×2 + AC), remplit `c`.
				void ComputeChroma(int32 mbX, int32 mbY, bool availTop, bool availLeft, ChromaMb &c);
				// Résidu + reconstruction chroma à partir d'une prédiction donnée (intra DC ou compensée).
				void ComputeChromaFromPred(int32 mbX, int32 mbY, const uint8 pred[2][64], ChromaMb &c);
				// Écrit le résidu chroma (DC puis AC) en CAVLC + met à jour la grille nC chroma.
				void WriteChromaResidual(NkH264BitWriter &bs, int32 mbX, int32 mbY, const ChromaMb &c);
				// Filtre de déblocage en boucle (§8.7) appliqué à la reconstruction (luma + chroma).
				void DeblockFrame();
				void Free();
		};

	} // namespace media
} // namespace nkentseu
