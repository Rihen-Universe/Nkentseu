// AudioGenerator.cpp
#include "Audio.hpp"
#include <cmath>
#include <random>

namespace nk {

// ====================================================================
// CONSTANTES
// ====================================================================

const float TWO_PI = 2.0f * M_PI;

// ====================================================================
// GENERATE TONE
// ====================================================================

AudioSample AudioGenerator::generateTone(float frequency, float duration, 
                                        WaveformType waveform, int sampleRate) {
    AudioSample sample;
    sample.sampleRate = sampleRate;
    sample.channels = 1;
    sample.bitsPerSample = 16;
    sample.format = SampleFormat::Int16;
    
    size_t frameCount = static_cast<size_t>(duration * sampleRate);
    sample.data.resize(frameCount * 2); // 16-bit mono
    
    int16_t* output = reinterpret_cast<int16_t*>(sample.data.data());
    
    for (size_t i = 0; i < frameCount; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float phase = TWO_PI * frequency * t;
        float value = 0.0f;
        
        switch (waveform) {
            case WaveformType::Sine:
                value = std::sin(phase);
                break;
                
            case WaveformType::Square:
                value = (std::sin(phase) >= 0.0f) ? 1.0f : -1.0f;
                break;
                
            case WaveformType::Triangle: {
                float normalizedPhase = std::fmod(phase, TWO_PI) / TWO_PI;
                if (normalizedPhase < 0.25f) {
                    value = 4.0f * normalizedPhase;
                } else if (normalizedPhase < 0.75f) {
                    value = 2.0f - 4.0f * normalizedPhase;
                } else {
                    value = 4.0f * normalizedPhase - 4.0f;
                }
                break;
            }
                
            case WaveformType::Sawtooth: {
                float normalizedPhase = std::fmod(phase, TWO_PI) / TWO_PI;
                value = 2.0f * normalizedPhase - 1.0f;
                break;
            }
                
            case WaveformType::Noise: {
                static std::mt19937 gen(std::random_device{}());
                static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                value = dist(gen);
                break;
            }
        }
        
        output[i] = static_cast<int16_t>(value * 32767.0f);
    }
    
    return sample;
}

// ====================================================================
// GENERATE NOISE
// ====================================================================

AudioSample AudioGenerator::generateNoise(float duration, int sampleRate) {
    AudioSample sample;
    sample.sampleRate = sampleRate;
    sample.channels = 1;
    sample.bitsPerSample = 16;
    sample.format = SampleFormat::Int16;
    
    size_t frameCount = static_cast<size_t>(duration * sampleRate);
    sample.data.resize(frameCount * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(sample.data.data());
    
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (size_t i = 0; i < frameCount; i++) {
        output[i] = static_cast<int16_t>(dist(gen) * 32767.0f);
    }
    
    return sample;
}

// ====================================================================
// GENERATE CHIRP
// ====================================================================

AudioSample AudioGenerator::generateChirp(float startFreq, float endFreq, 
                                         float duration, int sampleRate) {
    AudioSample sample;
    sample.sampleRate = sampleRate;
    sample.channels = 1;
    sample.bitsPerSample = 16;
    sample.format = SampleFormat::Int16;
    
    size_t frameCount = static_cast<size_t>(duration * sampleRate);
    sample.data.resize(frameCount * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(sample.data.data());
    
    float freqDelta = (endFreq - startFreq) / duration;
    
    for (size_t i = 0; i < frameCount; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float freq = startFreq + freqDelta * t;
        float phase = TWO_PI * freq * t;
        float value = std::sin(phase);
        
        output[i] = static_cast<int16_t>(value * 32767.0f);
    }
    
    return sample;
}

// ====================================================================
// APPLY ENVELOPE (ADSR)
// ====================================================================

void AudioGenerator::applyEnvelope(AudioSample& sample, const ADSREnvelope& envelope) {
    size_t frameCount = sample.getFrameCount();
    float sampleRate = static_cast<float>(sample.sampleRate);
    
    size_t attackFrames = static_cast<size_t>(envelope.attackTime * sampleRate);
    size_t decayFrames = static_cast<size_t>(envelope.decayTime * sampleRate);
    size_t releaseFrames = static_cast<size_t>(envelope.releaseTime * sampleRate);
    size_t sustainFrames = frameCount - attackFrames - decayFrames - releaseFrames;
    
    if (sample.format == SampleFormat::Int16) {
        int16_t* data = reinterpret_cast<int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount; i++) {
            float envValue = 1.0f;
            
            if (i < attackFrames) {
                // Attack
                envValue = static_cast<float>(i) / attackFrames;
            } else if (i < attackFrames + decayFrames) {
                // Decay
                float t = static_cast<float>(i - attackFrames) / decayFrames;
                envValue = 1.0f - (1.0f - envelope.sustainLevel) * t;
            } else if (i < attackFrames + decayFrames + sustainFrames) {
                // Sustain
                envValue = envelope.sustainLevel;
            } else {
                // Release
                float t = static_cast<float>(i - attackFrames - decayFrames - sustainFrames) / releaseFrames;
                envValue = envelope.sustainLevel * (1.0f - t);
            }
            
            for (int ch = 0; ch < sample.channels; ch++) {
                size_t index = i * sample.channels + ch;
                float value = data[index] / 32768.0f;
                value *= envValue;
                data[index] = static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
            }
        }
    }
}

// ====================================================================
// GENERATE CHORD
// ====================================================================

AudioSample AudioGenerator::generateChord(const std::vector<float>& frequencies, 
                                         float duration, int sampleRate) {
    if (frequencies.empty()) {
        return AudioSample();
    }
    
    AudioSample sample;
    sample.sampleRate = sampleRate;
    sample.channels = 1;
    sample.bitsPerSample = 16;
    sample.format = SampleFormat::Int16;
    
    size_t frameCount = static_cast<size_t>(duration * sampleRate);
    sample.data.resize(frameCount * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(sample.data.data());
    
    for (size_t i = 0; i < frameCount; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float value = 0.0f;
        
        for (float freq : frequencies) {
            value += std::sin(TWO_PI * freq * t);
        }
        
        value /= frequencies.size(); // Normaliser
        
        output[i] = static_cast<int16_t>(value * 32767.0f);
    }
    
    return sample;
}

} // namespace nk