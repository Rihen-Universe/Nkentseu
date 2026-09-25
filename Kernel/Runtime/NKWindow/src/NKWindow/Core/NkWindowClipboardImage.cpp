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

#include <cstdio> // (25/09) le refus nomme se LIT dans le journal

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

	// ── (25/09) LE REFUS EST NOMME, ET IL DIT LA BONNE CHOSE ────────────────
	// ⚠️ CE REPLI N'EST PAS UN VIDE, et le dire faux serait plus grave que de se
	//    taire : il EST un presse-papiers image, mais INTERNE A L'APPLICATION.
	//    Copier puis coller une image DANS l'application marche partout. Ce qui
	//    manque hors Win32, c'est l'INTEROPERABILITE avec le presse-papiers du
	//    SYSTEME -- coller une image copiee depuis un navigateur.
	// ⚠️ D'OU LA FORME DU MESSAGE : « rien en interne, ET je ne sais pas lire le
	//    systeme ». Un simple « aucune image » laisserait chercher pourquoi une
	//    image visiblement copiee n'arrive pas.
	// CONDITION DE RETRAIT : le jour ou X11 (cible image/png), NSPasteboard ou
	// wl_data_device existe, ce message disparait POUR CETTE PLATEFORME, pas pour
	// les autres.
	bool NkWindow::GetClipboardImage(NkClipboardImage &out) const {
		const NkClipboardImage &clip = NkInternalClipboardImage();
		if (!clip.IsValid()) {
			out = NkClipboardImage{};
			// UNE FOIS, pas a chaque tentative : un refus repete devient du bruit,
			// et le bruit fait qu'on cesse de lire.
			static bool dit = false;
			if (!dit) {
				dit = true;
				std::printf("[presse-papiers] REFUS NOMME : cette plateforme ne lit pas le "
							"presse-papiers IMAGE DU SYSTEME (seul Win32 le fait, via "
							"CF_DIB/CF_DIBV5). Le presse-papiers image INTERNE a l'application "
							"fonctionne ; une image copiee depuis une AUTRE application ne peut "
							"pas etre collee ici. A ecrire : X11 cible image/png, NSPasteboard, "
							"wl_data_device.\n");
				std::fflush(stdout);
			}
			return false;
		}
		out = clip;
		return true;
	}
		out = clip;
		return true;
	}

	bool NkWindow::HasClipboardImage() const {
		return NkInternalClipboardImage().IsValid();
	}

} // namespace nkentseu

#endif // !(desktop Win32)
