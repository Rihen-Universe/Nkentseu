// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
#define WIN32_LEAN_AND_MEAN
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
			HWND mHwnd = nullptr;
			HWND mParentHwnd = nullptr;
			HWND mUtilityOwner = nullptr;
			// `modal` : la fenetre parent que NOUS avons desactivee, et que nous
			// DEVONS reactiver. Nulle si la fenetre n'est pas modale — la remettre a
			// nullptr apres reactivation evite qu'une double fermeture reactive deux
			// fois une fenetre que quelqu'un d'autre aurait desactivee entretemps.
			HWND mModalOwner = nullptr;
			HINSTANCE mHInstance = nullptr;
			DWORD mDwStyle = 0;
			DWORD mDwExStyle = 0;
			// [FIX 2026-07-25] DEVMODEW EXPLICITE, jamais le macro DEVMODE :
			// DEVMODE = DEVMODEA (156 o) ou DEVMODEW (220 o) selon que la TU
			// définit UNICODE. Ce struct est embarqué par valeur dans NkWindow
			// (donc dans NkApplication) : un projet compilé avec UNICODE (Nogee,
			// NKWindow) et un sans (Engine/Noge) voyaient deux layouts décalés
			// de 64 octets → ODR violation, GetDevice() lisait mDevice au
			// mauvais offset → crash aléatoire au démarrage de Nogee.
			// Le layout doit être indépendant des macros de la TU incluante.
			DEVMODEW mDmScreen = {};
			ITaskbarList3 *mTaskbarList = nullptr;
			NkWin32DropTarget *mDropTarget = nullptr;
			HICON mIconSmall = nullptr;
			HICON mIconBig = nullptr;
			WNDPROC mPrevWndProc = nullptr;
			LONG_PTR mPrevUserData = 0;
			bool mExternal = false;
			bool mBorderless = false; // frame=false : zone non-cliente supprimee (WM_NCCALCSIZE)
			bool mMouseTracking = false;
			HCURSOR mClientCursor = nullptr; // curseur courant de la zone client (WM_SETCURSOR)
			NkSurfaceHints mAppliedHints{};
	};

	// -------------------------------------------------------------------------
	// Point 2 : registre backend Win32 — accès via fonctions, pas via extern.
	// Les variables sWin32WindowMap et sWin32LastWindow sont static dans le .cpp
	// et totalement invisibles depuis l'extérieur.
	// -------------------------------------------------------------------------

	/// Recherche une NkWindow* par son HWND — O(1) via unordered_map
	NkWindow *NkWin32FindWindow(HWND hwnd);

	/// Enregistre une association HWND → NkWindow* dans le registre backend
	void NkWin32RegisterWindow(HWND hwnd, NkWindow *win);

	/// Supprime l'association HWND → NkWindow* du registre backend
	void NkWin32UnregisterWindow(HWND hwnd);

	// -------------------------------------------------------------------------
	// Le fond PAR FENETRE — pose par NkWindow::SetBackgroundColor (26/09).
	//
	// La brosse est rangee dans une propriete du HWND plutot que dans
	// NkWindowData, dont l'agencement doit rester stable entre unites de
	// compilation. Le gestionnaire WM_ERASEBKGND la prefere a celle de la
	// CLASSE quand elle existe ; sinon il retombe sur la classe.
	// -------------------------------------------------------------------------

	/// Nom de la propriete Win32 qui porte la brosse de fond d'UNE fenetre.
	extern const wchar_t *kNkWin32PropFond;

	/// Detruit la brosse de fond propre a cette fenetre, s'il y en a une.
	/// A appeler avant la destruction de la fenetre. Sans effet sinon.
	/// ⚠️ Ne touche JAMAIS a la brosse de la classe : celle-la appartient a la
	///    classe et c'est UnregisterClass qui la detruit.
	void NkWin32LibererFondFenetre(HWND hwnd);

	/// Retourne la dernière fenêtre enregistrée (utile pour les messages orphelins)
	NkWindow *NkWin32GetLastWindow();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
