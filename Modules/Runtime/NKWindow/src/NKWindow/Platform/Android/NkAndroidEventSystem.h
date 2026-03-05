#pragma once
// =============================================================================
// NkAndroidEventSystem.h — Android platform data for NkEventSystem (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

struct android_app;

namespace nkentseu {

    struct NkEventSystemData {
        struct android_app* mAndroidApp = nullptr;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID
