// =============================================================================
// Audio/MouAudio.h
// Gestionnaire audio minimal de Mú : musique de fond en boucle (1 piste à la
// fois) via NKAudio, volume piloté par l'écran Réglages. Les effets/voix
// viendront s'ajouter quand les .wav seront fournis.
// =============================================================================
#pragma once

#ifndef MOU_AUDIO_H
#define MOU_AUDIO_H

#include "NKCore/NkTypes.h"
#include "NKAudio/NKAudio.h"

namespace mou {

    class MouAudio {
    public:
        // Effets courts (générés proceduralement). Correspondent aux Cue de MouFeedback.
        enum class Sfx { Tap, Good, Bad, Win, Fail, COUNT };

        bool Init() noexcept;
        void Shutdown() noexcept;
        bool IsReady() const noexcept { return mInitialized; }

        /// Joue un effet court (volume = Réglages Effets). No-op si audio absent.
        void PlaySfx(Sfx id) noexcept;

        /// Joue une voix `voice/<key>.wav` (volume = Réglages Effets). Silencieux si
        /// le fichier n'existe pas encore (voix optionnelles, ajoutées plus tard).
        void PlayVoice(const char* key) noexcept;

        /// Lance la musique `path` (relatif à assets/, ex. "audios/x.mp3") en boucle.
        /// No-op si c'est déjà la piste en cours. Applique le volume Réglages.
        void PlayMusic(const char* path) noexcept;
        void StopMusic() noexcept;

        /// Réapplique le volume musique courant (settings::EffectiveMusic()).
        void RefreshVolume() noexcept;

        /// Mise en arrière-plan / retour (Android) : suspend/relance le son.
        void Pause() noexcept;
        void Resume() noexcept;

    private:
        void GenerateSfxBank() noexcept;

        bool                       mInitialized = false;
        nkentseu::audio::AudioSample mMusicSample{};
        nkentseu::audio::AudioHandle mMusicHandle = nkentseu::audio::AUDIO_HANDLE_INVALID;
        char                       mCurrentPath[128] = {0};
        bool                       mHasSample = false;

        nkentseu::audio::AudioSample mSfx[(int)Sfx::COUNT] = {};
        bool                       mSfxLoaded = false;

        // Anneau de voix : garde les derniers samples vivants le temps de la lecture.
        static constexpr int       VOICE_RING = 4;
        nkentseu::audio::AudioSample mVoice[VOICE_RING] = {};
        int                        mVoiceNext = 0;
    };

}  // namespace mou

#endif // MOU_AUDIO_H
