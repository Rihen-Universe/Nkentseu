// AudioBackend_CoreAudio.cpp
#if defined(__APPLE__)

#include "AudioBackend.hpp"
#include "Error.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <atomic>

namespace nk {

class CoreAudioBackend : public IAudioBackend {
public:
    CoreAudioBackend()
        : m_audioQueue(nullptr)
        , m_sampleRate(44100)
        , m_channels(2)
        , m_bufferSize(512)
        , m_running(false)
        , m_paused(false) {
    }
    
    ~CoreAudioBackend() override {
        shutdown();
    }
    
    bool initialize(int sampleRate, int channels, int bufferSize) override {
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = bufferSize;
        
        // Configurer le format audio
        AudioStreamBasicDescription format = {};
        format.mSampleRate = sampleRate;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mBytesPerPacket = sizeof(float) * channels;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = sizeof(float) * channels;
        format.mChannelsPerFrame = channels;
        format.mBitsPerChannel = 32;
        
        // Créer l'audio queue
        OSStatus status = AudioQueueNewOutput(
            &format,
            audioQueueCallback,
            this,
            nullptr,
            nullptr,
            0,
            &m_audioQueue
        );
        
        NK_VALIDATE_MSG(nullptr, status == noErr, "Failed to create CoreAudio queue");
        
        // Créer les buffers
        int bufferBytes = bufferSize * channels * sizeof(float);
        
        for (int i = 0; i < 3; i++) { // Triple buffering
            AudioQueueBufferRef buffer;
            status = AudioQueueAllocateBuffer(m_audioQueue, bufferBytes, &buffer);
            
            if (status == noErr) {
                buffer->mAudioDataByteSize = bufferBytes;
                std::memset(buffer->mAudioData, 0, bufferBytes);
                AudioQueueEnqueueBuffer(m_audioQueue, buffer, 0, nullptr);
            }
        }
        
        return true;
    }
    
    void shutdown() override {
        stop();
        
        if (m_audioQueue) {
            AudioQueueDispose(m_audioQueue, true);
            m_audioQueue = nullptr;
        }
    }
    
    void setAudioCallback(AudioCallback callback) override {
        m_callback = callback;
    }
    
    void start() override {
        if (m_running) return;
        
        m_running = true;
        m_paused = false;
        
        AudioQueueStart(m_audioQueue, nullptr);
    }
    
    void stop() override {
        if (!m_running) return;
        
        m_running = false;
        
        if (m_audioQueue) {
            AudioQueueStop(m_audioQueue, true);
        }
    }
    
    void pause() override {
        m_paused = true;
        if (m_audioQueue) {
            AudioQueuePause(m_audioQueue);
        }
    }
    
    void resume() override {
        m_paused = false;
        if (m_audioQueue) {
            AudioQueueStart(m_audioQueue, nullptr);
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
    static void audioQueueCallback(void* userData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
        CoreAudioBackend* backend = static_cast<CoreAudioBackend*>(userData);
        
        if (!backend->m_running || backend->m_paused) {
            std::memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
        } else {
            float* output = static_cast<float*>(buffer->mAudioData);
            int frameCount = buffer->mAudioDataByteSize / (sizeof(float) * backend->m_channels);
            
            if (backend->m_callback) {
                backend->m_callback(output, frameCount);
            } else {
                std::memset(output, 0, buffer->mAudioDataByteSize);
            }
        }
        
        AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
    }
    
    AudioQueueRef m_audioQueue;
    
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    
    AudioCallback m_callback;
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
};

REGISTER_AUDIO_BACKEND(CoreAudioBackend, "CoreAudio");

} // namespace nk

#endif // __APPLE__