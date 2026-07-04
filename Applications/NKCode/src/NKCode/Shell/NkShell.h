#pragma once
// =============================================================================
// NkShell.h — exécution shell (std::system) avec garde plateforme.
// std::system() est marqué "unavailable" sur iOS/tvOS/watchOS : on le remplace
// par un no-op sur ces plateformes (fonctionnalités desktop : ouvrir un dossier
// dans l'explorateur, lancer un terminal, etc.).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include <cstdlib>

// Fonction libre (hors namespace pour éviter tout conflit d'ambiguïté avec le
// namespace nkcode existant de NKCode).
static inline int NkCodeShellRun(const char* cmd) {
#if defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || defined(NKENTSEU_PLATFORM_WATCHOS)
    (void)cmd;
    return -1;  // shell indisponible sur les plateformes Apple mobiles
#else
    return std::system(cmd);
#endif
}
