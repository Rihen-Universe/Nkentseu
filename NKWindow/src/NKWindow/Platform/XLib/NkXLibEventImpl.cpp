// =============================================================================
// NkXLibEventImpl.cpp  —  Système d'événements XLib
//
// Pattern identique à Win32 :
//   - Événements typés  : NkKeyPressEvent, NkMouseMoveEvent, etc.
//   - Enqueue via Clone() → unique_ptr dans mQueue.
//   - Utilisation du registre global gXLibWindowRegistry() pour récupérer
//     le pointeur de fenêtre à partir de ::Window.
//   - Les callbacks sont stockés dans mWindowCallbacks.
// =============================================================================

#include "NkXLibEventImpl.h"
#include "NkXLibWindowImpl.h"
#include "../../Events/NkKeyboardEvent.h"
#include "../../Events/NkMouseEvent.h"
#include "../../Events/NkWindowEvent.h"
#include <X11/keysym.h>
#include <utility>

namespace nkentseu {

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

void NkXLibEventImpl::Initialize(IWindowImpl *owner, void *nativeHandle) {
    auto *w = static_cast<NkXLibWindowImpl *>(owner);
    // On ne stocke pas le pointeur de fenêtre, juste le display pour PollEvents
    if (!mDisplay && w)
        mDisplay = w->GetDisplay();
    // On n'ajoute rien dans mWindowCallbacks ici (le callback sera ajouté plus tard)
}

void NkXLibEventImpl::Shutdown(void *nativeHandle) {
    // On retire le callback éventuel
    mWindowCallbacks.erase(nativeHandle);
    if (mWindowCallbacks.empty())
        mDisplay = nullptr;
}

// ---------------------------------------------------------------------------
// Queue FIFO
// ---------------------------------------------------------------------------

NkEvent* NkXLibEventImpl::Front() const {
    return mQueue.empty() ? nullptr : mQueue.front().get();
}

void NkXLibEventImpl::Pop() {
    if (!mQueue.empty())
        mQueue.pop();
}

bool NkXLibEventImpl::IsEmpty() const {
    return mQueue.empty();
}

std::size_t NkXLibEventImpl::Size() const {
    return mQueue.size();
}

void NkXLibEventImpl::PushEvent(std::unique_ptr<NkEvent> e) {
    mQueue.push(std::move(e));
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void NkXLibEventImpl::SetGlobalCallback(NkEventCallback cb) {
    mGlobalCallback = std::move(cb);
}

void NkXLibEventImpl::SetWindowCallback(void *nativeHandle, NkEventCallback cb) {
    mWindowCallbacks[nativeHandle] = std::move(cb);
}

void NkXLibEventImpl::DispatchEvent(NkEvent* event, void *nativeHandle) {
    if (!event) return;
    if (nativeHandle) {
        auto it = mWindowCallbacks.find(nativeHandle);
        if (it != mWindowCallbacks.end() && it->second)
            it->second(event);
    }
    if (mGlobalCallback)
        mGlobalCallback(event);
}

// ---------------------------------------------------------------------------
// PollEvents
//
// RÈGLE : On enqueue TOUJOURS via Clone() → unique_ptr.
//         On n'appelle PAS les callbacks ici ; cela sera fait par EventSystem.
// ---------------------------------------------------------------------------

void NkXLibEventImpl::PollEvents() {
    if (!mDisplay)
        return;

    auto Enqueue = [&](const NkEvent& evt) {
        mQueue.push(std::unique_ptr<NkEvent>(evt.Clone()));
    };

    while (XPending(mDisplay) > 0) {
        XEvent xev{};
        XNextEvent(mDisplay, &xev);

        ::Window srcWindow = xev.xany.window;

        switch (xev.type) {

            // ----------------------------------------------------------------
            // Clavier
            // ----------------------------------------------------------------

            case KeyPress:
            case KeyRelease: {
                NkScancode      sc   = NkScancodeFromXKeycode(static_cast<NkU32>(xev.xkey.keycode));
                NkKey           key  = NkScancodeToKey(sc);
                if (key == NkKey::NK_UNKNOWN) {
                    KeySym ks = XLookupKeysym(&xev.xkey, 0);
                    key = XlibKeysymToNkKey(ks);
                }
                NkModifierState mods = XlibMods(xev.xkey.state);
                NkU32           nk   = static_cast<NkU32>(xev.xkey.keycode);

                if (key != NkKey::NK_UNKNOWN) {
                    if (xev.type == KeyPress)
                        Enqueue(NkKeyPressEvent(key, sc, mods, nk));
                    else
                        Enqueue(NkKeyReleaseEvent(key, sc, mods, nk));
                }

                // Texte produit (après mapping layout / IME)
                if (xev.type == KeyPress) {
                    char   buf[32] = {};
                    KeySym dummy;
                    int    n = XLookupString(&xev.xkey, buf, sizeof(buf) - 1, &dummy, nullptr);
                    if (n > 0 && static_cast<unsigned char>(buf[0]) >= 0x20
                              && static_cast<unsigned char>(buf[0]) != 0x7F) {
                        NkU32 cp = static_cast<unsigned char>(buf[0]);
                        if (cp < 0x80)
                            Enqueue(NkTextInputEvent(cp));
                    }
                }
                break;
            }

            // ----------------------------------------------------------------
            // Souris — boutons
            // ----------------------------------------------------------------

            case ButtonPress:
            case ButtonRelease: {
                NkI32           x  = xev.xbutton.x;
                NkI32           y  = xev.xbutton.y;
                NkI32           sx = xev.xbutton.x_root;
                NkI32           sy = xev.xbutton.y_root;
                NkModifierState m  = XlibMods(xev.xbutton.state);
                bool            press = (xev.type == ButtonPress);

                switch (xev.xbutton.button) {
                    case Button1:
                        if (press) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_LEFT,   x,y,sx,sy,1,m));
                        else       Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_LEFT,  x,y,sx,sy,1,m));
                        break;
                    case Button2:
                        if (press) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_MIDDLE, x,y,sx,sy,1,m));
                        else       Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_MIDDLE,x,y,sx,sy,1,m));
                        break;
                    case Button3:
                        if (press) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_RIGHT,  x,y,sx,sy,1,m));
                        else       Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_RIGHT, x,y,sx,sy,1,m));
                        break;
                    // Button4 / Button5 = molette verticale
                    case Button4:
                        if (press) Enqueue(NkMouseWheelVerticalEvent( 1.0, x, y, 0.0, false, m));
                        break;
                    case Button5:
                        if (press) Enqueue(NkMouseWheelVerticalEvent(-1.0, x, y, 0.0, false, m));
                        break;
                    // Button6 / Button7 = molette horizontale
                    case 6:
                        if (press) Enqueue(NkMouseWheelHorizontalEvent(-1.0, x, y));
                        break;
                    case 7:
                        if (press) Enqueue(NkMouseWheelHorizontalEvent( 1.0, x, y));
                        break;
                    default:
                        break;
                }
                break;
            }

            // ----------------------------------------------------------------
            // Souris — déplacement
            // ----------------------------------------------------------------

            case MotionNotify: {
                NkI32 x  = xev.xmotion.x;
                NkI32 y  = xev.xmotion.y;
                NkI32 sx = xev.xmotion.x_root;
                NkI32 sy = xev.xmotion.y_root;
                NkI32 dx = x - mPrevMouseX;
                NkI32 dy = y - mPrevMouseY;
                mPrevMouseX = x;
                mPrevMouseY = y;
                Enqueue(NkMouseMoveEvent(x, y, sx, sy, dx, dy, {}, XlibMods(xev.xmotion.state)));
                break;
            }

            // ----------------------------------------------------------------
            // Fenêtre — focus
            // ----------------------------------------------------------------

            case FocusIn:
                Enqueue(NkWindowFocusGainedEvent());
                break;

            case FocusOut:
                Enqueue(NkWindowFocusLostEvent());
                break;

            // ----------------------------------------------------------------
            // Fenêtre — redimensionnement
            // ----------------------------------------------------------------

            case ConfigureNotify:
                Enqueue(NkWindowResizeEvent(
                    static_cast<NkU32>(xev.xconfigure.width),
                    static_cast<NkU32>(xev.xconfigure.height)));
                break;

            // ----------------------------------------------------------------
            // Fenêtre — fermeture (WM_DELETE_WINDOW)
            // ----------------------------------------------------------------

            case ClientMessage: {
                srcWindow = xev.xclient.window;
                auto *win = gXLibWindowRegistry().Find(srcWindow);
                if (win) {
                    XClientMessageEvent &cm = xev.xclient;
                    if (cm.message_type == win->GetWmProtocolsAtom() &&
                        static_cast<Atom>(cm.data.l[0]) == win->GetWmDeleteAtom())
                        Enqueue(NkWindowCloseEvent(false));
                }
                break;
            }

            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

NkKey NkXLibEventImpl::XlibKeysymToNkKey(KeySym ks) {
    return NkKeycodeMap::NkKeyFromX11KeySym(static_cast<NkU32>(ks));
}

NkModifierState NkXLibEventImpl::XlibMods(unsigned int state) {
    NkModifierState mods;
    mods.ctrl    = !!(state & ControlMask);
    mods.alt     = !!(state & Mod1Mask);
    mods.shift   = !!(state & ShiftMask);
    mods.super   = !!(state & Mod4Mask);
    mods.capLock = !!(state & LockMask);
    mods.numLock = !!(state & Mod2Mask);
    return mods;
}

const char *NkXLibEventImpl::GetPlatformName() const noexcept { return "XLib"; }
bool        NkXLibEventImpl::IsReady()         const noexcept { return true; }

} // namespace nkentseu
