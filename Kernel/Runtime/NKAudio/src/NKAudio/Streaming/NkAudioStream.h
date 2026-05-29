#pragma once
/**
 * @File    NkAudioStream.h
 * @Brief   Streaming audio long (musique de fond, ambient) sans charger tout en RAM.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Architecture
 *  IAudioStream est une interface "pull" : le caller demande N frames, le stream
 *  les fournit. Implementations :
 *   - WavStream     : streaming chunked depuis un fichier WAV (faible RAM).
 *   - FlacStream    : decode complet en RAM via NkFLACCodec puis lecture chunked.
 *                     (refacto vers streaming reel possible plus tard si besoin)
 *   - MemoryStream  : wrapper d'un AudioSample existant (utile pour SFX en
 *                     boucle long).
 *
 *  En production, on utilise un AudioStreamPlayer qui :
 *   - Lance un thread worker qui lit le stream et remplit un ring buffer.
 *   - Le mixer/backend consomme depuis le ring buffer (thread audio, lock-free
 *     en lecture).
 *   - Crossfade automatique a la fin du stream pour transition douce, et
 *     support du loop (re-seek a frame 0 et continue).
 *
 * @Usage_minimal
 *   IAudioStream* s = OpenAudioStream("music.flac");
 *   if (s) {
 *       AudioStreamPlayer player;
 *       player.Init();
 *       player.Play(s, /*loop=* /true);
 *       // ... player.ReadFrames(...) appele depuis le thread audio
 *       player.Shutdown();
 *   }
 */
#include "NKAudio/NKAudio.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
    namespace audio {

        // ── Interface ────────────────────────────────────────────────────────

        class NKENTSEU_AUDIO_API IAudioStream {
            public:
                virtual ~IAudioStream() = default;

                /// Lit jusqu'a `maxFrames` frames interleaved float32 dans outBuf.
                /// Retourne le nombre de frames effectivement ecrites.
                /// Retourne 0 en cas d'EOF (utiliser IsEOF() pour distinguer EOF d'erreur).
                virtual int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept = 0;

                /// Repositionne la lecture au frame index donne (0 = debut).
                /// Retourne true si succes.
                virtual bool  Seek(nk_int64 frameIdx) noexcept = 0;

                /// Nombre total de frames (-1 si inconnu / stream infini).
                virtual nk_int64 GetFrameCount() const noexcept = 0;

                virtual int32 GetSampleRate() const noexcept = 0;
                virtual int32 GetChannels()   const noexcept = 0;
                virtual bool  IsEOF()         const noexcept = 0;
        };

        // ── Factory ──────────────────────────────────────────────────────────
        // Detecte le format depuis le path ou la signature et retourne l'IAudioStream
        // adequat. Le caller doit liberer via `delete stream`. Retourne nullptr en cas
        // d'echec.
        NKENTSEU_AUDIO_API IAudioStream* OpenAudioStream(const char* path) noexcept;

        // ── Concrete : WavStream ────────────────────────────────────────────
        // Lecture chunked d'un WAV depuis le fichier (FILE*). Faible memoire.

        class NKENTSEU_AUDIO_API WavStream : public IAudioStream {
            public:
                WavStream() = default;
                ~WavStream() override;

                bool Open(const char* path) noexcept;

                int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept override;
                bool  Seek(nk_int64 frameIdx) noexcept override;
                nk_int64 GetFrameCount() const noexcept override { return mFrameCount; }
                int32 GetSampleRate() const noexcept override   { return mSampleRate; }
                int32 GetChannels()   const noexcept override   { return mChannels; }
                bool  IsEOF()         const noexcept override   { return mEOF; }

            private:
                NkFile   mFile;                  ///< NkFile gere Android AAssetManager automatiquement
                nk_int64 mDataStart   = 0;       ///< Offset du premier octet PCM
                nk_int64 mDataSize    = 0;
                nk_int64 mFrameCount  = 0;
                int32    mSampleRate  = 0;
                int32    mChannels    = 0;
                int32    mBitsPerSamp = 0;
                int32    mFormat      = 0;       ///< 1=PCM, 3=Float
                bool     mEOF         = false;
                nk_int64 mCurFrame    = 0;
        };

        // ── Concrete : MemoryStream (wrapper AudioSample) ───────────────────
        // Utile pour FLAC/MP3/OGG : decode le fichier entier puis stream depuis
        // la memoire. Plus simple que d'instrumenter chaque decodeur en streaming.

        class NKENTSEU_AUDIO_API MemoryStream : public IAudioStream {
            public:
                /// Prend possession du AudioSample (liberera via mAllocator a la destruction).
                explicit MemoryStream(AudioSample sample) noexcept;
                ~MemoryStream() override;

                int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept override;
                bool  Seek(nk_int64 frameIdx) noexcept override;
                nk_int64 GetFrameCount() const noexcept override { return nk_int64(mSample.frameCount); }
                int32 GetSampleRate() const noexcept override   { return mSample.sampleRate; }
                int32 GetChannels()   const noexcept override   { return mSample.channels; }
                bool  IsEOF()         const noexcept override   { return mCurFrame >= nk_int64(mSample.frameCount); }

            private:
                AudioSample mSample{};
                nk_int64    mCurFrame = 0;
        };

    } // namespace audio
} // namespace nkentseu
