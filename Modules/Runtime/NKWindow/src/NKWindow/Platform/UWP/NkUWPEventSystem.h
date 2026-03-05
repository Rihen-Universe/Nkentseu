#pragma once
// =============================================================================
// NkUWPEventSystem.h - UWP platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

namespace nkentseu {

    struct NkEventSystemData {
        bool mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_UWP
