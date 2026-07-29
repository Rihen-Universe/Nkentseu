// =============================================================================
// main.cpp — Démo minimale HarmonyOS pour Nkentseu.
//
// Ouvre une fenêtre (NkWindow) via l'XComponent ArkTS, s'abonne aux
// événements tactiles/fenêtre de base et boucle jusqu'à fermeture.
// Ne dépend que de NKWindow + NKEvent (pas de rendu) pour valider le
// bring-up HarmonyOS (NAPI Init -> thread nkmain -> XComponent).
//
// Le module ArkTS attendu côté .so est "entry" (cf. NkHarmonyBridge.ts /
// oh-package.json5 du projet HarmonyOS qui charge cette bibliothèque).
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

namespace {
	NkLogger logger("NkHarmonyMain");
}

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkHarmonyDemo";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const NkEntryState &state) {
	(void)state;
	logger.Infof("nkmain: start");

	NkWindowConfig cfg;
	cfg.title = "Nkentseu - HarmonyOS";

	NkWindow window;
	if (!window.Create(cfg)) {
		logger.Errorf("nkmain: window create FAILED");
		return -1;
	}
	logger.Infof("nkmain: window created id=%llu", (unsigned long long)window.GetId());

	auto &ev = NkEvents();

	ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) {
		logger.Infof("nkmain: close requested");
		window.Close();
	});

	ev.AddEventCallback<NkTouchBeginEvent>(
		[&](NkTouchBeginEvent *e) { logger.Infof("nkmain: touch begin contacts=%u", e->GetNumTouches()); });

	while (window.IsOpen()) {
		ev.PollEvents();
	}

	logger.Infof("nkmain: end");
	return 0;
}

#if defined(NKENTSEU_PLATFORM_HARMONYOS)
NKENTSEU_HARMONY_DEFINE_MODULE(entry)
#endif
