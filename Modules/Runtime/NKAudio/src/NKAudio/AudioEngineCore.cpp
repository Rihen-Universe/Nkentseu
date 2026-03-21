// AudioEngine_Core.cpp
#include "Audio.hpp"
#include "Error.hpp"
#include <thread>
#include <atomic>
#include <cmath>

namespace nk {

// ====================================================================
// SPATIAL AUDIO CALCULATOR
// ====================================================================

class SpatialAudioCalculator {
public:
    // Calculer le gain et le pan basé sur la position 3D
    struct SpatialResult {
        float leftGain;
        float rightGain;
        float dopplerPitch;
        float distanceAttenuation;
    };
    
    static SpatialResult calculate3D(const AudioSource& source, 
                                     const AudioListener& listener) {
        SpatialResult result;
        result.leftGain = 1.0f;
        result.rightGain = 1.0f;
        result.dopplerPitch = 1.0f;
        result.distanceAttenuation = 1.0f;
        
        if (!source.positional) {
            // Audio 2D simple avec pan
            result.leftGain = std::clamp(1.0f - source.pan, 0.0f, 1.0f);
            result.rightGain = std::clamp(1.0f + source.pan, 0.0f, 1.0f);
            return result;
        }
        
        // Calculer le vecteur source -> listener
        float dx = source.position[0] - listener.position[0];
        float dy = source.position[1] - listener.position[1];
        float dz = source.position[2] - listener.position[2];
        
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        // Distance attenuation (inverse distance law avec min/max)
        if (distance < source.minDistance) {
            result.distanceAttenuation = 1.0f;
        } else if (distance > source.maxDistance) {
            result.distanceAttenuation = 0.0f;
        } else {
            float normalizedDistance = (distance - source.minDistance) / 
                                      (source.maxDistance - source.minDistance);
            result.distanceAttenuation = std::pow(1.0f - normalizedDistance, source.rolloffFactor);
        }
        
        if (distance < 0.001f) {
            return result; // Source à la position du listener
        }
        
        // Normaliser le vecteur
        dx /= distance;
        dy /= distance;
        dz /= distance;
        
        // Calculer le vecteur "right" du listener
        // right = forward × up
        float rightX = listener.forward[1] * listener.up[2] - listener.forward[2] * listener.up[1];
        float rightY = listener.forward[2] * listener.up[0] - listener.forward[0] * listener.up[2];
        float rightZ = listener.forward[0] * listener.up[1] - listener.forward[1] * listener.up[0];
        
        // Normaliser right
        float rightLen = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
        if (rightLen > 0.001f) {
            rightX /= rightLen;
            rightY /= rightLen;
            rightZ /= rightLen;
        }
        
        // Produit scalaire pour déterminer la position gauche/droite
        float dotRight = dx * rightX + dy * rightY + dz * rightZ;
        
        // Produit scalaire pour déterminer avant/arrière
        float dotForward = dx * listener.forward[0] + 
                          dy * listener.forward[1] + 
                          dz * listener.forward[2];
        
        // HRTF simplifié (Head-Related Transfer Function)
        // Calculer l'angle azimutal (-180° à 180°)
        float azimuth = std::atan2(dotRight, dotForward);
        
        // Panning basé sur l'azimuth
        // -90° = gauche, 0° = centre, +90° = droite
        float panAngle = azimuth * (180.0f / M_PI);
        
        // Fonction de panning constant-power (sin/cos)
        float panRadians = panAngle * (M_PI / 180.0f);
        float panNormalized = (panRadians + M_PI) / (2.0f * M_PI); // 0 à 1
        
        result.leftGain = std::cos(panNormalized * M_PI / 2.0f);
        result.rightGain = std::sin(panNormalized * M_PI / 2.0f);
        
        // ITD (Interaural Time Difference) - simulation simple
        // Plus la source est sur le côté, plus le délai entre les oreilles est grand
        float itdFactor = std::abs(dotRight);
        
        // ILD (Interaural Level Difference) - atténuation basée sur la position
        if (dotRight > 0) {
            // Source à droite
            result.leftGain *= (1.0f - itdFactor * 0.3f); // Atténuer l'oreille gauche
        } else {
            // Source à gauche
            result.rightGain *= (1.0f - itdFactor * 0.3f); // Atténuer l'oreille droite
        }
        
        // Effet d'ombre de la tête (head shadow)
        if (dotForward < 0) {
            // Source derrière
            float behindFactor = -dotForward;
            result.leftGain *= (1.0f - behindFactor * 0.4f);
            result.rightGain *= (1.0f - behindFactor * 0.4f);
        }
        
        // Cone attenuation (pour sources directionnelles)
        if (source.coneInnerAngle < 360.0f) {
            // Calculer le vecteur de direction de la source vers le listener
            float toListenerX = -dx;
            float toListenerY = -dy;
            float toListenerZ = -dz;
            
            // Produit scalaire avec la direction de la source
            float dot = toListenerX * source.direction[0] +
                       toListenerY * source.direction[1] +
                       toListenerZ * source.direction[2];
            
            float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / M_PI);
            
            if (angle > source.coneInnerAngle / 2.0f) {
                if (angle > source.coneOuterAngle / 2.0f) {
                    result.distanceAttenuation *= source.coneOuterGain;
                } else {
                    // Interpolation entre inner et outer
                    float t = (angle - source.coneInnerAngle / 2.0f) / 
                             (source.coneOuterAngle / 2.0f - source.coneInnerAngle / 2.0f);
                    float coneGain = 1.0f + t * (source.coneOuterGain - 1.0f);
                    result.distanceAttenuation *= coneGain;
                }
            }
        }
        
        // Doppler effect
        const float SPEED_OF_SOUND = 343.0f; // m/s
        
        // Vitesse relative de la source vers le listener
        float relativeVelocity = 
            (source.velocity[0] - listener.velocity[0]) * dx +
            (source.velocity[1] - listener.velocity[1]) * dy +
            (source.velocity[2] - listener.velocity[2]) * dz;
        
        result.dopplerPitch = SPEED_OF_SOUND / (SPEED_OF_SOUND - relativeVelocity);
        result.dopplerPitch = std::clamp(result.dopplerPitch, 0.5f, 2.0f);
        
        return result;
    }
    
    // HRTF (Head-Related Transfer Function) avancé
    static void applyHRTF(float* buffer, int frameCount, float azimuth, float elevation) {
        // Implémentation HRTF simplifiée
        // Dans une vraie implémentation, on utiliserait des données HRTF mesurées
        
        // Filtrage basé sur l'angle
        float lowpassCutoff = 20000.0f;
        
        if (std::abs(azimuth) > 60.0f) {
            // Sons latéraux : atténuer les hautes fréquences
            lowpassCutoff = 10000.0f - std::abs(azimuth) * 50.0f;
        }
        
        // Élévation affecte aussi le spectre
        if (std::abs(elevation) > 30.0f) {
            lowpassCutoff -= std::abs(elevation) * 30.0f;
        }
        
        // Appliquer un filtre passe-bas simple (à améliorer avec un vrai filtre)
        // Pour cet exemple, on laisse le buffer tel quel
        // Une vraie implémentation utiliserait des IRs (Impulse Responses) HRTF
    }
};

// ====================================================================
// AUDIO ENGINE IMPLEMENTATION
// ====================================================================

class AudioEngine::AudioEngineImpl {
public:
    struct PlayingSound {
        uint32_t id;
        AudioSample sample;
        AudioSource source;
        
        size_t currentFrame;
        bool paused;
        bool finished;
        
        std::vector<AudioEffect*> effects;
        
        // Pour les callbacks
        AudioCallback callback;
        bool isCallback;
        
        PlayingSound() 
            : id(0)
            , currentFrame(0)
            , paused(false)
            , finished(false)
            , isCallback(false) {
        }
    };
    
    AudioEngineImpl()
        : m_nextId(1)
        , m_running(false) {
    }
    
    ~AudioEngineImpl() {
        shutdown();
    }
    
    bool initialize(AudioBackend backend, int sampleRate, int channels, int bufferSize) {
        m_backend = backend;
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = bufferSize;
        
        // Initialiser le backend spécifique
        if (!initializeBackend()) {
            return false;
        }
        
        m_running = true;
        
        // Démarrer le thread audio
        m_audioThread = std::thread(&AudioEngineImpl::audioThreadFunc, this);
        
        return true;
    }
    
    void shutdown() {
        if (m_running) {
            m_running = false;
            
            if (m_audioThread.joinable()) {
                m_audioThread.join();
            }
            
            shutdownBackend();
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_playingSounds.clear();
    }
    
    AudioHandle play(const AudioSample& sample, const AudioSource& source) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        PlayingSound sound;
        sound.id = m_nextId++;
        sound.sample = sample;
        sound.source = source;
        sound.currentFrame = 0;
        sound.paused = false;
        sound.finished = false;
        sound.isCallback = false;
        
        // Convertir au format du moteur si nécessaire
        if (sound.sample.sampleRate != m_sampleRate) {
            AudioLoader::resample(sound.sample, m_sampleRate);
        }
        
        if (sound.sample.channels != m_channels) {
            AudioLoader::convertChannels(sound.sample, m_channels);
        }
        
        if (sound.sample.format != SampleFormat::Float32) {
            AudioLoader::convertSampleFormat(sound.sample, SampleFormat::Float32);
        }
        
        m_playingSounds.push_back(sound);
        
        AudioHandle handle;
        handle.id = sound.id;
        return handle;
    }
    
    AudioHandle playCallback(AudioCallback callback, const AudioSource& source) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        PlayingSound sound;
        sound.id = m_nextId++;
        sound.source = source;
        sound.callback = callback;
        sound.isCallback = true;
        sound.currentFrame = 0;
        sound.paused = false;
        sound.finished = false;
        
        m_playingSounds.push_back(sound);
        
        AudioHandle handle;
        handle.id = sound.id;
        return handle;
    }
    
    void stop(AudioHandle handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = std::find_if(m_playingSounds.begin(), m_playingSounds.end(),
            [handle](const PlayingSound& s) { return s.id == handle.id; });
        
        if (it != m_playingSounds.end()) {
            m_playingSounds.erase(it);
        }
    }
    
    void pause(AudioHandle handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->paused = true;
        }
    }
    
    void resume(AudioHandle handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->paused = false;
        }
    }
    
    bool isPlaying(AudioHandle handle) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        return it != m_playingSounds.end() && !it->finished;
    }
    
    void setVolume(AudioHandle handle, float volume) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->source.volume = volume;
        }
    }
    
    void setPitch(AudioHandle handle, float pitch) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->source.pitch = pitch;
        }
    }
    
    void setSourcePosition(AudioHandle handle, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->source.position[0] = x;
            it->source.position[1] = y;
            it->source.position[2] = z;
        }
    }
    
    void setSourceVelocity(AudioHandle handle, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->source.velocity[0] = x;
            it->source.velocity[1] = y;
            it->source.velocity[2] = z;
        }
    }
    
    void addEffect(AudioHandle handle, AudioEffect* effect) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = findSound(handle);
        if (it != m_playingSounds.end()) {
            it->effects.push_back(effect);
        }
    }
    
    void setListenerPosition(float x, float y, float z) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_listener.position[0] = x;
        m_listener.position[1] = y;
        m_listener.position[2] = z;
    }
    
    void setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                               float upX, float upY, float upZ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_listener.forward[0] = forwardX;
        m_listener.forward[1] = forwardY;
        m_listener.forward[2] = forwardZ;
        
        m_listener.up[0] = upX;
        m_listener.up[1] = upY;
        m_listener.up[2] = upZ;
    }
    
    void setMasterVolume(float volume) {
        m_listener.masterVolume = volume;
    }
    
    float getMasterVolume() const {
        return m_listener.masterVolume;
    }
    
private:
    std::vector<PlayingSound>::iterator findSound(AudioHandle handle) {
        return std::find_if(m_playingSounds.begin(), m_playingSounds.end(),
            [handle](const PlayingSound& s) { return s.id == handle.id; });
    }
    
    std::vector<PlayingSound>::const_iterator findSound(AudioHandle handle) const {
        return std::find_if(m_playingSounds.begin(), m_playingSounds.end(),
            [handle](const PlayingSound& s) { return s.id == handle.id; });
    }
    
    void audioThreadFunc() {
        std::vector<float> mixBuffer(m_bufferSize * m_channels);
        std::vector<float> tempBuffer(m_bufferSize * m_channels);
        
        while (m_running) {
            // Clear mix buffer
            std::fill(mixBuffer.begin(), mixBuffer.end(), 0.0f);
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                // Mixer tous les sons en cours
                auto it = m_playingSounds.begin();
                while (it != m_playingSounds.end()) {
                    if (it->finished) {
                        it = m_playingSounds.erase(it);
                        continue;
                    }
                    
                    if (!it->paused) {
                        mixSound(*it, tempBuffer.data(), m_bufferSize);
                        
                        // Ajouter au mix buffer
                        for (size_t i = 0; i < mixBuffer.size(); i++) {
                            mixBuffer[i] += tempBuffer[i];
                        }
                    }
                    
                    ++it;
                }
            }
            
            // Appliquer le master volume
            for (size_t i = 0; i < mixBuffer.size(); i++) {
                mixBuffer[i] *= m_listener.masterVolume;
            }
            
            // Clipping
            for (size_t i = 0; i < mixBuffer.size(); i++) {
                mixBuffer[i] = std::clamp(mixBuffer[i], -1.0f, 1.0f);
            }
            
            // Envoyer au backend audio
            outputAudio(mixBuffer.data(), m_bufferSize);
        }
    }
    
    void mixSound(PlayingSound& sound, float* output, int frameCount) {
        std::fill(output, output + frameCount * m_channels, 0.0f);
        
        if (sound.isCallback) {
            // Générer depuis le callback
            sound.callback(output, frameCount, m_channels);
        } else {
            // Lire depuis le sample
            const float* sampleData = reinterpret_cast<const float*>(sound.sample.data.data());
            size_t totalFrames = sound.sample.getFrameCount();
            
            for (int i = 0; i < frameCount; i++) {
                if (sound.currentFrame >= totalFrames) {
                    if (sound.source.looping) {
                        sound.currentFrame = 0;
                    } else {
                        sound.finished = true;
                        break;
                    }
                }
                
                // Lire avec pitch shifting (resampling simple)
                float srcPos = sound.currentFrame;
                size_t srcIndex = static_cast<size_t>(srcPos);
                float frac = srcPos - srcIndex;
                
                for (int ch = 0; ch < m_channels; ch++) {
                    float s0 = sampleData[srcIndex * m_channels + ch];
                    float s1 = (srcIndex + 1 < totalFrames) ? 
                              sampleData[(srcIndex + 1) * m_channels + ch] : 0.0f;
                    
                    output[i * m_channels + ch] = s0 + frac * (s1 - s0);
                }
                
                sound.currentFrame += sound.source.pitch;
            }
        }
        
        // Appliquer la spatialisation 3D
        auto spatialResult = SpatialAudioCalculator::calculate3D(sound.source, m_listener);
        
        for (int i = 0; i < frameCount; i++) {
            if (m_channels >= 2) {
                float left = output[i * m_channels];
                float right = output[i * m_channels + 1];
                
                output[i * m_channels] = left * spatialResult.leftGain * 
                                        spatialResult.distanceAttenuation * sound.source.volume;
                output[i * m_channels + 1] = right * spatialResult.rightGain * 
                                            spatialResult.distanceAttenuation * sound.source.volume;
            } else {
                // Mono
                output[i] *= spatialResult.distanceAttenuation * sound.source.volume;
            }
        }
        
        // Appliquer les effets
        for (auto* effect : sound.effects) {
            effect->process(output, frameCount, m_channels);
        }
    }
    
    // Backend-specific implementations
    bool initializeBackend();
    void shutdownBackend();
    void outputAudio(const float* buffer, int frameCount);
    
    AudioBackend m_backend;
    int m_sampleRate;
    int m_channels;
    int m_bufferSize;
    
    std::vector<PlayingSound> m_playingSounds;
    AudioListener m_listener;
    
    uint32_t m_nextId;
    
    mutable std::mutex m_mutex;
    std::thread m_audioThread;
    std::atomic<bool> m_running;
    
    // Platform-specific data
#if defined(_WIN32)
    void* m_dsound; // IDirectSound8*
    void* m_primaryBuffer;
    void* m_secondaryBuffer;
#elif defined(__linux__) && !defined(__ANDROID__)
    void* m_alsaHandle; // snd_pcm_t*
#elif defined(__APPLE__)
    void* m_audioQueue; // AudioQueueRef
#elif defined(__ANDROID__)
    void* m_slEngine; // SLEngineItf
    void* m_slOutput; // SLObjectItf
#endif
};

} // namespace nk