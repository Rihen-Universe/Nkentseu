#pragma once
// =============================================================================
// NKECS/Components/Audio/NkAudioComponents.h
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace ecs {

        struct NkAudioSourceComponent {
            NkString   clipPath;
            nk_uint64  clipHandle = 0;
            float32    volume      = 1.f;
            float32    pitch       = 1.f;
            float32    minDistance = 1.f;
            float32    maxDistance = 30.f;
            bool       playOnStart = false;
            bool       loop        = false;
            bool       spatialize  = true;
            bool       playing     = false;
        };
        NK_COMPONENT(NkAudioSourceComponent)

        struct NkAudioListenerComponent {
            float32 volume = 1.f;
        };
        NK_COMPONENT(NkAudioListenerComponent)

    } // namespace ecs
} // namespace nkentseu
