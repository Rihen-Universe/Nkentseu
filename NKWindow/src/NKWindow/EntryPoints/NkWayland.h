#pragma once

// =============================================================================
// NkWayland.h
// Point d'entrée Linux Wayland.
//
// Note : la connexion au compositeur (wl_display_connect) est établie
// par fenêtre dans NkWaylandWindowImpl::Create() — pas au niveau du
// point d'entrée. Ce fichier vérifie seulement que WAYLAND_DISPLAY est
// définie et appelle nkmain().
// =============================================================================

#include "../Core/NkEntry.h"
#include "../Platform/Wayland/NkWaylandWindowImpl.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#ifndef NK_APP_NAME
#define NK_APP_NAME "wayland_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int main(int argc, char *argv[]) {
	// Vérification de l'environnement Wayland
	const char *waylandDisplay = std::getenv("WAYLAND_DISPLAY");
	if (!waylandDisplay || !*waylandDisplay) {
		std::fprintf(stderr,
		             "[NkWindow][Wayland] WAYLAND_DISPLAY n'est pas défini. "
		             "Un compositeur Wayland est-il en cours d'exécution?\n");
		return 1;
	}

	std::vector<std::string> args(argv, argv + argc);

	nkentseu::NkEntryState state(args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}
