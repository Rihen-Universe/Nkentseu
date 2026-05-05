#pragma once
// =============================================================================
// NkWindowConfig.h
// Configuration de création de fenêtre.
//
// Changement par rapport à la version précédente :
//   → Ajout du champ `surfaceHints` (NkSurfaceHints).
//     Ce champ est optionnel. Laisser vide pour Vulkan, DirectX, Metal,
//     Software, EGL. Rempli automatiquement par
//     NkGraphicsContextFactory::PrepareWindowConfig() pour OpenGL/GLX.
// =============================================================================

#include "NkTypes.h"
#include "NkSafeArea.h"
#include "NkSurfaceHint.h"   // ← seul ajout d'include

namespace nkentseu {

	enum class NkScreenOrientation : uint32 {
		NK_SCREEN_ORIENTATION_AUTO = 0,
		NK_SCREEN_ORIENTATION_PORTRAIT,
		NK_SCREEN_ORIENTATION_LANDSCAPE,
	};

	struct NkWebInputOptions {
		bool captureKeyboard        = true;
		bool allowBrowserShortcuts  = true;
		bool captureMouseMove       = true;
		bool captureMouseLeft       = true;
		bool captureMouseMiddle     = true;
		bool captureMouseRight      = false;
		bool captureMouseWheel      = true;
		bool captureTouch           = true;
		bool preventContextMenu     = false;
	};

	// -------------------------------------------------------------------------
	// NkNativeWindowOptions
	// Options natives avancées (cross-backend via handles opaques).
	//
	// Interprétation des handles:
	//   - Windows : externalWindowHandle/parentWindowHandle = HWND
	//   - XLib    : externalWindowHandle/parentWindowHandle = ::Window
	//   - XCB     : externalWindowHandle/parentWindowHandle = xcb_window_t
	//   - Wayland : externalWindowHandle = non supporte, parentWindowHandle = xdg_toplevel*
	//   - macOS   : externalWindowHandle/parentWindowHandle = NSWindow*
	//   - iOS     : externalWindowHandle = UIWindow*, parentWindowHandle = UIView*
	//   - Android : externalWindowHandle = ANativeWindow*
	//   - Web     : externalWindowHandle = const char* (canvas selector, ex: "#canvas")
	//   - UWP/Xbox/Noop : externalWindowHandle = handle opaque
	//
	// externalDisplayHandle est utilisé uniquement quand nécessaire:
	//   - XLib : Display*
	//   - XCB  : xcb_connection_t*
	//   - Wayland : wl_display*
	//   - macOS : NSScreen*
	//   - iOS   : UIScreen* (création interne uniquement)
	//   - Android : android_app* (optionnel)
	//   - Web : const char* (canvas selector alternatif)
	// -------------------------------------------------------------------------
	struct NkNativeWindowOptions {
		bool    useExternalWindow = false;
		uintptr externalWindowHandle = 0;
		uintptr externalDisplayHandle = 0;

		uintptr parentWindowHandle = 0;
		bool    utilityWindow = false;

		// Win32 only: copy pixel format from this HWND before WGL context creation.
		uintptr win32PixelFormatShareWindowHandle = 0;
	};

	struct NkWindowConfig {
		// --- Position et taille ---
		int32 x         = 100;
		int32 y         = 100;
		uint32 width     = 1280;
		uint32 height    = 720;
		uint32 minWidth  = 160;
		uint32 minHeight = 90;
		uint32 maxWidth  = 0xFFFF;
		uint32 maxHeight = 0xFFFF;

		// --- Comportement ---
		bool centered       = true;
		bool resizable      = true;
		bool movable        = true;
		bool closable       = true;
		bool minimizable    = true;
		bool maximizable    = true;
		bool canFullscreen  = true;
		bool fullscreen     = false;
		bool modal          = false;
		bool vsync          = true;
		bool dropEnabled    = false;
		NkScreenOrientation screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

		// --- Apparence ---
		bool   frame       = true;
		bool   hasShadow   = true;
		bool   transparent = false;
		bool   visible     = true;
		uint32  bgColor     = 0x141414FF;

		// --- Identité ---
		NkString title    = "NkWindow";
		NkString name     = "NkApp";
		NkString iconPath;
		NkNativeWindowOptions native;

		// --- Mobile / Safe Area ---
		bool respectSafeArea = true;

		// --- Web / WASM ---
		NkWebInputOptions webInput;

		// ── Hints de surface ────────────────────────────────────────────────────
		// Remplis par NkGraphicsContextFactory::PrepareWindowConfig() AVANT Create().
		// NkWindow transmet ces hints au backend platform (XLib, XCB…)
		// sans les interpréter.
		// Pour la grande majorité des cas (Vulkan, Metal, DirectX, Software,
		// OpenGL/WGL, OpenGL/EGL) : laisser vide, ne rien faire.
		// Pour OpenGL/GLX sur Linux uniquement : appeler PrepareWindowConfig().
		NkSurfaceHints surfaceHints;   // ← seul ajout dans ce struct
	};

} // namespace nkentseu
