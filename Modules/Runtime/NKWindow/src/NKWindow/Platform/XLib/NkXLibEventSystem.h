#pragma once
// =============================================================================
// NkXLibEventSystem.h - XLib platform data for NkEventSystem
//
// Pattern aligned with Win32 header:
// - platform-specific event state is owned by NkEventSystem::mData.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include "NKWindow/Core/NkTypes.h"
#include <X11/Xlib.h>

namespace nkentseu {

    struct NkEventSystemData {
        ::Display* mDisplay    = nullptr;
        NkI32      mPrevMouseX = 0;
        NkI32      mPrevMouseY = 0;
        bool       mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
