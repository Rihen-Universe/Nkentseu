// =============================================================================
// NkXCBEventSystem.cpp Ã¢â‚¬â€ XCB implementation of platform-specific methods
//
// MÃƒÂ©thodes implÃƒÂ©mentÃƒÂ©es ici :
//   Init, Shutdown (cross-plat dans NkEventSystem.cpp), PumpOS, GetPlatformName
//
// Pattern identique ÃƒÂ  NkWin32EventSystem.cpp :
//   PumpOS() pompe les ÃƒÂ©vÃƒÂ©nements XCB et appelle Enqueue(evt, winId)
//   qui gÃƒÂ¨re le ring buffer, les prioritÃƒÂ©s et les callbacks.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkKeycodeMap.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Platform/XCB/NkXCBWindow.h"
#include "NKWindow/Platform/XCB/NkXCBDropTarget.h"

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>

namespace nkentseu {

    // =============================================================================
    // Helpers
    // =============================================================================

    static NkModifierState XCBMods(uint16_t state) {
        NkModifierState m;
        m.shift   = !!(state & XCB_MOD_MASK_SHIFT);
        m.ctrl    = !!(state & XCB_MOD_MASK_CONTROL);
        m.alt     = !!(state & XCB_MOD_MASK_1);   // Mod1 = Alt
        m.super   = !!(state & XCB_MOD_MASK_4);   // Mod4 = Super/Win
        m.capLock = !!(state & XCB_MOD_MASK_LOCK);
        m.numLock = !!(state & XCB_MOD_MASK_2);   // Mod2 = NumLock
        return m;
    }

    // =============================================================================
    // NkEventSystem Ã¢â‚¬â€ mÃƒÂ©thodes platform-spÃƒÂ©cifiques XCB
    // =============================================================================

    bool NkEventSystem::Init() {
        if (mReady) return true;
        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;

        // Initialiser XKB si la connexion XCB existe dÃƒÂ©jÃƒÂ 
        xcb_connection_t* conn = NkXCBGetConnection();
        if (conn) {
            if (!mData.mXkbContext)
                mData.mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
            if (mData.mXkbContext && !mData.mXkbKeymap) {
                mData.mDefaultScreen = 0;
                mData.mXkbKeymap = xkb_x11_keymap_new_from_device(
                    mData.mXkbContext, conn,
                    xkb_x11_get_core_keyboard_device_id(conn),
                    XKB_KEYMAP_COMPILE_NO_FLAGS);
                if (mData.mXkbKeymap)
                    mData.mXkbState = xkb_x11_state_new_from_device(
                        mData.mXkbKeymap, conn,
                        xkb_x11_get_core_keyboard_device_id(conn));
            }
        }

        mReady = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.Reset();
        }
        mWindowCallbacks.Clear();

        if (mData.mXkbState) {
            xkb_state_unref(mData.mXkbState);
            mData.mXkbState = nullptr;
        }
        if (mData.mXkbKeymap) {
            xkb_keymap_unref(mData.mXkbKeymap);
            mData.mXkbKeymap = nullptr;
        }
        if (mData.mXkbContext) {
            xkb_context_unref(mData.mXkbContext);
            mData.mXkbContext = nullptr;
        }

        mData.mConnection = nullptr;
        mData.mDefaultScreen = 0;
        mData.mInitialized = false;
        mTotalEventCount = 0;
        mPumping = false;
        mPumpThreadId = 0;
        mReady = false;
    }


    void NkEventSystem::PumpOS() {
        if (mPumping) return;
        mPumping = true;

        xcb_connection_t* conn = NkXCBGetConnection();
        if (!conn) {
            mPumping = false;
            return;
        }

        const xcb_atom_t wmProtocols = NkXCBGetWmProtocolsAtom();
        const xcb_atom_t wmDeleteWindow = NkXCBGetWmDeleteWindowAtom();

        // Ãƒâ€°tat souris prÃƒÂ©cÃƒÂ©dent pour calcul delta
        static int32 sPrevX = 0, sPrevY = 0;

        xcb_generic_event_t* ev;
        while ((ev = xcb_poll_for_event(conn)) != nullptr) {
            uint8_t evType = ev->response_type & ~0x80;

            xcb_window_t srcXid  = 0;
            NkWindow*    window  = nullptr;
            NkWindowId   winId   = NK_INVALID_WINDOW_ID;

            // Extraire la window source selon le type
            switch (evType) {
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:
                    srcXid = ((xcb_key_press_event_t*)ev)->event; break;
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE:
                    srcXid = ((xcb_button_press_event_t*)ev)->event; break;
                case XCB_MOTION_NOTIFY:
                    srcXid = ((xcb_motion_notify_event_t*)ev)->event; break;
                case XCB_FOCUS_IN:
                case XCB_FOCUS_OUT:
                    srcXid = ((xcb_focus_in_event_t*)ev)->event; break;
                case XCB_EXPOSE:
                    srcXid = ((xcb_expose_event_t*)ev)->window; break;
                case XCB_CONFIGURE_NOTIFY:
                    srcXid = ((xcb_configure_notify_event_t*)ev)->window; break;
                case XCB_MAP_NOTIFY:
                case XCB_UNMAP_NOTIFY:
                    srcXid = ((xcb_map_notify_event_t*)ev)->window; break;
                case XCB_DESTROY_NOTIFY:
                    srcXid = ((xcb_destroy_notify_event_t*)ev)->window; break;
                case XCB_CLIENT_MESSAGE:
                    srcXid = ((xcb_client_message_event_t*)ev)->window; break;
                case XCB_SELECTION_NOTIFY:
                    srcXid = ((xcb_selection_notify_event_t*)ev)->requestor; break;
                case XCB_ENTER_NOTIFY:
                case XCB_LEAVE_NOTIFY:
                    srcXid = ((xcb_enter_notify_event_t*)ev)->event; break;
                default: break;
            }

            if (srcXid) {
                window = NkXCBFindWindow(srcXid);
                winId  = window ? window->GetId() : NK_INVALID_WINDOW_ID;
            }

            switch (evType) {

            // ----------------------------------------------------------------
            // Clavier
            // ----------------------------------------------------------------
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: {
                auto* ke = (xcb_key_press_event_t*)ev;
                NkScancode sc  = NkScancodeFromXKeycode(static_cast<uint32>(ke->detail));
                NkKey      key = NkScancodeToKey(sc);

                // Fallback XKB si disponible
                if (key == NkKey::NK_UNKNOWN && mData.mXkbState) {
                    xkb_keysym_t sym = xkb_state_key_get_one_sym(
                        mData.mXkbState, static_cast<xkb_keycode_t>(ke->detail));
                    key = NkKeycodeMap::NkKeyFromX11KeySym(static_cast<uint32>(sym));
                }

                NkModifierState mods = XCBMods(ke->state);
                uint32 nativeKey = static_cast<uint32>(ke->detail);

                if (key != NkKey::NK_UNKNOWN) {
                    if (evType == XCB_KEY_PRESS) {
                        NkKeyPressEvent e(key, sc, mods, nativeKey);
                        Enqueue(e, winId);
                    } else {
                        NkKeyReleaseEvent e(key, sc, mods, nativeKey);
                        Enqueue(e, winId);
                    }
                }

                // Texte produit (KeyPress uniquement)
                if (evType == XCB_KEY_PRESS && mData.mXkbState) {
                    char buf[32] = {};
                    xkb_state_key_get_utf8(mData.mXkbState,
                        static_cast<xkb_keycode_t>(ke->detail), buf, sizeof(buf));
                    if (buf[0] >= 0x20 && (unsigned char)buf[0] != 0x7F) {
                        NkTextInputEvent e(static_cast<uint32>((unsigned char)buf[0]));
                        Enqueue(e, winId);
                    }
                }
                break;
            }

            // ----------------------------------------------------------------
            // Souris Ã¢â‚¬â€ boutons
            // ----------------------------------------------------------------
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                auto* be = (xcb_button_press_event_t*)ev;
                int32 x  = (int32)be->event_x;
                int32 y  = (int32)be->event_y;
                int32 sx = (int32)be->root_x;
                int32 sy = (int32)be->root_y;
                NkModifierState m = XCBMods(be->state);
                bool pr = (evType == XCB_BUTTON_PRESS);

                switch (be->detail) {
                    case 1:
                        if (pr) { NkMouseButtonPressEvent   e(NkMouseButton::NK_MB_LEFT,   x,y,sx,sy,1,m); Enqueue(e, winId); }
                        else    { NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_LEFT,   x,y,sx,sy,1,m); Enqueue(e, winId); }
                        break;
                    case 2:
                        if (pr) { NkMouseButtonPressEvent   e(NkMouseButton::NK_MB_MIDDLE, x,y,sx,sy,1,m); Enqueue(e, winId); }
                        else    { NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_MIDDLE, x,y,sx,sy,1,m); Enqueue(e, winId); }
                        break;
                    case 3:
                        if (pr) { NkMouseButtonPressEvent   e(NkMouseButton::NK_MB_RIGHT,  x,y,sx,sy,1,m); Enqueue(e, winId); }
                        else    { NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_RIGHT,  x,y,sx,sy,1,m); Enqueue(e, winId); }
                        break;
                    case 4:
                        if (pr) { NkMouseWheelVerticalEvent   e( 1.0, x, y, 0.0, false, m); Enqueue(e, winId); }
                        break;
                    case 5:
                        if (pr) { NkMouseWheelVerticalEvent   e(-1.0, x, y, 0.0, false, m); Enqueue(e, winId); }
                        break;
                    case 6:
                        if (pr) { NkMouseWheelHorizontalEvent e(-1.0, x, y); Enqueue(e, winId); }
                        break;
                    case 7:
                        if (pr) { NkMouseWheelHorizontalEvent e( 1.0, x, y); Enqueue(e, winId); }
                        break;
                    default: break;
                }
                break;
            }

            // ----------------------------------------------------------------
            // Souris Ã¢â‚¬â€ dÃƒÂ©placement
            // ----------------------------------------------------------------
            case XCB_MOTION_NOTIFY: {
                auto* me = (xcb_motion_notify_event_t*)ev;
                int32 x  = (int32)me->event_x;
                int32 y  = (int32)me->event_y;
                int32 sx = (int32)me->root_x;
                int32 sy = (int32)me->root_y;
                NkMouseMoveEvent e(x, y, sx, sy, x - sPrevX, y - sPrevY,
                                NkMouseButtons{}, XCBMods(me->state));
                sPrevX = x; sPrevY = y;
                Enqueue(e, winId);
                break;
            }

            // ----------------------------------------------------------------
            // Survol
            // ----------------------------------------------------------------
            case XCB_ENTER_NOTIFY: {
                NkMouseEnterEvent e;
                Enqueue(e, winId);
                break;
            }
            case XCB_LEAVE_NOTIFY: {
                NkMouseLeaveEvent e;
                Enqueue(e, winId);
                break;
            }

            // ----------------------------------------------------------------
            // Focus
            // ----------------------------------------------------------------
            case XCB_FOCUS_IN: {
                NkWindowFocusGainedEvent e;
                Enqueue(e, winId);
                break;
            }
            case XCB_FOCUS_OUT: {
                NkWindowFocusLostEvent e;
                Enqueue(e, winId);
                break;
            }

            // ----------------------------------------------------------------
            // Exposition (paint)
            // ----------------------------------------------------------------
            case XCB_EXPOSE: {
                auto* xe = (xcb_expose_event_t*)ev;
                if (xe->count == 0) {
                    NkWindowPaintEvent e((int32)xe->x, (int32)xe->y,
                                        (uint32)xe->width, (uint32)xe->height);
                    Enqueue(e, winId);
                }
                break;
            }

            // ----------------------------------------------------------------
            // Redimensionnement / dÃƒÂ©placement
            // ----------------------------------------------------------------
            case XCB_CONFIGURE_NOTIFY: {
                auto* ce = (xcb_configure_notify_event_t*)ev;
                NkWindowResizeEvent er((uint32)ce->width, (uint32)ce->height,
                    window ? window->GetConfig().width  : 0u,
                    window ? window->GetConfig().height : 0u);
                Enqueue(er, winId);
                NkWindowMoveEvent em((int32)ce->x, (int32)ce->y);
                Enqueue(em, winId);
                break;
            }

            // ----------------------------------------------------------------
            // VisibilitÃƒÂ©
            // ----------------------------------------------------------------
            case XCB_MAP_NOTIFY: {
                NkWindowShownEvent e;
                Enqueue(e, winId);
                break;
            }
            case XCB_UNMAP_NOTIFY: {
                NkWindowHiddenEvent e;
                Enqueue(e, winId);
                break;
            }

            // ----------------------------------------------------------------
            // Destruction
            // ----------------------------------------------------------------
            case XCB_DESTROY_NOTIFY: {
                NkWindowDestroyEvent e;
                Enqueue(e, winId);
                break;
            }

            // ----------------------------------------------------------------
            // Messages WM + XDND client messages
            // ----------------------------------------------------------------
            case XCB_CLIENT_MESSAGE: {
                auto* cm = (xcb_client_message_event_t*)ev;
                if (window) {
                    if (window->mData.mDropTarget) {
                        window->mData.mDropTarget->HandleClientMessage(*cm);
                    }
                    if (cm->type == wmProtocols &&
                        cm->format == 32 &&
                        static_cast<xcb_atom_t>(cm->data.data32[0]) == wmDeleteWindow) {
                        NkWindowCloseEvent e(false);
                        Enqueue(e, winId);
                    }
                }
                break;
            }

            case XCB_SELECTION_NOTIFY: {
                auto* sn = (xcb_selection_notify_event_t*)ev;
                if (window && window->mData.mDropTarget) {
                    window->mData.mDropTarget->HandleSelectionNotify(*sn);
                }
                break;
            }
            default:
                break;
            }

            free(ev);
        }

        mPumping = false;
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "XCB";
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XCB
