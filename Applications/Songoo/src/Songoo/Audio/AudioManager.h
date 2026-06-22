#pragma once
// =============================================================================
// AudioManager.h — Gestionnaire audio Songo'o via NKAudio
// Remplace SFML3 (sf::Music, sf::Sound) par NKAudio (AudioSample, AudioStream)
// Sons générés procéduralement + lecture de fichiers MP3/WAV via NkAudioStream
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKAudio/NKAudio.h"
#include "Songoo/Game/GameTypes.h"

namespace nkentseu { namespace songoo {

    class AudioManager {
    public:
        AudioManager()  = default;
        ~AudioManager() = default;

        // ── Lifecycle ─────────────────────────────────────────────────────────
        bool Initialize();
        void Shutdown();
        bool IsInitialized() const noexcept { return mInitialized; }

        // ── Sons courts (one-shot) ────────────────────────────────────────────
        void Play(SoundId id, float volume = 1.0f) noexcept;

        void PlayPickup (float v = 1.0f) noexcept { Play(SoundId::Pickup,  v); }
        void PlayDeposit(float v = 1.0f) noexcept { Play(SoundId::Deposit, v); }
        void PlayDrum   (float v = 1.0f) noexcept { Play(SoundId::Drum,    v); }
        void PlayScore  (float v = 1.0f) noexcept { Play(SoundId::Score,   v); }

        // ── Musique de fond (streaming) ───────────────────────────────────────
        void PlayBgMusic(const char* path, bool loop = true, float volume = 0.1f);
        void StopBgMusic();
        void PauseBgMusic();
        void ResumeBgMusic();
        bool IsBgMusicPlaying() const noexcept;

        void PlayCreditMusic(float volume = 0.1f);
        void StopCreditMusic();

        // ── Volume ────────────────────────────────────────────────────────────
        void SetMasterVolume(float v) noexcept;
        void SetMusicVolume(float v)  noexcept;
        void SetSfxVolume(float v)    noexcept;

        float MasterVolume() const noexcept { return mMasterVolume; }
        float MusicVolume()  const noexcept { return mMusicVolume;  }
        float SfxVolume()    const noexcept { return mSfxVolume;    }

    private:
        bool  mInitialized  = false;
        float mMasterVolume = 1.0f;
        float mMusicVolume  = 0.10f;
        float mSfxVolume    = 1.00f;

        // Sons procéduraux courts
        audio::AudioSample mSamples[(int)SoundId::COUNT] = {};

        // Streams musicaux
        audio::NkAudioStreamPlayer mBgMusic;
        audio::NkAudioStreamPlayer mCreditMusic;

        bool LoadSample(SoundId id, const char* path) noexcept;

        // Générer les sons procéduralement (fallback si fichier absent)
        audio::AudioSample MakePickup()  const;
        audio::AudioSample MakeDeposit() const;
        audio::AudioSample MakeDrum()    const;
        audio::AudioSample MakeScore()   const;
    };

}} // namespace nkentseu::songoo
