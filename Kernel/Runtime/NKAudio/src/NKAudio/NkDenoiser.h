// =============================================================================
// NKAudio/NkDenoiser.h
// -----------------------------------------------------------------------------
// DÉBRUITAGE + NORMALISATION hors-ligne (from-scratch, zero-STL). Chaîne :
//   passe-haut (DC/rumble) → SOUSTRACTION SPECTRALE (STFT + FFT maison, profil de
//   bruit estimé sur le début) → noise gate (résidu dans les silences) →
//   NORMALISATION crête (auto-gain vers un niveau cible : corrige « volume trop bas »).
// Conçu pour nettoyer un enregistrement micro (NkAudioCapture → WAV). Traite chaque
// canal indépendamment. Pas de temps réel (buffer complet) — usage outil/offline.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKAudio/NkAudioExport.h"

namespace nkentseu {
	namespace audio {

		// Paramètres de débruitage/normalisation.
		struct NkDenoiseOptions {
				// Passe-haut (DC blocker) : coupe le continu et le rumble basse fréquence.
				bool highPass = true;
				float32 highPassHz = 80.0f;

				// Soustraction spectrale.
				bool spectral = true;
				float32 noiseMs = 400.0f;		// durée initiale supposée « bruit seul » → profil
				float32 overSubtraction = 1.8f; // facteur de sur-soustraction (agressivité)
				float32 spectralFloor = 0.06f;	// plancher (0..1) anti « musical noise »

				// Noise gate (post) : atténue sous un seuil d'enveloppe.
				bool gate = true;
				float32 gateThreshDb = -50.0f; // seuil (dBFS)
				float32 gateFloorDb = -24.0f;  // atténuation appliquée sous le seuil (dB, négatif)
				float32 attackMs = 5.0f;
				float32 releaseMs = 80.0f;

				// Normalisation crête finale (auto-gain) : corrige un volume trop bas.
				bool normalize = true;
				float32 targetPeakDb = -1.0f; // crête cible (dBFS)
				float32 maxGainDb = 30.0f;	  // gain max autorisé (évite d'exploser le bruit résiduel)
		};

		struct NKENTSEU_AUDIO_API NkDenoiser {
			public:
				// Traite un signal MONO. `out` reçoit `frames` échantillons nettoyés.
				static bool ProcessMono(const float32 *in, int32 frames, int32 sampleRate, NkVector<float32> &out,
										const NkDenoiseOptions &opt = NkDenoiseOptions{});

				// Traite un signal INTERLEAVED (channels canaux), canal par canal.
				static bool Process(const float32 *interleaved, int32 frames, int32 channels, int32 sampleRate,
									NkVector<float32> &out, const NkDenoiseOptions &opt = NkDenoiseOptions{});

				// Auto-test headless : bruit blanc + sinus → le plancher de bruit chute, le sinus survit.
				static bool SelfTest();
		};

	} // namespace audio
} // namespace nkentseu
