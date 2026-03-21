// AudioBackend_Custom.cpp - EXEMPLE pour créer votre propre backend

#include "AudioBackend.hpp"
#include <thread>
#include <atomic>
#include <iostream>

namespace nk {

// ====================================================================
// EXEMPLE: Backend qui écrit dans un fichier WAV au lieu de jouer
// ====================================================================

class FileWriterBackend : public IAudioBackend {
public:
    FileWriterBackend() 
        : m_sampleRate(44100)
        , m_channels(2)
        , m_bufferSize(512)
        , m_running(false) {
    }
    
    ~FileWriterBackend() override {
        shutdown();
    }
    
    bool initialize(int sampleRate, int channels, int bufferSize) override {
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = bufferSize;
        
        m_file.open("output.raw", std::ios::binary);
        return m_file.is_open();
    }
    
    void shutdown() override {
        stop();
        
        if (m_file.is_open()) {
            m_file.close();
        }
    }
    
    void setAudioCallback(AudioCallback callback) override {
        m_callback = callback;
    }
    
    void start() override {
        if (m_running) return;
        
        m_running = true;
        m_thread = std::thread(&FileWriterBackend::writeThreadFunc, this);
    }
    
    void stop() override {
        if (!m_running) return;
        
        m_running = false;
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    
    void pause() override { /* Not applicable */ }
    void resume() override { /* Not applicable */ }
    
    int getSampleRate() const override { return m_sampleRate; }
    int getChannels() const override { return m_channels; }
    int getBufferSize() const override { return m_bufferSize; }
    float getLatency() const override { return 0.0f; }
    bool isRunning() const override { return m_running; }
    
private:
    void writeThreadFunc() {
        std::vector<float> buffer(m_bufferSize * m_channels);
        
        // Générer 10 secondes d'audio
        int totalFrames = m_sampleRate * 10;
        int framesWritten = 0;
        
        while (m_running && framesWritten < totalFrames) {
            if (m_callback) {
                m_callback(buffer.data(), m_bufferSize);
            }
            
            m_file.write(reinterpret_cast<const char*>(buffer.data()), 
                        buffer.size() * sizeof(float));
            
            framesWritten += m_bufferSize;
        }
        
        m_running = false;
        std::cout << "Audio written to output.raw" << std::endl;
    }
    
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    AudioCallback m_callback;
    std::ofstream m_file;
    std::thread m_thread;
    std::atomic<bool> m_running;
};

REGISTER_AUDIO_BACKEND(FileWriterBackend, "FileWriter");

// ====================================================================
// EXEMPLE: Backend SDL2 Audio (si vous utilisez SDL2)
// ====================================================================

/*
class SDL2AudioBackend : public IAudioBackend {
public:
    // ... implémentation avec SDL_OpenAudioDevice, SDL_QueueAudio, etc.
};

REGISTER_AUDIO_BACKEND(SDL2AudioBackend, "SDL2");
*/

// ====================================================================
// EXEMPLE: Backend WebAudio pour Emscripten
// ====================================================================

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

class WebAudioBackend : public IAudioBackend {
    // Utilise emscripten_set_main_loop pour callback audio
    // Génère via ScriptProcessorNode ou AudioWorklet
};

REGISTER_AUDIO_BACKEND(WebAudioBackend, "WebAudio");
#endif

} // namespace nk