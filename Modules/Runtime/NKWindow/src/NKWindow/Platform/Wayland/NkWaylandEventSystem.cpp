// =============================================================================
// NkWaylandEventSystem.cpp
// Wayland-specific implementation of NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkKeyboardEvent.h"
#include "NKWindow/Events/NkMouseEvent.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkKeycodeMap.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Platform/Wayland/NkWaylandWindow.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <mutex>
#include <thread>

namespace nkentseu {

    static NkKey WlXkbSymToNkKey(xkb_keysym_t sym) {
        return NkKeycodeMap::NkKeyFromX11KeySym(static_cast<NkU32>(sym));
    }

    static NkModifierState WlBuildMods(xkb_state* state) {
        NkModifierState mods{};
        if (!state) {
            return mods;
        }

        mods.shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.alt = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.super = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0;
        return mods;
    }

    struct NkWaylandSeatCtx {
        NkEventSystem* eventSystem = nullptr;
        xkb_context*   xkbCtx = nullptr;
        xkb_keymap*    xkbKeymap = nullptr;
        xkb_state*     xkbState = nullptr;
        wl_keyboard*   keyboard = nullptr;
        wl_pointer*    pointer = nullptr;

        NkWindowId focusedWindow = NK_INVALID_WINDOW_ID;
        NkF32 pointerX = 0.f;
        NkF32 pointerY = 0.f;
        wl_surface* pointerSurface = nullptr;
    };

    static NkWaylandSeatCtx* gSeatCtx = nullptr;

    static void OnKeyboardKeymap(void* data, wl_keyboard*, uint32_t format, int fd, uint32_t size) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            close(fd);
            return;
        }
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }

        char* raw = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
        close(fd);
        if (raw == MAP_FAILED) {
            return;
        }

        if (ctx->xkbKeymap) {
            xkb_keymap_unref(ctx->xkbKeymap);
            ctx->xkbKeymap = nullptr;
        }
        if (ctx->xkbState) {
            xkb_state_unref(ctx->xkbState);
            ctx->xkbState = nullptr;
        }

        ctx->xkbKeymap = xkb_keymap_new_from_string(
            ctx->xkbCtx,
            raw,
            XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(raw, size);

        if (ctx->xkbKeymap) {
            ctx->xkbState = xkb_state_new(ctx->xkbKeymap);
        }
    }

    static void OnKeyboardEnter(void* data, wl_keyboard*, uint32_t, wl_surface* surface, wl_array*) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            return;
        }

        NkWindow* window = NkWaylandFindWindow(surface);
        ctx->focusedWindow = window ? window->GetId() : NK_INVALID_WINDOW_ID;

        if (ctx->eventSystem && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
            NkWindowFocusGainedEvent e;
            ctx->eventSystem->Enqueue_Public(e, ctx->focusedWindow);
        }
    }

    static void OnKeyboardLeave(void* data, wl_keyboard*, uint32_t, wl_surface*) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            return;
        }

        if (ctx->eventSystem && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
            NkWindowFocusLostEvent e;
            ctx->eventSystem->Enqueue_Public(e, ctx->focusedWindow);
        }
        ctx->focusedWindow = NK_INVALID_WINDOW_ID;
    }

    static void OnKeyboardKey(void* data,
                              wl_keyboard*,
                              uint32_t,
                              uint32_t,
                              uint32_t key,
                              uint32_t state) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem || !ctx->xkbState) {
            return;
        }

        const xkb_keycode_t keycode = static_cast<xkb_keycode_t>(key + 8);
        const xkb_keysym_t sym = xkb_state_key_get_one_sym(ctx->xkbState, keycode);

        NkScancode sc = NkScancodeFromLinux(key);
        NkKey nkKey = NkScancodeToKey(sc);
        if (nkKey == NkKey::NK_UNKNOWN) {
            nkKey = WlXkbSymToNkKey(sym);
        }

        const NkModifierState mods = WlBuildMods(ctx->xkbState);
        const NkWindowId winId = ctx->focusedWindow;

        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            NkKeyPressEvent e(nkKey, sc, mods, static_cast<NkU32>(sym));
            ctx->eventSystem->Enqueue_Public(e, winId);

            char buffer[32] = {};
            const int len = xkb_state_key_get_utf8(ctx->xkbState, keycode, buffer, sizeof(buffer));
            if (len > 0 && static_cast<unsigned char>(buffer[0]) >= 0x20 &&
                static_cast<unsigned char>(buffer[0]) != 0x7F) {
                NkTextInputEvent te(static_cast<NkU32>(static_cast<unsigned char>(buffer[0])));
                ctx->eventSystem->Enqueue_Public(te, winId);
            }
        } else {
            NkKeyReleaseEvent e(nkKey, sc, mods, static_cast<NkU32>(sym));
            ctx->eventSystem->Enqueue_Public(e, winId);
        }
    }

    static void OnKeyboardMods(void* data,
                               wl_keyboard*,
                               uint32_t,
                               uint32_t depressed,
                               uint32_t latched,
                               uint32_t locked,
                               uint32_t group) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->xkbState) {
            return;
        }

        xkb_state_update_mask(ctx->xkbState, depressed, latched, locked, 0, 0, group);
    }

    static void OnKeyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

    static const wl_keyboard_listener kKeyboardListener = {
        .keymap = OnKeyboardKeymap,
        .enter = OnKeyboardEnter,
        .leave = OnKeyboardLeave,
        .key = OnKeyboardKey,
        .modifiers = OnKeyboardMods,
        .repeat_info = OnKeyboardRepeatInfo,
    };

    static NkWindowId FindWindowForSurface(wl_surface* surface) {
        NkWindow* window = NkWaylandFindWindow(surface);
        return window ? window->GetId() : NK_INVALID_WINDOW_ID;
    }

    static void OnPointerEnter(void* data,
                               wl_pointer*,
                               uint32_t,
                               wl_surface* surface,
                               wl_fixed_t sx,
                               wl_fixed_t sy) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            return;
        }

        ctx->pointerSurface = surface;
        ctx->pointerX = static_cast<NkF32>(wl_fixed_to_double(sx));
        ctx->pointerY = static_cast<NkF32>(wl_fixed_to_double(sy));

        if (ctx->eventSystem) {
            NkMouseEnterEvent e;
            ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(surface));
        }
    }

    static void OnPointerLeave(void* data, wl_pointer*, uint32_t, wl_surface* surface) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            return;
        }

        if (ctx->eventSystem) {
            NkMouseLeaveEvent e;
            ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(surface));
        }

        ctx->pointerSurface = nullptr;
    }

    static void OnPointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) {
            return;
        }

        const NkI32 x = static_cast<NkI32>(wl_fixed_to_double(sx));
        const NkI32 y = static_cast<NkI32>(wl_fixed_to_double(sy));
        const NkI32 dx = x - static_cast<NkI32>(ctx->pointerX);
        const NkI32 dy = y - static_cast<NkI32>(ctx->pointerY);

        ctx->pointerX = static_cast<NkF32>(x);
        ctx->pointerY = static_cast<NkF32>(y);

        NkMouseMoveEvent e(x, y, x, y, dx, dy);
        ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(ctx->pointerSurface));
    }

    static void OnPointerButton(void* data,
                                wl_pointer*,
                                uint32_t,
                                uint32_t,
                                uint32_t button,
                                uint32_t state) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) {
            return;
        }

        const NkI32 x = static_cast<NkI32>(ctx->pointerX);
        const NkI32 y = static_cast<NkI32>(ctx->pointerY);
        const bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

        NkMouseButton mapped = NkMouseButton::NK_MB_LEFT;
        switch (button) {
            case BTN_RIGHT:  mapped = NkMouseButton::NK_MB_RIGHT; break;
            case BTN_MIDDLE: mapped = NkMouseButton::NK_MB_MIDDLE; break;
            case BTN_SIDE:   mapped = NkMouseButton::NK_MB_BACK; break;
            case BTN_EXTRA:  mapped = NkMouseButton::NK_MB_FORWARD; break;
            default:         mapped = NkMouseButton::NK_MB_LEFT; break;
        }

        const NkWindowId winId = FindWindowForSurface(ctx->pointerSurface);
        if (pressed) {
            NkMouseButtonPressEvent e(mapped, x, y, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        } else {
            NkMouseButtonReleaseEvent e(mapped, x, y, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        }
    }

    static void OnPointerAxis(void* data,
                              wl_pointer*,
                              uint32_t,
                              uint32_t axis,
                              wl_fixed_t value) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) {
            return;
        }

        const NkI32 x = static_cast<NkI32>(ctx->pointerX);
        const NkI32 y = static_cast<NkI32>(ctx->pointerY);
        const double v = wl_fixed_to_double(value);
        const NkWindowId winId = FindWindowForSurface(ctx->pointerSurface);

        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
            NkMouseWheelVerticalEvent e(-v / 10.0, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        } else {
            NkMouseWheelHorizontalEvent e(-v / 10.0, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        }
    }

    static void OnPointerFrame(void*, wl_pointer*) {}
    static void OnPointerAxisSource(void*, wl_pointer*, uint32_t) {}
    static void OnPointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
    static void OnPointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}

    static const wl_pointer_listener kPointerListener = {
        .enter = OnPointerEnter,
        .leave = OnPointerLeave,
        .motion = OnPointerMotion,
        .button = OnPointerButton,
        .axis = OnPointerAxis,
        .frame = OnPointerFrame,
        .axis_source = OnPointerAxisSource,
        .axis_stop = OnPointerAxisStop,
        .axis_discrete = OnPointerAxisDiscrete,
    };

    static void OnSeatCapabilities(void* data, wl_seat* seat, uint32_t caps) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) {
            return;
        }

        const bool hasKeyboard = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        const bool hasPointer = (caps & WL_SEAT_CAPABILITY_POINTER) != 0;

        if (hasKeyboard && !ctx->keyboard) {
            ctx->keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(ctx->keyboard, &kKeyboardListener, ctx);
        } else if (!hasKeyboard && ctx->keyboard) {
            wl_keyboard_destroy(ctx->keyboard);
            ctx->keyboard = nullptr;
        }

        if (hasPointer && !ctx->pointer) {
            ctx->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(ctx->pointer, &kPointerListener, ctx);
        } else if (!hasPointer && ctx->pointer) {
            wl_pointer_destroy(ctx->pointer);
            ctx->pointer = nullptr;
        }
    }

    static void OnSeatName(void*, wl_seat*, const char*) {}

    static const wl_seat_listener kSeatListener = {
        .capabilities = OnSeatCapabilities,
        .name = OnSeatName,
    };

    static void PumpWaylandDisplayNonBlocking(wl_display* display) {
        if (!display) {
            return;
        }

        const int fd = wl_display_get_fd(display);
        if (fd < 0) {
            wl_display_dispatch_pending(display);
            wl_display_flush(display);
            return;
        }

        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) < 0) {
                return;
            }
        }

        wl_display_flush(display);

        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;

        const int ready = poll(&pfd, 1, 0);
        if (ready > 0 && (pfd.revents & POLLIN) != 0) {
            if (wl_display_read_events(display) < 0) {
                wl_display_cancel_read(display);
                return;
            }
            wl_display_dispatch_pending(display);
        } else {
            wl_display_cancel_read(display);
            if (ready > 0 && (pfd.revents & (POLLERR | POLLHUP)) != 0) {
                wl_display_dispatch_pending(display);
            }
        }
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
        mReady = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        if (gSeatCtx) {
            if (gSeatCtx->keyboard) {
                wl_keyboard_destroy(gSeatCtx->keyboard);
                gSeatCtx->keyboard = nullptr;
            }
            if (gSeatCtx->pointer) {
                wl_pointer_destroy(gSeatCtx->pointer);
                gSeatCtx->pointer = nullptr;
            }
            if (gSeatCtx->xkbState) {
                xkb_state_unref(gSeatCtx->xkbState);
                gSeatCtx->xkbState = nullptr;
            }
            if (gSeatCtx->xkbKeymap) {
                xkb_keymap_unref(gSeatCtx->xkbKeymap);
                gSeatCtx->xkbKeymap = nullptr;
            }
            if (gSeatCtx->xkbCtx) {
                xkb_context_unref(gSeatCtx->xkbCtx);
                gSeatCtx->xkbCtx = nullptr;
            }
            delete gSeatCtx;
            gSeatCtx = nullptr;
        }

        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.reset();
        }

        mWindowCallbacks.clear();
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

        const NkU32 windowCount = NkSystem::Instance().GetWindowCount();
        for (NkU32 i = 0; i < windowCount; ++i) {
            NkWindow* window = NkSystem::Instance().GetWindowAt(i);
            if (!window) {
                continue;
            }

            wl_display* display = window->mData.mDisplay;
            if (!display) {
                continue;
            }

            if (!gSeatCtx && window->mData.mSeat) {
                gSeatCtx = new NkWaylandSeatCtx();
                gSeatCtx->eventSystem = this;
                gSeatCtx->xkbCtx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
                wl_seat_add_listener(window->mData.mSeat, &kSeatListener, gSeatCtx);
            }

            if (window->mData.mWantsClose) {
                window->mData.mWantsClose = false;
                NkWindowCloseEvent e(false);
                Enqueue(e, window->GetId());
            }

            PumpWaylandDisplayNonBlocking(display);

            if (window->mData.mPendingResize) {
                window->mData.mPendingResize = false;
                NkWindowResizeEvent e(
                    window->mData.mWidth,
                    window->mData.mHeight,
                    window->mData.mPrevWidth,
                    window->mData.mPrevHeight);
                Enqueue(e, window->GetId());
                window->mData.mPrevWidth = window->mData.mWidth;
                window->mData.mPrevHeight = window->mData.mHeight;
            }

        }

        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Wayland";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
