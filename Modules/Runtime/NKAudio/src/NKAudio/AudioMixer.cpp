// AudioMixer.cpp
#include "Audio.hpp"
#include <algorithm>
#include <cmath>

namespace nk {

AudioMixer::AudioMixer(int sampleRate, int channels)
    : m_sampleRate(sampleRate)
    , m_channels(channels) {
}

// ====================================================================
// MIX
// ====================================================================

AudioSample AudioMixer::mix(const std::vector<AudioSample>& samples, 
                           const std::vector<float>& volumes) {
    if (samples.empty()) {
        return AudioSample();
    }
    
    // Trouver la durée maximale
    size_t maxFrameCount = 0;
    for (const auto& sample : samples) {
        maxFrameCount = std::max(maxFrameCount, sample.getFrameCount());
    }
    
    AudioSample result;
    result.sampleRate = m_sampleRate;
    result.channels = m_channels;
    result.bitsPerSample = 16;
    result.format = SampleFormat::Int16;
    result.data.resize(maxFrameCount * m_channels * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(result.data.data());
    std::fill(result.data.begin(), result.data.end(), 0);
    
    // Mixer chaque sample
    for (size_t s = 0; s < samples.size(); s++) {
        const AudioSample& sample = samples[s];
        float volume = (s < volumes.size()) ? volumes[s] : 1.0f;
        
        if (sample.format != SampleFormat::Int16) continue;
        
        const int16_t* input = reinterpret_cast<const int16_t*>(sample.data.data());
        size_t frameCount = sample.getFrameCount();
        
        for (size_t i = 0; i < frameCount; i++) {
            for (int ch = 0; ch < std::min(sample.channels, m_channels); ch++) {
                float value = input[i * sample.channels + ch] / 32768.0f;
                value *= volume;
                
                size_t outIndex = i * m_channels + ch;
                float currentValue = output[outIndex] / 32768.0f;
                float mixedValue = currentValue + value;
                
                output[outIndex] = static_cast<int16_t>(
                    std::clamp(mixedValue, -1.0f, 1.0f) * 32767.0f
                );
            }
        }
    }
    
    return result;
}

// ====================================================================
// CROSSFADE
// ====================================================================

AudioSample AudioMixer::crossfade(const AudioSample& sample1, 
                                 const AudioSample& sample2,
                                 float duration) {
    
    size_t crossfadeFrames = static_cast<size_t>(duration * m_sampleRate);
    
    size_t totalFrames = sample1.getFrameCount() + sample2.getFrameCount() - crossfadeFrames;
    
    AudioSample result;
    result.sampleRate = m_sampleRate;
    result.channels = m_channels;
    result.bitsPerSample = 16;
    result.format = SampleFormat::Int16;
    result.data.resize(totalFrames * m_channels * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(result.data.data());
    
    const int16_t* input1 = reinterpret_cast<const int16_t*>(sample1.data.data());
    const int16_t* input2 = reinterpret_cast<const int16_t*>(sample2.data.data());
    
    size_t frame1Count = sample1.getFrameCount();
    size_t frame2Count = sample2.getFrameCount();
    
    // Partie 1 (avant crossfade)
    size_t beforeCrossfade = frame1Count - crossfadeFrames;
    
    for (size_t i = 0; i < beforeCrossfade; i++) {
        for (int ch = 0; ch < m_channels; ch++) {
            output[i * m_channels + ch] = input1[i * sample1.channels + (ch % sample1.channels)];
        }
    }
    
    // Crossfade
    for (size_t i = 0; i < crossfadeFrames; i++) {
        float t = static_cast<float>(i) / crossfadeFrames;
        float gain1 = 1.0f - t;
        float gain2 = t;
        
        for (int ch = 0; ch < m_channels; ch++) {
            float v1 = input1[(beforeCrossfade + i) * sample1.channels + (ch % sample1.channels)] / 32768.0f;
            float v2 = input2[i * sample2.channels + (ch % sample2.channels)] / 32768.0f;
            
            float mixed = v1 * gain1 + v2 * gain2;
            
            output[(beforeCrossfade + i) * m_channels + ch] = 
                static_cast<int16_t>(std::clamp(mixed, -1.0f, 1.0f) * 32767.0f);
        }
    }
    
    // Partie 2 (après crossfade)
    for (size_t i = crossfadeFrames; i < frame2Count; i++) {
        for (int ch = 0; ch < m_channels; ch++) {
            output[(beforeCrossfade + i) * m_channels + ch] = 
                input2[i * sample2.channels + (ch % sample2.channels)];
        }
    }
    
    return result;
}

// ====================================================================
// CONCATENATE
// ====================================================================

AudioSample AudioMixer::concatenate(const std::vector<AudioSample>& samples) {
    if (samples.empty()) {
        return AudioSample();
    }
    
    // Calculer la taille totale
    size_t totalFrames = 0;
    for (const auto& sample : samples) {
        totalFrames += sample.getFrameCount();
    }
    
    AudioSample result;
    result.sampleRate = m_sampleRate;
    result.channels = m_channels;
    result.bitsPerSample = 16;
    result.format = SampleFormat::Int16;
    result.data.resize(totalFrames * m_channels * 2);
    
    int16_t* output = reinterpret_cast<int16_t*>(result.data.data());
    size_t currentFrame = 0;
    
    for (const auto& sample : samples) {
        if (sample.format != SampleFormat::Int16) continue;
        
        const int16_t* input = reinterpret_cast<const int16_t*>(sample.data.data());
        size_t frameCount = sample.getFrameCount();
        
        for (size_t i = 0; i < frameCount; i++) {
            for (int ch = 0; ch < m_channels; ch++) {
                output[(currentFrame + i) * m_channels + ch] = 
                    input[i * sample.channels + (ch % sample.channels)];
            }
        }
        
        currentFrame += frameCount;
    }
    
    return result;
}

// ====================================================================
// NORMALIZE
// ====================================================================

void AudioMixer::normalize(AudioSample& sample, float targetPeak) {
    if (sample.format != SampleFormat::Int16) return;
    
    int16_t* data = reinterpret_cast<int16_t*>(sample.data.data());
    size_t sampleCount = sample.data.size() / 2;
    
    // Trouver le peak
    float peak = 0.0f;
    
    for (size_t i = 0; i < sampleCount; i++) {
        float value = std::abs(data[i] / 32768.0f);
        peak = std::max(peak, value);
    }
    
    if (peak < 0.0001f) return; // Éviter division par zéro
    
    // Appliquer le gain
    float gain = targetPeak / peak;
    
    for (size_t i = 0; i < sampleCount; i++) {
        float value = data[i] / 32768.0f;
        value *= gain;
        data[i] = static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
    }
}

// ====================================================================
// FADE IN
// ====================================================================

void AudioMixer::fadeIn(AudioSample& sample, float duration) {
    size_t fadeFrames = static_cast<size_t>(duration * sample.sampleRate);
    size_t totalFrames = sample.getFrameCount();
    
    fadeFrames = std::min(fadeFrames, totalFrames);
    
    if (sample.format != SampleFormat::Int16) return;
    
    int16_t* data = reinterpret_cast<int16_t*>(sample.data.data());
    
    for (size_t i = 0; i < fadeFrames; i++) {
        float gain = static_cast<float>(i) / fadeFrames;
        
        for (int ch = 0; ch < sample.channels; ch++) {
            size_t index = i * sample.channels + ch;
            float value = data[index] / 32768.0f;
            value *= gain;
            data[index] = static_cast<int16_t>(value * 32767.0f);
        }
    }
}

// ====================================================================
// FADE OUT
// ====================================================================

void AudioMixer::fadeOut(AudioSample& sample, float duration) {
    size_t fadeFrames = static_cast<size_t>(duration * sample.sampleRate);
    size_t totalFrames = sample.getFrameCount();
    
    fadeFrames = std::min(fadeFrames, totalFrames);
    
    if (sample.format != SampleFormat::Int16) return;
    
    int16_t* data = reinterpret_cast<int16_t*>(sample.data.data());
    
    size_t startFrame = totalFrames - fadeFrames;
    
    for (size_t i = 0; i < fadeFrames; i++) {
        float gain = 1.0f - (static_cast<float>(i) / fadeFrames);
        
        for (int ch = 0; ch < sample.channels; ch++) {
            size_t index = (startFrame + i) * sample.channels + ch;
            float value = data[index] / 32768.0f;
            value *= gain;
            data[index] = static_cast<int16_t>(value * 32767.0f);
        }
    }
}

} // namespace nk