#pragma once

// =============================================================================
// NkXLib.h
// Point d'entrée Linux Xlib.
// =============================================================================

#include "../Core/NkEntry.h"
#include "../Platform/XLib/NkXLibWindowImpl.h"
#include <X11/Xlib.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#ifndef NK_APP_NAME
#define NK_APP_NAME "xlib_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int main(int argc, char *argv[]) {
	XInitThreads();

	const char *displayName = std::getenv("DISPLAY");
	Display *display = XOpenDisplay((displayName && *displayName) ? displayName : nullptr);
	if (!display) {
		std::fprintf(stderr,
					 "[NkWindow][XLIB] Unable to open X display. DISPLAY='%s'. "
					 "Enable WSLg/X11 server (or run headless backend).\n",
					 displayName ? displayName : "(null)");
		return 1;
	}

	nkentseu::nk_xlib_global_display = display;

	std::vector<std::string> args(argv, argv + argc);

	nkentseu::NkEntryState state(display, args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	nkentseu::nk_xlib_global_display = nullptr;

	XCloseDisplay(display);
	return result;
}
