#pragma once
/**
 * @File    NkPAApp.h
 * @Brief   Classe principale de l'application NKPA.
 *
 *  Gestion :
 *    - Fenêtre NkWindow
 *    - Device NKRHI (API sélectionnable : software, OpenGL, Vulkan, DX11, DX12)
 *    - Boucle de jeu (BeginFrame / Render / EndFrame)
 *    - 12 créatures (4 marines, 4 terrestres, 4 mixtes)
 *    - Environnement 2 zones
 *    - UI overlay (panneau NKPA)
 */

#include "NKContainers/String/NkString.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKCanvas/Core/NkGraphicsApi.h" // NkGraphicsApi (partagé NKEvent) + NkGraphicsApiName
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // NkVertex2D
#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"  // sim rendue offscreen
#include "NKCanvas/Renderer/Resources/NkTexture.h"      // wrapper sampler de la RT
#include "NKTime/NkChrono.h"

#include "Renderer/NkPAMesh.h"
#include "Environment/NkPAEnvironment.h"
#include "UI/NkPAGui.h"

// ── Créatures marines ─────────────────────────────────────────────────────────
#include "Creatures/NkPAFish.h"
#include "Creatures/NkPAShark.h"
#include "Creatures/NkPAEel.h"
#include "Creatures/NkPAJellyfish.h"

// ── Créatures terrestres ──────────────────────────────────────────────────────
#include "Creatures/NkPASnake.h"
#include "Creatures/NkPACaterpillar.h"
#include "Creatures/NkPACentipede.h"
#include "Creatures/NkPAWorm.h"

// ── Créatures mixtes / volantes ───────────────────────────────────────────────
#include "Creatures/NkPALizard.h"
#include "Creatures/NkPATurtle.h"
#include "Creatures/NkPACat.h"
#include "Creatures/NkPABird.h"

namespace nkentseu {
	namespace renderer {
		class NkRenderWindow;
	}
	namespace nkpa {

		class NkPAApp {
			public:
				NkPAApp() = default;
				~NkPAApp() = default;

				bool Init(int32 width = 1280, int32 height = 720,
						  NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_SOFTWARE);
				void Run();
				void Shutdown();

				/// Infos de lancement (fichier de config éditable + chemin de l'exe) —
				/// nécessaires pour changer de backend + redémarrer depuis l'interface.
				void SetLaunchInfo(const NkString &configPath, const NkString &exePath) {
					mConfigPath = configPath;
					mExePath = exePath;
				}

				/// L'utilisateur a demandé un redémarrage (changement de backend appliqué).
				bool WantsRestart() const {
					return mRestartRequested;
				}

				const NkString &ExePath() const {
					return mExePath;
				}

			private:
				void Update(float32 dt);
				void Render();

				/// (Re)crée l'environnement + place les créatures dans un monde de
				/// dimensions worldW × worldH (= la vue centrale de l'éditeur).
				void SeedWorld(int32 worldW, int32 worldH);

				/// Applique le backend choisi dans l'UI : écrit la config + demande le
				/// redémarrage (relancé par nkmain).
				void ApplyBackendRequest();

				// ── Fenêtre & rendu NKCanvas ──────────────────────────────────────────────
				NkWindow mWindow;
				renderer::NkRenderWindow *mTarget = nullptr; // device + swapchain + renderer 2D

				// ── État ─────────────────────────────────────────────────────────────────
				bool mRunning = false;
				int32 mWidth = 1280;
				int32 mHeight = 720;
				int32 mViewW = 0; // dimensions de la VUE centrale (monde de la sim)
				int32 mViewH = 0;
				NkGraphicsApi mApi = NkGraphicsApi::NK_GFX_API_SOFTWARE;
				NkChrono mChrono;
				NkPAUIState mUIState;

				// ── Redimensionnement : une SEULE source de vérité = l'événement fenêtre
				// (autoritatif), traité à un point sûr de la boucle (pas de poll GetSize
				// qui « lag » selon le backend et se battait avec l'événement).
				bool mResizePending = false;
				int32 mPendingW = 0;
				int32 mPendingH = 0;

				// ── UI (NKGui + backend NKRHI texturé) ────────────────────────────────────
				NkPAGui mGui;

				// ── Environnement ────────────────────────────────────────────────────────
				Environment mEnv;

				// ── Créatures marines (zone eau) ─────────────────────────────────────────
				static constexpr int32 NUM_FISH = 3;
				static constexpr int32 NUM_SHARK = 1;
				static constexpr int32 NUM_EEL = 2;
				static constexpr int32 NUM_JELLYFISH = 2;

				Fish mFish[NUM_FISH];
				Shark mSharks[NUM_SHARK];
				Eel mEels[NUM_EEL];
				Jellyfish mJellyfish[NUM_JELLYFISH];

				// ── Créatures terrestres (zone sol) ──────────────────────────────────────
				static constexpr int32 NUM_SNAKE = 1;
				static constexpr int32 NUM_CATERPILLAR = 2;
				static constexpr int32 NUM_CENTIPEDE = 1;
				static constexpr int32 NUM_WORM = 2;

				Snake mSnakes[NUM_SNAKE];
				Caterpillar mCaterpillars[NUM_CATERPILLAR];
				Centipede mCentipedes[NUM_CENTIPEDE];
				Worm mWorms[NUM_WORM];

				// ── Créatures mixtes / volantes ───────────────────────────────────────────
				static constexpr int32 NUM_LIZARD = 1;
				static constexpr int32 NUM_TURTLE = 1;
				static constexpr int32 NUM_CAT = 1;
				static constexpr int32 NUM_BIRD = 2;

				Lizard mLizards[NUM_LIZARD];
				Turtle mTurtles[NUM_TURTLE];
				Cat mCats[NUM_CAT];
				Bird mBirds[NUM_BIRD];

				// ── Mesh ─────────────────────────────────────────────────────────────────
				MeshBuilder mMesh;
				NkVector<renderer::NkVertex2D> mSimVerts; // scratch : mesh → NkVertex2D
				NkVector<uint32> mSimIdx;                 // indices séquentiels (triangle-list)

				// ── Viewport offscreen : la sim est rendue dans une render-texture,
				// puis affichée dans le panneau central (comme un vrai éditeur). ────────
				renderer::NkRenderTexture mRT;  // cible offscreen (taille = panneau central)
				renderer::NkTexture mRTTexture; // wrapper pour sampler la RT sur le backbuffer
				int32 mRTW = 0;
				int32 mRTH = 0;

				// ── Config / redémarrage (changement de backend depuis l'UI) ─────────────
				NkString mConfigPath;
				NkString mExePath;
				bool mRestartRequested = false;
		};

	} // namespace nkpa
} // namespace nkentseu
