#pragma once
// =============================================================================
// NkUWP.h - Point d'entree UWP pur (CoreApplication/CoreWindow).
//
// Expose une petite API runtime consommee par le backend UWP :
//   NkUWPIsCoreWindowReady()
//   NkUWPGetCoreWindowHandle()
//   NkUWPPumpSystemEvents()
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

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
#define NK_APP_NAME "uwp_app"
#endif

#if defined(__has_include)
#if __has_include(<winrt/Windows.ApplicationModel.Core.h>) && __has_include(<winrt/Windows.UI.Core.h>)
#define NKENTSEU_UWP_HAS_WINRT 1
#endif
#endif

#ifndef NKENTSEU_UWP_HAS_WINRT
#define NKENTSEU_UWP_HAS_WINRT 0
#endif

#if NKENTSEU_UWP_HAS_WINRT
#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#endif

namespace nkentseu {
    inline NkEntryState *gState = nullptr;

    namespace {
        inline int gUwpExitCode = 0;

        static NkVector<NkString> NkBuildUtf8ArgsFromCommandLine() {
            int argc = 0;
            LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

            NkVector<NkString> args;
            args.reserve(static_cast<std::size_t>(argc));

            for (int i = 0; i < argc; ++i) {
                int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
                NkString s(static_cast<std::size_t>(sz), '\0');
                WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), sz, nullptr, nullptr);
                if (!s.empty() && s.back() == '\0') s.pop_back();
                args.push_back(std::move(s));
            }

            LocalFree(wargv);
            return args;
        }
    } // namespace

#if NKENTSEU_UWP_HAS_WINRT
    inline winrt::Windows::UI::CoreWindow gCoreWindow{nullptr};
    inline void *gCoreWindowOpaque = nullptr;

    inline bool NkUWPIsCoreWindowReady() {
        return gCoreWindow != nullptr;
    }

    inline void *NkUWPGetCoreWindowHandle() {
        return gCoreWindowOpaque;
    }

    inline void NkUWPPumpSystemEvents() {
        if (!gCoreWindow) return;
        using winrt::Windows::UI::CoreProcessEventsOption;
        gCoreWindow.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
    }

    #if !defined(NKENTSEU_UWP_RUNTIME_ONLY)
    struct NkUWPView final : winrt::implements<NkUWPView, winrt::Windows::ApplicationModel::IFrameworkView> {
        void Initialize(const winrt::Windows::ApplicationModel::CoreApplicationView &) {}
        void Load(const winrt::hstring &) {}
        void Uninitialize() {}

        void SetWindow(const winrt::Windows::UI::CoreWindow &window) {
            gCoreWindow = window;
            gCoreWindowOpaque = winrt::get_abi(gCoreWindow);
        }

        void Run() {
            if (gCoreWindow) {
                gCoreWindow.Activate();
            }

            NkVector<NkString> args = NkBuildUtf8ArgsFromCommandLine();
            NkEntryState state(args, gCoreWindowOpaque);
            state.appName = NK_APP_NAME;
            gState = &state;

            gUwpExitCode = nkmain(state);
            gState = nullptr;

            winrt::Windows::ApplicationModel::CoreApplication::Exit();
        }
    };

    struct NkUWPViewSource final
        : winrt::implements<NkUWPViewSource, winrt::Windows::ApplicationModel::IFrameworkViewSource> {
        winrt::Windows::ApplicationModel::IFrameworkView CreateView() {
            return winrt::make<NkUWPView>();
        }
    };

    [MTAThread]
    int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        winrt::Windows::ApplicationModel::CoreApplication::Run(winrt::make<NkUWPViewSource>());
        return gUwpExitCode;
    }
    #endif // !NKENTSEU_UWP_RUNTIME_ONLY

#else
    // Fallback sans C++/WinRT : permet la compilation, mais pas de CoreWindow runtime.
    inline bool NkUWPIsCoreWindowReady() { return false; }
    inline void *NkUWPGetCoreWindowHandle() { return nullptr; }
    inline void NkUWPPumpSystemEvents() {}

    #if !defined(NKENTSEU_UWP_RUNTIME_ONLY)
    int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
        NkVector<NkString> args = NkBuildUtf8ArgsFromCommandLine();
        NkEntryState state(args, nullptr);
        state.appName = NK_APP_NAME;
        gState = &state;

        const int result = nkmain(state);
        gState = nullptr;
        return result;
    }
    #endif // !NKENTSEU_UWP_RUNTIME_ONLY
#endif

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_UWP
