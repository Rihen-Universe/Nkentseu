// AudioEffects.cpp
#include "Audio.hpp"
#include <cmath>
#include <algorithm>

namespace nk {

// ====================================================================
// DELAY EFFECT
// ====================================================================

class DelayEffect : public AudioEffect {
public:
    DelayEffect(float delayTime, float feedback, float mix, int sampleRate)
        : m_delayTime(delayTime)
        , m_feedback(feedback)
        , m_mix(mix)
        , m_sampleRate(sampleRate) {
        
        m_delayBufferSize = static_cast<size_t>(delayTime * sampleRate) + 1;
        m_delayBuffer.resize(m_delayBufferSize * 2, 0.0f); // Stereo
        m_writePos = 0;
    }
    
    AudioEffectType getType() const override {
        return AudioEffectType::Delay;
    }
    
    void process(float* buffer, int frameCount, int channels) override {
        for (int i = 0; i < frameCount; i++) {
            for (int ch = 0; ch < channels && ch < 2; ch++) {
                float input = buffer[i * channels + ch];
                
                // Lire depuis le delay buffer
                size_t readPos = (m_writePos + m_delayBufferSize - 
                                 static_cast<size_t>(m_delayTime * m_sampleRate)) % m_delayBufferSize;
                
                float delayed = m_delayBuffer[readPos * 2 + ch];
                
                // Mix
                float output = input * (1.0f - m_mix) + delayed * m_mix;
                
                // Écrire dans le delay buffer avec feedback
                m_delayBuffer[m_writePos * 2 + ch] = input + delayed * m_feedback;
                
                buffer[i * channels + ch] = output;
            }
            
            m_writePos = (m_writePos + 1) % m_delayBufferSize;
        }
    }
    
    void reset() override {
        std::fill(m_delayBuffer.begin(), m_delayBuffer.end(), 0.0f);
        m_writePos = 0;
    }
    
private:
    float m_delayTime;
    float m_feedback;
    float m_mix;
    int m_sampleRate;
    
    std::vector<float> m_delayBuffer;
    size_t m_delayBufferSize;
    size_t m_writePos;
};

// ====================================================================
// REVERB EFFECT (Schroeder Reverb simplifié)
// ====================================================================

class ReverbEffect : public AudioEffect {
public:
    ReverbEffect(float roomSize, float damping, float mix, int sampleRate)
        : m_roomSize(roomSize)
        , m_damping(damping)
        , m_mix(mix)
        , m_sampleRate(sampleRate) {
        
        // Comb filters
        int combDelays[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        
        for (int i = 0; i < 8; i++) {
            int delay = static_cast<int>(combDelays[i] * roomSize);
            m_combBuffers.push_back(std::vector<float>(delay, 0.0f));
            m_combPositions.push_back(0);
            m_combFeedback.push_back(0.0f);
        }
        
        // Allpass filters
        int allpassDelays[] = {225, 556, 441, 341};
        
        for (int i = 0; i < 4; i++) {
            m_allpassBuffers.push_back(std::vector<float>(allpassDelays[i], 0.0f));
            m_allpassPositions.push_back(0);
        }
    }
    
    AudioEffectType getType() const override {
        return AudioEffectType::Reverb;
    }
    
    void process(float* buffer, int frameCount, int channels) override {
        for (int i = 0; i < frameCount; i++) {
            float input = 0.0f;
            
            // Mono sum
            for (int ch = 0; ch < channels; ch++) {
                input += buffer[i * channels + ch];
            }
            input /= channels;
            
            // Comb filters
            float combOut = 0.0f;
            
            for (size_t c = 0; c < m_combBuffers.size(); c++) {
                float delayed = m_combBuffers[c][m_combPositions[c]];
                
                float output = delayed + input;
                
                float feedback = output * m_roomSize * (1.0f - m_damping) + 
                                m_combFeedback[c] * m_damping;
                
                m_combBuffers[c][m_combPositions[c]] = input + feedback;
                m_combFeedback[c] = feedback;
                
                combOut += output;
                
                m_combPositions[c] = (m_combPositions[c] + 1) % m_combBuffers[c].size();
            }
            
            combOut /= m_combBuffers.size();
            
            // Allpass filters
            float allpassOut = combOut;
            
            for (size_t a = 0; a < m_allpassBuffers.size(); a++) {
                float delayed = m_allpassBuffers[a][m_allpassPositions[a]];
                float output = -allpassOut + delayed;
                
                m_allpassBuffers[a][m_allpassPositions[a]] = allpassOut + delayed * 0.5f;
                allpassOut = output;
                
                m_allpassPositions[a] = (m_allpassPositions[a] + 1) % m_allpassBuffers[a].size();
            }
            
            // Mix et output
            for (int ch = 0; ch < channels; ch++) {
                float dry = buffer[i * channels + ch];
                buffer[i * channels + ch] = dry * (1.0f - m_mix) + allpassOut * m_mix;
            }
        }
    }
    
    void reset() override {
        for (auto& buf : m_combBuffers) {
            std::fill(buf.begin(), buf.end(), 0.0f);
        }
        for (auto& buf : m_allpassBuffers) {
            std::fill(buf.begin(), buf.end(), 0.0f);
        }
        std::fill(m_combPositions.begin(), m_combPositions.end(), 0);
        std::fill(m_allpassPositions.begin(), m_allpassPositions.end(), 0);
        std::fill(m_combFeedback.begin(), m_combFeedback.end(), 0.0f);
    }
    
private:
    float m_roomSize;
    float m_damping;
    float m_mix;
    int m_sampleRate;
    
    std::vector<std::vector<float>> m_combBuffers;
    std::vector<size_t> m_combPositions;
    std::vector<float> m_combFeedback;
    
    std::vector<std::vector<float>> m_allpassBuffers;
    std::vector<size_t> m_allpassPositions;
};

// ====================================================================
// LOW PASS FILTER
// ====================================================================

class LowPassFilter : public AudioEffect {
public:
    LowPassFilter(float cutoffFreq, float resonance, int sampleRate)
        : m_cutoffFreq(cutoffFreq)
        , m_resonance(resonance)
        , m_sampleRate(sampleRate) {
        
        calculateCoefficients();
        reset();
    }
    
    AudioEffectType getType() const override {
        return AudioEffectType::LowPass;
    }
    
    void process(float* buffer, int frameCount, int channels) override {
        for (int i = 0; i < frameCount; i++) {
            for (int ch = 0; ch < channels && ch < 2; ch++) {
                float input = buffer[i * channels + ch];
                
                // Biquad filter
                float output = m_b0 * input + m_b1 * m_x1[ch] + m_b2 * m_x2[ch]
                             - m_a1 * m_y1[ch] - m_a2 * m_y2[ch];
                
                m_x2[ch] = m_x1[ch];
                m_x1[ch] = input;
                m_y2[ch] = m_y1[ch];
                m_y1[ch] = output;
                
                buffer[i * channels + ch] = output;
            }
        }
    }
    
    void reset() override {
        m_x1[0] = m_x1[1] = 0.0f;
        m_x2[0] = m_x2[1] = 0.0f;
        m_y1[0] = m_y1[1] = 0.0f;
        m_y2[0] = m_y2[1] = 0.0f;
    }
    
private:
    void calculateCoefficients() {
        float w0 = 2.0f * M_PI * m_cutoffFreq / m_sampleRate;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * m_resonance);
        
        float a0 = 1.0f + alpha;
        m_a1 = (-2.0f * cosw0) / a0;
        m_a2 = (1.0f - alpha) / a0;
        
        m_b0 = ((1.0f - cosw0) / 2.0f) / a0;
        m_b1 = (1.0f - cosw0) / a0;
        m_b2 = m_b0;
    }
    
    float m_cutoffFreq;
    float m_resonance;
    int m_sampleRate;
    
    float m_b0, m_b1, m_b2;
    float m_a1, m_a2;
    
    float m_x1[2] = {0};
    float m_x2[2] = {0};
    float m_y1[2] = {0};
    float m_y2[2] = {0};
};

// ====================================================================
// DISTORTION EFFECT
// ====================================================================

class DistortionEffect : public AudioEffect {
public:
    DistortionEffect(float drive, float mix)
        : m_drive(drive)
        , m_mix(mix) {
    }
    
    AudioEffectType getType() const override {
        return AudioEffectType::Distortion;
    }
    
    void process(float* buffer, int frameCount, int channels) override {
        for (int i = 0; i < frameCount * channels; i++) {
            float input = buffer[i];
            float driven = input * m_drive;
            
            // Soft clipping (tanh)
            float distorted = std::tanh(driven);
            
            buffer[i] = input * (1.0f - m_mix) + distorted * m_mix;
        }
    }
    
    void reset() override {
        // Nothing to reset
    }
    
private:
    float m_drive;
    float m_mix;
};

} // namespace nk