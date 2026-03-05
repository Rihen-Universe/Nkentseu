#pragma once

// =============================================================================
// NkSurface.h
// Descripteur de surface graphique natif par plateforme.
//
// NkSurfaceDesc contient tous les handles natifs nÃ©cessaires Ã  un backend
// graphique (Vulkan, Metal, DirectX, OpenGL, Software) pour crÃ©er ses
// propres ressources de rendu.
// =============================================================================

#include "NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"

// Inclusions conditionnelles des headers natifs
#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)

#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
// UWP/Xbox: generic native window handle (CoreWindow/GameCore equivalent).

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#elif defined(NKENTSEU_PLATFORM_MACOS)
#ifdef __OBJC__
@class NSView;
@class CAMetalLayer;
#else
using NSView = struct objc_object;
using CAMetalLayer = struct objc_object;
#endif

#elif defined(NKENTSEU_PLATFORM_IOS)
#ifdef __OBJC__
@class UIView;
@class CAMetalLayer;
#else
using UIView = struct objc_object;
using CAMetalLayer = struct objc_object;
#endif

#elif defined(NKENTSEU_WINDOWING_XCB)
#include <xcb/xcb.h>

#elif defined(NKENTSEU_WINDOWING_XLIB)
#include <X11/Xlib.h>

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include <android/native_window.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkSurfaceDesc - handles natifs de la surface de rendu
	// ---------------------------------------------------------------------------

	struct NkSurfaceDesc {
		NkU32 width = 0;  ///< Largeur en pixels physiques
		NkU32 height = 0; ///< Hauteur en pixels physiques
		float dpi = 1.f;  ///< Facteur de mise Ã  l'echelle DPI

		#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
			void *dummy = nullptr; ///< Stub explicite en mode Noop forcé

		#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
			void *nativeWindow = nullptr; ///< Handle natif UWP/Xbox (opaque)

		#elif defined(NKENTSEU_PLATFORM_WINDOWS)
			HWND hwnd = nullptr;		   ///< Handle natif Win32
			HINSTANCE hinstance = nullptr; ///< Instance de l'application

		#elif defined(NKENTSEU_PLATFORM_MACOS)
			NSView *view = nullptr;				///< Vue Cocoa (NSView)
			CAMetalLayer *metalLayer = nullptr; ///< Couche Metal

		#elif defined(NKENTSEU_PLATFORM_IOS)
			UIView *view = nullptr;				///< Vue UIKit
			CAMetalLayer *metalLayer = nullptr; ///< Couche Metal

		#elif defined(NKENTSEU_WINDOWING_XCB)
			xcb_connection_t *connection = nullptr; ///< Connexion XCB
			xcb_window_t window = 0;				///< Identifiant de fenêtre XCB

		#elif defined(NKENTSEU_WINDOWING_XLIB)
			Display *display = nullptr; ///< Connexion Xlib
			::Window window = 0;		///< Identifiant de fenÃªtre Xlib

		#elif defined(NKENTSEU_PLATFORM_ANDROID)
			ANativeWindow *nativeWindow = nullptr; ///< ANativeWindow Android

		#elif defined(NKENTSEU_PLATFORM_WEB) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
			const char *canvasId = "#canvas"; ///< ID du canvas HTML

		#else
			void *dummy = nullptr; ///< Stub pour plateforme inconnue / Noop
		#endif
	};

} // namespace nkentseu
