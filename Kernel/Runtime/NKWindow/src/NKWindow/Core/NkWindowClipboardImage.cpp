// =============================================================================
// NkWindowClipboardImage.cpp — Presse-papiers IMAGE : FALLBACK multiplateforme.
//   Même philosophie que NkWindowClipboard.cpp (texte) : Win32 desktop fournit
//   la vraie implementation OS (CF_DIBV5/CF_DIB) dans
//   Platform/Win32/NkWin32Window.cpp ; ce fichier fournit un presse-papiers
//   IMAGE interne a l'application pour toutes les autres plateformes ->
//   copier/coller d'images intra-app fonctionne partout, sans exception de
//   link. TODO (ROADMAP « Fenetre discrete / presse-papiers ») : impl OS
//   reelles — X11 CLIPBOARD cible image/png (exige la boucle de selection),
//   NSPasteboard (NSPasteboardTypePNG/TIFF), wl_data_device.
//
//   Guard : on exclut UNIQUEMENT le desktop Win32 (meme expression que
//   NkWindowCursor.cpp) — UWP/Xbox recoivent donc ce fallback au lieu d'un
//   trou de link.
// =============================================================================
#include "NKWindow/Core/NkWindow.h"

#if !(defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX))

namespace nkentseu {

	// Presse-papiers image interne (process-global) — partage par toutes les
	// fenetres, comme le fallback texte.
	static NkClipboardImage &NkInternalClipboardImage() {
		static NkClipboardImage s_clip;
		return s_clip;
	}

	bool NkWindow::SetClipboardImage(const NkClipboardImage &image) {
		if (!image.IsValid())
			return false;
		NkInternalClipboardImage() = image;
		return true;
	}

	bool NkWindow::GetClipboardImage(NkClipboardImage &out) const {
		const NkClipboardImage &clip = NkInternalClipboardImage();
		if (!clip.IsValid()) {
			out = NkClipboardImage{};
			return false;
		}
		out = clip;
		return true;
	}

	bool NkWindow::HasClipboardImage() const {
		return NkInternalClipboardImage().IsValid();
	}

} // namespace nkentseu

#endif // !(desktop Win32)
