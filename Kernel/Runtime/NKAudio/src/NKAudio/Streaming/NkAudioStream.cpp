/**
 * @File    NkAudioStream.cpp
 * @Brief   Implementations IAudioStream : WavStream, MemoryStream + factory.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Codecs/FLAC/NkFLACCodec.h"
#include "NKAudio/Codecs/MP3/NkMP3Codec.h"
#include "NKAudio/Codecs/OGG/NkOGGVorbisCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace audio {

        // ────────────────────────────────────────────────────────────────────
        //  Helpers little-endian (WAV est LE)
        // ────────────────────────────────────────────────────────────────────

        static NKENTSEU_INLINE uint16 RD_U16LE(const uint8* p) noexcept {
            return uint16(p[0]) | (uint16(p[1]) << 8);
        }
        static NKENTSEU_INLINE uint32 RD_U32LE(const uint8* p) noexcept {
            return uint32(p[0]) | (uint32(p[1]) << 8)
                 | (uint32(p[2]) << 16) | (uint32(p[3]) << 24);
        }

        // ════════════════════════════════════════════════════════════════════
        //  WavStream
        // ════════════════════════════════════════════════════════════════════

        WavStream::~WavStream() {
            if (mFile.IsOpen()) mFile.Close();
        }

        // ── Open : utilise NkFile (gere automatiquement Android AAssetManager) ──
        bool WavStream::Open(const char* path) noexcept {
            if (!path) return false;
            if (!mFile.Open(path, NkFileMode::NK_READ_BINARY)) {
                logger.Error("[WavStream] Impossible d'ouvrir : {0}", path);
                return false;
            }

            // Lire l'entete RIFF/WAVE (12 octets : "RIFF" + size + "WAVE")
            uint8 hdr[12];
            if (mFile.Read(hdr, 12) != 12) goto fail;
            if (hdr[0]!='R'||hdr[1]!='I'||hdr[2]!='F'||hdr[3]!='F') goto fail;
            if (hdr[8]!='W'||hdr[9]!='A'||hdr[10]!='V'||hdr[11]!='E') goto fail;

            // Parcourir les chunks (fmt , data, etc.)
            while (true) {
                uint8 chunkHdr[8];
                if (mFile.Read(chunkHdr, 8) != 8) break;
                uint32 chunkSize = RD_U32LE(chunkHdr + 4);

                if (chunkHdr[0]=='f' && chunkHdr[1]=='m' && chunkHdr[2]=='t' && chunkHdr[3]==' ') {
                    if (chunkSize < 16 || chunkSize > 64) goto fail;
                    uint8 fmt[64];
                    if (mFile.Read(fmt, chunkSize) != chunkSize) goto fail;
                    mFormat      = RD_U16LE(fmt + 0);
                    mChannels    = RD_U16LE(fmt + 2);
                    mSampleRate  = int32(RD_U32LE(fmt + 4));
                    mBitsPerSamp = RD_U16LE(fmt + 14);
                } else if (chunkHdr[0]=='d' && chunkHdr[1]=='a' && chunkHdr[2]=='t' && chunkHdr[3]=='a') {
                    mDataStart = mFile.Tell();
                    mDataSize  = chunkSize;
                    int32 bytesPerFrame = (mChannels * mBitsPerSamp) / 8;
                    if (bytesPerFrame <= 0) goto fail;
                    mFrameCount = mDataSize / bytesPerFrame;
                    mFile.Seek(mDataStart, NkSeekOrigin::NK_BEGIN);
                    mCurFrame = 0;
                    mEOF = false;
                    logger.Info("[WavStream] Ouvert : {0} frames, {1} Hz, {2} ch, {3} bps",
                                mFrameCount, mSampleRate, mChannels, mBitsPerSamp);
                    return true;
                } else {
                    // Skip chunk (alignement 2 octets)
                    mFile.Seek(nk_int64((chunkSize + 1) & ~1u), NkSeekOrigin::NK_CURRENT);
                }
            }

            fail:
            if (mFile.IsOpen()) mFile.Close();
            return false;
        }

        int32 WavStream::ReadFrames(float32* outBuf, int32 maxFrames) noexcept {
            if (!mFile.IsOpen() || mEOF || maxFrames <= 0) return 0;
            const int32 bytesPerSamp = mBitsPerSamp / 8;
            const int32 bytesPerFrame = bytesPerSamp * mChannels;
            const nk_int64 remaining = mFrameCount - mCurFrame;
            if (remaining <= 0) { mEOF = true; return 0; }
            const int32 framesToRead = (nk_int64(maxFrames) > remaining)
                                       ? int32(remaining) : maxFrames;

            // Buffer scratch pour la lecture brute (max 4096 frames a la fois)
            const int32 chunkFrames = 4096;
            uint8 raw[4096 * 2 * 4]; // 4096 frames * 2 channels * 4 bytes max
            int32 totalRead = 0;
            while (totalRead < framesToRead) {
                const int32 wantFrames = (framesToRead - totalRead > chunkFrames)
                                         ? chunkFrames : (framesToRead - totalRead);
                const usize wantBytes = usize(wantFrames) * usize(bytesPerFrame);
                if (wantBytes > sizeof(raw)) {
                    // Securite (ne devrait pas arriver si chunkFrames est borne)
                    break;
                }
                const usize got = mFile.Read(raw, wantBytes);
                if (got == 0) { mEOF = true; break; }
                const int32 frames = int32(got / usize(bytesPerFrame));
                if (frames == 0) { mEOF = true; break; }

                // Conversion vers float32 selon le format
                float32* dst = outBuf + usize(totalRead) * usize(mChannels);
                if (mFormat == 3 && mBitsPerSamp == 32) {
                    // Float32 natif
                    ::memcpy(dst, raw, usize(frames) * usize(mChannels) * sizeof(float32));
                } else if (mFormat == 1 && mBitsPerSamp == 16) {
                    // PCM 16-bit signed -> float32 / 32768
                    const int16* src = reinterpret_cast<const int16*>(raw);
                    const int32 cnt = frames * mChannels;
                    for (int32 i = 0; i < cnt; ++i) {
                        dst[i] = float32(src[i]) * (1.0f / 32768.0f);
                    }
                } else if (mFormat == 1 && mBitsPerSamp == 8) {
                    // PCM 8-bit unsigned -> float32 (offset 128)
                    const uint8* src = raw;
                    const int32 cnt = frames * mChannels;
                    for (int32 i = 0; i < cnt; ++i) {
                        dst[i] = (float32(int32(src[i]) - 128)) * (1.0f / 128.0f);
                    }
                } else if (mFormat == 1 && mBitsPerSamp == 24) {
                    // PCM 24-bit signed packed -> float32 / 2^23
                    const uint8* src = raw;
                    const int32 cnt = frames * mChannels;
                    for (int32 i = 0; i < cnt; ++i) {
                        int32 v = (int32(src[0]) | (int32(src[1]) << 8) | (int32(src[2]) << 16));
                        if (v & 0x00800000) v |= 0xFF000000; // sign-extend
                        dst[i] = float32(v) * (1.0f / 8388608.0f);
                        src += 3;
                    }
                } else {
                    // Format non supporte
                    mEOF = true;
                    break;
                }

                totalRead += frames;
                mCurFrame += frames;
                if (frames < wantFrames) { mEOF = true; break; }
            }
            return totalRead;
        }

        bool WavStream::Seek(nk_int64 frameIdx) noexcept {
            if (!mFile.IsOpen()) return false;
            if (frameIdx < 0) frameIdx = 0;
            if (frameIdx > mFrameCount) frameIdx = mFrameCount;
            const int32 bytesPerFrame = (mBitsPerSamp / 8) * mChannels;
            mFile.Seek(mDataStart + frameIdx * bytesPerFrame, NkSeekOrigin::NK_BEGIN);
            mCurFrame = frameIdx;
            mEOF = (frameIdx >= mFrameCount);
            return true;
        }

        // ════════════════════════════════════════════════════════════════════
        //  MemoryStream (wrapper sur un AudioSample deja decode)
        // ════════════════════════════════════════════════════════════════════

        MemoryStream::MemoryStream(AudioSample sample) noexcept
            : mSample(sample), mCurFrame(0) {}

        MemoryStream::~MemoryStream() {
            // Libere le sample (l'allocateur stocke dans mSample.mAllocator)
            if (mSample.data) {
                AudioLoader::Free(mSample);
            }
        }

        int32 MemoryStream::ReadFrames(float32* outBuf, int32 maxFrames) noexcept {
            if (!mSample.data || maxFrames <= 0) return 0;
            const nk_int64 remaining = nk_int64(mSample.frameCount) - mCurFrame;
            if (remaining <= 0) return 0;
            const int32 nFrames = (nk_int64(maxFrames) > remaining)
                                  ? int32(remaining) : maxFrames;
            const usize off = usize(mCurFrame) * usize(mSample.channels);
            const usize cnt = usize(nFrames)   * usize(mSample.channels);
            ::memcpy(outBuf, mSample.data + off, cnt * sizeof(float32));
            mCurFrame += nFrames;
            return nFrames;
        }

        bool MemoryStream::Seek(nk_int64 frameIdx) noexcept {
            if (!mSample.data) return false;
            if (frameIdx < 0) frameIdx = 0;
            if (frameIdx > nk_int64(mSample.frameCount)) frameIdx = nk_int64(mSample.frameCount);
            mCurFrame = frameIdx;
            return true;
        }

        // ════════════════════════════════════════════════════════════════════
        //  Factory : detecte le format et instancie le stream approprie
        // ════════════════════════════════════════════════════════════════════

        IAudioStream* OpenAudioStream(const char* path) noexcept {
            if (!path) return nullptr;

            // Detection par extension
            const char* ext = nullptr;
            for (const char* p = path; *p; ++p) {
                if (*p == '.') ext = p + 1;
            }
            if (!ext) {
                logger.Error("[OpenAudioStream] Extension manquante : {0}", path);
                return nullptr;
            }

            // WAV : streaming reel
            if ((ext[0]=='w'||ext[0]=='W') && (ext[1]=='a'||ext[1]=='A')
             && (ext[2]=='v'||ext[2]=='V') && ext[3] == 0) {
                auto* s = memory::NkGetDefaultAllocator().New<WavStream>();
                if (!s->Open(path)) { memory::NkGetDefaultAllocator().Delete(s); return nullptr; }
                return s;
            }

            // FLAC / MP3 / OGG : decode complet puis MemoryStream
            // Utilise NkFile qui gere automatiquement AAssetManager Android :
            // un .flac dans androidassets() de l'APK est ouvert via AAsset.
            NkFile f;
            if (!f.Open(path, NkFileMode::NK_READ_BINARY)) {
                logger.Error("[OpenAudioStream] Impossible d'ouvrir : {0}", path);
                return nullptr;
            }
            usize size = usize(f.GetSize());
            uint8* buf = static_cast<uint8*>(memory::NkAlloc(size, nullptr, sizeof(uint8)));
            if (!buf) { f.Close(); return nullptr; }
            f.Read(buf, size);
            f.Close();

            AudioSample sample{};
            if ((ext[0]=='f'||ext[0]=='F') && (ext[1]=='l'||ext[1]=='L')
             && (ext[2]=='a'||ext[2]=='A') && (ext[3]=='c'||ext[3]=='C')) {
                sample = NkFLACCodec::Decode(buf, size, nullptr);
            } else if ((ext[0]=='m'||ext[0]=='M') && (ext[1]=='p'||ext[1]=='P')
                    && ext[2]=='3' && ext[3] == 0) {
                sample = NkMP3Codec::Decode(buf, size, nullptr);
            } else if ((ext[0]=='m'||ext[0]=='M') && (ext[1]=='p'||ext[1]=='P')
                    && (ext[2]=='g'||ext[2]=='G') && (ext[3]=='a'||ext[3]=='A')) {
                // .mpga = MPEG-1 Audio (variante extension .mp3)
                sample = NkMP3Codec::Decode(buf, size, nullptr);
            } else if ((ext[0]=='o'||ext[0]=='O') && (ext[1]=='g'||ext[1]=='G')
                    && (ext[2]=='g'||ext[2]=='G') && ext[3] == 0) {
                sample = NkOGGVorbisCodec::Decode(buf, size, nullptr);
            }
            memory::NkFree(buf, nullptr);

            if (!sample.IsValid()) {
                logger.Error("[OpenAudioStream] Decode echec pour : {0}", path);
                return nullptr;
            }
            return memory::NkGetDefaultAllocator().New<MemoryStream>(sample);
        }

    } // namespace audio
} // namespace nkentseu
