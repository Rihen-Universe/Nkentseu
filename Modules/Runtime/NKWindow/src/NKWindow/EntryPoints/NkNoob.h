#pragma once
// =============================================================================
// NkNoob.h — Point d'entrée générique (headless, tests, inconnu)
// =============================================================================

#include "NKWindow/Core/NkEntry.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

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
	nkentseu::NkVector<nkentseu::NkString> args;
	for (int i = 0; i < argc; ++i)
		args.PushBack(nkentseu::NkString(argv[i]));

	nkentseu::NkEntryState state(args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}
