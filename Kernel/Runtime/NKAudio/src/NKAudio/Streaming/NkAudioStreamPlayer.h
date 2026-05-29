#pragma once
/**
 * @File    NkAudioStreamPlayer.h
 * @Brief   Player audio streame : decoder thread + ring buffer + crossfade + loop.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Architecture
 *  Le AudioStreamPlayer maintient :
 *   - Un IAudioStream actif (la source audio).
 *   - Un ring buffer SPSC (Single Producer Single Consumer) float32.
 *   - Un thread worker qui appelle stream->ReadFrames() et ecrit dans le buffer.
 *   - Une methode ReadFrames() appelable depuis le thread audio (lock-free en
 *     lecture) qui pompe le buffer.
 *
 *  Le buffer fait typiquement 1-2 secondes audio (~88200 frames * 2 ch * 4 bytes
 *  = ~700 Ko pour stereo 44.1 kHz). Suffisant pour absorber les jitters du
 *  thread worker (decode CPU peut prendre plusieurs ms par chunk).
 *
 * @Usage
 *   AudioStreamPlayer player;
 *   player.Init(48000, 2, 88200); // sampleRate, channels, ring buffer frames
 *   IAudioStream* s = OpenAudioStream("music.flac");
 *   player.Play(s, true); // loop = true
 *   // ... dans le thread audio :
 *   player.ReadFrames(outBuf, nFrames);
 *   // ... a la fin :
 *   player.Stop();
 *   player.Shutdown();
 */

#include "NKAudio/Streaming/NkAudioStream.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace nkentseu {
    namespace audio {

        class NKENTSEU_AUDIO_API AudioStreamPlayer {
            public:
                AudioStreamPlayer() = default;
                ~AudioStreamPlayer();

                /// Initialise le player avec la config sortie.
                /// @param sampleRate      Sample rate cible (= celui du backend audio).
                /// @param channels        Canaux sortie (1 ou 2).
                /// @param ringBufferFrames  Taille du ring buffer en frames (typiquement 88200 pour ~2s).
                bool Init(int32 sampleRate, int32 channels, int32 ringBufferFrames = 88200) noexcept;

                /// Stop, join le thread worker, libere les ressources.
                void Shutdown() noexcept;

                /// Demarre la lecture du stream donne. Le player prend possession du
                /// stream (le supprime via delete a Stop()/Shutdown()).
                /// Si un stream est deja en cours, il est arrete et remplace.
                /// @param loop  true = boucle infinie (seek(0) a la fin)
                bool Play(IAudioStream* stream, bool loop = false) noexcept;

                /// Arrete la lecture, libere le stream actuel.
                void Stop() noexcept;

                /// Pause / resume (le thread worker tourne toujours mais ne decode plus).
                void Pause()  noexcept { mPaused = true;  }
                void Resume() noexcept { mPaused = false; }

                /// Lit jusqu'a maxFrames frames depuis le ring buffer.
                /// Appel attendu depuis le thread audio - lock-free en lecture.
                /// Retourne le nombre de frames effectivement ecrites.
                int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept;

                /// True si stream actif et pas EOF + en lecture (non-pause).
                bool IsPlaying() const noexcept { return mActive && !mPaused; }

                /// Volume scalaire applique a la sortie (1.0 = neutre).
                void  SetVolume(float32 v) noexcept { mVolume = v; }
                float32 GetVolume() const noexcept  { return mVolume; }

            private:
                void DecoderThreadProc();

                // Sortie config
                int32 mSampleRate = 0;
                int32 mChannels   = 0;

                // Ring buffer SPSC
                float32* mRingBuf      = nullptr;
                int32    mRingFrames   = 0;     ///< Capacite en frames
                std::atomic<int32> mWritePos{0}; ///< Index frame producteur
                std::atomic<int32> mReadPos{0};  ///< Index frame consommateur

                // Thread worker
                std::thread             mThread;
                std::atomic<bool>       mRunning{false};
                std::mutex              mStreamMutex; ///< Protege mStream/mLoop changes
                std::condition_variable mCV;          ///< Notifie quand le buffer a de la place

                // Stream actif
                IAudioStream* mStream = nullptr;
                bool          mLoop   = false;
                std::atomic<bool> mActive{false};
                std::atomic<bool> mPaused{false};

                // Volume
                std::atomic<float32> mVolume{1.0f};
        };

    } // namespace audio
} // namespace nkentseu
