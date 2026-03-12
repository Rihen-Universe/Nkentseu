#pragma once

// =============================================================================
// NkXLib.h
// Point d'entrée Linux Xlib.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

#if defined(NKENTSEU_WINDOWING_XLIB)

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

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
	nkentseu::NkVector<nkentseu::NkString> args;
	for (int i = 0; i < argc; ++i)
		args.PushBack(nkentseu::NkString(argv[i]));

	if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
		return -1;
	}

	// Display is opened lazily by NkWindow::Create() in NkXLibWindow.cpp
	nkentseu::NkEntryState state(nullptr, args);
	nkentseu::NkApplyEntryAppName(state, NK_APP_NAME);
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	nkentseu::NkEntryRuntimeShutdown(true);
	return result;
}

#endif // NKENTSEU_WINDOWING_XLIB
