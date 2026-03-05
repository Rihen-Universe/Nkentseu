#pragma once

// =============================================================================
// NkXLib.h
// Point d'entrée Linux Xlib.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

#if defined(NKENTSEU_WINDOWING_XLIB)

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
	std::vector<std::string> args(argv, argv + argc);

	// Display is opened lazily by NkWindow::Create() in NkXLibWindow.cpp
	nkentseu::NkEntryState state(nullptr, args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}

#endif // NKENTSEU_WINDOWING_XLIB
