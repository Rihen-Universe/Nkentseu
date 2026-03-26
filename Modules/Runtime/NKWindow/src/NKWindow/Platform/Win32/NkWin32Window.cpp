// =============================================================================
// NkWin32Window.cpp
// Implémentation Win32 de NkWindow sans PIMPL.
//
// Points appliqués :
//   Point 2 : sWin32WindowMap et sWin32LastWindow sont static dans ce .cpp
//   Point 4 : WM_DESTROY conditionne PostQuitMessage à la dernière fenêtre
//   Point 6 : OleInitialize/OleUninitialize retirés — gérés par NkSystem
//   Point 7 : Synchronisation complète entre mData et mConfig
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NkWin32Window.h"
#include "NKWindow/Platform/Win32/NkWin32DropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKWindow/Platform/Win32/NkWin32EventSystem.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/String/NkWString.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <string>
#include <unordered_map>
#include <vector>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Dynamic loading of DPI-aware APIs (Windows 10 1607+)
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#   define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

typedef DPI_AWARENESS_CONTEXT (WINAPI* SetThreadDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
typedef UINT                  (WINAPI* GetDpiForWindowProc)(HWND);

namespace {

    SetThreadDpiAwarenessContextProc pSetThreadDpiAwarenessContext = nullptr;
    GetDpiForWindowProc              pGetDpiForWindow              = nullptr;
    bool                             sDpiAPIsInitialized           = false;

    const IID kIIDTaskbarList3 = {
        0xEA1AFB91, 0x9E28, 0x4B86,
        { 0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEA, 0x84 }
    };

    void InitializeDpiAPIs() {
        if (sDpiAPIsInitialized) return;
        sDpiAPIsInitialized = true;
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32) {
            pSetThreadDpiAwarenessContext = (SetThreadDpiAwarenessContextProc)
                GetProcAddress(user32, "SetThreadDpiAwarenessContext");
            pGetDpiForWindow = (GetDpiForWindowProc)
                GetProcAddress(user32, "GetDpiForWindow");
        }
    }

    DPI_AWARENESS_CONTEXT NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT ctx) {
        return pSetThreadDpiAwarenessContext ? pSetThreadDpiAwarenessContext(ctx) : nullptr;
    }

    UINT NkGetDpiForWindow(HWND hwnd) {
        return pGetDpiForWindow ? pGetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
    }

} // anonymous namespace

namespace nkentseu {
    using namespace math;

    // =============================================================================
    // Point 2 : registre backend — static dans ce .cpp, invisible à l'extérieur
    // =============================================================================

    // Function-local statics to avoid SIOF with the custom allocator
    static NkUnorderedMap<HWND, NkWindow*>& Win32WindowMap() {
        static NkUnorderedMap<HWND, NkWindow*> sMap;
        return sMap;
    }
    static NkWindow*& Win32LastWindow() {
        static NkWindow* sLast = nullptr;
        return sLast;
    }

    NkWindow* NkWin32FindWindow(HWND hwnd) {
        auto* win = Win32WindowMap().Find(hwnd);
        return win ? *win : nullptr;
    }

    void NkWin32RegisterWindow(HWND hwnd, NkWindow* win) {
        Win32WindowMap()[hwnd] = win;
        Win32LastWindow()      = win;
    }

    void NkWin32UnregisterWindow(HWND hwnd) {
        auto& map = Win32WindowMap();
        auto* win = map.Find(hwnd);
        if (!win) return;

        NkWindow* w = *win;
        map.Erase(hwnd);

        if (Win32LastWindow() == w) {
            NkWindow* first = nullptr;
            map.ForEach([&](HWND, NkWindow* v) { if (!first) first = v; });
            Win32LastWindow() = first;
        }
    }

    NkWindow* NkWin32GetLastWindow() {
        return Win32LastWindow();
    }

    // =============================================================================
    // Helpers UTF-8 ↔ Wide
    // =============================================================================

    static NkWString NkUtf8ToWide(const NkString& s) {
        if (s.Empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), (int)s.Size(), nullptr, 0);
        NkWString ws((size_t)len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.CStr(), (int)s.Size(), ws.Data(), len);
        return ws;
    }

    static NkString NkWideToUtf8(const NkWString& ws) {
        if (ws.Empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), (int)ws.Size(),
                                    nullptr, 0, nullptr, nullptr);
        NkString s((size_t)len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), (int)ws.Size(),
                            s.Data(), len, nullptr, nullptr);
        return NkString(s.CStr());
    }

    static void DestroyWindowIcons(NkWindowData& data) {
        if (data.mIconBig && data.mIconBig != data.mIconSmall) {
            DestroyIcon(data.mIconBig);
        }
        if (data.mIconSmall) {
            DestroyIcon(data.mIconSmall);
        }
        data.mIconSmall = nullptr;
        data.mIconBig = nullptr;
    }

    static void ApplyWindowIcons(HWND hwnd, NkWindowData& data, const NkString& iconPath) {
        if (!hwnd || iconPath.Empty()) return;

        DestroyWindowIcons(data);

        const NkWString wPath = NkUtf8ToWide(iconPath);
        const int smallW = GetSystemMetrics(SM_CXSMICON);
        const int smallH = GetSystemMetrics(SM_CYSMICON);
        const int bigW   = GetSystemMetrics(SM_CXICON);
        const int bigH   = GetSystemMetrics(SM_CYICON);

        HICON smallIcon = reinterpret_cast<HICON>(
            LoadImageW(nullptr, wPath.CStr(), IMAGE_ICON, smallW, smallH, LR_LOADFROMFILE));
        HICON bigIcon = reinterpret_cast<HICON>(
            LoadImageW(nullptr, wPath.CStr(), IMAGE_ICON, bigW, bigH, LR_LOADFROMFILE));

        if (!smallIcon && !bigIcon) return;

        data.mIconSmall = smallIcon ? smallIcon : bigIcon;
        data.mIconBig = bigIcon ? bigIcon : smallIcon;

        if (data.mIconSmall) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(data.mIconSmall));
        }
        if (data.mIconBig) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(data.mIconBig));
        }
    }

    static void ApplyTransparency(HWND hwnd) {
        if (!hwnd) return;
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.fTransitionOnMaximized = FALSE;
        HRGN region = CreateRectRgn(-1, -1, 0, 0);
        bb.hRgnBlur = region;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        if (region) DeleteObject(region);
    }

    static void ApplyShadow(HWND hwnd) {
        if (!hwnd) return;
        BOOL ncr = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncr, sizeof(ncr));
        const MARGINS shadow = { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(hwnd, &shadow);
    }

    // =============================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =============================================================================

    static void SyncConfigFromWindow(HWND hwnd, NkWindowConfig& config, const NkWindowData& data) {
        if (!hwnd) return;

        // Récupérer la position
        RECT winRect;
        GetWindowRect(hwnd, &winRect);
        config.x = winRect.left;
        config.y = winRect.top;

        // Récupérer la taille client
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        config.width = clientRect.right - clientRect.left;
        config.height = clientRect.bottom - clientRect.top;

        // État fenêtré / plein écran
        config.fullscreen = (GetWindowLongW(hwnd, GWL_STYLE) & WS_POPUP) != 0;

        // Visibilité
        config.visible = IsWindowVisible(hwnd) != 0;

        // Titre
        int len = GetWindowTextLengthW(hwnd);
        if (len > 0) {
            NkWString ws((size_t)len + 1, L'\0');
            GetWindowTextW(hwnd, ws.Data(), len + 1);
            ws.Resize((size_t)len);
            config.title = NkWideToUtf8(ws);
        }
    }

    static void SyncWindowFromConfig(HWND hwnd, const NkWindowConfig& config) {
        if (!hwnd) return;

        // Titre
        SetWindowTextW(hwnd, NkUtf8ToWide(config.title).CStr());

        // Redimensionner si nécessaire
        RECT currentRect;
        GetClientRect(hwnd, &currentRect);
        uint32 currentW = currentRect.right - currentRect.left;
        uint32 currentH = currentRect.bottom - currentRect.top;
        
        if (currentW != config.width || currentH != config.height) {
            RECT rc = { 0, 0, (LONG)config.width, (LONG)config.height };
            DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
            DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
            AdjustWindowRectEx(&rc, style, FALSE, exStyle);
            SetWindowPos(hwnd, nullptr, 0, 0,
                        rc.right - rc.left, rc.bottom - rc.top,
                        SWP_NOMOVE | SWP_NOZORDER);
        }

        // Position
        RECT winRect;
        GetWindowRect(hwnd, &winRect);
        if (winRect.left != config.x || winRect.top != config.y) {
            SetWindowPos(hwnd, nullptr, config.x, config.y, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER);
        }

        // Visibilité
        bool isVisible = IsWindowVisible(hwnd) != 0;
        if (isVisible != config.visible) {
            ShowWindow(hwnd, config.visible ? SW_SHOW : SW_HIDE);
        }
    }

    // =============================================================================
    // NkWindow — constructeurs / destructeur
    // =============================================================================

    NkWindow::NkWindow() {}

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) Close();
    }

    // =============================================================================
    // Create
    // =============================================================================

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;
        mData = {};
        mData.mHInstance = GetModuleHandleW(nullptr);
        mData.mAppliedHints = config.surfaceHints;

        if (config.native.win32PixelFormatShareWindowHandle != 0) {
            mData.mAppliedHints.Set(
                NkSurfaceHintKey::NK_WGL_SHARE_PIXEL_FORMAT_HWND,
                config.native.win32PixelFormatShareWindowHandle);
        }

        const bool useExternal = config.native.useExternalWindow &&
                                 config.native.externalWindowHandle != 0;

        mId = NkSystem::Instance().RegisterWindow(this);
        if (mId == NK_INVALID_WINDOW_ID) {
            mLastError = NkError(1, "RegisterWindow failed");
            return false;
        }

        auto setupDropTarget = [&]() {
            if (!config.dropEnabled || !mData.mHwnd) return;
            mData.mDropTarget = new NkWin32DropTarget(mData.mHwnd);
            if (!mData.mDropTarget) return;
            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& ev) {
                NkDropEnterEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& ev) {
                NkDropLeaveEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& ev) {
                NkDropFileEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& ev) {
                NkDropTextEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
        };

        if (useExternal) {
            HWND hwnd = reinterpret_cast<HWND>(config.native.externalWindowHandle);
            if (!hwnd || !IsWindow(hwnd)) {
                mLastError = NkError(1, "External HWND is invalid.");
                NkSystem::Instance().UnregisterWindow(mId);
                mId = NK_INVALID_WINDOW_ID;
                return false;
            }

            mData.mHwnd = hwnd;
            mData.mExternal = true;
            mData.mHInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            mData.mDwStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
            mData.mDwExStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
            mData.mParentHwnd = GetParent(hwnd);
            mData.mPrevUserData = GetWindowLongPtrW(hwnd, GWLP_USERDATA);

            ::SetLastError(0);
            mData.mPrevWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(NkWin32WndProc)));
            if (!mData.mPrevWndProc && ::GetLastError() != 0) {
                mLastError = NkError(2, NkString::Fmtf("Subclass external HWND failed (%lu)", ::GetLastError()));
                NkSystem::Instance().UnregisterWindow(mId);
                mId = NK_INVALID_WINDOW_ID;
                mData = {};
                return false;
            }

            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            NkWin32RegisterWindow(hwnd, this);
            setupDropTarget();

            CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                             kIIDTaskbarList3, reinterpret_cast<void**>(&mData.mTaskbarList));

            // Synchroniser mConfig depuis l'état réel de la fenêtre externe
            SyncConfigFromWindow(hwnd, mConfig, mData);

            mIsOpen = true;
            return true;
        }

        NkWString wClassName = NkUtf8ToWide(config.name);
        NkWString wTitle = NkUtf8ToWide(config.title);

        if (config.fullscreen) {
            DEVMODE dm = {};
            dm.dmSize = sizeof(DEVMODE);
            dm.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
            dm.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
            dm.dmBitsPerPel = 32;
            dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            mData.mDmScreen = dm;
            ChangeDisplaySettingsW(&mData.mDmScreen, CDS_FULLSCREEN);
            mData.mDwExStyle = WS_EX_APPWINDOW;
            mData.mDwStyle = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        } else {
            mData.mDwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
            mData.mDwStyle = config.frame
                ? WS_OVERLAPPEDWINDOW
                : (WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        }

        mData.mParentHwnd = reinterpret_cast<HWND>(config.native.parentWindowHandle);

        if (config.native.utilityWindow) {
            mData.mDwExStyle |= WS_EX_TOOLWINDOW;
            mData.mDwExStyle &= ~WS_EX_APPWINDOW;
        }
        if (config.transparent) {
            mData.mDwExStyle |= WS_EX_LAYERED;
        }

        if (config.native.utilityWindow && !mData.mParentHwnd) {
            mData.mUtilityOwner = CreateWindowExW(
                0, L"STATIC", L"",
                WS_POPUP,
                0, 0, 1, 1,
                nullptr, nullptr, mData.mHInstance, nullptr);
            mData.mParentHwnd = mData.mUtilityOwner;
        }

        RECT rc = {
            config.x, config.y,
            config.x + static_cast<LONG>(config.width),
            config.y + static_cast<LONG>(config.height)
        };
        AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = NkWin32WndProc;
        wc.hInstance = mData.mHInstance;
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = wClassName.CStr();
        wc.hIconSm = LoadIconW(nullptr, IDI_WINLOGO);
        RegisterClassExW(&wc);

        InitializeDpiAPIs();
        DPI_AWARENESS_CONTEXT prev =
            NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        mData.mHwnd = CreateWindowExW(
            mData.mDwExStyle,
            wClassName.CStr(),
            wTitle.CStr(),
            mData.mDwStyle,
            0, 0,
            rc.right - rc.left,
            rc.bottom - rc.top,
            mData.mParentHwnd,
            nullptr,
            mData.mHInstance,
            this);

        NkSetThreadDpiAwarenessContext(prev);

        if (!mData.mHwnd) {
            mLastError = NkError(3, NkString::Fmtf("CreateWindowExW failed (%lu)", ::GetLastError()));
            NkSystem::Instance().UnregisterWindow(mId);
            mId = NK_INVALID_WINDOW_ID;
            if (mData.mUtilityOwner) {
                DestroyWindow(mData.mUtilityOwner);
                mData.mUtilityOwner = nullptr;
            }
            return false;
        }

        if (!config.fullscreen) {
            const int sw = GetSystemMetrics(SM_CXSCREEN);
            const int sh = GetSystemMetrics(SM_CYSCREEN);
            const int ww = rc.right - rc.left;
            const int wh = rc.bottom - rc.top;
            if (config.centered) {
                SetWindowPos(mData.mHwnd, nullptr, (sw - ww) / 2, (sh - wh) / 2, ww, wh, SWP_NOZORDER);
            } else {
                SetWindowPos(mData.mHwnd, nullptr, config.x, config.y, ww, wh, SWP_NOZORDER);
            }
        }

        if (config.transparent) {
            ApplyTransparency(mData.mHwnd);
        } else if (config.hasShadow) {
            ApplyShadow(mData.mHwnd);
        }

        ApplyWindowIcons(mData.mHwnd, mData, config.iconPath);

        CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                         kIIDTaskbarList3, reinterpret_cast<void**>(&mData.mTaskbarList));

        setupDropTarget();

        if (config.visible) {
            ShowWindow(mData.mHwnd, SW_SHOWNORMAL);
            SetForegroundWindow(mData.mHwnd);
            SetFocus(mData.mHwnd);
        }

        // Synchronisation initiale : mConfig reflète l'état réel
        SyncConfigFromWindow(mData.mHwnd, mConfig, mData);

        mIsOpen = true;
        return true;
    }

    // =============================================================================
    // Close
    // =============================================================================

    void NkWindow::Close() {
        if (!mIsOpen) return;

        const HWND hwnd = mData.mHwnd;
        NkWin32UnregisterWindow(hwnd);
        NkSystem::Instance().UnregisterWindow(mId);

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mTaskbarList) {
            mData.mTaskbarList->Release();
            mData.mTaskbarList = nullptr;
        }

        if (hwnd) {
            if (mData.mExternal) {
                if (mData.mPrevWndProc &&
                    mData.mPrevWndProc != NkWin32WndProc) {
                    SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(mData.mPrevWndProc));
                }
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, mData.mPrevUserData);
            } else {
                DestroyWindow(hwnd);
            }
        }

        if (!mData.mExternal && mData.mHInstance) {
            UnregisterClassW(NkUtf8ToWide(mConfig.name).CStr(), mData.mHInstance);
        }

        if (mData.mUtilityOwner) {
            DestroyWindow(mData.mUtilityOwner);
            mData.mUtilityOwner = nullptr;
        }

        DestroyWindowIcons(mData);

        mData.mHwnd = nullptr;
        mData.mPrevWndProc = nullptr;
        mData.mPrevUserData = 0;
        mData.mExternal = false;

        mId     = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
    }

    // =============================================================================
    // State
    // =============================================================================

    bool NkWindow::IsOpen()  const { return mIsOpen; }
    bool NkWindow::IsValid() const { return mIsOpen && mData.mHwnd != nullptr; }

    NkError        NkWindow::GetLastError() const { return mLastError; }
    NkWindowConfig NkWindow::GetConfig()    const { 
        // Synchroniser avant de retourner
        if (mIsOpen && mData.mHwnd) {
            NkWindowConfig updatedConfig = mConfig;
            SyncConfigFromWindow(mData.mHwnd, updatedConfig, mData);
            const_cast<NkWindow*>(this)->mConfig = updatedConfig;
        }
        return mConfig; 
    }

    // =============================================================================
    // Title
    // =============================================================================

    NkString NkWindow::GetTitle() const {
        if (!mData.mHwnd) return {};
        int len = GetWindowTextLengthW(mData.mHwnd);
        if (len <= 0) return {};
        NkWString ws((size_t)len + 1, L'\0');
        GetWindowTextW(mData.mHwnd, ws.Data(), len + 1);
        ws.Resize((size_t)len);
        NkString title = NkWideToUtf8(ws);
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.title = title;
        
        return title;
    }

    void NkWindow::SetTitle(const NkString& t) {
        mConfig.title = t;
        if (mData.mHwnd) {
            SetWindowTextW(mData.mHwnd, NkUtf8ToWide(t).CStr());
            // La synchronisation est déjà faite via la modification de mConfig
        }
    }

    // =============================================================================
    // Size / Position
    // =============================================================================

    NkVec2u NkWindow::GetSize() const {
        RECT rc = {};
        if (mData.mHwnd) GetClientRect(mData.mHwnd, &rc);
        NkVec2u size = { (uint32)(rc.right - rc.left), (uint32)(rc.bottom - rc.top) };
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.width = size.x;
        const_cast<NkWindow*>(this)->mConfig.height = size.y;
        
        return size;
    }

    NkVec2u NkWindow::GetPosition() const {
        RECT rc = {};
        if (mData.mHwnd) GetWindowRect(mData.mHwnd, &rc);
        NkVec2u pos = { (uint32)rc.left, (uint32)rc.top };
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.x = pos.x;
        const_cast<NkWindow*>(this)->mConfig.y = pos.y;
        
        return pos;
    }

    float NkWindow::GetDpiScale() const {
        return mData.mHwnd
            ? (float)NkGetDpiForWindow(mData.mHwnd) / USER_DEFAULT_SCREEN_DPI
            : 1.f;
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        return { (uint32)GetSystemMetrics(SM_CXSCREEN),
                (uint32)GetSystemMetrics(SM_CYSCREEN) };
    }

    NkVec2u NkWindow::GetDisplayPosition() const { return { 0, 0 }; }

    void NkWindow::SetSize(uint32 w, uint32 h) {
        mConfig.width = w;
        mConfig.height = h;
        
        RECT rc = { 0, 0, (LONG)w, (LONG)h };
        AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);
        SetWindowPos(mData.mHwnd, nullptr, 0, 0,
                    rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOMOVE | SWP_NOZORDER);
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        mConfig.x = x;
        mConfig.y = y;
        
        SetWindowPos(mData.mHwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    void NkWindow::SetVisible(bool v) { 
        mConfig.visible = v;
        ShowWindow(mData.mHwnd, v ? SW_SHOW : SW_HIDE); 
    }
    
    void NkWindow::Minimize() { 
        ShowWindow(mData.mHwnd, SW_MINIMIZE); 
        // L'état de visibilité change, mais pas la taille/position
        mConfig.visible = IsWindowVisible(mData.mHwnd) != 0;
    }
    
    void NkWindow::Maximize() { 
        ShowWindow(mData.mHwnd, IsZoomed(mData.mHwnd) ? SW_RESTORE : SW_MAXIMIZE); 
        // Mettre à jour la taille après maximisation
        RECT rc;
        GetClientRect(mData.mHwnd, &rc);
        mConfig.width = rc.right - rc.left;
        mConfig.height = rc.bottom - rc.top;
        mConfig.visible = true;
    }
    
    void NkWindow::Restore() { 
        ShowWindow(mData.mHwnd, SW_RESTORE); 
        // Mettre à jour après restauration
        RECT rc;
        GetClientRect(mData.mHwnd, &rc);
        mConfig.width = rc.right - rc.left;
        mConfig.height = rc.bottom - rc.top;
        mConfig.visible = true;
    }

    void NkWindow::SetFullscreen(bool fs) {
        mConfig.fullscreen = fs;
        
        if (fs) {
            SetWindowLongW(mData.mHwnd, GWL_STYLE,
                        WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            SetWindowPos(mData.mHwnd, HWND_TOP, 0, 0,
                        GetSystemMetrics(SM_CXSCREEN),
                        GetSystemMetrics(SM_CYSCREEN),
                        SWP_FRAMECHANGED);
            
            // Mettre à jour la taille
            mConfig.width = GetSystemMetrics(SM_CXSCREEN);
            mConfig.height = GetSystemMetrics(SM_CYSCREEN);
        } else {
            SetWindowLongW(mData.mHwnd, GWL_STYLE, (LONG)mData.mDwStyle);
            SetWindowPos(mData.mHwnd, nullptr,
                        mConfig.x, mConfig.y,
                        (int)mConfig.width, (int)mConfig.height,
                        SWP_FRAMECHANGED | SWP_NOZORDER);
        }
    }

    // =============================================================================
    // Mouse
    // =============================================================================

    void NkWindow::SetMousePosition(uint32 x, uint32 y) { SetCursorPos((int)x, (int)y); }
    void NkWindow::ShowMouse(bool show)   { ShowCursor(show ? TRUE : FALSE); }
    void NkWindow::CaptureMouse(bool cap) {
        if (cap) SetCapture(mData.mHwnd);
        else     ReleaseCapture();
    }

    // =============================================================================
    // OS extras
    // =============================================================================

    void NkWindow::SetProgress(float progress) {
        if (mData.mTaskbarList) {
            const uint32 kMax = 10000;
            mData.mTaskbarList->SetProgressValue(mData.mHwnd,
                (ULONGLONG)(progress * kMax), kMax);
        }
    }

    // =============================================================================
    // Surface
    // =============================================================================

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc sd;
        auto sz      = GetSize();  // GetSize synchronise déjà mConfig
        sd.width     = sz.x;
        sd.height    = sz.y;
        sd.dpi       = GetDpiScale();
        sd.hwnd      = mData.mHwnd;
        sd.hinstance = mData.mHInstance;
        sd.appliedHints = mData.mAppliedHints;
        return sd;
    }

    // =============================================================================
    // Safe Area (desktop = zero insets)
    // =============================================================================

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const { return {}; }

    // =============================================================================
    // Orientation (N/A on Win32)
    // =============================================================================

    bool                NkWindow::SupportsOrientationControl() const { return false; }
    void                NkWindow::SetScreenOrientation(NkScreenOrientation) {}
    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }
    void NkWindow::SetAutoRotateEnabled(bool) {}
    bool NkWindow::IsAutoRotateEnabled() const { return false; }

    // =============================================================================
    // Web input (N/A on Win32)
    // =============================================================================

    void              NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}
    NkWebInputOptions NkWindow::GetWebInputOptions() const { return {}; }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX