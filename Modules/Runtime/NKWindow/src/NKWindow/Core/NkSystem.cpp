// =============================================================================
// NkSystem.cpp â€” cycle de vie global + registre des fenÃªtres
// =============================================================================

#include "NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NkWindow.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#   include <ole2.h>
#   pragma comment(lib, "ole32.lib")
#endif

namespace nkentseu {

    NkSystem& NkSystem::Instance() {
        static NkSystem sInstance;
        return sInstance;
    }

    bool NkSystem::Initialise(const NkAppData& data) {
        if (mInitialised) return true;
        mAppData = data;

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
        // Point 6 : OleInitialize une seule fois ici, avant toute crÃ©ation
        // de fenÃªtre ou de NkWin32DropTarget. Tous les DropTarget crÃ©Ã©s
        // ultÃ©rieurement peuvent appeler RegisterDragDrop directement sans
        // rappeler OleInitialize.
        if (!mOleInitialised) {
            HRESULT hr = OleInitialize(nullptr);
            // S_FALSE signifie que OLE Ã©tait dÃ©jÃ  initialisÃ© sur ce thread â€” acceptable.
            mOleInitialised = SUCCEEDED(hr);
        }
#endif

        // NkEventSystem est possÃ©dÃ© ici â€” on appelle Init() directement
        if (!mEventSystem.Init()) return false;

        mGamepadSystem.Init();
        mInitialised = true;
        return true;
    }

    void NkSystem::Close() {
        if (!mInitialised) return;

        mGamepadSystem.Shutdown();
        mEventSystem.Shutdown();

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
        // Point 6 : OleUninitialize symÃ©trique Ã  OleInitialize
        if (mOleInitialised) {
            OleUninitialize();
            mOleInitialised = false;
        }
#endif

        mInitialised = false;
    }

    // -------------------------------------------------------------------------
    // Window registry
    // -------------------------------------------------------------------------

    NkWindowId NkSystem::RegisterWindow(NkWindow* win) {
        if (!win) return NK_INVALID_WINDOW_ID;
        NkWindowId id = mNextWindowId++;
        mWindows[id]  = win;
        return id;
    }

    void NkSystem::UnregisterWindow(NkWindowId id) {
        auto it = mWindows.find(id);
        if (it != mWindows.end()) {
            mWindows.erase(it);
            // Supprime le callback associÃ© Ã  cette fenÃªtre dans NkEventSystem
            mEventSystem.RemoveWindowCallback(id);
        }
    }

    NkWindow* NkSystem::GetWindow(NkWindowId id) const {
        auto it = mWindows.find(id);
        return (it != mWindows.end()) ? it->second : nullptr;
    }

    NkWindow* NkSystem::GetWindowAt(NkU32 index) const {
        if (index >= mWindows.size()) return nullptr;
        auto it = mWindows.begin();
        std::advance(it, index);
        return it->second;
    }

} // namespace nkentseu

