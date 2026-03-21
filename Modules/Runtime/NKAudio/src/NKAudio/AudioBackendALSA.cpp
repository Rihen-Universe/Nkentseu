// AudioBackend_ALSA.cpp
#if defined(__linux__) && !defined(__ANDROID__)

#include "AudioBackend.hpp"
#include "Error.hpp"
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>

namespace nk {

class ALSABackend : public IAudioBackend {
public:
    ALSABackend()
        : m_pcm(nullptr)
        , m_sampleRate(44100)
        , m_channels(2)
        , m_bufferSize(512)
        , m_running(false)
        , m_paused(false) {
    }
    
    ~ALSABackend() override {
        shutdown();
    }
    
    bool initialize(int sampleRate, int channels, int bufferSize) override {
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = bufferSize;
        
        // Ouvrir le device ALSA
        int err = snd_pcm_open(&m_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
        NK_VALIDATE_MSG(nullptr, err >= 0, 
                       std::string("Failed to open ALSA device: ") + snd_strerror(err));
        
        // Configurer les paramètres hardware
        snd_pcm_hw_params_t* hwParams;
        snd_pcm_hw_params_alloca(&hwParams);
        
        snd_pcm_hw_params_any(m_pcm, hwParams);
        snd_pcm_hw_params_set_access(m_pcm, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(m_pcm, hwParams, SND_PCM_FORMAT_FLOAT_LE);
        snd_pcm_hw_params_set_channels(m_pcm, hwParams, channels);
        
        unsigned int rate = sampleRate;
        snd_pcm_hw_params_set_rate_near(m_pcm, hwParams, &rate, nullptr);
        
        snd_pcm_uframes_t frames = bufferSize;
        snd_pcm_hw_params_set_period_size_near(m_pcm, hwParams, &frames, nullptr);
        
        err = snd_pcm_hw_params(m_pcm, hwParams);
        NK_VALIDATE_MSG(nullptr, err >= 0,
                       std::string("Failed to set ALSA hw params: ") + snd_strerror(err));
        
        // Configurer les paramètres software
        snd_pcm_sw_params_t* swParams;
        snd_pcm_sw_params_alloca(&swParams);
        
        snd_pcm_sw_params_current(m_pcm, swParams);
        snd_pcm_sw_params_set_start_threshold(m_pcm, swParams, frames);
        snd_pcm_sw_params(m_pcm, swParams);
        
        return true;
    }
    
    void shutdown() override {
        stop();
        
        if (m_pcm) {
            snd_pcm_close(m_pcm);
            m_pcm = nullptr;
        }
    }
    
    void setAudioCallback(AudioCallback callback) override {
        m_callback = callback;
    }
    
    void start() override {
        if (m_running) return;
        
        m_running = true;
        m_paused = false;
        
        m_streamThread = std::thread(&ALSABackend::streamThreadFunc, this);
    }
    
    void stop() override {
        if (!m_running) return;
        
        m_running = false;
        
        if (m_streamThread.joinable()) {
            m_streamThread.join();
        }
        
        if (m_pcm) {
            snd_pcm_drop(m_pcm);
        }
    }
    
    void pause() override {
        m_paused = true;
        if (m_pcm) {
            snd_pcm_pause(m_pcm, 1);
        }
    }
    
    void resume() override {
        m_paused = false;
        if (m_pcm) {
            snd_pcm_pause(m_pcm, 0);
        }
    }
    
    int getSampleRate() const override { return m_sampleRate; }
    int getChannels() const override { return m_channels; }
    int getBufferSize() const override { return m_bufferSize; }
    
    float getLatency() const override {
        return (m_bufferSize * 1000.0f) / m_sampleRate;
    }
    
    bool isRunning() const override { return m_running; }
    
private:
    void streamThreadFunc() {
        std::vector<float> buffer(m_bufferSize * m_channels);
        
        while (m_running) {
            if (m_paused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Générer l'audio
            if (m_callback) {
                m_callback(buffer.data(), m_bufferSize);
            } else {
                std::fill(buffer.begin(), buffer.end(), 0.0f);
            }
            
            // Écrire vers ALSA
            snd_pcm_sframes_t frames = snd_pcm_writei(m_pcm, buffer.data(), m_bufferSize);
            
            if (frames < 0) {
                // Underrun ou erreur
                frames = snd_pcm_recover(m_pcm, frames, 0);
                
                if (frames < 0) {
                    NK_ERROR(ErrorCategory::Audio, 
                            std::string("ALSA write error: ") + snd_strerror(frames));
                    break;
                }
            }
        }
    }
    
    snd_pcm_t* m_pcm;
    
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    
    AudioCallback m_callback;
    
    std::thread m_streamThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
};

REGISTER_AUDIO_BACKEND(ALSABackend, "ALSA");

} // namespace nk

#endif // Linux