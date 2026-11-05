// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWindowFichiers.cpp — Fichiers du presse-papiers et glisser vers l'exterieur : REPLI des plateformes sans dorsal dedie.
//   Win32 desktop : Platform/Win32/NkWin32Fichiers.cpp (CF_HDROP, DoDragDrop). Ici, une liste INTERNE a l'application (copier /
//   couper / coller de fichiers dans l'application) ; le glisser vers l'exterieur n'existe pas (rend faux, l'appelant le dit).
// =============================================================================
#include "NKWindow/Core/NkWindow.h"

#if !(defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX))

namespace nkentseu {

	namespace {
		struct NkListeFichiers {
				NkVector<NkString> chemins;
				bool couper = false;
		};
		NkListeFichiers &Liste() {
			static NkListeFichiers l;
			return l;
		}
	} // namespace

	bool NkWindow::SetClipboardFiles(const NkVector<NkString> &chemins, bool couper) {
		Liste().chemins = chemins;
		Liste().couper = couper;
		SetClipboardText(NkString("")); // un seul presse-papiers : poser des fichiers remplace le texte
		return !chemins.Empty();
	}

	bool NkWindow::GetClipboardFiles(NkVector<NkString> &chemins, bool &couper) const {
		chemins = Liste().chemins;
		couper = Liste().couper;
		return !chemins.Empty();
	}

	bool NkWindow::BeginFileDrag(const NkVector<NkString> &, bool, int32 &effet) {
		effet = 0;
		return false;
	}

	void NkWindow::SetMaxButtonRect(int32, int32, int32, int32) {
	}

	int32 NkWindow::NativeHitTest(int32, int32) const {
		return 1;
	}

	void NkWindow::NativeSouris(NkSourisBanc, int32, int32) {
	}

} // namespace nkentseu

#endif
