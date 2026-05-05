#pragma once
// =============================================================================
// NkXCBEventSystem.h — XCB platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB)

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

namespace nkentseu {

    struct NkEventSystemData {
        xcb_connection_t* mConnection    = nullptr;
        int               mDefaultScreen = 0;
        bool              mInitialized   = false;
        // Pour XKB (libxkbcommon-x11)
        struct xkb_context*     mXkbContext = nullptr;
        struct xkb_keymap*      mXkbKeymap  = nullptr;
        struct xkb_state*       mXkbState   = nullptr;

        ~NkEventSystemData() {
            if (mXkbState)   { xkb_state_unref(mXkbState);   mXkbState   = nullptr; }
            if (mXkbKeymap)  { xkb_keymap_unref(mXkbKeymap); mXkbKeymap  = nullptr; }
            if (mXkbContext) { xkb_context_unref(mXkbContext); mXkbContext = nullptr; }
        }
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XCB