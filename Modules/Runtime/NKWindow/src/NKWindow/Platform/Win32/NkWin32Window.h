#pragma once
// =============================================================================
// NkWin32Window.h — Win32 platform data for NkWindow (data only, no methods)
//
// Point 2 : les globals HWND→NkWindow* sont désormais static dans le .cpp.
// Ce header n'expose que les fonctions d'accès — aucun extern visible.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <wingdi.h>
#include "NKWindow/Core/NkSurfaceHint.h"

namespace nkentseu {

    class NkWindow;
    class NkWin32DropTarget;

    // -------------------------------------------------------------------------
    // NkWindowData — données natives Win32 embarquées dans NkWindow (pas de pimpl)
    // -------------------------------------------------------------------------
    struct NkWindowData {
        HWND           mHwnd          = nullptr;
        HWND           mParentHwnd    = nullptr;
        HWND           mUtilityOwner  = nullptr;
        HINSTANCE      mHInstance     = nullptr;
        DWORD          mDwStyle       = 0;
        DWORD          mDwExStyle     = 0;
        DEVMODE        mDmScreen      = {};
        ITaskbarList3* mTaskbarList   = nullptr;
        NkWin32DropTarget* mDropTarget = nullptr;
        HICON          mIconSmall     = nullptr;
        HICON          mIconBig       = nullptr;
        WNDPROC        mPrevWndProc   = nullptr;
        LONG_PTR       mPrevUserData  = 0;
        bool           mExternal      = false;
        bool           mMouseTracking = false;
        NkSurfaceHints mAppliedHints{};
    };

    // -------------------------------------------------------------------------
    // Point 2 : registre backend Win32 — accès via fonctions, pas via extern.
    // Les variables sWin32WindowMap et sWin32LastWindow sont static dans le .cpp
    // et totalement invisibles depuis l'extérieur.
    // -------------------------------------------------------------------------

    /// Recherche une NkWindow* par son HWND — O(1) via unordered_map
    NkWindow* NkWin32FindWindow(HWND hwnd);

    /// Enregistre une association HWND → NkWindow* dans le registre backend
    void NkWin32RegisterWindow(HWND hwnd, NkWindow* win);

    /// Supprime l'association HWND → NkWindow* du registre backend
    void NkWin32UnregisterWindow(HWND hwnd);

    /// Retourne la dernière fenêtre enregistrée (utile pour les messages orphelins)
    NkWindow* NkWin32GetLastWindow();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
