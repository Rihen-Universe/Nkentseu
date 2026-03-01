// =============================================================================
// NkWin32WindowImpl.cpp
// Création fenêtre Win32 Unicode (RegisterClassExW / CreateWindowExW).
// Toutes les chaînes std::string sont converties en WCHAR via NkUtf8ToWide().
//
// CORRECTION v2 :
//   - SetCurrentImpl() est appelé AVANT CreateWindowExW afin que sCurrentImpl
//     soit valide dès la réception de WM_NCCREATE / WM_CREATE.
// =============================================================================

#include "NkWin32WindowImpl.h"
#include "NkWin32EventImpl.h"
#include "NKWindow/Core/NkSystem.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Dynamic loading of DPI-aware APIs (Windows 10 1607+)
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
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

    void InitializeDpiAPIs()
    {
        if (sDpiAPIsInitialized) return;
        sDpiAPIsInitialized = true;
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32)
        {
            pSetThreadDpiAwarenessContext = (SetThreadDpiAwarenessContextProc)
                GetProcAddress(user32, "SetThreadDpiAwarenessContext");
            pGetDpiForWindow = (GetDpiForWindowProc)
                GetProcAddress(user32, "GetDpiForWindow");
        }
    }

    DPI_AWARENESS_CONTEXT NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT ctx)
    {
        return pSetThreadDpiAwarenessContext ? pSetThreadDpiAwarenessContext(ctx) : nullptr;
    }

    UINT NkGetDpiForWindow(HWND hwnd)
    {
        return pGetDpiForWindow ? pGetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
    }

} // anonymous namespace

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // Utilitaires de conversion UTF-8 ↔ UTF-16
    // ---------------------------------------------------------------------------

    static std::wstring NkUtf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring ws((size_t)len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), len);
        return ws;
    }

    static std::string NkWideToUtf8(const std::wstring& ws)
    {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                                      nullptr, 0, nullptr, nullptr);
        std::string s((size_t)len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                            s.data(), len, nullptr, nullptr);
        return s;
    }

    // ---------------------------------------------------------------------------

    NkWin32WindowImpl::~NkWin32WindowImpl()
    {
        if (IsOpen()) Close();
    }

    // ---------------------------------------------------------------------------
    // Create
    // ---------------------------------------------------------------------------

    bool NkWin32WindowImpl::Create(const NkWindowConfig& config)
    {
        mConfig     = config;
        mData.hinstance = GetModuleHandleW(nullptr);

        std::wstring wClassName = NkUtf8ToWide(config.name);
        std::wstring wTitle     = NkUtf8ToWide(config.title);

        // --- Styles ---
        if (config.fullscreen)
        {
            DEVMODE dm = {};
            dm.dmSize        = sizeof(DEVMODE);
            dm.dmPelsWidth   = GetSystemMetrics(SM_CXSCREEN);
            dm.dmPelsHeight  = GetSystemMetrics(SM_CYSCREEN);
            dm.dmBitsPerPel  = 32;
            dm.dmFields      = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            mData.dmScreen   = dm;
            ChangeDisplaySettingsW(&mData.dmScreen, CDS_FULLSCREEN);
            mData.dwExStyle  = WS_EX_APPWINDOW;
            mData.dwStyle    = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        }
        else
        {
            mData.dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
            mData.dwStyle   = config.frame
                ? WS_OVERLAPPEDWINDOW
                : (WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        }

        RECT rc = {
            config.x, config.y,
            config.x + (LONG)config.width,
            config.y + (LONG)config.height
        };
        AdjustWindowRectEx(&rc, mData.dwStyle, FALSE, mData.dwExStyle);

        // --- Enregistrement de la classe de fenêtre ---
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc   = NkWin32EventImpl::WindowProcStatic;
        wc.hInstance     = mData.hinstance;
        wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = wClassName.c_str();
        wc.hIconSm       = LoadIconW(nullptr, IDI_WINLOGO);
        RegisterClassExW(&wc); // Peut échouer si déjà enregistrée — c'est OK

        // --- DPI awareness ---
        InitializeDpiAPIs();
        DPI_AWARENESS_CONTEXT prev =
            NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // --- CORRECTION CRITIQUE ---
        // Activer sCurrentImpl AVANT CreateWindowExW pour que WM_NCCREATE,
        // WM_CREATE et tout autre message envoyé pendant la création soient
        // correctement traités par ProcessWin32Message.
        if (auto* evImpl = static_cast<NkWin32EventImpl*>(NkGetEventImpl()))
            evImpl->SetCurrentImpl();

        // Renseigner gCurrentWindow pour que WM_NCCREATE puisse peupler gWindowMap
        gCurrentWindow = this;

        // --- Création de la fenêtre ---
        mData.hwnd = CreateWindowExW(
            0,
            wClassName.c_str(),
            wTitle.c_str(),
            mData.dwStyle,
            0, 0,
            rc.right  - rc.left,
            rc.bottom - rc.top,
            nullptr,
            nullptr,
            mData.hinstance,
            this     // ← récupéré dans WM_NCCREATE via CREATESTRUCT::lpCreateParams
        );

        NkSetThreadDpiAwarenessContext(prev);

        if (!mData.hwnd)
        {
            mLastError = NkError(1, "CreateWindowExW failed (" +
                                 std::to_string(::GetLastError()) + ")");
            return false;
        }

        // --- Centrage ---
        if (!config.fullscreen)
        {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int ww = rc.right  - rc.left;
            int wh = rc.bottom - rc.top;
            if (config.centered)
                SetWindowPos(mData.hwnd, nullptr,
                             (sw - ww) / 2, (sh - wh) / 2, ww, wh,
                             SWP_NOZORDER);
            else
                SetWindowPos(mData.hwnd, nullptr,
                             config.x, config.y, ww, wh,
                             SWP_NOZORDER);
        }

        // --- DWM ombre ---
        BOOL ncr = TRUE;
        DwmSetWindowAttribute(mData.hwnd, DWMWA_NCRENDERING_ENABLED, &ncr, sizeof(ncr));
        const MARGINS shadow = { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(mData.hwnd, &shadow);

        // --- Taskbar ---
        CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                         kIIDTaskbarList3, (void**)&mData.taskbarList);

        // --- Affichage ---
        if (config.visible)
        {
            ShowWindow(mData.hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(mData.hwnd);
            SetFocus(mData.hwnd);
        }

        mData.isOpen = true;

        // --- Enregistrement final dans l'EventImpl (RawInput, etc.) ---
        // Initialize() confirme sCurrentImpl et enregistre RawInput.
        if (auto* evImpl = static_cast<NkWin32EventImpl*>(NkGetEventImpl()))
            evImpl->Initialize(this, mData.hwnd);

        return true;
    }

    // ---------------------------------------------------------------------------
    // Close
    // ---------------------------------------------------------------------------

    void NkWin32WindowImpl::Close()
    {
        if (!mData.isOpen) return;

        // Retirer du registre global
        gWindowRegistry().Remove(mData.hwnd);

        // Détacher du backend événements
        if (auto* evImpl = static_cast<NkWin32EventImpl*>(NkGetEventImpl()))
            evImpl->Shutdown(mData.hwnd);

        if (mData.hwnd)
        {
            DestroyWindow(mData.hwnd);
            UnregisterClassW(NkUtf8ToWide(mConfig.name).c_str(), mData.hinstance);
            mData.hwnd = nullptr;
        }

        if (mData.taskbarList)
        {
            mData.taskbarList->Release();
            mData.taskbarList = nullptr;
        }

        mData.isOpen = false;
    }

    // ---------------------------------------------------------------------------
    // Getters / Setters
    // ---------------------------------------------------------------------------

    bool     NkWin32WindowImpl::IsOpen()       const { return mData.isOpen; }
    NkError  NkWin32WindowImpl::GetLastError() const { return mLastError; }

    std::string NkWin32WindowImpl::GetTitle() const
    {
        if (!mData.hwnd) return {};
        int len = GetWindowTextLengthW(mData.hwnd);
        if (len <= 0) return {};
        std::wstring ws((size_t)len + 1, L'\0');
        GetWindowTextW(mData.hwnd, ws.data(), len + 1);
        ws.resize((size_t)len);
        return NkWideToUtf8(ws);
    }

    void NkWin32WindowImpl::SetTitle(const std::string& t)
    {
        mConfig.title = t;
        if (mData.hwnd)
            SetWindowTextW(mData.hwnd, NkUtf8ToWide(t).c_str());
    }

    NkVec2u NkWin32WindowImpl::GetSize() const
    {
        RECT rc = {};
        if (mData.hwnd) GetClientRect(mData.hwnd, &rc);
        return { (NkU32)(rc.right - rc.left), (NkU32)(rc.bottom - rc.top) };
    }

    NkVec2u NkWin32WindowImpl::GetPosition() const
    {
        RECT rc = {};
        if (mData.hwnd) GetWindowRect(mData.hwnd, &rc);
        return { (NkU32)rc.left, (NkU32)rc.top };
    }

    float NkWin32WindowImpl::GetDpiScale() const
    {
        return mData.hwnd
            ? (float)NkGetDpiForWindow(mData.hwnd) / USER_DEFAULT_SCREEN_DPI
            : 1.f;
    }

    NkVec2u NkWin32WindowImpl::GetDisplaySize() const
    {
        return { (NkU32)GetSystemMetrics(SM_CXSCREEN),
                 (NkU32)GetSystemMetrics(SM_CYSCREEN) };
    }

    NkVec2u NkWin32WindowImpl::GetDisplayPosition() const
    {
        return { 0, 0 };
    }

    void NkWin32WindowImpl::SetSize(NkU32 w, NkU32 h)
    {
        RECT rc = { 0, 0, (LONG)w, (LONG)h };
        AdjustWindowRectEx(&rc, mData.dwStyle, FALSE, mData.dwExStyle);
        SetWindowPos(mData.hwnd, nullptr, 0, 0,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    void NkWin32WindowImpl::SetPosition(NkI32 x, NkI32 y)
    {
        SetWindowPos(mData.hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    void NkWin32WindowImpl::SetVisible(bool v)
    {
        ShowWindow(mData.hwnd, v ? SW_SHOW : SW_HIDE);
    }

    void NkWin32WindowImpl::Minimize() { ShowWindow(mData.hwnd, SW_MINIMIZE); }

    void NkWin32WindowImpl::Maximize()
    {
        ShowWindow(mData.hwnd, IsZoomed(mData.hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    }

    void NkWin32WindowImpl::Restore() { ShowWindow(mData.hwnd, SW_RESTORE); }

    void NkWin32WindowImpl::SetFullscreen(bool fs)
    {
        if (fs)
        {
            SetWindowLongW(mData.hwnd, GWL_STYLE,
                           WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            SetWindowPos(mData.hwnd, HWND_TOP, 0, 0,
                         GetSystemMetrics(SM_CXSCREEN),
                         GetSystemMetrics(SM_CYSCREEN),
                         SWP_FRAMECHANGED);
        }
        else
        {
            SetWindowLongW(mData.hwnd, GWL_STYLE, (LONG)mData.dwStyle);
            SetWindowPos(mData.hwnd, nullptr,
                         mConfig.x, mConfig.y,
                         (int)mConfig.width, (int)mConfig.height,
                         SWP_FRAMECHANGED | SWP_NOZORDER);
        }
        mConfig.fullscreen = fs;
    }

    void NkWin32WindowImpl::SetMousePosition(NkU32 x, NkU32 y)
    {
        SetCursorPos((int)x, (int)y);
    }

    void NkWin32WindowImpl::ShowMouse(bool show)
    {
        ShowCursor(show ? TRUE : FALSE);
    }

    void NkWin32WindowImpl::CaptureMouse(bool cap)
    {
        if (cap) SetCapture(mData.hwnd);
        else     ReleaseCapture();
    }

    void NkWin32WindowImpl::SetProgress(float progress)
    {
        if (mData.taskbarList)
        {
            const NkU32 kMax = 10000;
            mData.taskbarList->SetProgressValue(
                mData.hwnd,
                (ULONGLONG)(progress * kMax),
                kMax);
        }
    }

    NkSurfaceDesc NkWin32WindowImpl::GetSurfaceDesc() const
    {
        NkSurfaceDesc sd;
        auto sz       = GetSize();
        sd.width      = sz.x;
        sd.height     = sz.y;
        sd.dpi        = GetDpiScale();
        sd.hwnd       = mData.hwnd;
        sd.hinstance  = mData.hinstance;
        return sd;
    }

} // namespace nkentseu