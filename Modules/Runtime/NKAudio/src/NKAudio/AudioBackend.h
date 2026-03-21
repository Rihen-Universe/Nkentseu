// AudioBackend.hpp
#pragma once

#include <cstdint>
#include <functional>

namespace nk {

// ====================================================================
// INTERFACE BACKEND AUDIO (Pour créer vos propres backends)
// ====================================================================

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    
    // Initialisation
    virtual bool initialize(int sampleRate, int channels, int bufferSize) = 0;
    virtual void shutdown() = 0;
    
    // Callback pour remplir le buffer audio
    using AudioCallback = std::function<void(float* buffer, int frameCount)>;
    virtual void setAudioCallback(AudioCallback callback) = 0;
    
    // Contrôle
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    
    // Informations
    virtual int getSampleRate() const = 0;
    virtual int getChannels() const = 0;
    virtual int getBufferSize() const = 0;
    virtual float getLatency() const = 0; // En millisecondes
    
    // État
    virtual bool isRunning() const = 0;
};

// ====================================================================
// FACTORY POUR BACKENDS
// ====================================================================

class AudioBackendFactory {
public:
    using BackendCreator = std::function<IAudioBackend*()>;
    
    // Enregistrer un backend personnalisé
    static void registerBackend(const std::string& name, BackendCreator creator) {
        getRegistry()[name] = creator;
    }
    
    // Créer un backend par nom
    static IAudioBackend* create(const std::string& name) {
        auto& registry = getRegistry();
        auto it = registry.find(name);
        
        if (it != registry.end()) {
            return it->second();
        }
        
        return nullptr;
    }
    
    // Créer le backend par défaut pour la plateforme
    static IAudioBackend* createDefault();
    
    // Liste des backends disponibles
    static std::vector<std::string> getAvailableBackends() {
        std::vector<std::string> backends;
        for (const auto& pair : getRegistry()) {
            backends.push_back(pair.first);
        }
        return backends;
    }
    
private:
    static NkUnorderedMap<std::string, BackendCreator>& getRegistry() {
        static NkUnorderedMap<std::string, BackendCreator> registry;
        return registry;
    }
};

// ====================================================================
// MACRO POUR ENREGISTRER UN BACKEND FACILEMENT
// ====================================================================

#define REGISTER_AUDIO_BACKEND(BackendClass, Name) \
    namespace { \
        struct BackendClass##Registrar { \
            BackendClass##Registrar() { \
                AudioBackendFactory::registerBackend(Name, []() -> IAudioBackend* { \
                    return new BackendClass(); \
                }); \
            } \
        }; \
        static BackendClass##Registrar g_##BackendClass##Registrar; \
    }

} // namespace nk