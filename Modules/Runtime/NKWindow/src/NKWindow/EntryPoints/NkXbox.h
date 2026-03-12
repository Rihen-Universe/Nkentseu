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

#include "NKWindow/Core/NkEntry.h"
#include "NKCore/NkTraits.h"

#pragma comment(lib, "shell32.lib")

#ifndef NK_APP_NAME
#define NK_APP_NAME "xbox_app"
#endif

namespace nkentseu {
    NkEntryState *gState = nullptr;

    namespace {
        static void *gXboxNativeWindowOpaque = nullptr;

        static NkVector<NkString> NkBuildUtf8ArgsFromCommandLine() {
            int argc = 0;
            LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

            NkVector<NkString> args;
            args.Reserve(static_cast<nk_size>(argc));

            for (int i = 0; i < argc; ++i) {
                int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
                NkString s(static_cast<nk_size>(sz), '\0');
                WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.Data(), sz, nullptr, nullptr);
                if (!s.Empty() && s.Back() == '\0') s.PopBack();
                args.PushBack(traits::NkMove(s));
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
    if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
        return -1;
    }
    NkVector<NkString> args = nkentseu::NkBuildUtf8ArgsFromCommandLine();
    void *nativeWindow = nkentseu::NkXboxGetNativeWindowHandle();

    nkentseu::NkEntryState state(hInst, hPrev, nullptr, nCmdShow, args, nativeWindow);
    nkentseu::NkApplyEntryAppName(state, NK_APP_NAME);
    nkentseu::gState = &state;

    const int result = nkmain(state);
    nkentseu::gState = nullptr;
    nkentseu::NkEntryRuntimeShutdown(true);
    return result;
}

#endif // NKENTSEU_PLATFORM_XBOX
