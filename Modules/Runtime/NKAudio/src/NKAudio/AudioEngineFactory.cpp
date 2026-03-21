// AudioEngine_Factory.cpp
#include "Audio.hpp"
#include "AudioBackend.hpp"

namespace nk {

IAudioBackend* AudioBackendFactory::createDefault() {
#if defined(_WIN32)
    return create("DirectSound");
#elif defined(__APPLE__)
    return create("CoreAudio");
#elif defined(__linux__) && !defined(__ANDROID__)
    // Essayer PulseAudio d'abord, puis ALSA
    IAudioBackend* backend = create("PulseAudio");
    if (!backend) {
        backend = create("ALSA");
    }
    return backend;
#elif defined(__ANDROID__)
    return create("OpenSLES");
#elif defined(__EMSCRIPTEN__)
    return create("WebAudio");
#else
    return create("Dummy");
#endif
}

// ====================================================================
// AUDIO ENGINE IMPLEMENTATION (continuation)
// ====================================================================

bool AudioEngine::AudioEngineImpl::initializeBackend() {
    // Créer le backend via la factory
    std::string backendName;
    
    switch (m_backend) {
        case AudioBackend::DirectSound:
            backendName = "DirectSound";
            break;
        case AudioBackend::CoreAudio:
            backendName = "CoreAudio";
            break;
        case AudioBackend::ALSA:
            backendName = "ALSA";
            break;
        case AudioBackend::Auto:
        default:
            // Utiliser le backend par défaut
            m_backendImpl.reset(AudioBackendFactory::createDefault());
            if (!m_backendImpl) {
                NK_ERROR(ErrorCategory::Audio, "No audio backend available");
                return false;
            }
            break;
    }
    
    if (!backendName.empty()) {
        m_backendImpl.reset(AudioBackendFactory::create(backendName));
        
        if (!m_backendImpl) {
            NK_ERROR(ErrorCategory::Audio, 
                    std::string("Failed to create audio backend: ") + backendName);
            return false;
        }
    }
    
    // Initialiser le backend
    if (!m_backendImpl->initialize(m_sampleRate, m_channels, m_bufferSize)) {
        return false;
    }
    
    // Configurer le callback
    m_backendImpl->setAudioCallback([this](float* buffer, int frameCount) {
        this->fillAudioBuffer(buffer, frameCount);
    });
    
    // Démarrer
    m_backendImpl->start();
    
    return true;
}

void AudioEngine::AudioEngineImpl::shutdownBackend() {
    if (m_backendImpl) {
        m_backendImpl->shutdown();
        m_backendImpl.reset();
    }
}

void AudioEngine::AudioEngineImpl::outputAudio(const float* buffer, int frameCount) {
    // Le backend gère déjà l'output via son callback
    // Cette fonction n'est plus nécessaire avec la nouvelle architecture
}

} // namespace nk