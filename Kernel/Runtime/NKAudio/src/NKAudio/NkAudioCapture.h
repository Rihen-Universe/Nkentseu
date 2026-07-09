// =============================================================================
// NKAudio/NkAudioCapture.h
// -----------------------------------------------------------------------------
// ENREGISTREMENT / CAPTURE audio (entrée microphone) — miroir de la sortie.
// Format normalisé : Float32 interleaved (comme tout NKAudio). Deux modes de
// consommation : PULL (`Read` depuis un ring buffer) ou PUSH (callback appelé
// depuis le thread de capture). Backends : WASAPI capture (Windows), Null
// (autres plateformes / fallback ; ALSA/CoreAudio/AAudio à venir).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKAudio/NkAudioExport.h"

namespace nkentseu {
	namespace audio {

		// Un périphérique d'entrée (microphone).
		struct NkCaptureDeviceInfo {
				NkString id;			// identifiant OS (vide = défaut)
				NkString name;			// nom lisible
				bool isDefault = false; // périphérique d'entrée par défaut
		};

		// Configuration d'une session de capture.
		struct NkCaptureConfig {
				int32 sampleRate = 48000; // Hz
				int32 channels = 1;		  // 1 = mono (voix), 2 = stéréo
				int32 ringSeconds = 4;	  // capacité du ring buffer (secondes)
				NkString deviceId;		  // "" = périphérique par défaut
		};

		// Callback appelé DEPUIS le thread de capture (temps réel) : ne pas bloquer ni allouer.
		using NkAudioInCallback = NkFunction<void(const float32 *interleaved, int32 frames, int32 channels)>;

		// Capture audio (une session = un périphérique à la fois).
		class NKENTSEU_AUDIO_API NkAudioCapture {
			public:
				NkAudioCapture();
				~NkAudioCapture();
				NkAudioCapture(const NkAudioCapture &) = delete;
				NkAudioCapture &operator=(const NkAudioCapture &) = delete;

				// Liste les périphériques d'entrée disponibles.
				static NkVector<NkCaptureDeviceInfo> EnumerateDevices();

				// Ouvre le périphérique (négocie le format) ; false si échec / pas de backend.
				bool Open(const NkCaptureConfig &config);
				void Close();

				// Démarre / arrête la capture (le ring buffer se remplit ; le callback est appelé).
				bool Start();
				void Stop();
				bool IsCapturing() const;

				// PULL : copie jusqu'à `maxFrames` frames (interleaved Float32) dans `out`.
				// Retourne le nombre de frames réellement lues. `out` doit tenir maxFrames*channels floats.
				int32 Read(float32 *out, int32 maxFrames);
				// Frames disponibles à lire dans le ring buffer.
				int32 Available() const;

				// PUSH : callback alternatif au pull (appelé depuis le thread de capture).
				void SetCallback(NkAudioInCallback cb);

				int32 SampleRate() const;
				int32 Channels() const;
				const char *BackendName() const;

				// Auto-test headless du ring buffer (aucun périphérique requis).
				static bool SelfTest();

				struct Impl; // opaque (défini dans le .cpp) — public pour le backend natif

			private:
				Impl *mImpl = nullptr;
		};

	} // namespace audio
} // namespace nkentseu
