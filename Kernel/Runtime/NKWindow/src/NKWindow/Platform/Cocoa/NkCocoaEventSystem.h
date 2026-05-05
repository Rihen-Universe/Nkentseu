#pragma once
// =============================================================================
// NkCocoaEventSystem.h - Cocoa platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

namespace nkentseu {

    struct NkEventSystemData {
        bool mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS

