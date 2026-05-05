#pragma once
// =============================================================================
// NkSurface.h
// Descripteur de surface natif retourné par NkWindow::GetSurfaceDesc().
//
// Ce struct est le SEUL point de contact entre NkWindow et les backends
// graphiques. NkWindow ne sait pas qui le consomme.
//
// Suffisance par API :
//   OpenGL/WGL  (Windows)  : hwnd + hinstance          → GetDC(hwnd) dans le contexte
//   OpenGL/GLX  (XLib)     : display + window + screen + appliedHints(GlxFBConfigPtr)
//   OpenGL/GLX  (XCB)      : connection + window + screen (via Xlib-xcb) + appliedHints
//   OpenGL/EGL  (Wayland)  : display(wl) + surface(wl)
//   OpenGL/EGL  (Android)  : nativeWindow
//   OpenGL/WebGL(WASM)     : canvasId
//   Vulkan                 : tous les handles natifs suffisent (surface créée après)
//   Metal                  : view + metalLayer (déjà créée par NkCocoaWindow)
//   DirectX 11/12          : hwnd + hinstance
//   Software               : tous (pixel buffer géré par le contexte lui-même)
// =============================================================================

#include "NkTypes.h"
#include "NkSurfaceHint.h"
#include "NKPlatform/NkPlatformDetect.h"

// Inclusions natives conditionnelles — types de handles uniquement
#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
    // rien

#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
    // handle opaque — pas d'include système requis ici

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   ifdef __OBJC__
@class NSView;
@class CAMetalLayer;
#   else
    using NSView       = struct objc_object;
    using CAMetalLayer = struct objc_object;
#   endif

#elif defined(NKENTSEU_PLATFORM_IOS)
#   ifdef __OBJC__
@class UIView;
@class CAMetalLayer;
#   else
    using UIView       = struct objc_object;
    using CAMetalLayer = struct objc_object;
#   endif

#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <xcb/xcb.h>

#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <X11/Xlib.h>

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
	#include <wayland-client.h>
	#include <wayland-cursor.h>
	#include <xkbcommon/xkbcommon.h>

    struct wl_display;
    struct wl_surface;
	struct wl_buffer;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include <android/native_window.h>
#endif

namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkSurfaceDesc
	// ---------------------------------------------------------------------------
	struct NkSurfaceDesc {

		// --- Dimensions physiques (pixels) ---
		uint32  width  = 0;
		uint32  height = 0;
		float  dpi    = 1.f;

		// --- Handles natifs (conditionnel par plateforme) ---

	#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
		void* dummy = nullptr;

	#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
		void* nativeWindow = nullptr;

	#elif defined(NKENTSEU_PLATFORM_WINDOWS)
		HWND      hwnd      = nullptr;
		HINSTANCE hinstance = nullptr;
		// HDC délibérément absent : il est éphémère.
		// Le contexte GL/DX appelle GetDC(hwnd) lui-même.

	#elif defined(NKENTSEU_PLATFORM_MACOS)
		NSView*       view       = nullptr;
		CAMetalLayer* metalLayer = nullptr;

	#elif defined(NKENTSEU_PLATFORM_IOS)
		UIView*       view       = nullptr;
		CAMetalLayer* metalLayer = nullptr;

	#elif defined(NKENTSEU_WINDOWING_XCB)
		xcb_connection_t* connection = nullptr;
		xcb_window_t      window     = 0;
		xcb_screen_t*     screen     = nullptr;
		// Note : pour GLX sur XCB, le contexte OpenGL récupère le Display*
		// sous-jacent via XGetXCBConnection() — il n'a pas besoin qu'on le
		// transporte ici, il l'obtient depuis la connexion xcb.

	#elif defined(NKENTSEU_WINDOWING_XLIB)
		Display* display = nullptr;
		::Window window  = 0;
		int      screen  = 0;

	#elif defined(NKENTSEU_WINDOWING_WAYLAND)
		::wl_display* display   = nullptr;
		::wl_surface* surface   = nullptr;
		bool               waylandConfigured = false;
		// Software framebuffer (rempli par NkWaylandWindow::GetSurfaceDesc)
		void*              shmPixels = nullptr;
		::wl_buffer*  shmBuffer = nullptr;
		uint32_t           shmStride = 0;

	#elif defined(NKENTSEU_PLATFORM_ANDROID)
		ANativeWindow* nativeWindow = nullptr;

	#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		const char* canvasId = "#canvas";

	#else
		void* dummy = nullptr;
	#endif

		// --- Hints appliqués pendant la création de la fenêtre ---
		// Remplis par NkWindow à partir de NkWindowConfig::surfaceHints.
		// Le contexte OpenGL/GLX relit GlxFBConfigPtr ici pour créer
		// son contexte sans refaire glXChooseFBConfig.
		// Pour toutes les autres APIs : toujours vide.
		NkSurfaceHints appliedHints;

		// --- Helpers de validation (pratiques pour les assertions) ---
		bool IsValid() const {
	#if defined(NKENTSEU_PLATFORM_WINDOWS)
			return hwnd != nullptr && width > 0 && height > 0;
	#elif defined(NKENTSEU_WINDOWING_XLIB)
			return display != nullptr && window != 0 && width > 0;
	#elif defined(NKENTSEU_WINDOWING_XCB)
			return connection != nullptr && window != 0 && width > 0;
	#elif defined(NKENTSEU_WINDOWING_WAYLAND)
			return display != nullptr && surface != nullptr && width > 0;
	#elif defined(NKENTSEU_PLATFORM_ANDROID)
			return nativeWindow != nullptr && width > 0;
	#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
			return view != nullptr && width > 0;
	#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			return canvasId != nullptr && width > 0;
	#else
			return width > 0 && height > 0;
	#endif
		}
	};

} // namespace nkentseu
