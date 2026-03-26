// =============================================================================
// NkSystem.cpp â€” cycle de vie global + registre des fenÃªtres
// =============================================================================

#include "NkSystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkEventDispatcher.h"
#include "NkWindow.h"

// ---------------------------------------------------------------------------
// Selection du backend gamepad par plateforme
// (deplace depuis NkGamepadSystem.cpp pour isoler NKEvent des includes NKWindow)
// ---------------------------------------------------------------------------

#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
#   include "NKWindow/Platform/Noop/NkNoopGamepad.h"
    using PlatformGamepad = nkentseu::NkNoopGamepad;

#elif defined(NKENTSEU_PLATFORM_UWP)
#   include "NKWindow/Platform/UWP/NkUWPGamepad.h"
    using PlatformGamepad = nkentseu::NkUWPGamepad;

#elif defined(NKENTSEU_PLATFORM_XBOX)
#   include "NKWindow/Platform/Xbox/NkXboxGamepad.h"
    using PlatformGamepad = nkentseu::NkXboxGamepad;

#elif defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
#   include "NKWindow/Platform/Win32/NkWin32Gamepad.h"
    using PlatformGamepad = nkentseu::NkWin32Gamepad;

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include "NKWindow/Platform/Cocoa/NkCocoaGamepad.h"
    using PlatformGamepad = nkentseu::NkCocoaGamepad;

#elif defined(NKENTSEU_PLATFORM_IOS)
#   include "NKWindow/Platform/UIKit/NkUIKitGamepad.h"
    using PlatformGamepad = nkentseu::NkUIKitGamepad;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include "NKWindow/Platform/Android/NkAndroidGamepad.h"
    using PlatformGamepad = nkentseu::NkAndroidGamepad;

#elif defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_XLIB) \
   || defined(NKENTSEU_WINDOWING_WAYLAND)
#   include "NKWindow/Platform/Linux/NkLinuxGamepadBackend.h"
    using PlatformGamepad = nkentseu::NkLinuxGamepad;

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include "NKWindow/Platform/Emscripten/NkEmscriptenGamepad.h"
    using PlatformGamepad = nkentseu::NkEmscriptenGamepad;

#else
#   include "NKWindow/Platform/Noop/NkNoopGamepad.h"
    using PlatformGamepad = nkentseu::NkNoopGamepad;
#endif

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
        if (mWindows.BucketCount() == 0) {
            mWindows.Rehash(32);
        }

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

        // Injection de dependances croisees (NKEvent ne depend pas de NKWindow)
        mEventSystem.SetGamepadSystem(&mGamepadSystem);
        mGamepadSystem.SetEventSystem(&mEventSystem);
        NkInput.SetEventSystem(&mEventSystem);
        NkInput.SetGamepadSystem(&mGamepadSystem);

        // Cree le backend gamepad specifique a la plateforme
        {
            memory::NkAllocator& allocator = memory::NkGetDefaultAllocator();
            auto backend = memory::NkUniquePtr<NkIGamepad>(
                allocator.New<PlatformGamepad>(),
                memory::NkDefaultDelete<NkIGamepad>(&allocator));
            mGamepadSystem.Init(traits::NkMove(backend));
        }
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
        if (mWindows.BucketCount() == 0) {
            mWindows.Rehash(32);
        }
        NkWindowId id = mNextWindowId++;
        mWindows[id]  = win;
        return id;
    }

    void NkSystem::UnregisterWindow(NkWindowId id) {
        if (mWindows.Contains(id)) {
            mWindows.Erase(id);
            mEventSystem.RemoveWindowCallback(id);
        }
    }

    NkWindow* NkSystem::GetWindow(NkWindowId id) const {
        auto* win = mWindows.Find(id);
        return win ? *win : nullptr;
    }

    NkWindow* NkSystem::GetWindowAt(uint32 index) const {
        if (index >= mWindows.Size()) return nullptr;
        uint32 i = 0;
        NkWindow* result = nullptr;
        mWindows.ForEach([&](NkWindowId, NkWindow* win) {
            if (i++ == index) result = win;
        });
        return result;
    }

} // namespace nkentseu
