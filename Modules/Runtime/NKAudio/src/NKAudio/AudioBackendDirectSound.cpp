// AudioBackend_DirectSound.cpp
#ifdef _WIN32

#include "AudioBackend.hpp"
#include "Error.hpp"
#include <dsound.h>
#include <thread>
#include <atomic>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

namespace nk {

class DirectSoundBackend : public IAudioBackend {
public:
    DirectSoundBackend()
        : m_dsound(nullptr)
        , m_primaryBuffer(nullptr)
        , m_secondaryBuffer(nullptr)
        , m_sampleRate(44100)
        , m_channels(2)
        , m_bufferSize(512)
        , m_running(false)
        , m_paused(false) {
    }
    
    ~DirectSoundBackend() override {
        shutdown();
    }
    
    bool initialize(int sampleRate, int channels, int bufferSize) override {
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = bufferSize;
        
        // Créer DirectSound
        HRESULT hr = DirectSoundCreate8(nullptr, &m_dsound, nullptr);
        NK_VALIDATE_MSG(nullptr, SUCCEEDED(hr), "Failed to create DirectSound");
        
        // Obtenir la fenêtre
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) hwnd = GetDesktopWindow();
        
        hr = m_dsound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
        NK_VALIDATE_MSG(nullptr, SUCCEEDED(hr), "Failed to set cooperative level");
        
        // Créer le primary buffer
        DSBUFFERDESC primaryDesc = {};
        primaryDesc.dwSize = sizeof(DSBUFFERDESC);
        primaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        
        hr = m_dsound->CreateSoundBuffer(&primaryDesc, &m_primaryBuffer, nullptr);
        NK_VALIDATE_MSG(nullptr, SUCCEEDED(hr), "Failed to create primary buffer");
        
        // Configurer le format
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wfx.nChannels = channels;
        wfx.nSamplesPerSec = sampleRate;
        wfx.wBitsPerSample = 32; // Float32
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        
        hr = m_primaryBuffer->SetFormat(&wfx);
        NK_VALIDATE_MSG(nullptr, SUCCEEDED(hr), "Failed to set primary buffer format");
        
        // Créer le secondary buffer (streaming)
        DSBUFFERDESC secondaryDesc = {};
        secondaryDesc.dwSize = sizeof(DSBUFFERDESC);
        secondaryDesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        secondaryDesc.dwBufferBytes = bufferSize * channels * sizeof(float) * 2; // Double buffer
        secondaryDesc.lpwfxFormat = &wfx;
        
        hr = m_dsound->CreateSoundBuffer(&secondaryDesc, &m_secondaryBuffer, nullptr);
        NK_VALIDATE_MSG(nullptr, SUCCEEDED(hr), "Failed to create secondary buffer");
        
        return true;
    }
    
    void shutdown() override {
        stop();
        
        if (m_secondaryBuffer) {
            m_secondaryBuffer->Release();
            m_secondaryBuffer = nullptr;
        }
        
        if (m_primaryBuffer) {
            m_primaryBuffer->Release();
            m_primaryBuffer = nullptr;
        }
        
        if (m_dsound) {
            m_dsound->Release();
            m_dsound = nullptr;
        }
    }
    
    void setAudioCallback(AudioCallback callback) override {
        m_callback = callback;
    }
    
    void start() override {
        if (m_running) return;
        
        m_running = true;
        m_paused = false;
        
        // Démarrer le thread de streaming
        m_streamThread = std::thread(&DirectSoundBackend::streamThreadFunc, this);
        
        // Démarrer la lecture
        m_secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
    }
    
    void stop() override {
        if (!m_running) return;
        
        m_running = false;
        
        if (m_streamThread.joinable()) {
            m_streamThread.join();
        }
        
        if (m_secondaryBuffer) {
            m_secondaryBuffer->Stop();
        }
    }
    
    void pause() override {
        m_paused = true;
        if (m_secondaryBuffer) {
            m_secondaryBuffer->Stop();
        }
    }
    
    void resume() override {
        m_paused = false;
        if (m_secondaryBuffer) {
            m_secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
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
        DWORD lastPlayPos = 0;
        
        while (m_running) {
            if (m_paused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            DWORD playCursor, writeCursor;
            m_secondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
            
            // Calculer combien on doit écrire
            DWORD bufferBytes = m_bufferSize * m_channels * sizeof(float) * 2;
            DWORD writeBytes = m_bufferSize * m_channels * sizeof(float);
            
            // Si le curseur a avancé suffisamment
            DWORD distance = (playCursor >= lastPlayPos) ? 
                            (playCursor - lastPlayPos) : 
                            (bufferBytes - lastPlayPos + playCursor);
            
            if (distance >= writeBytes) {
                // Générer l'audio via callback
                if (m_callback) {
                    m_callback(buffer.data(), m_bufferSize);
                } else {
                    std::fill(buffer.begin(), buffer.end(), 0.0f);
                }
                
                // Écrire dans le buffer
                void* ptr1 = nullptr;
                void* ptr2 = nullptr;
                DWORD bytes1 = 0, bytes2 = 0;
                
                HRESULT hr = m_secondaryBuffer->Lock(
                    writeCursor, writeBytes,
                    &ptr1, &bytes1,
                    &ptr2, &bytes2,
                    0
                );
                
                if (SUCCEEDED(hr)) {
                    if (ptr1) {
                        std::memcpy(ptr1, buffer.data(), bytes1);
                    }
                    
                    if (ptr2) {
                        std::memcpy(ptr2, 
                                   reinterpret_cast<uint8_t*>(buffer.data()) + bytes1, 
                                   bytes2);
                    }
                    
                    m_secondaryBuffer->Unlock(ptr1, bytes1, ptr2, bytes2);
                }
                
                lastPlayPos = writeCursor;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    IDirectSound8* m_dsound;
    IDirectSoundBuffer* m_primaryBuffer;
    IDirectSoundBuffer* m_secondaryBuffer;
    
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    
    AudioCallback m_callback;
    
    std::thread m_streamThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
};

// Enregistrer le backend
REGISTER_AUDIO_BACKEND(DirectSoundBackend, "DirectSound");

} // namespace nk

#endif // _WIN32