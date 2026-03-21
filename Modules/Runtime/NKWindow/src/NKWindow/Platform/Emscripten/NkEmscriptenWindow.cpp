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
#include "NKCore/NkAtomic.h"
#include "NKMath/NkFunctions.h"

#include <emscripten.h>
#include <emscripten/html5.h>

namespace nkentseu {
    using namespace math;

    static NkSpinLock sWasmWindowsMutex;
    static NkWindow* sWasmLastWindow = nullptr;
    static NkWindowId sWasmActiveWindowId = NK_INVALID_WINDOW_ID;

    // Function-local statics avoid static init order fiasco with NkAllocator.
    static NkVector<NkWindow*>& WasmWindows() {
        static NkVector<NkWindow*> sVec;
        return sVec;
    }
    static NkUnorderedMap<NkWindowId, NkWindow*>& WasmWindowById() {
        static NkUnorderedMap<NkWindowId, NkWindow*> sMap;
        if (sMap.BucketCount() == 0) {
            sMap.Rehash(32);
        }
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
            static_cast<uint32>(math::NkMax(1, width)),
            static_cast<uint32>(math::NkMax(1, height)),
        };
    }

    static NkVec2u QueryCanvasSizeSafe(const char* selector) {
        const char* canvasSelector = (selector && *selector) ? selector : "#canvas";

        int width = 0;
        int height = 0;
        if (emscripten_get_canvas_element_size(canvasSelector, &width, &height) == EMSCRIPTEN_RESULT_SUCCESS &&
            width > 0 && height > 0) {
            return { static_cast<uint32>(width), static_cast<uint32>(height) };
        }

        double cssWidth = 0.0;
        double cssHeight = 0.0;
        if (emscripten_get_element_css_size(canvasSelector, &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS) {
            width = static_cast<int>(math::NkRound(cssWidth));
            height = static_cast<int>(math::NkRound(cssHeight));
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
        return { static_cast<uint32>(width), static_cast<uint32>(height) };
    }

    static void SetDocumentTitle(const NkString& title) {
        EM_ASM({ document.title = UTF8ToString($0); }, title.CStr());
    }

    static void ApplyDocumentIcon(const NkString& iconPath) {
        if (iconPath.Empty()) {
            return;
        }
        EM_ASM({
            var href = UTF8ToString($0);
            if (!href) {
                return;
            }
            var head = document.head || document.getElementsByTagName('head')[0];
            if (!head) {
                return;
            }
            var link = document.querySelector("link[rel~='icon']");
            if (!link) {
                link = document.createElement('link');
                link.rel = 'icon';
                head.appendChild(link);
            }
            link.href = href;
        }, iconPath.CStr());
    }

    static void ApplyCanvasTransparency(const char* selector, bool transparent) {
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
            target.style.backgroundColor = $1 ? "transparent" : "";
        }, canvasSelector, transparent ? 1 : 0);
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

    // =========================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =========================================================================

    static void SyncConfigFromWindow(const NkEmscriptenWindowData& data, NkWindowConfig& config) {
        config.width = data.mWidth;
        config.height = data.mHeight;
        config.visible = data.mVisible;
        config.fullscreen = data.mFullscreen;
        // Le titre n'est pas récupérable depuis JavaScript facilement, on garde config.title
        // La position n'est pas pertinente en WASM, on garde config.x/config.y
    }

    static void SyncWindowFromConfig(NkEmscriptenWindowData& data, const NkWindowConfig& config) {
        data.mVisible = config.visible;
        data.mFullscreen = config.fullscreen;
        
        const char* canvasSelector = NormalizeCanvasSelector(data.mCanvasId);
        
        // Appliquer la visibilité
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target) return;
            target.style.display = $1 ? "" : "none";
        }, canvasSelector, config.visible ? 1 : 0);
        
        // Appliquer le plein écran si nécessaire
        if (data.mFullscreen != config.fullscreen) {
            if (config.fullscreen) {
                EmscriptenFullscreenStrategy strategy{};
                strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
                strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE;
                strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
                emscripten_enter_soft_fullscreen(canvasSelector, &strategy);
            } else {
                emscripten_exit_soft_fullscreen();
            }
            data.mFullscreen = config.fullscreen;
        }
    }

    NkWindow* NkEmscriptenFindWindowById(NkWindowId id) {
        NkScopedSpinLock lock(sWasmWindowsMutex);
        NkWindow** found = WasmWindowById().Find(id);
        return found ? *found : nullptr;
    }

    NkVector<NkWindow*> NkEmscriptenGetWindowsSnapshot() {
        NkScopedSpinLock lock(sWasmWindowsMutex);
        return WasmWindows();
    }

    NkWindow* NkEmscriptenGetLastWindow() {
        NkScopedSpinLock lock(sWasmWindowsMutex);
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

        NkScopedSpinLock lock(sWasmWindowsMutex);

        bool found = false;
        for (uint32 i = 0; i < WasmWindows().Size(); ++i) {
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

        NkScopedSpinLock lock(sWasmWindowsMutex);

        for (uint32 i = 0; i < WasmWindows().Size(); ++i) {
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
        NkScopedSpinLock lock(sWasmWindowsMutex);
        return sWasmActiveWindowId;
    }

    void NkEmscriptenSetActiveWindowId(NkWindowId id) {
        NkScopedSpinLock lock(sWasmWindowsMutex);
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
        mData.mAppliedHints = config.surfaceHints;
        mData.mCanvasId = "#canvas";
        mData.mExternal = false;

        const bool wantsExternal = config.native.useExternalWindow;
        const bool hasExternalHandle = (config.native.externalWindowHandle != 0);
        if (wantsExternal && !hasExternalHandle) {
            mLastError = NkError(1, "WASM: useExternalWindow=true but externalWindowHandle is null");
            return false;
        }

        if (wantsExternal && hasExternalHandle) {
            const char* externalSelector =
                reinterpret_cast<const char*>(config.native.externalWindowHandle);
            if (!externalSelector || !externalSelector[0]) {
                mLastError = NkError(1, "WASM: externalWindowHandle must point to a non-empty canvas selector string");
                return false;
            }
            mData.mCanvasId = externalSelector;
            mData.mExternal = true;
        } else if (config.native.externalDisplayHandle != 0) {
            const char* externalSelector =
                reinterpret_cast<const char*>(config.native.externalDisplayHandle);
            if (externalSelector && externalSelector[0]) {
                mData.mCanvasId = externalSelector;
            }
        }

        const uint32 requestedWidth = config.width ? config.width : 1280u;
        const uint32 requestedHeight = config.height ? config.height : 720u;

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

        // Synchroniser mConfig avec les dimensions réelles
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        SetDocumentTitle(config.title);
        ApplyDocumentIcon(config.iconPath);
        InstallCanvasKeyboardFocus(canvasSelector);
        ApplyContextMenuPolicy(canvasSelector, config.webInput.preventContextMenu);
        ApplyCanvasTransparency(canvasSelector, config.transparent);
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
        mData.mExternal = false;
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
        // Synchroniser avant de retourner
        if (mIsOpen) {
            SyncConfigFromWindow(mData, const_cast<NkWindow*>(this)->mConfig);
        }
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        return mConfig.title;
    }

    NkVec2u NkWindow::GetSize() const {
        NkVec2u size = QueryCanvasSizeSafe(NormalizeCanvasSelector(mData.mCanvasId));
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mData.mWidth = size.x;
        const_cast<NkWindow*>(this)->mData.mHeight = size.y;
        const_cast<NkWindow*>(this)->mConfig.width = size.x;
        const_cast<NkWindow*>(this)->mConfig.height = size.y;
        
        return size;
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
            return { static_cast<uint32>(width), static_cast<uint32>(height) };
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

    void NkWindow::SetSize(uint32 width, uint32 height) {
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
        
        // S'assurer que mConfig est à jour
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;
    }

    void NkWindow::SetPosition(int32, int32) {}

    void NkWindow::SetVisible(bool visible) {
        mData.mVisible = visible;
        mConfig.visible = visible;
        
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
        
        // La taille peut changer en plein écran, on la met à jour
        const NkVec2u size = QueryCanvasSizeSafe(canvasSelector);
        mData.mWidth = size.x;
        mData.mHeight = size.y;
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;
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

    void NkWindow::SetMousePosition(uint32, uint32) {}

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
        const NkVec2u size = GetSize();  // GetSize synchronise déjà mConfig
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.canvasId = mData.mCanvasId.CStr();
        desc.appliedHints = mData.mAppliedHints;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__