// =============================================================================
// NkXLibEventSystem.cpp
// Implementation XLib des methodes platform-specifiques de NkEventSystem.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include "NKEvent/NkEventSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeycodeMap.h"
#include "NKWindow/Platform/XLib/NkXLibWindow.h"
#include "NKWindow/Platform/XLib/NkXLibDropTarget.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

namespace nkentseu {
    using namespace math;

    // =============================================================================
    // Helpers
    // =============================================================================

    static NkKey XLibKeysymToNkKey(KeySym ks) {
        return NkKeycodeMap::NkKeyFromX11KeySym(static_cast<uint32>(ks));
    }

    static NkModifierState XLibMods(unsigned int state) {
        NkModifierState mods;
        mods.ctrl    = !!(state & ControlMask);
        mods.alt     = !!(state & Mod1Mask);
        mods.shift   = !!(state & ShiftMask);
        mods.super   = !!(state & Mod4Mask);
        mods.capLock = !!(state & LockMask);
        mods.numLock = !!(state & Mod2Mask);
        return mods;
    }

    // =============================================================================
    // NkEventSystem - methodes platform-specifiques
    // =============================================================================

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }

        mPumping = false;
        mData.mDisplay = NkXLibGetDisplay();
        mData.mPrevMouseX = 0;
        mData.mPrevMouseY = 0;
        mData.mInitialized = true;
        mReady = true;
        return true;
    }

    // Shutdown() est gere dans NkEventSystem.cpp (cross-platform).

    void NkEventSystem::PumpOS() {
        if (mPumping) {
            return;
        }
        mPumping = true;

        // Le display peut etre ouvert apres Init(), donc refresh ici.
        mData.mDisplay = NkXLibGetDisplay();
        Display* display = mData.mDisplay;
        if (!display) {
            mPumping = false;
            return;
        }

        const Atom wmProtocols = XInternAtom(display, "WM_PROTOCOLS", True);

        while (XPending(display) > 0) {
            XEvent xev{};
            XNextEvent(display, &xev);

            ::Window srcXid = xev.xany.window;
            if (xev.type == SelectionNotify) {
                srcXid = xev.xselection.requestor;
            }
            NkWindow* window = NkXLibFindWindow(srcXid);
            NkWindowId winId = window ? window->GetId() : NK_INVALID_WINDOW_ID;

            switch (xev.type) {
                // ------------------------------------------------------------
                // Keyboard
                // ------------------------------------------------------------
                case KeyPress:
                case KeyRelease: {
                    NkScancode sc = NkScancodeFromXKeycode(static_cast<uint32>(xev.xkey.keycode));
                    NkKey key = NkScancodeToKey(sc);
                    if (key == NkKey::NK_UNKNOWN) {
                        KeySym ks = XLookupKeysym(&xev.xkey, 0);
                        key = XLibKeysymToNkKey(ks);
                    }

                    NkModifierState mods = XLibMods(xev.xkey.state);
                    uint32 nativeKey = static_cast<uint32>(xev.xkey.keycode);

                    if (key != NkKey::NK_UNKNOWN) {
                        if (xev.type == KeyPress) {
                            NkKeyPressEvent e(key, sc, mods, nativeKey);
                            Enqueue(e, winId);
                        } else {
                            NkKeyReleaseEvent e(key, sc, mods, nativeKey);
                            Enqueue(e, winId);
                        }
                    }

                    // Texte UTF-8 (base ASCII single-byte here)
                    if (xev.type == KeyPress) {
                        char buf[32] = {};
                        KeySym dummy;
                        int n = XLookupString(&xev.xkey, buf, static_cast<int>(sizeof(buf)) - 1, &dummy, nullptr);
                        if (n > 0 && static_cast<unsigned char>(buf[0]) >= 0x20 &&
                            static_cast<unsigned char>(buf[0]) != 0x7F) {
                            NkTextInputEvent e(static_cast<uint32>(static_cast<unsigned char>(buf[0])));
                            Enqueue(e, winId);
                        }
                    }
                    break;
                }

                // ------------------------------------------------------------
                // Mouse buttons + wheel
                // ------------------------------------------------------------
                case ButtonPress:
                case ButtonRelease: {
                    int32 x = xev.xbutton.x;
                    int32 y = xev.xbutton.y;
                    int32 sx = xev.xbutton.x_root;
                    int32 sy = xev.xbutton.y_root;
                    NkModifierState mods = XLibMods(xev.xbutton.state);
                    const bool pressed = (xev.type == ButtonPress);

                    switch (xev.xbutton.button) {
                        case Button1:
                            if (pressed) {
                                NkMouseButtonPressEvent e(NkMouseButton::NK_MB_LEFT, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            } else {
                                NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_LEFT, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            }
                            break;
                        case Button2:
                            if (pressed) {
                                NkMouseButtonPressEvent e(NkMouseButton::NK_MB_MIDDLE, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            } else {
                                NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_MIDDLE, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            }
                            break;
                        case Button3:
                            if (pressed) {
                                NkMouseButtonPressEvent e(NkMouseButton::NK_MB_RIGHT, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            } else {
                                NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_RIGHT, x, y, sx, sy, 1, mods);
                                Enqueue(e, winId);
                            }
                            break;
                        case Button4:
                            if (pressed) {
                                NkMouseWheelVerticalEvent e(1.0, x, y, 0.0, false, mods);
                                Enqueue(e, winId);
                            }
                            break;
                        case Button5:
                            if (pressed) {
                                NkMouseWheelVerticalEvent e(-1.0, x, y, 0.0, false, mods);
                                Enqueue(e, winId);
                            }
                            break;
                        case 6:
                            if (pressed) {
                                NkMouseWheelHorizontalEvent e(-1.0, x, y);
                                Enqueue(e, winId);
                            }
                            break;
                        case 7:
                            if (pressed) {
                                NkMouseWheelHorizontalEvent e(1.0, x, y);
                                Enqueue(e, winId);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                }

                // ------------------------------------------------------------
                // Mouse move
                // ------------------------------------------------------------
                case MotionNotify: {
                    int32 x = xev.xmotion.x;
                    int32 y = xev.xmotion.y;
                    int32 sx = xev.xmotion.x_root;
                    int32 sy = xev.xmotion.y_root;
                    int32 dx = x - mData.mPrevMouseX;
                    int32 dy = y - mData.mPrevMouseY;
                    mData.mPrevMouseX = x;
                    mData.mPrevMouseY = y;

                    NkMouseMoveEvent e(x, y, sx, sy, dx, dy, NkMouseButtons{}, XLibMods(xev.xmotion.state));
                    Enqueue(e, winId);
                    break;
                }

                case EnterNotify: {
                    NkMouseEnterEvent e;
                    Enqueue(e, winId);
                    break;
                }

                case LeaveNotify: {
                    NkMouseLeaveEvent e;
                    Enqueue(e, winId);
                    break;
                }

                // ------------------------------------------------------------
                // Focus
                // ------------------------------------------------------------
                case FocusIn: {
                    NkWindowFocusGainedEvent e;
                    Enqueue(e, winId);
                    break;
                }
                case FocusOut: {
                    NkWindowFocusLostEvent e;
                    Enqueue(e, winId);
                    break;
                }

                // ------------------------------------------------------------
                // Paint
                // ------------------------------------------------------------
                case Expose: {
                    if (xev.xexpose.count == 0) {
                        NkWindowPaintEvent e(
                            xev.xexpose.x,
                            xev.xexpose.y,
                            static_cast<uint32>(xev.xexpose.width),
                            static_cast<uint32>(xev.xexpose.height));
                        Enqueue(e, winId);
                    }
                    break;
                }

                // ------------------------------------------------------------
                // Resize / move
                // ------------------------------------------------------------
                case ConfigureNotify: {
                    // Drain burst: keep only the last ConfigureNotify for this window
                    {
                        XEvent next;
                        while (XCheckTypedWindowEvent(display, xev.xany.window, ConfigureNotify, &next)) {
                            xev = next;
                        }
                    }
                    {
                        NkWindowResizeBeginEvent beginEvt;
                        Enqueue(beginEvt, winId);
                    }
                    {
                        NkWindowResizeEvent e(
                            static_cast<uint32>(xev.xconfigure.width),
                            static_cast<uint32>(xev.xconfigure.height),
                            window ? window->GetConfig().width : 0u,
                            window ? window->GetConfig().height : 0u);
                        Enqueue(e, winId);
                    }
                    {
                        NkWindowResizeEndEvent endEvt;
                        Enqueue(endEvt, winId);
                    }
                    {
                        NkWindowMoveBeginEvent moveBegin;
                        Enqueue(moveBegin, winId);
                        NkWindowMoveEvent em(xev.xconfigure.x, xev.xconfigure.y);
                        Enqueue(em, winId);
                        NkWindowMoveEndEvent moveEnd;
                        Enqueue(moveEnd, winId);
                    }
                    break;
                }

                // ------------------------------------------------------------
                // Visibility
                // ------------------------------------------------------------
                case MapNotify: {
                    NkWindowShownEvent e;
                    Enqueue(e, winId);
                    break;
                }
                case UnmapNotify: {
                    NkWindowHiddenEvent e;
                    Enqueue(e, winId);
                    break;
                }

                // ------------------------------------------------------------
                // Close request (WM_DELETE_WINDOW)
                // ------------------------------------------------------------
                case ClientMessage: {
                    if (window) {
                        if (window->mData.mDropTarget) {
                            window->mData.mDropTarget->HandleClientMessage(xev.xclient);
                        }
                        const XClientMessageEvent& cm = xev.xclient;
                        if (cm.message_type == wmProtocols &&
                            static_cast<Atom>(cm.data.l[0]) == window->mData.mWmDeleteWindow) {
                            NkWindowCloseEvent e(false);
                            Enqueue(e, winId);
                        }
                    }
                    break;
                }

                case SelectionNotify: {
                    if (window && window->mData.mDropTarget) {
                        window->mData.mDropTarget->HandleSelectionNotify(xev.xselection);
                    }
                    break;
                }

                case DestroyNotify: {
                    NkWindowDestroyEvent e;
                    Enqueue(e, winId);
                    break;
                }

                default:
                    break;
            }
        }

        mPumping = false;
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "XLib";
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
