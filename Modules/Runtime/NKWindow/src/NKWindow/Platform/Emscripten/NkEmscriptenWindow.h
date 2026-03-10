#pragma once
// =============================================================================
// NkEmscriptenWindow.h â€” WASM/Emscripten platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkWindowId.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    class NkWindow;
    class NkEmscriptenDropTarget;

    struct NkWindowData {
        NkString        mCanvasId   = "#canvas";
        NkEmscriptenDropTarget*  mDropTarget = nullptr;
        NkU32              mWidth      = 0;
        NkU32              mHeight     = 0;
        NkU32              mPrevWidth  = 0;
        NkU32              mPrevHeight = 0;
        bool               mVisible    = true;
        bool               mFullscreen = false;
    };

    NkWindow*               NkEmscriptenFindWindowById(NkWindowId id);
    NkVector<NkWindow*>     NkEmscriptenGetWindowsSnapshot();
    NkWindow*               NkEmscriptenGetLastWindow();
    void                    NkEmscriptenRegisterWindow(NkWindow* window);
    void                    NkEmscriptenUnregisterWindow(NkWindow* window);
    NkWindowId              NkEmscriptenGetActiveWindowId();
    void                    NkEmscriptenSetActiveWindowId(NkWindowId id);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
