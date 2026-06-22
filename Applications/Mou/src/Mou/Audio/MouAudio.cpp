// =============================================================================
// Audio/MouAudio.cpp
// =============================================================================
#include "Audio/MouAudio.h"
#include "Core/MouConfig.h"
#include <cstring>
#include <cstdio>

namespace mou {

    using namespace nkentseu;
    using namespace nkentseu::audio;

    bool MouAudio::Init() noexcept {
        if (mInitialized) return true;
        if (!AudioEngine::Instance().Initialize()) {
            MOU_LOG_WARN("[MouAudio] AudioEngine init echoue - audio desactive");
            mInitialized = false;
            return false;
        }
        mInitialized = true;
        GenerateSfxBank();
        MOU_LOG_INFO("[MouAudio] Audio pret");
        return true;
    }

    // Effets courts généés proceduralement (doux, jamais agressifs pour les tout-petits).
    void MouAudio::GenerateSfxBank() noexcept {
        auto env = [](float32 a, float32 d, float32 s, float32 r) {
            AdsrEnvelope e; e.attackTime = a; e.decayTime = d; e.sustainLevel = s; e.releaseTime = r; return e;
        };

        // Tap : petit "pop" clair et léger.
        mSfx[(int)Sfx::Tap] = AudioGenerator::GenerateTone(880.f, 0.09f, WaveformType::SINE, 44100, 0.55f);
        AudioGenerator::ApplyEnvelope(mSfx[(int)Sfx::Tap], env(0.004f, 0.04f, 0.2f, 0.05f));

        // Good : carillon clair (accord montant brillant).
        {
            const float32 freqs[3] = { 784.f, 1047.f, 1319.f };  // G5 - C6 - E6
            mSfx[(int)Sfx::Good] = AudioGenerator::GenerateChord(freqs, 3, 0.28f, nullptr, 44100);
            AudioGenerator::ApplyEnvelope(mSfx[(int)Sfx::Good], env(0.005f, 0.10f, 0.5f, 0.14f));
        }

        // Bad : "boing" doux et gentil (descente courte), jamais un buzzer.
        mSfx[(int)Sfx::Bad] = AudioGenerator::GenerateChirp(440.f, 230.f, 0.22f, 44100);
        AudioGenerator::ApplyEnvelope(mSfx[(int)Sfx::Bad], env(0.006f, 0.06f, 0.4f, 0.10f));

        // Win : petite fanfare joyeuse (accord large brillant).
        {
            const float32 freqs[4] = { 523.f, 659.f, 784.f, 1047.f };  // C5 - E5 - G5 - C6
            mSfx[(int)Sfx::Win] = AudioGenerator::GenerateChord(freqs, 4, 0.6f, nullptr, 44100);
            AudioGenerator::ApplyEnvelope(mSfx[(int)Sfx::Win], env(0.01f, 0.18f, 0.6f, 0.22f));
        }

        // Fail : descente douce et encourageante (pas triste).
        mSfx[(int)Sfx::Fail] = AudioGenerator::GenerateChirp(520.f, 320.f, 0.45f, 44100);
        AudioGenerator::ApplyEnvelope(mSfx[(int)Sfx::Fail], env(0.01f, 0.12f, 0.5f, 0.18f));

        mSfxLoaded = true;
    }

    void MouAudio::PlaySfx(Sfx id) noexcept {
        if (!mInitialized || !mSfxLoaded) return;
        const int i = (int)id;
        if (i < 0 || i >= (int)Sfx::COUNT || !mSfx[i].IsValid()) return;
        VoiceParams p;
        p.volume = settings::EffectiveSfx();
        p.bus    = "SFX";
        AudioEngine::Instance().Play(mSfx[i], p);
    }

    void MouAudio::Shutdown() noexcept {
        if (!mInitialized) return;
        StopMusic();
        if (mSfxLoaded) {
            for (int i = 0; i < (int)Sfx::COUNT; ++i)
                if (mSfx[i].IsValid()) AudioLoader::Free(mSfx[i]);
            mSfxLoaded = false;
        }
        for (int i = 0; i < VOICE_RING; ++i)
            if (mVoice[i].IsValid()) AudioLoader::Free(mVoice[i]);
        AudioEngine::Instance().Shutdown();
        mInitialized = false;
    }

    void MouAudio::PlayMusic(const char* path) noexcept {
        if (!mInitialized || !path) return;
        // Déjà la piste en cours -> on ne relance pas (évite les coupures).
        if (mHasSample && std::strcmp(mCurrentPath, path) == 0) return;

        StopMusic();

        // Racine assets : "" sur Android (AAssetManager) / "assets/" sur desktop.
        char full[160];
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
        std::snprintf(full, sizeof(full), "%s", path);
#else
        std::snprintf(full, sizeof(full), "assets/%s", path);
#endif
        mMusicSample = AudioLoader::Load(full);   // allocateur global (= celui de Free)
        if (!mMusicSample.IsValid()) {
            MOU_LOG_WARNF("[MouAudio] Musique introuvable/illisible: %s", path);
            return;
        }
        mHasSample = true;
        std::strncpy(mCurrentPath, path, sizeof(mCurrentPath) - 1);
        mCurrentPath[sizeof(mCurrentPath) - 1] = '\0';

        VoiceParams p;
        p.looping = true;
        p.volume  = settings::EffectiveMusic();
        p.bus     = "Music";
        mMusicHandle = AudioEngine::Instance().Play(mMusicSample, p);
    }

    void MouAudio::PlayVoice(const char* key) noexcept {
        if (!mInitialized || !key) return;
        char full[160];
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
        std::snprintf(full, sizeof(full), "voice/%s.wav", key);
#else
        std::snprintf(full, sizeof(full), "assets/voice/%s.wav", key);
#endif
        AudioSample s = AudioLoader::Load(full);
        if (!s.IsValid()) return;   // voix pas encore fournie -> silencieux

        if (mVoice[mVoiceNext].IsValid()) AudioLoader::Free(mVoice[mVoiceNext]);
        mVoice[mVoiceNext] = s;
        VoiceParams p;
        p.volume = settings::EffectiveSfx();
        p.bus    = "Voice";
        AudioEngine::Instance().Play(mVoice[mVoiceNext], p);
        mVoiceNext = (mVoiceNext + 1) % VOICE_RING;
    }

    void MouAudio::StopMusic() noexcept {
        if (!mInitialized) return;
        if (mHasSample) {
            AudioEngine::Instance().Stop(mMusicHandle);
            AudioLoader::Free(mMusicSample);
            mHasSample = false;
        }
        mMusicHandle = AUDIO_HANDLE_INVALID;
        mCurrentPath[0] = '\0';
    }

    void MouAudio::RefreshVolume() noexcept {
        if (!mInitialized || !mHasSample) return;
        AudioEngine::Instance().SetVolume(mMusicHandle, settings::EffectiveMusic());
    }

    void MouAudio::Pause() noexcept {
        if (mInitialized && mHasSample) AudioEngine::Instance().Pause(mMusicHandle);
    }

    void MouAudio::Resume() noexcept {
        if (mInitialized && mHasSample) AudioEngine::Instance().Resume(mMusicHandle);
    }

}  // namespace mou
