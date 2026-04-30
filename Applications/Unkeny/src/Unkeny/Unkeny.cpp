#include "Nkentseu/Core/Application.h"
#include "Unkeny/UkConfig.h"
#include "Unkeny/UnkenyApp.h"

// =============================================================================
// CreateApplication — appelé par le framework (Application.cpp / nkmain)
// =============================================================================
nkentseu::Application* nkentseu::CreateApplication(
    const nkentseu::NkApplicationConfig& baseConfig)
{
    using namespace nkentseu;
    using namespace nkentseu::Unkeny;

    // ── Configuration de base ─────────────────────────────────────────────────
    UnkenyAppConfig ukConfig;
    ukConfig.appConfig = baseConfig;

    // Identité
    ukConfig.appConfig.appName    = "Unkeny";
    ukConfig.appConfig.appVersion = "0.1.0";

    // Fenêtre
    ukConfig.appConfig.windowConfig.title    = "Unkeny Editor";
    ukConfig.appConfig.windowConfig.width    = 1600;
    ukConfig.appConfig.windowConfig.height   = 900;
    ukConfig.appConfig.windowConfig.centered = true;
    ukConfig.appConfig.windowConfig.resizable = true;

    // Device RHI (défaut OpenGL — peut être surchargé par --backend=)
    ukConfig.appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
    ukConfig.appConfig.deviceInfo.context.vulkan.appName    = "Unkeny";
    ukConfig.appConfig.deviceInfo.context.vulkan.engineName = "Nkentseu";

    // Cache shader
    NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

    // ── Parse des arguments CLI ───────────────────────────────────────────────
    ukConfig.Initialize();

    // ── Création de l'application ─────────────────────────────────────────────
    return new UnkenyApp(ukConfig);
}
