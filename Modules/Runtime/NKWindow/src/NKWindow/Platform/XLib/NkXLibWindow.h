#pragma once
// =============================================================================
// NkXLibWindow.h - XLib platform data for NkWindow (data only, no methods)
//
// Pattern aligned with Win32/XCB:
// - Native data lives in NkWindow::mData.
// - Backend window registry is hidden in .cpp and exposed via helper functions.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include <X11/Xlib.h>

namespace nkentseu {

    class NkWindow;
    class NkXLibDropTarget;

    struct NkWindowData {
        ::Display* mDisplay        = nullptr;
        ::Window   mXid            = 0;
        ::Atom     mWmDeleteWindow = 0;
        int        mScreen         = 0;
        NkXLibDropTarget* mDropTarget = nullptr;
        NkSurfaceHints mAppliedHints;  // ← ajout
    };

    // Backend registry accessors (map lives as static in NkXLibWindow.cpp)
    NkWindow*  NkXLibFindWindow(::Window xid);
    void       NkXLibRegisterWindow(::Window xid, NkWindow* win);
    void       NkXLibUnregisterWindow(::Window xid);
    NkWindow*  NkXLibGetLastWindow();
    ::Display* NkXLibGetDisplay();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
