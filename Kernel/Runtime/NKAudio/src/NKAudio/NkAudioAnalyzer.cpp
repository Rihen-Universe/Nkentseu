// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioAnalyzer.cpp
// DESCRIPTION: Analyse audio DSP : FFT, RMS, tempo, pitch, spectrogram
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: FFT Cooley-Tukey itérative, détection pitch YIN, tempo autocorrélation
// -----------------------------------------------------------------------------

#include "NkAudio.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

namespace {
    constexpr nkentseu::float32 PI     = 3.14159265358979323846f;
    constexpr nkentseu::float32 TWO_PI = 6.28318530717958647692f;

    inline nkentseu::float32 NkCosf (nkentseu::float32 x) { return ::cosf(x); }
    inline nkentseu::float32 NkSinf (nkentseu::float32 x) { return ::sinf(x); }
    inline nkentseu::float32 NkSqrtf(nkentseu::float32 x) { return ::sqrtf(x); }
    inline nkentseu::float32 NkFabsf(nkentseu::float32 x) { return ::fabsf(x); }
    inline nkentseu::float32 NkLog2f(nkentseu::float32 x) { return ::log2f(x); }
    inline nkentseu::float32 NkAtan2f(nkentseu::float32 y, nkentseu::float32 x) { return ::atan2f(y,x); }

    inline nkentseu::float32* AllocFloat(nkentseu::usize n, nkentseu::memory::NkAllocator* a) {
        if (a) return (nkentseu::float32*)a->Allocate(n * sizeof(nkentseu::float32), sizeof(nkentseu::float32));
        return (nkentseu::float32*)::operator new(n * sizeof(nkentseu::float32));
    }
} // anonymous namespace

namespace nkentseu {
    namespace audio {

        // ──────────────────────────────────────────────────────────────────────
        // RMS ET PEAK
        // ──────────────────────────────────────────────────────────────────────

        float32 AudioAnalyzer::CalculateRms(const AudioSample& sample) {
            if (!sample.IsValid()) return 0.0f;

            double sum = 0.0;
            usize  count = sample.GetSampleCount();
            for (usize i = 0; i < count; ++i) {
                double v = (double)sample.data[i];
                sum += v * v;
            }
            return (count > 0) ? (float32)::sqrt(sum / (double)count) : 0.0f;
        }

        float32 AudioAnalyzer::CalculatePeak(const AudioSample& sample) {
            if (!sample.IsValid()) return 0.0f;

            float32 peak  = 0.0f;
            usize   count = sample.GetSampleCount();
            for (usize i = 0; i < count; ++i) {
                float32 v = NkFabsf(sample.data[i]);
                if (v > peak) peak = v;
            }
            return peak;
        }

        // ──────────────────────────────────────────────────────────────────────
        // FFT COOLEY-TUKEY ITÉRATIVE (Radix-2 DIT)
        // ──────────────────────────────────────────────────────────────────────

        void AudioAnalyzer::Fft(float32* real, float32* imag, int32 size) {
            // Bit-reversal permutation
            int32 bits = (int32)NkLog2f((float32)size);
            for (int32 i = 0; i < size; ++i) {
                int32 rev = 0;
                int32 x   = i;
                for (int32 b = 0; b < bits; ++b) {
                    rev = (rev << 1) | (x & 1);
                    x >>= 1;
                }
                if (rev > i) {
                    float32 tr = real[i]; real[i] = real[rev]; real[rev] = tr;
                    float32 ti = imag[i]; imag[i] = imag[rev]; imag[rev] = ti;
                }
            }

            // Papillons (butterfly)
            for (int32 len = 2; len <= size; len <<= 1) {
                float32 ang    = -TWO_PI / (float32)len;
                float32 wrBase = NkCosf(ang);
                float32 wiBase = NkSinf(ang);

                for (int32 i = 0; i < size; i += len) {
                    float32 wr = 1.0f, wi = 0.0f;
                    for (int32 j = 0; j < len / 2; ++j) {
                        float32 ur  = real[i + j];
                        float32 ui  = imag[i + j];
                        float32 vr  = real[i + j + len/2] * wr - imag[i + j + len/2] * wi;
                        float32 vi  = real[i + j + len/2] * wi + imag[i + j + len/2] * wr;

                        real[i + j]         = ur + vr;
                        imag[i + j]         = ui + vi;
                        real[i + j + len/2] = ur - vr;
                        imag[i + j + len/2] = ui - vi;

                        float32 nwr = wr * wrBase - wi * wiBase;
                        float32 nwi = wr * wiBase + wi * wrBase;
                        wr = nwr; wi = nwi;
                    }
                }
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // FENÊTRES DE PONDÉRATION
        // ──────────────────────────────────────────────────────────────────────

        void AudioAnalyzer::ApplyWindow(float32* buffer, int32 size, int32 windowType) {
            float32 invN = 1.0f / (float32)(size - 1);

            switch (windowType) {
                case 0: // Rectangle (aucune pondération)
                    break;

                case 1: // Fenêtre de Hann
                    for (int32 i = 0; i < size; ++i) {
                        float32 w = 0.5f * (1.0f - NkCosf(TWO_PI * (float32)i * invN));
                        buffer[i] *= w;
                    }
                    break;

                case 2: // Fenêtre de Hamming
                    for (int32 i = 0; i < size; ++i) {
                        float32 w = 0.54f - 0.46f * NkCosf(TWO_PI * (float32)i * invN);
                        buffer[i] *= w;
                    }
                    break;

                case 3: // Fenêtre de Blackman
                    for (int32 i = 0; i < size; ++i) {
                        float32 w = 0.42f
                            - 0.50f * NkCosf(TWO_PI * (float32)i * invN)
                            + 0.08f * NkCosf(4.0f * PI * (float32)i * invN);
                        buffer[i] *= w;
                    }
                    break;

                default:
                    break;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // COMPUTE FFT (OFFLINE — ALLOUE)
        // ──────────────────────────────────────────────────────────────────────

        FftResult AudioAnalyzer::ComputeFft(const AudioSample&   sample,
                                             int32                fftSize,
                                             int32                windowType,
                                             memory::NkAllocator* allocator) {
            FftResult result;
            if (!sample.IsValid() || fftSize <= 0) return result;

            // Allouer buffers temporaires (pas d'allocation depuis le tas STL)
            float32* real = AllocFloat((usize)fftSize, allocator);
            float32* imag = AllocFloat((usize)fftSize, allocator);
            if (!real || !imag) {
                if (allocator) {
                    if (real) allocator->Free(real);
                    if (imag) allocator->Free(imag);
                } else {
                    ::operator delete(real);
                    ::operator delete(imag);
                }
                return result;
            }

            // Remplir avec le premier canal
            int32 ch    = sample.channels;
            usize frames = sample.frameCount;
            usize copyN  = (frames < (usize)fftSize) ? frames : (usize)fftSize;

            for (usize i = 0; i < (usize)fftSize; ++i) {
                real[i] = (i < copyN) ? sample.data[i * (usize)ch] : 0.0f;
                imag[i] = 0.0f;
            }

            // Fenêtrage
            ApplyWindow(real, fftSize, windowType);

            // FFT
            Fft(real, imag, fftSize);

            // Résultat : magnitudes et phases (moitié + 1 bins)
            int32 bins = fftSize / 2 + 1;
            result.magnitudes.Reserve((usize)bins);
            result.phases.Reserve((usize)bins);

            for (int32 i = 0; i < bins; ++i) {
                float32 mag  = NkSqrtf(real[i]*real[i] + imag[i]*imag[i]) / (float32)(fftSize / 2);
                float32 ph   = NkAtan2f(imag[i], real[i]);
                result.magnitudes.PushBack(mag);
                result.phases.PushBack(ph);
            }
            result.fftSize    = fftSize;
            result.sampleRate = sample.sampleRate;

            // Libérer temporaires
            if (allocator) { allocator->Free(real); allocator->Free(imag); }
            else { ::operator delete(real); ::operator delete(imag); }

            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // COMPUTE FFT REALTIME (ZÉRO ALLOCATION)
        // ──────────────────────────────────────────────────────────────────────

        void AudioAnalyzer::ComputeFftRealtime(const float32* inputReal,
                                                float32*       outputMag,
                                                int32          fftSize) {
            if (!inputReal || !outputMag || fftSize <= 0) return;

            // Buffer temporaire sur la stack si petite taille, sinon new
            // Pour AAA : pré-allouer à l'init — on utilise ici un statique limité
            static float32 sRealBuf[AUDIO_DEFAULT_FFT_SIZE];
            static float32 sImagBuf[AUDIO_DEFAULT_FFT_SIZE];

            float32* real = sRealBuf;
            float32* imag = sImagBuf;

            bool usedStack = (fftSize <= AUDIO_DEFAULT_FFT_SIZE);
            float32* heapR = nullptr;
            float32* heapI = nullptr;

            if (!usedStack) {
                heapR = (float32*)::operator new((usize)fftSize * sizeof(float32));
                heapI = (float32*)::operator new((usize)fftSize * sizeof(float32));
                real = heapR;
                imag = heapI;
            }

            memcpy(real, inputReal, (usize)fftSize * sizeof(float32));
            memset(imag, 0, (usize)fftSize * sizeof(float32));

            // Fenêtre de Hann
            ApplyWindow(real, fftSize, 1);

            Fft(real, imag, fftSize);

            int32 bins = fftSize / 2 + 1;
            for (int32 i = 0; i < bins; ++i) {
                outputMag[i] = NkSqrtf(real[i]*real[i] + imag[i]*imag[i]) / (float32)(fftSize / 2);
            }

            if (!usedStack) {
                ::operator delete(heapR);
                ::operator delete(heapI);
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // BAND ENERGIES
        // ──────────────────────────────────────────────────────────────────────

        void AudioAnalyzer::ComputeBandEnergies(const float32* magnitudes,
                                                 int32          fftSize,
                                                 int32          sampleRate,
                                                 float32*       bands,
                                                 int32          bandCount) {
            if (!magnitudes || !bands || bandCount <= 0 || fftSize <= 0) return;

            // Bandes log-espacées de 20 Hz à Nyquist
            float32 nyquist  = (float32)sampleRate * 0.5f;
            float32 logMin   = NkLog2f(20.0f);
            float32 logMax   = NkLog2f(nyquist);
            float32 logRange = logMax - logMin;

            for (int32 b = 0; b < bandCount; ++b) {
                float32 freqLo = ::powf(2.0f, logMin + logRange * (float32)b     / (float32)bandCount);
                float32 freqHi = ::powf(2.0f, logMin + logRange * (float32)(b+1) / (float32)bandCount);

                int32 binLo = (int32)(freqLo / nyquist * (float32)(fftSize / 2));
                int32 binHi = (int32)(freqHi / nyquist * (float32)(fftSize / 2));

                if (binLo < 0) binLo = 0;
                if (binHi > fftSize / 2) binHi = fftSize / 2;

                float32 energy = 0.0f;
                for (int32 i = binLo; i <= binHi; ++i) {
                    energy += magnitudes[i];
                }
                float32 count = (float32)(binHi - binLo + 1);
                bands[b] = (count > 0.0f) ? energy / count : 0.0f;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // SPECTROGRAM (STFT)
        // ──────────────────────────────────────────────────────────────────────

        NkVector<FftResult> AudioAnalyzer::ComputeSpectrogram(const AudioSample&   sample,
                                                                int32                windowSize,
                                                                int32                hopSize,
                                                                memory::NkAllocator* allocator) {
            NkVector<FftResult> result;
            if (!sample.IsValid() || windowSize <= 0 || hopSize <= 0) return result;

            usize frames = sample.frameCount;
            int32 ch     = sample.channels;
            usize pos    = 0;

            while (pos + (usize)windowSize <= frames) {
                AudioSample window{};
                window.data       = sample.data + pos * (usize)ch;
                window.frameCount = (usize)windowSize;
                window.sampleRate = sample.sampleRate;
                window.channels   = ch;
                window.mAllocator = nullptr; // non-owning view

                FftResult fft = ComputeFft(window, windowSize, 1, allocator);
                result.PushBack(static_cast<FftResult&&>(fft));

                pos += (usize)hopSize;
            }

            return result;
        }

        // ──────────────────────────────────────────────────────────────────────
        // DÉTECTION PITCH (ALGORITHME YIN)
        // ──────────────────────────────────────────────────────────────────────

        float32 AudioAnalyzer::YinPitchDetect(const float32* data, int32 count,
                                               int32          sampleRate,
                                               float32        minFreq,
                                               float32        maxFreq) {
            // Plage de lags
            int32 tauMax = (int32)((float32)sampleRate / minFreq);
            int32 tauMin = (int32)((float32)sampleRate / maxFreq);
            if (tauMax > count / 2) tauMax = count / 2;
            if (tauMin < 1) tauMin = 1;

            // Étape 1 : différence
            float32 threshold = 0.1f;

            for (int32 tau = tauMin; tau < tauMax; ++tau) {
                float32 diff = 0.0f;
                for (int32 i = 0; i < count / 2; ++i) {
                    float32 d = data[i] - data[i + tau];
                    diff += d * d;
                }

                // Étape 2 : normalisation par la moyenne cumulative
                float32 runningSum = 0.0f;
                float32 cmnd       = 1.0f;
                for (int32 t2 = 1; t2 <= tau; ++t2) {
                    float32 d2 = 0.0f;
                    for (int32 i = 0; i < count / 2; ++i) {
                        float32 d = data[i] - data[i + t2];
                        d2 += d * d;
                    }
                    runningSum += d2;
                    if (runningSum > 0.0f) cmnd = diff * (float32)tau / runningSum;
                }

                if (cmnd < threshold) {
                    // Interpolation parabolique autour du minimum
                    if (tau > tauMin && tau < tauMax - 1) {
                        float32 x0 = (float32)(tau - 1);
                        float32 x1 = (float32)tau;
                        float32 x2 = (float32)(tau + 1);
                        float32 interp = x1 + (x0 - x2) * 0.5f * cmnd / (cmnd - 1.0f);
                        if (interp > 0.0f) return (float32)sampleRate / interp;
                    }
                    return (float32)sampleRate / (float32)tau;
                }
            }

            return 0.0f; // Indétectable
        }

        float32 AudioAnalyzer::DetectPitch(const AudioSample& sample,
                                            float32            minFreq,
                                            float32            maxFreq) {
            if (!sample.IsValid()) return 0.0f;

            // Utiliser uniquement le canal 0 (mono extraction)
            int32    ch    = sample.channels;
            usize    frames= sample.frameCount;
            float32* mono  = nullptr;
            bool     ownMono = false;

            if (ch == 1) {
                mono = sample.data;
            } else {
                // Downmix temporaire
                mono    = (float32*)::operator new(frames * sizeof(float32));
                ownMono = true;
                for (usize i = 0; i < frames; ++i) {
                    float32 sum = 0.0f;
                    for (int32 c = 0; c < ch; ++c) {
                        sum += sample.data[i * (usize)ch + (usize)c];
                    }
                    mono[i] = sum / (float32)ch;
                }
            }

            // Limiter l'analyse à 2048 échantillons pour performance
            int32 analysisSize = (int32)frames;
            if (analysisSize > 4096) analysisSize = 4096;

            float32 pitch = YinPitchDetect(mono, analysisSize, sample.sampleRate, minFreq, maxFreq);

            if (ownMono) ::operator delete(mono);
            return pitch;
        }

        // ──────────────────────────────────────────────────────────────────────
        // DÉTECTION TEMPO (AUTOCORRÉLATION + ONSET DETECTION)
        // ──────────────────────────────────────────────────────────────────────

        float32 AudioAnalyzer::DetectTempo(const AudioSample& sample,
                                            float32            minBpm,
                                            float32            maxBpm) {
            if (!sample.IsValid()) return 0.0f;

            float32 sr      = (float32)sample.sampleRate;
            int32   ch      = sample.channels;

            // Calculer l'enveloppe d'énergie par blocs (onset strength)
            int32  blockSize   = 512;
            usize  numBlocks   = sample.frameCount / (usize)blockSize;
            if (numBlocks < 4) return 0.0f;

            float32* energy = (float32*)::operator new(numBlocks * sizeof(float32));
            if (!energy) return 0.0f;

            for (usize b = 0; b < numBlocks; ++b) {
                float32 sum = 0.0f;
                usize   start = b * (usize)blockSize;
                for (int32 i = 0; i < blockSize; ++i) {
                    float32 s = 0.0f;
                    for (int32 c = 0; c < ch; ++c) {
                        s += NkFabsf(sample.data[(start + (usize)i) * (usize)ch + (usize)c]);
                    }
                    sum += s / (float32)ch;
                }
                energy[b] = sum / (float32)blockSize;
            }

            // Onset detection : dérivée positive de l'énergie
            float32* onsets = (float32*)::operator new(numBlocks * sizeof(float32));
            onsets[0] = 0.0f;
            for (usize b = 1; b < numBlocks; ++b) {
                float32 diff = energy[b] - energy[b-1];
                onsets[b] = (diff > 0.0f) ? diff : 0.0f;
            }

            // Autocorrélation des onsets sur la plage de tempos
            float32 blockDuration = (float32)blockSize / sr; // durée d'un bloc en secondes
            int32   lagMin = (int32)(60.0f / (maxBpm * blockDuration));
            int32   lagMax = (int32)(60.0f / (minBpm * blockDuration));

            if (lagMin < 1) lagMin = 1;
            if (lagMax > (int32)numBlocks / 2) lagMax = (int32)numBlocks / 2;

            float32 bestCorr = -1.0f;
            int32   bestLag  = lagMin;

            for (int32 lag = lagMin; lag <= lagMax; ++lag) {
                float32 corr = 0.0f;
                for (usize b = 0; b + (usize)lag < numBlocks; ++b) {
                    corr += onsets[b] * onsets[b + (usize)lag];
                }
                if (corr > bestCorr) {
                    bestCorr = corr;
                    bestLag  = lag;
                }
            }

            ::operator delete(energy);
            ::operator delete(onsets);

            if (bestCorr <= 0.0f) return 0.0f;

            float32 periodSeconds = (float32)bestLag * blockDuration;
            return 60.0f / periodSeconds;
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
