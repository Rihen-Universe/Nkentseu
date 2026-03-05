#pragma once
// =============================================================================
// NkXboxWindow.h - Xbox platform data for NkWindow (data only)
// =============================================================================

#include "NKWindow/Core/NkTypes.h"

#include <string>

namespace nkentseu {

    struct NkWindowData {
        void*       mNativeWindow = nullptr;
        std::string mTitle;
        NkU32       mWidth        = 0;
        NkU32       mHeight       = 0;
        bool        mVisible      = false;
        bool        mFullscreen   = false;
        bool        mOwnsNativeWindow = false;
    };

} // namespace nkentseu
