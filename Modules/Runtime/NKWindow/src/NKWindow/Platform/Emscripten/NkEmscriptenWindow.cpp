// =============================================================================
// NkEmscriptenWindow.cpp - NkWindow implementation for WASM/Emscripten
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKWindow/Platform/Emscripten/NkEmscriptenWindow.h"
#include "NKWindow/Platform/Emscripten/NkEmscriptenDropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <mutex>

namespace nkentseu {

    static std::mutex sWasmWindowsMutex;
    static NkWindow* sWasmLastWindow = nullptr;
    static NkWindowId sWasmActiveWindowId = NK_INVALID_WINDOW_ID;

    // Function-local statics avoid static init order fiasco with NkAllocator.
    static NkVector<NkWindow*>& WasmWindows() {
        static NkVector<NkWindow*> sVec;
        return sVec;
    }
    static NkUnorderedMap<NkWindowId, NkWindow*>& WasmWindowById() {
        static NkUnorderedMap<NkWindowId, NkWindow*> sMap;
        return sMap;
    }

    static const char* NormalizeCanvasSelector(const NkString& canvasId) {
        return canvasId.Empty() ? "#canvas" : canvasId.CStr();
    }

    static NkVec2u QueryViewportFallback() {
        const int width = EM_ASM_INT({
            var w = window.innerWidth || 0;
            if (w <= 0 && document && document.documentElement) {
                w = document.documentElement.clientWidth || 0;
            }
            return w > 0 ? (w | 0) : 1;
        });

        const int height = EM_ASM_INT({
            var h = window.innerHeight || 0;
            if (h <= 0 && document && document.documentElement) {
                h = document.documentElement.clientHeight || 0;
            }
            return h > 0 ? (h | 0) : 1;
        });

        return {
            static_cast<NkU32>(std::max(1, width)),
            static_cast<NkU32>(std::max(1, height)),
        };
    }

    static NkVec2u QueryCanvasSizeSafe(const char* selector) {
        const char* canvasSelector = (selector && *selector) ? selector : "#canvas";

        int width = 0;
        int height = 0;
        if (emscripten_get_canvas_element_size(canvasSelector, &width, &height) == EMSCRIPTEN_RESULT_SUCCESS &&
            width > 0 && height > 0) {
            return { static_cast<NkU32>(width), static_cast<NkU32>(height) };
        }

        double cssWidth = 0.0;
        double cssHeight = 0.0;
        if (emscripten_get_element_css_size(canvasSelector, &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS) {
            width = static_cast<int>(std::lround(cssWidth));
            height = static_cast<int>(std::lround(cssHeight));
        }

        if (width <= 0 || height <= 0) {
            const NkVec2u viewport = QueryViewportFallback();
            width = static_cast<int>(viewport.x);
            height = static_cast<int>(viewport.y);
        }

        if (width <= 0) {
            width = 1;
        }
        if (height <= 0) {
            height = 1;
        }

        emscripten_set_canvas_element_size(canvasSelector, width, height);
        return { static_cast<NkU32>(width), static_cast<NkU32>(height) };
    }

    static void SetDocumentTitle(const NkString& title) {
        EM_ASM({ document.title = UTF8ToString($0); }, title.CStr());
    }

    static void ApplyContextMenuPolicy(const char* selector, bool preventContextMenu) {
        const char* canvasSelector = (selector && *selector) ? selector : "#canvas";
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target) {
                return;
            }

            if (!target.__nkContextMenuHandler) {
                target.__nkContextMenuHandler = function(ev) {
                    ev.preventDefault();
                };
            }

            target.removeEventListener('contextmenu', target.__nkContextMenuHandler);
            if ($1) {
                target.addEventListener('contextmenu', target.__nkContextMenuHandler);
            }
        }, canvasSelector, preventContextMenu ? 1 : 0);
    }

    static void InstallCanvasKeyboardFocus(const char* selector) {
        const char* canvasSelector = (selector && *selector) ? selector : "#canvas";
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target) {
                return;
            }

            if (!target.hasAttribute('tabindex')) {
                target.setAttribute('tabindex', '0');
            }

            if (!target.__nkFocusHandler) {
                target.__nkFocusHandler = function() {
                    try {
                        target.focus({preventScroll: true});
                    } catch (e) {
                        try {
                            target.focus();
                        } catch (_) {
                        }
                    }
                };
            }

            target.removeEventListener('pointerdown', target.__nkFocusHandler);
            target.removeEventListener('mousedown', target.__nkFocusHandler);
            target.removeEventListener('touchstart', target.__nkFocusHandler);

            target.addEventListener('pointerdown', target.__nkFocusHandler);
            target.addEventListener('mousedown', target.__nkFocusHandler);
            target.addEventListener('touchstart', target.__nkFocusHandler, { passive: true });

            setTimeout(target.__nkFocusHandler, 0);
        }, canvasSelector);
    }

    static void RemoveCanvasKeyboardFocus(const char* selector) {
        const char* canvasSelector = (selector && *selector) ? selector : "#canvas";
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target || !target.__nkFocusHandler) {
                return;
            }

            target.removeEventListener('pointerdown', target.__nkFocusHandler);
            target.removeEventListener('mousedown', target.__nkFocusHandler);
            target.removeEventListener('touchstart', target.__nkFocusHandler);
        }, canvasSelector);
    }

    static void ApplyScreenOrientation(NkScreenOrientation orientation) {
        EM_ASM({
            var o = $0;
            var orientationApi = (typeof screen !== 'undefined') ? screen.orientation : null;
            if (!orientationApi) {
                return;
            }

            if (o === 0) {
                if (orientationApi.unlock) {
                    orientationApi.unlock();
                }
                return;
            }

            var mode = (o === 1) ? 'portrait' : 'landscape';
            if (orientationApi.lock) {
                orientationApi.lock(mode).catch(function() {});
            }
        }, static_cast<int>(orientation));
    }

    NkWindow* NkEmscriptenFindWindowById(NkWindowId id) {
        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);
        NkWindow** found = WasmWindowById().Find(id);
        return found ? *found : nullptr;
    }

    NkVector<NkWindow*> NkEmscriptenGetWindowsSnapshot() {
        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);
        return WasmWindows();
    }

    NkWindow* NkEmscriptenGetLastWindow() {
        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);
        return sWasmLastWindow;
    }

    void NkEmscriptenRegisterWindow(NkWindow* window) {
        if (!window) {
            return;
        }

        const NkWindowId id = window->GetId();
        if (id == NK_INVALID_WINDOW_ID) {
            return;
        }

        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);

        bool found = false;
        for (NkU32 i = 0; i < WasmWindows().Size(); ++i) {
            if (WasmWindows()[i] == window) { found = true; break; }
        }
        if (!found) {
            WasmWindows().PushBack(window);
        }

        WasmWindowById()[id] = window;
        sWasmLastWindow = window;

        if (sWasmActiveWindowId == NK_INVALID_WINDOW_ID) {
            sWasmActiveWindowId = id;
        }
    }

    void NkEmscriptenUnregisterWindow(NkWindow* window) {
        if (!window) {
            return;
        }

        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);

        for (NkU32 i = 0; i < WasmWindows().Size(); ++i) {
            if (WasmWindows()[i] == window) {
                WasmWindows().Erase(WasmWindows().begin() + i);
                break;
            }
        }

        NkWindowId staleId = NK_INVALID_WINDOW_ID;
        WasmWindowById().ForEach([&](const NkWindowId& k, NkWindow*& v) {
            if (v == window) { staleId = k; }
        });

        if (staleId != NK_INVALID_WINDOW_ID) {
            WasmWindowById().Erase(staleId);
            if (sWasmActiveWindowId == staleId) {
                sWasmActiveWindowId = NK_INVALID_WINDOW_ID;
            }
        }

        if (sWasmLastWindow == window) {
            sWasmLastWindow = WasmWindows().Empty() ? nullptr : WasmWindows().Back();
        }

        if (sWasmActiveWindowId == NK_INVALID_WINDOW_ID && sWasmLastWindow) {
            sWasmActiveWindowId = sWasmLastWindow->GetId();
        }
    }

    NkWindowId NkEmscriptenGetActiveWindowId() {
        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);
        return sWasmActiveWindowId;
    }

    void NkEmscriptenSetActiveWindowId(NkWindowId id) {
        std::lock_guard<std::mutex> lock(sWasmWindowsMutex);
        if (id != NK_INVALID_WINDOW_ID && !WasmWindowById().Contains(id)) {
            return;
        }
        sWasmActiveWindowId = id;
    }

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
        mConfig = config;
        mData.mCanvasId = "#canvas";

        const NkU32 requestedWidth = config.width ? config.width : 1280u;
        const NkU32 requestedHeight = config.height ? config.height : 720u;

        const char* canvasSelector = NormalizeCanvasSelector(mData.mCanvasId);
        emscripten_set_canvas_element_size(canvasSelector, static_cast<int>(requestedWidth), static_cast<int>(requestedHeight));

        const NkVec2u actual = QueryCanvasSizeSafe(canvasSelector);
        if (actual.x == 0 || actual.y == 0) {
            mLastError = NkError(1, "WASM: unable to determine canvas size");
            return false;
        }

        mData.mWidth = actual.x;
        mData.mHeight = actual.y;
        mData.mPrevWidth = mData.mWidth;
        mData.mPrevHeight = mData.mHeight;
        mData.mVisible = config.visible;
        mData.mFullscreen = config.fullscreen;

        SetDocumentTitle(config.title);
        InstallCanvasKeyboardFocus(canvasSelector);
        ApplyContextMenuPolicy(canvasSelector, config.webInput.preventContextMenu);
        ApplyScreenOrientation(config.screenOrientation);

        mId = NkSystem::Instance().RegisterWindow(this);
        NkEmscriptenRegisterWindow(this);
        NkEmscriptenSetActiveWindowId(mId);

        mData.mDropTarget = new NkEmscriptenDropTarget(mId, mData.mCanvasId);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& event) {
                NkDropEnterEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropOverCallback([this](const NkDropOverEvent& event) {
                NkDropOverEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& event) {
                NkDropLeaveEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& event) {
                NkDropFileEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& event) {
                NkDropTextEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
        }

        SetVisible(config.visible);
        SetWebInputOptions(config.webInput);
        if (config.fullscreen) {
            SetFullscreen(true);
        }

        mIsOpen = true;

        NkWindowCreateEvent event(mData.mWidth, mData.mHeight);
        NkSystem::Events().Enqueue_Public(event, mId);
        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }

        mIsOpen = false;

        RemoveCanvasKeyboardFocus(NormalizeCanvasSelector(mData.mCanvasId));

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        NkEmscriptenUnregisterWindow(this);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mPrevWidth = 0;
        mData.mPrevHeight = 0;
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
        return mConfig.title;
    }

    NkVec2u NkWindow::GetSize() const {
        return QueryCanvasSizeSafe(NormalizeCanvasSelector(mData.mCanvasId));
    }

    NkVec2u NkWindow::GetPosition() const {
        return {0u, 0u};
    }

    float NkWindow::GetDpiScale() const {
        return static_cast<float>(emscripten_get_device_pixel_ratio());
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        const int width = EM_ASM_INT({ return (window.screen && window.screen.width) ? (window.screen.width | 0) : 0; });
        const int height = EM_ASM_INT({ return (window.screen && window.screen.height) ? (window.screen.height | 0) : 0; });

        if (width > 0 && height > 0) {
            return { static_cast<NkU32>(width), static_cast<NkU32>(height) };
        }

        return QueryViewportFallback();
    }

    NkVec2u NkWindow::GetDisplayPosition() const {
        return {0u, 0u};
    }

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
        SetDocumentTitle(title);
    }

    void NkWindow::SetSize(NkU32 width, NkU32 height) {
        mConfig.width = width;
        mConfig.height = height;

        mData.mPrevWidth = mData.mWidth;
        mData.mPrevHeight = mData.mHeight;

        emscripten_set_canvas_element_size(
            NormalizeCanvasSelector(mData.mCanvasId),
            static_cast<int>(width),
            static_cast<int>(height));

        const NkVec2u size = QueryCanvasSizeSafe(NormalizeCanvasSelector(mData.mCanvasId));
        mData.mWidth = size.x;
        mData.mHeight = size.y;
    }

    void NkWindow::SetPosition(NkI32, NkI32) {}

    void NkWindow::SetVisible(bool visible) {
        mData.mVisible = visible;
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target) {
                return;
            }
            target.style.display = $1 ? "" : "none";
        }, NormalizeCanvasSelector(mData.mCanvasId), visible ? 1 : 0);
    }

    void NkWindow::Minimize() {}

    void NkWindow::Maximize() {}

    void NkWindow::Restore() {
        if (mData.mFullscreen) {
            SetFullscreen(false);
        }
        SetVisible(true);
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        mData.mFullscreen = fullscreen;
        mConfig.fullscreen = fullscreen;

        const char* canvasSelector = NormalizeCanvasSelector(mData.mCanvasId);
        if (fullscreen) {
            EmscriptenFullscreenStrategy strategy{};
            strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
            strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE;
            strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
            emscripten_enter_soft_fullscreen(canvasSelector, &strategy);
        } else {
            emscripten_exit_soft_fullscreen();
        }
    }

    bool NkWindow::SupportsOrientationControl() const {
        return true;
    }

    void NkWindow::SetScreenOrientation(NkScreenOrientation orientation) {
        mConfig.screenOrientation = orientation;
        ApplyScreenOrientation(orientation);
    }

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return mConfig.screenOrientation;
    }

    void NkWindow::SetAutoRotateEnabled(bool enabled) {
        if (enabled) {
            SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO);
            return;
        }

        const NkVec2u size = GetSize();
        SetScreenOrientation(
            size.x >= size.y
                ? NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE
                : NkScreenOrientation::NK_SCREEN_ORIENTATION_PORTRAIT);
    }

    bool NkWindow::IsAutoRotateEnabled() const {
        return mConfig.screenOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
    }

    void NkWindow::SetMousePosition(NkU32, NkU32) {}

    void NkWindow::ShowMouse(bool show) {
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target) {
                return;
            }
            target.style.cursor = $1 ? 'auto' : 'none';
        }, NormalizeCanvasSelector(mData.mCanvasId), show ? 1 : 0);
    }

    void NkWindow::CaptureMouse(bool capture) {
        const char* canvasSelector = NormalizeCanvasSelector(mData.mCanvasId);
        if (capture) {
            emscripten_request_pointerlock(canvasSelector, 1);
        } else {
            emscripten_exit_pointerlock();
        }
    }

    void NkWindow::SetWebInputOptions(const NkWebInputOptions& options) {
        mConfig.webInput = options;
        ApplyContextMenuPolicy(NormalizeCanvasSelector(mData.mCanvasId), options.preventContextMenu);
    }

    NkWebInputOptions NkWindow::GetWebInputOptions() const {
        return mConfig.webInput;
    }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        return {};
    }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const NkVec2u size = GetSize();
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.canvasId = mData.mCanvasId.CStr();
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
