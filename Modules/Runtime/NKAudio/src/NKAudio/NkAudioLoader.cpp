// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioLoader.cpp
// DESCRIPTION: Implémentation du chargeur audio (WAV natif, stubs MP3/OGG/FLAC)
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Zéro STL. Décodeurs intégrés inline (pas de dépendances externes).
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NkAudio.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKCore/NkMacros.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>
#include <cstdio>

// ============================================================
// ANONYMOUS NAMESPACE — HELPERS INTERNES
// ============================================================

namespace {

    // ──────────────────────────────────────────────────────────────────────────
    // LECTURE LITTLE-ENDIAN PORTABLE (pas de memcpy UB)
    // ──────────────────────────────────────────────────────────────────────────

    inline nkentseu::uint16 ReadU16LE(const nkentseu::uint8* p) {
        return (nkentseu::uint16)(p[0] | ((nkentseu::uint16)p[1] << 8));
    }

    inline nkentseu::uint32 ReadU32LE(const nkentseu::uint8* p) {
        return (nkentseu::uint32)(p[0]
            | ((nkentseu::uint32)p[1] <<  8)
            | ((nkentseu::uint32)p[2] << 16)
            | ((nkentseu::uint32)p[3] << 24));
    }

    inline nkentseu::int16 ReadI16LE(const nkentseu::uint8* p) {
        return (nkentseu::int16)ReadU16LE(p);
    }

    // ──────────────────────────────────────────────────────────────────────────
    // STRUCTURES WAV (RIFF)
    // ──────────────────────────────────────────────────────────────────────────

    static const nkentseu::uint8 WAV_MAGIC[4]  = { 'R','I','F','F' };
    static const nkentseu::uint8 WAV_WAVE[4]   = { 'W','A','V','E' };
    static const nkentseu::uint8 CHUNK_FMT[4]  = { 'f','m','t',' ' };
    static const nkentseu::uint8 CHUNK_DATA[4] = { 'd','a','t','a' };

    static const nkentseu::uint16 WAV_PCM_FORMAT      = 1;
    static const nkentseu::uint16 WAV_IEEE_FLOAT_FORMAT= 3;
    static const nkentseu::uint16 WAV_EXTENSIBLE_FORMAT= 0xFFFE;

    // ──────────────────────────────────────────────────────────────────────────
    // CONVERSION PCM → FLOAT32
    // ──────────────────────────────────────────────────────────────────────────

    inline nkentseu::float32 Int8ToFloat(nkentseu::int8 s)   { return (nkentseu::float32)s / 127.0f; }
    inline nkentseu::float32 Int16ToFloat(nkentseu::int16 s) { return (nkentseu::float32)s / 32767.0f; }
    inline nkentseu::float32 Int24ToFloat(const nkentseu::uint8* p) {
        // Signe extension manuelle sur 24 bits → 32 bits signé
        nkentseu::int32 v = (nkentseu::int32)(p[0] | ((nkentseu::uint32)p[1] << 8) | ((nkentseu::uint32)p[2] << 16));
        if (v & 0x00800000) v |= (nkentseu::int32)0xFF000000; // Sign extend
        return (nkentseu::float32)v / 8388607.0f;
    }
    inline nkentseu::float32 Int32ToFloat(nkentseu::int32 s) { return (nkentseu::float32)s / 2147483647.0f; }

    // ──────────────────────────────────────────────────────────────────────────
    // FILE I/O (C standard - pas de STL fstream)
    // ──────────────────────────────────────────────────────────────────────────

    struct FileBuffer {
        nkentseu::uint8*             data     = nullptr;
        nkentseu::usize              size     = 0;
        nkentseu::memory::NkAllocator* alloc  = nullptr;

        bool Load(const char* path, nkentseu::memory::NkAllocator* a) {
            alloc = a;
            FILE* f = fopen(path, "rb");
            if (!f) return false;

            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (len <= 0) { fclose(f); return false; }

            size = (nkentseu::usize)len;
            if (alloc) {
                data = (nkentseu::uint8*)alloc->Allocate(size, 1);
            } else {
                data = (nkentseu::uint8*)::operator new(size);
            }
            if (!data) { fclose(f); return false; }

            fread(data, 1, size, f);
            fclose(f);
            return true;
        }

        ~FileBuffer() {
            if (data) {
                if (alloc) {
                    alloc->Free(data);
                } else {
                    ::operator delete(data);
                }
                data = nullptr;
            }
        }
    };

} // anonymous namespace

// ============================================================
// IMPLEMENTATIONS
// ============================================================

namespace nkentseu {
    namespace audio {

        // ──────────────────────────────────────────────────────────────────────
        // DÉTECTION FORMAT
        // ──────────────────────────────────────────────────────────────────────

        AudioFormat AudioLoader::DetectFormat(const uint8* data, usize size) {
            if (!data || size < 4) return AudioFormat::UNKNOWN;

            // WAV : RIFF header
            if (size >= 12 &&
                data[0]=='R' && data[1]=='I' && data[2]=='F' && data[3]=='F' &&
                data[8]=='W' && data[9]=='A' && data[10]=='V' && data[11]=='E') {
                return AudioFormat::WAV;
            }

            // MP3 : ID3 tag ou sync word
            if ((size >= 3 && data[0]=='I' && data[1]=='D' && data[2]=='3') ||
                (size >= 2 && (data[0]==0xFF) && (data[1] & 0xE0)==0xE0)) {
                return AudioFormat::MP3;
            }

            // OGG : capture pattern
            if (size >= 4 && data[0]=='O' && data[1]=='g' && data[2]=='g' && data[3]=='S') {
                return AudioFormat::OGG;
            }

            // FLAC : fLaC marker
            if (size >= 4 && data[0]=='f' && data[1]=='L' && data[2]=='a' && data[3]=='C') {
                return AudioFormat::FLAC;
            }

            // AIFF / AIFC
            if (size >= 12 &&
                data[0]=='F' && data[1]=='O' && data[2]=='R' && data[3]=='M' &&
                (data[8]=='A' && data[9]=='I' && data[10]=='F')) {
                return AudioFormat::AIFF;
            }

            return AudioFormat::UNKNOWN;
        }

        AudioFormat AudioLoader::DetectFormatFromPath(const char* path) {
            if (!path) return AudioFormat::UNKNOWN;

            // Cherche la dernière extension
            const char* ext = nullptr;
            for (const char* p = path; *p; ++p) {
                if (*p == '.') ext = p + 1;
            }
            if (!ext) return AudioFormat::UNKNOWN;

            // Comparaison case-insensitive simple
            auto IEq = [](const char* a, const char* b) -> bool {
                while (*a && *b) {
                    char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
                    char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
                    if (ca != cb) return false;
                    ++a; ++b;
                }
                return *a == 0 && *b == 0;
            };

            if (IEq(ext, "wav")  || IEq(ext, "wave")) return AudioFormat::WAV;
            if (IEq(ext, "mp3"))                       return AudioFormat::MP3;
            if (IEq(ext, "ogg")  || IEq(ext, "oga"))  return AudioFormat::OGG;
            if (IEq(ext, "flac"))                      return AudioFormat::FLAC;
            if (IEq(ext, "opus"))                      return AudioFormat::OPUS;
            if (IEq(ext, "aif")  || IEq(ext, "aiff")) return AudioFormat::AIFF;

            return AudioFormat::UNKNOWN;
        }

        // ──────────────────────────────────────────────────────────────────────
        // LOAD DEPUIS LE DISQUE
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioLoader::Load(const char* path, memory::NkAllocator* allocator) {
            AudioSample invalid{};
            if (!path) return invalid;

            // Charger le fichier entier en mémoire tampon temporaire
            FileBuffer fb;
            if (!fb.Load(path, allocator)) return invalid;

            AudioFormat fmt = DetectFormat(fb.data, fb.size);
            if (fmt == AudioFormat::UNKNOWN) {
                fmt = DetectFormatFromPath(path);
            }

            return LoadFromMemory(fb.data, fb.size, fmt, allocator);
            // fb libéré automatiquement en sortie
        }

        AudioSample AudioLoader::LoadFromMemory(const uint8* data, usize size,
                                                 AudioFormat format,
                                                 memory::NkAllocator* allocator) {
            AudioSample invalid{};
            if (!data || size == 0) return invalid;

            if (format == AudioFormat::UNKNOWN) {
                format = DetectFormat(data, size);
            }

            switch (format) {
                case AudioFormat::WAV:  return LoadWAV (data, size, allocator);
                case AudioFormat::MP3:  return LoadMP3 (data, size, allocator);
                case AudioFormat::OGG:  return LoadOGG (data, size, allocator);
                case AudioFormat::FLAC: return LoadFLAC(data, size, allocator);
                default:                return invalid;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // LOADER WAV (COMPLET — NATIF, ZÉRO STL)
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioLoader::LoadWAV(const uint8* data, usize size,
                                          memory::NkAllocator* allocator) {
            AudioSample result{};

            // Validation minimum
            if (size < 44) return result;

            // RIFF header
            if (data[0]!='R'||data[1]!='I'||data[2]!='F'||data[3]!='F') return result;
            if (data[8]!='W'||data[9]!='A'||data[10]!='V'||data[11]!='E') return result;

            usize offset = 12;

            uint16 audioFormat    = 0;
            uint16 numChannels    = 0;
            uint32 sampleRate     = 0;
            uint16 bitsPerSample  = 0;
            const uint8* pcmData  = nullptr;
            usize  pcmSize        = 0;

            // Parcourir les chunks RIFF
            while (offset + 8 <= size) {
                const uint8* chunkId   = data + offset;
                uint32        chunkSize = ReadU32LE(data + offset + 4);
                offset += 8;

                if (chunkSize > size - offset) break;

                if (chunkId[0]=='f'&&chunkId[1]=='m'&&chunkId[2]=='t'&&chunkId[3]==' ') {
                    // fmt chunk (min 16 bytes)
                    if (chunkSize < 16) break;
                    const uint8* f = data + offset;
                    audioFormat   = ReadU16LE(f + 0);
                    numChannels   = ReadU16LE(f + 2);
                    sampleRate    = ReadU32LE(f + 4);
                    bitsPerSample = ReadU16LE(f + 14);

                } else if (chunkId[0]=='d'&&chunkId[1]=='a'&&chunkId[2]=='t'&&chunkId[3]=='a') {
                    pcmData = data + offset;
                    pcmSize = chunkSize;
                }

                // Aligner sur 2 bytes (RIFF spec)
                offset += (chunkSize + 1) & ~1u;
            }

            if (!pcmData || pcmSize == 0 || numChannels == 0 || sampleRate == 0) {
                return result;
            }

            bool isPcm      = (audioFormat == WAV_PCM_FORMAT);
            bool isFloat     = (audioFormat == WAV_IEEE_FLOAT_FORMAT);
            bool isExtensible= (audioFormat == WAV_EXTENSIBLE_FORMAT);

            if (!isPcm && !isFloat && !isExtensible) return result; // format non supporté

            uint32 bytesPerSample = bitsPerSample / 8u;
            if (bytesPerSample == 0) return result;

            usize frameCount = pcmSize / (numChannels * bytesPerSample);

            // Allouer le buffer Float32 de sortie
            usize sampleCount = frameCount * numChannels;
            float32* outData = nullptr;
            if (allocator) {
                outData = (float32*)allocator->Allocate(sampleCount * sizeof(float32), sizeof(float32));
            } else {
                outData = (float32*)::operator new(sampleCount * sizeof(float32));
            }
            if (!outData) return result;

            // Conversion vers Float32
            const uint8* src = pcmData;

            if (isFloat && bitsPerSample == 32) {
                // Float32 natif — copie directe
                memcpy(outData, src, sampleCount * sizeof(float32));

            } else if (isFloat && bitsPerSample == 64) {
                // Float64 → Float32
                for (usize i = 0; i < sampleCount; ++i) {
                    double v;
                    memcpy(&v, src + i * 8, 8);
                    outData[i] = (float32)v;
                }

            } else if (isPcm || isExtensible) {
                if (bitsPerSample == 8) {
                    for (usize i = 0; i < sampleCount; ++i) {
                        outData[i] = Int8ToFloat((int8)(src[i] - 128)); // WAV 8-bit est unsigned
                    }
                } else if (bitsPerSample == 16) {
                    for (usize i = 0; i < sampleCount; ++i) {
                        outData[i] = Int16ToFloat(ReadI16LE(src + i * 2));
                    }
                } else if (bitsPerSample == 24) {
                    for (usize i = 0; i < sampleCount; ++i) {
                        outData[i] = Int24ToFloat(src + i * 3);
                    }
                } else if (bitsPerSample == 32) {
                    for (usize i = 0; i < sampleCount; ++i) {
                        int32 v = (int32)ReadU32LE(src + i * 4);
                        outData[i] = Int32ToFloat(v);
                    }
                } else {
                    // Format non géré
                    if (allocator) allocator->Free(outData);
                    else ::operator delete(outData);
                    return result;
                }
            }

            result.data        = outData;
            result.frameCount  = frameCount;
            result.sampleRate  = (int32)sampleRate;
            result.channels    = (int32)numChannels;
            result.format      = AudioFormat::WAV;
            result.mAllocator  = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // LOADERS STUBS (MP3, OGG, FLAC)
        // NOTE: Intégrer minimp3.h / stb_vorbis.c / dr_flac.h selon besoin
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioLoader::LoadMP3(const uint8* data, usize size,
                                          memory::NkAllocator* allocator) {
            // TODO: Intégrer minimp3.h (header-only, ~60Ko, MIT license, zéro STL)
            // #define MINIMP3_IMPLEMENTATION
            // #include "minimp3.h"
            (void)data; (void)size; (void)allocator;
            return {};
        }

        AudioSample AudioLoader::LoadOGG(const uint8* data, usize size,
                                          memory::NkAllocator* allocator) {
            // TODO: Intégrer stb_vorbis.c (header-only, ~200Ko, public domain)
            // #define STB_VORBIS_NO_STDIO
            // #include "stb_vorbis.c"
            (void)data; (void)size; (void)allocator;
            return {};
        }

        AudioSample AudioLoader::LoadFLAC(const uint8* data, usize size,
                                           memory::NkAllocator* allocator) {
            // TODO: Intégrer dr_flac.h (header-only, ~130Ko, MIT license)
            // #define DR_FLAC_IMPLEMENTATION
            // #include "dr_flac.h"
            (void)data; (void)size; (void)allocator;
            return {};
        }

        // ──────────────────────────────────────────────────────────────────────
        // SAVE WAV
        // ──────────────────────────────────────────────────────────────────────

        bool AudioLoader::SaveWAV(const char* path, const AudioSample& sample) {
            if (!path || !sample.IsValid()) return false;

            FILE* f = fopen(path, "wb");
            if (!f) return false;

            usize sampleCount = sample.GetSampleCount();
            uint32 dataSize   = (uint32)(sampleCount * sizeof(int16)); // export en 16-bit

            // RIFF header (44 bytes)
            uint8 header[44];
            memset(header, 0, 44);

            // RIFF
            header[0]='R'; header[1]='I'; header[2]='F'; header[3]='F';
            uint32 riffSize = 36 + dataSize;
            header[4]=(uint8)(riffSize);
            header[5]=(uint8)(riffSize>>8);
            header[6]=(uint8)(riffSize>>16);
            header[7]=(uint8)(riffSize>>24);
            header[8]='W'; header[9]='A'; header[10]='V'; header[11]='E';

            // fmt chunk
            header[12]='f'; header[13]='m'; header[14]='t'; header[15]=' ';
            header[16]=16; // chunk size
            header[20]=1;  // PCM
            header[22]=(uint8)sample.channels;
            uint32 sr = (uint32)sample.sampleRate;
            header[24]=(uint8)(sr); header[25]=(uint8)(sr>>8);
            header[26]=(uint8)(sr>>16); header[27]=(uint8)(sr>>24);
            uint32 byteRate = sr * (uint32)sample.channels * 2;
            header[28]=(uint8)(byteRate); header[29]=(uint8)(byteRate>>8);
            header[30]=(uint8)(byteRate>>16); header[31]=(uint8)(byteRate>>24);
            header[32]=(uint8)(sample.channels * 2); // block align
            header[34]=16; // bits per sample

            // data chunk
            header[36]='d'; header[37]='a'; header[38]='t'; header[39]='a';
            header[40]=(uint8)(dataSize); header[41]=(uint8)(dataSize>>8);
            header[42]=(uint8)(dataSize>>16); header[43]=(uint8)(dataSize>>24);

            fwrite(header, 1, 44, f);

            // Écrire samples en Int16
            for (usize i = 0; i < sampleCount; ++i) {
                float32 s = sample.data[i];
                // Clamp
                if (s >  1.0f) s =  1.0f;
                if (s < -1.0f) s = -1.0f;
                int16 pcm = (int16)(s * 32767.0f);
                uint8 bytes[2] = { (uint8)(pcm), (uint8)((uint16)pcm >> 8) };
                fwrite(bytes, 1, 2, f);
            }

            fclose(f);
            return true;
        }

        // ──────────────────────────────────────────────────────────────────────
        // FREE
        // ──────────────────────────────────────────────────────────────────────

        void AudioLoader::Free(AudioSample& sample) {
            if (!sample.data) return;

            if (sample.mAllocator) {
                sample.mAllocator->Free(sample.data);
            } else {
                ::operator delete(sample.data);
            }
            sample.data       = nullptr;
            sample.frameCount = 0;
            sample.mAllocator = nullptr;
        }

        // ──────────────────────────────────────────────────────────────────────
        // RESAMPLE (LINÉAIRE)
        // ──────────────────────────────────────────────────────────────────────

        void AudioLoader::Resample(AudioSample& sample, int32 targetSampleRate, bool highQuality) {
            if (!sample.IsValid() || sample.sampleRate == targetSampleRate) return;

            if (highQuality) {
                ResampleSinc(sample, targetSampleRate);
            } else {
                ResampleLinear(sample, targetSampleRate);
            }
        }

        void AudioLoader::ResampleLinear(AudioSample& sample, int32 targetSampleRate) {
            if (!sample.IsValid()) return;

            double ratio      = (double)targetSampleRate / (double)sample.sampleRate;
            usize  newFrames  = (usize)((double)sample.frameCount * ratio);
            int32  ch         = sample.channels;

            float32* newData = nullptr;
            if (sample.mAllocator) {
                newData = (float32*)sample.mAllocator->Allocate(newFrames * (usize)ch * sizeof(float32), sizeof(float32));
            } else {
                newData = (float32*)::operator new(newFrames * (usize)ch * sizeof(float32));
            }
            if (!newData) return;

            for (usize outFrame = 0; outFrame < newFrames; ++outFrame) {
                double srcPos   = (double)outFrame / ratio;
                usize  srcFloor = (usize)srcPos;
                float32 frac    = (float32)(srcPos - (double)srcFloor);

                if (srcFloor + 1 >= sample.frameCount) srcFloor = sample.frameCount - 2;

                for (int32 c = 0; c < ch; ++c) {
                    float32 a = sample.data[srcFloor * (usize)ch + (usize)c];
                    float32 b = sample.data[(srcFloor+1) * (usize)ch + (usize)c];
                    newData[outFrame * (usize)ch + (usize)c] = a + (b - a) * frac;
                }
            }

            // Remplacer le buffer
            if (sample.mAllocator) sample.mAllocator->Free(sample.data);
            else ::operator delete(sample.data);

            sample.data        = newData;
            sample.frameCount  = newFrames;
            sample.sampleRate  = targetSampleRate;
        }

        void AudioLoader::ResampleSinc(AudioSample& sample, int32 targetSampleRate) {
            // Pour l'instant, fallback linéaire
            // TODO: Implémenter Kaiser-windowed Sinc pour qualité studio
            ResampleLinear(sample, targetSampleRate);
        }

        // ──────────────────────────────────────────────────────────────────────
        // CONVERT CHANNELS
        // ──────────────────────────────────────────────────────────────────────

        void AudioLoader::ConvertChannels(AudioSample& sample, int32 targetChannels) {
            if (!sample.IsValid() || sample.channels == targetChannels) return;

            usize   newCount = sample.frameCount * (usize)targetChannels;
            float32* newData = nullptr;

            if (sample.mAllocator) {
                newData = (float32*)sample.mAllocator->Allocate(newCount * sizeof(float32), sizeof(float32));
            } else {
                newData = (float32*)::operator new(newCount * sizeof(float32));
            }
            if (!newData) return;

            int32 srcCh = sample.channels;
            int32 dstCh = targetChannels;

            for (usize f = 0; f < sample.frameCount; ++f) {
                const float32* srcFrame = sample.data + f * (usize)srcCh;
                float32*       dstFrame = newData      + f * (usize)dstCh;

                if (srcCh == 1 && dstCh == 2) {
                    // Mono → Stéréo
                    dstFrame[0] = srcFrame[0];
                    dstFrame[1] = srcFrame[0];

                } else if (srcCh == 2 && dstCh == 1) {
                    // Stéréo → Mono (downmix)
                    dstFrame[0] = (srcFrame[0] + srcFrame[1]) * 0.5f;

                } else {
                    // Cas général : copier ce qui peut l'être, zero-pad sinon
                    for (int32 c = 0; c < dstCh; ++c) {
                        dstFrame[c] = (c < srcCh) ? srcFrame[c] : 0.0f;
                    }
                }
            }

            if (sample.mAllocator) sample.mAllocator->Free(sample.data);
            else ::operator delete(sample.data);

            sample.data     = newData;
            sample.channels = dstCh;
        }

        // ConvertSampleFormat : données internes toujours en Float32,
        // cette fonction est un no-op (conversion faite au chargement)
        void AudioLoader::ConvertSampleFormat(AudioSample& /*sample*/, SampleFormat /*target*/) {
            // Format interne : Float32 — conversion transparente au Load()
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
