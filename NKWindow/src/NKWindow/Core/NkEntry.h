#pragma once

// =============================================================================
// NkEntry.h
// NkEntryState â€” conteneur des arguments de dÃ©marrage par plateforme.
// La variable globale gState est crÃ©Ã©e par le point d'entrÃ©e de plateforme
// et dÃ©truite aprÃ¨s le retour de nkmain().
// =============================================================================

#include "NkPlatformDetect.h"
#include <string>
#include <vector>

// Inclusions conditionnelles des types natifs
#if defined(NKENTSEU_FAMILY_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(NKENTSEU_PLATFORM_XCB)
#include <xcb/xcb.h>
#elif defined(NKENTSEU_PLATFORM_XLIB)
#include <X11/Xlib.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// NkEntryState
// ---------------------------------------------------------------------------

struct NkEntryState {
	// --- Arguments communs Ã  toutes les plateformes ---
	std::string appName;
	std::vector<std::string> args;

	// --- Handles natifs optionnels (nullptr / 0 sur les autres plateformes) ---

#if defined(NKENTSEU_FAMILY_WINDOWS)
	HINSTANCE hInstance = nullptr;
	HINSTANCE hPrevInstance = nullptr;
	LPSTR lpCmdLine = nullptr;
	int nCmdShow = 1;

	NkEntryState(HINSTANCE hi, HINSTANCE hpi, LPSTR cmd, int ncmd, const std::vector<std::string> &a)
		: hInstance(hi), hPrevInstance(hpi), lpCmdLine(cmd), nCmdShow(ncmd), args(std::move(a)) {
	}

#elif defined(NKENTSEU_PLATFORM_XCB)
	xcb_connection_t *connection = nullptr;
	xcb_screen_t *screen = nullptr;

	NkEntryState(xcb_connection_t *c, xcb_screen_t *s, const std::vector<std::string> &a)
		: connection(c), screen(s), args(std::move(a)) {
	}

#elif defined(NKENTSEU_PLATFORM_XLIB)
	Display *display = nullptr;

	explicit NkEntryState(Display *d, const std::vector<std::string> &a) : display(d), args(std::move(a)) {
	}

#elif defined(NKENTSEU_PLATFORM_ANDROID)
	android_app *androidApp = nullptr;

	explicit NkEntryState(android_app *app, const std::vector<std::string> &a) : androidApp(app), args(std::move(a)) {
	}

#elif defined(NKENTSEU_PLATFORM_WAYLAND)
	// Wayland : wl_display est créé par fenêtre dans NkWaylandWindowImpl::Create()
	// — aucun handle global à transmettre ici.
	NkEntryState() = default;
	explicit NkEntryState(const std::vector<std::string> &a) : args(std::move(a)) {
	}

#else
	NkEntryState() = default;
	explicit NkEntryState(const std::vector<std::string> &a) : args(std::move(a)) {
	}
#endif

	// Accesseurs gÃ©nÃ©riques
	const std::vector<std::string> &GetArgs() const {
		return args;
	}
	const std::string &GetAppName() const {
		return appName;
	}
};

// ---------------------------------------------------------------------------
// Variable globale (dÃ©finie dans chaque entry point de plateforme)
// ---------------------------------------------------------------------------

extern NkEntryState *gState;

} // namespace nkentseu

// ---------------------------------------------------------------------------
// Prototype de la fonction utilisateur Ã  implÃ©menter
// ---------------------------------------------------------------------------

int nkmain(const nkentseu::NkEntryState &state);
