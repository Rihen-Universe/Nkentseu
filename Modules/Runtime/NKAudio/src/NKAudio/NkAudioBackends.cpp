// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioBackends.cpp
// DESCRIPTION: Implémentation du NullBackend + stubs des backends natifs
//              + enregistrement automatique dans la factory
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Le NullBackend est entièrement fonctionnel (thread de tick simulé).
//        Les autres backends ont leurs corps vides prêts à être connectés.
// -----------------------------------------------------------------------------

#include "NkAudioBackends.h"
#include "NKCore/NkPlatform.h"

#include <cstring>
#include <cstdio>

// Platform threading
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   include <pthread.h>
#   include <unistd.h>
#endif

namespace nkentseu {
    namespace audio {

        // ====================================================================
        // NULL BACKEND — IMPLÉMENTATION COMPLÈTE
        // ====================================================================

        struct NullAudioBackend::ThreadHandle {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            HANDLE handle = nullptr;
#else
            pthread_t thread = 0;
            bool      valid  = false;
#endif
        };

        void NullAudioBackend::ThreadFunc(void* userData) {
            NullAudioBackend* self = (NullAudioBackend*)userData;
            if (!self || !self->mBuffer) return;

            float32 periodMs = (float32)self->mBufferSize / (float32)self->mSampleRate * 1000.0f;
            int32   sleepMs  = (int32)periodMs;
            if (sleepMs < 1) sleepMs = 1;

            while (self->mRunning) {
                if (!self->mPaused && self->mCallback) {
                    memset(self->mBuffer, 0, (usize)self->mBufferSize * (usize)self->mChannels * sizeof(float32));
                    self->mCallback(self->mBuffer, self->mBufferSize, self->mChannels);
                }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
                Sleep((DWORD)sleepMs);
#else
                usleep((useconds_t)(sleepMs * 1000));
#endif
            }
        }

        NullAudioBackend::NullAudioBackend() = default;

        NullAudioBackend::~NullAudioBackend() {
            if (mRunning) Shutdown();
        }

        bool NullAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate;
            mChannels   = channels;
            mBufferSize = bufferSize;

            mBuffer = (float32*)::operator new((usize)bufferSize * (usize)channels * sizeof(float32));
            if (!mBuffer) return false;

            mThread = new ThreadHandle{};
            return true;
        }

        void NullAudioBackend::Shutdown() {
            Stop();
            if (mBuffer) { ::operator delete(mBuffer); mBuffer = nullptr; }
            delete mThread; mThread = nullptr;
        }

        void NullAudioBackend::SetCallback(AudioCallback callback) { mCallback = callback; }

        void NullAudioBackend::Start() {
            if (mRunning || !mThread) return;
            mRunning = true;
            mPaused  = false;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            mThread->handle = CreateThread(
                nullptr, 0,
                (LPTHREAD_START_ROUTINE)[](LPVOID p) -> DWORD { ThreadFunc(p); return 0; },
                this, 0, nullptr);
#else
            pthread_create(&mThread->thread, nullptr,
                [](void* p) -> void* { ThreadFunc(p); return nullptr; }, this);
            mThread->valid = true;
#endif
        }

        void NullAudioBackend::Stop() {
            if (!mRunning) return;
            mRunning = false;

            if (mThread) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mThread->handle) {
                    WaitForSingleObject(mThread->handle, 2000);
                    CloseHandle(mThread->handle);
                    mThread->handle = nullptr;
                }
#else
                if (mThread->valid) {
                    pthread_join(mThread->thread, nullptr);
                    mThread->valid = false;
                }
#endif
            }
        }

        void NullAudioBackend::Pause()  { mPaused = true;  }
        void NullAudioBackend::Resume() { mPaused = false; }

        // ====================================================================
        // WASAPI BACKEND (Windows) — CORPS PRÊTS À CONNECTER
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)

        struct WasapiAudioBackend::WasapiImpl {
            // IMMDeviceEnumerator* enumerator = nullptr;
            // IMMDevice*           device     = nullptr;
            // IAudioClient*        client     = nullptr;
            // IAudioRenderClient*  renderer   = nullptr;
            // HANDLE               event      = nullptr;
            // HANDLE               thread     = nullptr;
            // bool                 running    = false;
        };

        WasapiAudioBackend::WasapiAudioBackend() : mImpl(new WasapiImpl{}) { }
        WasapiAudioBackend::~WasapiAudioBackend() {
            if (mRunning) Shutdown();
            delete mImpl;
        }

        bool WasapiAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate;
            mChannels   = channels;
            mBufferSize = bufferSize;

            /*
             * CONNEXION RÉELLE :
             *
             * CoInitializeEx(nullptr, COINIT_MULTITHREADED);
             *
             * IMMDeviceEnumerator* enumerator = nullptr;
             * CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
             *     __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
             *
             * enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mImpl->device);
             * mImpl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&mImpl->client);
             *
             * WAVEFORMATEX wfx = {};
             * wfx.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
             * wfx.nChannels      = (WORD)channels;
             * wfx.nSamplesPerSec = (DWORD)sampleRate;
             * wfx.wBitsPerSample = 32;
             * wfx.nBlockAlign    = wfx.nChannels * (wfx.wBitsPerSample / 8);
             * wfx.nAvgBytesPerSec= wfx.nSamplesPerSec * wfx.nBlockAlign;
             *
             * REFERENCE_TIME desired = (REFERENCE_TIME)((double)bufferSize / sampleRate * 1e7);
             * mImpl->client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
             *     desired, 0, &wfx, nullptr);
             *
             * mImpl->event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
             * mImpl->client->SetEventHandle(mImpl->event);
             * mImpl->client->GetService(__uuidof(IAudioRenderClient), (void**)&mImpl->renderer);
             */

            // Fallback vers NullBackend pendant le dev
            (void)mImpl;
            return true;
        }

        void WasapiAudioBackend::Shutdown() {
            Stop();
            // mImpl->renderer->Release(); mImpl->client->Release(); etc.
            mRunning = false;
        }

        void WasapiAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }

        void WasapiAudioBackend::Start() {
            if (mRunning) return;
            mRunning = true;
            // mImpl->client->Start();
            // CreateThread(nullptr, 0, WasapiThreadFunc, this, 0, nullptr);
        }

        void WasapiAudioBackend::Stop() {
            if (!mRunning) return;
            mRunning = false;
            // mImpl->client->Stop();
        }

        void WasapiAudioBackend::Pause()  { mPaused = true;  }
        void WasapiAudioBackend::Resume() { mPaused = false; }

        // ── DirectSound ──────────────────────────────────────────────────────

        struct DirectSoundAudioBackend::DsImpl { /* IDirectSound8* ds = nullptr; ... */ };

        bool DirectSoundAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
            mSampleRate = sr; mChannels = ch; mBufferSize = buf;
            mImpl = new DsImpl{};
            // DirectSoundCreate8, CreateSoundBuffer, etc.
            return true;
        }
        void DirectSoundAudioBackend::Shutdown()  { delete mImpl; mImpl = nullptr; mRunning = false; }
        void DirectSoundAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void DirectSoundAudioBackend::Start()  { mRunning = true;  }
        void DirectSoundAudioBackend::Stop()   { mRunning = false; }
        void DirectSoundAudioBackend::Pause()  { }
        void DirectSoundAudioBackend::Resume() { }

#endif // NKENTSEU_PLATFORM_WINDOWS

        // ====================================================================
        // CORE AUDIO (macOS/iOS) — CORPS PRÊTS À CONNECTER
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

        struct CoreAudioBackend::CoreAudioImpl {
            // AudioUnit  outputUnit = nullptr;
            // bool       running    = false;
        };

        CoreAudioBackend::CoreAudioBackend() : mImpl(new CoreAudioImpl{}) { }
        CoreAudioBackend::~CoreAudioBackend() { if (mRunning) Shutdown(); delete mImpl; }

        bool CoreAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate; mChannels = channels; mBufferSize = bufferSize;

            /*
             * CONNEXION RÉELLE :
             *
             * AudioComponentDescription desc = {};
             * desc.componentType    = kAudioUnitType_Output;
             * desc.componentSubType = kAudioUnitSubType_DefaultOutput; // kAudioUnitSubType_RemoteIO sur iOS
             * desc.componentManufacturer = kAudioUnitManufacturer_Apple;
             *
             * AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
             * AudioComponentInstanceNew(comp, &mImpl->outputUnit);
             *
             * AURenderCallbackStruct cb;
             * cb.inputProc       = CoreAudioRenderCallback;
             * cb.inputProcRefCon = this;
             * AudioUnitSetProperty(mImpl->outputUnit,
             *     kAudioUnitProperty_SetRenderCallback,
             *     kAudioUnitScope_Input, 0, &cb, sizeof(cb));
             *
             * AudioStreamBasicDescription asbd = {};
             * asbd.mSampleRate      = sampleRate;
             * asbd.mFormatID        = kAudioFormatLinearPCM;
             * asbd.mFormatFlags     = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
             * asbd.mChannelsPerFrame = channels;
             * asbd.mBitsPerChannel  = 32;
             * asbd.mFramesPerPacket = 1;
             * asbd.mBytesPerFrame   = asbd.mChannelsPerFrame * sizeof(float);
             * asbd.mBytesPerPacket  = asbd.mBytesPerFrame;
             *
             * AudioUnitSetProperty(mImpl->outputUnit, kAudioUnitProperty_StreamFormat,
             *     kAudioUnitScope_Input, 0, &asbd, sizeof(asbd));
             * AudioUnitInitialize(mImpl->outputUnit);
             */

            return true;
        }

        void CoreAudioBackend::Shutdown() {
            Stop();
            // AudioUnitUninitialize(mImpl->outputUnit);
            // AudioComponentInstanceDispose(mImpl->outputUnit);
            mRunning = false;
        }

        void CoreAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void CoreAudioBackend::Start()  { mRunning = true;  /* AudioOutputUnitStart(mImpl->outputUnit); */ }
        void CoreAudioBackend::Stop()   { mRunning = false; /* AudioOutputUnitStop(mImpl->outputUnit);  */ }
        void CoreAudioBackend::Pause()  { Stop();   }
        void CoreAudioBackend::Resume() { Start();  }

#endif // NKENTSEU_PLATFORM_MACOS || NKENTSEU_PLATFORM_IOS

        // ====================================================================
        // ALSA BACKEND (Linux) — CORPS PRÊTS À CONNECTER
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_LINUX)

        struct AlsaAudioBackend::ThreadHandle {
            pthread_t thread = 0;
            bool      valid  = false;
        };

        void AlsaAudioBackend::ThreadFunc(void* userData) {
            AlsaAudioBackend* self = (AlsaAudioBackend*)userData;
            if (!self || !self->mPcm || !self->mBuffer) return;

            while (self->mRunning) {
                if (!self->mPaused && self->mCallback) {
                    memset(self->mBuffer, 0, (usize)self->mBufferSize * (usize)self->mChannels * sizeof(float32));
                    self->mCallback(self->mBuffer, self->mBufferSize, self->mChannels);

                    /*
                     * CONNEXION RÉELLE :
                     * snd_pcm_t* pcm = (snd_pcm_t*)self->mPcm;
                     * int err = snd_pcm_writei(pcm, self->mBuffer, (snd_pcm_uframes_t)self->mBufferSize);
                     * if (err == -EPIPE) snd_pcm_prepare(pcm); // XRUN recovery
                     */
                }
                usleep((useconds_t)((float32)self->mBufferSize / (float32)self->mSampleRate * 1e6f * 0.5f));
            }
        }

        AlsaAudioBackend::AlsaAudioBackend() = default;
        AlsaAudioBackend::~AlsaAudioBackend() { if (mRunning) Shutdown(); }

        bool AlsaAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate; mChannels = channels; mBufferSize = bufferSize;

            /*
             * CONNEXION RÉELLE :
             * snd_pcm_t* pcm = nullptr;
             * snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
             *
             * snd_pcm_hw_params_t* hw;
             * snd_pcm_hw_params_alloca(&hw);
             * snd_pcm_hw_params_any(pcm, hw);
             * snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
             * snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
             * snd_pcm_hw_params_set_rate(pcm, hw, (unsigned int)sampleRate, 0);
             * snd_pcm_hw_params_set_channels(pcm, hw, (unsigned int)channels);
             * snd_pcm_hw_params_set_period_size(pcm, hw, (snd_pcm_uframes_t)bufferSize, 0);
             * snd_pcm_hw_params(pcm, hw);
             * snd_pcm_prepare(pcm);
             * mPcm = pcm;
             */

            mBuffer = (float32*)::operator new((usize)bufferSize * (usize)channels * sizeof(float32));
            mThread = new ThreadHandle{};
            return true;
        }

        void AlsaAudioBackend::Shutdown() {
            Stop();
            ::operator delete(mBuffer); mBuffer = nullptr;
            delete mThread; mThread = nullptr;
            // snd_pcm_close((snd_pcm_t*)mPcm);
            mPcm = nullptr;
        }

        void AlsaAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }

        void AlsaAudioBackend::Start() {
            if (mRunning || !mThread) return;
            mRunning = true;
            mPaused  = false;
            pthread_create(&mThread->thread, nullptr,
                [](void* p) -> void* { ThreadFunc(p); return nullptr; }, this);
            mThread->valid = true;
        }

        void AlsaAudioBackend::Stop() {
            if (!mRunning) return;
            mRunning = false;
            if (mThread && mThread->valid) {
                pthread_join(mThread->thread, nullptr);
                mThread->valid = false;
            }
        }

        void AlsaAudioBackend::Pause()  { mPaused = true;  }
        void AlsaAudioBackend::Resume() { mPaused = false; }

#endif // NKENTSEU_PLATFORM_LINUX

        // ====================================================================
        // AAUDIO (Android) — CORPS PRÊTS À CONNECTER
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_ANDROID)

        struct AAudioBackend::AAudioImpl {
            // AAudioStream* stream = nullptr;
        };

        bool AAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
            mSampleRate = sr; mChannels = ch; mBufferSize = buf;
            mImpl = new AAudioImpl{};

            /*
             * CONNEXION RÉELLE :
             * AAudioStreamBuilder* builder = nullptr;
             * AAudio_createStreamBuilder(&builder);
             * AAudioStreamBuilder_setSampleRate(builder, sr);
             * AAudioStreamBuilder_setChannelCount(builder, ch);
             * AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
             * AAudioStreamBuilder_setBufferCapacityInFrames(builder, buf * 4);
             * AAudioStreamBuilder_setDataCallback(builder, AAudioDataCallback, this);
             * AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
             * AAudioStreamBuilder_openStream(builder, &mImpl->stream);
             * AAudioStreamBuilder_delete(builder);
             */

            return true;
        }

        void AAudioBackend::Shutdown() {
            Stop();
            // AAudioStream_close(mImpl->stream);
            delete mImpl; mImpl = nullptr;
        }

        void AAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void AAudioBackend::Start()  { mRunning = true;  /* AAudioStream_requestStart(mImpl->stream); */ }
        void AAudioBackend::Stop()   { mRunning = false; /* AAudioStream_requestStop(mImpl->stream);  */ }
        void AAudioBackend::Pause()  { Stop();  }
        void AAudioBackend::Resume() { Start(); }

#endif // NKENTSEU_PLATFORM_ANDROID

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUTO-ENREGISTREMENT DES BACKENDS DANS LA FACTORY
// ============================================================

namespace {
    struct NkAudioBackendAutoRegister {
        NkAudioBackendAutoRegister() {
            using namespace nkentseu::audio;

            // Null — toujours disponible (fallback)
            AudioBackendFactory::Register("Null",
                []() -> IAudioBackend* { return new NullAudioBackend(); });

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            AudioBackendFactory::Register("WASAPI",
                []() -> IAudioBackend* { return new WasapiAudioBackend(); });
            AudioBackendFactory::Register("DirectSound",
                []() -> IAudioBackend* { return new DirectSoundAudioBackend(); });
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
            AudioBackendFactory::Register("CoreAudio",
                []() -> IAudioBackend* { return new CoreAudioBackend(); });
#endif

#if defined(NKENTSEU_PLATFORM_LINUX)
            AudioBackendFactory::Register("ALSA",
                []() -> IAudioBackend* { return new AlsaAudioBackend(); });
#endif

#if defined(NKENTSEU_PLATFORM_ANDROID)
            AudioBackendFactory::Register("AAudio",
                []() -> IAudioBackend* { return new AAudioBackend(); });
#endif
        }
    };

    /// Instance statique — constructeur exécuté avant main()
    static NkAudioBackendAutoRegister gNkAudioBackendAutoRegister;
} // anonymous namespace

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
