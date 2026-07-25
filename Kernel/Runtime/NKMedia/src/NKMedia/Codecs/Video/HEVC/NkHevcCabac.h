// =============================================================================
// NKMedia/Codecs/Video/HEVC/NkHevcCabac.h
// -----------------------------------------------------------------------------
// CABAC HEVC (§9.3) — brique 4 du décodeur H.265.
//
// Le MOTEUR arithmétique est STRICTEMENT IDENTIQUE à celui d'H.264 (mêmes tables
// 9-44/9-45 rangeTabLPS + transIdx, même init range=510/offset 9 bits, mêmes
// DecodeDecision/DecodeBypass/DecodeTerminate) → `NkCabacEngine` de NkH264Cabac.h
// est réutilisé TEL QUEL (déjà validé bit-exact via le décodage H.264 Main/High).
//
// Ce qui est SPÉCIFIQUE à HEVC (fourni ici) :
//   - L'initialisation des contextes (§9.3.2.2) : un `initValue` 8 bits par
//     contexte (au lieu de paires (m,n) directes) → slopeIdx/offsetIdx →
//     m = slopeIdx*5-45, n = (offsetIdx<<3)-16, puis la MÊME formule preCtxState
//     qu'H.264 (NkCabacInitOne réutilisée).
//   - Les tables d'initValues : 3 initType × kHevcContextCount contextes,
//     disposition et valeurs alignées sur l'implémentation de référence ffmpeg
//     (libavcodec/hevc/cabac.c, validée bit-exacte) — cross-checkées
//     numériquement contre elle à la livraison de cette brique.
//   - Le choix d'initType par slice (§9.3.2.2) : I→0 ; P→(cabac_init?2:1) ;
//     B→(cabac_init?1:2).
//
// Le décodage des éléments de syntaxe CTU (sao(), coding_quadtree()…) vient
// dans les briques suivantes. Zero-STL, nkentseu::media.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cabac.h"

namespace nkentseu {
	namespace media {

		// Index de DÉPART des contextes de chaque élément de syntaxe dans le tableau
		// d'états (chaînés pour éviter toute erreur d'arithmétique manuelle). Même
		// disposition que la référence ffmpeg. Les éléments 100% bypass (sao_offset_sign,
		// mpm_idx, coeff_sign…) n'ont pas de contexte et n'apparaissent pas ici.
		enum NkHevcCtxIdx : nk_int32 {
			kHevcCtxSaoMergeFlag = 0,											 // 1 ctx
			kHevcCtxSaoTypeIdx = kHevcCtxSaoMergeFlag + 1,						 // 1
			kHevcCtxSplitCuFlag = kHevcCtxSaoTypeIdx + 1,						 // 3
			kHevcCtxCuTransquantBypass = kHevcCtxSplitCuFlag + 3,				 // 1
			kHevcCtxSkipFlag = kHevcCtxCuTransquantBypass + 1,					 // 3
			kHevcCtxCuQpDelta = kHevcCtxSkipFlag + 3,							 // 3
			kHevcCtxPredModeFlag = kHevcCtxCuQpDelta + 3,						 // 1
			kHevcCtxPartMode = kHevcCtxPredModeFlag + 1,						 // 4
			kHevcCtxPrevIntraLumaPred = kHevcCtxPartMode + 4,					 // 1
			kHevcCtxIntraChromaPredMode = kHevcCtxPrevIntraLumaPred + 1,		 // 2
			kHevcCtxMergeFlag = kHevcCtxIntraChromaPredMode + 2,				 // 1
			kHevcCtxMergeIdx = kHevcCtxMergeFlag + 1,							 // 1
			kHevcCtxInterPredIdc = kHevcCtxMergeIdx + 1,						 // 5
			kHevcCtxRefIdxL0 = kHevcCtxInterPredIdc + 5,						 // 2
			kHevcCtxRefIdxL1 = kHevcCtxRefIdxL0 + 2,							 // 2
			kHevcCtxAbsMvdGreater0 = kHevcCtxRefIdxL1 + 2,						 // 2
			kHevcCtxAbsMvdGreater1 = kHevcCtxAbsMvdGreater0 + 2,				 // 2
			kHevcCtxMvpLxFlag = kHevcCtxAbsMvdGreater1 + 2,						 // 1
			kHevcCtxNoResidualData = kHevcCtxMvpLxFlag + 1,						 // 1
			kHevcCtxSplitTransform = kHevcCtxNoResidualData + 1,				 // 3
			kHevcCtxCbfLuma = kHevcCtxSplitTransform + 3,						 // 2
			kHevcCtxCbfCbCr = kHevcCtxCbfLuma + 2,								 // 5
			kHevcCtxTransformSkip = kHevcCtxCbfCbCr + 5,						 // 2
			kHevcCtxExplicitRdpcmFlag = kHevcCtxTransformSkip + 2,				 // 2
			kHevcCtxExplicitRdpcmDir = kHevcCtxExplicitRdpcmFlag + 2,			 // 2
			kHevcCtxLastSigCoeffXPrefix = kHevcCtxExplicitRdpcmDir + 2,			 // 18
			kHevcCtxLastSigCoeffYPrefix = kHevcCtxLastSigCoeffXPrefix + 18,		 // 18
			kHevcCtxSigCoeffGroupFlag = kHevcCtxLastSigCoeffYPrefix + 18,		 // 4
			kHevcCtxSigCoeffFlag = kHevcCtxSigCoeffGroupFlag + 4,				 // 44 (42 Main + 2 ext)
			kHevcCtxCoeffAbsGreater1 = kHevcCtxSigCoeffFlag + 44,				 // 24
			kHevcCtxCoeffAbsGreater2 = kHevcCtxCoeffAbsGreater1 + 24,			 // 6
			kHevcCtxLog2ResScaleAbs = kHevcCtxCoeffAbsGreater2 + 6,				 // 8 (ext 4:4:4)
			kHevcCtxResScaleSign = kHevcCtxLog2ResScaleAbs + 8,					 // 2 (ext 4:4:4)
			kHevcCtxCuChromaQpOffsetFlag = kHevcCtxResScaleSign + 2,			 // 1 (ext)
			kHevcCtxCuChromaQpOffsetIdx = kHevcCtxCuChromaQpOffsetFlag + 1,		 // 1 (ext)
			kHevcContextCount = kHevcCtxCuChromaQpOffsetIdx + 1,
		};

		// État CABAC d'une slice : les kHevcContextCount modèles de contexte.
		// (WPP : la brique de décodage CTU devra en garder une COPIE après le 2e CTB
		// de chaque rangée pour initialiser la rangée suivante — §9.3.1.)
		struct NkHevcCabacState {
			public:
				NkCabacCtx ctx[kHevcContextCount];

				// §9.3.2.2 : initType effectif d'une slice.
				// I -> 0 ; P -> (cabac_init ? 2 : 1) ; B -> (cabac_init ? 1 : 2).
				static int32 InitTypeFor(int32 sliceType /*NkHevcSliceType*/, bool cabacInitFlag);

				// Initialise les contextes pour (SliceQpY, initType).
				void Init(int32 sliceQp, int32 initType);

				// Table brute (exposée pour le self-test/cross-check) :
				// [initType][ctxIdx] -> initValue 8 bits.
				static const uint8 *InitValues(int32 initType);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
