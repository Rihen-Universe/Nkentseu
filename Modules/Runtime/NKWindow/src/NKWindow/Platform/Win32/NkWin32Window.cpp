// =============================================================================
// NkWin32Window.cpp
// Implémentation Win32 de NkWindow sans PIMPL.
//
// Points appliqués :
//   Point 2 : sWin32WindowMap et sWin32LastWindow sont static dans ce .cpp
//   Point 4 : WM_DESTROY conditionne PostQuitMessage à la dernière fenêtre
//   Point 6 : OleInitialize/OleUninitialize retirés — gérés par NkSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NkWin32Window.h"
#include "NKWindow/Platform/Win32/NkWin32DropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"

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

    // =============================================================================
    // Point 2 : registre backend — static dans ce .cpp, invisible à l'extérieur
    // =============================================================================

    static std::unordered_map<HWND, NkWindow*> sWin32WindowMap;
    static NkWindow*                           sWin32LastWindow = nullptr;

    NkWindow* NkWin32FindWindow(HWND hwnd) {
        auto it = sWin32WindowMap.find(hwnd);
        return it != sWin32WindowMap.end() ? it->second : nullptr;
    }

    void NkWin32RegisterWindow(HWND hwnd, NkWindow* win) {
        sWin32WindowMap[hwnd] = win;
        sWin32LastWindow      = win;
    }

    void NkWin32UnregisterWindow(HWND hwnd) {
        auto it = sWin32WindowMap.find(hwnd);
        if (it == sWin32WindowMap.end()) return;

        NkWindow* win = it->second;
        sWin32WindowMap.erase(it);

        if (sWin32LastWindow == win) {
            sWin32LastWindow = sWin32WindowMap.empty()
                ? nullptr
                : sWin32WindowMap.begin()->second;
        }
    }

    NkWindow* NkWin32GetLastWindow() {
        return sWin32LastWindow;
    }

    // =============================================================================
    // Helpers UTF-8 ↔ Wide
    // =============================================================================

    static std::wstring NkUtf8ToWide(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring ws((size_t)len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), len);
        return ws;
    }

    static std::string NkWideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                                    nullptr, 0, nullptr, nullptr);
        std::string s((size_t)len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                            s.data(), len, nullptr, nullptr);
        return s;
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
        mConfig          = config;
        mData.mHInstance = GetModuleHandleW(nullptr);

        std::wstring wClassName = NkUtf8ToWide(config.name);
        std::wstring wTitle     = NkUtf8ToWide(config.title);

        // --- Styles ---
        if (config.fullscreen) {
            DEVMODE dm        = {};
            dm.dmSize         = sizeof(DEVMODE);
            dm.dmPelsWidth    = GetSystemMetrics(SM_CXSCREEN);
            dm.dmPelsHeight   = GetSystemMetrics(SM_CYSCREEN);
            dm.dmBitsPerPel   = 32;
            dm.dmFields       = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            mData.mDmScreen   = dm;
            ChangeDisplaySettingsW(&mData.mDmScreen, CDS_FULLSCREEN);
            mData.mDwExStyle  = WS_EX_APPWINDOW;
            mData.mDwStyle    = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        } else {
            mData.mDwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
            mData.mDwStyle   = config.frame
                ? WS_OVERLAPPEDWINDOW
                : (WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU |
                WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        }

        RECT rc = {
            config.x, config.y,
            config.x + (LONG)config.width,
            config.y + (LONG)config.height
        };
        AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);

        // --- Window class registration ---
        WNDCLASSEXW wc       = {};
        wc.cbSize            = sizeof(WNDCLASSEXW);
        wc.style             = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc       = NkSystem::Events().WindowProcStatic;
        wc.hInstance         = mData.mHInstance;
        wc.hIcon             = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor           = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground     = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName     = wClassName.c_str();
        wc.hIconSm           = LoadIconW(nullptr, IDI_WINLOGO);
        RegisterClassExW(&wc); // OK si déjà enregistrée

        // --- DPI awareness ---
        InitializeDpiAPIs();
        DPI_AWARENESS_CONTEXT prev =
            NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Enregistrement dans NkSystem (registre cross-plateforme)
        mId = NkSystem::Instance().RegisterWindow(this);

        // --- Création de la fenêtre ---
        // lpCreateParams = this (NkWindow*), récupéré dans WM_NCCREATE via WindowProcStatic
        mData.mHwnd = CreateWindowExW(
            0,
            wClassName.c_str(),
            wTitle.c_str(),
            mData.mDwStyle,
            0, 0,
            rc.right  - rc.left,
            rc.bottom - rc.top,
            nullptr,
            nullptr,
            mData.mHInstance,
            this    // passé comme lpCreateParams
        );

        NkSetThreadDpiAwarenessContext(prev);

        if (!mData.mHwnd) {
            mLastError = NkError(1, "CreateWindowExW failed (" +
                                std::to_string(::GetLastError()) + ")");
            NkSystem::Instance().UnregisterWindow(mId);
            mId = NK_INVALID_WINDOW_ID;
            return false;
        }

        // --- Centrage ---
        if (!config.fullscreen) {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int ww = rc.right  - rc.left;
            int wh = rc.bottom - rc.top;
            if (config.centered)
                SetWindowPos(mData.mHwnd, nullptr, (sw-ww)/2, (sh-wh)/2, ww, wh, SWP_NOZORDER);
            else
                SetWindowPos(mData.mHwnd, nullptr, config.x, config.y, ww, wh, SWP_NOZORDER);
        }

        // --- DWM shadow ---
        BOOL ncr = TRUE;
        DwmSetWindowAttribute(mData.mHwnd, DWMWA_NCRENDERING_ENABLED, &ncr, sizeof(ncr));
        const MARGINS shadow = { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(mData.mHwnd, &shadow);

        // --- Taskbar ---
        // Point 6 : CoCreateInstance fonctionne car OleInitialize a déjà été
        // appelé par NkSystem::Initialise() avant toute création de fenêtre.
        CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                        kIIDTaskbarList3, (void**)&mData.mTaskbarList);

        // OLE DropTarget integration (events forwarded to NkEventSystem queue).
        mData.mDropTarget = new NkWin32DropTarget(mData.mHwnd);
        if (mData.mDropTarget) {
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
        }

        // --- Affichage ---
        if (config.visible) {
            ShowWindow(mData.mHwnd, SW_SHOWNORMAL);
            SetForegroundWindow(mData.mHwnd);
            SetFocus(mData.mHwnd);
        }

        mIsOpen = true;
        return true;
    }

    // =============================================================================
    // Close
    // =============================================================================

    void NkWindow::Close() {
        if (!mIsOpen) return;

        // Point 2 : appel via la fonction d'accès, pas un extern
        NkWin32UnregisterWindow(mData.mHwnd);
        NkSystem::Instance().UnregisterWindow(mId);

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mHwnd) {
            DestroyWindow(mData.mHwnd);
            UnregisterClassW(NkUtf8ToWide(mConfig.name).c_str(), mData.mHInstance);
            mData.mHwnd = nullptr;
        }

        if (mData.mTaskbarList) {
            mData.mTaskbarList->Release();
            mData.mTaskbarList = nullptr;
        }

        mId     = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
    }

    // =============================================================================
    // State
    // =============================================================================

    bool NkWindow::IsOpen()  const { return mIsOpen; }
    bool NkWindow::IsValid() const { return mIsOpen && mData.mHwnd != nullptr; }

    NkError        NkWindow::GetLastError() const { return mLastError; }
    NkWindowConfig NkWindow::GetConfig()    const { return mConfig; }

    // =============================================================================
    // Title
    // =============================================================================

    std::string NkWindow::GetTitle() const {
        if (!mData.mHwnd) return {};
        int len = GetWindowTextLengthW(mData.mHwnd);
        if (len <= 0) return {};
        std::wstring ws((size_t)len + 1, L'\0');
        GetWindowTextW(mData.mHwnd, ws.data(), len + 1);
        ws.resize((size_t)len);
        return NkWideToUtf8(ws);
    }

    void NkWindow::SetTitle(const std::string& t) {
        mConfig.title = t;
        if (mData.mHwnd) SetWindowTextW(mData.mHwnd, NkUtf8ToWide(t).c_str());
    }

    // =============================================================================
    // Size / Position
    // =============================================================================

    NkVec2u NkWindow::GetSize() const {
        RECT rc = {};
        if (mData.mHwnd) GetClientRect(mData.mHwnd, &rc);
        return { (NkU32)(rc.right - rc.left), (NkU32)(rc.bottom - rc.top) };
    }

    NkVec2u NkWindow::GetPosition() const {
        RECT rc = {};
        if (mData.mHwnd) GetWindowRect(mData.mHwnd, &rc);
        return { (NkU32)rc.left, (NkU32)rc.top };
    }

    float NkWindow::GetDpiScale() const {
        return mData.mHwnd
            ? (float)NkGetDpiForWindow(mData.mHwnd) / USER_DEFAULT_SCREEN_DPI
            : 1.f;
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        return { (NkU32)GetSystemMetrics(SM_CXSCREEN),
                (NkU32)GetSystemMetrics(SM_CYSCREEN) };
    }

    NkVec2u NkWindow::GetDisplayPosition() const { return { 0, 0 }; }

    void NkWindow::SetSize(NkU32 w, NkU32 h) {
        RECT rc = { 0, 0, (LONG)w, (LONG)h };
        AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);
        SetWindowPos(mData.mHwnd, nullptr, 0, 0,
                    rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOMOVE | SWP_NOZORDER);
    }

    void NkWindow::SetPosition(NkI32 x, NkI32 y) {
        SetWindowPos(mData.mHwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    void NkWindow::SetVisible(bool v)  { ShowWindow(mData.mHwnd, v ? SW_SHOW : SW_HIDE); }
    void NkWindow::Minimize()          { ShowWindow(mData.mHwnd, SW_MINIMIZE); }
    void NkWindow::Maximize()          { ShowWindow(mData.mHwnd, IsZoomed(mData.mHwnd) ? SW_RESTORE : SW_MAXIMIZE); }
    void NkWindow::Restore()           { ShowWindow(mData.mHwnd, SW_RESTORE); }

    void NkWindow::SetFullscreen(bool fs) {
        if (fs) {
            SetWindowLongW(mData.mHwnd, GWL_STYLE,
                        WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            SetWindowPos(mData.mHwnd, HWND_TOP, 0, 0,
                        GetSystemMetrics(SM_CXSCREEN),
                        GetSystemMetrics(SM_CYSCREEN),
                        SWP_FRAMECHANGED);
        } else {
            SetWindowLongW(mData.mHwnd, GWL_STYLE, (LONG)mData.mDwStyle);
            SetWindowPos(mData.mHwnd, nullptr,
                        mConfig.x, mConfig.y,
                        (int)mConfig.width, (int)mConfig.height,
                        SWP_FRAMECHANGED | SWP_NOZORDER);
        }
        mConfig.fullscreen = fs;
    }

    // =============================================================================
    // Mouse
    // =============================================================================

    void NkWindow::SetMousePosition(NkU32 x, NkU32 y) { SetCursorPos((int)x, (int)y); }
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
            const NkU32 kMax = 10000;
            mData.mTaskbarList->SetProgressValue(mData.mHwnd,
                (ULONGLONG)(progress * kMax), kMax);
        }
    }

    // =============================================================================
    // Surface
    // =============================================================================

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc sd;
        auto sz      = GetSize();
        sd.width     = sz.x;
        sd.height    = sz.y;
        sd.dpi       = GetDpiScale();
        sd.hwnd      = mData.mHwnd;
        sd.hinstance = mData.mHInstance;
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
