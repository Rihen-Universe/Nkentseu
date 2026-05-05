#pragma once
// =============================================================================
// NkXboxWindow.h - Xbox platform data for NkWindow (data only)
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include "NKWindow/Core/NkSurfaceHint.h"

#include <string>

namespace nkentseu {

    struct NkWindowData {
        void*       mNativeWindow = nullptr;
        NkString mTitle;
        NkSurfaceHints mAppliedHints{};
        uint32       mWidth        = 0;
        uint32       mHeight       = 0;
        bool        mVisible      = false;
        bool        mFullscreen   = false;
        bool        mOwnsNativeWindow = false;
        bool        mExternal     = false;
    };

} // namespace nkentseu
