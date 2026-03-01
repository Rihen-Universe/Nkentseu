#pragma once
// =============================================================================
// NkNoob.h — Point d'entrée générique (headless, tests, inconnu)
// =============================================================================

#include "../Core/NkEntry.h"
#include <vector>
#include <string>

#ifndef NK_APP_NAME
#define NK_APP_NAME "noop_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int main(int argc, char *argv[]) {
	std::vector<std::string> args(argv, argv + argc);

	nkentseu::NkEntryState state(args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}
