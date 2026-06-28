// =============================================================================
// NkWindowClipboard.cpp — Presse-papiers : FALLBACK multiplateforme.
//   Win32 fournit la vraie implementation OS (CF_UNICODETEXT) dans
//   Platform/Win32/NkWin32Window.cpp ; ce fichier (compile sur TOUTES les
//   plateformes via Core/**.cpp) fournit un presse-papiers INTERNE a
//   l'application pour les plateformes sans implementation OS dediee
//   (Linux/X11/Wayland, macOS, etc.) -> copier/coller intra-app fonctionne
//   partout. TODO : presse-papiers OS reel par plateforme (X11 CLIPBOARD,
//   NSPasteboard, wl_data_device...).
// =============================================================================
#if !defined(_WIN32)

#include "NKWindow/Core/NkWindow.h"

namespace nkentseu {

    // Presse-papiers interne (process-global) — partage par toutes les fenetres.
    static NkString& NkInternalClipboard() {
        static NkString s_clip;
        return s_clip;
    }

    void NkWindow::SetClipboardText(const NkString& text) {
        NkInternalClipboard() = text;
    }

    NkString NkWindow::GetClipboardText() const {
        return NkInternalClipboard();
    }

} // namespace nkentseu

#endif // !_WIN32
