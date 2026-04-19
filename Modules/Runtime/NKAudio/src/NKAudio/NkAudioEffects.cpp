// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioEffects.cpp
// DESCRIPTION: Implémentation des effets DSP AAA
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#include "NkAudioEffects.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

namespace {
    constexpr nkentseu::float32 PI     = 3.14159265358979323846f;
    constexpr nkentseu::float32 TWO_PI = 6.28318530717958647692f;

    inline nkentseu::float32* AllocFloat(nkentseu::usize n, nkentseu::memory::NkAllocator* a) {
        if (a) return (nkentseu::float32*)a->Allocate(n * sizeof(nkentseu::float32), sizeof(nkentseu::float32));
        return (nkentseu::float32*)::operator new(n * sizeof(nkentseu::float32));
    }
    inline void FreeFloat(nkentseu::float32* p, nkentseu::memory::NkAllocator* a) {
        if (!p) return;
        if (a) a->Free(p);
        else ::operator delete(p);
    }

    inline nkentseu::float32 Clampf(nkentseu::float32 v, nkentseu::float32 lo, nkentseu::float32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    inline nkentseu::float32 Tanh(nkentseu::float32 x) { return ::tanhf(x); }
    inline nkentseu::float32 Sinf(nkentseu::float32 x) { return ::sinf(x);  }
    inline nkentseu::float32 Cosf(nkentseu::float32 x) { return ::cosf(x);  }
    inline nkentseu::float32 Sqrtf(nkentseu::float32 x){ return ::sqrtf(x); }
    inline nkentseu::float32 Powf(nkentseu::float32 b, nkentseu::float32 e){ return ::powf(b,e); }
    inline nkentseu::float32 Log10f(nkentseu::float32 x){ return ::log10f(x); }
    inline nkentseu::float32 Expf(nkentseu::float32 x) { return ::expf(x); }
    inline nkentseu::float32 Fabsf(nkentseu::float32 x){ return ::fabsf(x); }
} // anonymous namespace

namespace nkentseu {
    namespace audio {

        // ====================================================================
        // DELAY EFFECT
        // ====================================================================

        DelayEffect::DelayEffect(float32              delayTime,
                                 float32              feedback,
                                 float32              wet,
                                 int32                sampleRate,
                                 memory::NkAllocator* allocator)
            : mDelayTime(delayTime)
            , mFeedback (Clampf(feedback, 0.0f, 0.99f))
            , mSampleRate(sampleRate)
            , mAllocator(allocator)
        {
            mWetMix = wet;
            ReallocBuffer();
        }

        DelayEffect::~DelayEffect() {
            FreeFloat(mBuffer, mAllocator);
        }

        void DelayEffect::ReallocBuffer() {
            FreeFloat(mBuffer, mAllocator);
            mBufferSize = (usize)(mDelayTime * (float32)mSampleRate) + 2;
            mBufferSize *= 2; // stéréo
            mBuffer   = AllocFloat(mBufferSize, mAllocator);
            mWritePos = 0;
            if (mBuffer) memset(mBuffer, 0, mBufferSize * sizeof(float32));
        }

        void DelayEffect::Reset() {
            if (mBuffer) memset(mBuffer, 0, mBufferSize * sizeof(float32));
            mWritePos = 0;
        }

        void DelayEffect::OnSampleRateChanged(int32 sampleRate) {
            mSampleRate = sampleRate;
            ReallocBuffer();
        }

        void DelayEffect::SetDelayTime(float32 delaySeconds) {
            mDelayTime = delaySeconds;
            ReallocBuffer();
        }

        void DelayEffect::SetFeedback(float32 feedback) {
            mFeedback = Clampf(feedback, 0.0f, 0.99f);
        }

        void DelayEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled || !mBuffer || mBufferSize == 0) return;

            usize delaySamples = (usize)(mDelayTime * (float32)mSampleRate);
            if (delaySamples >= mBufferSize / 2) delaySamples = mBufferSize / 2 - 1;

            for (int32 i = 0; i < frameCount; ++i) {
                for (int32 ch = 0; ch < channels && ch < 2; ++ch) {
                    float32 input = buffer[i * channels + ch];

                    usize halfBuf = mBufferSize / 2;
                    usize readPos = (mWritePos + halfBuf - delaySamples) % halfBuf;

                    float32 delayed = mBuffer[readPos * 2 + (usize)ch];
                    float32 output  = input * (1.0f - mWetMix) + delayed * mWetMix;

                    mBuffer[mWritePos * 2 + (usize)ch] = input + delayed * mFeedback;
                    buffer[i * channels + ch] = output;
                }
                mWritePos = (mWritePos + 1) % (mBufferSize / 2);
            }
        }

        // ====================================================================
        // REVERB EFFECT (FDN Schroeder)
        // ====================================================================

        // Tailles des peignes Schroeder (premières mutuellement premières)
        static const int32 COMB_SIZES_MS[8]   = { 29, 37, 43, 47, 53, 59, 61, 67 };
        static const int32 ALLPASS_SIZES_MS[4] = { 5, 7, 11, 13 };
        static const float32 COMB_STEREO_OFFSET = 0.023f; // 2.3% pour décalage stéréo

        ReverbEffect::ReverbEffect(const Params&        params,
                                   int32                sampleRate,
                                   memory::NkAllocator* allocator)
            : mDamping(params.damping)
            , mRoomSize(params.roomSize)
            , mPreDelay(params.preDelay)
            , mSampleRate(sampleRate)
            , mAllocator(allocator)
        {
            mWetMix = params.wet;
            memset(mCombL,    0, sizeof(mCombL));
            memset(mCombR,    0, sizeof(mCombR));
            memset(mAllpassL, 0, sizeof(mAllpassL));
            memset(mAllpassR, 0, sizeof(mAllpassR));
            InitBuffers(sampleRate);
        }

        ReverbEffect::~ReverbEffect() {
            FreeBuffers();
        }

        void ReverbEffect::FreeBuffers() {
            for (int32 i = 0; i < NUM_COMBS; ++i) {
                FreeFloat(mCombL[i].buffer, mAllocator);
                FreeFloat(mCombR[i].buffer, mAllocator);
            }
            for (int32 i = 0; i < NUM_ALLPASS; ++i) {
                FreeFloat(mAllpassL[i].buffer, mAllocator);
                FreeFloat(mAllpassR[i].buffer, mAllocator);
            }
            FreeFloat(mPreDelayBuf, mAllocator);
            mPreDelayBuf = nullptr;
        }

        void ReverbEffect::InitBuffers(int32 sampleRate) {
            FreeBuffers();

            float32 srScale = (float32)sampleRate / 48000.0f;

            for (int32 i = 0; i < NUM_COMBS; ++i) {
                // Taille proportionnelle à roomSize et sample rate
                usize sizeL = (usize)((float32)COMB_SIZES_MS[i] * srScale * 0.001f * (float32)sampleRate * (0.5f + 0.5f * mRoomSize)) + 1;
                usize sizeR = (usize)((float32)sizeL * (1.0f + COMB_STEREO_OFFSET)) + 1;

                mCombL[i].buffer = AllocFloat(sizeL, mAllocator);
                mCombL[i].size   = sizeL;
                mCombL[i].pos    = 0;
                mCombL[i].store  = 0.0f;
                if (mCombL[i].buffer) memset(mCombL[i].buffer, 0, sizeL * sizeof(float32));

                mCombR[i].buffer = AllocFloat(sizeR, mAllocator);
                mCombR[i].size   = sizeR;
                mCombR[i].pos    = 0;
                mCombR[i].store  = 0.0f;
                if (mCombR[i].buffer) memset(mCombR[i].buffer, 0, sizeR * sizeof(float32));
            }

            for (int32 i = 0; i < NUM_ALLPASS; ++i) {
                usize sizeL = (usize)((float32)ALLPASS_SIZES_MS[i] * srScale * 0.001f * (float32)sampleRate) + 1;
                usize sizeR = (usize)((float32)sizeL * (1.0f + COMB_STEREO_OFFSET * 0.5f)) + 1;

                mAllpassL[i].buffer = AllocFloat(sizeL, mAllocator);
                mAllpassL[i].size   = sizeL;
                mAllpassL[i].pos    = 0;
                if (mAllpassL[i].buffer) memset(mAllpassL[i].buffer, 0, sizeL * sizeof(float32));

                mAllpassR[i].buffer = AllocFloat(sizeR, mAllocator);
                mAllpassR[i].size   = sizeR;
                mAllpassR[i].pos    = 0;
                if (mAllpassR[i].buffer) memset(mAllpassR[i].buffer, 0, sizeR * sizeof(float32));
            }

            mPreDelaySize = (usize)(mPreDelay * (float32)sampleRate) + 2;
            mPreDelayBuf  = AllocFloat(mPreDelaySize * 2, mAllocator); // stéréo
            mPreDelayPos  = 0;
            if (mPreDelayBuf) memset(mPreDelayBuf, 0, mPreDelaySize * 2 * sizeof(float32));
        }

        float32 ReverbEffect::ProcessComb(CombFilter& comb, float32 input) {
            if (!comb.buffer || comb.size == 0) return input;

            float32 output = comb.buffer[comb.pos];
            comb.store  = output * (1.0f - mDamping) + comb.store * mDamping;
            comb.buffer[comb.pos] = input + comb.store * mRoomSize;
            comb.pos = (comb.pos + 1) % comb.size;
            return output;
        }

        float32 ReverbEffect::ProcessAllpass(AllpassFilter& ap, float32 input) {
            if (!ap.buffer || ap.size == 0) return input;

            float32 bufOut = ap.buffer[ap.pos];
            float32 output = -input + bufOut;
            ap.buffer[ap.pos] = input + bufOut * 0.5f;
            ap.pos = (ap.pos + 1) % ap.size;
            return output;
        }

        void ReverbEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled) return;

            for (int32 f = 0; f < frameCount; ++f) {
                float32 inL = buffer[f * channels];
                float32 inR = (channels > 1) ? buffer[f * channels + 1] : inL;

                // Pre-delay
                float32 pdL = 0.0f, pdR = 0.0f;
                if (mPreDelayBuf && mPreDelaySize > 0) {
                    pdL = mPreDelayBuf[mPreDelayPos * 2 + 0];
                    pdR = mPreDelayBuf[mPreDelayPos * 2 + 1];
                    mPreDelayBuf[mPreDelayPos * 2 + 0] = inL;
                    mPreDelayBuf[mPreDelayPos * 2 + 1] = inR;
                    mPreDelayPos = (mPreDelayPos + 1) % mPreDelaySize;
                } else {
                    pdL = inL; pdR = inR;
                }

                float32 input = (pdL + pdR) * 0.5f; // Mono pre-mix

                // Banque de peignes en parallèle
                float32 outL = 0.0f, outR = 0.0f;
                for (int32 i = 0; i < NUM_COMBS; ++i) {
                    outL += ProcessComb(mCombL[i], input);
                    outR += ProcessComb(mCombR[i], input);
                }

                // Série d'all-pass
                for (int32 i = 0; i < NUM_ALLPASS; ++i) {
                    outL = ProcessAllpass(mAllpassL[i], outL);
                    outR = ProcessAllpass(mAllpassR[i], outR);
                }

                float32 dry = 1.0f - mWetMix;
                buffer[f * channels] = inL * dry + outL * mWetMix * 0.015f;
                if (channels > 1) {
                    buffer[f * channels + 1] = inR * dry + outR * mWetMix * 0.015f;
                }
            }
        }

        void ReverbEffect::Reset() {
            for (int32 i = 0; i < NUM_COMBS; ++i) {
                if (mCombL[i].buffer) memset(mCombL[i].buffer, 0, mCombL[i].size * sizeof(float32));
                if (mCombR[i].buffer) memset(mCombR[i].buffer, 0, mCombR[i].size * sizeof(float32));
                mCombL[i].pos = mCombR[i].pos = 0;
                mCombL[i].store = mCombR[i].store = 0.0f;
            }
            for (int32 i = 0; i < NUM_ALLPASS; ++i) {
                if (mAllpassL[i].buffer) memset(mAllpassL[i].buffer, 0, mAllpassL[i].size * sizeof(float32));
                if (mAllpassR[i].buffer) memset(mAllpassR[i].buffer, 0, mAllpassR[i].size * sizeof(float32));
                mAllpassL[i].pos = mAllpassR[i].pos = 0;
            }
        }

        void ReverbEffect::OnSampleRateChanged(int32 sampleRate) {
            mSampleRate = sampleRate;
            InitBuffers(sampleRate);
        }

        void ReverbEffect::SetParams(const Params& params) {
            mRoomSize = params.roomSize;
            mDamping  = params.damping;
            mWetMix   = params.wet;
            mPreDelay = params.preDelay;
            InitBuffers(mSampleRate);
        }

        // ====================================================================
        // COMPRESSOR
        // ====================================================================

        CompressorEffect::CompressorEffect(const Params& params, int32 sampleRate)
            : mParams(params)
            , mSampleRate(sampleRate)
            , mEnvelope(0.0f)
            , mGainDb(0.0f)
            , mGainReductionDb(0.0f)
        {
            ComputeCoeffs();
        }

        void CompressorEffect::ComputeCoeffs() {
            // Coefficients temporels (filtre passe-bas sur enveloppe)
            mAttackCoeff  = Expf(-1.0f / (mParams.attackMs  * 0.001f * (float32)mSampleRate));
            mReleaseCoeff = Expf(-1.0f / (mParams.releaseMs * 0.001f * (float32)mSampleRate));
        }

        void CompressorEffect::SetParams(const Params& params) {
            mParams = params;
            ComputeCoeffs();
        }

        void CompressorEffect::OnSampleRateChanged(int32 sampleRate) {
            mSampleRate = sampleRate;
            ComputeCoeffs();
        }

        void CompressorEffect::Reset() {
            mEnvelope = 0.0f;
            mGainDb   = 0.0f;
            mGainReductionDb = 0.0f;
        }

        float32 CompressorEffect::LinearToDb(float32 linear) {
            if (linear < 0.0000001f) return -140.0f;
            return 20.0f * Log10f(linear);
        }

        float32 CompressorEffect::DbToLinear(float32 db) {
            return Powf(10.0f, db / 20.0f);
        }

        void CompressorEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled) return;

            float32 threshold = mParams.threshold;
            float32 ratio     = mParams.ratio;
            float32 makeupGain = DbToLinear(mParams.makeupGain);

            for (int32 f = 0; f < frameCount; ++f) {
                // Calcul de l'amplitude peak (tous canaux)
                float32 peak = 0.0f;
                for (int32 c = 0; c < channels; ++c) {
                    float32 v = Fabsf(buffer[f * channels + c]);
                    if (v > peak) peak = v;
                }

                float32 inputDb = LinearToDb(peak);

                // Détection enveloppe (attack/release)
                float32 coeff = (peak > mEnvelope) ? mAttackCoeff : mReleaseCoeff;
                mEnvelope = peak + coeff * (mEnvelope - peak);

                float32 envDb = LinearToDb(mEnvelope);

                // Calcul gain reduction
                float32 gainReduction = 0.0f;
                if (envDb > threshold) {
                    if (mParams.softKnee) {
                        float32 kneeWidth = 6.0f;
                        float32 excess = envDb - threshold + kneeWidth * 0.5f;
                        if (excess > 0.0f) {
                            float32 kneeFactor = excess / kneeWidth;
                            if (kneeFactor > 1.0f) kneeFactor = 1.0f;
                            gainReduction = kneeFactor * (envDb - threshold) * (1.0f - 1.0f / ratio);
                        }
                    } else {
                        gainReduction = (envDb - threshold) * (1.0f - 1.0f / ratio);
                    }
                }

                mGainReductionDb = gainReduction;
                float32 gain = DbToLinear(-gainReduction) * makeupGain;

                for (int32 c = 0; c < channels; ++c) {
                    buffer[f * channels + c] *= gain;
                }

                (void)inputDb;
            }
        }

        // ====================================================================
        // BIQUAD FILTER
        // ====================================================================

        BiquadFilter::BiquadFilter(FilterType type, float32 cutoffHz,
                                   float32 qFactor, float32 gainDb,
                                   int32 sampleRate)
            : mType(type)
            , mCutoffHz(cutoffHz)
            , mQ(qFactor)
            , mGainDb(gainDb)
            , mSampleRate(sampleRate)
        {
            mZ1[0] = mZ1[1] = 0.0f;
            mZ2[0] = mZ2[1] = 0.0f;
            ComputeCoefficients();
        }

        AudioEffectType BiquadFilter::GetType() const {
            switch (mType) {
                case FilterType::LOW_PASS:  return AudioEffectType::LOW_PASS;
                case FilterType::HIGH_PASS: return AudioEffectType::HIGH_PASS;
                case FilterType::BAND_PASS: return AudioEffectType::BAND_PASS;
                case FilterType::NOTCH:     return AudioEffectType::NOTCH;
                default:                    return AudioEffectType::EQ_PARAMETRIC;
            }
        }

        void BiquadFilter::ComputeCoefficients() {
            float32 omega = TWO_PI * mCutoffHz / (float32)mSampleRate;
            float32 alpha = Sinf(omega) / (2.0f * mQ);
            float32 cosw  = Cosf(omega);
            float32 A     = Powf(10.0f, mGainDb / 40.0f); // Amplitude (pour EQ)

            float32 a0 = 1.0f;
            mB0 = mB1 = mB2 = 0.0f;
            mA1 = mA2 = 0.0f;

            switch (mType) {
                case FilterType::LOW_PASS:
                    mB0 = (1.0f - cosw) * 0.5f;
                    mB1 =  1.0f - cosw;
                    mB2 = (1.0f - cosw) * 0.5f;
                    a0  =  1.0f + alpha;
                    mA1 = -2.0f * cosw;
                    mA2 =  1.0f - alpha;
                    break;

                case FilterType::HIGH_PASS:
                    mB0 =  (1.0f + cosw) * 0.5f;
                    mB1 = -(1.0f + cosw);
                    mB2 =  (1.0f + cosw) * 0.5f;
                    a0  =   1.0f + alpha;
                    mA1 =  -2.0f * cosw;
                    mA2 =   1.0f - alpha;
                    break;

                case FilterType::BAND_PASS:
                    mB0 = alpha;
                    mB1 = 0.0f;
                    mB2 = -alpha;
                    a0  =  1.0f + alpha;
                    mA1 = -2.0f * cosw;
                    mA2 =  1.0f - alpha;
                    break;

                case FilterType::NOTCH:
                    mB0 = 1.0f;
                    mB1 = -2.0f * cosw;
                    mB2 = 1.0f;
                    a0  =  1.0f + alpha;
                    mA1 = -2.0f * cosw;
                    mA2 =  1.0f - alpha;
                    break;

                case FilterType::PEAK_EQ:
                    mB0 =  1.0f + alpha * A;
                    mB1 = -2.0f * cosw;
                    mB2 =  1.0f - alpha * A;
                    a0  =  1.0f + alpha / A;
                    mA1 = -2.0f * cosw;
                    mA2 =  1.0f - alpha / A;
                    break;

                case FilterType::LOW_SHELF: {
                    float32 sqA = Sqrtf(A);
                    float32 al  = Sinf(omega) * 0.5f * Sqrtf((A + 1.0f/A) * (1.0f/0.9f - 1.0f) + 2.0f);
                    mB0 =       A*( (A+1) - (A-1)*cosw + 2*sqA*al);
                    mB1 =  2.0f*A*( (A-1) - (A+1)*cosw);
                    mB2 =       A*( (A+1) - (A-1)*cosw - 2*sqA*al);
                    a0  =          (A+1) + (A-1)*cosw + 2*sqA*al;
                    mA1 = -2.0f*( (A-1) + (A+1)*cosw);
                    mA2 =          (A+1) + (A-1)*cosw - 2*sqA*al;
                    break;
                }

                case FilterType::HIGH_SHELF: {
                    float32 sqA = Sqrtf(A);
                    float32 al  = Sinf(omega) * 0.5f * Sqrtf((A + 1.0f/A) * (1.0f/0.9f - 1.0f) + 2.0f);
                    mB0 =      A*( (A+1) + (A-1)*cosw + 2*sqA*al);
                    mB1 = -2.0f*A*( (A-1) + (A+1)*cosw);
                    mB2 =      A*( (A+1) + (A-1)*cosw - 2*sqA*al);
                    a0  =         (A+1) - (A-1)*cosw + 2*sqA*al;
                    mA1 =  2.0f*( (A-1) - (A+1)*cosw);
                    mA2 =         (A+1) - (A-1)*cosw - 2*sqA*al;
                    break;
                }

                case FilterType::ALL_PASS:
                    mB0 =  1.0f - alpha;
                    mB1 = -2.0f * cosw;
                    mB2 =  1.0f + alpha;
                    a0  =  1.0f + alpha;
                    mA1 = -2.0f * cosw;
                    mA2 =  1.0f - alpha;
                    break;
            }

            // Normalisation par a0
            float32 inv_a0 = (a0 != 0.0f) ? 1.0f / a0 : 1.0f;
            mB0 *= inv_a0; mB1 *= inv_a0; mB2 *= inv_a0;
            mA1 *= inv_a0; mA2 *= inv_a0;
        }

        void BiquadFilter::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled) return;

            for (int32 f = 0; f < frameCount; ++f) {
                for (int32 c = 0; c < channels && c < 2; ++c) {
                    float32 x = buffer[f * channels + c];
                    // Transposed direct form II
                    float32 y  = mB0 * x + mZ1[c];
                    mZ1[c]     = mB1 * x - mA1 * y + mZ2[c];
                    mZ2[c]     = mB2 * x - mA2 * y;
                    buffer[f * channels + c] = y;
                }
            }
        }

        void BiquadFilter::Reset() {
            mZ1[0] = mZ1[1] = mZ2[0] = mZ2[1] = 0.0f;
        }

        void BiquadFilter::OnSampleRateChanged(int32 sampleRate) {
            mSampleRate = sampleRate;
            ComputeCoefficients();
        }

        void BiquadFilter::SetCutoff(float32 hz) { mCutoffHz = hz; ComputeCoefficients(); }
        void BiquadFilter::SetQ     (float32 q)  { mQ = q;         ComputeCoefficients(); }
        void BiquadFilter::SetGain  (float32 db) { mGainDb = db;   ComputeCoefficients(); }
        void BiquadFilter::SetType  (FilterType t){ mType = t;     ComputeCoefficients(); }

        // ====================================================================
        // EQ 3 BANDES
        // ====================================================================

        Eq3BandEffect::Eq3BandEffect(const Params& params, int32 sampleRate)
            : mLowShelf (BiquadFilter::FilterType::LOW_SHELF,  params.lowFreq,  0.707f, params.lowGainDb,  sampleRate)
            , mMidPeak  (BiquadFilter::FilterType::PEAK_EQ,    params.midFreq,  params.midQ, params.midGainDb, sampleRate)
            , mHighShelf(BiquadFilter::FilterType::HIGH_SHELF, params.highFreq, 0.707f, params.highGainDb, sampleRate)
        { }

        void Eq3BandEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled) return;
            mLowShelf .Process(buffer, frameCount, channels);
            mMidPeak  .Process(buffer, frameCount, channels);
            mHighShelf.Process(buffer, frameCount, channels);
        }

        void Eq3BandEffect::Reset() {
            mLowShelf.Reset(); mMidPeak.Reset(); mHighShelf.Reset();
        }

        void Eq3BandEffect::OnSampleRateChanged(int32 sr) {
            mLowShelf.OnSampleRateChanged(sr);
            mMidPeak.OnSampleRateChanged(sr);
            mHighShelf.OnSampleRateChanged(sr);
        }

        void Eq3BandEffect::SetParams(const Params& p) {
            mLowShelf.SetCutoff(p.lowFreq);   mLowShelf.SetGain(p.lowGainDb);
            mMidPeak.SetCutoff(p.midFreq);    mMidPeak.SetQ(p.midQ); mMidPeak.SetGain(p.midGainDb);
            mHighShelf.SetCutoff(p.highFreq); mHighShelf.SetGain(p.highGainDb);
        }

        // ====================================================================
        // DISTORTION
        // ====================================================================

        DistortionEffect::DistortionEffect(DistortionType type, float32 drive, float32 output)
            : mType(type), mDrive(drive), mOutput(output) { }

        float32 DistortionEffect::Clip(float32 x) const {
            float32 driven = x * mDrive;
            switch (mType) {
                case DistortionType::SOFT:
                    return Tanh(driven) * mOutput;
                case DistortionType::HARD:
                    return Clampf(driven, -1.0f, 1.0f) * mOutput;
                case DistortionType::FUZZ: {
                    // Asymétrique (simulation tube) + clipping dur
                    float32 v = Tanh(driven * 1.5f);
                    return Clampf(v * 0.6f, -0.7f, 1.0f) * mOutput;
                }
                case DistortionType::OVERDRIVE:
                    // Polynomiale douce pour couleur chaleureuse
                    if (driven >  1.0f) return  0.667f * mOutput;
                    if (driven < -1.0f) return -0.667f * mOutput;
                    return (driven - (driven * driven * driven / 3.0f)) * mOutput;
            }
            return x;
        }

        void DistortionEffect::SetDrive(float32 d)  { mDrive  = d; }
        void DistortionEffect::SetOutput(float32 o) { mOutput = o; }

        void DistortionEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled) return;
            for (int32 i = 0; i < frameCount * channels; ++i) {
                buffer[i] = Clip(buffer[i]);
            }
        }

        // ====================================================================
        // CHORUS
        // ====================================================================

        ChorusEffect::ChorusEffect(float32              rate,
                                   float32              depth,
                                   float32              feedback,
                                   float32              wet,
                                   int32                sampleRate,
                                   memory::NkAllocator* allocator)
            : mRate(rate)
            , mDepth(depth)
            , mFeedback(feedback)
            , mLfoPhase(0.0f)
            , mSampleRate(sampleRate)
            , mAllocator(allocator)
        {
            mWetMix = wet;
            mDelaySize = (usize)(0.1f * (float32)sampleRate) + 2; // 100ms max
            mDelayBuffer = AllocFloat(mDelaySize * 2, mAllocator); // stéréo
            mWritePos    = 0;
            if (mDelayBuffer) memset(mDelayBuffer, 0, mDelaySize * 2 * sizeof(float32));
        }

        ChorusEffect::~ChorusEffect() {
            FreeFloat(mDelayBuffer, mAllocator);
        }

        void ChorusEffect::Reset() {
            if (mDelayBuffer) memset(mDelayBuffer, 0, mDelaySize * 2 * sizeof(float32));
            mWritePos = 0;
            mLfoPhase = 0.0f;
        }

        void ChorusEffect::OnSampleRateChanged(int32 sampleRate) {
            mSampleRate = sampleRate;
            FreeFloat(mDelayBuffer, mAllocator);
            mDelaySize   = (usize)(0.1f * (float32)sampleRate) + 2;
            mDelayBuffer = AllocFloat(mDelaySize * 2, mAllocator);
            mWritePos    = 0;
            if (mDelayBuffer) memset(mDelayBuffer, 0, mDelaySize * 2 * sizeof(float32));
        }

        float32 ChorusEffect::ReadInterpolated(float32 delaySamples) const {
            if (!mDelayBuffer) return 0.0f;
            usize readI = (usize)(mWritePos + mDelaySize - (usize)delaySamples) % mDelaySize;
            usize nextI = (readI + 1) % mDelaySize;
            float32 frac = delaySamples - (float32)(usize)delaySamples;
            return mDelayBuffer[readI * 2] * (1.0f - frac) + mDelayBuffer[nextI * 2] * frac;
        }

        void ChorusEffect::Process(float32* buffer, int32 frameCount, int32 channels) {
            if (!mEnabled || !mDelayBuffer) return;

            float32 sr          = (float32)mSampleRate;
            float32 lfoInc      = TWO_PI * mRate / sr;
            float32 depthSamples = mDepth * sr;
            float32 baseSamples  = depthSamples * 2.0f; // Centre du délai

            for (int32 f = 0; f < frameCount; ++f) {
                float32 lfo = Sinf(mLfoPhase);
                mLfoPhase += lfoInc;
                if (mLfoPhase > TWO_PI) mLfoPhase -= TWO_PI;

                float32 delSamples = baseSamples + lfo * depthSamples;
                float32 delayed    = ReadInterpolated(delSamples);

                float32 inL = buffer[f * channels];
                float32 out = inL * (1.0f - mWetMix) + delayed * mWetMix;

                // Écriture avec feedback
                mDelayBuffer[mWritePos * 2] = inL + delayed * mFeedback;
                mWritePos = (mWritePos + 1) % mDelaySize;

                buffer[f * channels] = out;
                if (channels > 1) {
                    // Canal droit décalé de 90° pour effet stéréo
                    float32 lfoR    = Cosf(mLfoPhase);
                    float32 delR    = baseSamples + lfoR * depthSamples;
                    float32 delayedR = ReadInterpolated(delR);
                    buffer[f * channels + 1] = buffer[f * channels + 1] * (1.0f - mWetMix)
                                             + delayedR * mWetMix;
                }
            }
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
