// AudioLoaderWAV.cpp
#include "Audio.hpp"
#include "Error.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace nk {

#pragma pack(push, 1)

struct WAVHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;
    char wave[4];           // "WAVE"
};

struct WAVFormatChunk {
    char fmt[4];            // "fmt "
    uint32_t chunkSize;
    uint16_t audioFormat;   // 1 = PCM
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct WAVDataChunk {
    char data[4];           // "data"
    uint32_t dataSize;
};

#pragma pack(pop)

AudioSample AudioLoader::loadWAV(const uint8_t* data, size_t size) {
    AudioSample sample;
    
    NK_VALIDATE_MSG(data, size >= sizeof(WAVHeader) + sizeof(WAVFormatChunk) + sizeof(WAVDataChunk),
                   "Invalid WAV file size");
    
    const uint8_t* ptr = data;
    
    // Lire le header RIFF
    WAVHeader header;
    std::memcpy(&header, ptr, sizeof(WAVHeader));
    ptr += sizeof(WAVHeader);
    
    NK_VALIDATE_MSG(nullptr, std::memcmp(header.riff, "RIFF", 4) == 0,
                   "Invalid WAV file: missing RIFF header");
    
    NK_VALIDATE_MSG(nullptr, std::memcmp(header.wave, "WAVE", 4) == 0,
                   "Invalid WAV file: missing WAVE header");
    
    // Lire le format chunk
    WAVFormatChunk formatChunk;
    std::memcpy(&formatChunk, ptr, sizeof(WAVFormatChunk));
    ptr += sizeof(WAVFormatChunk);
    
    NK_VALIDATE_MSG(nullptr, std::memcmp(formatChunk.fmt, "fmt ", 4) == 0,
                   "Invalid WAV file: missing fmt chunk");
    
    NK_VALIDATE_MSG(nullptr, formatChunk.audioFormat == 1,
                   "Unsupported WAV format (only PCM supported)");
    
    sample.sampleRate = formatChunk.sampleRate;
    sample.channels = formatChunk.numChannels;
    sample.bitsPerSample = formatChunk.bitsPerSample;
    
    // Déterminer le format
    if (formatChunk.bitsPerSample == 8) {
        sample.format = SampleFormat::Int8;
    } else if (formatChunk.bitsPerSample == 16) {
        sample.format = SampleFormat::Int16;
    } else if (formatChunk.bitsPerSample == 24) {
        sample.format = SampleFormat::Int24;
    } else if (formatChunk.bitsPerSample == 32) {
        sample.format = SampleFormat::Int32;
    } else {
        NK_ERROR(ErrorCategory::Audio, "Unsupported bits per sample");
        return sample;
    }
    
    // Skip extra format bytes si présents
    if (formatChunk.chunkSize > 16) {
        ptr += formatChunk.chunkSize - 16;
    }
    
    // Chercher le data chunk
    while (ptr < data + size - 8) {
        char chunkId[4];
        std::memcpy(chunkId, ptr, 4);
        ptr += 4;
        
        uint32_t chunkSize;
        std::memcpy(&chunkSize, ptr, 4);
        ptr += 4;
        
        if (std::memcmp(chunkId, "data", 4) == 0) {
            // Trouvé le data chunk
            sample.data.assign(ptr, ptr + chunkSize);
            break;
        } else {
            // Skip ce chunk
            ptr += chunkSize;
        }
    }
    
    NK_VALIDATE_MSG(nullptr, !sample.data.empty(), "No audio data found in WAV file");
    
    return sample;
}

bool AudioLoader::saveWAV(const std::string& filepath, const AudioSample& sample) {
    NK_VALIDATE_MSG(nullptr, sample.isValid(), "Invalid audio sample");
    
    std::ofstream file(filepath, std::ios::binary);
    NK_VALIDATE_MSG(nullptr, file.is_open(), "Cannot open file for writing");
    
    // RIFF header
    WAVHeader header;
    std::memcpy(header.riff, "RIFF", 4);
    header.fileSize = 36 + sample.data.size();
    std::memcpy(header.wave, "WAVE", 4);
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));
    
    // Format chunk
    WAVFormatChunk formatChunk;
    std::memcpy(formatChunk.fmt, "fmt ", 4);
    formatChunk.chunkSize = 16;
    formatChunk.audioFormat = 1; // PCM
    formatChunk.numChannels = sample.channels;
    formatChunk.sampleRate = sample.sampleRate;
    formatChunk.bitsPerSample = sample.bitsPerSample;
    formatChunk.blockAlign = sample.channels * (sample.bitsPerSample / 8);
    formatChunk.byteRate = sample.sampleRate * formatChunk.blockAlign;
    
    file.write(reinterpret_cast<const char*>(&formatChunk), sizeof(WAVFormatChunk));
    
    // Data chunk
    WAVDataChunk dataChunk;
    std::memcpy(dataChunk.data, "data", 4);
    dataChunk.dataSize = sample.data.size();
    
    file.write(reinterpret_cast<const char*>(&dataChunk), sizeof(WAVDataChunk));
    file.write(reinterpret_cast<const char*>(sample.data.data()), sample.data.size());
    
    file.close();
    return true;
}

// ====================================================================
// AUDIO LOADER - MAIN
// ====================================================================

AudioFormat AudioLoader::detectFormat(const uint8_t* data, size_t size) {
    if (size < 4) return AudioFormat::Unknown;
    
    // WAV (RIFF)
    if (std::memcmp(data, "RIFF", 4) == 0) {
        return AudioFormat::WAV;
    }
    
    // MP3 (ID3 ou 0xFF 0xFB)
    if (std::memcmp(data, "ID3", 3) == 0 || 
        (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)) {
        return AudioFormat::MP3;
    }
    
    // OGG (OggS)
    if (std::memcmp(data, "OggS", 4) == 0) {
        return AudioFormat::OGG;
    }
    
    // FLAC (fLaC)
    if (std::memcmp(data, "fLaC", 4) == 0) {
        return AudioFormat::FLAC;
    }
    
    // AIFF (FORM)
    if (std::memcmp(data, "FORM", 4) == 0) {
        return AudioFormat::AIFF;
    }
    
    return AudioFormat::Unknown;
}

AudioFormat AudioLoader::detectFormatFromFile(const std::string& filepath) {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "wav") return AudioFormat::WAV;
    if (ext == "mp3") return AudioFormat::MP3;
    if (ext == "ogg") return AudioFormat::OGG;
    if (ext == "flac") return AudioFormat::FLAC;
    if (ext == "aiff" || ext == "aif") return AudioFormat::AIFF;
    
    return AudioFormat::Unknown;
}

AudioSample AudioLoader::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    
    NK_VALIDATE_MSG(nullptr, file.is_open(), "Cannot open audio file");
    
    size_t size = file.tellg();
    file.seekg(0);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();
    
    AudioFormat format = detectFormat(data.data(), data.size());
    
    return loadFromMemory(data.data(), data.size(), format);
}

AudioSample AudioLoader::loadFromMemory(const uint8_t* data, size_t size, AudioFormat format) {
    if (format == AudioFormat::Unknown) {
        format = detectFormat(data, size);
    }
    
    switch (format) {
        case AudioFormat::WAV:
            return loadWAV(data, size);
            
        case AudioFormat::MP3:
            return loadMP3(data, size);
            
        case AudioFormat::OGG:
            return loadOGG(data, size);
            
        case AudioFormat::FLAC:
            return loadFLAC(data, size);
            
        default:
            NK_ERROR(ErrorCategory::Audio, "Unsupported audio format");
            return AudioSample();
    }
}

// ====================================================================
// CONVERSION
// ====================================================================

void AudioLoader::convertSampleFormat(AudioSample& sample, SampleFormat targetFormat) {
    if (sample.format == targetFormat) return;
    
    // Pour simplifier, on convertit tout en Float32 puis vers le format cible
    std::vector<float> floatBuffer;
    size_t frameCount = sample.getFrameCount();
    floatBuffer.resize(frameCount * sample.channels);
    
    // Convertir vers Float32
    for (size_t i = 0; i < frameCount * sample.channels; i++) {
        float value = 0.0f;
        
        switch (sample.format) {
            case SampleFormat::Int8: {
                int8_t s = reinterpret_cast<int8_t*>(sample.data.data())[i];
                value = s / 128.0f;
                break;
            }
            case SampleFormat::Int16: {
                int16_t s = reinterpret_cast<int16_t*>(sample.data.data())[i];
                value = s / 32768.0f;
                break;
            }
            case SampleFormat::Int32: {
                int32_t s = reinterpret_cast<int32_t*>(sample.data.data())[i];
                value = s / 2147483648.0f;
                break;
            }
            case SampleFormat::Float32: {
                value = reinterpret_cast<float*>(sample.data.data())[i];
                break;
            }
            default:
                break;
        }
        
        floatBuffer[i] = value;
    }
    
    // Convertir vers le format cible
    sample.format = targetFormat;
    
    switch (targetFormat) {
        case SampleFormat::Int16: {
            sample.bitsPerSample = 16;
            sample.data.resize(frameCount * sample.channels * 2);
            int16_t* output = reinterpret_cast<int16_t*>(sample.data.data());
            
            for (size_t i = 0; i < frameCount * sample.channels; i++) {
                float clamped = std::clamp(floatBuffer[i], -1.0f, 1.0f);
                output[i] = static_cast<int16_t>(clamped * 32767.0f);
            }
            break;
        }
        
        case SampleFormat::Float32: {
            sample.bitsPerSample = 32;
            sample.data.resize(frameCount * sample.channels * 4);
            std::memcpy(sample.data.data(), floatBuffer.data(), 
                       frameCount * sample.channels * sizeof(float));
            break;
        }
        
        default:
            break;
    }
}

void AudioLoader::resample(AudioSample& sample, int targetSampleRate) {
    if (sample.sampleRate == targetSampleRate) return;
    
    // Resampling linéaire simple
    float ratio = static_cast<float>(targetSampleRate) / sample.sampleRate;
    size_t oldFrameCount = sample.getFrameCount();
    size_t newFrameCount = static_cast<size_t>(oldFrameCount * ratio);
    
    // Convertir en float pour l'interpolation
    std::vector<float> input(oldFrameCount * sample.channels);
    
    for (size_t i = 0; i < oldFrameCount * sample.channels; i++) {
        if (sample.format == SampleFormat::Int16) {
            int16_t s = reinterpret_cast<int16_t*>(sample.data.data())[i];
            input[i] = s / 32768.0f;
        } else if (sample.format == SampleFormat::Float32) {
            input[i] = reinterpret_cast<float*>(sample.data.data())[i];
        }
    }
    
    // Resample
    std::vector<float> output(newFrameCount * sample.channels);
    
    for (size_t i = 0; i < newFrameCount; i++) {
        float srcPos = i / ratio;
        size_t srcIndex = static_cast<size_t>(srcPos);
        float frac = srcPos - srcIndex;
        
        if (srcIndex + 1 < oldFrameCount) {
            for (int ch = 0; ch < sample.channels; ch++) {
                float s0 = input[srcIndex * sample.channels + ch];
                float s1 = input[(srcIndex + 1) * sample.channels + ch];
                output[i * sample.channels + ch] = s0 + frac * (s1 - s0);
            }
        } else {
            for (int ch = 0; ch < sample.channels; ch++) {
                output[i * sample.channels + ch] = input[srcIndex * sample.channels + ch];
            }
        }
    }
    
    // Reconvertir vers le format original
    sample.sampleRate = targetSampleRate;
    
    if (sample.format == SampleFormat::Int16) {
        sample.data.resize(newFrameCount * sample.channels * 2);
        int16_t* outData = reinterpret_cast<int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < newFrameCount * sample.channels; i++) {
            outData[i] = static_cast<int16_t>(std::clamp(output[i], -1.0f, 1.0f) * 32767.0f);
        }
    } else if (sample.format == SampleFormat::Float32) {
        sample.data.resize(newFrameCount * sample.channels * 4);
        std::memcpy(sample.data.data(), output.data(), output.size() * sizeof(float));
    }
}

void AudioLoader::convertChannels(AudioSample& sample, int targetChannels) {
    if (sample.channels == targetChannels) return;
    
    size_t frameCount = sample.getFrameCount();
    
    // Convertir en float
    std::vector<float> input(frameCount * sample.channels);
    
    for (size_t i = 0; i < frameCount * sample.channels; i++) {
        if (sample.format == SampleFormat::Int16) {
            int16_t s = reinterpret_cast<int16_t*>(sample.data.data())[i];
            input[i] = s / 32768.0f;
        }
    }
    
    std::vector<float> output(frameCount * targetChannels);
    
    if (sample.channels == 1 && targetChannels == 2) {
        // Mono -> Stereo
        for (size_t i = 0; i < frameCount; i++) {
            output[i * 2] = input[i];
            output[i * 2 + 1] = input[i];
        }
    } else if (sample.channels == 2 && targetChannels == 1) {
        // Stereo -> Mono
        for (size_t i = 0; i < frameCount; i++) {
            output[i] = (input[i * 2] + input[i * 2 + 1]) * 0.5f;
        }
    } else {
        // Conversion générique
        for (size_t i = 0; i < frameCount; i++) {
            for (int ch = 0; ch < targetChannels; ch++) {
                if (ch < sample.channels) {
                    output[i * targetChannels + ch] = input[i * sample.channels + ch];
                } else {
                    output[i * targetChannels + ch] = 0.0f;
                }
            }
        }
    }
    
    sample.channels = targetChannels;
    
    // Reconvertir
    if (sample.format == SampleFormat::Int16) {
        sample.data.resize(frameCount * targetChannels * 2);
        int16_t* outData = reinterpret_cast<int16_t*>(sample.data.data());
        
        for (size_t i = 0; i < frameCount * targetChannels; i++) {
            outData[i] = static_cast<int16_t>(std::clamp(output[i], -1.0f, 1.0f) * 32767.0f);
        }
    }
}

} // namespace nk