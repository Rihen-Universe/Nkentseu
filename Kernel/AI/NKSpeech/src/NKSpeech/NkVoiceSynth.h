// =============================================================================
// NKSpeech/NkVoiceSynth.h — synthèse vocale par FORMANTS (source-filtre) → onde.
//
// Brique TTS (Phase 8) : produire une voix AUDIBLE *from-scratch*, sans donnée ni
// modèle appris. Modèle source-filtre classique (à la Klatt), en DOMAINE TEMPOREL :
// une SOURCE (train d'impulsions glottiques à la fréquence fondamentale F0 pour les
// sons voisés, bruit large bande pour les non-voisés) excite trois RÉSONATEURS de
// FORMANTS en parallèle (F1/F2/F3, filtres numériques du 2nd ordre). Chaque résonateur
// « sonne » entre deux impulsions → un son SOUTENU (une vraie voyelle), pas un grain.
// (Le vocodeur Griffin-Lim NkGriffinLim reste une brique séparée, pour reconstruire une
// onde depuis un spectrogramme de magnitude — ex. mel produit par un futur modèle appris.)
//
// Petite échelle, pédagogique : ça « parle » des voyelles reconnaissables (a/e/i/o/u),
// pas une voix naturelle. Zero-STL, namespace nkentseu::ai.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {

		// Un « phone » à synthétiser : formants (Hz) + durée + voisement.
		struct NkPhone {
				float32 f1 = 0.0f, f2 = 0.0f, f3 = 0.0f; // formants (voyelle) ; 0 = pas de résonance
				float32 durationMs = 120.0f;
				bool voiced = true;   // true = source harmonique (F0) ; false = bruit (fricative)
				float32 gain = 1.0f;  // 0 = silence
		};

		struct NkVoiceSynthConfig {
				int32 sampleRate = 16000;
				float32 f0 = 120.0f;  // fréquence fondamentale (hauteur de voix)
				int32 fftSize = 1024;
				int32 hopSize = 256;
				int32 glIterations = 40;
		};

		class NkVoiceSynth {
			public:
				// Voyelle française approximée (male) : 'a','e','i','o','u' → NkPhone (formants).
				static NkPhone Vowel(char v, float32 durationMs);

				// Synthétise une séquence de phones → forme d'onde mono [-1,1] (via spectrogramme
				// de magnitude + Griffin-Lim). Renvoie les échantillons.
				static NkVector<float32> Synthesize(const NkVector<NkPhone> &phones,
													const NkVoiceSynthConfig &cfg = NkVoiceSynthConfig{});

				// Auto-test headless : synthétise la voyelle 'a' et vérifie que le spectre du signal
				// produit présente bien de l'énergie autour de ses formants F1 et F2.
				static bool SelfTest();
		};

	} // namespace ai
} // namespace nkentseu
