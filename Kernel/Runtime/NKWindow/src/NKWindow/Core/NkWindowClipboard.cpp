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
#include "NKWindow/Core/NkWindow.h"

// Guard elargi (2026-08-11) : `!defined(_WIN32)` excluait AUSSI UWP et Xbox,
// qui n'ont pas l'implementation desktop de NkWin32Window.cpp -> symboles
// manquants au link. Meme expression que NkWindowCursor.cpp : seul le desktop
// Win32 a sa vraie implementation OS, tout le reste recoit ce fallback.
#if !(defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX))

namespace nkentseu {

	// Presse-papiers interne (process-global) — partage par toutes les fenetres.
	static NkString &NkInternalClipboard() {
		static NkString s_clip;
		return s_clip;
	}

	void NkWindow::SetClipboardText(const NkString &text) {
		NkInternalClipboard() = text;
	}

	NkString NkWindow::GetClipboardText() const {
		return NkInternalClipboard();
	}

} // namespace nkentseu

#endif // !(desktop Win32)
