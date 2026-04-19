// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioMixer.cpp
// DESCRIPTION: Mixeur audio offline (composition de pistes, normalisation, fades)
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#include "NkAudio.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

namespace {
    inline nkentseu::float32 Clampf(nkentseu::float32 v, nkentseu::float32 lo, nkentseu::float32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    inline nkentseu::float32 Fabsf(nkentseu::float32 x) { return ::fabsf(x); }
    inline nkentseu::float32 Sqrtf(nkentseu::float32 x) { return ::sqrtf(x); }
    inline nkentseu::float32 Cosf (nkentseu::float32 x) { return ::cosf(x); }
    constexpr nkentseu::float32 HALF_PI = 1.5707963267948966f;

    inline nkentseu::float32* AllocFloat(nkentseu::usize n, nkentseu::memory::NkAllocator* a) {
        if (a) return (nkentseu::float32*)a->Allocate(n * sizeof(nkentseu::float32), sizeof(nkentseu::float32));
        return (nkentseu::float32*)::operator new(n * sizeof(nkentseu::float32));
    }
    inline void FreeFloat(nkentseu::float32* p, nkentseu::memory::NkAllocator* a) {
        if (!p) return;
        if (a) a->Free(p);
        else ::operator delete(p);
    }
} // anonymous namespace

namespace nkentseu {
    namespace audio {

        AudioMixer::AudioMixer(int32 sampleRate, int32 channels, memory::NkAllocator* allocator)
            : mSampleRate(sampleRate)
            , mChannels(channels)
            , mAllocator(allocator)
        { }

        AudioSample AudioMixer::AllocateSample(usize frameCount) const {
            AudioSample s{};
            usize total = frameCount * (usize)mChannels;
            s.data       = AllocFloat(total, mAllocator);
            s.frameCount = frameCount;
            s.sampleRate = mSampleRate;
            s.channels   = mChannels;
            s.format     = AudioFormat::RAW;
            s.mAllocator = mAllocator;
            if (s.data) memset(s.data, 0, total * sizeof(float32));
            return s;
        }

        // ──────────────────────────────────────────────────────────────────────
        // MIX
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioMixer::Mix(const AudioSample* samples, int32 count, const float32* volumes) {
            AudioSample result{};
            if (!samples || count <= 0) return result;

            // Durée = maximum des durées
            usize maxFrames = 0;
            for (int32 i = 0; i < count; ++i) {
                if (samples[i].IsValid() && samples[i].frameCount > maxFrames)
                    maxFrames = samples[i].frameCount;
            }
            if (maxFrames == 0) return result;

            result = AllocateSample(maxFrames);
            if (!result.data) return result;

            for (int32 s = 0; s < count; ++s) {
                if (!samples[s].IsValid()) continue;

                float32 vol    = volumes ? volumes[s] : 1.0f;
                int32   srcCh  = samples[s].channels;
                usize   srcF   = samples[s].frameCount;

                for (usize f = 0; f < srcF && f < maxFrames; ++f) {
                    for (int32 c = 0; c < mChannels; ++c) {
                        int32 srcC = (c < srcCh) ? c : srcCh - 1;
                        result.data[f * (usize)mChannels + (usize)c] +=
                            samples[s].data[f * (usize)srcCh + (usize)srcC] * vol;
                    }
                }
            }
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // CROSSFADE
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioMixer::Crossfade(const AudioSample& a,
                                           const AudioSample& b,
                                           float32            crossfadeLen) {
            AudioSample result{};
            if (!a.IsValid() || !b.IsValid()) return result;

            usize cfFrames  = (usize)(crossfadeLen * (float32)mSampleRate);
            usize totalOut  = a.frameCount + b.frameCount - cfFrames;
            if (totalOut < a.frameCount) totalOut = a.frameCount;

            result = AllocateSample(totalOut);
            if (!result.data) return result;

            int32 aCh = a.channels;
            int32 bCh = b.channels;

            for (usize f = 0; f < totalOut; ++f) {
                for (int32 c = 0; c < mChannels; ++c) {
                    int32   aC   = (c < aCh) ? c : aCh - 1;
                    int32   bC   = (c < bCh) ? c : bCh - 1;

                    float32 sampA = (f < a.frameCount) ? a.data[f * (usize)aCh + (usize)aC] : 0.0f;

                    usize bFrame = (f > a.frameCount - cfFrames) ? (f - (a.frameCount - cfFrames)) : 0;
                    float32 sampB = (bFrame < b.frameCount) ? b.data[bFrame * (usize)bCh + (usize)bC] : 0.0f;

                    float32 crossT = 0.0f;
                    if (f >= a.frameCount - cfFrames && cfFrames > 0) {
                        crossT = (float32)(f - (a.frameCount - cfFrames)) / (float32)cfFrames;
                        crossT = Clampf(crossT, 0.0f, 1.0f);
                    } else if (f >= a.frameCount) {
                        crossT = 1.0f;
                    }

                    // Constant-power crossfade
                    float32 gainA = Cosf(crossT * HALF_PI);
                    float32 gainB = Cosf((1.0f - crossT) * HALF_PI);

                    result.data[f * (usize)mChannels + (usize)c] = sampA * gainA + sampB * gainB;
                }
            }
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // CONCATENATE
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioMixer::Concatenate(const AudioSample* samples, int32 count) {
            AudioSample result{};
            if (!samples || count <= 0) return result;

            usize totalFrames = 0;
            for (int32 i = 0; i < count; ++i)
                if (samples[i].IsValid()) totalFrames += samples[i].frameCount;

            if (totalFrames == 0) return result;
            result = AllocateSample(totalFrames);
            if (!result.data) return result;

            usize offset = 0;
            for (int32 i = 0; i < count; ++i) {
                if (!samples[i].IsValid()) continue;

                int32 srcCh = samples[i].channels;
                usize srcF  = samples[i].frameCount;

                for (usize f = 0; f < srcF; ++f) {
                    for (int32 c = 0; c < mChannels; ++c) {
                        int32 srcC = (c < srcCh) ? c : srcCh - 1;
                        result.data[(offset + f) * (usize)mChannels + (usize)c] =
                            samples[i].data[f * (usize)srcCh + (usize)srcC];
                    }
                }
                offset += srcF;
            }
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // NORMALIZE
        // ──────────────────────────────────────────────────────────────────────

        void AudioMixer::Normalize(AudioSample& sample, float32 targetPeak) {
            if (!sample.IsValid()) return;

            float32 peak = 0.0f;
            usize   total = sample.GetSampleCount();
            for (usize i = 0; i < total; ++i) {
                float32 v = Fabsf(sample.data[i]);
                if (v > peak) peak = v;
            }
            if (peak < 0.000001f) return;

            float32 gain = targetPeak / peak;
            for (usize i = 0; i < total; ++i) sample.data[i] *= gain;
        }

        // ──────────────────────────────────────────────────────────────────────
        // FADE IN / OUT
        // ──────────────────────────────────────────────────────────────────────

        void AudioMixer::FadeIn(AudioSample& sample, float32 duration) {
            if (!sample.IsValid()) return;
            usize fadeFrames = (usize)(duration * (float32)sample.sampleRate);
            if (fadeFrames > sample.frameCount) fadeFrames = sample.frameCount;
            int32 ch = sample.channels;
            for (usize f = 0; f < fadeFrames; ++f) {
                float32 gain = (float32)f / (float32)fadeFrames;
                // Constant-power
                gain = Cosf((1.0f - gain) * HALF_PI);
                for (int32 c = 0; c < ch; ++c) sample.data[f * (usize)ch + (usize)c] *= gain;
            }
        }

        void AudioMixer::FadeOut(AudioSample& sample, float32 duration) {
            if (!sample.IsValid()) return;
            usize fadeFrames = (usize)(duration * (float32)sample.sampleRate);
            if (fadeFrames > sample.frameCount) fadeFrames = sample.frameCount;
            usize startFrame = sample.frameCount - fadeFrames;
            int32 ch = sample.channels;
            for (usize f = 0; f < fadeFrames; ++f) {
                float32 gain = Cosf(((float32)f / (float32)fadeFrames) * HALF_PI);
                usize   idx  = startFrame + f;
                for (int32 c = 0; c < ch; ++c) sample.data[idx * (usize)ch + (usize)c] *= gain;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // APPLY GAIN
        // ──────────────────────────────────────────────────────────────────────

        void AudioMixer::ApplyGain(AudioSample& sample, float32 gain) {
            if (!sample.IsValid()) return;
            usize total = sample.GetSampleCount();
            for (usize i = 0; i < total; ++i) sample.data[i] *= gain;
        }

        // ──────────────────────────────────────────────────────────────────────
        // REVERSE
        // ──────────────────────────────────────────────────────────────────────

        void AudioMixer::Reverse(AudioSample& sample) {
            if (!sample.IsValid()) return;
            int32 ch = sample.channels;
            usize lo = 0, hi = sample.frameCount - 1;
            while (lo < hi) {
                for (int32 c = 0; c < ch; ++c) {
                    float32 tmp = sample.data[lo * (usize)ch + (usize)c];
                    sample.data[lo * (usize)ch + (usize)c] = sample.data[hi * (usize)ch + (usize)c];
                    sample.data[hi * (usize)ch + (usize)c] = tmp;
                }
                ++lo; --hi;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // INSERT
        // ──────────────────────────────────────────────────────────────────────

        void AudioMixer::Insert(AudioSample& target, const AudioSample& insert,
                                 float32 positionSec, bool mix) {
            if (!target.IsValid() || !insert.IsValid()) return;

            usize startFrame = (usize)(positionSec * (float32)target.sampleRate);
            if (startFrame >= target.frameCount) return;

            int32 tCh  = target.channels;
            int32 iCh  = insert.channels;
            usize iEnd = startFrame + insert.frameCount;
            if (iEnd > target.frameCount) iEnd = target.frameCount;

            for (usize f = startFrame; f < iEnd; ++f) {
                usize iFrame = f - startFrame;
                for (int32 c = 0; c < tCh; ++c) {
                    int32   iC   = (c < iCh) ? c : iCh - 1;
                    float32 src  = insert.data[iFrame * (usize)iCh + (usize)iC];
                    if (mix) target.data[f * (usize)tCh + (usize)c] += src;
                    else     target.data[f * (usize)tCh + (usize)c]  = src;
                }
            }
        }

        void AudioMixer::NormalizeSample(AudioSample& sample) const {
            // Conversion canaux si nécessaire
            if (sample.channels != mChannels) {
                AudioLoader::ConvertChannels(sample, mChannels);
            }
            // Conversion sample rate si nécessaire
            if (sample.sampleRate != mSampleRate) {
                AudioLoader::Resample(sample, mSampleRate, false);
            }
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
