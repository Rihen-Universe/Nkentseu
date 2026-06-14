// =============================================================================
// AudioManager.cpp
// -----------------------------------------------------------------------------
// Init NKAudio + GENERATION PROCEDURALE des SFX via AudioGenerator. Pas de
// WAV externes : les sons sont synthetises en RAM au demarrage (oscillateurs
// + ADSR + chirps + bruit). Avantages :
//  - Aucun asset audio a embarquer (gain de taille APK/EXE)
//  - SFX uniques au jeu Pong (impossible a copier ailleurs)
//  - Variation aleatoire de pitch a chaque Play (anti-repetition)
//  - Modulable a l'execution (volume, pitch selon vitesse balle, etc.)
// =============================================================================

#include "AudioManager.h"
#include "NKAudio/NKAudio.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkRandom.h"
#include <cstdio>

namespace nkentseu
{
    namespace pong
    {

        using namespace audio;

        // ─────────────────────────────────────────────────────────────────────
        // Generateurs procedural par SoundId. Chaque fonction synthetise un
        // AudioSample en memoire et l'envoie dans mSamples[].
        // ─────────────────────────────────────────────────────────────────────

        // PaddleHit : court "ping" sinus 880 Hz (LA5) + ADSR snappy.
        // Duree 60ms : sec et reactif, pas envahissant aux echanges rapides.
        static AudioSample MakePaddleHit()
        {
            AudioSample s = AudioGenerator::GenerateTone(
                /*frequency*/ 880.0f,
                /*duration */ 0.06f,
                /*waveform */ WaveformType::SINE,
                /*sampleRate*/ 48000,
                /*amplitude*/ 0.85f);
            AdsrEnvelope env;
            env.attackTime   = 0.002f;   // attack immediate
            env.decayTime    = 0.02f;
            env.sustainLevel = 0.5f;
            env.releaseTime  = 0.03f;
            AudioGenerator::ApplyEnvelope(s, env);
            return s;
        }

        // WallHit : square 220 Hz court (sec, dur, comme un caillou).
        static AudioSample MakeWallHit()
        {
            AudioSample s = AudioGenerator::GenerateTone(
                220.0f, 0.05f, WaveformType::SQUARE, 48000, 0.6f);
            AdsrEnvelope env;
            env.attackTime   = 0.001f;
            env.decayTime    = 0.015f;
            env.sustainLevel = 0.3f;
            env.releaseTime  = 0.025f;
            AudioGenerator::ApplyEnvelope(s, env);
            return s;
        }

        // Score : chirp sweep up 220 -> 880 Hz (do montant) = sensation
        // de victoire + ADSR doux pour eviter le clic.
        static AudioSample MakeScore()
        {
            AudioSample s = AudioGenerator::GenerateChirp(
                /*startFreq*/ 220.0f,
                /*endFreq  */ 880.0f,
                /*duration */ 0.45f,
                /*sampleRate*/ 48000);
            AdsrEnvelope env;
            env.attackTime   = 0.01f;
            env.decayTime    = 0.05f;
            env.sustainLevel = 0.8f;
            env.releaseTime  = 0.20f;
            AudioGenerator::ApplyEnvelope(s, env);
            return s;
        }

        // Bonus : arpege magique 3 notes (do/mi/sol = accord majeur) joue
        // simultanement -> son cristallin.
        static AudioSample MakeBonus()
        {
            const float32 freqs[]      = { 523.25f, 659.25f, 783.99f }; // C5/E5/G5
            const float32 amplitudes[] = { 0.40f,   0.30f,   0.30f   };
            AudioSample s = AudioGenerator::GenerateChord(
                freqs, 3, /*duration*/ 0.40f, amplitudes, 48000);
            AdsrEnvelope env;
            env.attackTime   = 0.005f;
            env.decayTime    = 0.08f;
            env.sustainLevel = 0.6f;
            env.releaseTime  = 0.25f;
            AudioGenerator::ApplyEnvelope(s, env);
            return s;
        }

        // MenuSelect : bleep sine 1200 Hz tres court (40ms).
        static AudioSample MakeMenuSelect()
        {
            AudioSample s = AudioGenerator::GenerateTone(
                1200.0f, 0.04f, WaveformType::SINE, 48000, 0.5f);
            AdsrEnvelope env;
            env.attackTime   = 0.002f;
            env.decayTime    = 0.01f;
            env.sustainLevel = 0.4f;
            env.releaseTime  = 0.02f;
            AudioGenerator::ApplyEnvelope(s, env);
            return s;
        }

        // MatchStart : kick drum (FM synthese, descente 180 -> 40 Hz).
        // Donne une sensation de "GO!" puissant au coup d'envoi.
        static AudioSample MakeMatchStart()
        {
            return AudioGenerator::GenerateKick(
                /*duration   */ 0.30f,
                /*startFreq  */ 180.0f,
                /*endFreq    */ 40.0f,
                /*clickAttack*/ 0.003f,
                /*sampleRate */ 48000);
        }

        // ─────────────────────────────────────────────────────────────────────
        // Lifecycle
        // ─────────────────────────────────────────────────────────────────────

        bool AudioManager::Initialize()
        {
            if (mInitialized) return true;

            // 1. Init AudioEngine. Backend AUTO = WASAPI Windows, CoreAudio
            //    macOS, ALSA Linux, AAudio Android.
            AudioEngineConfig cfg;
            cfg.sampleRate   = 48000;
            cfg.channels     = 2;
            cfg.bufferSize   = 512;
            cfg.backend      = AudioBackendType::AUTO;
            cfg.masterVolume = mMasterVolume;

            if (!AudioEngine::Instance().Initialize(cfg))
            {
                logger.Warn("[AudioManager] AudioEngine init FAILED -> sons desactives");
                return false;
            }
            logger.Info("[AudioManager] AudioEngine OK : backend={0}, {1} Hz, {2} ch, latency={3:.1}ms",
                        AudioEngine::Instance().GetBackendName(),
                        AudioEngine::Instance().GetSampleRate(),
                        AudioEngine::Instance().GetChannels(),
                        AudioEngine::Instance().GetLatencyMs());

            // 2. Generation procedural de tous les SFX. Aucun WAV charge !
            mSamples[(int)SoundId::PaddleHit]  = MakePaddleHit();
            mSamples[(int)SoundId::WallHit]    = MakeWallHit();
            mSamples[(int)SoundId::Score]      = MakeScore();
            mSamples[(int)SoundId::Bonus]      = MakeBonus();
            mSamples[(int)SoundId::MenuSelect] = MakeMenuSelect();
            mSamples[(int)SoundId::MatchStart] = MakeMatchStart();
            logger.Info("[AudioManager] {0} SFX generes procedural en RAM",
                        (int)SoundId::COUNT);

            mInitialized = true;
            return true;
        }

        void AudioManager::Shutdown()
        {
            if (!mInitialized) return;
            // Free tous les samples (memoire allouee par AudioGenerator).
            for (int i = 0; i < (int)SoundId::COUNT; ++i)
            {
                if (mSamples[i].IsValid())
                {
                    AudioLoader::Free(mSamples[i]);
                }
            }
            AudioEngine::Instance().Shutdown();
            mInitialized = false;
            logger.Info("[AudioManager] Shutdown");
        }

        // ─────────────────────────────────────────────────────────────────────
        // Play : variation pitch aleatoire ±8% pour anti-robotique.
        // ─────────────────────────────────────────────────────────────────────
        void AudioManager::Play(SoundId id, float32 volume) noexcept
        {
            if (!mInitialized) return;
            const int idx = (int)id;
            if (idx < 0 || idx >= (int)SoundId::COUNT) return;
            if (!mSamples[idx].IsValid()) return;

            // Variation pitch : 0.92..1.08 (uniforme). Donne du naturel aux
            // sons repetes (paddle hit a chaque echange notamment).
            const uint32 r = math::NkRandom::Instance().NextUInt32(1000);
            const float32 pitchVar = 0.92f + (float32)r * 0.00016f; // [0.92..1.08]

            VoiceParams vp;
            vp.volume = volume;
            vp.pitch  = pitchVar;
            (void)AudioEngine::Instance().Play(mSamples[idx], vp);
        }

        void AudioManager::SetMasterVolume(float32 v) noexcept
        {
            mMasterVolume = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
            if (mInitialized)
            {
                AudioEngine::Instance().SetMasterVolume(mMasterVolume);
            }
        }

    } // namespace pong
} // namespace nkentseu
