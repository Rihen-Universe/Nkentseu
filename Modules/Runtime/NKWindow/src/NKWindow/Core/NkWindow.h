#pragma once

// =============================================================================
// NkWindow.h
// Classe publique Window â€” implÃ©mentation concrÃ¨te.
// Les membres platform-spÃ©cifiques sont encapsulÃ©s dans NkWindowData,
// dÃ©fini par le backend platform (NkWin32Window.h, NkXLibWindow.hâ€¦).
//
// Usage :
//   nkentseu::NkInitialise();
//   nkentseu::NkWindowConfig cfg; cfg.title = "Hello";
//   nkentseu::NkWindow window(cfg);
//   while (window.IsOpen()) {
//       nkentseu::NkEvents().PollEvents();
//       /* render */
//   }
//   nkentseu::NkClose();
// =============================================================================

#include "NkWindowConfig.h"
#include "NkSafeArea.h"
#include "NkSurface.h"
#include "NkWindowId.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <string>

// Platform-specific NkWindowData struct definition
#if defined(NKENTSEU_PLATFORM_UWP)
    #include "NKWindow/Platform/UWP/NkUWPWindow.h"
#elif defined(NKENTSEU_PLATFORM_XBOX)
    #include "NKWindow/Platform/Xbox/NkXboxWindow.h"
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
    #include "NKWindow/Platform/Win32/NkWin32Window.h"
#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
    #include "NKWindow/Platform/Noop/NkNoopWindow.h"
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    #include "NKWindow/Platform/Wayland/NkWaylandWindow.h"
#elif defined(NKENTSEU_WINDOWING_XCB)
    #include "NKWindow/Platform/XCB/NkXCBWindow.h"
#elif defined(NKENTSEU_WINDOWING_XLIB)
    #include "NKWindow/Platform/XLib/NkXLibWindow.h"
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    #include "NKWindow/Platform/Android/NkAndroidWindow.h"
#elif defined(NKENTSEU_PLATFORM_MACOS)
    #include "NKWindow/Platform/Cocoa/NkCocoaWindow.h"
#elif defined(NKENTSEU_PLATFORM_IOS)
    #include "NKWindow/Platform/UIKit/NkUIKitWindow.h"
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #include "NKWindow/Platform/Emscripten/NkEmscriptenWindow.h"
#else
    #include "NKWindow/Platform/Noop/NkNoopWindow.h"
#endif

namespace nkentseu {

    class NkEventSystem;

    // ---------------------------------------------------------------------------
    // NkWindow â€” faÃ§ade de fenÃªtre cross-plateforme
    // ---------------------------------------------------------------------------

    class NkWindow {
        public:
            NkWindow();
            explicit NkWindow(const NkWindowConfig& config);
            ~NkWindow();

            NkWindow(const NkWindow&) = delete;
            NkWindow& operator=(const NkWindow&) = delete;
            NkWindow(NkWindow&&) = default;
            NkWindow& operator=(NkWindow&&) = default;

            // --- Cycle de vie ---
            bool Create(const NkWindowConfig& config);
            void Close();
            bool IsOpen()  const;
            bool IsValid() const;

            // --- Identifiant ---
            NkWindowId GetId() const { return mId; }

            // --- PropriÃ©tÃ©s ---
            NkString    GetTitle() const;
            void           SetTitle(const NkString& title);
            NkVec2u        GetSize() const;
            NkVec2u        GetPosition() const;
            float          GetDpiScale() const;
            NkVec2u        GetDisplaySize() const;
            NkVec2u        GetDisplayPosition() const;
            NkError        GetLastError() const;
            NkWindowConfig GetConfig() const;

            // --- Manipulation ---
            void SetSize(uint32 width, uint32 height);
            void SetPosition(int32 x, int32 y);
            void SetVisible(bool visible);
            void Minimize();
            void Maximize();
            void Restore();
            void SetFullscreen(bool fullscreen);
            bool SupportsOrientationControl() const;
            void SetScreenOrientation(NkScreenOrientation orientation);
            NkScreenOrientation GetScreenOrientation() const;
            void SetAutoRotateEnabled(bool enabled);
            bool IsAutoRotateEnabled() const;

            // --- Souris ---
            void SetMousePosition(uint32 x, uint32 y);
            void ShowMouse(bool show);
            void CaptureMouse(bool capture);

            // --- Web / WASM ---
            void SetWebInputOptions(const NkWebInputOptions& options);
            NkWebInputOptions GetWebInputOptions() const;

            // --- OS extras ---
            void SetProgress(float progress);

            // --- Safe Area (mobile) ---
            NkSafeAreaInsets GetSafeAreaInsets() const;

            // --- Surface graphique ---
            NkSurfaceDesc GetSurfaceDesc() const;

            // Platform data â€” accessed directly by backend .cpp files
            struct NkWindowData mData;

        private:
            NkWindowId     mId      = 0;
            bool           mIsOpen  = false;
            NkWindowConfig mConfig;
            NkError        mLastError;
    };

} // namespace nkentseu
