#pragma once
// =============================================================================
// NkNoopWindow.h - No-op platform data for NkWindow (data only)
// =============================================================================

#include "NKWindow/Core/NkTypes.h"

#include <string>

namespace nkentseu {

struct NkWindowData {
    void*       mNativeHandle = nullptr;
    NkString mTitle;
    NkU32       mWidth        = 0;
    NkU32       mHeight       = 0;
    bool        mVisible      = false;
    bool        mFullscreen   = false;
};

} // namespace nkentseu

