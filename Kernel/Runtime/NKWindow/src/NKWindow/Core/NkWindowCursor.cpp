// =============================================================================
// NkWindowCursor.cpp — implémentation de NkWindow::SetCursor.
// Mappe NkCursorType sur les curseurs natifs. Win32 : IDC_* + ::SetCursor, le
// curseur étant ré-appliqué par WM_SETCURSOR (cf. NkWin32EventSystem.cpp) pour
// résister à la réinitialisation système à chaque mouvement souris.
// No-op sur les plateformes sans curseur (mobile / web tactile).
// =============================================================================
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowAudit.h" // le refus NOMME, pour les dorsaux non cables

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

	void NkWindow::SetCursor(NkCursorType cursor) {
		LPCWSTR idc = IDC_ARROW;
		switch (cursor) {
			case NkCursorType::TextInput:
				idc = IDC_IBEAM;
				break;
			case NkCursorType::Hand:
				idc = IDC_HAND;
				break;
			case NkCursorType::ResizeNS:
				idc = IDC_SIZENS;
				break;
			case NkCursorType::ResizeWE:
				idc = IDC_SIZEWE;
				break;
			case NkCursorType::ResizeNWSE:
				idc = IDC_SIZENWSE;
				break;
			case NkCursorType::ResizeNESW:
				idc = IDC_SIZENESW;
				break;
			case NkCursorType::Arrow:
			default:
				idc = IDC_ARROW;
				break;
		}
		HCURSOR hc = ::LoadCursorW(nullptr, idc);
		mData.mClientCursor = hc; // mémorisé pour WM_SETCURSOR
		// N'applique IMMEDIATEMENT que si le curseur survole NOTRE fenetre :
		// appelee chaque frame, ::SetCursor depuis l'ARRIERE-PLAN ecrasait le
		// curseur de la fenetre au premier plan en continu (clignotement
		// constate par Rihen). WM_SETCURSOR fait foi dans notre client.
		POINT ptC;
		if (!::GetCursorPos(&ptC))
			return;
		HWND underC = ::WindowFromPoint(ptC);
		if (underC && ::GetAncestor(underC, GA_ROOT) == mData.mHwnd)
			::SetCursor(hc);		  // applique immédiatement si dans le client
	}

#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)

	// ── CE BRANCHEMENT EXISTE PARCE QUE LE `#else` MENTAIT ──────────────────
	// L'utilisateur du 26/09 : « les methodes pour curseur dans NKWindow ne
	// fonctionnent pas ». Le fichier disait : si ce n'est pas Windows, alors
	// « plateformes sans curseur souris (Android, iOS, Web tactile, headless) ».
	// ⚠️ MAIS UN `#else` NE DIT PAS CE QU'IL CONTIENT : IL CONTIENT TOUT LE
	//    RESTE. X11, Wayland et macOS tombaient dedans -- des bureaux qui ont un
	//    curseur et savent le changer -- et n'obtenaient ni effet, ni message.
	//
	// Ils ne sont PAS « sans objet » : ils sont NON IMPLEMENTES. La difference
	// n'est pas theorique, c'est toute la difference entre « votre plateforme
	// n'a pas de curseur » et « nous ne l'avons pas encore ecrit » -- la premiere
	// phrase fait chercher ailleurs, la seconde fait attendre un correctif.
	void NkWindow::SetCursor(NkCursorType /*cursor*/) {
		NkWindowRefuserMethode("SetCursor",
#	if defined(NKENTSEU_PLATFORM_MACOS)
							   "Cocoa",
#	else
							   "X11/Wayland",
#	endif
							   "La forme du curseur n'est implementee que sur Win32 ; "
							   "les curseurs natifs de ce dorsal ne sont pas encore cables.");
	}

#else

	void NkWindow::SetCursor(NkCursorType /*cursor*/) {
		// SANS OBJET, et c'est different d'un refus : ces plateformes n'ont pas de
		// curseur souris a changer (mobile, web tactile, console, headless). Rien
		// a dire -- un avertissement ici apprendrait a l'utilisateur a ignorer nos
		// avertissements.
	}

#endif

} // namespace nkentseu
