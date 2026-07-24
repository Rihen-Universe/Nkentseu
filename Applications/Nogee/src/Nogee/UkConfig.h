#pragma once

#include "Noge/Core/NkApplicationConfig.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace noge {

		// =====================================================================
		// NogeeUiBackend — choix du moteur UI de l'éditeur (migration douce).
		//
		//  - NKUI (défaut, legacy) : UILayer/Overlay NKUI actuel de Nogee
		//    (Layers/UILayer.*, immediate-mode, layout fixe).
		//  - RHIShell : coquille NkEditorShell (NKEditorKit/NKGui) rendue par le
		//    renderer RHI GÉNÉRALISÉ nkentseu::nkgui::NkEditorRHIRenderer
		//    (Integrations/NKGui/NkEditorRHIRenderer.h — device NKRHI partagé,
		//    hook PreUI pour le viewport 3D offscreen, RegisterTexture ; règle
		//    « une fenêtre = une pile », jamais NKCanvas ici).
		//
		// ÉCART DOCUMENTÉ (2026-07-24) : Nogee n'utilise PAS NkEditorShell
		// aujourd'hui — son shell est Noge::Application + LayerStack + UILayer
		// NKUI. Le branchement effectif du mode RHIShell (créer NkEditorShell +
		// injecter NkEditorRHIRenderer via NkEditorShellConfig::renderer, comme
		// NkAnimaEditor/main.cpp) reste à câbler lors de la reprise de Nogee :
		// à cette date la cible Nogee ne compile pas (erreurs PRÉ-EXISTANTES
		// indépendantes : includes "Noge/Core/Application.h" obsolètes, API
		// NkString/Application divergée — voir Engine/Noge/ROADMAP.md pilier 6).
		// Ce flag est le point d'entrée de config de cette migration douce ;
		// NKUI n'est pas supprimé.
		// =====================================================================
		enum class NogeeUiBackend : nk_uint8 {
			NKUI = 0,	  // legacy (défaut)
			RHIShell = 1, // NkEditorShell + NkEditorRHIRenderer généralisé
		};

		// =====================================================================
		// NogeAppConfig
		// Configuration spécifique à l'éditeur Noge.
		// Étend NkApplicationConfig avec des paramètres propres à l'éditeur.
		// =====================================================================
		struct NogeAppConfig {
				NkApplicationConfig appConfig;

				// Moteur UI de l'éditeur (--ui=nkui|rhi ; défaut : NKUI legacy)
				NogeeUiBackend uiBackend = NogeeUiBackend::NKUI;

				// Titre fenêtre (le backend GPU est ajouté automatiquement)
				NkString windowTitle = "Noge Editor";

				// Chemin du projet ouvert au démarrage (optionnel)
				NkString startupProjectPath;

				// Mode édition démarré directement en "play" (pour debug rapide)
				bool startInPlayMode = false;

				// Résolution du viewport par défaut (peut différer de la fenêtre)
				nk_uint32 defaultViewportWidth = 1280;
				nk_uint32 defaultViewportHeight = 720;

				// ── Parse des arguments ligne de commande ─────────────────────────
				void Initialize() noexcept {
					for (nk_usize i = 0; i < appConfig.entryState.GetArgCount(); ++i) {
						const char *arg = appConfig.entryState.GetArg(i);
						if (!arg)
							continue;

						if (NkString(arg) == "--debug") {
							appConfig.debugMode = true;
							appConfig.logLevel = NkLogLevel::NK_DEBUG;
						} else if (NkString(arg) == "--play") {
							startInPlayMode = true;
						}
						// --project=path/to/project.nkproj
						else if (NkStringView(arg).StartsWith("--project=")) {
							startupProjectPath = NkString(arg + 10);
						}
						// --ui=nkui|rhi : moteur UI de l'éditeur (cf. NogeeUiBackend)
						else if (NkStringView(arg).StartsWith("--ui=")) {
							NkString ui(arg + 5);
							if (ui == "rhi")
								uiBackend = NogeeUiBackend::RHIShell;
							else if (ui == "nkui")
								uiBackend = NogeeUiBackend::NKUI;
						}
						// --backend=vulkan|dx12|dx11|opengl|sw
						else if (NkStringView(arg).StartsWith("--backend=")) {
							NkString backend(arg + 10);
							if (backend == "vulkan")
								appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_VULKAN;
							else if (backend == "dx12")
								appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_DX12;
							else if (backend == "dx11")
								appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_DX11;
							else if (backend == "opengl")
								appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
							else if (backend == "sw")
								appConfig.deviceInfo.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
						}
					}
				}
		};

	} // namespace noge
} // namespace nkentseu
