// =============================================================================
// NKSpeech/NkASR.h  —  SCAFFOLD (spec, impl staged — voir Kernel/AI/ROADMAP.md Phase 8)
// -----------------------------------------------------------------------------
// RECONNAISSANCE VOCALE (ASR) : audio → texte, from-scratch. Pipeline :
//   features (NkAudioFeatures MFCC) → modèle acoustique (Conv/GRU NKNN) →
//   probabilités par trame → décodage (CTC glouton puis beam + lexique/LM).
// Petit vocabulaire d'abord (mots isolés), multilingue fr/en/ghomala' (bbj).
// Zero-STL, namespace nkentseu::ai.
//
// ⚠️ SCAFFOLD : interfaces posées (briques 2-3 de la Phase 8). Nécessite une perte
// CTC dans NKAutograd + des couches récurrentes (GRU) dans NKNN.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {

		struct NkASRConfig {
				NkString language = NkString("fr"); // "fr" | "en" | "bbj" (ghomala')
				NkString modelPath;					// poids NKMD (vide = non chargé)
				bool useLanguageModel = false;		// re-scoring n-gram/GPT
				int32 beamWidth = 8;
		};

		struct NkASRResult {
				NkString text;
				float32 confidence = 0.0f;
		};

		class NkASR {
			public:
				// Charge un modèle acoustique + lexique. ⬜ impl staged (Phase 8).
				bool Load(const NkASRConfig &config);

				// Transcrit un signal mono [-1,1] (offline). ⬜ impl staged.
				NkASRResult Transcribe(const float32 *mono, int32 samples, int32 sampleRate);

				// Reste : décodage streaming (au fil de NkAudioCapture), diarisation, ponctuation.
		};

	} // namespace ai
} // namespace nkentseu
