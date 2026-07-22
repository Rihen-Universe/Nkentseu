// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — ORCHESTRATION (celt_decoder.c celt_decode_with_ec). Assemble
// toutes les briques CELT en un décodeur de trame → PCM. État persistant entre
// trames (énergie précédente `oldEBands`, historique anti-collapse, buffer d'overlap
// de synthèse, mémoire deemphasis, graine LCG).
//
// Chemin complet MONO câblé : silence + coarse/fine energy + tf_decode + spread +
// caps + dynalloc + compute_allocation (décodage) + quant_all_bands + anti_collapse
// + finalise + denormalise + IMDCT CELT (DFT directe) + overlap-add + deemphasis.
// Algorithmes RFC 6716 réécrits à la sauce Nkentseu. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltDecoder {
			public:
				static constexpr int32 kNumBands = 21;
				static constexpr int32 kShortMdctSize = 120; // à 48 kHz
				static constexpr int32 kOverlap = 120;
				static constexpr int32 kMaxChannels = 2;
				static constexpr int32 kDecBufSize = 2048; // tampon de synthèse par canal (overlap-add MDCT)

				// Résultat du décodage d'une trame.
				struct NkFrameFlags {
						bool silence = false;
						bool transient = false;
						bool intra = false;
				};

				// (Ré)initialise l'état pour `C` canaux.
				void Init(int32 C);

				// Décode UNE trame CELT (données range-codées `data/len`, facteur `LM`) → `pcm` (N*C samples
				// interleaved, N = (1<<LM)*shortMdctSize). Renvoie true si OK. Met à jour l'état interne.
				// `endBand` = dernière bande codée selon la bande passante du TOC (CELT-only :
				// NB→13, WB→17, SWB→19, FB→21 — opus_decoder.c). Décoder 21 bandes sur un
				// paquet WB désynchronise tout le bitstream (bits lus en trop).
				bool DecodeFrame(const uint8 *data, int32 len, int32 LM, float32 *pcm, NkFrameFlags *outFlags,
								 int32 endBand = 21);

				// Décode depuis un range decoder PARTAGÉ (mode HYBRIDE : CELT continue
				// après SILK dans le même flux), sur les bandes [start, end). Si accum,
				// AJOUTE au pcm (bande haute par-dessus SILK) au lieu d'écraser.
				// Le silence + post-filtre ne sont lus que si start == 0 (CELT-only).
				bool DecodeShared(NkOpusRangeDecoder &dec, int32 len, int32 LM, int32 start, int32 end, float32 *pcm,
								  bool accum, NkFrameFlags *outFlags);

				static bool SelfTest();

			private:
				int32 mC = 1;
				float32 mOldEBands[kNumBands * kMaxChannels];				// énergie (coarse+fine) trame précédente
				float32 mOldLogE[kNumBands * kMaxChannels];					// historique énergie (anti-collapse)
				float32 mOldLogE2[kNumBands * kMaxChannels];				// historique énergie -2 (anti-collapse)
				float32 mPreemphMem[kMaxChannels];							// état deemphasis
				float32 mDecodeMem[(kDecBufSize + kOverlap) * kMaxChannels]; // buffer glissant de synthèse
				uint32 mRng = 0;											// graine LCG (folding + anti-collapse)
				// État du post-filtre (comb filter CELT), conservé entre trames. La trame N applique
				// les params de la trame N-1 (…Old) sur le 1er sous-bloc puis fond vers les params
				// courants (crossfade). Décodés seulement en CELT-only (start==0).
				int32 mPostfilterPeriod = 0;
				int32 mPostfilterPeriodOld = 0;
				float32 mPostfilterGain = 0.0f;
				float32 mPostfilterGainOld = 0.0f;
				int32 mPostfilterTapset = 0;
				int32 mPostfilterTapsetOld = 0;
				bool mInit = false;
		};

	} // namespace media
} // namespace nkentseu
