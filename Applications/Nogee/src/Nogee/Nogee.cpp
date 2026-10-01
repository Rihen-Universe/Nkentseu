// -----------------------------------------------------------------------------
// @File    Nogee.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// =============================================================================
// Nogee.cpp — point d'entrée de l'éditeur Nogee
// =============================================================================
// DEUX CHEMINS, DANS LE MEME EXECUTABLE (« combler sans detruire l'existant ») :
//
//   (defaut, 2026-10-01, feuille de route R32) L'EDITEUR A L'APPARENCE
//   D'UNKENYEDITOR : NogeeEditeurApp (Nogee/Editeur/) -- NkApplication +
//   NkEngineLayer (le chemin de jeu de Noge, celui de NogeDemo) rendu dans une
//   vue 3D hors ecran au centre, et autour la disposition d'UE5 peinte avec les
//   pieces PARTAGEES de NKEditorKit (NKEditorKit/Famille).
//
//   (--ancienne-coquille, ou une des sondes ci-dessous) LA COQUILLE D'AVANT :
//   NkEditorShell + panneaux ancrables + palette Ctrl+P (Nogee/Shell/), INTACTE.
//   Elle s'ouvre aussi depuis Fenetre > « Ancienne coquille Nogee ».
//
// LANCER (depuis la RACINE du depot : les nuanceurs de NKRenderer y sont lus)
//   ./Build/Bin/Debug-Windows/Nogee/Nogee.exe
//       [--scene=F.nkscene] [--contenu=DOSSIER] [--theme=clair]
//       [--selection=NOM] [--outil=deplacer|tourner|echelle]
//       [--tiroir=contenu|journal|terminal] [--jouer]
//       [--capture=IMAGE.png [--temps-capture=S]]   (fenetre HORS ECRAN, puis sortie)
//       [--ancienne-coquille]
// =============================================================================
#include "Noge/Core/NkApplication.h"
#include "Nogee/UkConfig.h"
#include "Nogee/Shell/NogeeShell.h" // l'ancienne coquille NKEditorKit/NKGui (--ancienne-coquille)
#include "Nogee/Editeur/NogeeEditeurApp.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKWindow/NKMain.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>

// ── AppData global (consommé par le runtime NKWindow avant nkmain) ───────────
nkentseu::NkAppData appData = [] {
	nkentseu::NkAppData d{};
	d.appName = "Nogee";
	d.appVersion = "0.1.0";
	return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

namespace {

	/// Les nuanceurs sont lus RELATIVEMENT au repertoire courant
	/// (Resources/NKRenderer/Shaders) : lance d'ailleurs que la racine, on y
	/// remonte si on la reconnait, et on le DIT (le geste de NogeDemo).
	void AllerALaRacine() {
		using nkentseu::NkDirectory;
		if (NkDirectory::Exists("Resources/NKRenderer/Shaders")) {
			return;
		}
		const char *candidats[] = {"../../../..", "../../..", "../..", ".."};
		for (const char *c : candidats) {
			char essai[256];
			std::snprintf(essai, sizeof(essai), "%s/Resources/NKRenderer/Shaders", c);
			if (NkDirectory::Exists(essai)) {
				(void)NkDirectory::SetCurrentDirectory(c);
				std::printf("[Nogee] repertoire courant ramene a la racine du depot (%s)\n", c);
				return;
			}
		}
		std::printf("[Nogee] ATTENTION : Resources/NKRenderer/Shaders introuvable -- lancer depuis la racine du depot\n");
	}

	/// L'ANCIENNE COQUILLE (NkEditorShell), telle qu'elle etait avant R32.
	int LancerAncienneCoquille(const nkentseu::NkEntryState &state) {
		using namespace nkentseu;
		using namespace nkentseu::noge;
		NogeAppConfig ukConfig(state);
		ukConfig.appConfig.appName = "Noge";
		ukConfig.appConfig.appVersion = "0.1.0";
		ukConfig.appConfig.windowConfig.title = "Noge Editor";
		ukConfig.appConfig.windowConfig.width = 1600;
		ukConfig.appConfig.windowConfig.height = 900;
		ukConfig.appConfig.windowConfig.centered = true;
		ukConfig.appConfig.windowConfig.resizable = true;
		ukConfig.appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
		ukConfig.appConfig.deviceInfo.context.vulkan.appName = "Noge";
		ukConfig.appConfig.deviceInfo.context.vulkan.engineName = "Nkentseu";
		NkShaderCache::Global().SetCacheDir("Build/ShaderCache");
		ukConfig.Initialize();
		for (const auto &a : state.GetArgs()) {
			if (a == "--occlusion-test")
				NogeeShellEnableOcclusionProbe(false); // palette Ctrl+P
			else if (a == "--occlusion-test-prefs")
				NogeeShellEnableOcclusionProbe(true); // fenetre Preferences
			else if (a == "--no-mask-body")
				NogeeShellReproduceConquerorLabCondition(); // condition ConquerorLab
			else if (a == "--dragdrop-test")
				NogeeShellEnableDragDropProbe(); // sonde glisser-deposer §7/§9
			else if (a == "--panneaux-sonde")
				NogeeShellEnablePanneauxSonde(); // MESURE de la disposition (bandes + rects)
			else if (a == "--panneaux-sonde-redim")
				NogeeShellEnablePanneauxSonde(true); // ... puis on change la taille et on remesure
			else if (a == "--panneaux-pose")
				NogeeShellEnablePanneauxSonde(false, true); // ... puis on laisse poser pour la photo
		}
		logger.Info("[Nogee] ancienne coquille NkEditorShell (--ancienne-coquille)\n");
		return RunNogeeEditorShell(ukConfig);
	}

} // namespace

// =============================================================================
// nkmain — appelé par le point d'entrée natif (NkMain.h / NkEntry.h)
// =============================================================================
int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;
	using namespace nkentseu::nogee;
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
#endif

	// ── Quel chemin ? Les sondes de l'ancienne coquille l'y ramenent d'elles-memes.
	for (const auto &a : state.GetArgs()) {
		if (a == "--ancienne-coquille" || a == "--ui=rhi" || a == "--occlusion-test" || a == "--occlusion-test-prefs" ||
			a == "--no-mask-body" || a == "--dragdrop-test" || a == "--panneaux-sonde" || a == "--panneaux-sonde-redim" ||
			a == "--panneaux-pose") {
			return LancerAncienneCoquille(state);
		}
	}

	AllerALaRacine();
	NogeeOptions options;
	for (const auto &a : state.GetArgs()) {
		if (a.StartsWith("--capture=")) {
			options.capture = NkString(a.SubStr(10));
		} else if (a.StartsWith("--temps-capture=")) {
			options.tempsCapture = static_cast<float32>(std::atof(NkString(a.SubStr(16)).CStr()));
		} else if (a.StartsWith("--scene=")) {
			options.scene = NkString(a.SubStr(8));
		} else if (a.StartsWith("--contenu=")) {
			options.contenu = NkString(a.SubStr(10));
		} else if (a.StartsWith("--selection=")) {
			options.selection = NkString(a.SubStr(12));
		} else if (a.StartsWith("--outil=")) {
			const NkString o(a.SubStr(8));
			options.outil = o == NkString("deplacer") ? 1 : (o == NkString("tourner") ? 2 : (o == NkString("echelle") ? 3 : 0));
		} else if (a.StartsWith("--tiroir=")) {
			const NkString o(a.SubStr(9));
			options.tiroir = o == NkString("journal") ? 1 : (o == NkString("terminal") ? 2 : 0);
		} else if (a == "--theme=clair") {
			options.clair = true;
		} else if (a == "--jouer") {
			options.jouer = true;
		}
	}

	NkApplicationConfig config(state);
	config.appName = "Nogee";
	config.appVersion = "0.1.0";
	config.fixedTimeStep = 1.f / 60.f;
	config.windowConfig.title = "Nogee — éditeur de Noge";
	config.windowConfig.width = 1280;
	config.windowConfig.height = 760;
	config.windowConfig.minWidth = 900;
	config.windowConfig.minHeight = 560;
	config.windowConfig.centered = true;
	config.windowConfig.resizable = true;
	// SANS CADRE, comme UnkenyEditor et NKCraft : la barre de titre est peinte
	// (logo au coin, menus sur la ligne du titre) ; deplacer et redimensionner
	// sont confies a l'OS (NkWindow::BeginDragMove / BeginResize).
	config.windowConfig.frame = false;
	config.windowConfig.bgColor = 0x141414FFu;
	config.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	config.deviceInfo.context.vulkan.appName = "Nogee";
	config.deviceInfo.context.vulkan.engineName = "Nkentseu";
	if (!options.capture.Empty()) {
		// ── LA FENETRE DE CAPTURE NE GENE PERSONNE ─────────────────────────
		// Hors de tout ecran, sans focus, transparente aux clics, sans bouton de
		// barre des taches (le geste de NogeDemo --capture).
		config.windowConfig.title = "Nogee -- CAPTURE (pas le produit, se ferme seule)";
		config.windowConfig.centered = false;
		config.windowConfig.x = -20000;
		config.windowConfig.y = -20000;
		config.windowConfig.noActivate = true;
		config.windowConfig.clickThrough = true;
		config.windowConfig.native.utilityWindow = true;
	}
	NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

	NogeeEditeurApp app(config, options);
	if (!app.Init()) {
		logger.Error("[Nogee] l'editeur ne s'initialise pas (fenetre ou device)\n");
		return 2;
	}
	app.Run();
	return app.CodeSortie();
}
