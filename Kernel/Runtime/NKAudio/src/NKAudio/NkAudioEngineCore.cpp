// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioEngineCore.cpp
// DESCRIPTION: Implémentation du moteur audio AAA — pool de voix, mixage,
//              audio 3D, gestion des effets, backend abstraction
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Architecture lock-free pour le thread audio (temps réel).
//        Zéro STL. Utilise NkVector et NkAtomic de la fondation.
// -----------------------------------------------------------------------------

#include "NKAudio.h"
#include "NkAudioBackends.h"
#include "NkAudioEffects.h"
#include "NkAudioBus.h"
#include "NkHrtfDataset.h"
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

    /// sinc(x) = sin(pi*x) / (pi*x), avec sinc(0) = 1
    inline nkentseu::float32 NkSinc(nkentseu::float32 x) {
        if (x < 1e-6f && x > -1e-6f) return 1.0f;
        nkentseu::float32 px = 3.14159265358979f * x;
        return ::sinf(px) / px;
    }

    /// Lanczos kernel : sinc(x) * sinc(x/a), a = ordre (typique 4 ou 8).
    /// Retourne 0 si |x| >= a.
    inline nkentseu::float32 NkLanczosKernel(nkentseu::float32 x, nkentseu::float32 a) {
        nkentseu::float32 ax = x < 0.0f ? -x : x;
        if (ax >= a) return 0.0f;
        return NkSinc(x) * NkSinc(x / a);
    }
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
            bool     isFadingOut  = false; // true si Stop(handle, time>0) a ete appele
            float32  subFramePos = 0.0f;   // position sous-frame pour pitch

            // ── Effets DSP ────────────────────────────────────────────────
            IAudioEffect* effects[AUDIO_MAX_EFFECTS_PER_VOICE] = {};
            int32         effectCount = 0;

            // ── Audio 3D ──────────────────────────────────────────────────
            AudioSource3D source3d;

            // ── Routing bus ──────────────────────────────────────────────
            NkAudioBus* bus = nullptr;  ///< Bus de routage (nullptr = direct master)

            // ── Lowpass one-pole state (pour occlusion + air absorption) ─
            float32 lpfState[2] = { 0.0f, 0.0f };  ///< Stereo max

            // ── HRTF convolution state (delay line mono input) ────────────
            // Stocke les N derniers samples mono (downmix L+R) pour la
            // convolution avec les HRIR L/R du dataset.
            static constexpr int32 HRTF_MAX_IR = 512;
            float32 hrtfDelay[HRTF_MAX_IR] = {};
            int32   hrtfDelayPos = 0;

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
                isFadingOut  = false;
                effectCount  = 0;
                bus          = nullptr;
                lpfState[0]  = 0.0f;
                lpfState[1]  = 0.0f;
                ::memset(hrtfDelay, 0, sizeof(hrtfDelay));
                hrtfDelayPos = 0;
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
            LimiterEffect* autoMasterLimiter = nullptr; ///< Cree par engine si config.enableMasterLimiter

            // ── HRTF dataset ──────────────────────────────────────────────
            NkHrtfDataset hrtfDataset; ///< Charge via LoadHrtfDataset()

            // ── Audio Buses hierarchiques (Master -> SFX/Music/Voice/UI) ──
            static constexpr int32 MAX_BUSES = 32;
            NkAudioBus* masterBus           = nullptr;
            NkAudioBus* buses[MAX_BUSES]    = {};
            int32       busCount            = 0;

            NkAudioBus* FindBusByName(const char* name) noexcept {
                if (!name) return nullptr;
                for (int32 i = 0; i < busCount; ++i) {
                    if (buses[i] && ::strcmp(buses[i]->GetName(), name) == 0) {
                        return buses[i];
                    }
                }
                return nullptr;
            }

            NkAudioBus* CreateBusInternal(const char* name, NkAudioBus* parent) noexcept {
                if (busCount >= MAX_BUSES) return nullptr;
                NkAudioBus* b = static_cast<NkAudioBus*>(
                    memory::NkAlloc(sizeof(NkAudioBus), nullptr, alignof(NkAudioBus)));
                if (!b) return nullptr;
                new (b) NkAudioBus(name, parent);
                buses[busCount++] = b;
                return b;
            }

            void DestroyBuses() noexcept {
                for (int32 i = 0; i < busCount; ++i) {
                    if (buses[i]) {
                        buses[i]->~NkAudioBus();
                        memory::NkFree(buses[i], nullptr);
                        buses[i] = nullptr;
                    }
                }
                busCount = 0;
                masterBus = nullptr;
            }

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
                    // Volume effectif = voice * bus (hierarchique) * masterVolume * distance
                    float32 busGain = voice.bus ? voice.bus->GetEffectiveVolume() : 1.0f;
                    float32 finalVol = voice.volume * busGain * masterVolume * spatial.distanceAttenuation;
                    float32 finalPitch = voice.pitch * spatial.dopplerPitch;

                    // ── Occlusion + Air absorption ─────────────────────────
                    // Atténuation occlusion (max -70% volume si occlusion=1)
                    float32 occl = voice.source3d.occlusion;
                    if (occl < 0.0f) occl = 0.0f;
                    if (occl > 1.0f) occl = 1.0f;
                    finalVol *= (1.0f - occl * 0.7f);

                    // Cutoff lowpass one-pole : combine occlusion + air absorption.
                    // - Sans occl ni air abs : cutoff = sampleRate/2 (pas de filtre)
                    // - Occlusion=1 : cutoff ~10% nyquist (etouffe)
                    // - Air abs + distance > 50m : cutoff progressif vers 30% nyquist
                    float32 nyquist = (float32)config.sampleRate * 0.5f;
                    float32 cutoffOccl = nyquist * (1.0f - occl * 0.9f);
                    float32 cutoffAir = nyquist;
                    if (voice.source3d.airAbsorption) {
                        // Cherche distance via vecteur listener
                        float32 dx = voice.source3d.position[0] - listener.position[0];
                        float32 dy = voice.source3d.position[1] - listener.position[1];
                        float32 dz = voice.source3d.position[2] - listener.position[2];
                        float32 dist = NkSqrtf(dx*dx + dy*dy + dz*dz);
                        // Air abs : perd progressivement les hautes freq au-dela de 10m
                        float32 farRatio = Clampf((dist - 10.0f) / 100.0f, 0.0f, 0.7f);
                        cutoffAir = nyquist * (1.0f - farRatio);
                    }
                    float32 cutoff = cutoffOccl < cutoffAir ? cutoffOccl : cutoffAir;
                    // One-pole coeff : a = cutoff / sampleRate (approx 0..0.5)
                    float32 lpfA = Clampf(cutoff / (float32)config.sampleRate, 0.001f, 0.5f);
                    bool needLpf = (occl > 0.001f) || voice.source3d.airAbsorption;

                    // Remplir le mixBuffer pour cette voix
                    memset(mixBuffer, 0, (usize)frameCount * (usize)channels * sizeof(float32));

                    if (voice.isProcedural && voice.proceduralCallback) {
                        voice.proceduralCallback(mixBuffer, frameCount, channels);

                    } else if (voice.sample && voice.sample->IsValid()) {
                        const AudioSample& s    = *voice.sample;
                        int32              sCh  = s.channels;

                        // Determine la qualite de resampling (LINEAR / SINC_4 / SINC_8)
                        ResamplingQuality rq = config.resamplingQuality;
                        // Determine l'ordre Lanczos (a) et le nombre de taps
                        int32 sincOrder = (rq == ResamplingQuality::SINC_8) ? 8 :
                                           (rq == ResamplingQuality::SINC_4) ? 4 : 0;

                        for (int32 f = 0; f < frameCount; ++f) {
                            // Lecture avec pitch
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

                            // Bypass : si frac == 0 et pitch == 1.0, pas besoin
                            // d'interpoler (lecture directe sample).
                            bool bypass = (frac < 1e-6f) && (NkFabsf(finalPitch - 1.0f) < 1e-6f);

                            if (bypass || sincOrder == 0) {
                                // ── Mode LINEAR (default) ou bypass ──
                                for (int32 c = 0; c < channels; ++c) {
                                    int32 srcC = (c < sCh) ? c : sCh - 1;
                                    float32 a = s.data[srcFrame     * (usize)sCh + (usize)srcC];
                                    float32 b = (srcFrame + 1 < s.frameCount)
                                              ? s.data[(srcFrame+1) * (usize)sCh + (usize)srcC]
                                              : 0.0f;
                                    mixBuffer[f * channels + c] = a + (b - a) * frac;
                                }
                            } else {
                                // ── Mode SINC_4 / SINC_8 (Lanczos windowed sinc) ──
                                // Convolution avec kernel Lanczos centre sur frac.
                                // tap n in [-sincOrder+1 .. sincOrder] contribue avec
                                // kernel(n - frac, sincOrder).
                                float32 aF = (float32)sincOrder;
                                for (int32 c = 0; c < channels; ++c) {
                                    int32 srcC = (c < sCh) ? c : sCh - 1;
                                    float32 acc = 0.0f;
                                    // Sum from n = -sincOrder+1 to n = sincOrder
                                    for (int32 n = -sincOrder + 1; n <= sincOrder; ++n) {
                                        // Index source clamped aux bornes
                                        nk_int64 idx = (nk_int64)srcFrame + (nk_int64)n;
                                        if (idx < 0 || idx >= (nk_int64)s.frameCount) continue;
                                        float32 sample = s.data[(usize)idx * (usize)sCh + (usize)srcC];
                                        float32 w = NkLanczosKernel((float32)n - frac, aF);
                                        acc += sample * w;
                                    }
                                    mixBuffer[f * channels + c] = acc;
                                }
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

                    // ── HRTF setup (si active + dataset charge) ──────────
                    // HRTF replace le panning par convolution avec HRIR L/R.
                    // Recupere la paire HRIR pour l'azimut+elevation source.
                    bool useHrtfHere = voice.source3d.useHrtf
                                    && voice.source3d.positional
                                    && hrtfDataset.IsLoaded()
                                    && channels >= 2;
                    NkHrirPair hrir{};
                    if (useHrtfHere) {
                        // Calcul azimut/elevation depuis vecteur source->listener
                        float32 dx = voice.source3d.position[0] - listener.position[0];
                        float32 dy = voice.source3d.position[1] - listener.position[1];
                        float32 dz = voice.source3d.position[2] - listener.position[2];
                        // Azimut horizontal (XZ plane, atan2(x, -z) : 0=devant, 90=droite)
                        float32 azRad = NkAtan2f(dx, -dz);
                        float32 azDeg = azRad * (180.0f / 3.14159265f);
                        if (azDeg < 0.0f) azDeg += 360.0f;
                        // Elevation (angle vs plan horizontal)
                        float32 distXZ = NkSqrtf(dx*dx + dz*dz);
                        float32 elRad  = NkAtan2f(dy, distXZ);
                        float32 elDeg  = elRad * (180.0f / 3.14159265f);
                        hrir = hrtfDataset.GetHRIR(azDeg, elDeg);
                        if (hrir.length == 0) useHrtfHere = false; // dataset invalide
                    }

                    // Gain spatial + panning + fade (in OU out, mutuellement exclusif)
                    for (int32 f = 0; f < frameCount; ++f) {
                        float32 fadeGain = 1.0f;
                        if (voice.isFadingOut && voice.fadeOutFrames > 0.0f) {
                            // Fade out : 1 -> 0 sur fadeOutFrames
                            fadeGain = 1.0f - Clampf(voice.fadeProgress / voice.fadeOutFrames, 0.0f, 1.0f);
                            voice.fadeProgress++;
                            if (voice.fadeProgress >= voice.fadeOutFrames) {
                                // Fade out termine : voice -> FINISHED (sera cleanup ensuite)
                                voice.state = VoiceState::FINISHED;
                            }
                        } else if (voice.fadeInFrames > 0.0f) {
                            // Fade in : 0 -> 1 sur fadeInFrames
                            fadeGain = Clampf(voice.fadeProgress / voice.fadeInFrames, 0.0f, 1.0f);
                            voice.fadeProgress++;
                        }

                        if (useHrtfHere) {
                            // ── HRTF : convolution naive avec HRIR L/R ──
                            // Downmix mono = (L+R)/2 puis convoluer avec L et R IR.
                            float32 mono = mixBuffer[f * channels + 0];
                            if (channels >= 2) mono = 0.5f * (mono + mixBuffer[f * channels + 1]);
                            // Apply lowpass eventuel (occlusion + air abs) AVANT convolution
                            if (needLpf) {
                                float32 y = voice.lpfState[0] + lpfA * (mono - voice.lpfState[0]);
                                voice.lpfState[0] = y;
                                mono = y;
                            }
                            // Insert mono in delay line
                            voice.hrtfDelay[voice.hrtfDelayPos] = mono;
                            // Convolve : sum_{i=0..irLen-1} delay[pos-i] * hrir[i]
                            float32 outL = 0.0f, outR = 0.0f;
                            int32 irLen = hrir.length;
                            if (irLen > Voice::HRTF_MAX_IR) irLen = Voice::HRTF_MAX_IR;
                            for (int32 i = 0; i < irLen; ++i) {
                                int32 dp = voice.hrtfDelayPos - i;
                                while (dp < 0) dp += Voice::HRTF_MAX_IR;
                                float32 s = voice.hrtfDelay[dp];
                                outL += s * hrir.leftIR[i];
                                outR += s * hrir.rightIR[i];
                            }
                            voice.hrtfDelayPos = (voice.hrtfDelayPos + 1) % Voice::HRTF_MAX_IR;
                            // Ecrire dans outputBuffer (forcement >= 2 canaux)
                            outputBuffer[f * channels + 0] += outL * finalVol * fadeGain;
                            outputBuffer[f * channels + 1] += outR * finalVol * fadeGain;
                        } else {
                            // ── Panning standard ──
                            for (int32 c = 0; c < channels; ++c) {
                                float32 chGain = (c == 0) ? spatial.leftGain : spatial.rightGain;
                                // Pan 2D (override si non-positional)
                                if (!voice.source3d.positional) {
                                    chGain = (c == 0) ? Clampf(1.0f - voice.pan, 0.0f, 1.0f)
                                                      : Clampf(1.0f + voice.pan, 0.0f, 1.0f);
                                }
                                float32 sample = mixBuffer[f * channels + c];
                                // Lowpass one-pole : y[n] = y[n-1] + a*(x[n] - y[n-1])
                                if (needLpf && c < 2) {
                                    float32 y = voice.lpfState[c] + lpfA * (sample - voice.lpfState[c]);
                                    voice.lpfState[c] = y;
                                    sample = y;
                                }
                                outputBuffer[f * channels + c] +=
                                    sample * finalVol * chGain * fadeGain;
                            }
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
                        // Decrement compteur du bus pour sidechain tracking
                        if (voices[v].bus) {
                            voices[v].bus->DecrementActiveVoices();
                            voices[v].bus = nullptr;
                        }
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
            // Default-init (pas value-init) : evite l'instanciation des
            // explicit constructors par defaut sur les NkFunction nested
            // dans le tableau de Voice. Tous les autres membres ont des
            // initializers in-class qui s'appliquent quand meme.
            mImpl = new Impl;
        }

        AudioEngine::~AudioEngine() {
            if (mImpl->initialized) Shutdown();
            delete mImpl;
        }

        bool AudioEngine::Initialize(const AudioEngineConfig& config) {
            if (mImpl->initialized) return true;

            // Force l'enregistrement des backends natifs (necessaire en lib
            // statique : le linker stripe sinon les static initializers).
            EnsureBackendsRegistered();

            mImpl->config = config;

            // Limiter maxVoices au pool statique
            if (mImpl->config.maxVoices > AUDIO_MAX_VOICES) {
                mImpl->config.maxVoices = AUDIO_MAX_VOICES;
            }

            // Allouer le buffer de mix
            mImpl->mixBufferSize = config.bufferSize * config.channels;
            // Allocation unifie via NKMemory : config.allocator peut etre
            // nullptr (defaut global). Buffer du mixeur audio temps reel.
            mImpl->mixBuffer = (float32*)memory::NkAlloc(
                (usize)mImpl->mixBufferSize * sizeof(float32),
                config.allocator, sizeof(float32));

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

            // Cree la hierarchie de buses standard :
            //   Master (root)
            //     ├── SFX     (default pour voices)
            //     ├── Music   (musique de fond)
            //     ├── Voice   (dialogues)
            //     └── UI      (interface)
            mImpl->masterBus = mImpl->CreateBusInternal("Master", nullptr);
            if (mImpl->masterBus) {
                mImpl->CreateBusInternal("SFX",   mImpl->masterBus);
                mImpl->CreateBusInternal("Music", mImpl->masterBus);
                mImpl->CreateBusInternal("Voice", mImpl->masterBus);
                mImpl->CreateBusInternal("UI",    mImpl->masterBus);
            }

            // Limiter brick-wall auto sur le bus Master (anti-clip).
            // Ajoute aux masterEffects (applique sur le mix final).
            if (config.enableMasterLimiter && mImpl->masterEffectCount < AUDIO_MAX_MASTER_EFFECTS) {
                LimiterEffect::Params lp;
                lp.thresholdDb = config.masterLimiterThresholdDb;
                mImpl->autoMasterLimiter = new LimiterEffect(lp, config.sampleRate);
                mImpl->masterEffects[mImpl->masterEffectCount++] = mImpl->autoMasterLimiter;
            }

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

            // Libere l'auto-limiter master (cree par Initialize)
            if (mImpl->autoMasterLimiter) {
                // Retirer du tableau masterEffects pour eviter use-after-free
                for (int32 i = 0; i < mImpl->masterEffectCount; ++i) {
                    if (mImpl->masterEffects[i] == mImpl->autoMasterLimiter) {
                        for (int32 j = i; j < mImpl->masterEffectCount - 1; ++j) {
                            mImpl->masterEffects[j] = mImpl->masterEffects[j+1];
                        }
                        mImpl->masterEffects[--mImpl->masterEffectCount] = nullptr;
                        break;
                    }
                }
                delete mImpl->autoMasterLimiter;
                mImpl->autoMasterLimiter = nullptr;
            }

            // Libere les buses (en ordre inverse : enfants avant parent OK car
            // ownership = engine, pas hierarchie destruction).
            mImpl->DestroyBuses();

            memory::NkFree(mImpl->mixBuffer, mImpl->config.allocator);
            mImpl->mixBuffer = nullptr;

            mImpl->initialized = false;
        }

        // ────── Audio Buses hierarchiques ────────────────────────────────────

        NkAudioBus* AudioEngine::GetMasterBus() {
            return mImpl ? mImpl->masterBus : nullptr;
        }

        NkAudioBus* AudioEngine::GetBus(const char* name) {
            if (!mImpl) return nullptr;
            return mImpl->FindBusByName(name);
        }

        NkAudioBus* AudioEngine::GetOrCreateBus(const char* name, NkAudioBus* parent) {
            if (!mImpl || !name) return nullptr;
            NkAudioBus* existing = mImpl->FindBusByName(name);
            if (existing) return existing;
            if (!parent) parent = mImpl->masterBus;
            return mImpl->CreateBusInternal(name, parent);
        }

        // ────── Crossfade musique ────────────────────────────────────────────

        AudioHandle AudioEngine::PlayMusicCrossfade(const AudioSample& newMusic,
                                                     float32 fadeTime,
                                                     const VoiceParams& params) {
            if (!mImpl || !mImpl->initialized || !newMusic.IsValid()) {
                return AUDIO_HANDLE_INVALID;
            }
            // 1. Fade-out toutes les voix actuellement sur le bus "Music"
            NkAudioBus* musicBus = mImpl->FindBusByName("Music");
            if (musicBus) {
                for (int32 i = 0; i < mImpl->config.maxVoices; ++i) {
                    Voice& v = mImpl->voices[i];
                    if (v.state == VoiceState::PLAYING && v.bus == musicBus && !v.isFadingOut) {
                        v.isFadingOut   = true;
                        v.fadeOutFrames = fadeTime * (float32)mImpl->config.sampleRate;
                        v.fadeProgress  = 0.0f;
                    }
                }
            }
            // 2. Joue la nouvelle musique sur le bus "Music" avec fade-in
            VoiceParams p = params;
            p.bus        = "Music";
            p.fadeInTime = fadeTime;
            return Play(newMusic, p);
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
            v->bus           = mImpl->FindBusByName(params.bus); // nullptr-safe (defaut Master)
            if (v->bus) v->bus->IncrementActiveVoices();          // pour sidechain tracking

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
            v->bus                 = mImpl->FindBusByName(params.bus);
            if (v->bus) v->bus->IncrementActiveVoices();
            v->volume              = params.volume;
            v->pitch               = params.pitch;
            v->pan                 = params.pan;
            v->source3d            = params.source3d;

            return AudioHandle{ v->id };
        }

        // ────── Contrôle voix ─────────────────────────────────────────────────

        void AudioEngine::Stop(AudioHandle handle, float32 fadeOutTime) {
            Voice* v = mImpl->FindVoice(handle);
            if (!v) return;
            if (fadeOutTime > 0.0f && !v->isFadingOut) {
                // Demarre un fade-out : le mixer attenuera 1 -> 0 sur fadeOutFrames
                // puis passera la voix en FINISHED automatiquement.
                v->isFadingOut   = true;
                v->fadeOutFrames = fadeOutTime * (float32)mImpl->config.sampleRate;
                v->fadeProgress  = 0.0f;
            } else {
                // Stop immediat
                v->state = VoiceState::FINISHED;
            }
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
        void AudioEngine::SetSourceOcclusion(AudioHandle h, float32 occlusion) {
            Voice* v = mImpl->FindVoice(h);
            if (v) {
                if (occlusion < 0.0f) occlusion = 0.0f;
                if (occlusion > 1.0f) occlusion = 1.0f;
                v->source3d.occlusion = occlusion;
            }
        }
        void AudioEngine::SetSourceAirAbsorption(AudioHandle h, bool enabled) {
            Voice* v = mImpl->FindVoice(h); if (v) v->source3d.airAbsorption = enabled;
        }
        void AudioEngine::SetSourceHRTF(AudioHandle h, bool enabled) {
            Voice* v = mImpl->FindVoice(h); if (v) v->source3d.useHrtf = enabled;
        }

        bool AudioEngine::LoadHrtfDataset(const char* path) {
            if (!mImpl) return false;
            return mImpl->hrtfDataset.LoadFromFile(path, mImpl->config.allocator);
        }

        bool AudioEngine::GenerateSyntheticHrtf(int32 irLength,
                                                  int32 nAzimuths,
                                                  int32 nElevations) {
            if (!mImpl) return false;
            return mImpl->hrtfDataset.CreateSynthetic(mImpl->config.sampleRate,
                                                       irLength, nAzimuths,
                                                       nElevations,
                                                       mImpl->config.allocator);
        }

        void AudioEngine::UnloadHrtfDataset() {
            if (mImpl) mImpl->hrtfDataset.Unload();
        }

        bool AudioEngine::IsHrtfLoaded() const {
            return mImpl && mImpl->hrtfDataset.IsLoaded();
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

        void AudioEngine::RenderToBuffer(float32* outputBuffer, int32 frameCount, int32 channels) {
            if (!mImpl || !outputBuffer || frameCount <= 0 || channels <= 0) return;
            mImpl->AudioCallback(outputBuffer, frameCount, channels);
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
                case AudioBackendType::OPEN_SL_ES:  name = "OpenSLES";    break;
                case AudioBackendType::WEB_AUDIO:   name = "WebAudio";    break;
                case AudioBackendType::NULL_OUTPUT: name = "Null";        break;
                case AudioBackendType::AUTO:
                default:
                    // Sélection automatique par plateforme
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                    name = "WASAPI";
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                    name = "CoreAudio";
#elif defined(NKENTSEU_PLATFORM_ANDROID)
                    // Android : on essaie AAudio (latence ~5ms, API 26+).
                    // Si Initialize echoue (Android 24-25), AudioEngine
                    // retentera en fallback. CreateByType ne sait pas
                    // tester Initialize ici, mais l'engine s'en charge.
                    name = "AAudio";
#elif defined(NKENTSEU_PLATFORM_LINUX)
                    name = "ALSA";
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                    name = "WebAudio";
#else
                    name = "Null";
#endif
                    break;
            }

            IAudioBackend* b = name ? Create(name) : nullptr;
#if defined(NKENTSEU_PLATFORM_ANDROID)
            // Sur Android, si AAudio n'est pas dispo (API <26), fallback
            // automatique sur OpenSL ES (dispo depuis API 9).
            if (!b && type == AudioBackendType::AUTO) {
                b = Create("OpenSLES");
            }
#endif
            if (!b) b = Create("Null"); // Fallback ultime
            return b;
        }

    } // namespace audio
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
