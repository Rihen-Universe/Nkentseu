// =============================================================================
// Nogee.cpp — point d'entrée de l'éditeur Nogee
// =============================================================================
// Pattern d'entrée réel du framework : NKWindow/NKMain.h fournit le main
// natif cross-platform qui appelle nkmain(const NkEntryState &). L'application
// est la classe Noge NkApplication (Engine/Noge/src/Noge/Core/NkApplication.h) :
// Init() → Run() (boucle, fenêtre, device RHI, dispatch des événements).
// =============================================================================
#include "Noge/Core/NkApplication.h"
#include "Nogee/UkConfig.h"
#include "Nogee/Shell/NogeeShell.h" // coquille NKEditorKit/NKGui — SEUL chemin depuis la coupe
#include "NKWindow/NKMain.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"
// ⚠️ COUPE NKUI (2026-08-17, decision de Rodolf : « retirer NKUI des
// dependances des autres applications ») : `NogeeApp.h` n'est plus inclus — il
// tire UILayer, donc NKUI. Le chemin legacy (NogeApp + LayerStack + UILayer)
// est EXCLU du build (cf. Nogee.jenga, excludefiles) ; ses sources restent au
// depot, depreciees. La coquille a la parite MESUREE : les 4 panneaux portes
// affichent les memes donnees (temoin visuel + numerique, rejoue dans les deux
// sens, 0 [ERR]), le monde ECS et les systemes editeur sont cables cote shell
// (NogeeShell.cpp), et le viewport NKUI ne rendait RIEN (RenderScene et
// RenderGizmos etaient des TODO — le FBO etait seulement nettoye).

// ── AppData global (consommé par le runtime NKWindow avant nkmain) ───────────
nkentseu::NkAppData appData = [] {
	nkentseu::NkAppData d{};
	d.appName = "Nogee";
	d.appVersion = "0.1.0";
	return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

// =============================================================================
// nkmain — appelé par le point d'entrée natif (NkMain.h / NkEntry.h)
// =============================================================================
int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;
	using namespace nkentseu::noge;

	// ── Configuration de base ─────────────────────────────────────────────────
	NogeAppConfig ukConfig(state);

	// Identité
	ukConfig.appConfig.appName = "Noge";
	ukConfig.appConfig.appVersion = "0.1.0";

	// Fenêtre
	ukConfig.appConfig.windowConfig.title = "Noge Editor";
	ukConfig.appConfig.windowConfig.width = 1600;
	ukConfig.appConfig.windowConfig.height = 900;
	ukConfig.appConfig.windowConfig.centered = true;
	ukConfig.appConfig.windowConfig.resizable = true;

	// Device RHI (défaut OpenGL — peut être surchargé par --backend=)
	ukConfig.appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	ukConfig.appConfig.deviceInfo.context.vulkan.appName = "Noge";
	ukConfig.appConfig.deviceInfo.context.vulkan.engineName = "Nkentseu";

	// Cache shader
	NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

	// ── Parse des arguments CLI ───────────────────────────────────────────────
	ukConfig.Initialize();

	// ── COQUILLE NKEditorKit/NKGui — le chemin UNIQUE depuis la coupe ────────
	// Historique : jusqu'au 2026-08-17, la coquille etait le chemin OPTIONNEL
	// (--ui=rhi) et NogeApp+UILayer (NKUI) le defaut. Les 4 panneaux sont
	// portes, le monde et les systemes editeur sont cables cote shell, et la
	// parite est mesuree (temoin dans les deux sens) : la coquille devient le
	// seul chemin. `--ui=rhi` reste accepte (sans effet) ; `NogeeUiBackend`
	// documente la migration dans UkConfig.h.
	for (const auto &a : state.GetArgs()) {
		if (a == "--occlusion-test")
			NogeeShellEnableOcclusionProbe(false); // palette Ctrl+P
		else if (a == "--occlusion-test-prefs")
			NogeeShellEnableOcclusionProbe(true); // fenetre Preferences
		else if (a == "--no-mask-body")
			NogeeShellReproduceConquerorLabCondition(); // condition ConquerorLab
		else if (a == "--dragdrop-test")
			NogeeShellEnableDragDropProbe(); // sonde glisser-deposer §7/§9
	}
	logger.Info("[Nogee] montage de la coquille NkEditorShell (chemin unique depuis la coupe NKUI)\n");
	return RunNogeeEditorShell(ukConfig);
}
