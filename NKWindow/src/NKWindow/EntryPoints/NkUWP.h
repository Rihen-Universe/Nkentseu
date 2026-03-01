#pragma once
// =============================================================================
// NkUWP.h â€” Point d'entrÃ©e UWP / Xbox
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <string>

#include "../Core/NkEntry.h"

#pragma comment(lib, "shell32.lib")

#if defined(__XBOX__) || defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX_XBOXONE)
#define NK_APP_NAME "xbox_app"
#else
#define NK_APP_NAME "uwp_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
	int argc = 0;
	LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<std::string> args;
	for (int i = 0; i < argc; ++i) {
		int sz = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
		std::string s(static_cast<std::size_t>(sz), 0);
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), sz, nullptr, nullptr);
		if (!s.empty() && s.back() == '\0')
			s.pop_back();
		args.push_back(std::move(s));
	}
	LocalFree(wargv);

	nkentseu::NkEntryState state(hInst, nullptr, nullptr, 1, args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	return result;
}

