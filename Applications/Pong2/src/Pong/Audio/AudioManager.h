#pragma once
// =============================================================================
// AudioManager.h
// -----------------------------------------------------------------------------
// Gestionnaire audio Pong : init NKAudio + cache des samples + helpers
// PlayPaddle/PlayScore/PlayBonus/etc.
//
// Architecture :
//   - 1 instance vivante pendant toute la duree de l'app (detenue par PongApp).
//   - PongApp::Init appelle InitAudio() : charge les WAV + start AudioEngine.
//   - Les scenes appellent PlayPaddle(), PlayScore(), etc. (no-op si pas init).
//   - PongApp::Shutdown appelle Shutdown() : free samples + stop engine.
//
// Cf [[nkaudio_roadmap]] et [[pong_release_2026-05-20]].
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKAudio/NKAudio.h"

namespace nkentseu
{
    namespace pong
    {

        /// Identifiants logiques des sons utilises par Pong.
        enum class SoundId : uint8
        {
            PaddleHit = 0,  ///< Balle vs raquette ("bleep")
            WallHit,        ///< Balle vs mur/obstacle ("solid")
            Score,          ///< But marque (long jingle)
            Bonus,          ///< Bonus ramasse / powerup actif
            MenuSelect,     ///< Click bouton menu
            MatchStart,     ///< Coup d'envoi du match
            COUNT
        };

        class AudioManager
        {
        public:
            AudioManager()  = default;
            ~AudioManager() = default;

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Init AudioEngine + charge les samples. Retourne false si echec
            /// (mais l'app continue : tous les Play deviennent no-op).
            bool Initialize();
            /// Liberation : stop engine + free samples. Idempotent.
            void Shutdown();
            bool IsInitialized() const noexcept { return mInitialized; }

            // ── Helpers de lecture (no-op si !mInitialized) ──────────────────
            /// Joue un sound avec volume optionnel [0..1].
            void Play(SoundId id, float32 volume = 1.0f) noexcept;

            // Aliases pratiques (lisibilite cote scenes).
            void PlayPaddle(float32 v = 1.0f)   noexcept { Play(SoundId::PaddleHit,  v); }
            void PlayWall  (float32 v = 1.0f)   noexcept { Play(SoundId::WallHit,    v); }
            void PlayScore (float32 v = 1.0f)   noexcept { Play(SoundId::Score,      v); }
            void PlayBonus (float32 v = 1.0f)   noexcept { Play(SoundId::Bonus,      v); }
            void PlayMenu  (float32 v = 0.6f)   noexcept { Play(SoundId::MenuSelect, v); }
            void PlayStart (float32 v = 1.0f)   noexcept { Play(SoundId::MatchStart, v); }

            /// Volume master global [0..1] (affecte tous les sons).
            void  SetMasterVolume(float32 v) noexcept;
            float32 MasterVolume() const noexcept { return mMasterVolume; }

        private:
            bool                    mInitialized  = false;
            float32                 mMasterVolume = 0.8f;
            audio::AudioSample      mSamples[(int)SoundId::COUNT] = {};

            /// Charge un WAV en sample. Retourne false si fichier introuvable.
            bool LoadSample(SoundId id, const char* path) noexcept;
        };

    } // namespace pong
} // namespace nkentseu
