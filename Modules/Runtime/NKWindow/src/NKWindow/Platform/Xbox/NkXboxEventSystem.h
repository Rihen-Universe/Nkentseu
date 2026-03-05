#pragma once
// =============================================================================
// NkXboxEventSystem.h - Xbox platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

namespace nkentseu {

    struct NkEventSystemData {
        bool mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_XBOX
