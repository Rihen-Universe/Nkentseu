#pragma once
// =============================================================================
// NkXbox.h - Point d'entree Xbox.
//
// Variante separee du backend UWP pur pour permettre des implementations
// distinctes (GameCore/GDK, XDK legacy, etc.).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "NKWindow/Core/NkEntry.h"

#pragma comment(lib, "shell32.lib")

#ifndef NK_APP_NAME
#define NK_APP_NAME "xbox_app"
#endif

namespace nkentseu {
    NkEntryState *gState = nullptr;

    namespace {
        static void *gXboxNativeWindowOpaque = nullptr;

        static std::vector<std::string> NkBuildUtf8ArgsFromCommandLine() {
            int argc = 0;
            LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

            std::vector<std::string> args;
            args.reserve(static_cast<std::size_t>(argc));

            for (int i = 0; i < argc; ++i) {
                int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
                std::string s(static_cast<std::size_t>(sz), '\0');
                WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), sz, nullptr, nullptr);
                if (!s.empty() && s.back() == '\0') s.pop_back();
                args.push_back(std::move(s));
            }

            LocalFree(wargv);
            return args;
        }

        static HWND NkCaptureAnyWindowHandle() {
            HWND hwnd = GetActiveWindow();
            if (!hwnd) hwnd = GetForegroundWindow();
            return hwnd;
        }
    } // namespace

    inline bool NkXboxIsNativeWindowReady() {
        if (!gXboxNativeWindowOpaque) {
            gXboxNativeWindowOpaque = NkCaptureAnyWindowHandle();
        }
        return gXboxNativeWindowOpaque != nullptr;
    }

    inline void *NkXboxGetNativeWindowHandle() {
        if (!gXboxNativeWindowOpaque) {
            gXboxNativeWindowOpaque = NkCaptureAnyWindowHandle();
        }
        return gXboxNativeWindowOpaque;
    }

    inline void NkXboxPumpSystemEvents() {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

} // namespace nkentseu

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR, int nCmdShow) {
    std::vector<std::string> args = nkentseu::NkBuildUtf8ArgsFromCommandLine();
    void *nativeWindow = nkentseu::NkXboxGetNativeWindowHandle();

    nkentseu::NkEntryState state(hInst, hPrev, nullptr, nCmdShow, args, nativeWindow);
    state.appName = NK_APP_NAME;
    nkentseu::gState = &state;

    const int result = nkmain(state);
    nkentseu::gState = nullptr;
    return result;
}

#endif // NKENTSEU_PLATFORM_XBOX
