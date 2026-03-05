#pragma once
// =============================================================================
// NkWASMWindow.h — WASM/Emscripten platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkWindowId.h"

#if defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)

#include <string>
#include <vector>

namespace nkentseu {

class NkWindow;
class NkWASMDropTarget;

struct NkWindowData {
    std::string        mCanvasId   = "#canvas";
    NkWASMDropTarget*  mDropTarget = nullptr;
    NkU32              mWidth      = 0;
    NkU32              mHeight     = 0;
    NkU32              mPrevWidth  = 0;
    NkU32              mPrevHeight = 0;
    bool               mVisible    = true;
    bool               mFullscreen = false;
};

NkWindow*              NkWASMFindWindowById(NkWindowId id);
std::vector<NkWindow*> NkWASMGetWindowsSnapshot();
NkWindow*              NkWASMGetLastWindow();
void                   NkWASMRegisterWindow(NkWindow* window);
void                   NkWASMUnregisterWindow(NkWindow* window);
NkWindowId             NkWASMGetActiveWindowId();
void                   NkWASMSetActiveWindowId(NkWindowId id);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WEB || __EMSCRIPTEN__
