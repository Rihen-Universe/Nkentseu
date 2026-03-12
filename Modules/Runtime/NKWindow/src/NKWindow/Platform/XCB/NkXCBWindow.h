#pragma once
// =============================================================================
// NkXCBWindow.h — XCB platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkSurfaceHint.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB)

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/randr.h>
#include <xcb/xfixes.h>
// xcb/xkb.h uses 'explicit' as a struct member name, which is a C++ keyword.
// Temporarily rename it to avoid a compilation error.
#define explicit explicit_xkb_workaround
#include <xcb/xkb.h>
#undef explicit
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <unordered_map>

namespace nkentseu {

    class NkWindow;
    class NkXCBDropTarget;

    struct NkWindowData {
        xcb_connection_t* mConnection   = nullptr;
        xcb_screen_t*     mScreen       = nullptr;
        xcb_window_t      mWindow       = 0;
        xcb_window_t      mParentWindow = 0;
        NkSurfaceHints    mAppliedHints{};
        xcb_colormap_t    mColormap     = 0;
        xcb_cursor_t      mHiddenCursor = 0;
        bool              mExternal     = false;
        NkXCBDropTarget*  mDropTarget   = nullptr;

        // Pour le suivi des événements
        bool              mIsMapped      = false;
        bool              mMouseInWindow = false;
        bool              mRawMouseMotion = false;
        // etc.
    };

    // Registre backend XCB — accès via fonctions, la map est static dans le .cpp
    NkWindow* NkXCBFindWindow(xcb_window_t xid);
    void      NkXCBRegisterWindow(xcb_window_t xid, NkWindow* win);
    void      NkXCBUnregisterWindow(xcb_window_t xid);
    xcb_connection_t* NkXCBGetConnection();
    xcb_screen_t*     NkXCBGetScreen();
    xcb_atom_t        NkXCBGetWmDeleteWindowAtom();
    xcb_atom_t        NkXCBGetWmProtocolsAtom();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XCB
