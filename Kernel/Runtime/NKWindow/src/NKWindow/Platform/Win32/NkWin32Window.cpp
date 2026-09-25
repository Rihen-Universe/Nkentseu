// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWin32Window.cpp
// Implémentation Win32 de NkWindow sans PIMPL.
//
// Points appliqués :
//   Point 2 : sWin32WindowMap et sWin32LastWindow sont static dans ce .cpp
//   Point 4 : WM_DESTROY conditionne PostQuitMessage à la dernière fenêtre
//   Point 6 : OleInitialize/OleUninitialize retirés — gérés par NkSystem
//   Point 7 : Synchronisation complète entre mData et mConfig
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NkWin32Window.h"
#include "NKWindow/Platform/Win32/NkWin32DropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowAudit.h" // ce que Win32 ne tient pas, il le DIT
#include "NKLogger/NkLog.h" // SetMousePositionClient DIT ses refus : jamais un repli muet
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKWindow/Platform/Win32/NkWin32EventSystem.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/String/NkWString.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h> // SetCurrentProcessExplicitAppUserModelID
#include <string>
#include <unordered_map>
#include <vector>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")

// Dynamic loading of DPI-aware APIs (Windows 10 1607+)
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT) - 4)
#endif

typedef DPI_AWARENESS_CONTEXT(WINAPI *SetThreadDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
typedef UINT(WINAPI *GetDpiForWindowProc)(HWND);
// GetDpiForMonitor : shcore.dll (Windows 8.1+). MONITOR_DPI_TYPE passé en int
// (0 = MDT_EFFECTIVE_DPI) pour éviter d'inclure <shellscalingapi.h>.
typedef HRESULT(WINAPI *GetDpiForMonitorProc)(HMONITOR, int, UINT *, UINT *);

namespace {

	SetThreadDpiAwarenessContextProc pSetThreadDpiAwarenessContext = nullptr;
	GetDpiForWindowProc pGetDpiForWindow = nullptr;
	GetDpiForMonitorProc pGetDpiForMonitor = nullptr;
	bool sDpiAPIsInitialized = false;

	const IID kIIDTaskbarList3 = {0xEA1AFB91, 0x9E28, 0x4B86, {0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEA, 0x84}};

	typedef BOOL(WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);

	void InitializeDpiAPIs() {
		if (sDpiAPIsInitialized)
			return;
		sDpiAPIsInitialized = true;
		HMODULE user32 = GetModuleHandleA("user32.dll");
		if (user32) {
			pSetThreadDpiAwarenessContext =
				(SetThreadDpiAwarenessContextProc)GetProcAddress(user32, "SetThreadDpiAwarenessContext");
			pGetDpiForWindow = (GetDpiForWindowProc)GetProcAddress(user32, "GetDpiForWindow");
			// DPI-awareness au niveau PROCESS (per-monitor v2) : sans ça, sur un moniteur >100 %
			// Windows fait rendre l'app en résolution LOGIQUE puis l'UPSCALE vers le physique
			// -> tout est flou (icônes ET texte). Avec, on rend en pixels PHYSIQUES = net.
			// Doit être appelé AVANT toute fenêtre (ici : à la 1re création). Échoue sans effet
			// si un manifeste a déjà fixé l'awareness — acceptable.
			SetProcessDpiAwarenessContextProc pSetProcessDpiAwarenessContext =
				(SetProcessDpiAwarenessContextProc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
			if (pSetProcessDpiAwarenessContext)
				pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		}
		// shcore.dll (Win 8.1+) pour le DPI par moniteur. Chargée une fois,
		// jamais libérée (vit toute la durée du process) — acceptable.
		HMODULE shcore = LoadLibraryA("shcore.dll");
		if (shcore) {
			pGetDpiForMonitor = (GetDpiForMonitorProc)GetProcAddress(shcore, "GetDpiForMonitor");
		}
	}

	DPI_AWARENESS_CONTEXT NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT ctx) {
		return pSetThreadDpiAwarenessContext ? pSetThreadDpiAwarenessContext(ctx) : nullptr;
	}

	UINT NkGetDpiForWindow(HWND hwnd) {
		return pGetDpiForWindow ? pGetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
	}

} // anonymous namespace

namespace nkentseu {
	using namespace math;

	// =============================================================================
	// Point 2 : registre backend — static dans ce .cpp, invisible à l'extérieur
	// =============================================================================

	// Function-local statics to avoid SIOF with the custom allocator
	static NkUnorderedMap<HWND, NkWindow *> &Win32WindowMap() {
		static NkUnorderedMap<HWND, NkWindow *> sMap;
		return sMap;
	}

	static NkWindow *&Win32LastWindow() {
		static NkWindow *sLast = nullptr;
		return sLast;
	}

	NkWindow *NkWin32FindWindow(HWND hwnd) {
		auto *win = Win32WindowMap().Find(hwnd);
		return win ? *win : nullptr;
	}

	void NkWin32RegisterWindow(HWND hwnd, NkWindow *win) {
		Win32WindowMap()[hwnd] = win;
		Win32LastWindow() = win;
	}

	void NkWin32UnregisterWindow(HWND hwnd) {
		auto &map = Win32WindowMap();
		auto *win = map.Find(hwnd);
		if (!win)
			return;

		NkWindow *w = *win;
		map.Erase(hwnd);

		if (Win32LastWindow() == w) {
			NkWindow *first = nullptr;
			map.ForEach([&](HWND, NkWindow *v) {
				if (!first)
					first = v;
			});
			Win32LastWindow() = first;
		}
	}

	NkWindow *NkWin32GetLastWindow() {
		return Win32LastWindow();
	}

	// =============================================================================
	// Helpers UTF-8 ↔ Wide
	// =============================================================================

	static NkWString NkUtf8ToWide(const NkString &s) {
		if (s.Empty())
			return {};
		int len = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), (int)s.Size(), nullptr, 0);
		NkWString ws((size_t)len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.CStr(), (int)s.Size(), ws.Data(), len);
		return ws;
	}

	static NkString NkWideToUtf8(const NkWString &ws) {
		if (ws.Empty())
			return {};
		int len = WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), (int)ws.Size(), nullptr, 0, nullptr, nullptr);
		NkString s((size_t)len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), (int)ws.Size(), s.Data(), len, nullptr, nullptr);
		return NkString(s.CStr());
	}

	static void DestroyWindowIcons(NkWindowData &data) {
		if (data.mIconBig && data.mIconBig != data.mIconSmall) {
			DestroyIcon(data.mIconBig);
		}
		if (data.mIconSmall) {
			DestroyIcon(data.mIconSmall);
		}
		data.mIconSmall = nullptr;
		data.mIconBig = nullptr;
	}

	static void ApplyWindowIcons(HWND hwnd, NkWindowData &data, const NkString &iconPath) {
		if (!hwnd || iconPath.Empty())
			return;

		DestroyWindowIcons(data);

		const NkWString wPath = NkUtf8ToWide(iconPath);
		const int smallW = GetSystemMetrics(SM_CXSMICON);
		const int smallH = GetSystemMetrics(SM_CYSMICON);
		const int bigW = GetSystemMetrics(SM_CXICON);
		const int bigH = GetSystemMetrics(SM_CYICON);

		HICON smallIcon =
			reinterpret_cast<HICON>(LoadImageW(nullptr, wPath.CStr(), IMAGE_ICON, smallW, smallH, LR_LOADFROMFILE));
		HICON bigIcon =
			reinterpret_cast<HICON>(LoadImageW(nullptr, wPath.CStr(), IMAGE_ICON, bigW, bigH, LR_LOADFROMFILE));

		if (!smallIcon && !bigIcon)
			return;

		data.mIconSmall = smallIcon ? smallIcon : bigIcon;
		data.mIconBig = bigIcon ? bigIcon : smallIcon;

		if (data.mIconSmall) {
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(data.mIconSmall));
		}
		if (data.mIconBig) {
			SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(data.mIconBig));
		}
	}

	static void ApplyTransparency(HWND hwnd) {
		if (!hwnd)
			return;
		SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

		DWM_BLURBEHIND bb = {};
		bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
		bb.fEnable = TRUE;
		bb.fTransitionOnMaximized = FALSE;
		HRGN region = CreateRectRgn(-1, -1, 0, 0);
		bb.hRgnBlur = region;
		DwmEnableBlurBehindWindow(hwnd, &bb);
		if (region)
			DeleteObject(region);
	}

	static void ApplyShadow(HWND hwnd) {
		if (!hwnd)
			return;
		BOOL ncr = TRUE;
		DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncr, sizeof(ncr));
		const MARGINS shadow = {1, 1, 1, 1};
		DwmExtendFrameIntoClientArea(hwnd, &shadow);
	}

	// =============================================================================
	// LE FOND DE LA FENETRE — `bgColor`, enfin lu (25/09)
	//
	// AVANT : `wc.hbrBackground = BLACK_BRUSH`, en dur. C'est CETTE brosse que
	// Windows etale sur la zone nouvellement decouverte pendant un
	// redimensionnement, avant que l'application ait eu sa chance de peindre.
	// D'ou le clignotement NOIR au redimensionnement, sur toutes les
	// applications — pendant que `bgColor` valait `0x141414FF` et n'etait lu
	// nulle part.
	//
	// ⚠️ CHANGEMENT VISIBLE, ASSUME : le clignotement passe du NOIR PUR au
	//    `0x141414` par defaut. Ce n'est pas une regression — c'est la valeur que
	//    `NkWindowConfig` annonce depuis le debut, et elle est plus proche du
	//    theme sombre des editeurs que le noir qu'elle n'a jamais remplace.
	//
	// ⚠️ LA BROSSE APPARTIENT A LA CLASSE, PAS A LA FENETRE, et la classe est
	//    nommee par `config.name`. Deux fenetres de MEME `name` et de `bgColor`
	//    DIFFERENTS partagent donc la brosse de la premiere enregistree. Ce n'est
	//    pas rattrapable sans repeindre nous-memes a chaque WM_ERASEBKGND — ce qui
	//    rendrait le clignotement que le code actuel evite en ne peignant RIEN
	//    (WM_ERASEBKGND rend 1 sans toucher au HDC). Le second appelant recoit
	//    donc un REFUS NOMME au lieu d'une couleur silencieusement ignoree.
	//
	// ⚠️ DUREE DE VIE — ET UN PIEGE PAYE LE 25/09. `NkWindow::Close()` appelle
	//    `UnregisterClassW`, et `UnregisterClass` DETRUIT la brosse de fond de la
	//    classe. Une premiere version gardait un cache de brosses par couleur
	//    pour le processus : la deuxieme fenetre recevait donc un HBRUSH MORT,
	//    et `CreateWindowExW` echouait. Le banc l'a vu parce qu'il ouvre
	//    plusieurs fenetres dans un seul processus ; un banc a une fenetre
	//    serait reste vert.
	//    Regle : la brosse est creee UNIQUEMENT quand la classe ne l'est pas
	//    encore, et c'est `UnregisterClass` qui la detruit. Si la classe existe
	//    deja, on reprend LA SIENNE — jamais une copie, jamais un cache.
	// =============================================================================
	static COLORREF NkWin32CouleurDeFond(uint32 rgba) {
		// `bgColor` est 0xRRGGBBAA ; COLORREF est 0x00BBGGRR. L'alpha n'a pas de
		// sens pour une brosse GDI : il est ignore, et c'est dit ici plutot que
		// laisse deviner.
		return RGB((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF);
	}

	// =============================================================================
	// LE STYLE VIENT DE LA CONFIGURATION — il ne la contredit plus
	//
	// AVANT (jusqu'au 25/09/2026) : `mDwStyle = WS_OVERLAPPEDWINDOW` quoi qu'il
	// arrive. Ce seul mot contient WS_THICKFRAME, WS_MINIMIZEBOX et
	// WS_MAXIMIZEBOX : `resizable = false` etait accepte puis jete. Un
	// utilisateur a perdu du temps a chercher l'erreur chez lui.
	//
	// ⚠️ EQUIVALENCE AUX DEFAUTS. Avec la configuration par defaut (frame,
	//    resizable, minimizable, maximizable tous vrais) cette fonction rend
	//    EXACTEMENT WS_OVERLAPPEDWINDOW (= WS_OVERLAPPED | WS_CAPTION |
	//    WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX), et pour
	//    `frame = false` exactement l'ancien WS_POPUP | WS_THICKFRAME |
	//    WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX. NKWindow est
	//    partage par TOUTES les applications : aucune d'elles ne doit voir sa
	//    fenetre changer parce qu'on a corrige celles qui demandaient autre
	//    chose.
	//
	// ⚠️ `maximizable` est SUBORDONNE a `resizable` : maximiser, c'est
	//    redimensionner. Laisser WS_MAXIMIZEBOX sur une fenetre declaree non
	//    redimensionnable offrirait un bouton qui fait precisement ce qui vient
	//    d'etre interdit.
	//
	// ⚠️ FENETRE SANS CADRE (`frame = false`). WS_THICKFRAME y etait garde
	//    EXPRES : sa bordure invisible donne l'accrochage Aero et le
	//    redimensionnement natif que `BeginResize` relaie depuis notre propre
	//    decoration. DECISION : `resizable = false` l'emporte. Le style tombe,
	//    l'accrochage Aero est perdu, `BeginResize` refuse au journal — et c'est
	//    juste : une fenetre declaree non redimensionnable n'a aucun bord a
	//    accrocher. WS_CAPTION et WS_SYSMENU restent (animation de reduction et
	//    menu systeme ; ils ne peignent aucun pixel puisque WM_NCCALCSIZE rend
	//    toute la fenetre cliente).
	// =============================================================================
	static DWORD NkWin32ComposerStyle(const NkWindowConfig &c) {
		DWORD style = (c.frame ? WS_OVERLAPPED : WS_POPUP) | WS_CAPTION | WS_SYSMENU;
		if (c.resizable)
			style |= WS_THICKFRAME;
		if (c.minimizable)
			style |= WS_MINIMIZEBOX;
		if (c.maximizable && c.resizable)
			style |= WS_MAXIMIZEBOX;
		return style;
	}

	// =============================================================================
	// Menu systeme : griser ce qui est interdit
	//
	// Le style suffit pour `resizable`/`minimizable`/`maximizable` (Windows grise
	// les boutons correspondants tout seul), mais PAS pour `closable` ni
	// `movable` : il n'existe aucun style « pas de bouton X » ni « pas de
	// deplacement ». Ces deux-la passent par le menu systeme — et griser SC_CLOSE
	// eteint AUSSI le bouton X de la barre de titre, c'est le meme item.
	//
	// ⚠️ Griser ne suffit pas non plus a lui seul : le double-clic sur la barre de
	//    titre, Alt+F4 et le glisser de titre n'ouvrent pas le menu, ils envoient
	//    directement le WM_SYSCOMMAND. Le filtre qui les arrete vit dans
	//    NkWin32EventSystem.cpp. Les deux moities sont necessaires ; aucune ne
	//    tient seule.
	// =============================================================================
	static void NkWin32AppliquerMenuSysteme(HWND hwnd, const NkWindowConfig &c) {
		if (!hwnd)
			return;
		if (c.closable && c.movable && c.resizable && c.minimizable && c.maximizable)
			return; // rien d'interdit : ne pas toucher au menu du systeme
		HMENU menu = GetSystemMenu(hwnd, FALSE);
		if (!menu)
			return;
		const UINT eteint = MF_BYCOMMAND | MF_GRAYED | MF_DISABLED;
		if (!c.closable)
			EnableMenuItem(menu, SC_CLOSE, eteint);
		if (!c.movable)
			EnableMenuItem(menu, SC_MOVE, eteint);
		if (!c.resizable)
			EnableMenuItem(menu, SC_SIZE, eteint);
		if (!c.minimizable)
			EnableMenuItem(menu, SC_MINIMIZE, eteint);
		if (!c.maximizable || !c.resizable)
			EnableMenuItem(menu, SC_MAXIMIZE, eteint);
	}

	// =============================================================================
	// Fonctions de synchronisation mData ↔ mConfig
	// =============================================================================

	static void SyncConfigFromWindow(HWND hwnd, NkWindowConfig &config, const NkWindowData &data) {
		if (!hwnd)
			return;

		// Récupérer la position
		RECT winRect;
		GetWindowRect(hwnd, &winRect);
		config.x = winRect.left;
		config.y = winRect.top;

		// Récupérer la taille client
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		config.width = clientRect.right - clientRect.left;
		config.height = clientRect.bottom - clientRect.top;

		// État fenêtré / plein écran
		config.fullscreen = (GetWindowLongW(hwnd, GWL_STYLE) & WS_POPUP) != 0;

		// Visibilité
		config.visible = IsWindowVisible(hwnd) != 0;

		// Titre
		int len = GetWindowTextLengthW(hwnd);
		if (len > 0) {
			NkWString ws((size_t)len + 1, L'\0');
			GetWindowTextW(hwnd, ws.Data(), len + 1);
			ws.Resize((size_t)len);
			config.title = NkWideToUtf8(ws);
		}
	}

	static void SyncWindowFromConfig(HWND hwnd, const NkWindowConfig &config) {
		if (!hwnd)
			return;

		// Titre
		SetWindowTextW(hwnd, NkUtf8ToWide(config.title).CStr());

		// Redimensionner si nécessaire
		RECT currentRect;
		GetClientRect(hwnd, &currentRect);
		uint32 currentW = currentRect.right - currentRect.left;
		uint32 currentH = currentRect.bottom - currentRect.top;

		if (currentW != config.width || currentH != config.height) {
			RECT rc = {0, 0, (LONG)config.width, (LONG)config.height};
			DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
			DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
			AdjustWindowRectEx(&rc, style, FALSE, exStyle);
			SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
		}

		// Position
		RECT winRect;
		GetWindowRect(hwnd, &winRect);
		if (winRect.left != config.x || winRect.top != config.y) {
			SetWindowPos(hwnd, nullptr, config.x, config.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		// Visibilité
		bool isVisible = IsWindowVisible(hwnd) != 0;
		if (isVisible != config.visible) {
			ShowWindow(hwnd, config.visible ? SW_SHOW : SW_HIDE);
		}
	}

	// =============================================================================
	// NkWindow — constructeurs / destructeur
	// =============================================================================

	NkWindow::NkWindow() {
	}

	NkWindow::NkWindow(const NkWindowConfig &config) {
		Create(config);
	}

	NkWindow::~NkWindow() {
		if (mIsOpen)
			Close();
	}

	// =============================================================================
	// Create
	// =============================================================================

	bool NkWindow::Create(const NkWindowConfig &config) {
		mConfig = config;
		mData = {};
		mData.mHInstance = GetModuleHandleW(nullptr);
		mData.mAppliedHints = config.surfaceHints;

		if (config.native.win32PixelFormatShareWindowHandle != 0) {
			mData.mAppliedHints.Set(NkSurfaceHintKey::NK_WGL_SHARE_PIXEL_FORMAT_HWND,
									config.native.win32PixelFormatShareWindowHandle);
		}

		const bool useExternal = config.native.useExternalWindow && config.native.externalWindowHandle != 0;

		mId = NkWESystem::Instance().RegisterWindow(this);
		if (mId == NK_INVALID_WINDOW_ID) {
			mLastError = NkError(1, "RegisterWindow failed");
			return false;
		}

		auto setupDropTarget = [&]() {
			if (!config.dropEnabled || !mData.mHwnd)
				return;
			mData.mDropTarget = new NkWin32DropTarget(mData.mHwnd);
			if (!mData.mDropTarget)
				return;
			mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent &ev) {
				NkDropEnterEvent copy(ev);
				NkWESystem::Events().Enqueue_Public(copy, mId);
			});
			mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent &ev) {
				NkDropLeaveEvent copy(ev);
				NkWESystem::Events().Enqueue_Public(copy, mId);
			});
			mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent &ev) {
				NkDropFileEvent copy(ev);
				NkWESystem::Events().Enqueue_Public(copy, mId);
			});
			mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent &ev) {
				NkDropTextEvent copy(ev);
				NkWESystem::Events().Enqueue_Public(copy, mId);
			});
		};

		if (useExternal) {
			HWND hwnd = reinterpret_cast<HWND>(config.native.externalWindowHandle);
			if (!hwnd || !IsWindow(hwnd)) {
				mLastError = NkError(1, "External HWND is invalid.");
				NkWESystem::Instance().UnregisterWindow(mId);
				mId = NK_INVALID_WINDOW_ID;
				return false;
			}

			mData.mHwnd = hwnd;
			mData.mExternal = true;
			mData.mHInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
			mData.mDwStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
			mData.mDwExStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
			mData.mParentHwnd = GetParent(hwnd);
			mData.mPrevUserData = GetWindowLongPtrW(hwnd, GWLP_USERDATA);

			::SetLastError(0);
			mData.mPrevWndProc = reinterpret_cast<WNDPROC>(
				SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NkWin32WndProc)));
			if (!mData.mPrevWndProc && ::GetLastError() != 0) {
				mLastError = NkError(2, NkString::Fmtf("Subclass external HWND failed (%lu)", ::GetLastError()));
				NkWESystem::Instance().UnregisterWindow(mId);
				mId = NK_INVALID_WINDOW_ID;
				mData = {};
				return false;
			}

			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			NkWin32RegisterWindow(hwnd, this);
			setupDropTarget();

			CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, kIIDTaskbarList3,
							 reinterpret_cast<void **>(&mData.mTaskbarList));

			// Synchroniser mConfig depuis l'état réel de la fenêtre externe
			SyncConfigFromWindow(hwnd, mConfig, mData);

			mIsOpen = true;
			return true;
		}

		NkWString wClassName = NkUtf8ToWide(config.name);
		NkWString wTitle = NkUtf8ToWide(config.title);

		if (config.fullscreen) {
			// [FIX 2026-07-25] DEVMODEW explicite (cf. NkWindowData::mDmScreen) —
			// le macro DEVMODE dépend de UNICODE et cassait le layout partagé.
			DEVMODEW dm = {};
			dm.dmSize = sizeof(DEVMODEW);
			dm.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
			dm.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
			dm.dmBitsPerPel = 32;
			dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			mData.mDmScreen = dm;
			ChangeDisplaySettingsW(&mData.mDmScreen, CDS_FULLSCREEN);
			mData.mDwExStyle = WS_EX_APPWINDOW;
			mData.mDwStyle = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		} else {
			mData.mDwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
			// Le style DESCEND de la configuration (cf. NkWin32ComposerStyle) : il ne
			// vaut plus WS_OVERLAPPEDWINDOW quoi qu'on demande.
			mData.mDwStyle = NkWin32ComposerStyle(config);
			// frame=false : on SUPPRIME visuellement la zone non-cliente (titre +
			// bordure OS) via WM_NCCALCSIZE -> seule notre deco s'affiche.
			mData.mBorderless = !config.frame;
		}

		mData.mParentHwnd = reinterpret_cast<HWND>(config.native.parentWindowHandle);

		if (config.native.utilityWindow) {
			mData.mDwExStyle |= WS_EX_TOOLWINDOW;
			mData.mDwExStyle &= ~WS_EX_APPWINDOW;
		}
		if (config.transparent) {
			mData.mDwExStyle |= WS_EX_LAYERED;
		}
		// Fenêtre discrète : poser les styles DÈS la création évite le
		// clignotement d'une fenêtre normale corrigée une frame plus tard.
		// WS_EX_TRANSPARENT (click-through) n'agit que sur une fenêtre layered.
		if (config.alwaysOnTop) {
			mData.mDwExStyle |= WS_EX_TOPMOST;
		}
		if (config.clickThrough) {
			mData.mDwExStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
		}
		if (config.opacity < 1.0f) {
			mData.mDwExStyle |= WS_EX_LAYERED;
		}
		// (25/09) NE PAS VOLER LE FOCUS. `WS_EX_NOACTIVATE` empeche la fenetre de
		// devenir active au clic comme a l'affichage ; `SW_SHOWNOACTIVATE` plus bas
		// empeche l'activation initiale. Les deux sont necessaires : le style seul
		// laisse passer l'activation de la PREMIERE apparition.
		if (config.noActivate) {
			mData.mDwExStyle |= WS_EX_NOACTIVATE;
		}

		if (config.native.utilityWindow && !mData.mParentHwnd) {
			mData.mUtilityOwner =
				CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, mData.mHInstance, nullptr);
			mData.mParentHwnd = mData.mUtilityOwner;
		}

		RECT rc = {config.x, config.y, config.x + static_cast<LONG>(config.width),
				   config.y + static_cast<LONG>(config.height)};
		// Fenetre SANS decoration : WM_NCCALCSIZE rend TOUTE la fenetre cliente,
		// donc la taille exterieure EST la taille client — appliquer
		// AdjustWindowRectEx (calcule pour les styles caches WS_CAPTION/THICKFRAME
		// conserves) creait une fenetre 1296x839 pour un client demande 1280x800 :
		// swapchain et fenetre desaccordees, bande morte a droite/en bas (vecu
		// dans NkRef borderless, 2026-08-11).
		if (!mData.mBorderless) {
			AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);
		}

		// AppUserModelID : indispensable pour que Windows associe correctement
		// l'icone de la fenetre runtime avec celle de l'exe quand l'app est
		// installee via MSI/EXE installer. Sans AUMID explicite, Windows
		// generee un AUMID base sur le ProductCode MSI ce qui casse le
		// lookup d'icone (la fenetre apparait dans la taskbar avec une icone
		// generique au lieu de celle de Pong). Avec un AUMID stable, Windows
		// utilise les icones embarquees dans l'exe pour TOUS les contextes
		// (taskbar, titre fenetre, Alt+Tab, jump list).
		// L'API est dans shell32.dll/shobjidl.h, dispo depuis Windows 7.
		// No-op si appel multiple : on set qu'une fois par process.
		{
			static bool s_aumidSet = false;
			if (!s_aumidSet) {
				// ID stable par defaut. L'application peut override via
				// NkAppUserModelID env var pour les installations multi-version.
				const wchar_t *aumid = L"Rihen.Nkentseu.Pong";
				wchar_t buf[256];
				DWORD len = GetEnvironmentVariableW(L"NkAppUserModelID", buf, 256);
				if (len > 0 && len < 256)
					aumid = buf;
				SetCurrentProcessExplicitAppUserModelID(aumid);
				s_aumidSet = true;
			}
		}

		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc = NkWin32WndProc;
		wc.hInstance = mData.mHInstance;
		// Tente de charger l'icone embarquee dans le .exe via les ressources
		// PE/COFF (jenga genere IDI_ICON1 via app_icon.rc -> app_icon.res
		// lors du link). On utilise LoadImage avec les tailles SM/Big pour
		// que la barre des taches (qui consomme ICON_BIG via WM_SETICON)
		// recoive la bonne resolution.
		HINSTANCE hExeInst = GetModuleHandleW(nullptr);
		const int smCX = GetSystemMetrics(SM_CXSMICON); // 16 generalement
		const int smCY = GetSystemMetrics(SM_CYSMICON);
		const int bgCX = GetSystemMetrics(SM_CXICON); // 32 generalement
		const int bgCY = GetSystemMetrics(SM_CYICON);
		HICON hExeIconSmall = static_cast<HICON>(
			LoadImageW(hExeInst, MAKEINTRESOURCEW(1), IMAGE_ICON, smCX, smCY, LR_DEFAULTCOLOR | LR_SHARED));
		HICON hExeIconBig = static_cast<HICON>(
			LoadImageW(hExeInst, MAKEINTRESOURCEW(1), IMAGE_ICON, bgCX, bgCY, LR_DEFAULTCOLOR | LR_SHARED));
		wc.hIcon = hExeIconBig ? hExeIconBig : LoadIconW(nullptr, IDI_APPLICATION);
		wc.hIconSm = hExeIconSmall ? hExeIconSmall : LoadIconW(nullptr, IDI_WINLOGO);
		// Memorise pour WM_SETICON apres CreateWindowEx (la barre des taches
		// utilise specifiquement WM_SETICON ICON_BIG, pas WNDCLASSEX seul).
		mData.mIconBig = wc.hIcon;
		mData.mIconSmall = wc.hIconSm;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		// `bgColor` : la brosse est posee plus bas, une fois su si la classe
		// existe deja (elle en possede alors une, qu'il ne faut pas doubler).
		wc.hbrBackground = nullptr;
		wc.lpszClassName = wClassName.CStr();
		// ── LE FOND : `bgColor` agit, et sa reserve se dit ────────────────────
		{
			const COLORREF voulue = NkWin32CouleurDeFond(config.bgColor);
			WNDCLASSEXW existante = {};
			existante.cbSize = sizeof(WNDCLASSEXW);
			if (GetClassInfoExW(mData.mHInstance, wClassName.CStr(), &existante)) {
				// La classe existe : elle POSSEDE deja sa brosse et la detruira a
				// son desenregistrement. On reprend la sienne, on n'en cree pas une
				// seconde — et si la couleur demandee n'est pas la sienne, on le DIT.
				wc.hbrBackground = existante.hbrBackground;
				LOGBRUSH lb = {};
				if (GetObjectW(existante.hbrBackground, sizeof(lb), &lb) && lb.lbColor != voulue) {
					NkWindowRefuserUneFois(
						NkWindowProp::BgColor, "Win32",
						"la brosse de fond appartient a la CLASSE de fenetre, nommee par config.name : "
						"une classe deja enregistree garde la couleur de la premiere fenetre. Donnez un "
						"`name` distinct pour une couleur distincte");
				}
			} else {
				// Classe neuve : la brosse lui appartient des l'enregistrement, et
				// `UnregisterClassW` (dans Close) la detruira. Rien a liberer ici.
				wc.hbrBackground = CreateSolidBrush(voulue);
				if (!wc.hbrBackground)
					wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
			}
		}
		RegisterClassExW(&wc);

		InitializeDpiAPIs();
		DPI_AWARENESS_CONTEXT prev = NkSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		mData.mHwnd =
			CreateWindowExW(mData.mDwExStyle, wClassName.CStr(), wTitle.CStr(), mData.mDwStyle, 0, 0,
							rc.right - rc.left, rc.bottom - rc.top, mData.mParentHwnd, nullptr, mData.mHInstance, this);

		NkSetThreadDpiAwarenessContext(prev);

		if (!mData.mHwnd) {
			mLastError = NkError(3, NkString::Fmtf("CreateWindowExW failed (%lu)", ::GetLastError()));
			NkWESystem::Instance().UnregisterWindow(mId);
			mId = NK_INVALID_WINDOW_ID;
			if (mData.mUtilityOwner) {
				DestroyWindow(mData.mUtilityOwner);
				mData.mUtilityOwner = nullptr;
			}
			return false;
		}

		// Force l'icone de la fenetre via WM_SETICON. WNDCLASSEX::hIcon ne
		// suffit pas pour la BARRE DES TACHES sur certains Windows : il faut
		// explicitement passer ICON_BIG (taskbar) et ICON_SMALL (titre +
		// Alt+Tab) via SendMessage. Sans ca, Windows affiche son icone
		// generique "Application" meme si l'exe embarque IDI_ICON1.
		if (mData.mIconBig) {
			SendMessageW(mData.mHwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(mData.mIconBig));
		}
		if (mData.mIconSmall) {
			SendMessageW(mData.mHwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(mData.mIconSmall));
		}

		// Fenetre sans decoration : force le recalcul du cadre DES LA CREATION
		// (sinon WM_NCCALCSIZE ne s'applique qu'au 1er redimensionnement -> la barre
		// de titre OS reste visible au lancement).
		if (mData.mBorderless) {
			SetWindowPos(mData.mHwnd, nullptr, 0, 0, 0, 0,
						 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}

		if (!config.fullscreen) {
			const int sw = GetSystemMetrics(SM_CXSCREEN);
			const int sh = GetSystemMetrics(SM_CYSCREEN);
			const int ww = rc.right - rc.left;
			const int wh = rc.bottom - rc.top;
			if (config.centered) {
				SetWindowPos(mData.mHwnd, nullptr, (sw - ww) / 2, (sh - wh) / 2, ww, wh, SWP_NOZORDER);
			} else {
				SetWindowPos(mData.mHwnd, nullptr, config.x, config.y, ww, wh, SWP_NOZORDER);
			}
		}

		if (config.transparent) {
			ApplyTransparency(mData.mHwnd);
		} else if (config.hasShadow) {
			ApplyShadow(mData.mHwnd);
		}

		// Une fenêtre WS_EX_LAYERED qui n'a JAMAIS reçu SetLayeredWindowAttributes
		// ne peint rien (piège Win32 classique) : si le style a été posé pour le
		// click-through ou l'opacité — et pas par ApplyTransparency qui le fait
		// déjà — armer l'alpha maintenant, même à 255.
		if ((config.clickThrough || config.opacity < 1.0f) && !config.transparent) {
			const float32 a = config.opacity < 0.0f ? 0.0f : (config.opacity > 1.0f ? 1.0f : config.opacity);
			SetLayeredWindowAttributes(mData.mHwnd, 0, static_cast<BYTE>(a * 255.0f + 0.5f), LWA_ALPHA);
		} else if (config.transparent && config.opacity < 1.0f) {
			SetOpacity(config.opacity); // ré-applique l'alpha par-dessus les 255 d'ApplyTransparency
		}

		ApplyWindowIcons(mData.mHwnd, mData, config.iconPath);

		// `closable` et `movable` n'ont pas de style : ils vivent dans le menu
		// systeme (et dans le filtre WM_SYSCOMMAND de NkWin32EventSystem.cpp).
		NkWin32AppliquerMenuSysteme(mData.mHwnd, config);

		// (L'audit de ce que Win32 ne tient pas est declenche pour TOUS les dorsaux
		//  depuis NkWESystem::RegisterWindow — un seul endroit, une seule verite.)

		CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, kIIDTaskbarList3,
						 reinterpret_cast<void **>(&mData.mTaskbarList));

		setupDropTarget();

		if (config.visible) {
			if (config.noActivate) {
				// ⚠️ NI `SetForegroundWindow` NI `SetFocus` : les appeler ici
				//    annulerait `WS_EX_NOACTIVATE` d'une ligne. Le style dit « ne
				//    m'active pas » ; ces deux appels disent « active-moi ». Le
				//    dernier qui parle gagne, et ce serait eux.
				ShowWindow(mData.mHwnd, SW_SHOWNOACTIVATE);
			} else {
				ShowWindow(mData.mHwnd, SW_SHOWNORMAL);
				SetForegroundWindow(mData.mHwnd);
				SetFocus(mData.mHwnd);
			}
		}

		// Synchronisation initiale : mConfig reflète l'état réel
		SyncConfigFromWindow(mData.mHwnd, mConfig, mData);

		mIsOpen = true;
		return true;
	}

	// =============================================================================
	// Close
	// =============================================================================

	void NkWindow::Close() {
		if (!mIsOpen)
			return;

		const HWND hwnd = mData.mHwnd;
		NkWin32UnregisterWindow(hwnd);
		NkWESystem::Instance().UnregisterWindow(mId);

		if (mData.mDropTarget) {
			delete mData.mDropTarget;
			mData.mDropTarget = nullptr;
		}

		if (mData.mTaskbarList) {
			mData.mTaskbarList->Release();
			mData.mTaskbarList = nullptr;
		}

		if (hwnd) {
			if (mData.mExternal) {
				if (mData.mPrevWndProc && mData.mPrevWndProc != NkWin32WndProc) {
					SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(mData.mPrevWndProc));
				}
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, mData.mPrevUserData);
			} else {
				DestroyWindow(hwnd);
			}
		}

		if (!mData.mExternal && mData.mHInstance) {
			UnregisterClassW(NkUtf8ToWide(mConfig.name).CStr(), mData.mHInstance);
		}

		if (mData.mUtilityOwner) {
			DestroyWindow(mData.mUtilityOwner);
			mData.mUtilityOwner = nullptr;
		}

		DestroyWindowIcons(mData);

		mData.mHwnd = nullptr;
		mData.mPrevWndProc = nullptr;
		mData.mPrevUserData = 0;
		mData.mExternal = false;

		mId = NK_INVALID_WINDOW_ID;
		mIsOpen = false;
	}

	// =============================================================================
	// State
	// =============================================================================

	bool NkWindow::IsOpen() const {
		return mIsOpen;
	}

	bool NkWindow::IsValid() const {
		return mIsOpen && mData.mHwnd != nullptr;
	}

	NkError NkWindow::GetLastError() const {
		return mLastError;
	}

	NkWindowConfig NkWindow::GetConfig() const {
		// Synchroniser avant de retourner
		if (mIsOpen && mData.mHwnd) {
			NkWindowConfig updatedConfig = mConfig;
			SyncConfigFromWindow(mData.mHwnd, updatedConfig, mData);
			const_cast<NkWindow *>(this)->mConfig = updatedConfig;
		}
		return mConfig;
	}

	// =============================================================================
	// Title
	// =============================================================================

	NkString NkWindow::GetTitle() const {
		if (!mData.mHwnd)
			return {};
		int len = GetWindowTextLengthW(mData.mHwnd);
		if (len <= 0)
			return {};
		NkWString ws((size_t)len + 1, L'\0');
		GetWindowTextW(mData.mHwnd, ws.Data(), len + 1);
		ws.Resize((size_t)len);
		NkString title = NkWideToUtf8(ws);

		// Synchroniser mConfig
		const_cast<NkWindow *>(this)->mConfig.title = title;

		return title;
	}

	void NkWindow::SetTitle(const NkString &t) {
		mConfig.title = t;
		if (mData.mHwnd) {
			SetWindowTextW(mData.mHwnd, NkUtf8ToWide(t).CStr());
			// La synchronisation est déjà faite via la modification de mConfig
		}
	}

	// =============================================================================
	// Size / Position
	// =============================================================================

	NkVec2u NkWindow::GetSize() const {
		RECT rc = {};
		if (mData.mHwnd)
			GetClientRect(mData.mHwnd, &rc);
		NkVec2u size = {(uint32)(rc.right - rc.left), (uint32)(rc.bottom - rc.top)};

		// Synchroniser mConfig
		const_cast<NkWindow *>(this)->mConfig.width = size.x;
		const_cast<NkWindow *>(this)->mConfig.height = size.y;

		return size;
	}

	NkVec2u NkWindow::GetPosition() const {
		RECT rc = {};
		if (mData.mHwnd)
			GetWindowRect(mData.mHwnd, &rc);
		NkVec2u pos = {(uint32)rc.left, (uint32)rc.top};

		// Synchroniser mConfig
		const_cast<NkWindow *>(this)->mConfig.x = pos.x;
		const_cast<NkWindow *>(this)->mConfig.y = pos.y;

		return pos;
	}

	float NkWindow::GetDpiScale() const {
		return mData.mHwnd ? (float)NkGetDpiForWindow(mData.mHwnd) / USER_DEFAULT_SCREEN_DPI : 1.f;
	}

	NkVec2u NkWindow::GetDisplaySize() const {
		return {(uint32)GetSystemMetrics(SM_CXSCREEN), (uint32)GetSystemMetrics(SM_CYSCREEN)};
	}

	NkVec2u NkWindow::GetDisplayPosition() const {
		return {0, 0};
	}

	// =============================================================================
	// Moniteurs / Display (hot-plug + DPI runtime)
	// =============================================================================

	// Remplit un NkDisplayInfo depuis un HMONITOR : géométrie (rcMonitor),
	// primaire, résolution physique + refresh (EnumDisplaySettings), DPI
	// effectif (GetDpiForMonitor shcore, fallback 96), nom du device.
	static NkDisplayInfo Win32FillDisplayInfo(HMONITOR hmon, uint32 index) {
		InitializeDpiAPIs();
		NkDisplayInfo info;
		info.index = index;

		MONITORINFOEXW mi{};
		mi.cbSize = sizeof(mi);
		if (GetMonitorInfoW(hmon, &mi)) {
			info.posX = (int32)mi.rcMonitor.left;
			info.posY = (int32)mi.rcMonitor.top;
			info.width = (uint32)(mi.rcMonitor.right - mi.rcMonitor.left);
			info.height = (uint32)(mi.rcMonitor.bottom - mi.rcMonitor.top);
			info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

			// Résolution physique + fréquence de rafraîchissement.
			DEVMODEW dm{};
			dm.dmSize = sizeof(dm);
			if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
				info.physWidth = (uint32)dm.dmPelsWidth;
				info.physHeight = (uint32)dm.dmPelsHeight;
				info.refreshRate = (uint32)dm.dmDisplayFrequency;
			}

			// Nom lisible (device path, ex. "\\.\DISPLAY1").
			NkString dev = NkWideToUtf8(NkWString(mi.szDevice));
			usize n = dev.Size() < (sizeof(info.name) - 1) ? dev.Size() : (sizeof(info.name) - 1);
			for (usize i = 0; i < n; ++i)
				info.name[i] = dev.CStr()[i];
			info.name[n] = '\0';
		}

		// DPI effectif par moniteur (shcore). Fallback : DPI système (96).
		UINT dx = USER_DEFAULT_SCREEN_DPI, dy = USER_DEFAULT_SCREEN_DPI;
		if (pGetDpiForMonitor && pGetDpiForMonitor(hmon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy) == S_OK) {
			// ok
		}
		info.dpiX = (float32)dx;
		info.dpiY = (float32)dy;
		info.dpiScale = (float32)dx / (float32)USER_DEFAULT_SCREEN_DPI;
		return info;
	}

	static BOOL CALLBACK Win32MonitorEnumProc(HMONITOR hmon, HDC, LPRECT, LPARAM lparam) {
		auto *vec = reinterpret_cast<NkVector<HMONITOR> *>(lparam);
		vec->PushBack(hmon);
		return TRUE;
	}

	NkVector<NkDisplayInfo> NkWindow::EnumerateMonitors() const {
		NkVector<HMONITOR> mons;
		EnumDisplayMonitors(nullptr, nullptr, Win32MonitorEnumProc, reinterpret_cast<LPARAM>(&mons));
		NkVector<NkDisplayInfo> out;
		for (usize i = 0; i < mons.Size(); ++i)
			out.PushBack(Win32FillDisplayInfo(mons[i], (uint32)i));
		return out;
	}

	NkDisplayInfo NkWindow::GetCurrentMonitor() const {
		HMONITOR hmon = mData.mHwnd ? MonitorFromWindow(mData.mHwnd, MONITOR_DEFAULTTONEAREST)
									: MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
		return Win32FillDisplayInfo(hmon, 0);
	}

	uint32 NkWindow::GetMonitorCount() const {
		int n = GetSystemMetrics(SM_CMONITORS);
		return n > 0 ? (uint32)n : 1u;
	}

	void NkWindow::SetSize(uint32 w, uint32 h) {
		mConfig.width = w;
		mConfig.height = h;

		RECT rc = {0, 0, (LONG)w, (LONG)h};
		AdjustWindowRectEx(&rc, mData.mDwStyle, FALSE, mData.mDwExStyle);
		SetWindowPos(mData.mHwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
	}

	void NkWindow::SetPosition(int32 x, int32 y) {
		mConfig.x = x;
		mConfig.y = y;

		SetWindowPos(mData.mHwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	void NkWindow::SetVisible(bool v) {
		mConfig.visible = v;
		ShowWindow(mData.mHwnd, v ? SW_SHOW : SW_HIDE);
	}

	void NkWindow::Minimize() {
		ShowWindow(mData.mHwnd, SW_MINIMIZE);
		// L'état de visibilité change, mais pas la taille/position
		mConfig.visible = IsWindowVisible(mData.mHwnd) != 0;
	}

	void NkWindow::Maximize() {
		ShowWindow(mData.mHwnd, IsZoomed(mData.mHwnd) ? SW_RESTORE : SW_MAXIMIZE);
		// Mettre à jour la taille après maximisation
		RECT rc;
		GetClientRect(mData.mHwnd, &rc);
		mConfig.width = rc.right - rc.left;
		mConfig.height = rc.bottom - rc.top;
		mConfig.visible = true;
	}

	void NkWindow::Restore() {
		ShowWindow(mData.mHwnd, SW_RESTORE);
		// Mettre à jour après restauration
		RECT rc;
		GetClientRect(mData.mHwnd, &rc);
		mConfig.width = rc.right - rc.left;
		mConfig.height = rc.bottom - rc.top;
		mConfig.visible = true;
	}

	// Win32 : la decoration est un STYLE de fenetre. WS_OVERLAPPEDWINDOW porte
	// bordure + barre de titre ; WS_POPUP n'en a aucune. SetWindowPos avec
	// SWP_FRAMECHANGED est indispensable pour que le nouveau style soit
	// reellement applique — sans lui, le changement reste invisible jusqu'au
	// prochain redimensionnement.
	// ⚠️ 25/09 — CE SETTER DEFAISAIT LE CORRECTIF. Il remettait
	//    `WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX` SANS CONDITION : une
	//    fenetre creee `resizable = false` redevenait redimensionnable des le
	//    premier passage par une barre de titre personnalisee. Corriger la
	//    creation sans corriger ceci n'aurait tenu que jusqu'au premier appel.
	//    Il repasse donc par la MEME fonction que la creation.
	void NkWindow::SetDecorated(bool decorated) {
		mConfig.frame = decorated;
		if (!mData.mHwnd)
			return;
		// mBorderless commande WM_NCCALCSIZE : sans cette ligne, SetDecorated(false)
		// retirait la barre de titre du STYLE mais laissait l'OS peindre son cadre.
		mData.mBorderless = !decorated;
		LONG_PTR style = GetWindowLongPtrW(mData.mHwnd, GWL_STYLE);
		style &= ~(LONG_PTR)(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_POPUP);
		style |= (LONG_PTR)NkWin32ComposerStyle(mConfig);
		mData.mDwStyle = (DWORD)style;
		SetWindowLongPtrW(mData.mHwnd, GWL_STYLE, style);
		// Le menu systeme est reconstruit avec le style : re-griser ce qui reste interdit.
		NkWin32AppliquerMenuSysteme(mData.mHwnd, mConfig);
		SetWindowPos(mData.mHwnd, nullptr, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	bool NkWindow::IsDecorated() const {
		return mConfig.frame;
	}

	bool NkWindow::IsMaximized() const {
		return mData.mHwnd && IsZoomed(mData.mHwnd) != 0;
	}

	bool NkWindow::IsMinimized() const {
		// Une fenetre REDUITE n'a plus de surface de rendu utile, mais Windows
		// lui laisse un rect de placeholder (~160x28) : tester la taille ne
		// detecte donc JAMAIS la minimisation. C'est ce rect qui partait en
		// ResizeSwapchain et tuait NK3DModeler a la restauration (defaut 4.3).
		return mData.mHwnd && IsIconic(mData.mHwnd) != 0;
	}

	void NkWindow::BeginDragMove() {
		// Hand-off natif : l'OS prend la main sur le deplacement (snap Aero inclus).
		// Indispensable pour une barre de titre custom (fenetre sans bordure).
		if (!mData.mHwnd)
			return;
		if (!mConfig.movable) {
			NkWindowRefuserUneFois(NkWindowProp::Movable, "Win32",
								   "BeginDragMove appele sur une fenetre creee movable=false : ignore");
			return;
		}
		ReleaseCapture();
		SendMessageW(mData.mHwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
	}

	// ── Presse-papiers OS (CF_UNICODETEXT, conversion UTF-8 <-> UTF-16) ──────────
	void NkWindow::SetClipboardText(const NkString &text) {
		if (!OpenClipboard(mData.mHwnd))
			return;
		EmptyClipboard();
		const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.CStr(), -1, nullptr, 0);
		if (wlen > 0) {
			HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
			if (hg) {
				wchar_t *dst = static_cast<wchar_t *>(GlobalLock(hg));
				if (dst) {
					MultiByteToWideChar(CP_UTF8, 0, text.CStr(), -1, dst, wlen);
					GlobalUnlock(hg);
					SetClipboardData(CF_UNICODETEXT, hg);
				} else
					GlobalFree(hg);
			}
		}
		CloseClipboard();
	}

	NkString NkWindow::GetClipboardText() const {
		NkString out;
		if (!OpenClipboard(mData.mHwnd))
			return out;
		HANDLE h = GetClipboardData(CF_UNICODETEXT);
		if (h) {
			const wchar_t *w = static_cast<const wchar_t *>(GlobalLock(h));
			if (w) {
				const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
				if (len > 0) {
					NkVector<char> buf;
					buf.Resize(static_cast<usize>(len));
					WideCharToMultiByte(CP_UTF8, 0, w, -1, buf.Data(), len, nullptr, nullptr);
					out = NkString(buf.Data());
				}
				GlobalUnlock(h);
			}
		}
		CloseClipboard();
		return out;
	}

	// ── (Q9) L'IMAGE DU PRESSE-PAPIERS : CF_DIBV5 / CF_DIB -> RGBA ───────────────
	// ⚠️ UN DIB EST RANGE DU BAS VERS LE HAUT quand biHeight > 0 ; en BGR(A) ;
	//    avec des lignes alignees sur 4 octets ; et BI_BITFIELDS pose 3 masques
	//    apres l'en-tete d'un BITMAPINFOHEADER (pas d'un V5, qui les porte). Chacun
	//    de ces quatre details, oublie, donne une image a l'envers, bleue, cisaillee
	//    ou decalee de 12 octets.
	bool NkWindow::GetClipboardImage(NkVector<uint8> &rgba, int32 &w, int32 &h, NkString &motif) const {
		w = h = 0;
		rgba.Clear();
		if (!IsClipboardFormatAvailable(CF_DIB) && !IsClipboardFormatAvailable(CF_DIBV5)) {
			motif = NkString("le presse-papiers ne contient pas d'image");
			return false;
		}
		if (!OpenClipboard(mData.mHwnd)) {
			motif = NkString("presse-papiers occupe par une autre application");
			return false;
		}
		HANDLE hd = GetClipboardData(IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 : CF_DIB);
		bool ok = false;
		if (hd) {
			const uint8 *p = static_cast<const uint8 *>(GlobalLock(hd));
			const SIZE_T taille = GlobalSize(hd);
			if (p && taille >= sizeof(BITMAPINFOHEADER)) {
				const BITMAPINFOHEADER *bi = reinterpret_cast<const BITMAPINFOHEADER *>(p);
				const int32 bw = (int32)bi->biWidth;
				const int32 bh = bi->biHeight < 0 ? -(int32)bi->biHeight : (int32)bi->biHeight;
				const bool basEnHaut = bi->biHeight > 0;
				const int32 bpp = (int32)bi->biBitCount;
				usize decal = bi->biSize;
				if (bi->biCompression == BI_BITFIELDS && bi->biSize == sizeof(BITMAPINFOHEADER))
					decal += 12u;
				decal += (usize)bi->biClrUsed * 4u;
				const usize ligne = (((usize)bw * (usize)bpp + 31u) / 32u) * 4u;
				if (bw > 0 && bh > 0 && bw <= 16384 && bh <= 16384 && (bpp == 24 || bpp == 32) &&
					(bi->biCompression == BI_RGB || bi->biCompression == BI_BITFIELDS) &&
					decal + ligne * (usize)bh <= (usize)taille) {
					rgba.Resize((usize)bw * (usize)bh * 4u);
					// un 32 bits dont l'alpha vaut 0 partout : l'alpha n'est pas porte
					bool alphaNul = bpp == 32;
					for (int32 y = 0; y < bh && alphaNul; ++y) {
						const uint8 *s = p + decal + (usize)y * ligne;
						for (int32 x = 0; x < bw; ++x)
							if (s[x * 4 + 3] != 0) {
								alphaNul = false;
								break;
							}
					}
					for (int32 y = 0; y < bh; ++y) {
						const int32 sy = basEnHaut ? (bh - 1 - y) : y;
						const uint8 *s = p + decal + (usize)sy * ligne;
						uint8 *d = rgba.Data() + (usize)y * (usize)bw * 4u;
						for (int32 x = 0; x < bw; ++x) {
							const uint8 *px = s + x * (bpp / 8);
							d[x * 4 + 0] = px[2];
							d[x * 4 + 1] = px[1];
							d[x * 4 + 2] = px[0];
							d[x * 4 + 3] = (bpp == 32 && !alphaNul) ? px[3] : 255u;
						}
					}
					w = bw;
					h = bh;
					ok = true;
				} else
					motif = NkString("format d'image du presse-papiers non pris en charge (ni 24 ni 32 bits non compresses)");
				GlobalUnlock(hd);
			}
		}
		CloseClipboard();
		if (!ok && motif.Length() == 0)
			motif = NkString("image du presse-papiers illisible");
		return ok;
	}

	// ── Fenêtre discrète : opacité / toujours-devant / click-through ─────────────
	//
	// Les trois reposent sur les styles étendus. Règle WS_EX_LAYERED : il reste
	// posé tant qu'UN consommateur en a besoin (transparent, opacité < 1 ou
	// click-through) et n'est retiré que quand plus personne ne s'en sert — le
	// retirer trop tôt casserait la transparence du fond, le garder pour rien
	// coûte une composition d'écran inutile.

	void NkWindow::SetOpacity(float32 opacity) {
		if (opacity < 0.0f)
			opacity = 0.0f;
		if (opacity > 1.0f)
			opacity = 1.0f;
		mConfig.opacity = opacity;
		if (!mData.mHwnd)
			return;
		LONG_PTR ex = GetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE);
		if (opacity >= 1.0f && !mConfig.transparent && !mConfig.clickThrough) {
			// Plus personne n'a besoin du layering : fenêtre redevenue ordinaire.
			if (ex & WS_EX_LAYERED)
				SetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
			return;
		}
		if (!(ex & WS_EX_LAYERED))
			SetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
		SetLayeredWindowAttributes(mData.mHwnd, 0, static_cast<BYTE>(opacity * 255.0f + 0.5f), LWA_ALPHA);
	}

	float32 NkWindow::GetOpacity() const {
		return mConfig.opacity;
	}

	void NkWindow::SetAlwaysOnTop(bool onTop) {
		mConfig.alwaysOnTop = onTop;
		if (!mData.mHwnd)
			return;
		SetWindowPos(mData.mHwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	bool NkWindow::IsAlwaysOnTop() const {
		// L'OS fait autorité : un autre process peut retirer le topmost.
		if (mData.mHwnd)
			return (GetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
		// Pas de fenetre native : rien n'a ete applique, donc rien n'est vrai.
		// Meme regle que l'arbitrage du 25/09 — un accesseur decrit le monde.
		return false;
	}

	void NkWindow::SetClickThrough(bool clickThrough) {
		mConfig.clickThrough = clickThrough;
		if (!mData.mHwnd)
			return;
		LONG_PTR ex = GetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE);
		const bool hadLayered = (ex & WS_EX_LAYERED) != 0;
		if (clickThrough) {
			ex |= WS_EX_LAYERED | WS_EX_TRANSPARENT; // TRANSPARENT n'agit que layered
		} else {
			ex &= ~WS_EX_TRANSPARENT;
			if (!mConfig.transparent && mConfig.opacity >= 1.0f)
				ex &= ~WS_EX_LAYERED; // cf. règle en tête de section
		}
		SetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE, ex);
		// Layered fraîchement posé sans alpha armé = fenêtre qui ne peint plus.
		if (clickThrough && !hadLayered)
			SetLayeredWindowAttributes(mData.mHwnd, 0, static_cast<BYTE>(mConfig.opacity * 255.0f + 0.5f), LWA_ALPHA);
	}

	bool NkWindow::IsClickThrough() const {
		if (mData.mHwnd)
			return (GetWindowLongPtrW(mData.mHwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0;
		// Pas de fenetre native : rien n'a ete applique, donc rien n'est vrai.
		// Meme regle que l'arbitrage du 25/09 — un accesseur decrit le monde.
		return false;
	}

	// ── Presse-papiers OS image (CF_DIBV5 / CF_DIB ↔ RGBA8) ─────────────────────
	//
	// Écriture : on pose CF_DIBV5 (masques BGRA explicites, alpha réel) ET
	// CF_DIB 32 bpp (les consommateurs anciens ne lisent que lui). Lecture :
	// CF_DIBV5 d'abord (alpha fiable), sinon CF_DIB — que Windows synthétise
	// au besoin depuis CF_BITMAP, donc une capture d'écran est toujours lisible.
	// Pas de PNG : le décoder exigerait NKImage, que NKWindow ne tire pas.

	namespace {
		// Lignes DIB : alignées 32 bits, ordre BAS → HAUT quand height > 0.
		inline uint32 NkDibStride(uint32 width, uint32 bitCount) {
			return ((width * bitCount + 31u) / 32u) * 4u;
		}

		// Position du bit le plus bas d'un masque (0 si masque nul) — pour
		// décoder les BI_BITFIELDS quel que soit l'agencement des canaux.
		inline uint32 NkMaskShift(uint32 mask) {
			if (!mask)
				return 0;
			uint32 shift = 0;
			while (!(mask & 1u)) {
				mask >>= 1;
				++shift;
			}
			return shift;
		}

		// Ramène un canal masqué sur 8 bits (les masques usuels sont déjà 8 bits).
		inline uint8 NkMaskedChannel(uint32 px, uint32 mask, uint32 shift) {
			if (!mask)
				return 0;
			uint32 v = (px & mask) >> shift;
			uint32 span = mask >> shift; // ex. 0xFF
			if (span == 0)
				return 0;
			if (span != 0xFFu)
				v = (v * 255u) / span;
			return static_cast<uint8>(v);
		}
	} // namespace

	bool NkWindow::SetClipboardImage(const NkClipboardImage &image) {
		if (!image.IsValid())
			return false;
		if (!OpenClipboard(mData.mHwnd))
			return false;
		EmptyClipboard();

		const uint32 w = image.width, h = image.height;
		const SIZE_T pixelBytes = static_cast<SIZE_T>(w) * h * 4u;
		bool ok = false;

		// CF_DIBV5 — alpha réel via masques explicites.
		if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + pixelBytes)) {
			if (uint8 *dst = static_cast<uint8 *>(GlobalLock(hg))) {
				BITMAPV5HEADER hdr = {};
				hdr.bV5Size = sizeof(BITMAPV5HEADER);
				hdr.bV5Width = static_cast<LONG>(w);
				hdr.bV5Height = static_cast<LONG>(h); // positif = bottom-up
				hdr.bV5Planes = 1;
				hdr.bV5BitCount = 32;
				hdr.bV5Compression = BI_BITFIELDS;
				hdr.bV5RedMask = 0x00FF0000u;
				hdr.bV5GreenMask = 0x0000FF00u;
				hdr.bV5BlueMask = 0x000000FFu;
				hdr.bV5AlphaMask = 0xFF000000u;
				hdr.bV5CSType = 0x73524742; // 'sRGB'
				hdr.bV5Intent = LCS_GM_IMAGES;
				const uint8 *hdrBytes = reinterpret_cast<const uint8 *>(&hdr);
				for (usize i = 0; i < sizeof(hdr); ++i)
					dst[i] = hdrBytes[i];
				uint8 *px = dst + sizeof(hdr);
				for (uint32 y = 0; y < h; ++y) {
					const uint8 *src = image.pixels.Data() + static_cast<usize>(h - 1 - y) * w * 4u; // inversion bottom-up
					uint8 *row = px + static_cast<usize>(y) * w * 4u;
					for (uint32 x = 0; x < w; ++x) { // RGBA → BGRA
						row[x * 4 + 0] = src[x * 4 + 2];
						row[x * 4 + 1] = src[x * 4 + 1];
						row[x * 4 + 2] = src[x * 4 + 0];
						row[x * 4 + 3] = src[x * 4 + 3];
					}
				}
				GlobalUnlock(hg);
				if (SetClipboardData(CF_DIBV5, hg))
					ok = true;
				else
					GlobalFree(hg);
			} else {
				GlobalFree(hg);
			}
		}

		// CF_DIB 32 bpp BI_RGB — pour les consommateurs qui ignorent V5.
		if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixelBytes)) {
			if (uint8 *dst = static_cast<uint8 *>(GlobalLock(hg))) {
				BITMAPINFOHEADER hdr = {};
				hdr.biSize = sizeof(BITMAPINFOHEADER);
				hdr.biWidth = static_cast<LONG>(w);
				hdr.biHeight = static_cast<LONG>(h);
				hdr.biPlanes = 1;
				hdr.biBitCount = 32;
				hdr.biCompression = BI_RGB;
				const uint8 *hdrBytes = reinterpret_cast<const uint8 *>(&hdr);
				for (usize i = 0; i < sizeof(hdr); ++i)
					dst[i] = hdrBytes[i];
				uint8 *px = dst + sizeof(hdr);
				for (uint32 y = 0; y < h; ++y) {
					const uint8 *src = image.pixels.Data() + static_cast<usize>(h - 1 - y) * w * 4u;
					uint8 *row = px + static_cast<usize>(y) * w * 4u;
					for (uint32 x = 0; x < w; ++x) {
						row[x * 4 + 0] = src[x * 4 + 2];
						row[x * 4 + 1] = src[x * 4 + 1];
						row[x * 4 + 2] = src[x * 4 + 0];
						row[x * 4 + 3] = src[x * 4 + 3];
					}
				}
				GlobalUnlock(hg);
				if (SetClipboardData(CF_DIB, hg))
					ok = true;
				else
					GlobalFree(hg);
			} else {
				GlobalFree(hg);
			}
		}

		CloseClipboard();
		return ok;
	}

	namespace {
		// Décode un bloc DIB (BITMAPINFOHEADER/V4/V5, 24 ou 32 bpp, BI_RGB ou
		// BI_BITFIELDS) vers RGBA8 top-down. Partagé par CF_DIBV5 et CF_DIB.
		bool NkDecodeDibToRgba(const uint8 *dib, SIZE_T dibSize, NkClipboardImage &out) {
			if (!dib || dibSize < sizeof(BITMAPINFOHEADER))
				return false;
			const BITMAPINFOHEADER *bih = reinterpret_cast<const BITMAPINFOHEADER *>(dib);
			const uint32 hdrSize = bih->biSize;
			if (hdrSize < sizeof(BITMAPINFOHEADER) || hdrSize > dibSize)
				return false;
			const uint32 bpp = bih->biBitCount;
			const uint32 comp = bih->biCompression;
			if ((bpp != 24 && bpp != 32) || (comp != BI_RGB && comp != BI_BITFIELDS))
				return false; // paletisés/compressés : hors périmètre (sources image réelles = 24/32 bpp)
			const LONG rawH = bih->biHeight;
			const bool bottomUp = rawH > 0; // convention DIB : hauteur positive = lignes bas → haut
			const uint32 w = static_cast<uint32>(bih->biWidth);
			const uint32 h = static_cast<uint32>(bottomUp ? rawH : -rawH);
			if (w == 0 || h == 0 || w > 32768u || h > 32768u)
				return false;

			// Masques : dans l'en-tête à partir de V4/V5, sinon 3 DWORD APRÈS
			// un BITMAPINFOHEADER en BI_BITFIELDS (convention CF_DIB).
			uint32 maskR = 0x00FF0000u, maskG = 0x0000FF00u, maskB = 0x000000FFu, maskA = 0;
			SIZE_T pixelOffset = hdrSize;
			if (comp == BI_BITFIELDS) {
				if (hdrSize >= 108) { // BITMAPV4HEADER et au-delà
					const BITMAPV5HEADER *v5 = reinterpret_cast<const BITMAPV5HEADER *>(dib);
					maskR = v5->bV5RedMask;
					maskG = v5->bV5GreenMask;
					maskB = v5->bV5BlueMask;
					maskA = v5->bV5AlphaMask;
				} else {
					if (dibSize < hdrSize + 12)
						return false;
					const uint32 *m = reinterpret_cast<const uint32 *>(dib + hdrSize);
					maskR = m[0];
					maskG = m[1];
					maskB = m[2];
					pixelOffset += 12;
				}
			} else if (bpp == 32 && hdrSize >= 108) {
				// V4/V5 en BI_RGB : l'alpha peut quand même être déclaré.
				maskA = reinterpret_cast<const BITMAPV5HEADER *>(dib)->bV5AlphaMask;
			}
			if (bpp == 32 && comp == BI_RGB)
				maskA = maskA ? maskA : 0xFF000000u; // BGRX : on lira l'octet, heuristique alpha plus bas

			const uint32 stride = NkDibStride(w, bpp);
			if (pixelOffset + static_cast<SIZE_T>(stride) * h > dibSize)
				return false;
			const uint8 *px = dib + pixelOffset;

			out.width = w;
			out.height = h;
			out.pixels.Resize(static_cast<usize>(w) * h * 4u);
			const uint32 shR = NkMaskShift(maskR), shG = NkMaskShift(maskG), shB = NkMaskShift(maskB),
						 shA = NkMaskShift(maskA);
			bool anyAlpha = false;
			for (uint32 y = 0; y < h; ++y) {
				const uint8 *row = px + static_cast<usize>(bottomUp ? (h - 1 - y) : y) * stride;
				uint8 *dst = out.pixels.Data() + static_cast<usize>(y) * w * 4u;
				if (bpp == 24) { // BGR → RGBA opaque
					for (uint32 x = 0; x < w; ++x) {
						dst[x * 4 + 0] = row[x * 3 + 2];
						dst[x * 4 + 1] = row[x * 3 + 1];
						dst[x * 4 + 2] = row[x * 3 + 0];
						dst[x * 4 + 3] = 255;
					}
				} else {
					for (uint32 x = 0; x < w; ++x) {
						uint32 v;
						const uint8 *s = row + x * 4;
						v = static_cast<uint32>(s[0]) | (static_cast<uint32>(s[1]) << 8) |
							(static_cast<uint32>(s[2]) << 16) | (static_cast<uint32>(s[3]) << 24);
						const uint8 a = maskA ? NkMaskedChannel(v, maskA, shA) : 255;
						dst[x * 4 + 0] = NkMaskedChannel(v, maskR, shR);
						dst[x * 4 + 1] = NkMaskedChannel(v, maskG, shG);
						dst[x * 4 + 2] = NkMaskedChannel(v, maskB, shB);
						dst[x * 4 + 3] = a;
						if (a != 0)
							anyAlpha = true;
					}
				}
			}
			// Heuristique alpha : beaucoup de producteurs posent du 32 bpp avec
			// TOUS les alphas à 0 en voulant dire « opaque » (BGRX). Une image
			// intégralement invisible n'a aucun sens pour un coller.
			if (bpp == 32 && !anyAlpha) {
				for (usize i = 3; i < out.pixels.Size(); i += 4)
					out.pixels[i] = 255;
			}
			return true;
		}
	} // namespace

	bool NkWindow::GetClipboardImage(NkClipboardImage &out) const {
		out = NkClipboardImage{};
		if (!OpenClipboard(mData.mHwnd))
			return false;
		bool ok = false;
		// V5 d'abord (alpha fiable), sinon DIB — synthétisé depuis CF_BITMAP au besoin.
		const UINT formats[2] = {CF_DIBV5, CF_DIB};
		for (int i = 0; i < 2 && !ok; ++i) {
			HANDLE h = GetClipboardData(formats[i]);
			if (!h)
				continue;
			const SIZE_T size = GlobalSize(h);
			if (const uint8 *dib = static_cast<const uint8 *>(GlobalLock(h))) {
				ok = NkDecodeDibToRgba(dib, size, out);
				GlobalUnlock(h);
			}
		}
		CloseClipboard();
		if (!ok)
			out = NkClipboardImage{};
		return ok;
	}

	bool NkWindow::HasClipboardImage() const {
		// Sans OpenClipboard : test léger. CF_DIB couvre aussi CF_BITMAP (synthèse OS).
		return IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_DIB) ||
			   IsClipboardFormatAvailable(CF_BITMAP);
	}

	void NkWindow::BeginResize(NkResizeEdge edge) {
		if (!mData.mHwnd)
			return;
		// Une fenetre declaree non redimensionnable ne se redimensionne pas non plus
		// par la porte de service : `BeginResize` est le hand-off natif qu'une
		// decoration personnalisee appelle depuis ses bords. Le refus est NOMME —
		// une decoration qui croit encore gerer le redimensionnement doit l'apprendre.
		if (!mConfig.resizable) {
			NkWindowRefuserUneFois(NkWindowProp::Resizable, "Win32",
								   "BeginResize appele sur une fenetre creee resizable=false : ignore");
			return;
		}
		WPARAM ht = HTCAPTION;
		switch (edge) {
			case NkResizeEdge::Left:
				ht = HTLEFT;
				break;
			case NkResizeEdge::Right:
				ht = HTRIGHT;
				break;
			case NkResizeEdge::Top:
				ht = HTTOP;
				break;
			case NkResizeEdge::Bottom:
				ht = HTBOTTOM;
				break;
			case NkResizeEdge::TopLeft:
				ht = HTTOPLEFT;
				break;
			case NkResizeEdge::TopRight:
				ht = HTTOPRIGHT;
				break;
			case NkResizeEdge::BottomLeft:
				ht = HTBOTTOMLEFT;
				break;
			case NkResizeEdge::BottomRight:
				ht = HTBOTTOMRIGHT;
				break;
		}
		ReleaseCapture();
		SendMessageW(mData.mHwnd, WM_NCLBUTTONDOWN, ht, 0);
	}

	void NkWindow::SetFullscreen(bool fs) {
		mConfig.fullscreen = fs;

		if (fs) {
			SetWindowLongW(mData.mHwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
			SetWindowPos(mData.mHwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
						 SWP_FRAMECHANGED);

			// Mettre à jour la taille
			mConfig.width = GetSystemMetrics(SM_CXSCREEN);
			mConfig.height = GetSystemMetrics(SM_CYSCREEN);
		} else {
			SetWindowLongW(mData.mHwnd, GWL_STYLE, (LONG)mData.mDwStyle);
			SetWindowPos(mData.mHwnd, nullptr, mConfig.x, mConfig.y, (int)mConfig.width, (int)mConfig.height,
						 SWP_FRAMECHANGED | SWP_NOZORDER);
		}
	}

	// =============================================================================
	// Mouse
	// =============================================================================

	// ── COORDONNEES CLIENT -> ECRAN, PAR L'API QUI SAIT ─────────────────────
	// `ClientToScreen` fait la conversion ; on ne la REFAIT PAS a la main a
	// partir de `GetWindowRect`, qui rend le coin du CADRE et non celui de la
	// zone client -- juste sur une fenetre sans cadre, faux des qu'on en met un.
	//
	// ⚠ ON BORNE. Une conversion fausse enverrait le curseur de l'utilisateur
	// n'importe ou sur son ecran, et rien ne le lui dirait. On refuse donc une
	// destination qui tombe HORS de la zone client : le pire cas devient « rien
	// ne bouge et le journal le dit », au lieu de « le curseur a saute ».
	//
	// ⚠ CE QUI N'EST PAS TRAITE ICI, ET C'EST NOMME PLUTOT QUE TU : quand le
	// curseur sort par un bord qui est AUSSI le bord de la fenetre et qu'aucun
	// bouton n'est tenu (une modale lancee au clavier), Windows cesse d'envoyer
	// des `WM_MOUSEMOVE` -- la position hors bornes n'est alors JAMAIS vue, et
	// le rebouclage ne se declenche pas. La parade identifiee est d'appeler
	// `ClipMouseToClient(true)` pendant la modale (elle existe deja, juste en
	// dessous) et de la relacher a la sortie. Elle n'est PAS posee ici parce
	// qu'elle change le curseur physique de l'utilisateur d'une facon qui se
	// juge a l'oeil et non au chiffre : c'est une decision, pas une correction.
	// CONDITION DE RETRAIT DE CE COMMENTAIRE : le jour ou la modale prend et
	// relache le clip, ce paragraphe disparait avec lui.
	bool NkWindow::SetMousePositionClient(int32 x, int32 y) {
		if (!mData.mHwnd) {
			NkLog::Instance().Warnf("[NkWindow] SetMousePositionClient refuse : aucune fenetre native.");
			return false;
		}
		RECT cr;
		if (!::GetClientRect(mData.mHwnd, &cr)) {
			NkLog::Instance().Warnf("[NkWindow] SetMousePositionClient refuse : GetClientRect a echoue.");
			return false;
		}
		if (x < cr.left || y < cr.top || x >= cr.right || y >= cr.bottom) {
			NkLog::Instance().Warnf("[NkWindow] SetMousePositionClient refuse : (%d, %d) est hors de la "
									"zone client (%d x %d). Rien n'a bouge.",
									(int)x, (int)y, (int)(cr.right - cr.left), (int)(cr.bottom - cr.top));
			return false;
		}
		POINT pt = {(LONG)x, (LONG)y};
		if (!::ClientToScreen(mData.mHwnd, &pt)) {
			NkLog::Instance().Warnf("[NkWindow] SetMousePositionClient refuse : ClientToScreen a echoue.");
			return false;
		}
		if (!::SetCursorPos(pt.x, pt.y)) {
			NkLog::Instance().Warnf("[NkWindow] SetMousePositionClient : SetCursorPos a echoue (%d, %d).",
									(int)pt.x, (int)pt.y);
			return false;
		}
		return true;
	}

	void NkWindow::SetMousePosition(uint32 x, uint32 y) {
		SetCursorPos((int)x, (int)y);
	}

	void NkWindow::ShowMouse(bool show) {
		ShowCursor(show ? TRUE : FALSE);
	}

	void NkWindow::CaptureMouse(bool cap) {
		if (cap)
			SetCapture(mData.mHwnd);
		else
			ReleaseCapture();
	}

	void NkWindow::ClipMouseToClient(bool clip) {
		if (!mData.mHwnd)
			return;
		if (clip) {
			RECT cr;
			if (!::GetClientRect(mData.mHwnd, &cr))
				return;
			POINT lt = {cr.left, cr.top};
			POINT rb = {cr.right, cr.bottom};
			::ClientToScreen(mData.mHwnd, &lt);
			::ClientToScreen(mData.mHwnd, &rb);
			RECT clipR = {lt.x, lt.y, rb.x, rb.y};
			::ClipCursor(&clipR);
		} else {
			::ClipCursor(nullptr);
		}
	}

	// =============================================================================
	// OS extras
	// =============================================================================

	void NkWindow::SetProgress(float progress) {
		if (mData.mTaskbarList) {
			const uint32 kMax = 10000;
			mData.mTaskbarList->SetProgressValue(mData.mHwnd, (ULONGLONG)(progress * kMax), kMax);
		}
	}

	// Desktop Windows : clavier physique, pas de clavier logiciel. No-op.
	void NkWindow::ShowSoftKeyboard(const NkSoftKeyboardConfig &) {
	}

	void NkWindow::HideSoftKeyboard() {
	}

	bool NkWindow::IsSoftKeyboardVisible() const {
		return false;
	}

	// =============================================================================
	// Surface
	// =============================================================================

	NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
		NkSurfaceDesc sd;
		auto sz = GetSize(); // GetSize synchronise déjà mConfig
		sd.width = sz.x;
		sd.height = sz.y;
		sd.dpi = GetDpiScale();
		sd.hwnd = mData.mHwnd;
		sd.hinstance = mData.mHInstance;
		sd.appliedHints = mData.mAppliedHints;
		return sd;
	}

	// =============================================================================
	// Safe Area (desktop = zero insets)
	// =============================================================================

	NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
		return {};
	}

	// =============================================================================
	// Orientation (N/A on Win32)
	// =============================================================================

	bool NkWindow::SupportsOrientationControl() const {
		return false;
	}

	void NkWindow::SetScreenOrientation(NkScreenOrientation) {
		// Un bureau Windows ne tourne pas sur commande d'une application. Le corps
		// etait VIDE : l'appelant croyait avoir agi.
		NkWindowRefuserUneFois(NkWindowProp::ScreenOrientation, "Win32",
							   "un bureau Windows n'a pas d'orientation pilotable par l'application");
	}

	NkScreenOrientation NkWindow::GetScreenOrientation() const {
		return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
	}

	void NkWindow::SetAutoRotateEnabled(bool) {
		NkWindowRefuserUneFois(NkWindowProp::LockOrientation, "Win32",
							   "la rotation automatique n'existe pas sur un bureau Windows");
	}

	bool NkWindow::IsAutoRotateEnabled() const {
		return false;
	}

	// =============================================================================
	// Web input (N/A on Win32)
	// =============================================================================

	void NkWindow::SetWebInputOptions(const NkWebInputOptions &) {
	}

	NkWebInputOptions NkWindow::GetWebInputOptions() const {
		return {};
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX