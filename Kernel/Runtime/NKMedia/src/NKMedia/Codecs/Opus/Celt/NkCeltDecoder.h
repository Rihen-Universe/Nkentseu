// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — ORCHESTRATION (celt_decoder.c celt_decode_with_ec). Assemble
// toutes les briques CELT en un décodeur de trame → PCM. État persistant entre
// trames (énergie précédente `oldEBands`, mémoire d'overlap, deemphasis).
//
// ⚠️ EN COURS D'ASSEMBLAGE : les FLAGS (silence/transient/intra) + le chemin SILENCE
// (→ PCM zéro) sont câblés et testés ; le chemin non-silence (quant_all_bands +
// IMDCT CELT overlap-add) est en cours. Zero-STL, nkentseu::media.
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
				bool DecodeFrame(const uint8 *data, int32 len, int32 LM, float32 *pcm, NkFrameFlags *outFlags);

				static bool SelfTest();

			private:
				int32 mC = 1;
				float32 mOldEBands[kNumBands * kMaxChannels];
				float32 mPreemphMem[kMaxChannels];
				float32 mOverlapMem[kOverlap * kMaxChannels];
				bool mInit = false;
		};

	} // namespace media
} // namespace nkentseu
