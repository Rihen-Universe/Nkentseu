// =============================================================================
// NKSpeech/NkAudioFeatures.h  —  SCAFFOLD (spec, impl staged — voir Kernel/AI/ROADMAP.md Phase 8)
// -----------------------------------------------------------------------------
// Extraction de FEATURES audio pour la parole (fondation partagée ASR + TTS).
// Chaîne classique : pré-emphase → trames fenêtrées (Hann) → FFT (radix-2, cf.
// NkDenoiser) → spectre de puissance → BANC DE FILTRES MEL → log → DCT = MFCC
// (+ deltas). Zero-STL, namespace nkentseu::ai. From-scratch, petite échelle.
//
// ⚠️ SCAFFOLD : interfaces posées, implémentation à venir (brique 1 de la Phase 8).
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {

		// Paramètres d'extraction (voix : 16 kHz, trames 25 ms, pas 10 ms, 40 Mel, 13 MFCC).
		struct NkAudioFeatureConfig {
				int32 sampleRate = 16000;
				int32 frameSizeMs = 25;
				int32 frameStepMs = 10;
				int32 fftSize = 512;
				int32 melBands = 40;
				int32 mfccCount = 13;
				float32 preEmphasis = 0.97f;
				float32 fMin = 20.0f;
				float32 fMax = 8000.0f;
				bool useDeltas = true; // ajoute Δ et ΔΔ (×3 la dimension)
		};

		// Un tableau de features : frames × dims (row-major), + les dimensions.
		struct NkFeatureMatrix {
				NkVector<float32> data; // frames*dims, row-major
				int32 frames = 0;
				int32 dims = 0;
		};

		struct NkAudioFeatures {
			public:
				// Mel-spectrogramme log (frames × melBands) d'un signal mono normalisé [-1,1].
				// ⬜ impl staged (Phase 8, brique 1).
				static NkFeatureMatrix LogMelSpectrogram(const float32 *mono, int32 samples,
														 const NkAudioFeatureConfig &cfg = NkAudioFeatureConfig{});

				// MFCC (frames × mfccCount, +deltas si activés) — DCT-II du log-mel.
				// ⬜ impl staged (Phase 8, brique 1).
				static NkFeatureMatrix MFCC(const float32 *mono, int32 samples,
											const NkAudioFeatureConfig &cfg = NkAudioFeatureConfig{});
		};

	} // namespace ai
} // namespace nkentseu
