// =============================================================================
// AudioManager.cpp — Audio Songo'o via NKAudio
// Les sons sont d'abord tentés depuis les fichiers MP3 (audio/),
// avec fallback sur génération procédurale si le fichier est absent.
// =============================================================================

#include "AudioManager.h"
#include "NKAudio/NKAudio.h"
#include "NKLogger/NkLog.h"
#include <cmath>

namespace nkentseu { namespace songoo {

    using namespace audio;

    // ── Sons procéduraux ─────────────────────────────────────────────────────

    AudioSample AudioManager::MakePickup() const {
        // Court "clic" grave : semis de graines
        AudioSample s = AudioGenerator::GenerateTone(
            440.0f, 0.08f, WaveformType::SINE, 44100, 0.7f);
        AdsrEnvelope env;
        env.attackTime   = 0.003f;
        env.decayTime    = 0.03f;
        env.sustainLevel = 0.4f;
        env.releaseTime  = 0.04f;
        AudioGenerator::ApplyEnvelope(s, env);
        return s;
    }

    AudioSample AudioManager::MakeDeposit() const {
        // "Toc" bois : graine déposée dans le trou
        AudioSample s = AudioGenerator::GenerateTone(
            660.0f, 0.05f, WaveformType::SQUARE, 44100, 0.5f);
        AdsrEnvelope env;
        env.attackTime   = 0.001f;
        env.decayTime    = 0.015f;
        env.sustainLevel = 0.2f;
        env.releaseTime  = 0.025f;
        AudioGenerator::ApplyEnvelope(s, env);
        return s;
    }

    AudioSample AudioManager::MakeDrum() const {
        // Tambour africain : sweep descendant 200→80 Hz
        AudioSample s = AudioGenerator::GenerateChirp(
            200.0f, 80.0f, 0.30f, 44100);
        AdsrEnvelope env;
        env.attackTime   = 0.005f;
        env.decayTime    = 0.05f;
        env.sustainLevel = 0.6f;
        env.releaseTime  = 0.15f;
        AudioGenerator::ApplyEnvelope(s, env);
        return s;
    }

    AudioSample AudioManager::MakeScore() const {
        // Montée joyeuse 220→880 Hz (capture réussie)
        AudioSample s = AudioGenerator::GenerateChirp(
            220.0f, 880.0f, 0.40f, 44100);
        AdsrEnvelope env;
        env.attackTime   = 0.010f;
        env.decayTime    = 0.05f;
        env.sustainLevel = 0.8f;
        env.releaseTime  = 0.18f;
        AudioGenerator::ApplyEnvelope(s, env);
        return s;
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    bool AudioManager::Initialize() {
        if (!AudioEngine::Init()) {
            logger.Warn("[AudioManager] AudioEngine::Init failed — audio disabled");
            return false;
        }
        mInitialized = true;

        // Tenter de charger les fichiers MP3 originaux ; sinon génération
        struct SndFile { SoundId id; const char* path; } files[] = {
            { SoundId::Pickup,  "audio/pickup.mp3"  },
            { SoundId::Deposit, "audio/deposit.mp3" },
            { SoundId::Drum,    "audio/tambour.mp3" },
            { SoundId::Score,   "audio/songo2.mp3"  },
        };

        for (auto& sf : files) {
            if (!LoadSample(sf.id, sf.path)) {
                // Génération procédurale si fichier absent
                switch (sf.id) {
                    case SoundId::Pickup:  mSamples[(int)sf.id] = MakePickup();  break;
                    case SoundId::Deposit: mSamples[(int)sf.id] = MakeDeposit(); break;
                    case SoundId::Drum:    mSamples[(int)sf.id] = MakeDrum();    break;
                    case SoundId::Score:   mSamples[(int)sf.id] = MakeScore();   break;
                    default: break;
                }
            }
        }

        logger.Info("[AudioManager] Initialized OK");
        return true;
    }

    bool AudioManager::LoadSample(SoundId id, const char* path) noexcept {
        bool ok = AudioLoader::LoadFile(path, mSamples[(int)id]);
        if (!ok) logger.Warn("[AudioManager] Cannot load '{}' — using procedural", path);
        return ok;
    }

    void AudioManager::Shutdown() {
        if (!mInitialized) return;
        StopBgMusic();
        StopCreditMusic();
        for (int i = 0; i < (int)SoundId::COUNT; i++)
            mSamples[i] = {};
        AudioEngine::Shutdown();
        mInitialized = false;
    }

    // ── Lecture sons courts ───────────────────────────────────────────────────

    void AudioManager::Play(SoundId id, float volume) noexcept {
        if (!mInitialized) return;
        float v = volume * mSfxVolume * mMasterVolume;
        AudioEngine::PlaySample(mSamples[(int)id], v);
    }

    // ── Musique de fond (streaming) ───────────────────────────────────────────

    void AudioManager::PlayBgMusic(const char* path, bool loop, float volume) {
        if (!mInitialized) return;
        StopBgMusic();
        float v = volume * mMusicVolume * mMasterVolume;
        if (!mBgMusic.Open(path)) {
            logger.Warn("[AudioManager] Cannot open '{}'", path);
            return;
        }
        mBgMusic.SetLooping(loop);
        mBgMusic.SetVolume(v);
        mBgMusic.Play();
    }

    void AudioManager::StopBgMusic()    { if (mInitialized) mBgMusic.Stop();   }
    void AudioManager::PauseBgMusic()   { if (mInitialized) mBgMusic.Pause();  }
    void AudioManager::ResumeBgMusic()  { if (mInitialized) mBgMusic.Resume(); }

    bool AudioManager::IsBgMusicPlaying() const noexcept {
        return mInitialized && mBgMusic.IsPlaying();
    }

    void AudioManager::PlayCreditMusic(float volume) {
        if (!mInitialized) return;
        StopCreditMusic();
        float v = volume * mMusicVolume * mMasterVolume;
        if (!mCreditMusic.Open("audio/credit.mp3")) {
            logger.Warn("[AudioManager] Cannot open 'audio/credit.mp3'");
            return;
        }
        mCreditMusic.SetLooping(true);
        mCreditMusic.SetVolume(v);
        mCreditMusic.Play();
    }

    void AudioManager::StopCreditMusic() { if (mInitialized) mCreditMusic.Stop(); }

    // ── Volume ────────────────────────────────────────────────────────────────

    void AudioManager::SetMasterVolume(float v) noexcept {
        mMasterVolume = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
        if (mInitialized) AudioEngine::SetMasterVolume(mMasterVolume);
    }

    void AudioManager::SetMusicVolume(float v) noexcept {
        mMusicVolume = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
        float eff = mMusicVolume * mMasterVolume;
        if (mInitialized) { mBgMusic.SetVolume(eff); mCreditMusic.SetVolume(eff); }
    }

    void AudioManager::SetSfxVolume(float v) noexcept {
        mSfxVolume = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    }

}} // namespace nkentseu::songoo
