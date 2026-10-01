// =============================================================================
// NogeDemo — le premier resultat de Noge, de bout en bout
//
// Une scene 3D (un sol, un cube rouge que l'on pousse, une balle jaune qui lui
// tombe dessus, une camera, un soleil) jouee par le VRAI chemin du moteur :
// NkApplication + NkEngineLayer (ECS, pont physique, pont de rendu) et la
// carte d'entree du noyau (NkInputMap, la meme qu'Unkeny).
//
// LANCER (de n'importe ou : on va a la racine du depot, ou les nuanceurs sont lus)
//   ./Build/Bin/Debug-Windows/NogeDemo/NogeDemo.exe
//       [--selftest] [--capture=IMAGE.png] [--charger]
//       [--scene=FICHIER.nkscene] [--entrees=FICHIER.nkinput]
//
//   (rien)          la fenetre : ZQSD/WASD pousser le cube, Espace sauter,
//                   E lacher une caisse, clic droit + souris ou fleches pour
//                   tourner la camera, molette zoom, F5 sauver, F9 recharger,
//                   Echap quitter
//   --selftest      le banc SANS fenetre (NogeDemoBanc.cpp) ; 0 = tout tient
//   --capture=...   fenetre HORS ECRAN et sans focus : rend 3 s de scene,
//                   ecrit l'image, verifie des pixels, sort (0 = tout tient)
//   --charger       demarre sur la scene sauvegardee (F5) au lieu de la neuve
//   --scene=...     le fichier de F5 / F9 (defaut Build/NogeDemo/scene.nkscene)
//   --entrees=...   le fichier des entrees (defaut Build/NogeDemo/entrees.nkinput,
//                   ecrit au premier lancement : modifiez-le pour changer les touches)
// =============================================================================

#include "NogeDemoApp.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h" // LocateResource : les nuanceurs, quel que soit le dossier de lancement
#include "NKLogger/NkLog.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/NKMain.h"

#include <cstdio>

namespace nkentseu {
	namespace nogedemo {
		int32 NogeDemoLancerBanc(); // NogeDemoBanc.cpp
	} // namespace nogedemo
} // namespace nkentseu

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "NogeDemo";
	d.appVersion = "0.1.0";
	return d;
})());

namespace {

	/// Les nuanceurs sont lus RELATIVEMENT au repertoire courant
	/// (Resources/NKRenderer/Shaders, ROADMAP Noge §9decies). Lance d'ailleurs
	/// que la racine -- un double-clic dans Build/Bin/..., un raccourci, une
	/// console ailleurs -- on va au dossier qui les contient, et on le DIT.
	///
	/// (2026-10-01) Le dossier est TROUVE par NkPath::LocateResource (dossier
	/// courant, dossier de l'exe, puis en remontant leurs parents) : la liste
	/// fixe « ../../../.. » d'avant ne marchait que lance DEPUIS Build/Bin/...,
	/// pas depuis un dossier sans rapport.
	void AllerALaRacine() {
		using nkentseu::NkDirectory;
		using nkentseu::NkString;
		static const char kNuanceurs[] = "Resources/NKRenderer/Shaders";
		if (NkDirectory::Exists(kNuanceurs)) {
			return;
		}
		const NkString trouve = nkentseu::NkPath::LocateResource(kNuanceurs);
		const nkentseu::usize n = sizeof(kNuanceurs) - 1u; // + la barre qui le precede
		if (trouve.Length() > n + 1u) {
			const NkString racine(trouve.CStr(), trouve.Length() - n - 1u);
			if (NkDirectory::SetCurrentDirectory(racine.CStr())) {
				std::printf("[NogeDemo] repertoire courant ramene a la racine du depot (%s)\n", racine.CStr());
				return;
			}
		}
		std::printf("[NogeDemo] ATTENTION : Resources/NKRenderer/Shaders introuvable -- lancer depuis la racine du depot\n");
	}

} // namespace

int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;
	using namespace nkentseu::nogedemo;
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
#endif
	AllerALaRacine();

	NogeDemoOptions options;
	for (const auto &a : state.GetArgs()) {
		if (a == "--selftest") {
			return NogeDemoLancerBanc();
		}
		if (a.StartsWith("--capture=")) {
			options.capture = NkString(a.SubStr(10));
		} else if (a.StartsWith("--scene=")) {
			options.cheminScene = NkString(a.SubStr(8));
		} else if (a.StartsWith("--entrees=")) {
			options.cheminEntrees = NkString(a.SubStr(10));
		} else if (a == "--charger") {
			options.chargerAuDepart = true;
		}
	}

	NkApplicationConfig config(state);
	config.appName = "NogeDemo";
	config.appVersion = "0.1.0";
	config.fixedTimeStep = 1.f / 60.f;
	config.windowConfig.title = "NogeDemo -- Noge : premier resultat";
	config.windowConfig.width = 1280;
	config.windowConfig.height = 720;
	config.windowConfig.centered = true;
	config.windowConfig.resizable = true;
	config.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	if (!options.capture.Empty()) {
		// ── LA FENETRE DE CAPTURE NE GENE PERSONNE ─────────────────────────
		// Hors de tout ecran, sans focus (WS_EX_NOACTIVATE), transparente aux
		// clics, sans bouton de barre des taches. Le rendu va dans une cible
		// HORS ECRAN : ce que la fenetre montre n'est pas ce qui est mesure.
		config.windowConfig.title = "NogeDemo -- CAPTURE (pas le produit, se ferme seule)";
		config.windowConfig.centered = false;
		config.windowConfig.x = -20000;
		config.windowConfig.y = -20000;
		config.windowConfig.noActivate = true;
		config.windowConfig.clickThrough = true;
		config.windowConfig.native.utilityWindow = true;
	}
	NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

	NogeDemoApp app(config, options);
	if (!app.Init()) {
		logger.Error("[NogeDemo] l'application ne s'initialise pas (fenetre ou device)\n");
		return 2;
	}
	app.Run();
	return app.CodeSortie();
}
