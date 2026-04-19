// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioGenerator.cpp
// DESCRIPTION: Synthèse audio procédurale (oscillateurs, drums, enveloppes)
//              Moteur de synthèse pour DAW, beatmakers, SFX temps réel
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Zéro STL. Implémentation AAA-grade des générateurs de sons.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NkAudio.h"
#include "NKCore/NkMacros.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

// ============================================================
// ANONYMOUS NAMESPACE — CONSTANTES ET HELPERS
// ============================================================

namespace {

    constexpr nkentseu::float32 TWO_PI = 6.28318530717958647692f;
    constexpr nkentseu::float32 PI     = 3.14159265358979323846f;

    // ──────────────────────────────────────────────────────────────────────────
    // ALLOCATION HELPER
    // ──────────────────────────────────────────────────────────────────────────

    inline nkentseu::float32* AllocFloat(nkentseu::usize count, nkentseu::memory::NkAllocator* alloc) {
        if (alloc) {
            return (nkentseu::float32*)alloc->Allocate(count * sizeof(nkentseu::float32), sizeof(nkentseu::float32));
        }
        return (nkentseu::float32*)::operator new(count * sizeof(nkentseu::float32));
    }

    // ──────────────────────────────────────────────────────────────────────────
    // FONCTIONS MATHÉMATIQUES (sans <cmath> si besoin de portage)
    // ──────────────────────────────────────────────────────────────────────────

    inline nkentseu::float32 NkSin(nkentseu::float32 x) { return ::sinf(x); }
    inline nkentseu::float32 NkCos(nkentseu::float32 x) { return ::cosf(x); }
    inline nkentseu::float32 NkFmod(nkentseu::float32 x, nkentseu::float32 y) { return ::fmodf(x, y); }
    inline nkentseu::float32 NkPow(nkentseu::float32 b, nkentseu::float32 e) { return ::powf(b, e); }
    inline nkentseu::float32 NkExp(nkentseu::float32 x) { return ::expf(x); }
    inline nkentseu::float32 NkLog(nkentseu::float32 x) { return ::logf(x); }
    inline nkentseu::float32 NkSqrt(nkentseu::float32 x) { return ::sqrtf(x); }
    inline nkentseu::float32 NkFabs(nkentseu::float32 x) { return ::fabsf(x); }
    inline nkentseu::float32 NkClamp(nkentseu::float32 v, nkentseu::float32 lo, nkentseu::float32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

} // anonymous namespace

// ============================================================
// IMPLÉMENTATIONS
// ============================================================

namespace nkentseu {
    namespace audio {

        // ──────────────────────────────────────────────────────────────────────
        // ÉVALUATION D'OSCILLATEUR
        // ──────────────────────────────────────────────────────────────────────

        float32 AudioGenerator::EvaluateWaveform(WaveformType waveform,
                                                  float32      phase,
                                                  float32      pulseWidth) {
            // Phase normalisée [0, 2π]
            float32 normPhase = NkFmod(phase, TWO_PI);
            if (normPhase < 0.0f) normPhase += TWO_PI;
            float32 t = normPhase / TWO_PI; // [0, 1)

            switch (waveform) {
                case WaveformType::SINE:
                    return NkSin(phase);

                case WaveformType::SQUARE:
                    return (t < 0.5f) ? 1.0f : -1.0f;

                case WaveformType::PULSE:
                    return (t < pulseWidth) ? 1.0f : -1.0f;

                case WaveformType::TRIANGLE:
                    if (t < 0.25f)       return  4.0f * t;
                    else if (t < 0.75f)  return  2.0f - 4.0f * t;
                    else                 return -4.0f + 4.0f * t;

                case WaveformType::SAWTOOTH:
                    return 2.0f * t - 1.0f;

                case WaveformType::NOISE_WHITE:
                case WaveformType::NOISE_PINK:
                    // Bruit géré séparément (nécessite état)
                    return 0.0f;

                default:
                    return 0.0f;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // BRUIT ROSE (Voss-McCartney, 7 niveaux)
        // ──────────────────────────────────────────────────────────────────────

        float32 AudioGenerator::GeneratePinkNoiseSample(PinkNoiseState& state) {
            // LCG simple pour bruit blanc rapide
            static uint32 rng = 12345u;
            auto whiteNoise = [&]() -> float32 {
                rng = rng * 1664525u + 1013904223u;
                return (float32)((int32)rng) / (float32)0x7FFFFFFF;
            };

            // Filtre Voss-McCartney 7 étages
            static const float32 A[7] = {
                0.99886f, 0.99332f, 0.96900f, 0.86650f,
                0.55000f, -0.7616f, 0.115926f
            };
            static const float32 B[7] = {
                0.0555179f, 0.0750759f, 0.1538520f, 0.3104856f,
                0.5329522f, -0.0168980f, 0.5362f
            };

            float32 white = whiteNoise();
            float32 pink  = 0.0f;

            for (int32 i = 0; i < 7; ++i) {
                state.b[i] = A[i] * state.b[i] + white * B[i];
                pink += state.b[i];
            }
            pink += white * 0.5362f;
            return NkClamp(pink * 0.11f, -1.0f, 1.0f);
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE TONE
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateTone(float32              frequency,
                                                  float32              duration,
                                                  WaveformType         waveform,
                                                  int32                sampleRate,
                                                  float32              amplitude,
                                                  memory::NkAllocator* allocator) {
            AudioSample result{};
            if (frequency <= 0.0f || duration <= 0.0f || sampleRate <= 0) return result;

            usize   frameCount  = (usize)((float32)sampleRate * duration);
            usize   totalSamples = frameCount; // mono output
            float32* data        = AllocFloat(totalSamples, allocator);
            if (!data) return result;

            float32 phaseInc = TWO_PI * frequency / (float32)sampleRate;
            float32 phase    = 0.0f;

            if (waveform == WaveformType::NOISE_WHITE) {
                uint32 rng = 54321u;
                for (usize i = 0; i < totalSamples; ++i) {
                    rng = rng * 1664525u + 1013904223u;
                    data[i] = ((float32)((int32)rng) / (float32)0x7FFFFFFF) * amplitude;
                    (void)phase;
                }
            } else if (waveform == WaveformType::NOISE_PINK) {
                PinkNoiseState pns{};
                for (usize i = 0; i < totalSamples; ++i) {
                    data[i] = GeneratePinkNoiseSample(pns) * amplitude;
                }
            } else {
                for (usize i = 0; i < totalSamples; ++i) {
                    data[i] = EvaluateWaveform(waveform, phase) * amplitude;
                    phase += phaseInc;
                }
            }

            result.data        = data;
            result.frameCount  = frameCount;
            result.sampleRate  = sampleRate;
            result.channels    = 1;
            result.format      = AudioFormat::RAW;
            result.mAllocator  = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE NOISE
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateNoise(float32              duration,
                                                   WaveformType         type,
                                                   int32                sampleRate,
                                                   float32              amplitude,
                                                   memory::NkAllocator* allocator) {
            return GenerateTone(440.0f, duration, type, sampleRate, amplitude, allocator);
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE CHIRP
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateChirp(float32              startFreq,
                                                   float32              endFreq,
                                                   float32              duration,
                                                   int32                sampleRate,
                                                   memory::NkAllocator* allocator) {
            AudioSample result{};
            if (duration <= 0.0f || sampleRate <= 0) return result;

            usize    frameCount = (usize)((float32)sampleRate * duration);
            float32* data       = AllocFloat(frameCount, allocator);
            if (!data) return result;

            // Chirp logarithmique (plus naturel musicalement)
            float32 logStart = NkLog(startFreq);
            float32 logEnd   = NkLog(endFreq);

            float32 phase = 0.0f;
            for (usize i = 0; i < frameCount; ++i) {
                float32 t    = (float32)i / (float32)sampleRate;
                float32 tNorm = t / duration;
                // Fréquence instantanée interpolation log
                float32 freq = NkPow(2.718281828f, logStart + (logEnd - logStart) * tNorm);
                phase += TWO_PI * freq / (float32)sampleRate;
                data[i] = NkSin(phase) * 0.8f;
            }

            result.data       = data;
            result.frameCount = frameCount;
            result.sampleRate = sampleRate;
            result.channels   = 1;
            result.format     = AudioFormat::RAW;
            result.mAllocator = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE CHORD (SYNTHÈSE ADDITIVE)
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateChord(const float32*       frequencies,
                                                   int32                count,
                                                   float32              duration,
                                                   const float32*       amplitudes,
                                                   int32                sampleRate,
                                                   memory::NkAllocator* allocator) {
            AudioSample result{};
            if (!frequencies || count <= 0 || duration <= 0.0f || sampleRate <= 0) return result;

            usize    frameCount = (usize)((float32)sampleRate * duration);
            float32* data       = AllocFloat(frameCount, allocator);
            if (!data) return result;

            // Initialiser à zéro
            memset(data, 0, frameCount * sizeof(float32));

            float32 normGain = 1.0f / (float32)count;

            for (int32 n = 0; n < count; ++n) {
                float32 freq  = frequencies[n];
                float32 amp   = amplitudes ? amplitudes[n] : normGain;
                float32 phase = 0.0f;
                float32 phaseInc = TWO_PI * freq / (float32)sampleRate;

                for (usize i = 0; i < frameCount; ++i) {
                    data[i] += NkSin(phase) * amp;
                    phase += phaseInc;
                }
            }

            // Normalisation soft pour éviter le clipping
            float32 peak = 0.0f;
            for (usize i = 0; i < frameCount; ++i) {
                float32 v = NkFabs(data[i]);
                if (v > peak) peak = v;
            }
            if (peak > 0.0f) {
                float32 gain = 0.8f / peak;
                for (usize i = 0; i < frameCount; ++i) {
                    data[i] *= gain;
                }
            }

            result.data       = data;
            result.frameCount = frameCount;
            result.sampleRate = sampleRate;
            result.channels   = 1;
            result.format     = AudioFormat::RAW;
            result.mAllocator = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // APPLY ADSR ENVELOPE
        // ──────────────────────────────────────────────────────────────────────

        void AudioGenerator::ApplyEnvelope(AudioSample& sample, const AdsrEnvelope& env) {
            if (!sample.IsValid()) return;

            float32  sr       = (float32)sample.sampleRate;
            usize    frames   = sample.frameCount;
            int32    ch       = sample.channels;
            float32* data     = sample.data;

            usize attackSamples  = (usize)(env.attackTime  * sr);
            usize decaySamples   = (usize)(env.decayTime   * sr);
            usize releaseSamples = (usize)(env.releaseTime * sr);

            // Garantir que attaque + déclin + relâche ne dépassent pas la durée
            if (attackSamples + decaySamples + releaseSamples > frames) {
                float32 total = env.attackTime + env.decayTime + env.releaseTime;
                if (total > 0.0f) {
                    float32 scale = (float32)frames / (sr * total);
                    attackSamples  = (usize)(env.attackTime  * sr * scale);
                    decaySamples   = (usize)(env.decayTime   * sr * scale);
                    releaseSamples = (usize)(env.releaseTime * sr * scale);
                }
            }

            usize sustainStart  = attackSamples + decaySamples;
            usize releaseStart  = frames > releaseSamples ? frames - releaseSamples : 0;

            for (usize f = 0; f < frames; ++f) {
                float32 env_gain = 1.0f;

                if (f < attackSamples) {
                    // Phase d'attaque : montée linéaire de 0 à 1
                    env_gain = (attackSamples > 0) ? (float32)f / (float32)attackSamples : 1.0f;

                } else if (f < sustainStart) {
                    // Phase de déclin : descente de 1 à sustainLevel
                    float32 t = (decaySamples > 0)
                        ? (float32)(f - attackSamples) / (float32)decaySamples
                        : 0.0f;
                    env_gain = 1.0f - t * (1.0f - env.sustainLevel);

                } else if (f < releaseStart) {
                    // Phase de soutien : niveau constant
                    env_gain = env.sustainLevel;

                } else {
                    // Phase de relâche : descente à 0
                    float32 t = (releaseSamples > 0)
                        ? (float32)(f - releaseStart) / (float32)releaseSamples
                        : 1.0f;
                    env_gain = env.sustainLevel * (1.0f - t);
                }

                // Appliquer à tous les canaux de ce frame
                for (int32 c = 0; c < ch; ++c) {
                    data[f * (usize)ch + (usize)c] *= env_gain;
                }
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // APPLY LFO
        // ──────────────────────────────────────────────────────────────────────

        void AudioGenerator::ApplyLfo(AudioSample& sample,
                                       float32      rate,
                                       float32      depth,
                                       WaveformType waveform) {
            if (!sample.IsValid() || rate <= 0.0f) return;

            float32  sr   = (float32)sample.sampleRate;
            usize    frames = sample.frameCount;
            int32    ch   = sample.channels;
            float32* data = sample.data;

            float32 phaseInc = TWO_PI * rate / sr;
            float32 phase    = 0.0f;

            for (usize f = 0; f < frames; ++f) {
                float32 lfoVal  = EvaluateWaveform(waveform, phase);
                // Moduler le volume : 1.0 ± depth
                float32 modGain = 1.0f + lfoVal * depth;
                if (modGain < 0.0f) modGain = 0.0f;

                for (int32 c = 0; c < ch; ++c) {
                    data[f * (usize)ch + (usize)c] *= modGain;
                }
                phase += phaseInc;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE KICK DRUM (synthèse FM descendante)
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateKick(float32              duration,
                                                  float32              startFreq,
                                                  float32              endFreq,
                                                  float32              clickAttack,
                                                  int32                sampleRate,
                                                  memory::NkAllocator* allocator) {
            AudioSample result{};
            if (duration <= 0.0f || sampleRate <= 0) return result;

            usize    frameCount = (usize)((float32)sampleRate * duration);
            float32* data       = AllocFloat(frameCount, allocator);
            if (!data) return result;

            float32 sr         = (float32)sampleRate;
            float32 clickFrames = clickAttack * sr;
            float32 freqRange  = startFreq - endFreq;

            float32 phase = 0.0f;

            for (usize i = 0; i < frameCount; ++i) {
                float32 t = (float32)i / sr;
                float32 tNorm = t / duration;

                // Descente de fréquence exponentielle (punch du kick)
                float32 freqDecay = NkPow(0.0001f, tNorm);
                float32 freq      = endFreq + freqRange * freqDecay;

                float32 body = NkSin(phase);

                // Transient de clic au début (bruit filtré)
                float32 click = 0.0f;
                if ((float32)i < clickFrames) {
                    float32 clickT = (float32)i / clickFrames;
                    // Bruit blanc à haute fréquence en attaque
                    static uint32 rng = 99991u;
                    rng = rng * 1664525u + 1013904223u;
                    float32 noise = (float32)((int32)rng) / (float32)0x7FFFFFFF;
                    click = noise * (1.0f - clickT) * 0.3f;
                }

                // Enveloppe d'amplitude exponentielle
                float32 amp = NkPow(0.0001f, tNorm) * 0.85f;

                data[i] = (body + click) * amp;

                phase += TWO_PI * freq / sr;
            }

            result.data       = data;
            result.frameCount = frameCount;
            result.sampleRate = sampleRate;
            result.channels   = 1;
            result.format     = AudioFormat::RAW;
            result.mAllocator = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE SNARE DRUM
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateSnare(float32              duration,
                                                   float32              toneFreq,
                                                   float32              noiseMix,
                                                   int32                sampleRate,
                                                   memory::NkAllocator* allocator) {
            AudioSample result{};
            if (duration <= 0.0f || sampleRate <= 0) return result;

            usize    frameCount = (usize)((float32)sampleRate * duration);
            float32* data       = AllocFloat(frameCount, allocator);
            if (!data) return result;

            float32 sr     = (float32)sampleRate;
            uint32  rng    = 31337u;
            float32 phase  = 0.0f;

            for (usize i = 0; i < frameCount; ++i) {
                float32 tNorm = (float32)i / ((float32)frameCount - 1.0f);

                // Corps tonal (descente de fréquence douce)
                float32 freq = toneFreq * (1.0f + 0.5f * NkPow(0.001f, tNorm));
                float32 body = NkSin(phase);
                phase += TWO_PI * freq / sr;

                // Bruit blanc pour les cordes du snare
                rng = rng * 1664525u + 1013904223u;
                float32 noise = (float32)((int32)rng) / (float32)0x7FFFFFFF;

                // Enveloppe globale
                float32 amp = NkPow(0.0001f, tNorm * 1.5f);

                data[i] = ((1.0f - noiseMix) * body + noiseMix * noise) * amp * 0.8f;
            }

            result.data       = data;
            result.frameCount = frameCount;
            result.sampleRate = sampleRate;
            result.channels   = 1;
            result.format     = AudioFormat::RAW;
            result.mAllocator = allocator;
            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // GENERATE HIHAT
        // ──────────────────────────────────────────────────────────────────────

        AudioSample AudioGenerator::GenerateHihat(float32              duration,
                                                   bool                 closed,
                                                   int32                sampleRate,
                                                   memory::NkAllocator* allocator) {
            AudioSample result{};
            if (duration <= 0.0f || sampleRate <= 0) return result;

            usize    frameCount = (usize)((float32)sampleRate * duration);
            float32* data       = AllocFloat(frameCount, allocator);
            if (!data) return result;

            float32 sr  = (float32)sampleRate;
            uint32  rng = 88888u;

            // Déclin rapide pour hihat fermée, plus long pour ouverte
            float32 decayRate = closed ? 8.0f : 1.5f;

            for (usize i = 0; i < frameCount; ++i) {
                float32 tNorm = (float32)i / ((float32)frameCount - 1.0f);

                // Bruit blanc filtré passe-haut (haute fréquence uniquement)
                rng = rng * 1664525u + 1013904223u;
                float32 noise = (float32)((int32)rng) / (float32)0x7FFFFFFF;

                // Filtre passe-haut simple (différentiel d'ordre 1)
                static float32 prevNoise = 0.0f;
                float32 filtered = noise - prevNoise * 0.7f;
                prevNoise = noise;

                // Enveloppe rapide
                float32 amp = NkPow(0.0001f, tNorm * decayRate);

                data[i] = filtered * amp * 0.6f;
            }

            result.data       = data;
            result.frameCount = frameCount;
            result.sampleRate = sampleRate;
            result.channels   = 1;
            result.format     = AudioFormat::RAW;
            result.mAllocator = allocator;
            return result;
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
