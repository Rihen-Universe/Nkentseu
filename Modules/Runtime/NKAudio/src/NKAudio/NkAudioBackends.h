// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioBackends.h
// DESCRIPTION: Backends audio natifs (WASAPI, CoreAudio, ALSA, Null)
//              Stubs complets prêts à connecter aux API système
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Chaque backend est enregistré automatiquement via NK_REGISTER_AUDIO_BACKEND
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H_INCLUDED
#define NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H_INCLUDED

#include "NkAudio.h"
#include "NKCore/NkPlatform.h"

namespace nkentseu {
    namespace audio {

        // ====================================================================
        // NULL BACKEND (Test, no output — toutes plateformes)
        // ====================================================================

        /**
         * @brief Backend silencieux pour tests et serveurs sans sortie audio
         *
         * Génère des "ticks" périodiques simulant le timing hardware
         * via un thread C (POSIX/WinAPI). Idéal pour tests unitaires
         * du moteur audio sans matériel.
         */
        class NKENTSEU_AUDIO_API NullAudioBackend : public IAudioBackend {
        public:
            NullAudioBackend();
            ~NullAudioBackend() override;

            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels;   }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return (float32)mBufferSize / (float32)mSampleRate * 1000.0f; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "Null"; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;
            float32*      mBuffer     = nullptr;

            // Thread de tick simulé
            struct ThreadHandle;
            ThreadHandle* mThread = nullptr;

            static void ThreadFunc(void* userData);
        };

        // ====================================================================
        // WASAPI BACKEND (Windows Vista+)
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)

        /**
         * @brief Backend WASAPI (Windows Audio Session API)
         *
         * Mode exclusif ou partagé. Mode exclusif = latence minimale (~5ms).
         * Mode partagé = latence variable (~20-50ms) mais compatible.
         *
         * Connexion réelle :
         *   #include <mmdeviceapi.h>
         *   #include <audioclient.h>
         *   CoInitializeEx(nullptr, COINIT_MULTITHREADED);
         */
        class NKENTSEU_AUDIO_API WasapiAudioBackend : public IAudioBackend {
        public:
            WasapiAudioBackend();
            ~WasapiAudioBackend() override;

            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels; }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return mLatencyMs; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "WASAPI"; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            float32       mLatencyMs  = 10.0f;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;

            // Handles WASAPI (opaque pour éviter windows.h dans le header)
            struct WasapiImpl;
            WasapiImpl* mImpl = nullptr;
        };

        /**
         * @brief Backend DirectSound (Windows XP/7 legacy)
         *
         * Latence plus élevée que WASAPI (~40-80ms) mais compatible
         * XP/Vista sans AudioClient. Utilisé comme fallback.
         */
        class NKENTSEU_AUDIO_API DirectSoundAudioBackend : public IAudioBackend {
        public:
            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels; }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return 40.0f; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "DirectSound"; }

        private:
            int32         mSampleRate = 44100;
            int32         mChannels   = 2;
            int32         mBufferSize = 512;
            bool          mRunning    = false;
            AudioCallback mCallback;

            struct DsImpl;
            DsImpl* mImpl = nullptr;
        };

#endif // NKENTSEU_PLATFORM_WINDOWS

        // ====================================================================
        // CORE AUDIO BACKEND (macOS / iOS)
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

        /**
         * @brief Backend CoreAudio (macOS 10.6+ / iOS 2.0+)
         *
         * Connexion via AudioUnit / AUGraph.
         * Latence native ~5-10ms sur macOS, ~20ms sur iOS.
         *
         * Connexion réelle :
         *   #include <AudioToolbox/AudioToolbox.h>
         *   #include <CoreAudio/CoreAudio.h>
         */
        class NKENTSEU_AUDIO_API CoreAudioBackend : public IAudioBackend {
        public:
            CoreAudioBackend();
            ~CoreAudioBackend() override;

            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels; }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return mLatencyMs; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "CoreAudio"; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            float32       mLatencyMs  = 8.0f;
            bool          mRunning    = false;
            AudioCallback mCallback;

            struct CoreAudioImpl;
            CoreAudioImpl* mImpl = nullptr;
        };

#endif // NKENTSEU_PLATFORM_MACOS || NKENTSEU_PLATFORM_IOS

        // ====================================================================
        // ALSA BACKEND (Linux)
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_LINUX)

        /**
         * @brief Backend ALSA (Advanced Linux Sound Architecture)
         *
         * Accès direct au matériel. Latence ~10-20ms.
         * Connexion réelle :
         *   #include <alsa/asoundlib.h>
         *   snd_pcm_open(&mPcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
         */
        class NKENTSEU_AUDIO_API AlsaAudioBackend : public IAudioBackend {
        public:
            AlsaAudioBackend();
            ~AlsaAudioBackend() override;

            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels; }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return mLatencyMs; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "ALSA"; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            float32       mLatencyMs  = 15.0f;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;

            // Handle snd_pcm_t* (opaque)
            void* mPcm    = nullptr;
            float32* mBuffer = nullptr;

            struct ThreadHandle;
            ThreadHandle* mThread = nullptr;

            static void ThreadFunc(void* userData);
        };

#endif // NKENTSEU_PLATFORM_LINUX

        // ====================================================================
        // AAUDIO BACKEND (Android 8.0+)
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_ANDROID)

        /**
         * @brief Backend AAudio (Android 8.0+)
         *
         * Latence ultra-faible ~5ms sur hardware compatible.
         * Connexion réelle :
         *   #include <aaudio/AAudio.h>
         */
        class NKENTSEU_AUDIO_API AAudioBackend : public IAudioBackend {
        public:
            bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) override;
            void Shutdown()  override;
            void SetCallback(AudioCallback callback) override;
            void Start()     override;
            void Stop()      override;
            void Pause()     override;
            void Resume()    override;

            int32   GetSampleRate()  const override { return mSampleRate; }
            int32   GetChannels()    const override { return mChannels; }
            int32   GetBufferSize()  const override { return mBufferSize; }
            float32 GetLatencyMs()   const override { return 5.0f; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "AAudio"; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 128;
            bool          mRunning    = false;
            AudioCallback mCallback;

            struct AAudioImpl;
            AAudioImpl* mImpl = nullptr;
        };

#endif // NKENTSEU_PLATFORM_ANDROID

    } // namespace audio
} // namespace nkentseu

#endif // NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
