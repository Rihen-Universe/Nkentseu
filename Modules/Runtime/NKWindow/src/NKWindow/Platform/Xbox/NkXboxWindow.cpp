// =============================================================================
// NkXboxWindow.cpp
// Xbox implementation of NkWindow without PIMPL.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Platform/Xbox/NkXboxWindow.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKContainers/String/NkWString.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>

namespace {

    constexpr wchar_t kNkXboxFallbackWindowClassName[] = L"NkXboxFallbackWindowClass";

    LRESULT CALLBACK NkXboxFallbackWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        if (msg == WM_CLOSE) {
            DestroyWindow(hwnd);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    NkWString NkUtf8ToWide(const NkString &value) {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 1) return {};
        NkWString wide(static_cast<usize>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.Data(), size);
        return wide;
    }

    HWND NkCreateFallbackXboxWindow(const nkentseu::NkWindowConfig &config) {
        HINSTANCE hInstance = GetModuleHandleW(nullptr);
        if (!hInstance) return nullptr;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = NkXboxFallbackWindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kNkXboxFallbackWindowClassName;
        RegisterClassExW(&wc);

        const nkentseu::uint32 width = config.width > 0 ? config.width : 1280u;
        const nkentseu::uint32 height = config.height > 0 ? config.height : 720u;
        RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        NkWString title = NkUtf8ToWide(config.title);
        if (title.Empty()) title = L"NkXboxWindow";

        HWND hwnd = CreateWindowExW(
            0,
            kNkXboxFallbackWindowClassName,
            title.CStr(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        if (hwnd && config.visible) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            UpdateWindow(hwnd);
        }
        return hwnd;
    }

} // namespace

namespace nkentseu {

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) {
            Close();
        }
    }

    bool NkWindow::Create(const NkWindowConfig& config) {
        if (mIsOpen) {
            Close();
        }

        mConfig = config;
        mData.mTitle = config.title;
        mData.mWidth = config.width > 0 ? config.width : 1280u;
        mData.mHeight = config.height > 0 ? config.height : 720u;
        mData.mVisible = config.visible;
        mData.mFullscreen = config.fullscreen;
        mData.mOwnsNativeWindow = false;

        // 1) runtime handle exposé par l'entrypoint Xbox
        mData.mNativeWindow = NkXboxGetNativeWindowHandle();
        if (!mData.mNativeWindow && gState) {
            mData.mNativeWindow = gState->xboxNativeWindow;
        }

        // 2) fallback desktop-compatible (utile hors runtime console complet)
        if (!mData.mNativeWindow) {
            HWND fallback = NkCreateFallbackXboxWindow(config);
            if (fallback) {
                mData.mNativeWindow = reinterpret_cast<void *>(fallback);
                mData.mOwnsNativeWindow = true;
            }
        }

        if (!mData.mNativeWindow) {
            mLastError = NkError(1, "Xbox: native window not available");
            return false;
        }

        mId = NkSystem::Instance().RegisterWindow(this);
        if (mId == NK_INVALID_WINDOW_ID) {
            mLastError = NkError(1, "Xbox: failed to register window");
            if (mData.mOwnsNativeWindow) {
                DestroyWindow(reinterpret_cast<HWND>(mData.mNativeWindow));
                mData.mOwnsNativeWindow = false;
                mData.mNativeWindow = nullptr;
            }
            return false;
        }

        mIsOpen = true;

        NkWindowCreateEvent createEvent(mData.mWidth, mData.mHeight);
        NkSystem::Events().Enqueue_Public(createEvent, mId);

        if (mData.mVisible) {
            NkWindowShownEvent shownEvent;
            NkSystem::Events().Enqueue_Public(shownEvent, mId);
        }
        if (mData.mFullscreen) {
            NkWindowFullscreenEvent fullscreenEvent;
            NkSystem::Events().Enqueue_Public(fullscreenEvent, mId);
        }

        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }

        const NkWindowId closingId = mId;

        NkWindowCloseEvent closeEvent(false);
        NkSystem::Events().Enqueue_Public(closeEvent, closingId);

        NkWindowDestroyEvent destroyEvent;
        NkSystem::Events().Enqueue_Public(destroyEvent, closingId);

        NkSystem::Instance().UnregisterWindow(closingId);

        if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
            DestroyWindow(reinterpret_cast<HWND>(mData.mNativeWindow));
        }

        mId = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
        mData.mNativeWindow = nullptr;
        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mVisible = false;
        mData.mFullscreen = false;
        mData.mOwnsNativeWindow = false;
    }

    bool NkWindow::IsOpen() const {
        return mIsOpen;
    }

    bool NkWindow::IsValid() const {
        return mIsOpen && (mData.mNativeWindow != nullptr);
    }

    NkError NkWindow::GetLastError() const {
        return mLastError;
    }

    NkWindowConfig NkWindow::GetConfig() const {
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        return mData.mTitle;
    }

    void NkWindow::SetTitle(const NkString& title) {
        mData.mTitle = title;
        mConfig.title = title;
        if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
            NkWString wide = NkUtf8ToWide(title);
            if (!wide.Empty()) {
                SetWindowTextW(reinterpret_cast<HWND>(mData.mNativeWindow), wide.CStr());
            }
        }
    }

    NkVec2u NkWindow::GetSize() const {
        return { mData.mWidth, mData.mHeight };
    }

    NkVec2u NkWindow::GetPosition() const {
        return { 0u, 0u };
    }

    float NkWindow::GetDpiScale() const {
        return 1.0f;
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        return { mData.mWidth, mData.mHeight };
    }

    NkVec2u NkWindow::GetDisplayPosition() const {
        return { 0u, 0u };
    }

    void NkWindow::SetSize(uint32 width, uint32 height) {
        const uint32 w = math::NkMax<uint32>(width, 1u);
        const uint32 h = math::NkMax<uint32>(height, 1u);
        const uint32 oldW = mData.mWidth;
        const uint32 oldH = mData.mHeight;

        mData.mWidth = w;
        mData.mHeight = h;
        mConfig.width = w;
        mConfig.height = h;

        NkWindowResizeEvent resizeEvent(w, h, oldW, oldH);
        NkSystem::Events().Enqueue_Public(resizeEvent, mId);

        if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
            SetWindowPos(
                reinterpret_cast<HWND>(mData.mNativeWindow),
                nullptr,
                0,
                0,
                static_cast<int>(w),
                static_cast<int>(h),
                SWP_NOZORDER | SWP_NOMOVE);
        }
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
            SetWindowPos(
                reinterpret_cast<HWND>(mData.mNativeWindow),
                nullptr,
                static_cast<int>(x),
                static_cast<int>(y),
                0,
                0,
                SWP_NOZORDER | SWP_NOSIZE);
        }
    }

    void NkWindow::SetVisible(bool visible) {
        if (mData.mVisible == visible) {
            return;
        }
        mData.mVisible = visible;
        mConfig.visible = visible;

        if (visible) {
            NkWindowShownEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
            if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
                ShowWindow(reinterpret_cast<HWND>(mData.mNativeWindow), SW_SHOW);
            }
        } else {
            NkWindowHiddenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
            if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
                ShowWindow(reinterpret_cast<HWND>(mData.mNativeWindow), SW_HIDE);
            }
        }
    }

    void NkWindow::Minimize() {}

    void NkWindow::Maximize() {}

    void NkWindow::Restore() {}

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (mData.mFullscreen == fullscreen) {
            return;
        }
        mData.mFullscreen = fullscreen;
        mConfig.fullscreen = fullscreen;

        if (fullscreen) {
            NkWindowFullscreenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
            if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
                ShowWindow(reinterpret_cast<HWND>(mData.mNativeWindow), SW_MAXIMIZE);
            }
        } else {
            NkWindowWindowedEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
            if (mData.mOwnsNativeWindow && mData.mNativeWindow) {
                ShowWindow(reinterpret_cast<HWND>(mData.mNativeWindow), SW_RESTORE);
            }
        }
    }

    void NkWindow::SetMousePosition(uint32, uint32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    void NkWindow::SetProgress(float) {}

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        desc.width = mData.mWidth;
        desc.height = mData.mHeight;
        desc.dpi = 1.0f;
        desc.nativeWindow = mData.mNativeWindow;
        return desc;
    }

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        return {};
    }

    bool NkWindow::SupportsOrientationControl() const {
        return false;
    }

    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }

    void NkWindow::SetAutoRotateEnabled(bool) {}

    bool NkWindow::IsAutoRotateEnabled() const {
        return false;
    }

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}

    NkWebInputOptions NkWindow::GetWebInputOptions() const {
        return {};
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_XBOX
