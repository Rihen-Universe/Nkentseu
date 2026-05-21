// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioBackends.h
// DESCRIPTION: Backends audio natifs (WASAPI, CoreAudio, ALSA, Null)
//              Stubs complets prêts à connecter aux API système
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Chaque backend est enregistré automatiquement via NK_REGISTER_AUDIO_BACKEND
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H
#define NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H

#include "NKAudio.h"
#include "NKCore/NkPlatform.h"

namespace nkentseu {
    namespace audio {

        /// Enregistre les backends natifs dans AudioBackendFactory.
        /// Appele EXPLICITEMENT par AudioEngine::Initialize() pour garantir
        /// que les backends sont disponibles meme quand NKAudio est en lib
        /// statique (le linker peut stripper les static initializers).
        /// Idempotent : appels multiples sont no-op.
        NKENTSEU_AUDIO_API void EnsureBackendsRegistered();

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

            // ── Accesseurs pour le thread audio (WasapiThreadProc) ─────────
            // Le thread est un std::function defini dans le .cpp ; il a besoin
            // d'acceder a quelques membres prives. On expose proprement.
            struct WasapiImpl;
            WasapiImpl*           GetImpl()      noexcept { return mImpl; }
            const AudioCallback&  GetCallback() const noexcept { return mCallback; }
            bool                  IsPaused()    const noexcept { return mPaused; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            float32       mLatencyMs  = 10.0f;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;

            // Handles WASAPI (opaque pour éviter windows.h dans le header)
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

            // Accesseurs pour le callback CoreAudio.
            const AudioCallback& GetCallback() const noexcept { return mCallback; }
            bool                 IsPaused()    const noexcept { return mPaused; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            float32       mLatencyMs  = 8.0f;
            bool          mRunning    = false;
            bool          mPaused     = false;
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

            // Accesseurs pour le callback AAudio (defini en C, hors classe).
            const AudioCallback& GetCallback() const noexcept { return mCallback; }
            bool                 IsPaused()    const noexcept { return mPaused; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 128;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;

            struct AAudioImpl;
            AAudioImpl* mImpl = nullptr;
        };

        /**
         * @brief Backend OpenSL ES (Android 4.0+) — fallback pour Android 24-25.
         *
         * AAudio n'est dispo qu'a partir d'Android 26. Pour la compatibilite
         * Android 7.x (Nougat, API 24-25), on utilise OpenSL ES, l'API audio
         * native legacy d'Android (similaire a l'AAL d'iOS). Latence ~30ms
         * (vs ~5ms AAudio) mais marche depuis Android 4.0.
         *
         * Architecture :
         *  - Engine + OutputMix + AudioPlayer (Khronos OpenSL ES spec)
         *  - BufferQueue avec 2 buffers (double-buffering) pour latence basse
         *  - Format : int16 PCM (OpenSL ES ne supporte pas float32 nativement)
         *  - Conversion float32 -> int16 dans le callback
         */
        class NKENTSEU_AUDIO_API OpenSLESAudioBackend : public IAudioBackend {
        public:
            OpenSLESAudioBackend();
            ~OpenSLESAudioBackend() override;

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
            float32 GetLatencyMs()   const override { return 30.0f; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "OpenSLES"; }

            // Accesseurs pour le callback C-style (BufferQueue).
            struct SLImpl;
            const AudioCallback& GetCallback() const noexcept { return mCallback; }
            bool                 IsPaused()    const noexcept { return mPaused; }
            SLImpl*              GetImpl()     noexcept       { return mImpl; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 256;
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;

            SLImpl* mImpl = nullptr;
        };

#endif // NKENTSEU_PLATFORM_ANDROID

        // ====================================================================
        // WEB AUDIO BACKEND (Emscripten / WebAssembly)
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

        /**
         * @brief Backend WebAudio via Emscripten.
         *
         * Cree un AudioContext + ScriptProcessorNode (legacy mais marche
         * partout) cote JavaScript via EM_JS. Le ScriptProcessor reveille
         * un callback C exporte qui appelle mCallback puis transfere le
         * buffer float32 a JS.
         *
         * Latence ~50-150ms selon le navigateur (Chrome/Firefox).
         * Alternative future : AudioWorklet (latence ~5ms) mais necessite
         * cross-origin isolation (COOP/COEP headers) qui limite l'hosting.
         */
        class NKENTSEU_AUDIO_API WebAudioBackend : public IAudioBackend {
        public:
            WebAudioBackend();
            ~WebAudioBackend() override;

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
            float32 GetLatencyMs()   const override { return 80.0f; }
            bool    IsRunning()      const override { return mRunning; }
            const char* GetName()    const override { return "WebAudio"; }

            // Accesseurs pour le bridge C/JS.
            const AudioCallback& GetCallback() const noexcept { return mCallback; }
            bool                 IsPaused()    const noexcept { return mPaused; }

        private:
            int32         mSampleRate = 48000;
            int32         mChannels   = 2;
            int32         mBufferSize = 1024;     // WebAudio buffers larger
            bool          mRunning    = false;
            bool          mPaused     = false;
            AudioCallback mCallback;
            // Buffer interleaved float32 transmis a JS via Module.HEAPF32.
            float32*      mInterleavedBuf = nullptr;
            // Buffer planaire (ch0[0..N], ch1[0..N], ...) attendu par WebAudio.
            float32*      mPlanarBuf      = nullptr;
        };

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN

    } // namespace audio
} // namespace nkentseu

#endif // NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOBACKENDS_H

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
