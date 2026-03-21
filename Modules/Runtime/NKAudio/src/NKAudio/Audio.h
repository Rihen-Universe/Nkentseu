// Audio.hpp
#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace nkentseu {

// ====================================================================
// FORMATS AUDIO
// ====================================================================

enum class AudioFormat {
    Unknown,
    WAV,
    MP3,
    OGG,
    FLAC,
    AIFF
};

enum class SampleFormat {
    Unknown,
    Int8,
    Int16,
    Int24,
    Int32,
    Float32,
    Float64
};

// ====================================================================
// AUDIO SAMPLE
// ====================================================================

struct AudioSample {
    std::vector<uint8_t> data;
    int sampleRate = 44100;
    int channels = 2;
    int bitsPerSample = 16;
    SampleFormat format = SampleFormat::Int16;
    
    size_t getFrameCount() const {
        size_t bytesPerSample = bitsPerSample / 8;
        return data.size() / (channels * bytesPerSample);
    }
    
    float getDuration() const {
        return static_cast<float>(getFrameCount()) / sampleRate;
    }
    
    bool isValid() const {
        return !data.empty() && sampleRate > 0 && channels > 0;
    }
};

// ====================================================================
// AUDIO SOURCE (3D Spatial)
// ====================================================================

struct AudioSource {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float direction[3] = {0.0f, 0.0f, -1.0f};
    
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f; // -1.0 (left) to 1.0 (right)
    
    bool looping = false;
    bool positional = false;
    
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float rolloffFactor = 1.0f;
    
    float coneInnerAngle = 360.0f;
    float coneOuterAngle = 360.0f;
    float coneOuterGain = 0.0f;
};

// ====================================================================
// AUDIO LISTENER (3D)
// ====================================================================

struct AudioListener {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float forward[3] = {0.0f, 0.0f, -1.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    
    float masterVolume = 1.0f;
};

// ====================================================================
// AUDIO EFFECT
// ====================================================================

enum class AudioEffectType {
    None,
    Reverb,
    Echo,
    Delay,
    Chorus,
    Flanger,
    Phaser,
    Distortion,
    Compressor,
    Equalizer,
    LowPass,
    HighPass,
    BandPass
};

class AudioEffect {
public:
    virtual ~AudioEffect() = default;
    
    virtual AudioEffectType getType() const = 0;
    virtual void process(float* buffer, int frameCount, int channels) = 0;
    virtual void reset() = 0;
};

// ====================================================================
// AUDIO LOADER
// ====================================================================

class AudioLoader {
public:
    // Détecter le format
    static AudioFormat detectFormat(const uint8_t* data, size_t size);
    static AudioFormat detectFormatFromFile(const std::string& filepath);
    
    // Charger un fichier audio
    static AudioSample load(const std::string& filepath);
    static AudioSample loadFromMemory(const uint8_t* data, size_t size, AudioFormat format = AudioFormat::Unknown);
    
    // Loaders spécifiques
    static AudioSample loadWAV(const uint8_t* data, size_t size);
    static AudioSample loadMP3(const uint8_t* data, size_t size);
    static AudioSample loadOGG(const uint8_t* data, size_t size);
    static AudioSample loadFLAC(const uint8_t* data, size_t size);
    
    // Savers
    static bool saveWAV(const std::string& filepath, const AudioSample& sample);
    
    // Conversion
    static void convertSampleFormat(AudioSample& sample, SampleFormat targetFormat);
    static void resample(AudioSample& sample, int targetSampleRate);
    static void convertChannels(AudioSample& sample, int targetChannels);
};

// ====================================================================
// AUDIO HANDLE
// ====================================================================

struct AudioHandle {
    uint32_t id = 0;
    bool isValid() const { return id != 0; }
    operator bool() const { return isValid(); }
};

// ====================================================================
// AUDIO ENGINE
// ====================================================================

enum class AudioBackend {
    Auto,
    DirectSound,    // Windows
    WASAPI,         // Windows
    CoreAudio,      // macOS/iOS
    ALSA,           // Linux
    PulseAudio,     // Linux
    OpenSLES,       // Android
    AAudio,         // Android (modern)
    WebAudio,       // Emscripten
    Dummy           // Pas de sortie audio
};

class AudioEngine {
public:
    static AudioEngine& instance();
    
    // Initialisation
    bool initialize(AudioBackend backend = AudioBackend::Auto,
                   int sampleRate = 44100,
                   int channels = 2,
                   int bufferSize = 512);
    
    void shutdown();
    
    bool isInitialized() const { return m_initialized; }
    
    // Playback
    AudioHandle play(const AudioSample& sample, const AudioSource& source = AudioSource());
    AudioHandle play(const std::string& filepath, const AudioSource& source = AudioSource());
    
    void stop(AudioHandle handle);
    void pause(AudioHandle handle);
    void resume(AudioHandle handle);
    
    bool isPlaying(AudioHandle handle) const;
    bool isPaused(AudioHandle handle) const;
    
    // Contrôle
    void setVolume(AudioHandle handle, float volume);
    void setPitch(AudioHandle handle, float pitch);
    void setPan(AudioHandle handle, float pan);
    void setLooping(AudioHandle handle, bool looping);
    
    float getVolume(AudioHandle handle) const;
    float getPitch(AudioHandle handle) const;
    float getPan(AudioHandle handle) const;
    bool isLooping(AudioHandle handle) const;
    
    // Position de lecture
    void setPlaybackPosition(AudioHandle handle, float seconds);
    float getPlaybackPosition(AudioHandle handle) const;
    
    // 3D Audio
    void setSourcePosition(AudioHandle handle, float x, float y, float z);
    void setSourceVelocity(AudioHandle handle, float x, float y, float z);
    void setSourceDirection(AudioHandle handle, float x, float y, float z);
    void setSourcePositional(AudioHandle handle, bool positional);
    void setSourceMinDistance(AudioHandle handle, float distance);
    void setSourceMaxDistance(AudioHandle handle, float distance);
    
    void setListenerPosition(float x, float y, float z);
    void setListenerVelocity(float x, float y, float z);
    void setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                               float upX, float upY, float upZ);
    
    // Effets
    void addEffect(AudioHandle handle, AudioEffect* effect);
    void removeEffect(AudioHandle handle, AudioEffect* effect);
    void clearEffects(AudioHandle handle);
    
    // Master controls
    void setMasterVolume(float volume);
    float getMasterVolume() const;
    
    void pauseAll();
    void resumeAll();
    void stopAll();
    
    // Informations
    AudioBackend getBackend() const { return m_backend; }
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    int getBufferSize() const { return m_bufferSize; }
    
    // Callback pour génération procédurale
    using AudioCallback = std::function<void(float* buffer, int frameCount, int channels)>;
    AudioHandle playCallback(AudioCallback callback, const AudioSource& source = AudioSource());
    
private:
    AudioEngine();
    ~AudioEngine();
    
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    
    class AudioEngineImpl;
    std::unique_ptr<AudioEngineImpl> m_impl;
    
    AudioBackend m_backend;
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    bool m_initialized;
};

// ====================================================================
// AUDIO GENERATOR (Synthèse)
// ====================================================================

enum class WaveformType {
    Sine,
    Square,
    Triangle,
    Sawtooth,
    Noise
};

class AudioGenerator {
public:
    // Générer un ton
    static AudioSample generateTone(float frequency, float duration, 
                                   WaveformType waveform = WaveformType::Sine,
                                   int sampleRate = 44100);
    
    // Générer du bruit
    static AudioSample generateNoise(float duration, int sampleRate = 44100);
    
    // Générer un chirp (sweep de fréquence)
    static AudioSample generateChirp(float startFreq, float endFreq, float duration,
                                    int sampleRate = 44100);
    
    // ADSR Envelope
    struct ADSREnvelope {
        float attackTime = 0.1f;
        float decayTime = 0.1f;
        float sustainLevel = 0.7f;
        float releaseTime = 0.2f;
    };
    
    static void applyEnvelope(AudioSample& sample, const ADSREnvelope& envelope);
    
    // Générer un accord
    static AudioSample generateChord(const std::vector<float>& frequencies, 
                                    float duration, int sampleRate = 44100);
};

// ====================================================================
// AUDIO MIXER
// ====================================================================

class AudioMixer {
public:
    AudioMixer(int sampleRate = 44100, int channels = 2);
    
    // Mixer plusieurs samples
    AudioSample mix(const std::vector<AudioSample>& samples, 
                   const std::vector<float>& volumes = {});
    
    // Crossfade entre deux samples
    AudioSample crossfade(const AudioSample& sample1, 
                         const AudioSample& sample2,
                         float duration);
    
    // Concatener des samples
    AudioSample concatenate(const std::vector<AudioSample>& samples);
    
    // Normaliser
    void normalize(AudioSample& sample, float targetPeak = 1.0f);
    
    // Fade in/out
    void fadeIn(AudioSample& sample, float duration);
    void fadeOut(AudioSample& sample, float duration);
    
private:
    int m_sampleRate;
    int m_channels;
};

// ====================================================================
// AUDIO ANALYZER
// ====================================================================

class AudioAnalyzer {
public:
    // Calculer le volume RMS
    static float calculateRMS(const AudioSample& sample);
    
    // Calculer le peak
    static float calculatePeak(const AudioSample& sample);
    
    // Détecter le tempo (BPM)
    static float detectTempo(const AudioSample& sample);
    
    // Détecter la pitch (note dominante)
    static float detectPitch(const AudioSample& sample);
    
    // FFT (Fast Fourier Transform)
    static std::vector<float> computeFFT(const AudioSample& sample, int fftSize = 2048);
    
    // Spectrogram
    static std::vector<std::vector<float>> computeSpectrogram(const AudioSample& sample,
                                                              int windowSize = 2048,
                                                              int hopSize = 512);
};

} // namespace nkentseu