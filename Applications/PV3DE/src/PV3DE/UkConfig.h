#pragma once

#include "NKApplication/Core/NkApplicationConfig.h"
#include "NKContainers/String/NkString.h"

// =============================================================================

namespace nkentseu {
    namespace Unkeny {
        // ============================================================================
        // UkConfig — Configuration spécifique à Unkeny (ex: pour l'éditeur)
        // ============================================================================
        struct UnkenyAppConfig {
            NkApplicationConfig appConfig;

            // Titre de la fenêtre (peut être modifié dynamiquement)
            NkString windowTitle;

            // Initialisation spécifique à Unkeny (ex: chargement de plugins, etc.)
            void Initialize() noexcept {
                // Exemple : parse args pour activer le mode debug
                for (const char* arg : args.args) {
                    if (std::strcmp(arg, "--debug") == 0) {
                        appConfig.debugMode = true;
                    }
                }
            }
        };

    }
} // namespace nkentseu 