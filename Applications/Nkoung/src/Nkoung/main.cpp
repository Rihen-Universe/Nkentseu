// =============================================================================
// main.cpp � Point d'entr�e Nkoung
// Plateforme de jeux 2D : s�lection et lancement de jeux Nkoung.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "Core/NkoungConfig.h"
#include "Platform/NkoungPlatformApp.h"

using namespace nkentseu;
using namespace nkoung;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName = "Nkoung";
    d.appVersion = "0.2.0";
    return d;
})());

int nkmain(const NkEntryState& state) {
    logger.Infof("%s v%s demarrage", globals::PLATFORM_NAME, globals::PLATFORM_VERSION);

    NkoungPlatformApp app;
    if (!app.Initialize(state)) {
        logger.Error("Impossible d'initialiser la plateforme Nkoung");
        return -1;
    }

    int exitCode = app.Run();
    logger.Infof("%s execution terminee (code: %d)", globals::PLATFORM_NAME, exitCode);

    return exitCode;
}
