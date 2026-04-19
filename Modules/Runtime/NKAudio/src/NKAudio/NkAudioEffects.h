// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioEffects.h
// DESCRIPTION: Effets DSP AAA — Delay, Reverb, Compressor, Filters, EQ, etc.
//              Implémentations concrètes de IAudioEffect, STL-free
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEFFECTS_H_INCLUDED
#define NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEFFECTS_H_INCLUDED

#include "NkAudio.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace audio {

        // ====================================================================
        // DELAY EFFECT (Délai multi-tap avec feedback)
        // ====================================================================

        /**
         * @brief Délai stéréo avec feedback et wet/dry mix
         *
         * Implémente un délai à lecture circulaire, stéréo.
         * Utile pour echo, slapback, ping-pong.
         *
         * @example
         * @code
         * DelayEffect* delay = new DelayEffect(0.25f, 0.4f, 0.5f, 48000);
         * engine.AddEffect(handle, delay);
         * @endcode
         */
        class NKENTSEU_AUDIO_API DelayEffect : public IAudioEffect {
        public:
            /**
             * @param delayTime Temps de délai en secondes
             * @param feedback  Feedback [0, 0.99] (éviter ≥1 : feedback infini)
             * @param wet       Mix effet [0, 1]
             * @param sampleRate Fréquence d'échantillonnage
             * @param allocator Allocateur pour le buffer de délai
             */
            DelayEffect(float32              delayTime,
                        float32              feedback,
                        float32              wet,
                        int32                sampleRate,
                        memory::NkAllocator* allocator = nullptr);

            ~DelayEffect() override;

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override { return AudioEffectType::DELAY; }

            void SetDelayTime(float32 delaySeconds);
            void SetFeedback (float32 feedback);

        private:
            float32*             mBuffer     = nullptr;
            usize                mBufferSize = 0;
            usize                mWritePos   = 0;
            float32              mDelayTime;
            float32              mFeedback;
            int32                mSampleRate;
            memory::NkAllocator* mAllocator;

            void ReallocBuffer();
        };

        // ====================================================================
        // REVERB EFFECT (Schroeder FDN simplifié)
        // ====================================================================

        /**
         * @brief Réverbération basée sur Feedback Delay Network (FDN)
         *
         * Simule l'acoustique d'une salle en parallélisant plusieurs
         * lignes de délai avec un réseau de feedback.
         * Qualité suffisante pour jeux AAA sans algorithme convolution.
         */
        class NKENTSEU_AUDIO_API ReverbEffect : public IAudioEffect {
        public:
            struct Params {
                float32 roomSize   = 0.7f;  ///< Taille de la salle [0,1]
                float32 damping    = 0.5f;  ///< Amortissement HF [0,1]
                float32 wet        = 0.3f;  ///< Mix réverbéré [0,1]
                float32 preDelay   = 0.02f; ///< Pré-délai (s)
                float32 diffusion  = 0.8f;  ///< Diffusion [0,1]
            };

            explicit ReverbEffect(const Params&        params    = Params{},
                                  int32                sampleRate = 48000,
                                  memory::NkAllocator* allocator  = nullptr);
            ~ReverbEffect() override;

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override { return AudioEffectType::REVERB; }

            void SetParams(const Params& params);

        private:
            static const int32 NUM_COMBS   = 8;
            static const int32 NUM_ALLPASS = 4;

            struct CombFilter {
                float32* buffer  = nullptr;
                usize    size    = 0;
                usize    pos     = 0;
                float32  store   = 0.0f; // Feedback state
            };

            struct AllpassFilter {
                float32* buffer = nullptr;
                usize    size   = 0;
                usize    pos    = 0;
            };

            CombFilter           mCombL[NUM_COMBS];
            CombFilter           mCombR[NUM_COMBS];
            AllpassFilter        mAllpassL[NUM_ALLPASS];
            AllpassFilter        mAllpassR[NUM_ALLPASS];

            float32              mDamping;
            float32              mRoomSize;
            float32              mPreDelay;
            int32                mSampleRate;
            memory::NkAllocator* mAllocator;

            // Pre-delay buffer (stéréo)
            float32*             mPreDelayBuf = nullptr;
            usize                mPreDelaySize = 0;
            usize                mPreDelayPos  = 0;

            void InitBuffers(int32 sampleRate);
            void FreeBuffers();
            float32 ProcessComb   (CombFilter& comb,   float32 input);
            float32 ProcessAllpass(AllpassFilter& ap,  float32 input);
        };

        // ====================================================================
        // COMPRESSOR / LIMITER
        // ====================================================================

        /**
         * @brief Compresseur dynamique (RMS detection + gain computation)
         *
         * Réduit la plage dynamique pour un son plus "punchy" et constant.
         * Utilisé massivement en production musicale, podcasts, effets.
         */
        class NKENTSEU_AUDIO_API CompressorEffect : public IAudioEffect {
        public:
            struct Params {
                float32 threshold  = -12.0f; ///< Seuil en dBFS
                float32 ratio      = 4.0f;   ///< Ratio de compression (ex: 4:1)
                float32 attackMs   = 5.0f;   ///< Attaque (ms)
                float32 releaseMs  = 100.0f; ///< Relâche (ms)
                float32 makeupGain = 0.0f;   ///< Gain de compensation (dB)
                bool    softKnee   = true;   ///< Transition douce autour du threshold
            };

            explicit CompressorEffect(const Params& params = Params{},
                                      int32 sampleRate = 48000);

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override { return AudioEffectType::COMPRESSOR; }

            void SetParams(const Params& params);
            float32 GetCurrentGainReduction() const { return mGainReductionDb; }

        private:
            Params  mParams;
            int32   mSampleRate;
            float32 mEnvelope;       // Enveloppe RMS courante
            float32 mGainDb;         // Gain appliqué courant
            float32 mGainReductionDb;
            float32 mAttackCoeff;
            float32 mReleaseCoeff;

            void ComputeCoeffs();
            static float32 LinearToDb(float32 linear);
            static float32 DbToLinear(float32 db);
        };

        // ====================================================================
        // BIQUAD FILTER (Low-pass, High-pass, Band-pass, Notch, Peak EQ)
        // ====================================================================

        /**
         * @brief Filtre biquad polyvalent (transposed direct form II)
         *
         * Implémente tous les filtres standard avec une seule structure.
         * Base de tout égaliseur paramétrique professionnel.
         *
         * @note Stable numériquement grâce à la forme transposée.
         */
        class NKENTSEU_AUDIO_API BiquadFilter : public IAudioEffect {
        public:
            enum class FilterType {
                LOW_PASS,
                HIGH_PASS,
                BAND_PASS,
                NOTCH,
                PEAK_EQ,    ///< Boost/Cut paramétrique
                LOW_SHELF,
                HIGH_SHELF,
                ALL_PASS
            };

            /**
             * @param type      Type de filtre
             * @param cutoffHz  Fréquence de coupure/centrale (Hz)
             * @param qFactor   Facteur Q (largeur de bande)
             * @param gainDb    Gain (uniquement pour PEAK_EQ, LOW_SHELF, HIGH_SHELF)
             * @param sampleRate Fréquence d'échantillonnage
             */
            BiquadFilter(FilterType type,
                         float32    cutoffHz,
                         float32    qFactor   = 0.707f,
                         float32    gainDb    = 0.0f,
                         int32      sampleRate = 48000);

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override;

            void SetCutoff  (float32 cutoffHz);
            void SetQ       (float32 q);
            void SetGain    (float32 gainDb);
            void SetType    (FilterType type);

        private:
            FilterType mType;
            float32    mCutoffHz;
            float32    mQ;
            float32    mGainDb;
            int32      mSampleRate;

            // Coefficients biquad
            float32 mB0, mB1, mB2;   // numérateur
            float32 mA1, mA2;        // dénominateur (a0 normalisé à 1)

            // États (stéréo max — 2 canaux)
            float32 mZ1[2], mZ2[2];  // délais internes

            void ComputeCoefficients();
        };

        // ====================================================================
        // EQ 3 BANDES (Grave / Médium / Aigu)
        // ====================================================================

        /**
         * @brief Égaliseur 3 bandes classic (shelves + peak)
         *
         * Simule un EQ hardware analogique 3 bandes.
         * Fréquences fixes : 100 Hz / 1 kHz / 10 kHz.
         */
        class NKENTSEU_AUDIO_API Eq3BandEffect : public IAudioEffect {
        public:
            struct Params {
                float32 lowGainDb  = 0.0f;    ///< Gain grave (dB)
                float32 midGainDb  = 0.0f;    ///< Gain médium (dB)
                float32 highGainDb = 0.0f;    ///< Gain aigu (dB)
                float32 lowFreq    = 100.0f;  ///< Fréquence basse (Hz)
                float32 midFreq    = 1000.0f; ///< Fréquence médium (Hz)
                float32 highFreq   = 10000.0f;///< Fréquence haute (Hz)
                float32 midQ       = 1.0f;    ///< Q du band-mid
            };

            explicit Eq3BandEffect(const Params& params = Params{},
                                   int32 sampleRate = 48000);

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override { return AudioEffectType::EQ_3BAND; }

            void SetParams(const Params& params);

        private:
            BiquadFilter mLowShelf;
            BiquadFilter mMidPeak;
            BiquadFilter mHighShelf;
        };

        // ====================================================================
        // DISTORTION / OVERDRIVE
        // ====================================================================

        /**
         * @brief Distortion avec algorithme de clipping (soft/hard)
         *
         * Implémente plusieurs courbes de clipping :
         * - Soft (tube) : tanh
         * - Hard (transistor) : clamp brutal
         * - Fuzz : clipping asymétrique
         */
        class NKENTSEU_AUDIO_API DistortionEffect : public IAudioEffect {
        public:
            enum class DistortionType { SOFT, HARD, FUZZ, OVERDRIVE };

            DistortionEffect(DistortionType type  = DistortionType::SOFT,
                             float32        drive  = 5.0f,
                             float32        output = 0.5f);

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override { }
            void OnSampleRateChanged(int32) override { }
            AudioEffectType GetType() const override { return AudioEffectType::DISTORTION; }

            void SetDrive (float32 drive);
            void SetOutput(float32 output);

        private:
            DistortionType mType;
            float32        mDrive;
            float32        mOutput;

            float32 Clip(float32 x) const;
        };

        // ====================================================================
        // CHORUS EFFECT
        // ====================================================================

        /**
         * @brief Chorus — délai modulé par un LFO
         *
         * Crée l'illusion de plusieurs instruments légèrement désaccordés.
         * Classique en synthèse polyphonique et cordes.
         */
        class NKENTSEU_AUDIO_API ChorusEffect : public IAudioEffect {
        public:
            ChorusEffect(float32              rate      = 0.5f,
                         float32              depth     = 0.003f,
                         float32              feedback  = 0.3f,
                         float32              wet       = 0.5f,
                         int32                sampleRate = 48000,
                         memory::NkAllocator* allocator   = nullptr);

            ~ChorusEffect() override;

            void Process(float32* buffer, int32 frameCount, int32 channels) override;
            void Reset()  override;
            void OnSampleRateChanged(int32 sampleRate) override;
            AudioEffectType GetType() const override { return AudioEffectType::CHORUS; }

        private:
            static const int32 MAX_DELAY_SAMPLES = 4800; // 100ms @48kHz

            float32*             mDelayBuffer  = nullptr;
            usize                mDelaySize    = 0;
            usize                mWritePos     = 0;
            float32              mRate;
            float32              mDepth;       // Profondeur en secondes
            float32              mFeedback;
            float32              mLfoPhase;
            int32                mSampleRate;
            memory::NkAllocator* mAllocator;

            float32 ReadInterpolated(float32 delaySamples) const;
        };

    } // namespace audio
} // namespace nkentseu

#endif // NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEFFECTS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
