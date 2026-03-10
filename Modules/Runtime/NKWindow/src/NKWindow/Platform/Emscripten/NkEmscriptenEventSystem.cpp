// =============================================================================
// NkEmscriptenEventSystem.cpp
// WASM/Emscripten implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkKeyboardEvent.h"
#include "NKWindow/Events/NkMouseEvent.h"
#include "NKWindow/Events/NkTouchEvent.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkKeycodeMap.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Platform/Emscripten/NkEmscriptenDropTarget.h"
#include "NKWindow/Platform/Emscripten/NkEmscriptenWindow.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace nkentseu {

    static NkEventSystem* gWASMEventSystem = nullptr;
    static NkWindowId gFocusedWindowId = NK_INVALID_WINDOW_ID;
    static NkWebInputOptions gWebInputOpts{};
    static NkString gCanvasSelector = "#canvas";

    static const char* CurrentCanvasSelector() {
        return gCanvasSelector.Empty() ? "#canvas" : gCanvasSelector.CStr();
    }

    template <typename Fn>
    static void ForEachWASMWindow(Fn&& fn) {
        const NkVector<NkWindow*> windows = NkEmscriptenGetWindowsSnapshot();
        for (NkWindow* window : windows) {
            if (!window) {
                continue;
            }
            const NkWindowId id = window->GetId();
            if (id == NK_INVALID_WINDOW_ID) {
                continue;
            }
            fn(*window, id);
        }
    }

    static NkWindowId ResolveActiveWindowId() {
        NkWindowId active = NkEmscriptenGetActiveWindowId();
        if (active != NK_INVALID_WINDOW_ID && NkEmscriptenFindWindowById(active)) {
            return active;
        }

        if (gFocusedWindowId != NK_INVALID_WINDOW_ID && NkEmscriptenFindWindowById(gFocusedWindowId)) {
            return gFocusedWindowId;
        }

        if (NkWindow* last = NkEmscriptenGetLastWindow()) {
            const NkWindowId id = last->GetId();
            if (id != NK_INVALID_WINDOW_ID) {
                return id;
            }
        }

        const NkVector<NkWindow*> windows = NkEmscriptenGetWindowsSnapshot();
        for (NkU32 i = windows.Size(); i > 0; --i) {
            NkWindow* window = windows[i - 1];
            if (!window) {
                continue;
            }
            const NkWindowId id = window->GetId();
            if (id != NK_INVALID_WINDOW_ID) {
                return id;
            }
        }

        return NK_INVALID_WINDOW_ID;
    }

    static void SyncActiveWindowContext() {
        const NkWindowId active = ResolveActiveWindowId();
        gFocusedWindowId = active;
        NkEmscriptenSetActiveWindowId(active);

        NkWindow* window = NkEmscriptenFindWindowById(active);
        if (!window) {
            gWebInputOpts = {};
            gCanvasSelector = "#canvas";
            return;
        }

        gWebInputOpts = window->GetWebInputOptions();
        gCanvasSelector = window->mData.mCanvasId.Empty() ? "#canvas" : window->mData.mCanvasId;
    }

    struct CanvasCoords {
        NkI32 x;
        NkI32 y;
        double sx;
        double sy;
    };

    static CanvasCoords MapCssToCanvas(const char* canvasSelector, NkI32 cssX, NkI32 cssY) {
        CanvasCoords out{cssX, cssY, 1.0, 1.0};

        int canvasWidth = 0;
        int canvasHeight = 0;
        double cssWidth = 0.0;
        double cssHeight = 0.0;

        if (emscripten_get_canvas_element_size(canvasSelector, &canvasWidth, &canvasHeight) != EMSCRIPTEN_RESULT_SUCCESS) {
            return out;
        }
        if (emscripten_get_element_css_size(canvasSelector, &cssWidth, &cssHeight) != EMSCRIPTEN_RESULT_SUCCESS) {
            return out;
        }

        if (canvasWidth <= 0 || canvasHeight <= 0 || cssWidth <= 0.0 || cssHeight <= 0.0) {
            return out;
        }

        out.sx = static_cast<double>(canvasWidth) / cssWidth;
        out.sy = static_cast<double>(canvasHeight) / cssHeight;
        out.x = static_cast<NkI32>(std::lround(static_cast<double>(cssX) * out.sx));
        out.y = static_cast<NkI32>(std::lround(static_cast<double>(cssY) * out.sy));
        return out;
    }

    static bool IsBrowserShortcut(const EmscriptenKeyboardEvent* ke) {
        if (!ke) {
            return false;
        }

        const unsigned long keycode = ke->keyCode;
        if (keycode == 123) {
            return true;
        }
        if (ke->ctrlKey && ke->shiftKey && (keycode == 73 || keycode == 74)) {
            return true;
        }
        if (ke->ctrlKey && (keycode == 82 || keycode == 116)) {
            return true;
        }
        if (ke->metaKey) {
            return true;
        }
        return false;
    }

    static void InstallDropBridge(const char* canvasSelector) {
        const char* selector = (canvasSelector && *canvasSelector) ? canvasSelector : "#canvas";
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target || target.__nkDropBridgeInstalled) {
                return;
            }

            target.__nkDropBridgeInstalled = true;

            target.__nkDragEnterHandler = function(ev) {
                ev.preventDefault();
                ev.stopPropagation();

                var count = 0;
                var hasText = 0;
                var hasImage = 0;

                if (ev.dataTransfer) {
                    if (ev.dataTransfer.items) {
                        count = ev.dataTransfer.items.length || 0;
                    } else if (ev.dataTransfer.files) {
                        count = ev.dataTransfer.files.length || 0;
                    }

                    var types = ev.dataTransfer.types || [];
                    for (var i = 0; i < types.length; ++i) {
                        if (types[i] === "text/plain" || types[i] === "text/uri-list") {
                            hasText = 1;
                        }
                        if (types[i].indexOf("image/") === 0) {
                            hasImage = 1;
                        }
                    }
                }

                if (typeof Module !== 'undefined' && Module['_NkEmscriptenOnDragEnter']) {
                    Module._NkEmscriptenOnDragEnter(ev.clientX, ev.clientY, count, hasText, hasImage);
                }
            };

            target.__nkDragOverHandler = function(ev) {
                ev.preventDefault();
                ev.stopPropagation();
                if (typeof Module !== 'undefined' && Module['_NkEmscriptenOnDragOver']) {
                    Module._NkEmscriptenOnDragOver(ev.clientX, ev.clientY);
                }
            };

            target.__nkDragLeaveHandler = function(ev) {
                ev.preventDefault();
                ev.stopPropagation();
                if (typeof Module !== 'undefined' && Module['_NkEmscriptenOnDragLeave']) {
                    Module._NkEmscriptenOnDragLeave();
                }
            };

            target.__nkDropHandler = function(ev) {
                ev.preventDefault();
                ev.stopPropagation();

                if (typeof Module !== 'undefined' && Module['_NkEmscriptenOnDragLeave']) {
                    Module._NkEmscriptenOnDragLeave();
                }

                var hasCcall = (typeof Module !== 'undefined' && typeof Module.ccall === 'function');

                if (hasCcall && ev.dataTransfer) {
                    var text = ev.dataTransfer.getData("text/plain") || "";
                    if (text.length > 0) {
                        Module.ccall("NkEmscriptenOnDropText", null, ["string", "number", "number"], [text, ev.clientX, ev.clientY]);
                    }

                    var names = [];
                    var files = ev.dataTransfer.files || [];
                    for (var i = 0; i < files.length; ++i) {
                        names.push(files[i].name || "");
                    }
                    if (names.length > 0) {
                        Module.ccall("NkEmscriptenOnDropFiles", null, ["string", "number", "number"], [names.join("\n"), ev.clientX, ev.clientY]);
                    }
                }
            };

            target.addEventListener('dragenter', target.__nkDragEnterHandler);
            target.addEventListener('dragover', target.__nkDragOverHandler);
            target.addEventListener('dragleave', target.__nkDragLeaveHandler);
            target.addEventListener('drop', target.__nkDropHandler);
        }, selector);
    }

    static void UninstallDropBridge(const char* canvasSelector) {
        const char* selector = (canvasSelector && *canvasSelector) ? canvasSelector : "#canvas";
        EM_ASM({
            var sel = UTF8ToString($0);
            var target = document.querySelector(sel);
            if (!target && typeof Module !== 'undefined' && Module['canvas']) {
                target = Module['canvas'];
            }
            if (!target || !target.__nkDropBridgeInstalled) {
                return;
            }

            target.removeEventListener('dragenter', target.__nkDragEnterHandler);
            target.removeEventListener('dragover', target.__nkDragOverHandler);
            target.removeEventListener('dragleave', target.__nkDragLeaveHandler);
            target.removeEventListener('drop', target.__nkDropHandler);

            target.__nkDragEnterHandler = null;
            target.__nkDragOverHandler = null;
            target.__nkDragLeaveHandler = null;
            target.__nkDropHandler = null;
            target.__nkDropBridgeInstalled = false;
        }, selector);
    }

    static NkWindowId ResolveDropWindowId() {
        SyncActiveWindowContext();
        return gFocusedWindowId;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnKeyDown(int, const EmscriptenKeyboardEvent* ke, void*) {
        if (!gWASMEventSystem || !ke) {
            return EM_FALSE;
        }

        SyncActiveWindowContext();
        if (!gWebInputOpts.captureKeyboard) {
            return EM_FALSE;
        }
        if (gWebInputOpts.allowBrowserShortcuts && IsBrowserShortcut(ke)) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        NkScancode scancode = NkScancodeFromDOMCode(ke->code);
        NkKey key = NkScancodeToKey(scancode);
        if (key == NkKey::NK_UNKNOWN) {
            key = NkKeycodeMap::NkKeyFromDomCode(ke->code);
        }
        if (key == NkKey::NK_UNKNOWN) {
            return EM_FALSE;
        }

        NkModifierState modifiers(ke->ctrlKey, ke->altKey, ke->shiftKey, ke->metaKey);
        if (ke->repeat) {
            NkKeyRepeatEvent event(key, 1, scancode, modifiers, static_cast<NkU32>(ke->keyCode));
            gWASMEventSystem->Enqueue_Public(event, targetWindowId);
        } else {
            NkKeyPressEvent event(key, scancode, modifiers, static_cast<NkU32>(ke->keyCode));
            gWASMEventSystem->Enqueue_Public(event, targetWindowId);
        }

        if (!ke->repeat && ke->key[0] >= 0x20 && static_cast<unsigned char>(ke->key[0]) != 0x7F) {
            NkTextInputEvent event(static_cast<NkU32>(static_cast<unsigned char>(ke->key[0])));
            gWASMEventSystem->Enqueue_Public(event, targetWindowId);
        }

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnKeyUp(int, const EmscriptenKeyboardEvent* ke, void*) {
        if (!gWASMEventSystem || !ke) {
            return EM_FALSE;
        }

        SyncActiveWindowContext();
        if (!gWebInputOpts.captureKeyboard) {
            return EM_FALSE;
        }
        if (gWebInputOpts.allowBrowserShortcuts && IsBrowserShortcut(ke)) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        NkScancode scancode = NkScancodeFromDOMCode(ke->code);
        NkKey key = NkScancodeToKey(scancode);
        if (key == NkKey::NK_UNKNOWN) {
            key = NkKeycodeMap::NkKeyFromDomCode(ke->code);
        }
        if (key == NkKey::NK_UNKNOWN) {
            return EM_FALSE;
        }

        NkModifierState modifiers(ke->ctrlKey, ke->altKey, ke->shiftKey, ke->metaKey);
        NkKeyReleaseEvent event(key, scancode, modifiers, static_cast<NkU32>(ke->keyCode));
        gWASMEventSystem->Enqueue_Public(event, targetWindowId);

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnMouseMove(int, const EmscriptenMouseEvent* me, void*) {
        if (!gWASMEventSystem || !me) {
            return EM_TRUE;
        }

        SyncActiveWindowContext();
        if (!gWebInputOpts.captureMouseMove) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        const CanvasCoords mapped = MapCssToCanvas(CurrentCanvasSelector(), static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
        const NkI32 dx = static_cast<NkI32>(std::lround(static_cast<double>(me->movementX) * mapped.sx));
        const NkI32 dy = static_cast<NkI32>(std::lround(static_cast<double>(me->movementY) * mapped.sy));

        NkMouseMoveEvent event(
            mapped.x,
            mapped.y,
            static_cast<NkI32>(me->screenX),
            static_cast<NkI32>(me->screenY),
            dx,
            dy);
        gWASMEventSystem->Enqueue_Public(event, targetWindowId);

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnMouseDown(int, const EmscriptenMouseEvent* me, void*) {
        if (!gWASMEventSystem || !me) {
            return EM_TRUE;
        }

        SyncActiveWindowContext();
        if ((me->button == 0 && !gWebInputOpts.captureMouseLeft) ||
            (me->button == 1 && !gWebInputOpts.captureMouseMiddle) ||
            (me->button == 2 && !gWebInputOpts.captureMouseRight)) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        const NkMouseButton button =
            (me->button == 2) ? NkMouseButton::NK_MB_RIGHT :
            (me->button == 1) ? NkMouseButton::NK_MB_MIDDLE :
                                NkMouseButton::NK_MB_LEFT;

        const CanvasCoords mapped = MapCssToCanvas(CurrentCanvasSelector(), static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
        NkMouseButtonPressEvent event(
            button,
            mapped.x,
            mapped.y,
            static_cast<NkI32>(me->screenX),
            static_cast<NkI32>(me->screenY));
        gWASMEventSystem->Enqueue_Public(event, targetWindowId);

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnMouseUp(int, const EmscriptenMouseEvent* me, void*) {
        if (!gWASMEventSystem || !me) {
            return EM_TRUE;
        }

        SyncActiveWindowContext();
        if ((me->button == 0 && !gWebInputOpts.captureMouseLeft) ||
            (me->button == 1 && !gWebInputOpts.captureMouseMiddle) ||
            (me->button == 2 && !gWebInputOpts.captureMouseRight)) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        const NkMouseButton button =
            (me->button == 2) ? NkMouseButton::NK_MB_RIGHT :
            (me->button == 1) ? NkMouseButton::NK_MB_MIDDLE :
                                NkMouseButton::NK_MB_LEFT;

        const CanvasCoords mapped = MapCssToCanvas(CurrentCanvasSelector(), static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
        NkMouseButtonReleaseEvent event(
            button,
            mapped.x,
            mapped.y,
            static_cast<NkI32>(me->screenX),
            static_cast<NkI32>(me->screenY));
        gWASMEventSystem->Enqueue_Public(event, targetWindowId);

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnWheel(int, const EmscriptenWheelEvent* we, void*) {
        if (!gWASMEventSystem || !we) {
            return EM_TRUE;
        }

        SyncActiveWindowContext();
        if (!gWebInputOpts.captureMouseWheel) {
            return EM_FALSE;
        }

        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID) {
            return EM_FALSE;
        }

        const double vertical = -we->deltaY / 100.0;
        const double horizontal = we->deltaX / 100.0;

        if (vertical != 0.0) {
            NkMouseWheelVerticalEvent event(vertical);
            gWASMEventSystem->Enqueue_Public(event, targetWindowId);
        }
        if (horizontal != 0.0) {
            NkMouseWheelHorizontalEvent event(horizontal);
            gWASMEventSystem->Enqueue_Public(event, targetWindowId);
        }

        return EM_TRUE;
    }

    EMSCRIPTEN_KEEPALIVE void PushTouchEvents(const EmscriptenTouchEvent* te, NkTouchPhase phase) {
        if (!te || !gWASMEventSystem) {
            return;
        }

        SyncActiveWindowContext();
        const NkWindowId targetWindowId = gFocusedWindowId;
        if (targetWindowId == NK_INVALID_WINDOW_ID || !gWebInputOpts.captureTouch) {
            return;
        }

        NkTouchPoint points[NK_MAX_TOUCH_POINTS];
        NkU32 count = 0;

        for (int i = 0; i < te->numTouches && count < NK_MAX_TOUCH_POINTS; ++i) {
            const EmscriptenTouchPoint& tp = te->touches[i];
            if (!tp.isChanged && phase != NkTouchPhase::NK_TOUCH_PHASE_MOVED) {
                continue;
            }

            const CanvasCoords mapped = MapCssToCanvas(CurrentCanvasSelector(), static_cast<NkI32>(tp.targetX), static_cast<NkI32>(tp.targetY));
            NkTouchPoint point;
            point.id = static_cast<NkU64>(tp.identifier);
            point.phase = phase;
            point.clientX = static_cast<float>(mapped.x);
            point.clientY = static_cast<float>(mapped.y);
            point.screenX = static_cast<float>(tp.screenX);
            point.screenY = static_cast<float>(tp.screenY);
            points[count++] = point;
        }

        if (count == 0) {
            return;
        }

        switch (phase) {
            case NkTouchPhase::NK_TOUCH_PHASE_BEGAN: {
                NkTouchBeginEvent event(points, count);
                gWASMEventSystem->Enqueue_Public(event, targetWindowId);
                break;
            }
            case NkTouchPhase::NK_TOUCH_PHASE_MOVED: {
                NkTouchMoveEvent event(points, count);
                gWASMEventSystem->Enqueue_Public(event, targetWindowId);
                break;
            }
            case NkTouchPhase::NK_TOUCH_PHASE_ENDED: {
                NkTouchEndEvent event(points, count);
                gWASMEventSystem->Enqueue_Public(event, targetWindowId);
                break;
            }
            case NkTouchPhase::NK_TOUCH_PHASE_CANCELLED: {
                NkTouchCancelEvent event(points, count);
                gWASMEventSystem->Enqueue_Public(event, targetWindowId);
                break;
            }
            default:
                break;
        }
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnTouchStart(int, const EmscriptenTouchEvent* te, void*) {
        PushTouchEvents(te, NkTouchPhase::NK_TOUCH_PHASE_BEGAN);
        return gWebInputOpts.captureTouch ? EM_TRUE : EM_FALSE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnTouchMove(int, const EmscriptenTouchEvent* te, void*) {
        PushTouchEvents(te, NkTouchPhase::NK_TOUCH_PHASE_MOVED);
        return gWebInputOpts.captureTouch ? EM_TRUE : EM_FALSE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnTouchEnd(int, const EmscriptenTouchEvent* te, void*) {
        PushTouchEvents(te, NkTouchPhase::NK_TOUCH_PHASE_ENDED);
        return gWebInputOpts.captureTouch ? EM_TRUE : EM_FALSE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnTouchCancel(int, const EmscriptenTouchEvent* te, void*) {
        PushTouchEvents(te, NkTouchPhase::NK_TOUCH_PHASE_CANCELLED);
        return gWebInputOpts.captureTouch ? EM_TRUE : EM_FALSE;
    }

    EMSCRIPTEN_KEEPALIVE EM_BOOL OnCanvasResize(int, const EmscriptenUiEvent*, void*) {
        if (!gWASMEventSystem) {
            return EM_FALSE;
        }

        SyncActiveWindowContext();

        ForEachWASMWindow([&](NkWindow& window, NkWindowId id) {
            const char* selector = window.mData.mCanvasId.Empty() ? "#canvas" : window.mData.mCanvasId.CStr();

            int width = 0;
            int height = 0;
            emscripten_get_canvas_element_size(selector, &width, &height);
            if (width <= 0 || height <= 0) {
                return;
            }

            const NkU32 previousWidth = window.mData.mWidth;
            const NkU32 previousHeight = window.mData.mHeight;

            window.mData.mPrevWidth = previousWidth;
            window.mData.mPrevHeight = previousHeight;
            window.mData.mWidth = static_cast<NkU32>(width);
            window.mData.mHeight = static_cast<NkU32>(height);

            NkWindowResizeEvent event(
                window.mData.mWidth,
                window.mData.mHeight,
                previousWidth,
                previousHeight);
            gWASMEventSystem->Enqueue_Public(event, id);
        });

        return EM_TRUE;
    }

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mTotalEventCount = 0;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;

        gWASMEventSystem = this;
        SyncActiveWindowContext();

        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, OnKeyDown);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, OnKeyUp);
        emscripten_set_mousemove_callback(CurrentCanvasSelector(), nullptr, 1, OnMouseMove);
        emscripten_set_mousedown_callback(CurrentCanvasSelector(), nullptr, 1, OnMouseDown);
        emscripten_set_mouseup_callback(CurrentCanvasSelector(), nullptr, 1, OnMouseUp);
        emscripten_set_wheel_callback(CurrentCanvasSelector(), nullptr, 1, OnWheel);
        emscripten_set_touchstart_callback(CurrentCanvasSelector(), nullptr, 1, OnTouchStart);
        emscripten_set_touchmove_callback(CurrentCanvasSelector(), nullptr, 1, OnTouchMove);
        emscripten_set_touchend_callback(CurrentCanvasSelector(), nullptr, 1, OnTouchEnd);
        emscripten_set_touchcancel_callback(CurrentCanvasSelector(), nullptr, 1, OnTouchCancel);
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 1, OnCanvasResize);

        InstallDropBridge(CurrentCanvasSelector());

        mReady = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        if (gWASMEventSystem == this) {
            UninstallDropBridge(CurrentCanvasSelector());

            emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, nullptr);
            emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, nullptr);
            emscripten_set_mousemove_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_mousedown_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_mouseup_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_wheel_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_touchstart_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_touchmove_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_touchend_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_touchcancel_callback(CurrentCanvasSelector(), nullptr, 1, nullptr);
            emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 1, nullptr);

            gWASMEventSystem = nullptr;
            gFocusedWindowId = NK_INVALID_WINDOW_ID;
            gCanvasSelector = "#canvas";
            gWebInputOpts = {};
        }

        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.reset();
        }
        mWindowCallbacks.Clear();
        mTotalEventCount = 0;
        mPumping = false;
        mPumpThreadId = std::thread::id{};
        mReady = false;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping) {
            return;
        }

        mPumping = true;
        SyncActiveWindowContext();
        // Keep PumpOS non-blocking on Web. Cooperative yielding is handled
        // at higher level (PollEvent/PollEvents or app frame loop) to avoid
        // nested async unwinds in a single frame.
        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "WASM";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

    static NkVector<NkString> ParseDroppedNames(const char* newlineSeparated) {
        NkVector<NkString> names;
        if (!newlineSeparated || !*newlineSeparated) {
            return names;
        }

        std::istringstream stream(newlineSeparated);
        std::string item;
        while (std::getline(stream, item, '\n')) {
            if (!item.empty()) {
                names.PushBack(NkString(item.c_str()));
            }
        }
        return names;
    }

} // namespace nkentseu

extern "C" {

EMSCRIPTEN_KEEPALIVE void NkEmscriptenOnDragEnter(int x, int y, int numItems, int hasText, int hasImage) {
    if (!nkentseu::gWASMEventSystem) {
        return;
    }

    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId();
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkEmscriptenDropTarget::DispatchDragEnter(
        target,
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<nkentseu::NkU32>(numItems < 0 ? 0 : numItems),
        hasText != 0,
        hasImage != 0);
}

EMSCRIPTEN_KEEPALIVE void NkEmscriptenOnDragOver(int x, int y) {
    if (!nkentseu::gWASMEventSystem) {
        return;
    }

    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId();
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkEmscriptenDropTarget::DispatchDragOver(target, static_cast<float>(x), static_cast<float>(y));
}

EMSCRIPTEN_KEEPALIVE void NkEmscriptenOnDragLeave() {
    if (!nkentseu::gWASMEventSystem) {
        return;
    }

    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId();
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkEmscriptenDropTarget::DispatchDragLeave(target);
}

EMSCRIPTEN_KEEPALIVE void NkEmscriptenOnDropText(const char* text, int x, int y) {
    if (!nkentseu::gWASMEventSystem) {
        return;
    }

    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId();
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkEmscriptenDropTarget::DispatchDropText(
        target,
        static_cast<float>(x),
        static_cast<float>(y),
        text ? nkentseu::NkString(text) : nkentseu::NkString(),
        "text/plain");
}

EMSCRIPTEN_KEEPALIVE void NkEmscriptenOnDropFiles(const char* names, int x, int y) {
    if (!nkentseu::gWASMEventSystem) {
        return;
    }

    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId();
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkEmscriptenDropTarget::DispatchDropFiles(
        target,
        static_cast<float>(x),
        static_cast<float>(y),
        nkentseu::ParseDroppedNames(names));
}

} // extern "C"

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
