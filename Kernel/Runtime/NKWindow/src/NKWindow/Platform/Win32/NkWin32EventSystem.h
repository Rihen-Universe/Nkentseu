#pragma once
// =============================================================================
// NkWin32EventSystem.h
// Partie Win32 du système d'événements, séparée de NkEventSystem (NKEvent).
//
// NkEventSystem (NKEvent) est platform-agnostique.
// Ce header expose uniquement ce qui est nécessaire pour enregistrer la
// procédure de fenêtre Win32 (NkWin32WndProc) dans NkWin32Window.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace nkentseu {

    // WndProc Win32 — à enregistrer dans WNDCLASSEXW.lpfnWndProc
    LRESULT CALLBACK NkWin32WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

} // namespace nkentseu
