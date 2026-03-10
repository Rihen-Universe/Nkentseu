#pragma once
// =============================================================================
// NkWin32EventSystem.h — Win32 platform data for NkEventSystem (data only)
// Window lookup uses GWLP_USERDATA (O(1)) — no HWND→ID map needed.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Core/NkTypes.h"

namespace nkentseu {

    struct NkEventSystemData {
        bool   mRawInputRegistered = false;
        int32  mPrevMouseX         = 0;
        int32  mPrevMouseY         = 0;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
