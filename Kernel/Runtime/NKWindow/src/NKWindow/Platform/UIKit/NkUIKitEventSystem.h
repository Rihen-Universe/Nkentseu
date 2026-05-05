#pragma once
// =============================================================================
// NkUIKitEventSystem.h - UIKit platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

namespace nkentseu {

    struct NkEventSystemData {
        bool mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS

