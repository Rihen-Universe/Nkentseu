// =============================================================================
// NKSpeech/NkTTS.h  —  SCAFFOLD (spec, impl staged — voir Kernel/AI/ROADMAP.md Phase 8)
// -----------------------------------------------------------------------------
// SYNTHÈSE VOCALE (TTS) : texte → audio, from-scratch. Pipeline :
//   normalisation texte → G2P (texte→phonèmes, table par langue) → durées →
//   modèle acoustique (phonèmes→mel) → VOCODEUR (Griffin-Lim d'abord, puis
//   neuronal léger) → PCM float32 jouable via NKAudio.
// Multilingue fr/en/ghomala' (bbj). Zero-STL, namespace nkentseu::ai.
//
// ⚠️ SCAFFOLD : interfaces posées (briques 4-5 de la Phase 8).
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {

		struct NkTTSConfig {
				NkString language = NkString("fr"); // "fr" | "en" | "bbj" (ghomala')
				NkString voicePath;					// poids/voix (vide = non chargé)
				int32 sampleRate = 22050;
				float32 speed = 1.0f; // facteur de débit
				float32 pitch = 1.0f; // facteur de hauteur
		};

		// Audio synthétisé (mono, float32 [-1,1]).
		struct NkTTSAudio {
				NkVector<float32> samples;
				int32 sampleRate = 22050;
		};

		class NkTTS {
			public:
				// Charge le front-end (G2P) + le modèle acoustique + vocodeur. ⬜ impl staged (Phase 8).
				bool Load(const NkTTSConfig &config);

				// Synthétise une phrase. ⬜ impl staged.
				NkTTSAudio Speak(const NkString &text);

				// Étape intermédiaire exposée (utile debug / visèmes PV3DE) : texte → phonèmes.
				NkVector<NkString> TextToPhonemes(const NkString &text);

				// Reste : prosodie, styles, streaming, visèmes (lien PV3DE / animation faciale).
		};

	} // namespace ai
} // namespace nkentseu
