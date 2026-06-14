// =============================================================================
// Platform/NkoungPlatformApp.h
// Classe principale orchestrant la plateforme Nkoung.
// =============================================================================
#pragma once

#ifndef NKOUNG_PLATFORM_APP_H
#define NKOUNG_PLATFORM_APP_H

#include "NKCore/NkTypes.h"
#include "NKWindow/NkWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKUI/NKUI.h"
#include "Games/Common/NkoungGame.h"
#include "Games/Common/GameMetadata.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKTime/NkClock.h"                       // NkClock (game loop)
#include "NKCanvas/UI/NkUICanvasBackend.h"        // renderer::NkUICanvasBackend

namespace nkoung {

    // =========================================================================
    // NkoungPlatformApp — orchestrateur principal
    // =========================================================================
    class NkoungPlatformApp {
    public:
        NkoungPlatformApp() = default;
        ~NkoungPlatformApp();

        NkoungPlatformApp(const NkoungPlatformApp&) = delete;
        NkoungPlatformApp& operator=(const NkoungPlatformApp&) = delete;

        /// Initialise la plateforme : fenêtre, contexte de rendu, UI, etc.
        /// @param state État d'entrée contenant les arguments de démarrage.
        /// @return true si succès.
        bool Initialize(const nkentseu::NkEntryState& state) noexcept;

        /// Exécute la boucle principale de la plateforme.
        /// Retourne quand l'utilisateur ferme la fenêtre.
        /// @return Code de sortie (0 = succès).
        int Run() noexcept;

        /// Initie le passage à la scène de jeu pour le jeu spécifié.
        void LaunchGame(GameId gameId) noexcept;

        /// Retourne à la scène de sélection de jeu.
        void ReturnToPlatformMenu() noexcept;

    private:
        /// Parse les arguments de démarrage (backend, config, etc.)
        void ParseArguments(const nkentseu::NkVector<nkentseu::NkString>& args) noexcept;

        /// Initialise la scène de menu de plateforme.
        bool InitPlatformMenu() noexcept;

        /// Exécute la logique d'update et render du menu de plateforme.
        void UpdatePlatformMenu(nkentseu::float32 dt) noexcept;
        void RenderPlatformMenu(nkentseu::float32 dt) noexcept;

        /// Exécute la logique d'update et render du jeu actuellement actif.
        void UpdateGameScene(nkentseu::float32 dt) noexcept;
        void RenderGameScene(nkentseu::float32 dt) noexcept;

        /// Gère un événement utilisateur pour le jeu actif.
        void HandleGameEvent(nkentseu::NkEvent* event) noexcept;
        void HandlePlatformMenuEvent(nkentseu::NkEvent* event) noexcept;

        // === État de la fenêtre ===
        nkentseu::NkWindow mWindow;
        nkentseu::renderer::NkRenderWindow* mRenderTarget = nullptr;

        // === État de l'UI globale ===
        nkentseu::nkui::NkUIContext* mUIContext = nullptr;
        nkentseu::nkui::NkUIWindowManager* mUIWindowManager = nullptr;
        nkentseu::renderer::NkUICanvasBackend* mUIBackend = nullptr;
        nkentseu::nkui::NkUIFont* mUIFont = nullptr;
        nkentseu::nkui::NkUIFont* mTitleFont = nullptr;
        nkentseu::uint32 mBodyFontId = 0;
        nkentseu::uint32 mTitleFontId = 0;

        // État d'entrée NKUI (pour le draw list du menu) + survol + FPS lissé.
        nkentseu::nkui::NkUIInputState mUIInput;
        nkentseu::int32 mHoveredGame = -1;
        nkentseu::float32 mFpsSmooth = 60.f;

        // === État de la plateforme ===
        AppScene mCurrentScene = AppScene::PlatformMenu;
        nkentseu::memory::NkUniquePtr<NkoungGame> mCurrentGame;
        GameId mSelectedGameId = GameId::LaserPuzzle;
        bool mRunning = true;

        // === Configuration ===
        nkentseu::NkGraphicsApi mGraphicsApi = nkentseu::NkGraphicsApi::NK_GFX_API_AUTO;

        // === Input ===
        struct FrameInput {
            nkentseu::math::NkVec2f mousePos { 0.f, 0.f };
            bool mouseLPressed = false;
            bool mouseLPressedThisFrame = false;
            bool mouseLReleasedThisFrame = false;

            void BeginFrame() {
                mouseLPressedThisFrame = false;
                mouseLReleasedThisFrame = false;
            }
        } mInput;

        // === Timing ===
        nkentseu::NkClock mClock;
        nkentseu::uint32 mLastWindowWidth = 0;
        nkentseu::uint32 mLastWindowHeight = 0;
    };

}  // namespace nkoung

#endif // NKOUNG_PLATFORM_APP_H
