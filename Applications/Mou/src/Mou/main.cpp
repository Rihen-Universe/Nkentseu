// =============================================================================
// main.cpp — Point d'entrée Mú (jeux éducatifs maternelle).
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "Core/MouConfig.h"
#include "Platform/MouPlatformApp.h"

using namespace nkentseu;
using namespace mou;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName = "Mu";
    d.appVersion = "0.1.0";
    return d;
})());

int nkmain(const NkEntryState& state) {
    logger.Infof("%s v%s demarrage", globals::PLATFORM_NAME, globals::PLATFORM_VERSION);

    MouPlatformApp app;
    if (!app.Initialize(state)) {
        logger.Error("Impossible d'initialiser la plateforme Mu");
        return -1;
    }

    int exitCode = app.Run();
    logger.Infof("%s execution terminee (code: %d)", globals::PLATFORM_NAME, exitCode);
    return exitCode;
}
