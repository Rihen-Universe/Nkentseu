// =============================================================================
// NkNoopWindow.cpp
// Headless/fallback implementation of NkWindow without PIMPL.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY) || \
    (!defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX) && \
    !defined(NKENTSEU_PLATFORM_ANDROID) && !defined(NKENTSEU_PLATFORM_MACOS) && !defined(NKENTSEU_PLATFORM_IOS) && \
    !defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && !defined(__EMSCRIPTEN__) && \
    !(defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_WAYLAND))))

#include "NKWindow/Platform/Noop/NkNoopWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"

#include <algorithm>

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

        mId = NkSystem::Instance().RegisterWindow(this);
        if (mId == NK_INVALID_WINDOW_ID) {
            mLastError = NkError(1, "Noop: failed to register window");
            return false;
        }

        mIsOpen = true;

        NkWindowCreateEvent createEvent(mData.mWidth, mData.mHeight);
        NkSystem::Events().Enqueue_Public(createEvent, mId);

        if (mData.mVisible) {
            NkWindowShownEvent shownEvent;
            NkSystem::Events().Enqueue_Public(shownEvent, mId);
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

        mId = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
        mData.mNativeHandle = nullptr;
        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mVisible = false;
        mData.mFullscreen = false;
    }

    bool NkWindow::IsOpen() const {
        return mIsOpen;
    }

    bool NkWindow::IsValid() const {
        return mIsOpen;
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
    }

    NkVec2u NkWindow::GetSize() const {
        return {mData.mWidth, mData.mHeight};
    }

    NkVec2u NkWindow::GetPosition() const {
        return {0u, 0u};
    }

    float NkWindow::GetDpiScale() const {
        return 1.0f;
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        return {mData.mWidth, mData.mHeight};
    }

    NkVec2u NkWindow::GetDisplayPosition() const {
        return {0u, 0u};
    }

    void NkWindow::SetSize(NkU32 width, NkU32 height) {
        const NkU32 oldW = mData.mWidth;
        const NkU32 oldH = mData.mHeight;

        mData.mWidth = std::max<NkU32>(width, 1u);
        mData.mHeight = std::max<NkU32>(height, 1u);
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        NkWindowResizeEvent resizeEvent(mData.mWidth, mData.mHeight, oldW, oldH);
        NkSystem::Events().Enqueue_Public(resizeEvent, mId);
    }

    void NkWindow::SetPosition(NkI32, NkI32) {}

    void NkWindow::SetVisible(bool visible) {
        if (mData.mVisible == visible) {
            return;
        }
        mData.mVisible = visible;
        mConfig.visible = visible;

        if (visible) {
            NkWindowShownEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowHiddenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
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
        } else {
            NkWindowWindowedEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        }
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

    void NkWindow::SetMousePosition(NkU32, NkU32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}

    NkWebInputOptions NkWindow::GetWebInputOptions() const {
        return {};
    }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        return {};
    }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        desc.width = mData.mWidth;
        desc.height = mData.mHeight;
        desc.dpi = 1.0f;
        desc.dummy = mData.mNativeHandle;
        return desc;
    }

} // namespace nkentseu

#endif
