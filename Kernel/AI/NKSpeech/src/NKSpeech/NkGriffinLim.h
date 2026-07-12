// =============================================================================
// NKSpeech/NkGriffinLim.h — vocodeur Griffin-Lim (spectrogramme → forme d'onde).
//
// Brique 5 de la Phase 8 (TTS) : reconstruire un SIGNAL audio à partir d'un
// spectrogramme de MAGNITUDE seul (la phase est perdue). Algorithme de Griffin &
// Lim (1984) : on part d'une phase arbitraire, puis on ALTERNE iSTFT → STFT en
// réimposant à chaque tour la magnitude cible et en gardant la phase estimée ;
// la phase converge vers un signal cohérent. Sert de vocodeur *from-scratch* pour
// la synthèse vocale (mel → linéaire → onde), sans aucun modèle appris ni donnée.
//
// STFT/iSTFT maison (FFT radix-2 avant/arrière, fenêtre de Hann, overlap-add
// normalisé). Zero-STL, namespace nkentseu::ai. From-scratch, petite échelle.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {

		// Paramètres STFT + nombre d'itérations de reconstruction de phase.
		struct NkGriffinLimConfig {
				int32 fftSize = 1024; // longueur de fenêtre = longueur FFT (puissance de 2)
				int32 hopSize = 256;  // pas entre trames (recouvrement = fftSize - hopSize)
				int32 iterations = 60; // itérations Griffin-Lim (plus = meilleure phase)
		};

		// Spectrogramme de magnitude : `frames` trames × `bins` (= fftSize/2 + 1), row-major.
		struct NkMagSpectrogram {
				NkVector<float32> data; // frames*bins, row-major (magnitude linéaire ≥ 0)
				int32 frames = 0;
				int32 bins = 0;
		};

		class NkGriffinLim {
			public:
				// STFT « magnitude » d'un signal mono : |FFT| par trame fenêtrée (Hann).
				// bins = fftSize/2 + 1. Utile pour préparer/valider un spectrogramme.
				static NkMagSpectrogram Magnitude(const float32 *mono, int32 samples,
												  const NkGriffinLimConfig &cfg = NkGriffinLimConfig{});

				// Reconstruit un signal (mono, [-1,1] non garanti) depuis un spectrogramme de
				// MAGNITUDE, par itérations Griffin-Lim. Longueur ≈ (frames-1)*hop + fftSize.
				static NkVector<float32> Reconstruct(const NkMagSpectrogram &mag,
													 const NkGriffinLimConfig &cfg = NkGriffinLimConfig{});

				// Auto-test headless : un signal connu → magnitude → reconstruction ; la magnitude
				// du signal reconstruit doit re-coller à la cible (erreur relative faible) et
				// l'énergie être préservée. Aucun GPU, aucune donnée.
				static bool SelfTest();
		};

	} // namespace ai
} // namespace nkentseu
