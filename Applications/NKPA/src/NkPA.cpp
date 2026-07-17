/**
 * @File    NkPA.cpp
 * @Brief   Point d'entree — NKPA Procedural Animation.
 *          12 especes animees par IK (poisson, requin, anguille, meduse,
 *          serpent, chenille, mille-pattes, ver, lezard, tortue, chat, oiseau).
 *
 *  Argument optionnel : --api=<software|opengl|vulkan|dx11|dx12>
 *  Defaut : software
 */

#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKFileSystem/NkFile.h"
#include "NkPAApp.h"
#include "NkPAConfig.h"

#include <cstring>
#include <cstdio>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	#include <windows.h>
#endif

using namespace nkentseu;
using namespace nkentseu::nkpa;

// Relance le MÊME exécutable (il relira nkpa.cfg pour le nouveau backend).
static void RelaunchSelf() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	char path[1024];
	if (GetModuleFileNameA(nullptr, path, (DWORD)sizeof(path)) == 0)
		return;
	char cmd[1100];
	std::snprintf(cmd, sizeof(cmd), "\"%s\"", path);
	STARTUPINFOA si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
#endif
}

// ─── Backend depuis --api=xxx (PRIORITAIRE sur le fichier de config) ──────────

static bool TryParseApiArg(const NkEntryState &state, NkGraphicsApi &out) {
	for (usize i = 0; i < state.args.Size(); ++i) {
		const char *arg = state.args[i].CStr();
		if (!arg)
			continue;
		const char *val = nullptr;
		if (::strncmp(arg, "--api=", 6) == 0)
			val = arg + 6;
		else if (::strncmp(arg, "-api=", 5) == 0)
			val = arg + 5;
		if (val && NkPAConfig::ParseApi(NkString(val), out))
			return true;
	}
	return false;
}

int nkmain(const NkEntryState &state) {
	// ── Fichier de config ÉDITABLE (créé avec les défauts s'il n'existe pas) ────
	const NkString cfgPath = NkPAConfig::DefaultPath();
	if (!NkFile::Exists(cfgPath.CStr()))
		NkPAConfig{}.Save(cfgPath);
	NkPAConfig cfg = NkPAConfig::Load(cfgPath);

	// ── Choix du backend : --api=xxx > fichier de config ───────────────────────
	NkGraphicsApi api;
	if (!TryParseApiArg(state, api))
		api = cfg.backend;

	// Chemin de l'exe (args[0]) → nécessaire au redémarrage déclenché par l'UI.
	const NkString exePath = state.args.Size() > 0 ? state.args[0] : NkString("");

	NkPAApp app;
	app.SetLaunchInfo(cfgPath, exePath);
	if (!app.Init(1280, 720, api))
		return 1;
	app.Run();
	const bool restart = app.WantsRestart();
	app.Shutdown();

	// Changement de backend depuis l'UI → relancer l'appli (nouveau backend via config).
	if (restart)
		RelaunchSelf();
	return 0;
}
