#pragma once
// =============================================================================
// NkXLibWindow.h - XLib platform data for NkWindow (data only, no methods)
//
// Pattern aligned with Win32/XCB:
// - Native data lives in NkWindow::mData.
// - Backend window registry is hidden in .cpp and exposed via helper functions.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkSurfaceHint.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include <X11/Xlib.h>
#include "NKContainers/String/NkString.h"			 // (21/10) le texte du presse-papiers
#include "NKWindow/Platform/XLib/NkXLibPressePapiers.h" // (21/10) CLIPBOARD
#include "NKWindow/Platform/XLib/NkXLibSaisie.h"		 // (21/10) accents, touches mortes (XIM)

namespace nkentseu {

	class NkWindow;
	class NkXLibDropTarget;

	struct NkWindowData {
			::Display *mDisplay = nullptr;
			::Window mXid = 0;
			::Atom mWmDeleteWindow = 0;
			::Window mParentXid = 0;
			int mScreen = 0;
			bool mExternal = false;
			NkXLibDropTarget *mDropTarget = nullptr;
			NkSurfaceHints mAppliedHints; // ← ajout
			XIC mXic = nullptr;			  ///< (21/10) contexte de saisie (NkXLibSaisie.h) ; nullptr = repli XLookupString
	};

	// Backend registry accessors (map lives as static in NkXLibWindow.cpp)
	NkWindow *NkXLibFindWindow(::Window xid);
	void NkXLibRegisterWindow(::Window xid, NkWindow *win);
	void NkXLibUnregisterWindow(::Window xid);
	NkWindow *NkXLibGetLastWindow();
	::Display *NkXLibGetDisplay();

	// (21/10) Le presse-papiers systeme (CLIPBOARD, NkXLibPressePapiers.h) : le texte que NKWindow
	// POSSEDE (sert les SelectionRequest de la boucle d'evenements) et les atomes du protocole.
	NkString &NkXLibTextePossede();
	const xlibpp::NkPpAtomes &NkXLibAtomesPp(::Display *d);
	// (21/10) La methode de saisie d'Xlib (une par connexion ; nullptr si aucune).
	XIM NkXLibIm(::Display *d);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
