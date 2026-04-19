// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioEngineCore.cpp
// DESCRIPTION: Implémentation du moteur audio AAA — pool de voix, mixage,
//              audio 3D, gestion des effets, backend abstraction
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Architecture lock-free pour le thread audio (temps réel).
//        Zéro STL. Utilise NkVector et NkAtomic de la fondation.
// -----------------------------------------------------------------------------

#include "NkAudio.h"
#include "NkAudioEffects.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

namespace {
    constexpr nkentseu::float32 TWO_PI = 6.28318530717958647692f;

    inline nkentseu::float32 Clampf(nkentseu::float32 v, nkentseu::float32 lo, nkentseu::float32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    inline nkentseu::float32 NkSqrtf(nkentseu::float32 x) { return ::sqrtf(x); }
    inline nkentseu::float32 NkFabsf(nkentseu::float32 x) { return ::fabsf(x); }
    inline nkentseu::float32 NkAtan2f(nkentseu::float32 y, nkentseu::float32 x) { return ::atan2f(y,x); }
    inline nkentseu::float32 NkCosf (nkentseu::float32 x) { return ::cosf(x); }
    inline nkentseu::float32 NkSinf (nkentseu::float32 x) { return ::sinf(x); }
    inline nkentseu::float32 NkPowf (nkentseu::float32 b, nkentseu::float32 e) { return ::powf(b,e); }
} // anonymous namespace

namespace nkentseu {
    namespace audio {

        // ====================================================================
        // CALCUL SPATIAL 3D
        // ====================================================================

        struct SpatialResult {
            float32 leftGain;
            float32 rightGain;
            float32 dopplerPitch;
            float32 distanceAttenuation;
        };

        /**
         * @brief Calcule les paramètres spatiaux (pan 3D, Doppler, atténuation)
         * @internal
         */
        static SpatialResult CalculateSpatial(const AudioSource3D&   source,
                                               const AudioListener3D& listener) {
            SpatialResult result;
            result.leftGain           = 1.0f;
            result.rightGain          = 1.0f;
            result.dopplerPitch       = 1.0f;
            result.distanceAttenuation= 1.0f;

            if (!source.positional) {
                // Audio 2D : panning simple
                float32 pan = Clampf(0.0f, -1.0f, 1.0f); // VoiceParams.pan géré ailleurs
                result.leftGain  = Clampf(1.0f - pan, 0.0f, 1.0f);
                result.rightGain = Clampf(1.0f + pan, 0.0f, 1.0f);
                return result;
            }

            // Vecteur source → listener
            float32 dx = source.position[0] - listener.position[0];
            float32 dy = source.position[1] - listener.position[1];
            float32 dz = source.position[2] - listener.position[2];
            float32 distance = NkSqrtf(dx*dx + dy*dy + dz*dz);

            // Atténuation selon le modèle
            switch (source.attenuationModel) {
                case AttenuationModel::NONE:
                    result.distanceAttenuation = 1.0f;
                    break;
                case AttenuationModel::INVERSE:
                    if (distance < source.minDistance) {
                        result.distanceAttenuation = 1.0f;
                    } else if (distance > source.maxDistance) {
                        result.distanceAttenuation = 0.0f;
                    } else {
                        result.distanceAttenuation = source.minDistance /
                            (source.minDistance + source.rolloffFactor * (distance - source.minDistance));
                    }
                    break;
                case AttenuationModel::LINEAR:
                    result.distanceAttenuation = Clampf(
                        1.0f - source.rolloffFactor * (distance - source.minDistance) /
                               (source.maxDistance - source.minDistance),
                        0.0f, 1.0f);
                    break;
                case AttenuationModel::EXPONENTIAL:
                    if (distance < source.minDistance) {
                        result.distanceAttenuation = 1.0f;
                    } else {
                        result.distanceAttenuation = NkPowf(
                            distance / source.minDistance,
                            -source.rolloffFactor);
                        result.distanceAttenuation = Clampf(result.distanceAttenuation, 0.0f, 1.0f);
                    }
                    break;
            }

            if (distance < 0.001f) return result;

            // Normaliser vecteur direction
            float32 invD = 1.0f / distance;
            dx *= invD; dy *= invD; dz *= invD;

            // Vecteur "right" du listener (forward × up)
            float32 rightX = listener.forward[1]*listener.up[2] - listener.forward[2]*listener.up[1];
            float32 rightY = listener.forward[2]*listener.up[0] - listener.forward[0]*listener.up[2];
            float32 rightZ = listener.forward[0]*listener.up[1] - listener.forward[1]*listener.up[0];
            float32 rightLen = NkSqrtf(rightX*rightX + rightY*rightY + rightZ*rightZ);
            if (rightLen > 0.001f) { rightX /= rightLen; rightY /= rightLen; rightZ /= rightLen; }

            // Azimut pour panning
            float32 dotRight   = dx*rightX   + dy*rightY   + dz*rightZ;
            float32 dotForward = dx*listener.forward[0] + dy*listener.forward[1] + dz*listener.forward[2];
            float32 azimuth    = NkAtan2f(dotRight, dotForward);

            // Panning constant-power
            float32 panAngle     = (azimuth / (3.14159265f * 0.5f)); // [-1, 1]
            panAngle             = Clampf(panAngle, -1.0f, 1.0f);
            float32 panRadians   = (panAngle + 1.0f) * 0.25f * TWO_PI; // [0, π/2]
            result.leftGain      = NkCosf(panRadians);
            result.rightGain     = NkSinf(panRadians);

            // Effet Doppler
            if (source.useDoppler) {
                // Vitesse radiale relative (projection sur la direction)
                float32 srcVelRad = source.velocity[0]*dx   + source.velocity[1]*dy   + source.velocity[2]*dz;
                float32 lisVelRad = listener.velocity[0]*dx + listener.velocity[1]*dy + listener.velocity[2]*dz;
                float32 denom     = AUDIO_SPEED_OF_SOUND + lisVelRad;
                if (NkFabsf(denom) > 0.001f) {
                    result.dopplerPitch = (AUDIO_SPEED_OF_SOUND - srcVelRad) / denom;
                    result.dopplerPitch = Clampf(result.dopplerPitch, 0.05f, 20.0f);
                }
            }

            return result;
        }

        // ====================================================================
        // STRUCTURE INTERNE : VOIX
        // ====================================================================

        struct Voice {
            // ── Identité ────────────────────────────────────────────────────
            uint32      id          = AUDIO_INVALID_ID;
            VoiceState  state       = VoiceState::FREE;
            int32       priority    = 128;

            // ── Source audio ─────────────────────────────────────────────
            const AudioSample* sample   = nullptr;  ///< Non-owning
            usize              readPos  = 0;         ///< Frame position courante
            bool               isProcedural = false;

            // ── Paramètres de lecture ─────────────────────────────────────
            float32  volume      = 1.0f;
            float32  pitch       = 1.0f;
            float32  pan         = 0.0f;
            bool     looping     = false;
            float32  loopStart   = 0.0f;   // en frames
            float32  loopEnd     = -1.0f;  // en frames (-1 = fin)
            float32  fadeInFrames= 0.0f;   // frames de fondu entrant
            float32  fadeOutFrames = 0.0f; // frames de fondu sortant
            float32  fadeProgress = 0.0f;  // progression du fade [0,1]
            float32  subFramePos = 0.0f;   // position sous-frame pour pitch

            // ── Effets DSP ────────────────────────────────────────────────
            IAudioEffect* effects[AUDIO_MAX_EFFECTS_PER_VOICE] = {};
            int32         effectCount = 0;

            // ── Audio 3D ──────────────────────────────────────────────────
            AudioSource3D source3d;

            // ── Callback procédural ───────────────────────────────────────
            AudioEngine::ProceduralCallback proceduralCallback;

            void Reset() {
                id           = AUDIO_INVALID_ID;
                state        = VoiceState::FREE;
                sample       = nullptr;
                readPos      = 0;
                subFramePos  = 0.0f;
                isProcedural = false;
                volume       = 1.0f;
                pitch        = 1.0f;
                pan          = 0.0f;
                looping      = false;
                loopStart    = 0.0f;
                loopEnd      = -1.0f;
                fadeInFrames = 0.0f;
                fadeOutFrames= 0.0f;
                fadeProgress = 0.0f;
                effectCount  = 0;
                for (int32 i = 0; i < AUDIO_MAX_EFFECTS_PER_VOICE; ++i) effects[i] = nullptr;
                proceduralCallback = {};
            }
        };

        // ====================================================================
        // IMPLÉMENTATION CACHÉE (PIMPL)
        // ====================================================================

        struct AudioEngine::Impl {
            // ── Configuration ──────────────────────────────────────────────
            AudioEngineConfig  config;
            bool               initialized = false;

            // ── Backend ────────────────────────────────────────────────────
            IAudioBackend* backend = nullptr;

            // ── Pool de voix ───────────────────────────────────────────────
            Voice    voices[AUDIO_MAX_VOICES];
            uint32   nextId     = 1u; ///< ID auto-incrémenté (jamais 0)
            int32    activeCount = 0;

            // ── Listener ──────────────────────────────────────────────────
            AudioListener3D listener;

            // ── Effets master ─────────────────────────────────────────────
            IAudioEffect* masterEffects[AUDIO_MAX_MASTER_EFFECTS] = {};
            int32         masterEffectCount = 0;

            // ── Buffer de mix intermédiaire ────────────────────────────────
            float32* mixBuffer     = nullptr;
            int32    mixBufferSize = 0;

            // ── Volume master ─────────────────────────────────────────────
            float32 masterVolume = 1.0f;

            // ── Atomics pour communication inter-thread ───────────────────
            NkAtomic<bool> shutdownRequested;

            // ──────────────────────────────────────────────────────────────
            // Cherche une voix libre (priorise les voix de plus faible priorité)
            Voice* AcquireVoice(int32 priority) {
                // Cherche une voix FREE
                for (int32 i = 0; i < config.maxVoices; ++i) {
                    if (voices[i].state == VoiceState::FREE) {
                        voices[i].Reset();
                        voices[i].id       = nextId++;
                        if (nextId == AUDIO_INVALID_ID) nextId = 1u; // overflow safe
                        voices[i].priority = priority;
                        activeCount++;
                        return &voices[i];
                    }
                }

                // Pool plein : voler la voix de plus basse priorité et d'état non-critique
                Voice* victim = nullptr;
                for (int32 i = 0; i < config.maxVoices; ++i) {
                    if (voices[i].priority <= priority &&
                        voices[i].state != VoiceState::STOPPING) {
                        if (!victim || voices[i].priority < victim->priority) {
                            victim = &voices[i];
                        }
                    }
                }

                if (victim) {
                    victim->Reset();
                    victim->id       = nextId++;
                    if (nextId == AUDIO_INVALID_ID) nextId = 1u;
                    victim->priority = priority;
                    return victim;
                }

                return nullptr; // Pool vraiment plein, priorité insuffisante
            }

            Voice* FindVoice(AudioHandle handle) {
                if (!handle.IsValid()) return nullptr;
                for (int32 i = 0; i < config.maxVoices; ++i) {
                    if (voices[i].id == handle.id && voices[i].state != VoiceState::FREE) {
                        return &voices[i];
                    }
                }
                return nullptr;
            }

            // ──────────────────────────────────────────────────────────────
            // CALLBACK AUDIO (appelé depuis le thread temps réel du backend)
            // Zéro allocation, zéro lock
            // ──────────────────────────────────────────────────────────────

            void AudioCallback(float32* outputBuffer, int32 frameCount, int32 channels) {
                // Zeroing du buffer de sortie
                memset(outputBuffer, 0, (usize)frameCount * (usize)channels * sizeof(float32));

                // Buffer de mix par voix (réutilisation du mixBuffer pré-alloué)
                if (!mixBuffer || mixBufferSize < frameCount * channels) {
                    // Ne pas allouer dans le callback RT : ignorer le surplus
                    frameCount = mixBufferSize / channels;
                    if (frameCount <= 0) return;
                }

                for (int32 v = 0; v < config.maxVoices; ++v) {
                    Voice& voice = voices[v];

                    if (voice.state == VoiceState::FREE || voice.state == VoiceState::FINISHED)
                        continue;

                    // Calcul spatial
                    SpatialResult spatial = CalculateSpatial(voice.source3d, listener);
                    float32 finalVol = voice.volume * masterVolume * spatial.distanceAttenuation;
                    float32 finalPitch = voice.pitch * spatial.dopplerPitch;

                    // Remplir le mixBuffer pour cette voix
                    memset(mixBuffer, 0, (usize)frameCount * (usize)channels * sizeof(float32));

                    if (voice.isProcedural && voice.proceduralCallback) {
                        voice.proceduralCallback(mixBuffer, frameCount, channels);

                    } else if (voice.sample && voice.sample->IsValid()) {
                        const AudioSample& s    = *voice.sample;
                        int32              sCh  = s.channels;

                        for (int32 f = 0; f < frameCount; ++f) {
                            // Lecture avec pitch (interpolation linéaire)
                            double exactPos = (double)voice.readPos + (double)voice.subFramePos;

                            usize  srcFrame = (usize)exactPos;
                            float32 frac    = (float32)(exactPos - (double)srcFrame);

                            if (srcFrame + 1 >= s.frameCount) {
                                if (voice.looping) {
                                    srcFrame  = (usize)voice.loopStart;
                                    frac      = 0.0f;
                                    voice.readPos   = srcFrame;
                                    voice.subFramePos = 0.0f;
                                } else {
                                    voice.state = VoiceState::FINISHED;
                                    break;
                                }
                            }

                            for (int32 c = 0; c < channels; ++c) {
                                int32 srcC = (c < sCh) ? c : sCh - 1;
                                float32 a = s.data[srcFrame     * (usize)sCh + (usize)srcC];
                                float32 b = (srcFrame + 1 < s.frameCount)
                                          ? s.data[(srcFrame+1) * (usize)sCh + (usize)srcC]
                                          : 0.0f;
                                mixBuffer[f * channels + c] = a + (b - a) * frac;
                            }

                            // Avancer la position (supporte pitch != 1)
                            voice.subFramePos += finalPitch;
                            while (voice.subFramePos >= 1.0f) {
                                voice.readPos++;
                                voice.subFramePos -= 1.0f;
                            }
                        }
                    }

                    // Appliquer les effets de la voix
                    for (int32 e = 0; e < voice.effectCount; ++e) {
                        if (voice.effects[e] && voice.effects[e]->IsEnabled()) {
                            voice.effects[e]->Process(mixBuffer, frameCount, channels);
                        }
                    }

                    // Gain spatial + panning + fade
                    for (int32 f = 0; f < frameCount; ++f) {
                        float32 fadeGain = 1.0f;
                        if (voice.fadeInFrames > 0.0f) {
                            fadeGain = Clampf(voice.fadeProgress / voice.fadeInFrames, 0.0f, 1.0f);
                            voice.fadeProgress++;
                        }

                        for (int32 c = 0; c < channels; ++c) {
                            float32 chGain = (c == 0) ? spatial.leftGain : spatial.rightGain;
                            // Pan 2D (override si non-positional)
                            if (!voice.source3d.positional) {
                                chGain = (c == 0) ? Clampf(1.0f - voice.pan, 0.0f, 1.0f)
                                                  : Clampf(1.0f + voice.pan, 0.0f, 1.0f);
                            }
                            outputBuffer[f * channels + c] +=
                                mixBuffer[f * channels + c] * finalVol * chGain * fadeGain;
                        }
                    }

                } // for each voice

                // Effets master
                for (int32 e = 0; e < masterEffectCount; ++e) {
                    if (masterEffects[e] && masterEffects[e]->IsEnabled()) {
                        masterEffects[e]->Process(outputBuffer, frameCount, channels);
                    }
                }

                // Limiter le signal de sortie (soft limiter)
                for (int32 i = 0; i < frameCount * channels; ++i) {
                    float32& s = outputBuffer[i];
                    // Tanh soft clip pour éviter toute saturation hardware
                    s = ::tanhf(s * 0.95f);
                }

                // Nettoyer les voix FINISHED
                for (int32 v = 0; v < config.maxVoices; ++v) {
                    if (voices[v].state == VoiceState::FINISHED) {
                        voices[v].state = VoiceState::FREE;
                        voices[v].id    = AUDIO_INVALID_ID;
                        activeCount--;
                        if (activeCount < 0) activeCount = 0;
                    }
                }
            }
        };

        // ====================================================================
        // AudioEngine — Singleton + Lifecycle
        // ====================================================================

        AudioEngine& AudioEngine::Instance() {
            static AudioEngine sInstance;
            return sInstance;
        }

        AudioEngine::AudioEngine() {
            mImpl = new Impl{};
        }

        AudioEngine::~AudioEngine() {
            if (mImpl->initialized) Shutdown();
            delete mImpl;
        }

        bool AudioEngine::Initialize(const AudioEngineConfig& config) {
            if (mImpl->initialized) return true;

            mImpl->config = config;

            // Limiter maxVoices au pool statique
            if (mImpl->config.maxVoices > AUDIO_MAX_VOICES) {
                mImpl->config.maxVoices = AUDIO_MAX_VOICES;
            }

            // Allouer le buffer de mix
            mImpl->mixBufferSize = config.bufferSize * config.channels;
            memory::NkAllocator* alloc = config.allocator;
            if (alloc) {
                mImpl->mixBuffer = (float32*)alloc->Allocate(
                    (usize)mImpl->mixBufferSize * sizeof(float32), sizeof(float32));
            } else {
                mImpl->mixBuffer = (float32*)::operator new((usize)mImpl->mixBufferSize * sizeof(float32));
            }

            // Créer le backend
            mImpl->backend = AudioBackendFactory::CreateByType(config.backend);
            if (!mImpl->backend) return false;

            // Configurer le callback
            mImpl->backend->SetCallback(
                [this](float32* buf, int32 frames, int32 channels) {
                    mImpl->AudioCallback(buf, frames, channels);
                }
            );

            bool ok = mImpl->backend->Initialize(
                config.sampleRate, config.channels, config.bufferSize);

            if (!ok) {
                delete mImpl->backend;
                mImpl->backend = nullptr;
                return false;
            }

            mImpl->backend->Start();
            mImpl->masterVolume = config.masterVolume;
            mImpl->initialized  = true;
            return true;
        }

        void AudioEngine::Shutdown() {
            if (!mImpl->initialized) return;

            if (mImpl->backend) {
                mImpl->backend->Stop();
                mImpl->backend->Shutdown();
                delete mImpl->backend;
                mImpl->backend = nullptr;
            }

            memory::NkAllocator* alloc = mImpl->config.allocator;
            if (alloc) alloc->Free(mImpl->mixBuffer);
            else ::operator delete(mImpl->mixBuffer);
            mImpl->mixBuffer = nullptr;

            mImpl->initialized = false;
        }

        bool AudioEngine::IsInitialized() const { return mImpl->initialized; }

        // ────── Lecture ──────────────────────────────────────────────────────

        AudioHandle AudioEngine::Play(const AudioSample& sample, const VoiceParams& params) {
            if (!mImpl->initialized || !sample.IsValid()) return AUDIO_HANDLE_INVALID;

            Voice* v = mImpl->AcquireVoice(params.priority);
            if (!v) return AUDIO_HANDLE_INVALID;

            v->sample        = &sample;
            v->state         = VoiceState::PLAYING;
            v->volume        = params.volume;
            v->pitch         = params.pitch;
            v->pan           = params.pan;
            v->looping       = params.looping;
            v->loopStart     = params.loopStart * (float32)sample.sampleRate;
            v->loopEnd       = (params.loopEnd < 0.0f) ? -1.0f : params.loopEnd * (float32)sample.sampleRate;
            v->fadeInFrames  = params.fadeInTime * (float32)mImpl->config.sampleRate;
            v->readPos       = (usize)(params.startOffset * (float32)sample.sampleRate);
            v->subFramePos   = 0.0f;
            v->fadeProgress  = 0.0f;
            v->source3d      = params.source3d;
            v->isProcedural  = false;

            return AudioHandle{ v->id };
        }

        AudioHandle AudioEngine::PlayProcedural(ProceduralCallback callback,
                                                 const VoiceParams& params) {
            if (!mImpl->initialized || !callback) return AUDIO_HANDLE_INVALID;

            Voice* v = mImpl->AcquireVoice(params.priority);
            if (!v) return AUDIO_HANDLE_INVALID;

            v->state               = VoiceState::PLAYING;
            v->isProcedural        = true;
            v->proceduralCallback  = callback;
            v->volume              = params.volume;
            v->pitch               = params.pitch;
            v->pan                 = params.pan;
            v->source3d            = params.source3d;

            return AudioHandle{ v->id };
        }

        // ────── Contrôle voix ─────────────────────────────────────────────────

        void AudioEngine::Stop(AudioHandle handle, float32 /*fadeOutTime*/) {
            Voice* v = mImpl->FindVoice(handle);
            if (v) v->state = VoiceState::FINISHED;
        }

        void AudioEngine::Pause(AudioHandle handle) {
            Voice* v = mImpl->FindVoice(handle);
            if (v && v->state == VoiceState::PLAYING) v->state = VoiceState::PAUSED;
        }

        void AudioEngine::Resume(AudioHandle handle) {
            Voice* v = mImpl->FindVoice(handle);
            if (v && v->state == VoiceState::PAUSED) v->state = VoiceState::PLAYING;
        }

        bool AudioEngine::IsPlaying(AudioHandle handle) const {
            const Voice* v = mImpl->FindVoice(handle);
            return v && v->state == VoiceState::PLAYING;
        }

        bool AudioEngine::IsPaused(AudioHandle handle) const {
            const Voice* v = mImpl->FindVoice(handle);
            return v && v->state == VoiceState::PAUSED;
        }

        bool AudioEngine::IsLooping(AudioHandle handle) const {
            const Voice* v = mImpl->FindVoice(handle);
            return v && v->looping;
        }

        void AudioEngine::SetVolume(AudioHandle handle, float32 volume) {
            Voice* v = mImpl->FindVoice(handle); if (v) v->volume = volume;
        }
        void AudioEngine::SetPitch(AudioHandle handle, float32 pitch) {
            Voice* v = mImpl->FindVoice(handle); if (v) v->pitch = pitch;
        }
        void AudioEngine::SetPan(AudioHandle handle, float32 pan) {
            Voice* v = mImpl->FindVoice(handle); if (v) v->pan = Clampf(pan, -1.0f, 1.0f);
        }
        void AudioEngine::SetLooping(AudioHandle handle, bool looping) {
            Voice* v = mImpl->FindVoice(handle); if (v) v->looping = looping;
        }

        float32 AudioEngine::GetVolume(AudioHandle h) const { const Voice* v=mImpl->FindVoice(h); return v ? v->volume : 0.0f; }
        float32 AudioEngine::GetPitch (AudioHandle h) const { const Voice* v=mImpl->FindVoice(h); return v ? v->pitch  : 0.0f; }
        float32 AudioEngine::GetPan   (AudioHandle h) const { const Voice* v=mImpl->FindVoice(h); return v ? v->pan    : 0.0f; }

        float32 AudioEngine::GetPlaybackPosition(AudioHandle handle) const {
            const Voice* v = mImpl->FindVoice(handle);
            if (!v || !v->sample) return 0.0f;
            return (float32)v->readPos / (float32)v->sample->sampleRate;
        }

        void AudioEngine::SetPlaybackPosition(AudioHandle handle, float32 seconds) {
            Voice* v = mImpl->FindVoice(handle);
            if (v && v->sample) {
                v->readPos    = (usize)(seconds * (float32)v->sample->sampleRate);
                v->subFramePos = 0.0f;
            }
        }

        // ────── Audio 3D ───────────────────────────────────────────────────────

        void AudioEngine::SetSourcePosition(AudioHandle h, float32 x, float32 y, float32 z) {
            Voice* v = mImpl->FindVoice(h);
            if (v) { v->source3d.position[0]=x; v->source3d.position[1]=y; v->source3d.position[2]=z; }
        }
        void AudioEngine::SetSourceVelocity(AudioHandle h, float32 x, float32 y, float32 z) {
            Voice* v = mImpl->FindVoice(h);
            if (v) { v->source3d.velocity[0]=x; v->source3d.velocity[1]=y; v->source3d.velocity[2]=z; }
        }
        void AudioEngine::SetSourceDirection(AudioHandle h, float32 x, float32 y, float32 z) {
            Voice* v = mImpl->FindVoice(h);
            if (v) { v->source3d.direction[0]=x; v->source3d.direction[1]=y; v->source3d.direction[2]=z; }
        }
        void AudioEngine::SetSourcePositional(AudioHandle h, bool p) {
            Voice* v = mImpl->FindVoice(h); if (v) v->source3d.positional = p;
        }
        void AudioEngine::SetSourceMinDistance(AudioHandle h, float32 d) {
            Voice* v = mImpl->FindVoice(h); if (v) v->source3d.minDistance = d;
        }
        void AudioEngine::SetSourceMaxDistance(AudioHandle h, float32 d) {
            Voice* v = mImpl->FindVoice(h); if (v) v->source3d.maxDistance = d;
        }

        void AudioEngine::SetListenerPosition(float32 x, float32 y, float32 z) {
            mImpl->listener.position[0]=x; mImpl->listener.position[1]=y; mImpl->listener.position[2]=z;
        }
        void AudioEngine::SetListenerVelocity(float32 x, float32 y, float32 z) {
            mImpl->listener.velocity[0]=x; mImpl->listener.velocity[1]=y; mImpl->listener.velocity[2]=z;
        }
        void AudioEngine::SetListenerOrientation(float32 fx, float32 fy, float32 fz,
                                                  float32 ux, float32 uy, float32 uz) {
            mImpl->listener.forward[0]=fx; mImpl->listener.forward[1]=fy; mImpl->listener.forward[2]=fz;
            mImpl->listener.up[0]=ux;      mImpl->listener.up[1]=uy;      mImpl->listener.up[2]=uz;
        }

        // ────── Effets ─────────────────────────────────────────────────────────

        bool AudioEngine::AddEffect(AudioHandle handle, IAudioEffect* effect) {
            Voice* v = mImpl->FindVoice(handle);
            if (!v || v->effectCount >= AUDIO_MAX_EFFECTS_PER_VOICE) return false;
            v->effects[v->effectCount++] = effect;
            return true;
        }

        void AudioEngine::RemoveEffect(AudioHandle handle, IAudioEffect* effect) {
            Voice* v = mImpl->FindVoice(handle);
            if (!v) return;
            for (int32 i = 0; i < v->effectCount; ++i) {
                if (v->effects[i] == effect) {
                    for (int32 j = i; j < v->effectCount - 1; ++j) v->effects[j] = v->effects[j+1];
                    v->effects[--v->effectCount] = nullptr;
                    break;
                }
            }
        }

        void AudioEngine::ClearEffects(AudioHandle handle) {
            Voice* v = mImpl->FindVoice(handle);
            if (!v) return;
            for (int32 i = 0; i < v->effectCount; ++i) v->effects[i] = nullptr;
            v->effectCount = 0;
        }

        bool AudioEngine::AddMasterEffect(IAudioEffect* effect) {
            if (mImpl->masterEffectCount >= AUDIO_MAX_MASTER_EFFECTS) return false;
            mImpl->masterEffects[mImpl->masterEffectCount++] = effect;
            return true;
        }

        void AudioEngine::RemoveMasterEffect(IAudioEffect* effect) {
            for (int32 i = 0; i < mImpl->masterEffectCount; ++i) {
                if (mImpl->masterEffects[i] == effect) {
                    for (int32 j = i; j < mImpl->masterEffectCount - 1; ++j)
                        mImpl->masterEffects[j] = mImpl->masterEffects[j+1];
                    mImpl->masterEffects[--mImpl->masterEffectCount] = nullptr;
                    break;
                }
            }
        }

        void AudioEngine::ClearMasterEffects() {
            for (int32 i = 0; i < mImpl->masterEffectCount; ++i) mImpl->masterEffects[i] = nullptr;
            mImpl->masterEffectCount = 0;
        }

        // ────── Global ─────────────────────────────────────────────────────────

        void    AudioEngine::SetMasterVolume(float32 v) { mImpl->masterVolume = Clampf(v, 0.0f, 4.0f); }
        float32 AudioEngine::GetMasterVolume()    const { return mImpl->masterVolume; }

        void AudioEngine::StopAll() {
            for (int32 i = 0; i < mImpl->config.maxVoices; ++i) {
                if (mImpl->voices[i].state != VoiceState::FREE)
                    mImpl->voices[i].state = VoiceState::FINISHED;
            }
        }
        void AudioEngine::PauseAll() {
            for (int32 i = 0; i < mImpl->config.maxVoices; ++i)
                if (mImpl->voices[i].state == VoiceState::PLAYING)
                    mImpl->voices[i].state = VoiceState::PAUSED;
        }
        void AudioEngine::ResumeAll() {
            for (int32 i = 0; i < mImpl->config.maxVoices; ++i)
                if (mImpl->voices[i].state == VoiceState::PAUSED)
                    mImpl->voices[i].state = VoiceState::PLAYING;
        }

        // ────── Informations ──────────────────────────────────────────────────

        AudioBackendType AudioEngine::GetBackendType()  const { return mImpl->config.backend; }
        const char*      AudioEngine::GetBackendName()  const { return mImpl->backend ? mImpl->backend->GetName() : "None"; }
        int32            AudioEngine::GetSampleRate()   const { return mImpl->config.sampleRate; }
        int32            AudioEngine::GetChannels()     const { return mImpl->config.channels; }
        int32            AudioEngine::GetBufferSize()   const { return mImpl->config.bufferSize; }
        float32          AudioEngine::GetLatencyMs()    const { return mImpl->backend ? mImpl->backend->GetLatencyMs() : 0.0f; }
        int32            AudioEngine::GetActiveVoices() const { return mImpl->activeCount; }
        int32            AudioEngine::GetMaxVoices()    const { return mImpl->config.maxVoices; }

        // ====================================================================
        // AUDIO BACKEND FACTORY
        // ====================================================================

        // Registry simple (tableau fixe de paires nom/créateur)
        struct BackendEntry {
            char              name[64];
            AudioBackendFactory::CreatorFunc creator;
        };
        static BackendEntry gBackendRegistry[32];
        static int32        gBackendCount = 0;

        void AudioBackendFactory::Register(const char* name, CreatorFunc creator) {
            if (gBackendCount >= 32) return;
            int32 len = 0;
            while (name[len] && len < 63) { gBackendRegistry[gBackendCount].name[len] = name[len]; ++len; }
            gBackendRegistry[gBackendCount].name[len] = 0;
            gBackendRegistry[gBackendCount].creator   = creator;
            gBackendCount++;
        }

        IAudioBackend* AudioBackendFactory::Create(const char* name) {
            for (int32 i = 0; i < gBackendCount; ++i) {
                const char* a = gBackendRegistry[i].name;
                const char* b = name;
                bool eq = true;
                while (*a && *b) { if (*a != *b) { eq=false; break; } ++a; ++b; }
                if (eq && *a==0 && *b==0 && gBackendRegistry[i].creator) {
                    return gBackendRegistry[i].creator();
                }
            }
            return nullptr;
        }

        IAudioBackend* AudioBackendFactory::CreateDefault() {
            return CreateByType(AudioBackendType::AUTO);
        }

        IAudioBackend* AudioBackendFactory::CreateByType(AudioBackendType type) {
            // Déléguer à la factory enregistrée selon la plateforme
            const char* name = nullptr;
            switch (type) {
                case AudioBackendType::WASAPI:      name = "WASAPI";      break;
                case AudioBackendType::DIRECTSOUND: name = "DirectSound"; break;
                case AudioBackendType::CORE_AUDIO:  name = "CoreAudio";   break;
                case AudioBackendType::ALSA:        name = "ALSA";        break;
                case AudioBackendType::PULSE_AUDIO: name = "PulseAudio";  break;
                case AudioBackendType::AAUDIO:      name = "AAudio";      break;
                case AudioBackendType::NULL_OUTPUT: name = "Null";        break;
                case AudioBackendType::AUTO:
                default:
                    // Sélection automatique par plateforme
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                    name = "WASAPI";
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                    name = "CoreAudio";
#elif defined(NKENTSEU_PLATFORM_ANDROID)
                    name = "AAudio";
#elif defined(NKENTSEU_PLATFORM_LINUX)
                    name = "ALSA";
#else
                    name = "Null";
#endif
                    break;
            }

            IAudioBackend* b = name ? Create(name) : nullptr;
            if (!b) b = Create("Null"); // Fallback
            return b;
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
