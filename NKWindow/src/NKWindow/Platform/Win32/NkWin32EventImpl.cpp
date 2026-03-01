// =============================================================================
// NkWin32EventImpl.cpp  — v3
//
// CORRECTION PRINCIPALE :
//   La lambda Enqueue() ne filtre plus par evt.IsValid().
//   IsValid() retourne false pour certains types d'événements (NkWindowCloseEvent,
//   NkKeyPressEvent, etc.) si leur NkEventType n'est pas dans la liste reconnue
//   par l'implémentation de base — ce qui les faisait disparaître silencieusement.
//   On pousse TOUJOURS dans la queue ; c'est NkEventSystem qui filtre si besoin.
// =============================================================================

#include "NkWin32EventImpl.h"
#include "NkWin32WindowImpl.h"
#include "NKWindow/Core/NkEvents.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <vector>
#include <iostream>

#pragma comment(lib, "dwmapi.lib")

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC  ((USHORT)0x01)
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

namespace nkentseu {

    thread_local NkWin32EventImpl* NkWin32EventImpl::sCurrentImpl = nullptr;

    // -------------------------------------------------------------------------
    // Cycle de vie
    // -------------------------------------------------------------------------

    void NkWin32EventImpl::Initialize(IWindowImpl* /*owner*/, void* nativeHandle)
    {
        HWND hwnd = static_cast<HWND>(nativeHandle);
        sCurrentImpl = this;

        if (!mRawInputRegistered) {
            mRawInputRegistered = true;
            RAWINPUTDEVICE rid{};
            rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
            rid.usUsage     = HID_USAGE_GENERIC_MOUSE;
            rid.dwFlags     = RIDEV_INPUTSINK;
            rid.hwndTarget  = hwnd;
            RegisterRawInputDevices(&rid, 1, sizeof(rid));
        }
    }

    void NkWin32EventImpl::Shutdown(void* /*nativeHandle*/) {}

    // -------------------------------------------------------------------------
    // Pompe OS
    // -------------------------------------------------------------------------

    void NkWin32EventImpl::PollEvents()
    {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // -------------------------------------------------------------------------
    // Queue FIFO
    // -------------------------------------------------------------------------

    NkEvent* NkWin32EventImpl::Front() const {
        return mQueue.empty() ? nullptr : mQueue.front().get();
    }
    void NkWin32EventImpl::Pop() {
        if (!mQueue.empty()) mQueue.pop();
    }
    bool        NkWin32EventImpl::IsEmpty() const { return mQueue.empty(); }
    std::size_t NkWin32EventImpl::Size()    const { return mQueue.size();  }
    void NkWin32EventImpl::PushEvent(std::unique_ptr<NkEvent> e) { mQueue.push(std::move(e)); }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    void NkWin32EventImpl::SetGlobalCallback(NkEventCallback cb) {
        mGlobalCallback = std::move(cb);
    }

    void NkWin32EventImpl::SetWindowCallback(void* nativeHandle, NkEventCallback cb) {
        HWND hwnd = static_cast<HWND>(nativeHandle);
        mWindowCallbacks[hwnd] = std::move(cb);
    }

    void NkWin32EventImpl::AddTypedCallback(NkEventType::Value type, NkEventCallback cb) {
        mTypedCallbacks[type].push_back(std::move(cb));
    }

    void NkWin32EventImpl::RemoveTypedCallback(NkEventType::Value type, NkEventCallback cb) {
        auto it = mTypedCallbacks.find(type);
        if (it != mTypedCallbacks.end()) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [&cb](const NkEventCallback& stored) {
                    return stored.target_type() == cb.target_type();
                }), vec.end());
            if (vec.empty()) mTypedCallbacks.erase(it);
        }
    }

    void NkWin32EventImpl::ClearTypedCallbacks(NkEventType::Value type) {
        mTypedCallbacks.erase(type);
    }

    void NkWin32EventImpl::ClearAllCallbacks() {
        mGlobalCallback = nullptr;
        mWindowCallbacks.clear();
        mTypedCallbacks.clear();
    }

    void NkWin32EventImpl::DispatchEvent(NkEvent* event, void* nativeHandle) {
        if (!event) return;

        // Callback de fenêtre
        if (nativeHandle) {
            HWND hwnd = static_cast<HWND>(nativeHandle);
            auto it = mWindowCallbacks.find(hwnd);
            if (it != mWindowCallbacks.end() && it->second) {
                it->second(event);
            }
        }

        // Callbacks typés
        auto it = mTypedCallbacks.find(event->GetType());
        if (it != mTypedCallbacks.end()) {
            auto callbacks = it->second; // copie pour tolérer les modifications
            for (auto& cb : callbacks) {
                if (cb) cb(event);
            }
        }

        // Callback global
        if (mGlobalCallback) {
            mGlobalCallback(event);
        }
    }

    // -------------------------------------------------------------------------
    // État / infos
    // -------------------------------------------------------------------------

    const NkEventState& NkWin32EventImpl::GetInputState() const { return mInputState; }
    NkEventState&       NkWin32EventImpl::GetInputState()       { return mInputState; }
    const char* NkWin32EventImpl::GetPlatformName() const noexcept { return "Win32"; }
    bool        NkWin32EventImpl::IsReady()         const noexcept { return true; }

    // -------------------------------------------------------------------------
    // Blit logiciel
    // -------------------------------------------------------------------------

    void NkWin32EventImpl::BlitToHwnd(HWND hwnd, const NkU8* rgba, NkU32 w, NkU32 h)
    {
        if (!hwnd || !rgba || !w || !h) return;
        std::vector<NkU8> bgra((size_t)w * h * 4);
        for (NkU32 i = 0; i < w * h; ++i) {
            bgra[i*4+0] = rgba[i*4+2];
            bgra[i*4+1] = rgba[i*4+1];
            bgra[i*4+2] = rgba[i*4+0];
            bgra[i*4+3] = rgba[i*4+3];
        }
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = (LONG)w;
        bmi.bmiHeader.biHeight      = -(LONG)h;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HDC hdc = GetDC(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        StretchDIBits(hdc, 0,0,rc.right-rc.left,rc.bottom-rc.top,
                      0,0,(int)w,(int)h, bgra.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        ReleaseDC(hwnd, hdc);
    }

    // -------------------------------------------------------------------------
    // WndProc statique
    // -------------------------------------------------------------------------

    LRESULT CALLBACK NkWin32EventImpl::WindowProcStatic(HWND hwnd, UINT msg,
                                                         WPARAM wp, LPARAM lp)
    {
        if (msg == WM_NCCREATE) {
            auto* cs     = reinterpret_cast<CREATESTRUCTW*>(lp);
            auto* window = static_cast<NkWin32WindowImpl*>(cs->lpCreateParams);
            if (window) gWindowRegistry().Insert(hwnd, window);
        }

        NkWin32WindowImpl* window = gWindowRegistry().Find(hwnd);

        if (msg == WM_NCDESTROY)
            gWindowRegistry().Remove(hwnd);

        if (!sCurrentImpl)
            return DefWindowProcW(hwnd, msg, wp, lp);

        return sCurrentImpl->ProcessWin32Message(hwnd, msg, wp, lp, window);
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    NkModifierState NkWin32EventImpl::CurrentMods()
    {
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

    // -------------------------------------------------------------------------
    // ProcessWin32Message
    //
    // RÈGLE : Enqueue() pousse SANS filtrer par IsValid().
    // -------------------------------------------------------------------------

    LRESULT NkWin32EventImpl::ProcessWin32Message(HWND hwnd, UINT msg, WPARAM wp,
                                                   LPARAM lp, NkWin32WindowImpl* owner)
    {
        LRESULT result              = 0;
        bool    suppressDefaultProc = false;

        // Pousse TOUJOURS dans la queue — pas de filtre IsValid().
        // Clone() préserve le type polymorphe de l'événement.
        auto Enqueue = [&](const NkEvent& evt) {
            auto clone = std::unique_ptr<NkEvent>(evt.Clone());
            DispatchEvent(clone.get(), hwnd); // Appel immédiat des callbacks
            mQueue.push(std::move(clone));    // Puis empilement pour le mode file
        };

        switch (msg)
        {
            case WM_CREATE: {
                NkWindowCreateEvent evt(
                    owner ? owner->GetConfig().width  : 0u,
                    owner ? owner->GetConfig().height : 0u);
                Enqueue(evt);
                break;
            }

            case WM_CLOSE: {
                NkWindowCloseEvent evt(false);
                Enqueue(evt);
                suppressDefaultProc = true;
                break;
            }

            case WM_DESTROY: {
                NkWindowDestroyEvent evt;
                Enqueue(evt);
                PostQuitMessage(0);
                break;
            }

            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                NkWindowPaintEvent evt(
                    (NkI32)ps.rcPaint.left, (NkI32)ps.rcPaint.top,
                    (NkU32)(ps.rcPaint.right  - ps.rcPaint.left),
                    (NkU32)(ps.rcPaint.bottom - ps.rcPaint.top));
                Enqueue(evt);
                break;
            }

            case WM_ERASEBKGND:
                result = 1;
                break;

            case WM_SETFOCUS: {
                NkWindowFocusGainedEvent evt;
                Enqueue(evt);
                break;
            }
            case WM_KILLFOCUS: {
                NkWindowFocusLostEvent evt;
                Enqueue(evt);
                break;
            }

            case WM_SHOWWINDOW: {
                if (wp) { NkWindowShownEvent  e; Enqueue(e); }
                else    { NkWindowHiddenEvent e; Enqueue(e); }
                break;
            }

            case WM_SIZE: {
                NkU32 nw = LOWORD(lp), nh = HIWORD(lp);
                NkWindowResizeEvent evt(nw, nh,
                    owner ? owner->GetConfig().width  : 0u,
                    owner ? owner->GetConfig().height : 0u);
                Enqueue(evt);
                if      (wp == SIZE_MINIMIZED) { NkWindowMinimizeEvent e; Enqueue(e); }
                else if (wp == SIZE_MAXIMIZED) { NkWindowMaximizeEvent e; Enqueue(e); }
                else if (wp == SIZE_RESTORED)  { NkWindowRestoreEvent  e; Enqueue(e); }
                break;
            }

            case WM_MOVE: {
                NkWindowMoveEvent evt((NkI32)LOWORD(lp), (NkI32)HIWORD(lp));
                Enqueue(evt);
                break;
            }

            case WM_DPICHANGED: {
                WORD dpi = HIWORD(wp);
                NkWindowDpiEvent evt(
                    (NkF32)dpi / USER_DEFAULT_SCREEN_DPI,
                    owner ? owner->GetDpiScale() : 1.f,
                    (NkU32)dpi);
                Enqueue(evt);
                const RECT* r = reinterpret_cast<const RECT*>(lp);
                if (r) SetWindowPos(hwnd, nullptr,
                    r->left, r->top, r->right-r->left, r->bottom-r->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }

            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                POINT pt = {x,y}; ClientToScreen(hwnd,&pt);
                NkMouseButtons buttons;
                SHORT mk = LOWORD(wp);
                if (mk&MK_LBUTTON)  buttons.Set(NkMouseButton::NK_MB_LEFT);
                if (mk&MK_RBUTTON)  buttons.Set(NkMouseButton::NK_MB_RIGHT);
                if (mk&MK_MBUTTON)  buttons.Set(NkMouseButton::NK_MB_MIDDLE);
                if (mk&MK_XBUTTON1) buttons.Set(NkMouseButton::NK_MB_BACK);
                if (mk&MK_XBUTTON2) buttons.Set(NkMouseButton::NK_MB_FORWARD);
                NkMouseMoveEvent evt(x,y,(NkI32)pt.x,(NkI32)pt.y,
                    x-mPrevMouseX,y-mPrevMouseY,buttons,CurrentMods());
                mPrevMouseX=x; mPrevMouseY=y;
                Enqueue(evt);
                break;
            }

            case WM_INPUT: {
                UINT sz = 0;
                GetRawInputData((HRAWINPUT)lp,RID_INPUT,nullptr,&sz,sizeof(RAWINPUTHEADER));
                if (sz > 0) {
                    std::vector<BYTE> buf(sz);
                    if (GetRawInputData((HRAWINPUT)lp,RID_INPUT,buf.data(),
                                        &sz,sizeof(RAWINPUTHEADER))==sz) {
                        auto* raw = reinterpret_cast<const RAWINPUT*>(buf.data());
                        if (raw->header.dwType==RIM_TYPEMOUSE) {
                            NkMouseRawEvent evt(raw->data.mouse.lLastX,
                                               raw->data.mouse.lLastY,0);
                            Enqueue(evt);
                        }
                    }
                }
                break;
            }

            case WM_MOUSEWHEEL: {
                POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd,&pt);
                SHORT mk=LOWORD(wp);
                double delta=GET_WHEEL_DELTA_WPARAM(wp)/(double)WHEEL_DELTA;
                NkModifierState mods;
                mods.ctrl=!!(mk&MK_CONTROL); mods.shift=!!(mk&MK_SHIFT);
                NkMouseWheelVerticalEvent evt(delta,(NkI32)pt.x,(NkI32)pt.y,0.0,false,mods);
                Enqueue(evt);
                break;
            }

            case WM_MOUSEHWHEEL: {
                POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd,&pt);
                double delta=GET_WHEEL_DELTA_WPARAM(wp)/(double)WHEEL_DELTA;
                NkMouseWheelHorizontalEvent evt(delta,(NkI32)pt.x,(NkI32)pt.y);
                Enqueue(evt);
                break;
            }

#define NK_MB_PRESS(Button) {                                                 \
    POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};                            \
    POINT sp=pt; ClientToScreen(hwnd,&sp);                                    \
    NkMouseButtonPressEvent evt(NkMouseButton::Button,                        \
        pt.x,pt.y,sp.x,sp.y,1,CurrentMods()); Enqueue(evt); } break

#define NK_MB_RELEASE(Button) {                                               \
    POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};                            \
    POINT sp=pt; ClientToScreen(hwnd,&sp);                                    \
    NkMouseButtonReleaseEvent evt(NkMouseButton::Button,                      \
        pt.x,pt.y,sp.x,sp.y,1,CurrentMods()); Enqueue(evt); } break

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
                POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
                POINT sp=pt; ClientToScreen(hwnd,&sp);
                NkMouseButton btn=
                    (msg==WM_LBUTTONDBLCLK)?NkMouseButton::NK_MB_LEFT:
                    (msg==WM_RBUTTONDBLCLK)?NkMouseButton::NK_MB_RIGHT:
                                            NkMouseButton::NK_MB_MIDDLE;
                NkMouseDoubleClickEvent evt(btn,pt.x,pt.y,sp.x,sp.y,CurrentMods());
                Enqueue(evt);
                break;
            }

            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP: {
                POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
                POINT sp=pt; ClientToScreen(hwnd,&sp);
                NkMouseButton btn=(HIWORD(wp)&XBUTTON1)?
                    NkMouseButton::NK_MB_BACK:NkMouseButton::NK_MB_FORWARD;
                if (msg==WM_XBUTTONDOWN) {
                    NkMouseButtonPressEvent   e(btn,pt.x,pt.y,sp.x,sp.y,1,CurrentMods());
                    Enqueue(e);
                } else {
                    NkMouseButtonReleaseEvent e(btn,pt.x,pt.y,sp.x,sp.y,1,CurrentMods());
                    Enqueue(e);
                }
                break;
            }

            case WM_MOUSELEAVE: {
                NkMouseLeaveEvent evt; Enqueue(evt);
                break;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                UINT sc    = (lp>>16)&0xFF;
                bool isExt = (lp>>24)&1;
                bool isRep = (lp>>30)&1;
                NkScancode nkSc = NkScancodeFromWin32(sc,isExt);
                NkKey k = NkScancodeToKey(nkSc);
                if (k==NkKey::NK_UNKNOWN)
                    k = NkKeycodeMap::NkKeyFromWin32VK((UINT)wp,isExt);
                if (k != NkKey::NK_UNKNOWN) {
                    bool isPress = (msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN);
                    NkModifierState mods=CurrentMods();
                    UINT nk=(UINT)wp;
                    if (isPress&&isRep) { NkKeyRepeatEvent  e(k,1,nkSc,mods,nk,isExt); Enqueue(e); }
                    else if (isPress)   { NkKeyPressEvent   e(k,nkSc,mods,nk,isExt);   Enqueue(e); }
                    else                { NkKeyReleaseEvent e(k,nkSc,mods,nk,isExt);   Enqueue(e); }
                }
                break;
            }

            case WM_CHAR: {
                UINT cp=(UINT)wp;
                if (cp>=32&&cp!=127) {
                    NkTextInputEvent evt(cp); Enqueue(evt);
                }
                break;
            }

            case WM_NCHITTEST:
                if (owner&&!owner->GetConfig().frame) {
                    RECT rc; GetWindowRect(hwnd,&rc);
                    int x=GET_X_LPARAM(lp)-rc.left, y=GET_Y_LPARAM(lp)-rc.top;
                    int w=rc.right-rc.left, h=rc.bottom-rc.top;
                    int b=IsZoomed(hwnd)?0:5;
                    if (x<b&&y<b)            {result=HTTOPLEFT;    break;}
                    if (x>w-b&&y<b)          {result=HTTOPRIGHT;   break;}
                    if (x<b&&y>h-b)          {result=HTBOTTOMLEFT; break;}
                    if (x>w-b&&y>h-b)        {result=HTBOTTOMRIGHT;break;}
                    if (x<b)                 {result=HTLEFT;       break;}
                    if (x>w-b)               {result=HTRIGHT;      break;}
                    if (y<b)                 {result=HTTOP;        break;}
                    if (y>h-b)               {result=HTBOTTOM;     break;}
                    if (y<32&&x>260&&x<w-260){result=HTCAPTION;    break;}
                    result=HTCLIENT;
                }
                break;

            case WM_GETMINMAXINFO:
                if (owner) {
                    auto* mm=reinterpret_cast<MINMAXINFO*>(lp);
                    mm->ptMinTrackSize.x=(LONG)owner->GetConfig().minWidth;
                    mm->ptMinTrackSize.y=(LONG)owner->GetConfig().minHeight;
                }
                break;

            default:
                break;
        }

        if (suppressDefaultProc) return 0;
        return result ? result : DefWindowProcW(hwnd,msg,wp,lp);
    }

    // -------------------------------------------------------------------------
    // NkKeycodeMap
    // -------------------------------------------------------------------------

    NkKey NkKeycodeMap::NkKeyFromWin32VK(NkU32 vk, bool extended) noexcept
    {
        switch (vk) {
            case VK_ESCAPE:     return NkKey::NK_ESCAPE;
            case VK_F1:         return NkKey::NK_F1;   case VK_F2:  return NkKey::NK_F2;
            case VK_F3:         return NkKey::NK_F3;   case VK_F4:  return NkKey::NK_F4;
            case VK_F5:         return NkKey::NK_F5;   case VK_F6:  return NkKey::NK_F6;
            case VK_F7:         return NkKey::NK_F7;   case VK_F8:  return NkKey::NK_F8;
            case VK_F9:         return NkKey::NK_F9;   case VK_F10: return NkKey::NK_F10;
            case VK_F11:        return NkKey::NK_F11;  case VK_F12: return NkKey::NK_F12;
            case VK_F13:        return NkKey::NK_F13;  case VK_F14: return NkKey::NK_F14;
            case VK_F15:        return NkKey::NK_F15;  case VK_F16: return NkKey::NK_F16;
            case VK_F17:        return NkKey::NK_F17;  case VK_F18: return NkKey::NK_F18;
            case VK_F19:        return NkKey::NK_F19;  case VK_F20: return NkKey::NK_F20;
            case VK_F21:        return NkKey::NK_F21;  case VK_F22: return NkKey::NK_F22;
            case VK_F23:        return NkKey::NK_F23;  case VK_F24: return NkKey::NK_F24;
            case VK_OEM_3:      return NkKey::NK_GRAVE;
            case '1': return NkKey::NK_NUM1; case '2': return NkKey::NK_NUM2;
            case '3': return NkKey::NK_NUM3; case '4': return NkKey::NK_NUM4;
            case '5': return NkKey::NK_NUM5; case '6': return NkKey::NK_NUM6;
            case '7': return NkKey::NK_NUM7; case '8': return NkKey::NK_NUM8;
            case '9': return NkKey::NK_NUM9; case '0': return NkKey::NK_NUM0;
            case VK_OEM_MINUS:  return NkKey::NK_MINUS;
            case VK_OEM_PLUS:   return NkKey::NK_EQUALS;
            case VK_BACK:       return NkKey::NK_BACK;
            case VK_TAB:        return NkKey::NK_TAB;
            case 'Q': return NkKey::NK_Q; case 'W': return NkKey::NK_W;
            case 'E': return NkKey::NK_E; case 'R': return NkKey::NK_R;
            case 'T': return NkKey::NK_T; case 'Y': return NkKey::NK_Y;
            case 'U': return NkKey::NK_U; case 'I': return NkKey::NK_I;
            case 'O': return NkKey::NK_O; case 'P': return NkKey::NK_P;
            case VK_OEM_4:      return NkKey::NK_LBRACKET;
            case VK_OEM_6:      return NkKey::NK_RBRACKET;
            case VK_OEM_5:      return NkKey::NK_BACKSLASH;
            case VK_CAPITAL:    return NkKey::NK_CAPSLOCK;
            case 'A': return NkKey::NK_A; case 'S': return NkKey::NK_S;
            case 'D': return NkKey::NK_D; case 'F': return NkKey::NK_F;
            case 'G': return NkKey::NK_G; case 'H': return NkKey::NK_H;
            case 'J': return NkKey::NK_J; case 'K': return NkKey::NK_K;
            case 'L': return NkKey::NK_L;
            case VK_OEM_1:      return NkKey::NK_SEMICOLON;
            case VK_OEM_7:      return NkKey::NK_APOSTROPHE;
            case VK_RETURN:     return extended ? NkKey::NK_NUMPAD_ENTER : NkKey::NK_ENTER;
            case VK_LSHIFT:     return NkKey::NK_LSHIFT;
            case VK_RSHIFT:     return NkKey::NK_RSHIFT;
            case 'Z': return NkKey::NK_Z; case 'X': return NkKey::NK_X;
            case 'C': return NkKey::NK_C; case 'V': return NkKey::NK_V;
            case 'B': return NkKey::NK_B; case 'N': return NkKey::NK_N;
            case 'M': return NkKey::NK_M;
            case VK_OEM_COMMA:  return NkKey::NK_COMMA;
            case VK_OEM_PERIOD: return NkKey::NK_PERIOD;
            case VK_OEM_2:      return NkKey::NK_SLASH;
            case VK_LCONTROL:   return NkKey::NK_LCTRL;
            case VK_RCONTROL:   return NkKey::NK_RCTRL;
            case VK_LWIN:       return NkKey::NK_LSUPER;
            case VK_RWIN:       return NkKey::NK_RSUPER;
            case VK_LMENU:      return NkKey::NK_LALT;
            case VK_RMENU:      return NkKey::NK_RALT;
            case VK_SPACE:      return NkKey::NK_SPACE;
            case VK_APPS:       return NkKey::NK_MENU;
            case VK_CONTROL:    return extended ? NkKey::NK_RCTRL  : NkKey::NK_LCTRL;
            case VK_MENU:       return extended ? NkKey::NK_RALT   : NkKey::NK_LALT;
            case VK_SHIFT:      return NkKey::NK_LSHIFT;
            case VK_SNAPSHOT:   return NkKey::NK_PRINT_SCREEN;
            case VK_SCROLL:     return NkKey::NK_SCROLL_LOCK;
            case VK_PAUSE:      return NkKey::NK_PAUSE_BREAK;
            case VK_INSERT:     return extended ? NkKey::NK_INSERT    : NkKey::NK_NUMPAD_0;
            case VK_DELETE:     return extended ? NkKey::NK_DELETE    : NkKey::NK_NUMPAD_DOT;
            case VK_HOME:       return extended ? NkKey::NK_HOME      : NkKey::NK_NUMPAD_7;
            case VK_END:        return extended ? NkKey::NK_END       : NkKey::NK_NUMPAD_1;
            case VK_PRIOR:      return extended ? NkKey::NK_PAGE_UP   : NkKey::NK_NUMPAD_9;
            case VK_NEXT:       return extended ? NkKey::NK_PAGE_DOWN : NkKey::NK_NUMPAD_3;
            case VK_UP:         return extended ? NkKey::NK_UP    : NkKey::NK_NUMPAD_8;
            case VK_DOWN:       return extended ? NkKey::NK_DOWN  : NkKey::NK_NUMPAD_2;
            case VK_LEFT:       return extended ? NkKey::NK_LEFT  : NkKey::NK_NUMPAD_4;
            case VK_RIGHT:      return extended ? NkKey::NK_RIGHT : NkKey::NK_NUMPAD_6;
            case VK_NUMLOCK:    return NkKey::NK_NUM_LOCK;
            case VK_DIVIDE:     return NkKey::NK_NUMPAD_DIV;
            case VK_MULTIPLY:   return NkKey::NK_NUMPAD_MUL;
            case VK_SUBTRACT:   return NkKey::NK_NUMPAD_SUB;
            case VK_ADD:        return NkKey::NK_NUMPAD_ADD;
            case VK_DECIMAL:    return NkKey::NK_NUMPAD_DOT;
            case VK_NUMPAD0:    return NkKey::NK_NUMPAD_0;
            case VK_NUMPAD1:    return NkKey::NK_NUMPAD_1;
            case VK_NUMPAD2:    return NkKey::NK_NUMPAD_2;
            case VK_NUMPAD3:    return NkKey::NK_NUMPAD_3;
            case VK_NUMPAD4:    return NkKey::NK_NUMPAD_4;
            case VK_NUMPAD5:    return NkKey::NK_NUMPAD_5;
            case VK_NUMPAD6:    return NkKey::NK_NUMPAD_6;
            case VK_NUMPAD7:    return NkKey::NK_NUMPAD_7;
            case VK_NUMPAD8:    return NkKey::NK_NUMPAD_8;
            case VK_NUMPAD9:    return NkKey::NK_NUMPAD_9;
            case VK_CLEAR:      return NkKey::NK_CLEAR;
            case VK_MEDIA_PLAY_PAUSE:  return NkKey::NK_MEDIA_PLAY_PAUSE;
            case VK_MEDIA_STOP:        return NkKey::NK_MEDIA_STOP;
            case VK_MEDIA_NEXT_TRACK:  return NkKey::NK_MEDIA_NEXT;
            case VK_MEDIA_PREV_TRACK:  return NkKey::NK_MEDIA_PREV;
            case VK_VOLUME_UP:         return NkKey::NK_MEDIA_VOLUME_UP;
            case VK_VOLUME_DOWN:       return NkKey::NK_MEDIA_VOLUME_DOWN;
            case VK_VOLUME_MUTE:       return NkKey::NK_MEDIA_MUTE;
            case VK_BROWSER_BACK:      return NkKey::NK_BROWSER_BACK;
            case VK_BROWSER_FORWARD:   return NkKey::NK_BROWSER_FORWARD;
            case VK_BROWSER_REFRESH:   return NkKey::NK_BROWSER_REFRESH;
            case VK_BROWSER_HOME:      return NkKey::NK_BROWSER_HOME;
            case VK_BROWSER_SEARCH:    return NkKey::NK_BROWSER_SEARCH;
            case VK_BROWSER_FAVORITES: return NkKey::NK_BROWSER_FAVORITES;
            case VK_KANA:       return NkKey::NK_KANA;
            case VK_KANJI:      return NkKey::NK_KANJI;
            case VK_CONVERT:    return NkKey::NK_CONVERT;
            case VK_NONCONVERT: return NkKey::NK_NONCONVERT;
            case VK_SLEEP:      return NkKey::NK_SLEEP;
            case VK_SEPARATOR:  return NkKey::NK_SEPARATOR;
            case VK_OEM_8:      return NkKey::NK_OEM_8;
            case VK_OEM_102:    return NkKey::NK_OEM_1;
            default:            return NkKey::NK_UNKNOWN;
        }
    }

    NkKey NkKeycodeMap::NkKeyFromWin32Scancode(NkU32 sc, bool extended) noexcept
    {
        NkScancode usbSc = NkScancodeFromWin32(sc, extended);
        NkKey k = NkScancodeToKey(usbSc);
        if (k == NkKey::NK_UNKNOWN) {
            UINT vk = MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX);
            if (vk != 0) k = NkKeyFromWin32VK((NkU32)vk, extended);
        }
        return k;
    }

} // namespace nkentseu