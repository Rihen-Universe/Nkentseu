#pragma once

// =============================================================================
// NkXCB.h
// Point d'entrée Linux XCB.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

#if defined(NKENTSEU_WINDOWING_XCB)

#include <xcb/xcb.h>
#include <vector>
#include <string>
#include <cstdlib>

#ifndef NK_APP_NAME
#define NK_APP_NAME "xcb_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int main(int argc, char *argv[]) {
	// --- Connexion XCB ---
	int screenNum = 0;
	xcb_connection_t *conn = xcb_connect(nullptr, &screenNum);
	if (xcb_connection_has_error(conn)) {
		xcb_disconnect(conn);
		return 1;
	}

	const xcb_setup_t *setup = xcb_get_setup(conn);
	xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
	for (int i = 0; i < screenNum; ++i, xcb_screen_next(&it)) {
	}

	// Connection is passed via NkEntryState; NkXCBWindow.cpp manages it from there
	std::vector<std::string> args(argv, argv + argc);

	nkentseu::NkEntryState state(conn, it.data, args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	xcb_disconnect(conn);
	return result;
}

#endif // NKENTSEU_WINDOWING_XCB
