/**
 * @File    NkAudioStreamPlayer.cpp
 * @Brief   Implementation du player streaming.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <chrono>
#include <cstring>

namespace nkentseu {
    namespace audio {

        // ────────────────────────────────────────────────────────────────────
        //  Helpers ring buffer SPSC (mono-producer / mono-consumer lock-free).
        //  L'invariant : writePos >= readPos toujours. La taille disponible
        //  pour lecture = writePos - readPos. La taille libre en ecriture =
        //  ringFrames - 1 - (writePos - readPos). On laisse 1 frame de marge
        //  pour distinguer "plein" de "vide".
        // ────────────────────────────────────────────────────────────────────

        static NKENTSEU_INLINE int32 FramesAvailable(int32 wPos, int32 rPos,
                                                      int32 capacity) noexcept {
            return wPos - rPos;
        }

        static NKENTSEU_INLINE int32 FramesFree(int32 wPos, int32 rPos,
                                                 int32 capacity) noexcept {
            return capacity - 1 - (wPos - rPos);
        }

        // ════════════════════════════════════════════════════════════════════
        //  AudioStreamPlayer
        // ════════════════════════════════════════════════════════════════════

        AudioStreamPlayer::~AudioStreamPlayer() {
            Shutdown();
        }

        bool AudioStreamPlayer::Init(int32 sampleRate, int32 channels,
                                     int32 ringBufferFrames) noexcept {
            if (mRunning.load()) return true; // deja initialise
            if (sampleRate <= 0 || channels <= 0 || ringBufferFrames <= 0) return false;

            mSampleRate = sampleRate;
            mChannels   = channels;
            mRingFrames = ringBufferFrames;

            usize ringBytes = usize(mRingFrames) * usize(mChannels) * sizeof(float32);
            mRingBuf = static_cast<float32*>(memory::NkAlloc(ringBytes, nullptr, sizeof(float32)));
            if (!mRingBuf) {
                logger.Error("[StreamPlayer] Echec allocation ring buffer ({0} octets)", ringBytes);
                return false;
            }
            ::memset(mRingBuf, 0, ringBytes);

            mWritePos = 0;
            mReadPos  = 0;
            mActive   = false;
            mPaused   = false;
            mRunning  = true;

            mThread = std::thread([this]{ DecoderThreadProc(); });
            logger.Info("[StreamPlayer] Init OK : {0} Hz, {1} ch, ring buffer {2} frames.",
                        sampleRate, channels, ringBufferFrames);
            return true;
        }

        void AudioStreamPlayer::Shutdown() noexcept {
            if (!mRunning.load()) return;
            mRunning = false;
            mActive  = false;
            mCV.notify_all();
            if (mThread.joinable()) mThread.join();

            // Cleanup
            {
                std::lock_guard<std::mutex> lock(mStreamMutex);
                if (mStream) { memory::NkGetDefaultAllocator().Delete(mStream); mStream = nullptr; }
            }
            if (mRingBuf) {
                memory::NkFree(mRingBuf, nullptr);
                mRingBuf = nullptr;
            }
            mRingFrames = 0;
        }

        bool AudioStreamPlayer::Play(IAudioStream* stream, bool loop) noexcept {
            if (!mRunning.load() || !stream) return false;
            std::lock_guard<std::mutex> lock(mStreamMutex);

            // Remplace le stream actuel (delete l'ancien)
            if (mStream) { memory::NkGetDefaultAllocator().Delete(mStream); mStream = nullptr; }
            mStream = stream;
            mLoop   = loop;

            // Reset le ring buffer
            mWritePos = 0;
            mReadPos  = 0;
            mActive   = true;
            mPaused   = false;
            mCV.notify_all();

            logger.Info("[StreamPlayer] Play : {0} Hz, {1} ch, loop={2}",
                        stream->GetSampleRate(), stream->GetChannels(), loop ? 1 : 0);
            return true;
        }

        void AudioStreamPlayer::Stop() noexcept {
            std::lock_guard<std::mutex> lock(mStreamMutex);
            mActive = false;
            if (mStream) { memory::NkGetDefaultAllocator().Delete(mStream); mStream = nullptr; }
            mWritePos = 0;
            mReadPos  = 0;
        }

        int32 AudioStreamPlayer::ReadFrames(float32* outBuf, int32 maxFrames) noexcept {
            if (!mRunning.load() || !mActive.load() || mPaused.load() || maxFrames <= 0) {
                ::memset(outBuf, 0, usize(maxFrames) * usize(mChannels) * sizeof(float32));
                return 0;
            }
            const int32 wPos = mWritePos.load(std::memory_order_acquire);
            const int32 rPos = mReadPos.load(std::memory_order_relaxed);
            int32 avail = FramesAvailable(wPos, rPos, mRingFrames);
            if (avail <= 0) {
                // Buffer vide : remplir avec du silence (le worker est en retard)
                ::memset(outBuf, 0, usize(maxFrames) * usize(mChannels) * sizeof(float32));
                return 0;
            }
            const int32 nFrames = (maxFrames < avail) ? maxFrames : avail;

            // Copie depuis le ring buffer (modulo mRingFrames)
            const float32 vol = mVolume.load();
            int32 rIdx = rPos % mRingFrames;
            int32 left = nFrames;
            float32* dst = outBuf;
            while (left > 0) {
                int32 chunk = (rIdx + left <= mRingFrames) ? left : (mRingFrames - rIdx);
                const float32* src = mRingBuf + usize(rIdx) * usize(mChannels);
                if (vol == 1.0f) {
                    ::memcpy(dst, src, usize(chunk) * usize(mChannels) * sizeof(float32));
                } else {
                    const int32 cnt = chunk * mChannels;
                    for (int32 i = 0; i < cnt; ++i) dst[i] = src[i] * vol;
                }
                dst   += usize(chunk) * usize(mChannels);
                rIdx   = (rIdx + chunk) % mRingFrames;
                left  -= chunk;
            }
            mReadPos.store(rPos + nFrames, std::memory_order_release);
            mCV.notify_all(); // reveille le worker s'il attend

            // Si on a lu moins que demande, remplir le reste avec du silence
            if (nFrames < maxFrames) {
                const int32 remain = maxFrames - nFrames;
                ::memset(dst, 0, usize(remain) * usize(mChannels) * sizeof(float32));
            }
            return nFrames;
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decoder thread : alimente le ring buffer en arriere-plan
        // ────────────────────────────────────────────────────────────────────

        void AudioStreamPlayer::DecoderThreadProc() {
            constexpr int32 kChunkFrames = 1024; // decode par paquets de 1024 frames
            // Buffer scratch pour conversion / mismatch channels
            float32* scratch = static_cast<float32*>(
                memory::NkAlloc(usize(kChunkFrames) * 8 * sizeof(float32), nullptr, sizeof(float32)));
            if (!scratch) {
                logger.Error("[StreamPlayer] Allocation scratch buffer echec.");
                return;
            }

            while (mRunning.load()) {
                if (!mActive.load() || mPaused.load()) {
                    std::unique_lock<std::mutex> lk(mStreamMutex);
                    mCV.wait_for(lk, std::chrono::milliseconds(50));
                    continue;
                }

                // Y a-t-il assez de place dans le ring buffer ?
                int32 wPos = mWritePos.load(std::memory_order_relaxed);
                int32 rPos = mReadPos.load(std::memory_order_acquire);
                int32 freeFrames = FramesFree(wPos, rPos, mRingFrames);
                if (freeFrames < kChunkFrames) {
                    // Attendre que le consommateur libere de la place
                    std::unique_lock<std::mutex> lk(mStreamMutex);
                    mCV.wait_for(lk, std::chrono::milliseconds(10));
                    continue;
                }

                // Recupere le stream (sous lock pour eviter race avec Play/Stop)
                IAudioStream* stream = nullptr;
                bool loop = false;
                int32 streamCh = 0;
                {
                    std::lock_guard<std::mutex> lock(mStreamMutex);
                    stream = mStream;
                    loop   = mLoop;
                    if (stream) streamCh = stream->GetChannels();
                }
                if (!stream) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // Decode kChunkFrames frames
                if (streamCh <= 0 || streamCh > 8) {
                    mActive = false;
                    continue;
                }
                int32 nRead = stream->ReadFrames(scratch, kChunkFrames);
                if (nRead == 0) {
                    if (loop) {
                        stream->Seek(0);
                        continue;
                    } else {
                        mActive = false;
                        continue;
                    }
                }

                // Conversion canaux si necessaire (downmix stereo->mono ou
                // upmix mono->stereo)
                int32 wIdx = wPos % mRingFrames;
                int32 left = nRead;
                const float32* src = scratch;
                while (left > 0) {
                    int32 chunk = (wIdx + left <= mRingFrames) ? left : (mRingFrames - wIdx);
                    float32* dst = mRingBuf + usize(wIdx) * usize(mChannels);
                    if (streamCh == mChannels) {
                        ::memcpy(dst, src, usize(chunk) * usize(mChannels) * sizeof(float32));
                    } else if (streamCh == 1 && mChannels == 2) {
                        // Mono -> Stereo : duplique
                        for (int32 i = 0; i < chunk; ++i) {
                            dst[i*2 + 0] = src[i];
                            dst[i*2 + 1] = src[i];
                        }
                    } else if (streamCh == 2 && mChannels == 1) {
                        // Stereo -> Mono : moyenne
                        for (int32 i = 0; i < chunk; ++i) {
                            dst[i] = (src[i*2 + 0] + src[i*2 + 1]) * 0.5f;
                        }
                    } else {
                        // Mismatch non gere : silence
                        ::memset(dst, 0, usize(chunk) * usize(mChannels) * sizeof(float32));
                    }
                    src   += usize(chunk) * usize(streamCh);
                    wIdx   = (wIdx + chunk) % mRingFrames;
                    left  -= chunk;
                }
                mWritePos.store(wPos + nRead, std::memory_order_release);
            }

            memory::NkFree(scratch, nullptr);
            logger.Info("[StreamPlayer] Decoder thread shutdown.");
        }

    } // namespace audio
} // namespace nkentseu
