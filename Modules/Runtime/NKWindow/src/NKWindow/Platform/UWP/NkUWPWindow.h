#pragma once
// =============================================================================
// NkUWPWindow.h - UWP platform data for NkWindow (data only)
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include "NKWindow/Core/NkSurfaceHint.h"

#include <string>

namespace nkentseu {

    struct NkWindowData {
        // CoreWindow or platform equivalent native handle when available.
        void*       mNativeWindow = nullptr;
        NkString mTitle;
        NkSurfaceHints mAppliedHints{};
        uint32       mWidth        = 0;
        uint32       mHeight       = 0;
        bool        mVisible      = false;
        bool        mFullscreen   = false;
        bool        mExternal     = false;
    };

} // namespace nkentseu
