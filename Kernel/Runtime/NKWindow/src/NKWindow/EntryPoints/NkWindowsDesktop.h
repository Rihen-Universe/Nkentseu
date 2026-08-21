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
	// ⚠️ NE PRENDRE UNE CONSOLE QUE SI L'APPELANT N'EN A PAS DONNE UNE.
	//
	//    Releve le 2026-08-21 : `AllocConsole()` REINITIALISE les handles standard
	//    du processus vers la console qu'elle vient de creer. Lance depuis un
	//    terminal, l'application VOLAIT donc la sortie standard de l'appelant :
	//    tout ce qu'elle imprimait partait dans une console jetable, detruite
	//    quelques lignes plus bas par `FreeConsole()`. Et une redirection
	//    `> fichier` etait purement et simplement court-circuitee.
	//
	//    Consequence concrete : `NKUIDesign --probe` et `--roundtrip` rendaient 0
	//    avec une sortie VIDE alors qu'ils avaient tout ecrit. Un banc de mesure
	//    dont personne ne voit le resultat ne sert qu'a celui qui l'a ecrit.
	//
	//    La console reste allouee dans le seul cas ou elle sert vraiment : un
	//    lancement sans sortie standard (double-clic depuis l'explorateur), ou
	//    `GetStdHandle` rend un handle nul ou invalide.
	const HANDLE nkInheritedOut = GetStdHandle(STD_OUTPUT_HANDLE);
	const bool nkConsoleAllocated = (!nkInheritedOut || nkInheritedOut == INVALID_HANDLE_VALUE);
	if (nkConsoleAllocated) {
		AllocConsole();
		freopen_s(reinterpret_cast<FILE **>(stdout), "CONOUT$", "w", stdout);
		freopen_s(reinterpret_cast<FILE **>(stderr), "CONOUT$", "w", stderr);
	}
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

	if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
		return -1;
	}

	nkentseu::NkEntryState state(hInstance, hPrevInstance, lpCmdLine, nCmdShow, args);
	nkentseu::NkApplyEntryAppName(state, NK_APP_NAME);
	nkentseu::gState = &state;

	int result = nkmain(state);

	nkentseu::gState = nullptr;
	nkentseu::NkEntryRuntimeShutdown(true);

#if defined(_DEBUG) || defined(NKENTSEU_DEBUG_CONSOLE)
	fflush(stdout);
	fflush(stderr);
	// Symetrique du bloc d'entree : on ne libere que ce qu'on a alloue. Appeler
	// `FreeConsole()` sur la console de l'appelant l'en detacherait.
	if (nkConsoleAllocated) {
		FreeConsole();
	}
#endif

	return result;
}
