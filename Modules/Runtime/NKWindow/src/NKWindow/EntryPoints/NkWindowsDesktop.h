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

#include "NKWindow/Core/NkEntry.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTraits.h"

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

	nkentseu::NkVector<nkentseu::NkString> args;
	args.Reserve(static_cast<nkentseu::usize>(argc));
	for (int i = 0; i < argc; ++i) {
		int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
		nkentseu::NkString s(static_cast<nkentseu::usize>(sz), 0);
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.Data(), sz, nullptr, nullptr);
		if (!s.Empty() && s.Back() == '\0')
			s.PopBack();
		args.PushBack(nkentseu::traits::NkMove(s));
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

#if defined(_DEBUG) || defined(NKENTSEU_DEBUG_CONSOLE)
	fflush(stdout);
	fflush(stderr);
	FreeConsole();
#endif

	return result;
}

