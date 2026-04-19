#pragma once

#include "Nkentseu/Core/NkApplicationConfig.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace Unkeny {

        // =====================================================================
        // UnkenyAppConfig
        // Configuration spécifique à l'éditeur Unkeny.
        // Étend NkApplicationConfig avec des paramètres propres à l'éditeur.
        // =====================================================================
        struct UnkenyAppConfig {
            NkApplicationConfig appConfig;

            // Titre fenêtre (le backend GPU est ajouté automatiquement)
            NkString windowTitle = "Unkeny Editor";

            // Chemin du projet ouvert au démarrage (optionnel)
            NkString startupProjectPath;

            // Mode édition démarré directement en "play" (pour debug rapide)
            bool startInPlayMode = false;

            // Résolution du viewport par défaut (peut différer de la fenêtre)
            nk_uint32 defaultViewportWidth  = 1280;
            nk_uint32 defaultViewportHeight = 720;

            // ── Parse des arguments ligne de commande ─────────────────────────
            void Initialize() noexcept {
                for (nk_usize i = 0; i < appConfig.entryState.GetArgCount(); ++i) {
                    const char* arg = appConfig.entryState.GetArg(i);
                    if (!arg) continue;

                    if (NkString(arg) == "--debug") {
                        appConfig.debugMode = true;
                        appConfig.logLevel  = NkLogLevel::NK_DEBUG;
                    }
                    else if (NkString(arg) == "--play") {
                        startInPlayMode = true;
                    }
                    // --project=path/to/project.nkproj
                    else if (NkStringView(arg).StartsWith("--project=")) {
                        startupProjectPath = NkString(arg + 10);
                    }
                    // --backend=vulkan|dx12|dx11|opengl|sw
                    else if (NkStringView(arg).StartsWith("--backend=")) {
                        NkString backend(arg + 10);
                        if (backend == "vulkan")  appConfig.deviceInfo.api = NkGraphicsApi::NK_API_VULKAN;
                        else if (backend == "dx12")   appConfig.deviceInfo.api = NkGraphicsApi::NK_API_DIRECTX12;
                        else if (backend == "dx11")   appConfig.deviceInfo.api = NkGraphicsApi::NK_API_DIRECTX11;
                        else if (backend == "opengl") appConfig.deviceInfo.api = NkGraphicsApi::NK_API_OPENGL;
                        else if (backend == "sw")     appConfig.deviceInfo.api = NkGraphicsApi::NK_API_SOFTWARE;
                    }
                }
            }
        };

    } // namespace Unkeny
} // namespace nkentseu
