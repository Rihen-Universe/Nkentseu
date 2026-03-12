#pragma once

// =============================================================================
// NkWayland.h
// Point d'entrée Linux Wayland.
//
// Note : la connexion au compositeur (wl_display_connect) est établie
// par fenêtre dans NkWaylandWindow.cpp — pas au niveau du point d'entrée.
// Ce fichier vérifie seulement que WAYLAND_DISPLAY est définie et appelle nkmain().
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

#if defined(NKENTSEU_WINDOWING_WAYLAND)

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

#ifndef NK_APP_NAME
#define NK_APP_NAME "wayland_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

namespace {

bool NkWaylandSocketExists(const char* runtimeDir, const char* displayName) {
	if (!runtimeDir || !runtimeDir[0] || !displayName || !displayName[0]) {
		return false;
	}

	char socketPath[1024];
	const int written = ::snprintf(socketPath, sizeof(socketPath), "%s/%s", runtimeDir, displayName);
	if (written <= 0 || written >= static_cast<int>(sizeof(socketPath))) {
		return false;
	}
	return ::access(socketPath, F_OK) == 0;
}

void NkWaylandMaybeFixWslRuntimeDir(const char* displayName) {
	const char* runtimeDir = ::getenv("XDG_RUNTIME_DIR");
	if (NkWaylandSocketExists(runtimeDir, displayName)) {
		return;
	}

	constexpr const char* kWslgRuntimeDir = "/mnt/wslg/runtime-dir";
	if (NkWaylandSocketExists(kWslgRuntimeDir, displayName)) {
		::setenv("XDG_RUNTIME_DIR", kWslgRuntimeDir, 1);
		logger.Warnf("[NkWindow][Wayland] XDG_RUNTIME_DIR invalide. Fallback WSLg -> %s", kWslgRuntimeDir);
	}
}

} // namespace

int main(int argc, char *argv[]) {
	// Vérification/réparation de l'environnement Wayland.
	const char *waylandDisplay = ::getenv("WAYLAND_DISPLAY");
	if (!waylandDisplay || !*waylandDisplay) {
		constexpr const char* kDefaultWaylandDisplay = "wayland-0";
		::setenv("WAYLAND_DISPLAY", kDefaultWaylandDisplay, 0);
		waylandDisplay = ::getenv("WAYLAND_DISPLAY");
	}
	NkWaylandMaybeFixWslRuntimeDir(waylandDisplay);

	const char* runtimeDir = ::getenv("XDG_RUNTIME_DIR");
	if (!waylandDisplay || !*waylandDisplay || !runtimeDir || !*runtimeDir || !NkWaylandSocketExists(runtimeDir, waylandDisplay)) {
		logger.Errorf("[NkWindow][Wayland] Environnement Wayland invalide (WAYLAND_DISPLAY='%s', XDG_RUNTIME_DIR='%s').",
			waylandDisplay ? waylandDisplay : "",
			runtimeDir ? runtimeDir : "");
		return 1;
	}

	nkentseu::NkVector<nkentseu::NkString> args;
	for (int i = 0; i < argc; ++i)
		args.PushBack(nkentseu::NkString(argv[i]));

	if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
		return -1;
	}

	nkentseu::NkEntryState state(args);
	nkentseu::NkApplyEntryAppName(state, NK_APP_NAME);
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	nkentseu::NkEntryRuntimeShutdown(true);
	return result;
}

#endif // NKENTSEU_WINDOWING_WAYLAND
