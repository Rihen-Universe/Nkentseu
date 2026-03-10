// =============================================================================
// NkWin32EventSystem.cpp
// Implémentation Win32 des méthodes platform-spécifiques de NkEventSystem.
//
// Points appliqués :
//   Point 1 : NkSystem::Events() remplace NkEventSystem::Instance()
//   Point 2 : NkWin32FindWindow() remplace gNkWin32WindowMap direct
//   Point 4 : WM_DESTROY conditionne PostQuitMessage au compte de fenêtres
//             Chaque event porte son winId via Enqueue(evt, winId)
//   Point 5 : Enqueue() (dans NkEventSystem) prend le winId — plus de
//             SetWindowId séparé ici, tout est atomique dans Enqueue
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkKeycodeMap.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Platform/Win32/NkWin32Window.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

#ifndef HID_USAGE_PAGE_GENERIC
#   define HID_USAGE_PAGE_GENERIC  ((USHORT)0x01)
#   define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

namespace nkentseu {

    // =============================================================================
    // Init
    // =============================================================================

    bool NkEventSystem::Init() {
        if (mReady) return true;
        mTotalEventCount = 0;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;
        mReady   = true;
        return true;
    }

    // =============================================================================
    // PumpOS
    // =============================================================================

    void NkEventSystem::PumpOS() {
        if (mPumping) return;
        mPumping = true;

        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg); // route vers WindowProcStatic
        }

        mPumping = false;
    }

    // =============================================================================
    // GetPlatformName
    // =============================================================================

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Win32";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

    // =============================================================================
    // WindowProcStatic
    //
    // Point 1 : NkSystem::Events() remplace NkEventSystem::Instance()
    // Point 2 : NkWin32RegisterWindow / NkWin32FindWindow remplacent les globals
    // =============================================================================

    LRESULT CALLBACK NkEventSystem::WindowProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        // Point 1 : accès via NkSystem, pas via singleton de NkEventSystem
        auto& sys = NkSystem::Events();

        if (msg == WM_NCCREATE) {
            auto* cs  = reinterpret_cast<CREATESTRUCTW*>(lp);
            auto* win = static_cast<NkWindow*>(cs->lpCreateParams);
            if (win) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);

                // Raw input sur la première fenêtre
                if (!sys.mData.mRawInputRegistered) {
                    sys.mData.mRawInputRegistered = true;
                    RAWINPUTDEVICE rid{};
                    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
                    rid.usUsage     = HID_USAGE_GENERIC_MOUSE;
                    rid.dwFlags     = RIDEV_INPUTSINK;
                    rid.hwndTarget  = hwnd;
                    RegisterRawInputDevices(&rid, 1, sizeof(rid));
                }

                // Point 2 : via fonction d'accès, pas extern
                NkWin32RegisterWindow(hwnd, win);
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        if (msg == WM_NCDESTROY) {
            // Point 2 : via fonction d'accès
            NkWin32UnregisterWindow(hwnd);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        // O(1) lookup via GWLP_USERDATA
        auto* owner  = reinterpret_cast<NkWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        NkWindowId winId = owner ? owner->GetId() : NK_INVALID_WINDOW_ID;

        return sys.ProcessMessage(hwnd, msg, wp, lp, winId, owner);
    }

    // =============================================================================
    // Helpers
    // =============================================================================

    NkModifierState NkEventSystem::CurrentMods() {
        NkModifierState m;
        m.ctrl    = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        m.alt     = (GetKeyState(VK_MENU)    & 0x8000) != 0;
        m.shift   = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
        m.super   = ((GetKeyState(VK_LWIN)   & 0x8000) != 0) ||
                    ((GetKeyState(VK_RWIN)   & 0x8000) != 0);
        m.capLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        m.numLock = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        return m;
    }

    NkKey NkEventSystem::VkeyToNkKey(UINT vk, bool extended) noexcept {
        return NkKeycodeMap::NkKeyFromWin32VK((uint32)vk, extended);
    }

    // =============================================================================
    // ProcessMessage
    //
    // Point 4 : WM_DESTROY — PostQuitMessage uniquement si c'est la dernière fenêtre.
    //           Chaque Enqueue(evt, winId) estampille l'event avec le bon winId.
    // Point 5 : Enqueue() (dans NkEventSystem.cpp) gère le lock de mQueueMutex
    //           de façon atomique — pas de double SetWindowId ici.
    // =============================================================================

    LRESULT NkEventSystem::ProcessMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                        NkWindowId winId, NkWindow* owner)
    {
        LRESULT result              = 0;
        bool    suppressDefaultProc = false;

        // Point 4 : macro locale qui passe winId à Enqueue
        // Chaque event sait exactement de quelle fenêtre il provient.
        auto EnqueueForWindow = [&](NkEvent& evt) {
            Enqueue(evt, winId);
        };

        switch (msg)
        {
            // =====================================================================
            // Fenêtre — cycle de vie
            // =====================================================================

            case WM_CREATE: {
                NkWindowCreateEvent evt(
                    owner ? owner->GetConfig().width  : 0u,
                    owner ? owner->GetConfig().height : 0u);
                EnqueueForWindow(evt);
                break;
            }

            case WM_CLOSE: {
                NkWindowCloseEvent evt(false);
                EnqueueForWindow(evt);
                suppressDefaultProc = true;
                break;
            }

            case WM_DESTROY: {
                // Point 4 : l'event porte le winId de la fenêtre détruite.
                // PostQuitMessage seulement si c'est la dernière fenêtre enregistrée.
                NkWindowDestroyEvent evt;
                EnqueueForWindow(evt);

                // On désenregistre la fenêtre avant de vérifier le compte
                // (UnregisterWindow est déjà appelé dans NkWindow::Close,
                //  mais WM_DESTROY peut arriver avant Close dans certains flux)
                uint32 remaining = NkSystem::Instance().GetWindowCount();
                if (remaining == 0) {
                    PostQuitMessage(0);
                }
                break;
            }

            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                NkWindowPaintEvent evt(
                    (int32)ps.rcPaint.left, (int32)ps.rcPaint.top,
                    (uint32)(ps.rcPaint.right  - ps.rcPaint.left),
                    (uint32)(ps.rcPaint.bottom - ps.rcPaint.top));
                EnqueueForWindow(evt);
                break;
            }

            case WM_ERASEBKGND:
                result = 1;
                break;

            // =====================================================================
            // Fenêtre — focus / visibilité
            // =====================================================================

            case WM_SETFOCUS: {
                NkWindowFocusGainedEvent evt;
                EnqueueForWindow(evt);
                break;
            }

            case WM_KILLFOCUS: {
                NkWindowFocusLostEvent evt;
                EnqueueForWindow(evt);
                break;
            }

            case WM_SHOWWINDOW: {
                if (wp) { NkWindowShownEvent  e; EnqueueForWindow(e); }
                else    { NkWindowHiddenEvent e; EnqueueForWindow(e); }
                break;
            }

            // =====================================================================
            // Fenêtre — taille / position
            // =====================================================================

            case WM_SIZE: {
                uint32 nw = LOWORD(lp), nh = HIWORD(lp);
                NkWindowResizeEvent evt(nw, nh,
                    owner ? owner->GetConfig().width  : 0u,
                    owner ? owner->GetConfig().height : 0u);
                EnqueueForWindow(evt);
                if      (wp == SIZE_MINIMIZED) { NkWindowMinimizeEvent e; EnqueueForWindow(e); }
                else if (wp == SIZE_MAXIMIZED) { NkWindowMaximizeEvent e; EnqueueForWindow(e); }
                else if (wp == SIZE_RESTORED)  { NkWindowRestoreEvent  e; EnqueueForWindow(e); }
                break;
            }

            case WM_MOVE: {
                NkWindowMoveEvent evt((int32)LOWORD(lp), (int32)HIWORD(lp));
                EnqueueForWindow(evt);
                break;
            }

            case WM_DPICHANGED: {
                WORD dpi = HIWORD(wp);
                NkWindowDpiEvent evt(
                    (float32)dpi / USER_DEFAULT_SCREEN_DPI,
                    owner ? owner->GetDpiScale() : 1.f,
                    (uint32)dpi);
                EnqueueForWindow(evt);
                const RECT* r = reinterpret_cast<const RECT*>(lp);
                if (r) SetWindowPos(hwnd, nullptr,
                    r->left, r->top, r->right - r->left, r->bottom - r->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }

            // =====================================================================
            // Souris
            // =====================================================================

            case WM_MOUSEMOVE: {
                if (owner && !owner->mData.mMouseTracking) {
                    TRACKMOUSEEVENT tme{};
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    if (TrackMouseEvent(&tme)) {
                        owner->mData.mMouseTracking = true;
                    }
                    NkMouseEnterEvent enterEvt;
                    EnqueueForWindow(enterEvt);
                }

                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                POINT pt = { x, y }; ClientToScreen(hwnd, &pt);
                NkMouseButtons buttons;
                SHORT mk = LOWORD(wp);
                if (mk & MK_LBUTTON)  buttons.Set(NkMouseButton::NK_MB_LEFT);
                if (mk & MK_RBUTTON)  buttons.Set(NkMouseButton::NK_MB_RIGHT);
                if (mk & MK_MBUTTON)  buttons.Set(NkMouseButton::NK_MB_MIDDLE);
                if (mk & MK_XBUTTON1) buttons.Set(NkMouseButton::NK_MB_BACK);
                if (mk & MK_XBUTTON2) buttons.Set(NkMouseButton::NK_MB_FORWARD);
                NkMouseMoveEvent evt(x, y, (int32)pt.x, (int32)pt.y,
                    x - mData.mPrevMouseX, y - mData.mPrevMouseY, buttons, CurrentMods());
                mData.mPrevMouseX = x;
                mData.mPrevMouseY = y;
                EnqueueForWindow(evt);
                break;
            }

            case WM_INPUT: {
                UINT sz = 0;
                GetRawInputData((HRAWINPUT)lp, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER));
                if (sz > 0) {
                    std::vector<BYTE> buf(sz);
                    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf.data(),
                                        &sz, sizeof(RAWINPUTHEADER)) == sz) {
                        auto* raw = reinterpret_cast<const RAWINPUT*>(buf.data());
                        if (raw->header.dwType == RIM_TYPEMOUSE) {
                            NkMouseRawEvent evt(raw->data.mouse.lLastX,
                                            raw->data.mouse.lLastY, 0);
                            EnqueueForWindow(evt);
                        }
                    }
                }
                break;
            }

            case WM_MOUSEWHEEL: {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                SHORT mk    = LOWORD(wp);
                double delta = GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA;
                NkModifierState mods;
                mods.ctrl  = !!(mk & MK_CONTROL);
                mods.shift = !!(mk & MK_SHIFT);
                NkMouseWheelVerticalEvent evt(delta, (int32)pt.x, (int32)pt.y, 0.0, false, mods);
                EnqueueForWindow(evt);
                break;
            }

            case WM_MOUSEHWHEEL: {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                double delta = GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA;
                NkMouseWheelHorizontalEvent evt(delta, (int32)pt.x, (int32)pt.y);
                EnqueueForWindow(evt);
                break;
            }

    #define NK_MB_PRESS(Button) {                                                   \
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};                           \
        POINT sp = pt; ClientToScreen(hwnd, &sp);                                   \
        NkMouseButtonPressEvent evt(NkMouseButton::Button,                          \
            pt.x, pt.y, sp.x, sp.y, 1, CurrentMods()); EnqueueForWindow(evt); } break

    #define NK_MB_RELEASE(Button) {                                                 \
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};                           \
        POINT sp = pt; ClientToScreen(hwnd, &sp);                                   \
        NkMouseButtonReleaseEvent evt(NkMouseButton::Button,                        \
            pt.x, pt.y, sp.x, sp.y, 1, CurrentMods()); EnqueueForWindow(evt); } break

            case WM_LBUTTONDOWN: NK_MB_PRESS(NK_MB_LEFT);
            case WM_LBUTTONUP:   NK_MB_RELEASE(NK_MB_LEFT);
            case WM_RBUTTONDOWN: NK_MB_PRESS(NK_MB_RIGHT);
            case WM_RBUTTONUP:   NK_MB_RELEASE(NK_MB_RIGHT);
            case WM_MBUTTONDOWN: NK_MB_PRESS(NK_MB_MIDDLE);
            case WM_MBUTTONUP:   NK_MB_RELEASE(NK_MB_MIDDLE);

    #undef NK_MB_PRESS
    #undef NK_MB_RELEASE

            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK: {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                POINT sp = pt; ClientToScreen(hwnd, &sp);
                NkMouseButton btn =
                    (msg == WM_LBUTTONDBLCLK) ? NkMouseButton::NK_MB_LEFT   :
                    (msg == WM_RBUTTONDBLCLK) ? NkMouseButton::NK_MB_RIGHT   :
                                                NkMouseButton::NK_MB_MIDDLE;
                NkMouseDoubleClickEvent evt(btn, pt.x, pt.y, sp.x, sp.y, CurrentMods());
                EnqueueForWindow(evt);
                break;
            }

            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP: {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                POINT sp = pt; ClientToScreen(hwnd, &sp);
                NkMouseButton btn = (HIWORD(wp) & XBUTTON1)
                    ? NkMouseButton::NK_MB_BACK : NkMouseButton::NK_MB_FORWARD;
                if (msg == WM_XBUTTONDOWN) {
                    NkMouseButtonPressEvent   e(btn, pt.x, pt.y, sp.x, sp.y, 1, CurrentMods());
                    EnqueueForWindow(e);
                } else {
                    NkMouseButtonReleaseEvent e(btn, pt.x, pt.y, sp.x, sp.y, 1, CurrentMods());
                    EnqueueForWindow(e);
                }
                break;
            }

            case WM_MOUSELEAVE: {
                if (owner) {
                    owner->mData.mMouseTracking = false;
                }
                NkMouseLeaveEvent evt;
                EnqueueForWindow(evt);
                break;
            }

            // =====================================================================
            // Clavier
            // =====================================================================

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                UINT sc    = (lp >> 16) & 0xFF;
                bool isExt = (lp >> 24) & 1;
                bool isRep = (lp >> 30) & 1;
                NkScancode nkSc = NkScancodeFromWin32(sc, isExt);
                NkKey k = NkScancodeToKey(nkSc);
                if (k == NkKey::NK_UNKNOWN)
                    k = VkeyToNkKey((UINT)wp, isExt);
                if (k != NkKey::NK_UNKNOWN) {
                    bool isPress = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
                    NkModifierState mods    = CurrentMods();
                    UINT            nativeKey = (UINT)wp;
                    if (isPress && isRep) {
                        NkKeyRepeatEvent e(k, 1, nkSc, mods, nativeKey, isExt);
                        EnqueueForWindow(e);
                    } else if (isPress) {
                        NkKeyPressEvent e(k, nkSc, mods, nativeKey, isExt);
                        EnqueueForWindow(e);
                    } else {
                        NkKeyReleaseEvent e(k, nkSc, mods, nativeKey, isExt);
                        EnqueueForWindow(e);
                    }
                }
                break;
            }

            case WM_CHAR: {
                UINT cp = (UINT)wp;
                if (cp >= 32 && cp != 127) {
                    NkTextInputEvent evt(cp);
                    EnqueueForWindow(evt);
                }
                break;
            }

            // =====================================================================
            // Hit-test (fenêtre sans bordure)
            // =====================================================================

            case WM_NCHITTEST:
                if (owner && !owner->GetConfig().frame) {
                    RECT rc; GetWindowRect(hwnd, &rc);
                    int x = GET_X_LPARAM(lp) - rc.left;
                    int y = GET_Y_LPARAM(lp) - rc.top;
                    int w = rc.right  - rc.left;
                    int h = rc.bottom - rc.top;
                    int b = IsZoomed(hwnd) ? 0 : 5;
                    if (x < b && y < b)             { result = HTTOPLEFT;     break; }
                    if (x > w-b && y < b)           { result = HTTOPRIGHT;    break; }
                    if (x < b && y > h-b)           { result = HTBOTTOMLEFT;  break; }
                    if (x > w-b && y > h-b)         { result = HTBOTTOMRIGHT; break; }
                    if (x < b)                      { result = HTLEFT;        break; }
                    if (x > w-b)                    { result = HTRIGHT;       break; }
                    if (y < b)                      { result = HTTOP;         break; }
                    if (y > h-b)                    { result = HTBOTTOM;      break; }
                    if (y < 32 && x > 260 && x < w-260) { result = HTCAPTION; break; }
                    result = HTCLIENT;
                }
                break;

            case WM_GETMINMAXINFO:
                if (owner) {
                    auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
                    mm->ptMinTrackSize.x = (LONG)owner->GetConfig().minWidth;
                    mm->ptMinTrackSize.y = (LONG)owner->GetConfig().minHeight;
                }
                break;

            default:
                break;
        }

        if (suppressDefaultProc) return 0;
        return result ? result : DefWindowProcW(hwnd, msg, wp, lp);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
