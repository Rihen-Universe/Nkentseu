// =============================================================================
// Tutoriels3D / Étape 1 — LA FENÊTRE
//
// Le strict minimum d'une application Nkentseu : ouvrir une fenêtre native et
// réagir aux événements (fermeture, clavier, redimensionnement).
// Pas encore de rendu — c'est l'objet de l'étape 2.
// =============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkClock.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "Tuto01Fenetre";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

int nkmain(const NkEntryState &state) {
	(void)state;

	// 1) Décrire puis créer la fenêtre.
	NkWindowConfig cfg;
	cfg.title = "Tuto 01 — Une fenetre Nkentseu";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsValid()) {
		logger.Error("[Tuto01] Creation fenetre KO");
		return 1;
	}

	// 2) S'abonner aux événements qui nous intéressent.
	bool running = true;
	NkEventSystem &events = NkEvents();

	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		logger.Info("[Tuto01] Touche pressee : {0}", e->ToString());
		if (e->GetKey() == NkKey::NK_ESCAPE)
			running = false;
	});
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		logger.Info("[Tuto01] Nouvelle taille : {0}x{1}", e->GetWidth(), e->GetHeight());
	});

	// 3) La boucle principale : dépiler les événements, puis (plus tard) rendre.
	while (running && window.IsOpen()) {
		events.PollEvents();
		NkClock::Sleep((int64)10); // pas de rendu -> on rend la main au CPU
	}

	window.Close();
	logger.Info("[Tuto01] Termine proprement.");
	return 0;
}
