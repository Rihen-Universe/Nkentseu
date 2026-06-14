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
#include "NKLogger/NkLog.h"

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

            mBuffer = (float32*)memory::NkAlloc(
                (usize)bufferSize * (usize)channels * sizeof(float32));
            if (!mBuffer) return false;

            mThread = new ThreadHandle{};
            return true;
        }

        void NullAudioBackend::Shutdown() {
            Stop();
            if (mBuffer) { memory::NkFree(mBuffer); mBuffer = nullptr; }
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
        // WASAPI BACKEND (Windows) — IMPLEMENTATION REELLE 2026-05-21
        // --------------------------------------------------------------------
        // Pipeline shared-mode + event-driven (basse latence) :
        //  1. CoInitializeEx + IMMDeviceEnumerator -> device de rendu par defaut
        //  2. IAudioClient::Initialize(AUDCLNT_STREAMFLAGS_EVENTCALLBACK)
        //  3. SetEventHandle pour reveil dirige par le driver
        //  4. Thread audio :
        //       WaitForSingleObject(event) puis GetCurrentPadding,
        //       GetBuffer, callback de l'app, ReleaseBuffer
        //  5. Shutdown : Stop client + Release COM objects + CoUninitialize
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>

        struct WasapiAudioBackend::WasapiImpl {
            IMMDeviceEnumerator* enumerator   = nullptr;
            IMMDevice*           device       = nullptr;
            IAudioClient*        client       = nullptr;
            IAudioRenderClient*  renderer     = nullptr;
            HANDLE               event        = nullptr;
            HANDLE               thread       = nullptr;
            UINT32               bufferFrames = 0;   ///< Capacite reelle du buffer WASAPI
            bool                 coInitialized = false;
        };

        WasapiAudioBackend::WasapiAudioBackend() : mImpl(new WasapiImpl{}) { }
        WasapiAudioBackend::~WasapiAudioBackend() {
            if (mRunning) Shutdown();
            delete mImpl;
        }

        // Thread audio : appelle mCallback chaque fois que le driver signale
        // qu'il y a de la place dans son buffer (event-driven, latence min).
        static DWORD WINAPI WasapiThreadProc(LPVOID userData) {
            WasapiAudioBackend* self = (WasapiAudioBackend*)userData;
            if (!self) return 0;
            WasapiAudioBackend::WasapiImpl* impl = self->GetImpl();

            // Hint MMCSS : tag ce thread comme Audio Pro (prio realtime).
            DWORD taskIndex = 0;
            HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

            const int32 channels   = self->GetChannels();
            const UINT32 bufFrames = impl->bufferFrames;

            while (self->IsRunning()) {
                // Attente du signal driver (timeout 2s = safety).
                DWORD wr = WaitForSingleObject(impl->event, 2000);
                if (wr != WAIT_OBJECT_0) continue;
                if (!self->IsRunning()) break;
                if (self->IsPaused()) continue;

                // Combien de frames le driver a deja consommees ?
                UINT32 padding = 0;
                if (FAILED(impl->client->GetCurrentPadding(&padding))) continue;
                UINT32 framesAvail = bufFrames - padding;
                if (framesAvail == 0) continue;

                // Demande un buffer d'ecriture du driver.
                BYTE* data = nullptr;
                if (FAILED(impl->renderer->GetBuffer(framesAvail, &data))) continue;
                if (!data) continue;

                // Remplit via le callback applicatif (float interleaved).
                if (self->GetCallback()) {
                    self->GetCallback()((float*)data, (int32)framesAvail, channels);
                } else {
                    // Pas de callback : zero pour eviter du bruit.
                    memset(data, 0, (usize)framesAvail * (usize)channels * sizeof(float));
                }
                impl->renderer->ReleaseBuffer(framesAvail, 0);
            }

            if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
            return 0;
        }

        bool WasapiAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate;
            mChannels   = channels;
            mBufferSize = bufferSize;

            // 1. COM init (multithread car notre thread audio sera dedie).
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (SUCCEEDED(hr)) mImpl->coInitialized = true;
            // RPC_E_CHANGED_MODE = deja init en STA, OK on continue.

            // 2. Enumerateur + device de rendu par defaut (haut-parleurs).
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&mImpl->enumerator);
            if (FAILED(hr)) { logger.Error("[WASAPI] CoCreateInstance MMDeviceEnumerator FAILED hr=0x{0:X}", (unsigned)hr); return false; }
            hr = mImpl->enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                            &mImpl->device);
            if (FAILED(hr)) { logger.Error("[WASAPI] GetDefaultAudioEndpoint FAILED hr=0x{0:X}", (unsigned)hr); return false; }

            // 3. AudioClient.
            hr = mImpl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                         nullptr, (void**)&mImpl->client);
            if (FAILED(hr)) { logger.Error("[WASAPI] Activate IAudioClient FAILED hr=0x{0:X}", (unsigned)hr); return false; }

            // 4. Format float32 interleaved (notre format interne).
            // WASAPI shared-mode utilise le format du mix engine. Si on tente
            // un format different, on peut avoir AUDCLNT_E_UNSUPPORTED_FORMAT.
            // On recupere donc le mix format du device et on s'aligne dessus.
            WAVEFORMATEX* mixFormat = nullptr;
            hr = mImpl->client->GetMixFormat(&mixFormat);
            if (FAILED(hr) || !mixFormat) { logger.Error("[WASAPI] GetMixFormat FAILED hr=0x{0:X}", (unsigned)hr); return false; }
            // Force le format en Float32 interleaved au sample rate du mix.
            // Cela garantit le moins de conversion.
            mSampleRate = (int32)mixFormat->nSamplesPerSec;
            mChannels   = (int32)mixFormat->nChannels;
            channels    = mChannels;
            sampleRate  = mSampleRate;

            WAVEFORMATEX wfx = {};
            wfx.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
            wfx.nChannels      = (WORD)channels;
            wfx.nSamplesPerSec = (DWORD)sampleRate;
            wfx.wBitsPerSample = 32;
            wfx.nBlockAlign    = (WORD)(wfx.nChannels * (wfx.wBitsPerSample / 8));
            wfx.nAvgBytesPerSec= wfx.nSamplesPerSec * wfx.nBlockAlign;
            wfx.cbSize         = 0;
            CoTaskMemFree(mixFormat);
            logger.Info("[WASAPI] Format negocie : {0} Hz, {1} canaux, Float32",
                        sampleRate, channels);

            // Duree du buffer voulu en 100ns units (REFERENCE_TIME).
            REFERENCE_TIME desired =
                (REFERENCE_TIME)((double)bufferSize / sampleRate * 1e7);

            hr = mImpl->client->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                desired, 0, &wfx, nullptr);
            if (FAILED(hr)) { logger.Error("[WASAPI] IAudioClient::Initialize FAILED hr=0x{0:X}", (unsigned)hr); return false; }

            // Taille reelle du buffer WASAPI (peut differ du desired).
            hr = mImpl->client->GetBufferSize(&mImpl->bufferFrames);
            if (FAILED(hr)) { logger.Error("[WASAPI] GetBufferSize FAILED hr=0x{0:X}", (unsigned)hr); return false; }
            logger.Info("[WASAPI] Buffer effectif : {0} frames", (int)mImpl->bufferFrames);

            // 5. Event handle pour reveil dirige par le driver.
            mImpl->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!mImpl->event) { logger.Error("[WASAPI] CreateEventW FAILED"); return false; }
            hr = mImpl->client->SetEventHandle(mImpl->event);
            if (FAILED(hr)) { logger.Error("[WASAPI] SetEventHandle FAILED hr=0x{0:X}", (unsigned)hr); return false; }

            // 6. Service renderer pour ecrire les samples.
            hr = mImpl->client->GetService(__uuidof(IAudioRenderClient),
                                           (void**)&mImpl->renderer);
            if (FAILED(hr)) { logger.Error("[WASAPI] GetService AudioRenderClient FAILED hr=0x{0:X}", (unsigned)hr); return false; }
            logger.Info("[WASAPI] Initialize OK");
            return true;
        }

        void WasapiAudioBackend::Shutdown() {
            Stop();
            if (mImpl->renderer)   { mImpl->renderer->Release();   mImpl->renderer   = nullptr; }
            if (mImpl->client)     { mImpl->client->Release();     mImpl->client     = nullptr; }
            if (mImpl->device)     { mImpl->device->Release();     mImpl->device     = nullptr; }
            if (mImpl->enumerator) { mImpl->enumerator->Release(); mImpl->enumerator = nullptr; }
            if (mImpl->event)      { CloseHandle(mImpl->event);    mImpl->event      = nullptr; }
            if (mImpl->coInitialized) { CoUninitialize(); mImpl->coInitialized = false; }
            mRunning = false;
        }

        void WasapiAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }

        void WasapiAudioBackend::Start() {
            if (mRunning) return;
            if (!mImpl->client) return;
            mRunning = true;
            // Pre-fill avec du silence pour eviter un click au demarrage.
            BYTE* data = nullptr;
            if (SUCCEEDED(mImpl->renderer->GetBuffer(mImpl->bufferFrames, &data)) && data) {
                memset(data, 0, (usize)mImpl->bufferFrames * (usize)mChannels * sizeof(float));
                mImpl->renderer->ReleaseBuffer(mImpl->bufferFrames, 0);
            }
            mImpl->client->Start();
            mImpl->thread = CreateThread(nullptr, 0, WasapiThreadProc, this, 0, nullptr);
        }

        void WasapiAudioBackend::Stop() {
            if (!mRunning) return;
            mRunning = false;
            if (mImpl->thread) {
                // Reveille le thread pour qu'il sorte de WaitForSingleObject.
                if (mImpl->event) SetEvent(mImpl->event);
                WaitForSingleObject(mImpl->thread, 3000);
                CloseHandle(mImpl->thread);
                mImpl->thread = nullptr;
            }
            if (mImpl->client) mImpl->client->Stop();
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

#include <AudioToolbox/AudioToolbox.h>

        struct CoreAudioBackend::CoreAudioImpl {
            AudioUnit outputUnit = nullptr;
        };

        CoreAudioBackend::CoreAudioBackend() : mImpl(new CoreAudioImpl{}) { }
        CoreAudioBackend::~CoreAudioBackend() { if (mRunning) Shutdown(); delete mImpl; }

        // Render callback CoreAudio : appele par AudioUnit pour remplir le
        // buffer. Le format ASBD impose Float32 interleaved => on ecrit
        // directement dans ioData->mBuffers[0].mData.
        static OSStatus CoreAudioRenderCallback(
            void* refCon, AudioUnitRenderActionFlags* /*flags*/,
            const AudioTimeStamp* /*ts*/, UInt32 /*busNum*/,
            UInt32 inNumberFrames, AudioBufferList* ioData)
        {
            CoreAudioBackend* self = (CoreAudioBackend*)refCon;
            if (!self || !ioData || ioData->mNumberBuffers == 0) return noErr;
            float* out = (float*)ioData->mBuffers[0].mData;
            const int32 channels = self->GetChannels();
            const int32 frames   = (int32)inNumberFrames;
            if (!self->IsRunning() || self->IsPaused()) {
                memset(out, 0, (usize)frames * (usize)channels * sizeof(float));
                return noErr;
            }
            const auto& cb = self->GetCallback();
            if (cb) {
                cb(out, frames, channels);
            } else {
                memset(out, 0, (usize)frames * (usize)channels * sizeof(float));
            }
            return noErr;
        }

        bool CoreAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate; mChannels = channels; mBufferSize = bufferSize;

            AudioComponentDescription desc = {};
            desc.componentType = kAudioUnitType_Output;
#if defined(NKENTSEU_PLATFORM_IOS)
            desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
            desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;

            AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
            if (!comp) return false;
            if (AudioComponentInstanceNew(comp, &mImpl->outputUnit) != noErr) return false;

            // Format float32 interleaved.
            AudioStreamBasicDescription asbd = {};
            asbd.mSampleRate       = (Float64)sampleRate;
            asbd.mFormatID         = kAudioFormatLinearPCM;
            asbd.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            asbd.mChannelsPerFrame = (UInt32)channels;
            asbd.mBitsPerChannel   = 32;
            asbd.mFramesPerPacket  = 1;
            asbd.mBytesPerFrame    = asbd.mChannelsPerFrame * sizeof(float);
            asbd.mBytesPerPacket   = asbd.mBytesPerFrame;
            if (AudioUnitSetProperty(mImpl->outputUnit,
                    kAudioUnitProperty_StreamFormat,
                    kAudioUnitScope_Input, 0, &asbd, sizeof(asbd)) != noErr) {
                return false;
            }

            AURenderCallbackStruct cb;
            cb.inputProc       = CoreAudioRenderCallback;
            cb.inputProcRefCon = this;
            if (AudioUnitSetProperty(mImpl->outputUnit,
                    kAudioUnitProperty_SetRenderCallback,
                    kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr) {
                return false;
            }

            if (AudioUnitInitialize(mImpl->outputUnit) != noErr) return false;
            return true;
        }

        void CoreAudioBackend::Shutdown() {
            Stop();
            if (mImpl && mImpl->outputUnit) {
                AudioUnitUninitialize(mImpl->outputUnit);
                AudioComponentInstanceDispose(mImpl->outputUnit);
                mImpl->outputUnit = nullptr;
            }
            mRunning = false;
        }

        void CoreAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void CoreAudioBackend::Start()  {
            if (!mImpl || !mImpl->outputUnit) return;
            mRunning = true; mPaused = false;
            AudioOutputUnitStart(mImpl->outputUnit);
        }
        void CoreAudioBackend::Stop()   {
            mRunning = false;
            if (mImpl && mImpl->outputUnit) AudioOutputUnitStop(mImpl->outputUnit);
        }
        void CoreAudioBackend::Pause()  { mPaused = true;  if (mImpl && mImpl->outputUnit) AudioOutputUnitStop (mImpl->outputUnit); }
        void CoreAudioBackend::Resume() { mPaused = false; if (mImpl && mImpl->outputUnit) AudioOutputUnitStart(mImpl->outputUnit); }

#endif // NKENTSEU_PLATFORM_MACOS || NKENTSEU_PLATFORM_IOS

        // ====================================================================
        // ALSA BACKEND (Linux) — CORPS PRÊTS À CONNECTER
        //
        // NB : HarmonyOS hérite de NKENTSEU_PLATFORM_LINUX (chemins POSIX) mais
        // n'a pas ALSA — il faut donc l'exclure explicitement. Android idem.
        // ====================================================================

#if defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_HARMONYOS) && !defined(NKENTSEU_PLATFORM_ANDROID)

#include <alsa/asoundlib.h>

        struct AlsaAudioBackend::ThreadHandle {
            pthread_t thread = 0;
            bool      valid  = false;
        };

        void AlsaAudioBackend::ThreadFunc(void* userData) {
            AlsaAudioBackend* self = (AlsaAudioBackend*)userData;
            if (!self || !self->mPcm || !self->mBuffer) return;
            snd_pcm_t* pcm = (snd_pcm_t*)self->mPcm;

            while (self->mRunning) {
                if (!self->mPaused && self->mCallback) {
                    memset(self->mBuffer, 0, (usize)self->mBufferSize * (usize)self->mChannels * sizeof(float32));
                    self->mCallback(self->mBuffer, self->mBufferSize, self->mChannels);
                    // Ecriture bloquante. snd_pcm_writei retourne le nombre de
                    // frames ecrites, ou erreur negative (XRUN: -EPIPE).
                    snd_pcm_sframes_t written = snd_pcm_writei(
                        pcm, self->mBuffer, (snd_pcm_uframes_t)self->mBufferSize);
                    if (written < 0) {
                        // Recovery underrun. snd_pcm_recover gere EPIPE/ESTRPIPE.
                        snd_pcm_recover(pcm, (int)written, /*silent*/ 1);
                    }
                } else {
                    // Pause : on dort une periode pour ne pas brule CPU.
                    usleep((useconds_t)((float32)self->mBufferSize
                            / (float32)self->mSampleRate * 1e6f));
                }
            }
        }

        AlsaAudioBackend::AlsaAudioBackend() = default;
        AlsaAudioBackend::~AlsaAudioBackend() { if (mRunning) Shutdown(); }

        bool AlsaAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate; mChannels = channels; mBufferSize = bufferSize;

            snd_pcm_t* pcm = nullptr;
            if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
                return false;
            }

            snd_pcm_hw_params_t* hw = nullptr;
            snd_pcm_hw_params_alloca(&hw);
            snd_pcm_hw_params_any(pcm, hw);
            snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
            snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
            snd_pcm_hw_params_set_rate(pcm, hw, (unsigned int)sampleRate, 0);
            snd_pcm_hw_params_set_channels(pcm, hw, (unsigned int)channels);
            snd_pcm_uframes_t period = (snd_pcm_uframes_t)bufferSize;
            snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
            snd_pcm_uframes_t bufFrames = period * 4;   // 4 periodes buffer
            snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &bufFrames);
            if (snd_pcm_hw_params(pcm, hw) < 0) {
                snd_pcm_close(pcm);
                return false;
            }
            snd_pcm_prepare(pcm);
            mPcm = pcm;

            mBuffer = (float32*)memory::NkAlloc(
                (usize)bufferSize * (usize)channels * sizeof(float32));
            if (!mBuffer) { snd_pcm_close(pcm); mPcm = nullptr; return false; }
            mThread = new ThreadHandle{};
            return true;
        }

        void AlsaAudioBackend::Shutdown() {
            Stop();
            if (mPcm) {
                snd_pcm_drop((snd_pcm_t*)mPcm);
                snd_pcm_close((snd_pcm_t*)mPcm);
                mPcm = nullptr;
            }
            if (mBuffer) { memory::NkFree(mBuffer); mBuffer = nullptr; }
            delete mThread; mThread = nullptr;
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

// AAudio est dispo Android 26+. Pong cible minSdk 24 (compat large) -> on
// charge libaaudio.so via dlopen/dlsym au runtime. Sur 24-25 : Initialize
// retourne false, AudioEngine fallback sur Null backend (silencieux).

#include <dlfcn.h>
#include <android/api-level.h>

// Forward-declare les types AAudio sans inclure le header system (qui erreur
// quand __ANDROID_API__ < 26). Layout binaire-compatible avec la lib.
extern "C" {
    typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
    typedef struct AAudioStreamStruct        AAudioStream;
    typedef int32_t aaudio_result_t;
    typedef int32_t aaudio_data_callback_result_t;
    typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
}
static constexpr aaudio_result_t                AAUDIO_OK_                    = 0;
static constexpr int32_t                        AAUDIO_FORMAT_PCM_FLOAT_      = 2;
static constexpr int32_t                        AAUDIO_PERFORMANCE_LOW_LAT_   = 12;
static constexpr int32_t                        AAUDIO_SHARING_SHARED_        = 1;
static constexpr aaudio_data_callback_result_t  AAUDIO_CALLBACK_CONTINUE_     = 0;

// Function pointers chargees via dlsym.
namespace {
    using PFN_createBuilder    = aaudio_result_t (*)(AAudioStreamBuilder**);
    using PFN_setSampleRate    = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setChannelCount  = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setFormat        = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setBufferCap     = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setFramesPerCb   = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setDataCallback  = void (*)(AAudioStreamBuilder*, AAudioStream_dataCallback, void*);
    using PFN_setPerformance   = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_setSharing       = void (*)(AAudioStreamBuilder*, int32_t);
    using PFN_openStream       = aaudio_result_t (*)(AAudioStreamBuilder*, AAudioStream**);
    using PFN_builderDelete    = aaudio_result_t (*)(AAudioStreamBuilder*);
    using PFN_streamClose      = aaudio_result_t (*)(AAudioStream*);
    using PFN_streamStart      = aaudio_result_t (*)(AAudioStream*);
    using PFN_streamStop       = aaudio_result_t (*)(AAudioStream*);
    using PFN_streamPause      = aaudio_result_t (*)(AAudioStream*);
    using PFN_getSampleRate    = int32_t (*)(AAudioStream*);
    using PFN_getChannelCount  = int32_t (*)(AAudioStream*);

    struct AAudioFns {
        void* lib = nullptr;
        PFN_createBuilder    createBuilder    = nullptr;
        PFN_setSampleRate    setSampleRate    = nullptr;
        PFN_setChannelCount  setChannelCount  = nullptr;
        PFN_setFormat        setFormat        = nullptr;
        PFN_setBufferCap     setBufferCap     = nullptr;
        PFN_setFramesPerCb   setFramesPerCb   = nullptr;
        PFN_setDataCallback  setDataCallback  = nullptr;
        PFN_setPerformance   setPerformance   = nullptr;
        PFN_setSharing       setSharing       = nullptr;
        PFN_openStream       openStream       = nullptr;
        PFN_builderDelete    builderDelete    = nullptr;
        PFN_streamClose      streamClose      = nullptr;
        PFN_streamStart      streamStart      = nullptr;
        PFN_streamStop       streamStop       = nullptr;
        PFN_streamPause      streamPause      = nullptr;
        PFN_getSampleRate    getSampleRate    = nullptr;
        PFN_getChannelCount  getChannelCount  = nullptr;
        bool available = false;
    };
    static AAudioFns gAAudio;

    // Charge libaaudio.so + symbols. Idempotent. Retourne true si dispo.
    bool LoadAAudio() {
        if (gAAudio.lib) return gAAudio.available;
        // Verif API level systeme (24 ou 25 -> pas d'AAudio).
        if (android_get_device_api_level() < 26) return false;
        gAAudio.lib = dlopen("libaaudio.so", RTLD_NOW | RTLD_LOCAL);
        if (!gAAudio.lib) return false;
        #define LOAD(name, sym) gAAudio.name = (PFN_##name)dlsym(gAAudio.lib, sym)
        LOAD(createBuilder,    "AAudio_createStreamBuilder");
        LOAD(setSampleRate,    "AAudioStreamBuilder_setSampleRate");
        LOAD(setChannelCount,  "AAudioStreamBuilder_setChannelCount");
        LOAD(setFormat,        "AAudioStreamBuilder_setFormat");
        LOAD(setBufferCap,     "AAudioStreamBuilder_setBufferCapacityInFrames");
        LOAD(setFramesPerCb,   "AAudioStreamBuilder_setFramesPerDataCallback");
        LOAD(setDataCallback,  "AAudioStreamBuilder_setDataCallback");
        LOAD(setPerformance,   "AAudioStreamBuilder_setPerformanceMode");
        LOAD(setSharing,       "AAudioStreamBuilder_setSharingMode");
        LOAD(openStream,       "AAudioStreamBuilder_openStream");
        LOAD(builderDelete,    "AAudioStreamBuilder_delete");
        LOAD(streamClose,      "AAudioStream_close");
        LOAD(streamStart,      "AAudioStream_requestStart");
        LOAD(streamStop,       "AAudioStream_requestStop");
        LOAD(streamPause,      "AAudioStream_requestPause");
        LOAD(getSampleRate,    "AAudioStream_getSampleRate");
        LOAD(getChannelCount,  "AAudioStream_getChannelCount");
        #undef LOAD
        // Check que les symboles minimum sont presents.
        gAAudio.available =
            (gAAudio.createBuilder && gAAudio.openStream
             && gAAudio.streamStart && gAAudio.streamClose);
        return gAAudio.available;
    }
} // anonymous namespace

        struct AAudioBackend::AAudioImpl {
            AAudioStream* stream = nullptr;
        };

        // Callback AAudio : appele par le driver pour remplir le buffer.
        static aaudio_data_callback_result_t AAudioDataCallback(
            AAudioStream* /*stream*/, void* userData,
            void* audioData, int32_t numFrames)
        {
            AAudioBackend* self = (AAudioBackend*)userData;
            if (!self || !self->IsRunning() || self->IsPaused()) {
                memset(audioData, 0,
                       (usize)numFrames * (usize)self->GetChannels() * sizeof(float));
                return AAUDIO_CALLBACK_CONTINUE_;
            }
            const auto& cb = self->GetCallback();
            if (cb) {
                cb((float*)audioData, (int32)numFrames, self->GetChannels());
            } else {
                memset(audioData, 0,
                       (usize)numFrames * (usize)self->GetChannels() * sizeof(float));
            }
            return AAUDIO_CALLBACK_CONTINUE_;
        }

        bool AAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
            mSampleRate = sr; mChannels = ch; mBufferSize = buf;
            if (!LoadAAudio()) return false;
            mImpl = new AAudioImpl{};

            AAudioStreamBuilder* builder = nullptr;
            if (gAAudio.createBuilder(&builder) != AAUDIO_OK_) {
                delete mImpl; mImpl = nullptr;
                return false;
            }
            gAAudio.setSampleRate  (builder, sr);
            gAAudio.setChannelCount(builder, ch);
            gAAudio.setFormat      (builder, AAUDIO_FORMAT_PCM_FLOAT_);
            gAAudio.setBufferCap   (builder, buf * 4);
            gAAudio.setFramesPerCb (builder, buf);
            gAAudio.setDataCallback(builder, AAudioDataCallback, this);
            gAAudio.setPerformance (builder, AAUDIO_PERFORMANCE_LOW_LAT_);
            gAAudio.setSharing     (builder, AAUDIO_SHARING_SHARED_);

            aaudio_result_t r = gAAudio.openStream(builder, &mImpl->stream);
            gAAudio.builderDelete(builder);
            if (r != AAUDIO_OK_ || !mImpl->stream) {
                delete mImpl; mImpl = nullptr;
                return false;
            }
            mSampleRate = gAAudio.getSampleRate(mImpl->stream);
            mChannels   = gAAudio.getChannelCount(mImpl->stream);
            return true;
        }

        void AAudioBackend::Shutdown() {
            Stop();
            if (mImpl && mImpl->stream && gAAudio.streamClose) {
                gAAudio.streamClose(mImpl->stream);
                mImpl->stream = nullptr;
            }
            delete mImpl; mImpl = nullptr;
        }

        void AAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void AAudioBackend::Start()  {
            if (!mImpl || !mImpl->stream || !gAAudio.streamStart) return;
            mRunning = true; mPaused = false;
            gAAudio.streamStart(mImpl->stream);
        }
        void AAudioBackend::Stop()   {
            mRunning = false;
            if (mImpl && mImpl->stream && gAAudio.streamStop)
                gAAudio.streamStop(mImpl->stream);
        }
        void AAudioBackend::Pause()  {
            mPaused = true;
            if (mImpl && mImpl->stream && gAAudio.streamPause)
                gAAudio.streamPause(mImpl->stream);
        }
        void AAudioBackend::Resume() {
            mPaused = false;
            if (mImpl && mImpl->stream && gAAudio.streamStart)
                gAAudio.streamStart(mImpl->stream);
        }

// ====================================================================
// OpenSL ES backend (Android 4.0+) — fallback pour Android 24-25
// --------------------------------------------------------------------
// NKAudio compile en C++17 (cf NKAudio.jenga) car C++20 strict + NDK r27
// jni.h font conflit sur va_list&. C++17 accepte sans probleme.
// ====================================================================

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

        struct OpenSLESAudioBackend::SLImpl {
            SLObjectItf engineObj  = nullptr;
            SLEngineItf engine     = nullptr;
            SLObjectItf outputMix  = nullptr;
            SLObjectItf playerObj  = nullptr;
            SLPlayItf   player     = nullptr;
            SLAndroidSimpleBufferQueueItf queue = nullptr;
            // Double-buffer int16 (2 buffers alternes via index).
            int16_t* buffers[2] = { nullptr, nullptr };
            int32    nextBuf  = 0;
            int32    framesPerBuffer = 0;
            int32    channels        = 2;
            // Workspace float : on appelle mCallback ici, puis on convertit
            // en int16 dans le buffer enqueue (OpenSL ES ne supporte pas
            // float natif avant Android 5.0).
            float*   floatBuf = nullptr;
        };

        // Callback OpenSL ES : appele quand le driver a fini de jouer un
        // buffer. On genere le suivant via mCallback (float32) puis on le
        // convertit en int16 et on l'enqueue.
        static void OpenSLBufferCallback(SLAndroidSimpleBufferQueueItf bq, void* context)
        {
            OpenSLESAudioBackend* self = (OpenSLESAudioBackend*)context;
            if (!self || !self->IsRunning()) return;
            OpenSLESAudioBackend::SLImpl* impl = self->GetImpl();
            if (!impl || !impl->floatBuf) return;

            const int32 frames   = impl->framesPerBuffer;
            const int32 channels = impl->channels;
            const usize sampleCount = (usize)frames * (usize)channels;
            const auto& cb = self->GetCallback();

            // Genere les samples float dans le workspace.
            if (cb && !self->IsPaused()) {
                cb(impl->floatBuf, frames, channels);
            } else {
                memset(impl->floatBuf, 0, sampleCount * sizeof(float));
            }
            // Convertit float [-1..1] -> int16 [-32768..32767] dans le buffer
            // a enqueue. Buffer alternates entre buffers[0] et buffers[1].
            int16_t* out = impl->buffers[impl->nextBuf];
            impl->nextBuf ^= 1;
            for (usize i = 0; i < sampleCount; ++i) {
                float v = impl->floatBuf[i];
                if (v >  1.0f) v =  1.0f;
                if (v < -1.0f) v = -1.0f;
                out[i] = (int16_t)(v * 32767.0f);
            }
            (*bq)->Enqueue(bq, out,
                           (SLuint32)(sampleCount * sizeof(int16_t)));
        }

        OpenSLESAudioBackend::OpenSLESAudioBackend() = default;
        OpenSLESAudioBackend::~OpenSLESAudioBackend() { if (mRunning) Shutdown(); }

        bool OpenSLESAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
            mSampleRate = sr; mChannels = ch; mBufferSize = buf;
            mImpl = new SLImpl{};
            mImpl->framesPerBuffer = buf;
            mImpl->channels        = ch;

            // 1. Engine.
            if (slCreateEngine(&mImpl->engineObj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) {
                Shutdown(); return false;
            }
            (*mImpl->engineObj)->Realize(mImpl->engineObj, SL_BOOLEAN_FALSE);
            (*mImpl->engineObj)->GetInterface(mImpl->engineObj, SL_IID_ENGINE, &mImpl->engine);

            // 2. Output mix.
            (*mImpl->engine)->CreateOutputMix(mImpl->engine, &mImpl->outputMix, 0, nullptr, nullptr);
            (*mImpl->outputMix)->Realize(mImpl->outputMix, SL_BOOLEAN_FALSE);

            // 3. Audio player : Android Simple BufferQueue source -> OutputMix sink.
            SLDataLocator_AndroidSimpleBufferQueue locBQ = {
                SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2  // 2 buffers (double-buffering)
            };
            SLDataFormat_PCM fmt = {};
            fmt.formatType    = SL_DATAFORMAT_PCM;
            fmt.numChannels   = (SLuint32)ch;
            fmt.samplesPerSec = (SLuint32)(sr * 1000);   // OpenSL ES utilise mHz, pas Hz
            fmt.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
            fmt.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
            fmt.channelMask   = (ch == 1)
                ? SL_SPEAKER_FRONT_CENTER
                : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
            fmt.endianness    = SL_BYTEORDER_LITTLEENDIAN;
            SLDataSource src = { &locBQ, &fmt };

            SLDataLocator_OutputMix locOut = { SL_DATALOCATOR_OUTPUTMIX, mImpl->outputMix };
            SLDataSink sink = { &locOut, nullptr };

            const SLInterfaceID ids[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
            const SLboolean    req[] = { SL_BOOLEAN_TRUE };
            if ((*mImpl->engine)->CreateAudioPlayer(mImpl->engine, &mImpl->playerObj,
                    &src, &sink, 1, ids, req) != SL_RESULT_SUCCESS) {
                Shutdown(); return false;
            }
            (*mImpl->playerObj)->Realize(mImpl->playerObj, SL_BOOLEAN_FALSE);
            (*mImpl->playerObj)->GetInterface(mImpl->playerObj, SL_IID_PLAY, &mImpl->player);
            (*mImpl->playerObj)->GetInterface(mImpl->playerObj,
                                              SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &mImpl->queue);

            // 4. Buffers int16 (PCM) + workspace float pour le callback.
            const usize bytesPerBuf = (usize)buf * (usize)ch * sizeof(int16_t);
            mImpl->buffers[0] = (int16_t*)memory::NkAlloc(bytesPerBuf);
            mImpl->buffers[1] = (int16_t*)memory::NkAlloc(bytesPerBuf);
            mImpl->floatBuf   = (float*)memory::NkAlloc((usize)buf * (usize)ch * sizeof(float));
            if (!mImpl->buffers[0] || !mImpl->buffers[1] || !mImpl->floatBuf) {
                Shutdown(); return false;
            }
            memset(mImpl->buffers[0], 0, bytesPerBuf);
            memset(mImpl->buffers[1], 0, bytesPerBuf);

            // Enregistre le callback (context = this pointer pour acceder mImpl).
            (*mImpl->queue)->RegisterCallback(mImpl->queue, OpenSLBufferCallback, this);
            return true;
        }

        void OpenSLESAudioBackend::Shutdown() {
            Stop();
            if (mImpl) {
                if (mImpl->playerObj) { (*mImpl->playerObj)->Destroy(mImpl->playerObj); mImpl->playerObj = nullptr; }
                if (mImpl->outputMix) { (*mImpl->outputMix)->Destroy(mImpl->outputMix); mImpl->outputMix = nullptr; }
                if (mImpl->engineObj) { (*mImpl->engineObj)->Destroy(mImpl->engineObj); mImpl->engineObj = nullptr; }
                if (mImpl->buffers[0]) { memory::NkFree(mImpl->buffers[0]); mImpl->buffers[0] = nullptr; }
                if (mImpl->buffers[1]) { memory::NkFree(mImpl->buffers[1]); mImpl->buffers[1] = nullptr; }
                if (mImpl->floatBuf)   { memory::NkFree(mImpl->floatBuf);   mImpl->floatBuf   = nullptr; }
                delete mImpl; mImpl = nullptr;
            }
        }

        void OpenSLESAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }

        void OpenSLESAudioBackend::Start() {
            if (!mImpl || !mImpl->player || !mImpl->queue) return;
            mRunning = true; mPaused = false;
            // Pre-enqueue 2 buffers de silence pour amorcer la lecture.
            // Le callback prendra le relais pour re-enqueue dynamiquement.
            const usize bytes = (usize)mBufferSize * (usize)mChannels * sizeof(int16_t);
            (*mImpl->queue)->Enqueue(mImpl->queue, mImpl->buffers[0], (SLuint32)bytes);
            (*mImpl->queue)->Enqueue(mImpl->queue, mImpl->buffers[1], (SLuint32)bytes);
            (*mImpl->player)->SetPlayState(mImpl->player, SL_PLAYSTATE_PLAYING);
        }

        void OpenSLESAudioBackend::Stop() {
            mRunning = false;
            if (mImpl && mImpl->player) {
                (*mImpl->player)->SetPlayState(mImpl->player, SL_PLAYSTATE_STOPPED);
            }
            if (mImpl && mImpl->queue) {
                (*mImpl->queue)->Clear(mImpl->queue);
            }
        }

        void OpenSLESAudioBackend::Pause() {
            mPaused = true;
            if (mImpl && mImpl->player)
                (*mImpl->player)->SetPlayState(mImpl->player, SL_PLAYSTATE_PAUSED);
        }
        void OpenSLESAudioBackend::Resume() {
            mPaused = false;
            if (mImpl && mImpl->player)
                (*mImpl->player)->SetPlayState(mImpl->player, SL_PLAYSTATE_PLAYING);
        }

#endif // NKENTSEU_PLATFORM_ANDROID

// ====================================================================
// WEB AUDIO (Emscripten) — IMPLEMENTATION REELLE
// --------------------------------------------------------------------
// Approche :
//  1. EM_JS pour creer un AudioContext + ScriptProcessorNode (4096 frames)
//  2. Le ScriptProcessor JS reveille un callback C exporte EMSCRIPTEN_KEEPALIVE
//  3. Le callback C remplit un buffer float32 interleaved (notre format)
//  4. JS lit ce buffer + le copie planaire dans outputBuffer.getChannelData(c)
//
// Notes :
//  - ScriptProcessorNode est deprecated mais marche partout (vs AudioWorklet
//    qui requiert cross-origin isolation - COOP/COEP headers)
//  - Sample rate impose par le navigateur (typiquement 48000 ou 44100)
// ====================================================================

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include <emscripten/emscripten.h>
#include <emscripten/em_js.h>

        // Backend actif (singleton pour le bridge C/JS).
        static WebAudioBackend* gWebAudioActive = nullptr;

        // Callback C invoque depuis JS (cf EM_JS InitWebAudio). Recoit
        // frames a generer + buffer ptr ou ecrire (interleaved float32).
        extern "C" EMSCRIPTEN_KEEPALIVE
        void NkWebAudioFillBuffer(int frames, float* interleavedOut)
        {
            if (!gWebAudioActive || !gWebAudioActive->IsRunning()) {
                memset(interleavedOut, 0, frames * 2 * sizeof(float));
                return;
            }
            const auto& cb = gWebAudioActive->GetCallback();
            const int32 channels = gWebAudioActive->GetChannels();
            if (cb && !gWebAudioActive->IsPaused()) {
                cb(interleavedOut, frames, channels);
            } else {
                memset(interleavedOut, 0, frames * channels * sizeof(float));
            }
        }

        // EM_JS : code JavaScript inline qui s'execute cote navigateur.
        // Cree l'AudioContext + ScriptProcessor + cable le callback C.
        EM_JS(int, NkWebAudio_JSInit, (int sampleRate, int channels, int bufferSize), {
            // Cree ou reutilise l'AudioContext global (Chrome limite a 6).
            if (!window.NkAudioCtx) {
                var Ctx = window.AudioContext || window.webkitAudioContext;
                if (!Ctx) return 0;
                window.NkAudioCtx = new Ctx({ sampleRate: sampleRate });
            }
            var ctx = window.NkAudioCtx;
            // Reveille le contexte si suspendu (politique autoplay nav).
            if (ctx.state === 'suspended') ctx.resume();
            // Cree le ScriptProcessor : bufferSize doit etre puissance de 2
            // entre 256 et 16384. On clamp au plus proche.
            var bs = bufferSize;
            if (bs < 256)   bs = 256;
            if (bs > 16384) bs = 16384;
            // Round to next power of 2.
            bs = Math.pow(2, Math.ceil(Math.log2(bs)));
            var node = ctx.createScriptProcessor(bs, 0, channels);
            // Heap interleaved buffer alloue cote C, mapped vers Module.HEAPF32.
            var heapPtr = _malloc(bs * channels * 4);
            window.NkAudioHeapPtr  = heapPtr;
            window.NkAudioHeapSize = bs * channels;
            node.onaudioprocess = function(e) {
                // Appelle le callback C (ecrit interleaved float32 dans le heap).
                Module._NkWebAudioFillBuffer(bs, heapPtr);
                // Recopie planaire vers les channels JS.
                var floatPtr = heapPtr >> 2;  // byte ptr -> float32 index
                for (var c = 0; c < channels; ++c) {
                    var out = e.outputBuffer.getChannelData(c);
                    for (var i = 0; i < bs; ++i) {
                        out[i] = HEAPF32[floatPtr + i * channels + c];
                    }
                }
            };
            node.connect(ctx.destination);
            window.NkAudioNode = node;
            return ctx.sampleRate;  // retourne le sample rate reel
        });

        EM_JS(void, NkWebAudio_JSShutdown, (), {
            if (window.NkAudioNode) {
                window.NkAudioNode.disconnect();
                window.NkAudioNode.onaudioprocess = null;
                window.NkAudioNode = null;
            }
            if (window.NkAudioHeapPtr) {
                _free(window.NkAudioHeapPtr);
                window.NkAudioHeapPtr = 0;
            }
        });

        EM_JS(void, NkWebAudio_JSResume, (), {
            if (window.NkAudioCtx && window.NkAudioCtx.state === 'suspended') {
                window.NkAudioCtx.resume();
            }
        });

        EM_JS(void, NkWebAudio_JSSuspend, (), {
            if (window.NkAudioCtx && window.NkAudioCtx.state === 'running') {
                window.NkAudioCtx.suspend();
            }
        });

        WebAudioBackend::WebAudioBackend() = default;
        WebAudioBackend::~WebAudioBackend() { if (mRunning) Shutdown(); }

        bool WebAudioBackend::Initialize(int32 sampleRate, int32 channels, int32 bufferSize) {
            mSampleRate = sampleRate; mChannels = channels; mBufferSize = bufferSize;
            // Initialise le contexte audio cote JS et recupere le sample rate
            // reel (le navigateur peut ignorer notre request).
            int actualSR = NkWebAudio_JSInit(sampleRate, channels, bufferSize);
            if (actualSR == 0) return false;
            mSampleRate = actualSR;
            gWebAudioActive = this;
            return true;
        }

        void WebAudioBackend::Shutdown() {
            if (gWebAudioActive == this) gWebAudioActive = nullptr;
            NkWebAudio_JSShutdown();
            mRunning = false;
        }

        void WebAudioBackend::SetCallback(AudioCallback cb) { mCallback = cb; }
        void WebAudioBackend::Start()  { mRunning = true; mPaused = false; NkWebAudio_JSResume(); }
        void WebAudioBackend::Stop()   { mRunning = false; NkWebAudio_JSSuspend(); }
        void WebAudioBackend::Pause()  { mPaused  = true;  NkWebAudio_JSSuspend(); }
        void WebAudioBackend::Resume() { mPaused  = false; NkWebAudio_JSResume();  }

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN

// ============================================================
// AUTO-ENREGISTREMENT DES BACKENDS DANS LA FACTORY
// ============================================================

        // Fonction publique appelee EXPLICITEMENT par AudioEngine::Initialize
        // pour garantir que les backends sont enregistres meme en lib statique
        // (le linker peut stripper les static initializers d'une lib non-utilisee).
        // Idempotent via un guard local.
        void EnsureBackendsRegistered() {
            static bool sRegistered = false;
            if (sRegistered) return;
            sRegistered = true;

            // Null — toujours disponible (fallback).
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
            // OpenSL ES : fallback Android 24-25 (AAudio requiert 26+).
            AudioBackendFactory::Register("OpenSLES",
                []() -> IAudioBackend* { return new OpenSLESAudioBackend(); });
#endif

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            AudioBackendFactory::Register("WebAudio",
                []() -> IAudioBackend* { return new WebAudioBackend(); });
#endif
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
