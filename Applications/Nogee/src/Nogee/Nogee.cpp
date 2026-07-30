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
#include "Nogee/NogeeApp.h"
#include "NKWindow/NKMain.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"

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

	// ── Migration douce UI (2026-07-24) ──────────────────────────────────────
	// --ui=rhi demande la coquille NkEditorShell (NKEditorKit) rendue par le
	// renderer RHI GÉNÉRALISÉ nkentseu::nkgui::NkEditorRHIRenderer
	// (Integrations/NKGui/NkEditorRHIRenderer.h). Le câblage effectif (modèle :
	// Applications/NkAnimaEditor/src/NkAnimaEditor/main.cpp — NkEditorShellConfig
	// cfg; cfg.renderer = &rhi; + SetPreUI pour le viewport offscreen) sera fait
	// à la reprise de Nogee ; en attendant on reste sur l'UILayer NKUI legacy
	// (voir NogeeUiBackend dans UkConfig.h pour l'écart documenté).
	if (ukConfig.uiBackend == NogeeUiBackend::RHIShell) {
		logger.Warn("[Nogee] --ui=rhi demande : chemin NkEditorShell+NkEditorRHIRenderer "
					"pas encore cable — repli sur l'UI NKUI legacy.\n");
	}

	// ── Création + boucle ─────────────────────────────────────────────────────
	NogeApp *app = new NogeApp(ukConfig);
	if (!app->Init()) {
		logger.Error("[Nogee] Erreur d'initialisation de l'application");
		delete app;
		return 2;
	}
	app->Run();
	delete app;
	return 0;
}
