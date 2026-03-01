#pragma once

// =============================================================================
// NkWindowsDesktop.h
// Point d'entrÃ©e Win32 Desktop.
// Analyse CommandLineToArgvW, crÃ©e NkEntryState, appelle nkmain().
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include "../Core/NkEntry.h"

#pragma comment(lib, "shell32.lib")

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#if defined(_DEBUG) || defined(NKENTSEU_DEBUG_CONSOLE)
	AllocConsole();
	freopen_s(reinterpret_cast<FILE **>(stdout), "CONOUT$", "w", stdout);
	freopen_s(reinterpret_cast<FILE **>(stderr), "CONOUT$", "w", stderr);
#endif

	// --- RÃ©cupÃ©ration des arguments CLI (UTF-8) ---
	int argc = 0;
	LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<std::string> args;
	args.reserve(static_cast<std::size_t>(argc));
	for (int i = 0; i < argc; ++i) {
		int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
		std::string s(static_cast<std::size_t>(sz), 0);
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), sz, nullptr, nullptr);
		if (!s.empty() && s.back() == '\0')
			s.pop_back();
		args.push_back(std::move(s));
	}
	LocalFree(wargv);

#ifndef NK_APP_NAME
#define NK_APP_NAME "windows_app"
#endif

	nkentseu::NkEntryState state(hInstance, hPrevInstance, lpCmdLine, nCmdShow, args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}

