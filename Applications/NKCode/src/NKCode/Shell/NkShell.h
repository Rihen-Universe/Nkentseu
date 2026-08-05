#pragma once
// =============================================================================
// NkShell.h — exécution shell avec garde plateforme.
// WRAPPER MAISON DÉSIGNÉ : c'est LE point d'entrée unique de NKCode pour
// l'appel système shell ; partout ailleurs, utiliser NkCodeShellRun.
//
// Windows : PAS de std::system(). Celui-ci lance cmd.exe en lui laissant
// créer sa propre console — une fenêtre noire qui apparaît puis disparaît
// par-dessus l'IDE (issue beta #15 : « Nouvelle fenêtre »). On passe donc par
// CreateProcessW avec CREATE_NO_WINDOW, qui exécute la même commande sans
// jamais allouer de console. Tous les appelants en profitent : ouvrir un
// dossier dans l'explorateur, lancer une seconde fenêtre de l'IDE…
//
// std::system() est par ailleurs marqué "unavailable" sur iOS/tvOS/watchOS :
// no-op sur ces plateformes (ces fonctions sont des commodités de bureau).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include <cstdlib>

#if defined(_WIN32)
	#include <windows.h>
#endif

// Fonction libre (hors namespace pour éviter tout conflit d'ambiguïté avec le
// namespace nkcode existant de NKCode).
static inline int NkCodeShellRun(const char *cmd) {
#if defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || defined(NKENTSEU_PLATFORM_WATCHOS)
	(void)cmd;
	return -1; // shell indisponible sur les plateformes Apple mobiles
#elif defined(_WIN32)
	if (!cmd || !*cmd)
		return -1;
	// « cmd.exe /c <commande> » : même interprétation que std::system (la
	// commande peut contenir `start`, des redirections, des guillemets…),
	// mais SANS console. CreateProcessW exige une ligne de commande
	// MODIFIABLE, d'où la copie.
	const int wide = MultiByteToWideChar(CP_UTF8, 0, cmd, -1, nullptr, 0);
	if (wide <= 0)
		return -1;
	const int prefixe = 7; // « cmd /c »
	wchar_t *ligne = static_cast<wchar_t *>(
		HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * (static_cast<size_t>(wide) + prefixe + 1)));
	if (!ligne)
		return -1;
	wcscpy_s(ligne, static_cast<size_t>(wide) + prefixe + 1, L"cmd /c ");
	MultiByteToWideChar(CP_UTF8, 0, cmd, -1, ligne + prefixe, wide);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {};
	int code = -1;
	if (CreateProcessW(nullptr, ligne, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
		// On attend cmd.exe, comme le faisait std::system. Avec `start`, il rend
		// la main immediatement : le processus lance, lui, reste independant.
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD ec = 0;
		if (GetExitCodeProcess(pi.hProcess, &ec))
			code = static_cast<int>(ec);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	HeapFree(GetProcessHeap(), 0, ligne);
	return code;
#else
	return std::system(cmd);
#endif
}
