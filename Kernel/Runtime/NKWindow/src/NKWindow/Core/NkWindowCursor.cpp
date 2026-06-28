// =============================================================================
// NkWindowCursor.cpp — implémentation de NkWindow::SetCursor.
// Mappe NkCursorType sur les curseurs natifs. Win32 : IDC_* + ::SetCursor, le
// curseur étant ré-appliqué par WM_SETCURSOR (cf. NkWin32EventSystem.cpp) pour
// résister à la réinitialisation système à chaque mouvement souris.
// No-op sur les plateformes sans curseur (mobile / web tactile).
// =============================================================================
#include "NKWindow/Core/NkWindow.h"

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

    void NkWindow::SetCursor(NkCursorType cursor) {
        LPCWSTR idc = IDC_ARROW;
        switch (cursor) {
            case NkCursorType::TextInput:  idc = IDC_IBEAM;    break;
            case NkCursorType::Hand:       idc = IDC_HAND;     break;
            case NkCursorType::ResizeNS:   idc = IDC_SIZENS;   break;
            case NkCursorType::ResizeWE:   idc = IDC_SIZEWE;   break;
            case NkCursorType::ResizeNWSE: idc = IDC_SIZENWSE; break;
            case NkCursorType::ResizeNESW: idc = IDC_SIZENESW; break;
            case NkCursorType::Arrow:
            default:                       idc = IDC_ARROW;    break;
        }
        HCURSOR hc = ::LoadCursorW(nullptr, idc);
        mData.mClientCursor = hc;          // mémorisé pour WM_SETCURSOR
        ::SetCursor(hc);                    // applique immédiatement si dans le client
    }

#else

    void NkWindow::SetCursor(NkCursorType /*cursor*/) {
        // Plateformes sans curseur souris (Android, iOS, Web tactile, headless).
    }

#endif

} // namespace nkentseu
