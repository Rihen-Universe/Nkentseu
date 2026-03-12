// =============================================================================
// NkWaylandEventSystem.cpp
// Wayland-specific implementation of NkEventSystem methods.
//
// Ajouts par rapport à la version précédente :
//   - UpdateCursor()           : applique ShowMouse() dans OnPointerEnter/Motion
//   - mPendingSizeRelease      : relâche les contraintes après SetSize manuel
//   - mPendingResize           : émet NkWindowResizeEvent + redimensionne le SHM buffer
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
#include "NKWindow/Platform/Wayland/NkWaylandEventSystem.h"
#include "NKWindow/Platform/Wayland/NkWaylandWindow.h"
#include "NKMemory/NkUtils.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include "NKWindow/Platform/Wayland/xdg-shell-client-protocol.h"
#if defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include("libdecor.h")
#   include "libdecor.h"
#   define NKENTSEU_HAS_LIBDECOR 1
#elif defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include(<libdecor.h>)
#   include <libdecor.h>
#   define NKENTSEU_HAS_LIBDECOR 1
#elif defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include(<libdecor-0/libdecor.h>)
#   include <libdecor-0/libdecor.h>
#   define NKENTSEU_HAS_LIBDECOR 1
#else
#   define NKENTSEU_HAS_LIBDECOR 0
#endif
#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <cstring>

namespace nkentseu {

    // =========================================================================
    // Helpers xkb
    // =========================================================================

    static NkKey WlXkbSymToNkKey(xkb_keysym_t sym) {
        return NkKeycodeMap::NkKeyFromX11KeySym(static_cast<uint32>(sym));
    }

    static NkModifierState WlBuildMods(xkb_state* state) {
        NkModifierState mods{};
        if (!state) return mods;
        mods.shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.ctrl  = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,  XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.alt   = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,   XKB_STATE_MODS_EFFECTIVE) > 0;
        mods.super = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,  XKB_STATE_MODS_EFFECTIVE) > 0;
        return mods;
    }

    // =========================================================================
    // NkWaylandSeatCtx — état du siège (clavier + pointeur)
    // =========================================================================

    struct NkWaylandSeatCtx {
        NkEventSystem* eventSystem  = nullptr;

        // XKB
        xkb_context*   xkbCtx      = nullptr;
        xkb_keymap*    xkbKeymap   = nullptr;
        xkb_state*     xkbState    = nullptr;

        // Devices
        wl_keyboard*   keyboard    = nullptr;
        wl_pointer*    pointer     = nullptr;

        // Focus clavier
        NkWindowId     focusedWindow = NK_INVALID_WINDOW_ID;

        // État pointeur
        float32          pointerX    = 0.f;
        float32          pointerY    = 0.f;
        wl_surface*    pointerSurface = nullptr;

        // Numéro de série du dernier événement pointer (pour set_cursor)
        uint32_t       pointerSerial = 0;
    };

    static NkWaylandSeatCtx* gSeatCtx = nullptr;
    static wl_seat*          gSeatObject = nullptr;
    static bool              gSeatListenerAttached = false;

    // =========================================================================
    // Resolution helpers
    // =========================================================================

    static NkWindow* ResolveWindowFromSeat(const NkWaylandSeatCtx* ctx, wl_surface* surface) {
        if (surface) {
            if (NkWindow* w = NkWaylandFindWindow(surface)) {
                return w;
            }
        }
        if (ctx && ctx->pointerSurface) {
            if (NkWindow* underPointer = NkWaylandFindWindow(ctx->pointerSurface)) {
                return underPointer;
            }
        }
        if (ctx && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
            if (NkWindow* focused = NkSystem::Instance().GetWindow(ctx->focusedWindow)) {
                return focused;
            }
        }
        return NkWaylandGetLastWindow();
    }

    static NkWindowId ResolveWindowIdFromSeat(const NkWaylandSeatCtx* ctx, wl_surface* surface) {
        NkWindow* w = ResolveWindowFromSeat(ctx, surface);
        return w ? w->GetId() : NK_INVALID_WINDOW_ID;
    }

    // =========================================================================
    // UpdateCursor — applique l'état ShowMouse() de la fenêtre sous le curseur
    // =========================================================================

    static void UpdateCursor(NkWaylandSeatCtx* ctx, NkWindow* window) {
        if (!ctx || !ctx->pointer || !window) return;

        if (window->mData.mMouseHidden) {
            // Curseur invisible : on attache nullptr à la surface curseur
            wl_pointer_set_cursor(ctx->pointer, ctx->pointerSerial, nullptr, 0, 0);
        } else {
            // Curseur système (flèche par défaut)
            if (window->mData.mCursorTheme && window->mData.mCursorSurface) {
                ::wl_cursor* cursor =
                    wl_cursor_theme_get_cursor(window->mData.mCursorTheme, "left_ptr");
                if (!cursor) {
                    cursor = wl_cursor_theme_get_cursor(window->mData.mCursorTheme, "default");
                }
                if (cursor && cursor->image_count > 0) {
                    ::wl_cursor_image*  image  = cursor->images[0];
                    ::wl_buffer*        buffer = wl_cursor_image_get_buffer(image);
                    if (buffer) {
                        wl_surface_attach(window->mData.mCursorSurface, buffer, 0, 0);
                        wl_surface_damage(window->mData.mCursorSurface, 0, 0,
                            static_cast<int32_t>(image->width),
                            static_cast<int32_t>(image->height));
                        wl_surface_commit(window->mData.mCursorSurface);
                        wl_pointer_set_cursor(
                            ctx->pointer, ctx->pointerSerial,
                            window->mData.mCursorSurface,
                            static_cast<int32_t>(image->hotspot_x),
                            static_cast<int32_t>(image->hotspot_y));
                    }
                }
            }
        }
    }

    // =========================================================================
    // Keyboard listeners
    // =========================================================================

    static void OnKeyboardKeymap(void* data, wl_keyboard*, uint32_t format, int fd, uint32_t size) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
        if (!ctx->xkbCtx) { close(fd); return; }

        char* raw = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
        close(fd);
        if (raw == MAP_FAILED) return;

        if (ctx->xkbKeymap) { xkb_keymap_unref(ctx->xkbKeymap); ctx->xkbKeymap = nullptr; }
        if (ctx->xkbState)  { xkb_state_unref(ctx->xkbState);   ctx->xkbState  = nullptr; }

        ctx->xkbKeymap = xkb_keymap_new_from_string(ctx->xkbCtx, raw,
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(raw, size);

        if (ctx->xkbKeymap) ctx->xkbState = xkb_state_new(ctx->xkbKeymap);
    }

    static void OnKeyboardEnter(void* data, wl_keyboard*, uint32_t,
                                wl_surface* surface, wl_array*) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) return;

        ctx->focusedWindow = ResolveWindowIdFromSeat(ctx, surface);

        if (ctx->eventSystem && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
            NkWindowFocusGainedEvent e;
            ctx->eventSystem->Enqueue_Public(e, ctx->focusedWindow);
        }
    }

    static void OnKeyboardLeave(void* data, wl_keyboard*, uint32_t, wl_surface*) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) return;

        if (ctx->eventSystem && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
            NkWindowFocusLostEvent e;
            ctx->eventSystem->Enqueue_Public(e, ctx->focusedWindow);
        }
        ctx->focusedWindow = NK_INVALID_WINDOW_ID;
    }

    static void OnKeyboardKey(void* data, wl_keyboard*, uint32_t, uint32_t,
                              uint32_t key, uint32_t state) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) return;

        const xkb_keycode_t keycode = static_cast<xkb_keycode_t>(key + 8);
        const xkb_keysym_t  sym     = ctx->xkbState
                                    ? xkb_state_key_get_one_sym(ctx->xkbState, keycode)
                                    : XKB_KEY_NoSymbol;

        const NkScancode scLinux = NkScancodeFromLinux(key);
        const NkScancode scX     = NkScancodeFromXKeycode(key);
        NkScancode       sc      = (scLinux != NkScancode::NK_SC_UNKNOWN) ? scLinux : scX;
        // Prefer keysym (layout-aware, robust across compositor keycode quirks),
        // then fall back to scancode mapping.
        NkKey      nkKey = WlXkbSymToNkKey(sym);
        if (nkKey == NkKey::NK_UNKNOWN) nkKey = NkScancodeToKey(sc);
        if (nkKey == NkKey::NK_UNKNOWN && scX != NkScancode::NK_SC_UNKNOWN) {
            nkKey = NkScancodeToKey(scX);
            if (nkKey != NkKey::NK_UNKNOWN) {
                sc = scX;
            }
        }
        if (sym == XKB_KEY_Escape || sc == NkScancode::NK_SC_ESCAPE) {
            nkKey = NkKey::NK_ESCAPE;
        }

        const NkModifierState mods  = WlBuildMods(ctx->xkbState);
        NkWindowId            winId = ResolveWindowIdFromSeat(ctx, nullptr);
        if (winId != NK_INVALID_WINDOW_ID) {
            ctx->focusedWindow = winId;
        }

        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            NkKeyPressEvent e(nkKey, sc, mods, static_cast<uint32>(sym));
            ctx->eventSystem->Enqueue_Public(e, winId);

            // Génération de l'événement texte
            if (ctx->xkbState) {
                char buf[32] = {};
                const int len = xkb_state_key_get_utf8(ctx->xkbState, keycode, buf, sizeof(buf));
                if (len > 0 && static_cast<unsigned char>(buf[0]) >= 0x20
                            && static_cast<unsigned char>(buf[0]) != 0x7F) {
                    NkTextInputEvent te(static_cast<uint32>(static_cast<unsigned char>(buf[0])));
                    ctx->eventSystem->Enqueue_Public(te, winId);
                }
            }
        } else {
            NkKeyReleaseEvent e(nkKey, sc, mods, static_cast<uint32>(sym));
            ctx->eventSystem->Enqueue_Public(e, winId);
        }
    }

    static void OnKeyboardMods(void* data, wl_keyboard*, uint32_t,
                               uint32_t depressed, uint32_t latched,
                               uint32_t locked,    uint32_t group) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->xkbState) return;
        xkb_state_update_mask(ctx->xkbState, depressed, latched, locked, 0, 0, group);
    }

    static void OnKeyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

    static const wl_keyboard_listener kKeyboardListener = {
        OnKeyboardKeymap,
        OnKeyboardEnter,
        OnKeyboardLeave,
        OnKeyboardKey,
        OnKeyboardMods,
        OnKeyboardRepeatInfo,
    };

    // =========================================================================
    // Pointer listeners
    // =========================================================================

    static NkWindowId FindWindowForSurface(const NkWaylandSeatCtx* ctx, wl_surface* surface) {
        return ResolveWindowIdFromSeat(ctx, surface);
    }

    static void OnPointerEnter(void* data, wl_pointer*, uint32_t serial,
                               wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) return;

        ctx->pointerSerial  = serial;
        ctx->pointerSurface = surface;
        ctx->pointerX       = static_cast<float32>(wl_fixed_to_double(sx));
        ctx->pointerY       = static_cast<float32>(wl_fixed_to_double(sy));

        // Applique le curseur de la fenêtre (visible ou caché)
        NkWindow* window = ResolveWindowFromSeat(ctx, surface);
        UpdateCursor(ctx, window);

        if (ctx->eventSystem) {
            NkMouseEnterEvent e;
            ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(ctx, surface));
        }
    }

    static void OnPointerLeave(void* data, wl_pointer*, uint32_t,
                               wl_surface* surface) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) return;

        if (ctx->eventSystem) {
            NkMouseLeaveEvent e;
            ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(ctx, surface));
        }
        ctx->pointerSurface = nullptr;
    }

    static void OnPointerMotion(void* data, wl_pointer*, uint32_t,
                                wl_fixed_t sx, wl_fixed_t sy) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) return;

        const int32 x  = static_cast<int32>(wl_fixed_to_double(sx));
        const int32 y  = static_cast<int32>(wl_fixed_to_double(sy));
        const int32 dx = x - static_cast<int32>(ctx->pointerX);
        const int32 dy = y - static_cast<int32>(ctx->pointerY);
        ctx->pointerX  = static_cast<float32>(x);
        ctx->pointerY  = static_cast<float32>(y);

        // Si le curseur doit être caché, on le réapplique à chaque mouvement
        // (certains compositeurs le réinitialisent après un enter)
        NkWindow* window = ResolveWindowFromSeat(ctx, ctx->pointerSurface);
        if (window && window->mData.mMouseHidden) {
            UpdateCursor(ctx, window);
        }

        NkMouseMoveEvent e(x, y, x, y, dx, dy);
        ctx->eventSystem->Enqueue_Public(e, FindWindowForSurface(ctx, ctx->pointerSurface));
    }

    static void OnPointerButton(void* data, wl_pointer*, uint32_t, uint32_t,
                                uint32_t button, uint32_t state) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) return;

        const int32 x       = static_cast<int32>(ctx->pointerX);
        const int32 y       = static_cast<int32>(ctx->pointerY);
        const bool  pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

        NkMouseButton mapped;
        switch (button) {
            case BTN_RIGHT:  mapped = NkMouseButton::NK_MB_RIGHT;   break;
            case BTN_MIDDLE: mapped = NkMouseButton::NK_MB_MIDDLE;  break;
            case BTN_SIDE:   mapped = NkMouseButton::NK_MB_BACK;    break;
            case BTN_EXTRA:  mapped = NkMouseButton::NK_MB_FORWARD; break;
            default:         mapped = NkMouseButton::NK_MB_LEFT;    break;
        }

        const NkWindowId winId = FindWindowForSurface(ctx, ctx->pointerSurface);
        if (pressed) {
            NkMouseButtonPressEvent e(mapped, x, y, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        } else {
            NkMouseButtonReleaseEvent e(mapped, x, y, x, y);
            ctx->eventSystem->Enqueue_Public(e, winId);
        }
    }

    static void OnPointerAxis(void* data, wl_pointer*, uint32_t,
                              uint32_t axis, wl_fixed_t value) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx || !ctx->eventSystem) return;

        const int32      x     = static_cast<int32>(ctx->pointerX);
        const int32      y     = static_cast<int32>(ctx->pointerY);
        const double     v     = wl_fixed_to_double(value);
        const NkWindowId winId = FindWindowForSurface(ctx, ctx->pointerSurface);

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
        OnPointerEnter,
        OnPointerLeave,
        OnPointerMotion,
        OnPointerButton,
        OnPointerAxis,
        OnPointerFrame,
        OnPointerAxisSource,
        OnPointerAxisStop,
        OnPointerAxisDiscrete,
    };

    // =========================================================================
    // Seat listener
    // =========================================================================

    static void OnSeatCapabilities(void* data, wl_seat* seat, uint32_t caps) {
        auto* ctx = static_cast<NkWaylandSeatCtx*>(data);
        if (!ctx) return;

        const bool hasKeyboard = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        const bool hasPointer  = (caps & WL_SEAT_CAPABILITY_POINTER)  != 0;

        if (hasKeyboard && !ctx->keyboard) {
            if (!ctx->xkbCtx) {
                ctx->xkbCtx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
            }
            ctx->keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(ctx->keyboard, &kKeyboardListener, ctx);
        } else if (!hasKeyboard && ctx->keyboard) {
            if (ctx->eventSystem && ctx->focusedWindow != NK_INVALID_WINDOW_ID) {
                NkWindowFocusLostEvent e;
                ctx->eventSystem->Enqueue_Public(e, ctx->focusedWindow);
            }
            ctx->focusedWindow = NK_INVALID_WINDOW_ID;
            wl_keyboard_destroy(ctx->keyboard);
            ctx->keyboard = nullptr;
        }

        if (hasPointer && !ctx->pointer) {
            ctx->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(ctx->pointer, &kPointerListener, ctx);
        } else if (!hasPointer && ctx->pointer) {
            wl_pointer_destroy(ctx->pointer);
            ctx->pointer = nullptr;
            ctx->pointerSurface = nullptr;
        }
    }

    static void OnSeatName(void*, wl_seat*, const char*) {}

    static const wl_seat_listener kSeatListener = {
        OnSeatCapabilities,
        OnSeatName,
    };

    void NkWaylandAttachSeatListener(NkEventSystem* eventSystem, wl_seat* seat) {
        if (!eventSystem || !seat) return;

        if (!gSeatCtx) {
            gSeatCtx = new NkWaylandSeatCtx();
        }
        gSeatCtx->eventSystem = eventSystem;
        if (!gSeatCtx->xkbCtx) {
            gSeatCtx->xkbCtx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        }

        if (gSeatObject == seat && gSeatListenerAttached) {
            return;
        }

        if (wl_seat_add_listener(seat, &kSeatListener, gSeatCtx) == 0) {
            gSeatObject = seat;
            gSeatListenerAttached = true;
            return;
        }

        if (gSeatObject == seat) {
            gSeatListenerAttached = true;
        }
    }

    void NkWaylandNotifySeatDestroy(wl_seat* seat) {
        if (!seat || !gSeatCtx) return;
        if (gSeatObject != seat) return;

        // The seat parent is going away. Child proxies must not outlive it.
        if (gSeatCtx->keyboard) {
            wl_keyboard_destroy(gSeatCtx->keyboard);
            gSeatCtx->keyboard = nullptr;
        }
        if (gSeatCtx->pointer) {
            wl_pointer_destroy(gSeatCtx->pointer);
            gSeatCtx->pointer = nullptr;
        }
        gSeatCtx->focusedWindow = NK_INVALID_WINDOW_ID;
        gSeatCtx->pointerSurface = nullptr;
        gSeatObject = nullptr;
        gSeatListenerAttached = false;
    }

    // =========================================================================
    // Pump non-bloquant
    // =========================================================================

    static void PumpWaylandDisplayNonBlocking(wl_display* display) {
        if (!display) return;

        const int fd = wl_display_get_fd(display);
        if (fd < 0) {
            wl_display_dispatch_pending(display);
            wl_display_flush(display);
            return;
        }

        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) < 0) return;
        }
        wl_display_flush(display);

        pollfd pfd{};
        pfd.fd     = fd;
        pfd.events = POLLIN;
        const int ready = poll(&pfd, 1, 0);

        if (ready > 0 && (pfd.revents & POLLIN)) {
            if (wl_display_read_events(display) < 0) {
                wl_display_cancel_read(display);
                return;
            }
            wl_display_dispatch_pending(display);
        } else {
            wl_display_cancel_read(display);
            if (ready > 0 && (pfd.revents & (POLLERR | POLLHUP))) {
                wl_display_dispatch_pending(display);
            }
        }
    }

    // =========================================================================
    // Redimensionnement du SHM buffer après un resize
    // =========================================================================

    static void ResizeShmBuffer(NkWindow* window) {
        NkWindowData& d = window->mData;

        if (!d.mShm || d.mWidth == 0 || d.mHeight == 0) return;

        // Libère l'ancien buffer
        if (d.mPixels && d.mBuffer) {
            munmap(d.mPixels, static_cast<size_t>(d.mStride) * d.mPrevHeight);
            d.mPixels = nullptr;
        }
        if (d.mBuffer) {
            wl_buffer_destroy(d.mBuffer);
            d.mBuffer = nullptr;
        }

        // Crée le nouveau buffer aux nouvelles dimensions
        d.mStride = d.mWidth * 4;
        d.mBuffer = nullptr; // sera réassigné par CreateShmBuffer

        // On réutilise CreateShmBuffer depuis NkWaylandWindow.cpp
        // via la surface desc (on passe par les membres publics).
        // Pour éviter de re-déclarer la fonction ici, on la redéfinit localement.
        const size_t size = static_cast<size_t>(d.mStride) * d.mHeight;
        char path[] = "/tmp/nk-wl-rsz-XXXXXX";
        const int fd = mkstemp(path);
        if (fd < 0) return;
        unlink(path);
        if (ftruncate(fd, static_cast<off_t>(size)) < 0) { close(fd); return; }

        void* pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (pixels == MAP_FAILED) { close(fd); return; }

        ::wl_shm_pool* pool = wl_shm_create_pool(d.mShm, fd, static_cast<int32_t>(size));
        ::wl_buffer*   buf  = wl_shm_pool_create_buffer(pool, 0,
            static_cast<int32_t>(d.mWidth), static_cast<int32_t>(d.mHeight),
            static_cast<int32_t>(d.mStride), WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        d.mPixels = pixels;
        d.mBuffer = buf;

        // Attache et commit si la fenêtre est visible
        if (d.mVisible && d.mSurface && d.mConfigured) {
            memory::NkMemSet(d.mPixels, 0, size);
            wl_surface_attach(d.mSurface, d.mBuffer, 0, 0);
            wl_surface_damage(d.mSurface, 0, 0,
                static_cast<int32_t>(d.mWidth),
                static_cast<int32_t>(d.mHeight));
            wl_surface_commit(d.mSurface);
        }
    }

    // =========================================================================
    // NkEventSystem::Init / Shutdown
    // =========================================================================

    bool NkEventSystem::Init() {
        if (mReady) return true;

        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;
        mReady   = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        if (gSeatCtx) {
            const bool canDestroySeatChildren = (gSeatObject != nullptr);
            if (gSeatCtx->keyboard) {
                if (canDestroySeatChildren) {
                    wl_keyboard_destroy(gSeatCtx->keyboard);
                }
                gSeatCtx->keyboard = nullptr;
            }
            if (gSeatCtx->pointer)  {
                if (canDestroySeatChildren) {
                    wl_pointer_destroy(gSeatCtx->pointer);
                }
                gSeatCtx->pointer  = nullptr;
            }
            if (gSeatCtx->xkbState)  { xkb_state_unref(gSeatCtx->xkbState);   gSeatCtx->xkbState  = nullptr; }
            if (gSeatCtx->xkbKeymap) { xkb_keymap_unref(gSeatCtx->xkbKeymap); gSeatCtx->xkbKeymap = nullptr; }
            if (gSeatCtx->xkbCtx)    { xkb_context_unref(gSeatCtx->xkbCtx);   gSeatCtx->xkbCtx    = nullptr; }
            delete gSeatCtx;
            gSeatCtx = nullptr;
        }
        gSeatObject = nullptr;
        gSeatListenerAttached = false;

        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.Reset();
        }
        mWindowCallbacks.Clear();
        mTotalEventCount = 0;
        mPumping         = false;
        mPumpThreadId    = 0;
        mReady           = false;
    }

    // =========================================================================
    // PumpOS — cœur de la boucle d'événements Wayland
    // =========================================================================

    void NkEventSystem::PumpOS() {
        if (mPumping) return;
        mPumping = true;

        const uint32 windowCount = NkSystem::Instance().GetWindowCount();

        for (uint32 i = 0; i < windowCount; ++i) {
            NkWindow* window = NkSystem::Instance().GetWindowAt(i);
            if (!window) continue;

            wl_display* display = window->mData.mDisplay;
            if (!display)      continue;

            // ------------------------------------------------------------------
            // Initialisation du siège (une seule fois, au premier display actif)
            // ------------------------------------------------------------------
            if (window->mData.mSeat) {
                NkWaylandAttachSeatListener(this, window->mData.mSeat);
            } else if (gSeatCtx) {
                gSeatCtx->eventSystem = this;
            }

            // ------------------------------------------------------------------
            // Événement de fermeture demandée par le compositeur
            // ------------------------------------------------------------------
            if (window->mData.mWantsClose) {
                window->mData.mWantsClose = false;
                NkWindowCloseEvent e(false);
                Enqueue(e, window->GetId());
            }

            // ------------------------------------------------------------------
            // Pump Wayland non-bloquant
            // ------------------------------------------------------------------
            PumpWaylandDisplayNonBlocking(display);
#if NKENTSEU_HAS_LIBDECOR
            if (window->mData.mUsingLibdecor && window->mData.mLibdecor) {
                (void)libdecor_dispatch(window->mData.mLibdecor, 0);
            }
#endif

            // ------------------------------------------------------------------
            // Redimensionnement : émet l'événement + redimensionne le SHM buffer
            // ------------------------------------------------------------------
            if (window->mData.mPendingResize) {
                window->mData.mPendingResize = false;

                // Redimensionne le buffer logiciel si utilisé
                ResizeShmBuffer(window);

                NkWindowResizeEvent e(
                    window->mData.mWidth,
                    window->mData.mHeight,
                    window->mData.mPrevWidth,
                    window->mData.mPrevHeight);
                Enqueue(e, window->GetId());

                window->mData.mPrevWidth  = window->mData.mWidth;
                window->mData.mPrevHeight = window->mData.mHeight;
            }

            // ------------------------------------------------------------------
            // Relâchement des contraintes après un SetSize() manuel
            //
            // On attend que mConfigured soit true (le configure a été acquitté)
            // avant de relâcher min/max pour ne pas créer une boucle configure.
            // ------------------------------------------------------------------
            if (window->mData.mPendingSizeRelease && window->mData.mConfigured) {
                window->mData.mPendingSizeRelease = false;

                const int32_t minW = static_cast<int32_t>(window->mData.mPendingMinW);
                const int32_t minH = static_cast<int32_t>(window->mData.mPendingMinH);
                const int32_t maxW = static_cast<int32_t>(window->mData.mPendingMaxW);
                const int32_t maxH = static_cast<int32_t>(window->mData.mPendingMaxH);

#if NKENTSEU_HAS_LIBDECOR
                if (window->mData.mUsingLibdecor && window->mData.mLibdecorFrame) {
                    libdecor_frame_set_min_content_size(window->mData.mLibdecorFrame, minW, minH);
                    libdecor_frame_set_max_content_size(window->mData.mLibdecorFrame, maxW, maxH);
                    wl_surface_commit(window->mData.mSurface);
                    wl_display_flush(display);
                } else
#endif
                if (window->mData.mXdgToplevel) {
                    xdg_toplevel_set_min_size(window->mData.mXdgToplevel,
                        minW, minH);
                    xdg_toplevel_set_max_size(window->mData.mXdgToplevel,
                        maxW, maxH);
                    wl_surface_commit(window->mData.mSurface);
                    wl_display_flush(display);
                }
            }
        }

        mPumping = false;
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Wayland";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
