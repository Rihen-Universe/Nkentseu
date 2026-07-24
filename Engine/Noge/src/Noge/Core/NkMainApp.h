#pragma once

#include "NKWindow/NKMain.h"
#include "NkApplication.h"
#include "NKLogger/NkLog.h"

// =========================================================================
// NkMainApplication
// À définir par l'utilisateur dans son fichier application.
// Le framework appelle cette fonction pour obtenir l'instance.
//
// Exemple :
//  NkApplication* nkentseu::NkMainApplication(const NkApplicationConfig& cfg) {
//       return new MyApp(cfg);
//  }
// =========================================================================
NkApplication *NkMainApplication(const NkApplicationConfig &config);

namespace nkentseu {
	// =============================================================================
	// nkmain — point d'entrée cross-platform
	// Le framework appelle nkmain() depuis NkMain.h / NkEntry.h
	// =============================================================================
	int nkmain(const nkentseu::NkEntryState &state) {
		// Construction de la config de base (NkEntryState n'a pas de ctor par
		// défaut sur toutes les plateformes → passage par le constructeur)
		nkentseu::NkApplicationConfig config(state);

		// L'utilisateur remplit le reste via CreateApplication
		nkentseu::NkApplication *app = nkentseu::NkMainApplication(config);

		if (!app) {
			logger.Error("[Application] Erreur de creation de lapplication");
			return 1;
		}

		if (!app->Init()) {
			logger.Error("[Application] Erreur dinitialisation de lapplication");
			return 2;
		}

		app->Run();

		delete app;
		return 0;
	}

} // namespace nkentseu