#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Audio/NkAudio.h
// DESCRIPTION: Composants audio.
//   NkAudioSource   — source audio 3D/2D
//   NkAudioListener — oreille du joueur (max 1 par scène)
//   NkAudioMixer    — bus de mixage
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"

namespace nkentseu { namespace ecs {

// =============================================================================
// NkAudioSource
// =============================================================================

enum class NkAudioRolloff : uint8 { Logarithmic = 0, Linear = 1, Custom = 2 };

struct NkAudioSource {
    uint32  clipId          = 0;        // handle vers le clip audio
    char    clipPath[256]   = {};
    float32 volume          = 1.f;      // [0..1]
    float32 pitch           = 1.f;      // [0.1..3.0]
    float32 pan             = 0.f;      // 2D: [-1..1]
    bool    loop            = false;
    bool    playOnAwake     = true;
    bool    is3D            = true;     // false = 2D (pas d'atténuation spatiale)
    float32 minDistance     = 1.f;      // distance sans atténuation
    float32 maxDistance     = 500.f;
    float32 spatialBlend    = 1.f;      // 0=2D, 1=3D (blendable)
    float32 dopplerLevel    = 1.f;
    float32 spread          = 0.f;     // degrés (0 = mono, 360 = omni)
    NkAudioRolloff rolloff  = NkAudioRolloff::Logarithmic;

    // État runtime
    bool    isPlaying       = false;
    float32 playbackTime    = 0.f;
    uint32  audioSourceHandle = 0;      // handle moteur audio

    // Bus de mixage
    char    mixerGroup[64] = "Master";

    // Effets
    bool    enableReverb    = false;
    float32 reverbWet       = 0.3f;
    bool    enableLowPass   = false;
    float32 lowPassCutoff   = 22000.f;  // Hz
    bool    mute            = false;

    void Play()    noexcept { isPlaying = true;  }
    void Stop()    noexcept { isPlaying = false; playbackTime = 0.f; }
    void Pause()   noexcept { isPlaying = false; }
    void Resume()  noexcept { isPlaying = true;  }
};
NK_COMPONENT(NkAudioSource)

// =============================================================================
// NkAudioListener — oreille (doit être unique dans la scène)
// =============================================================================

struct NkAudioListener {
    float32 volume      = 1.f;
    bool    enabled     = true;
};
NK_COMPONENT(NkAudioListener)

// =============================================================================
// NkAudioReverbZone — zone de réverbération
// =============================================================================

struct NkAudioReverbZone {
    float32 minDistance = 10.f;
    float32 maxDistance = 50.f;
    // Paramètres EAX standard
    float32 room        = -1000.f;
    float32 roomHF      = -100.f;
    float32 decayTime   = 1.49f;
    float32 decayHFRatio = 0.83f;
    float32 reflections = -2602.f;
    float32 reflectionsDelay = 0.007f;
    float32 reverb      = 200.f;
    float32 reverbDelay = 0.011f;
    float32 hFReference = 5000.f;
    float32 lFReference = 250.f;
    bool    enabled     = true;
};
NK_COMPONENT(NkAudioReverbZone)

}} // namespace nkentseu::ecs
