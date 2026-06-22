// =============================================================================
// Platform/MouPlatformApp.h
// Orchestrateur principal de la plateforme Mú.
// =============================================================================
#pragma once

#ifndef MOU_PLATFORM_APP_H
#define MOU_PLATFORM_APP_H

#include "NKCore/NkTypes.h"
#include "NKWindow/NKWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKUI/NKUI.h"
#include "Games/Common/MouGame.h"
#include "Games/Common/GameMetadata.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKTime/NkClock.h"
#include "NKTime/NkChrono.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include "Assets/MouAssets.h"
#include "Audio/MouAudio.h"

namespace mou {

    class MouPlatformApp {
    public:
        MouPlatformApp() = default;
        ~MouPlatformApp();

        MouPlatformApp(const MouPlatformApp&) = delete;
        MouPlatformApp& operator=(const MouPlatformApp&) = delete;

        bool Initialize(const nkentseu::NkEntryState& state) noexcept;
        int  Run() noexcept;

        void LaunchGame(GameId gameId) noexcept;
        void ReturnToMainMenu() noexcept;

    private:
        void ParseArguments(const nkentseu::NkVector<nkentseu::NkString>& args) noexcept;

        /// Intro de marque : logo optionnel (tex=0 => texte seul) + titre + sous-titre,
        /// fondu + zoom sur fond blanc. @return true quand finie.
        bool RenderBrandIntro(nkentseu::uint32 tex, nkentseu::float32 aspect,
                              const char* title, const char* subtitle,
                              const nkentseu::math::NkColor& textCol,
                              nkentseu::float32 dt) noexcept;
        /// Intro Noge "moteur" : hexagone qui se trace + cœur + POWERED BY/NOGE/Moteur
        /// (inspirée de Pong, palette navy+orange). @return true quand finie.
        bool RenderNogeIntro(nkentseu::float32 dt) noexcept;
        void RenderSplash(nkentseu::float32 dt) noexcept;
        void RenderMainMenu(nkentseu::float32 dt) noexcept;
        void RenderSettings(nkentseu::float32 dt) noexcept;
        void UpdateGameScene(nkentseu::float32 dt) noexcept;
        void RenderGameScene(nkentseu::float32 dt) noexcept;

        void HandleGameEvent(nkentseu::NkEvent* event) noexcept;
        void HandleMainMenuEvent(nkentseu::NkEvent* event) noexcept;

        // === Fenêtre / rendu ===
        nkentseu::NkWindow mWindow;
        nkentseu::renderer::NkRenderWindow* mRenderTarget = nullptr;

        // === UI globale ===
        nkentseu::nkui::NkUIContext*           mUIContext = nullptr;
        nkentseu::nkui::NkUIWindowManager*     mUIWindowManager = nullptr;
        nkentseu::renderer::NkUICanvasBackend* mUIBackend = nullptr;
        nkentseu::nkui::NkUIFont*              mUIFont = nullptr;
        nkentseu::nkui::NkUIFont*              mTitleFont = nullptr;
        nkentseu::uint32                       mBodyFontId = 0;
        nkentseu::uint32                       mTitleFontId = 0;
        nkentseu::nkui::NkUIInputState         mUIInput;
        MouAssets                              mAssets;
        MouAudio                               mAudio;
        nkentseu::uint32                       mVoiceRot = 0;  // alterne les voix d'encouragement

        // === Assets globaux (intros + icônes menu + mascotte) ===
        nkentseu::uint32 mRihenTex = 0, mNogeTex = 0;
        nkentseu::float32 mRihenAspect = 1.f, mNogeAspect = 1.f;
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mIconTex[6] = {0, 0, 0, 0, 0, 0};  // Couleurs, Compter, Calcul, Formes, Animaux, Memoire
        nkentseu::float32 mIntroTime = 0.f;
        nkentseu::float32 mSplashTime = 0.f;

        // === État plateforme ===
        AppScene mCurrentScene = AppScene::IntroRihen;
        nkentseu::memory::NkUniquePtr<MouGame> mCurrentGame;
        GameId mSelectedGameId = GameId::Couleurs;
        bool   mRunning = true;
        bool   mActive  = true;   // false en arrière-plan (Android) : on ne rend pas
        bool   mPaused  = false;  // true quand la fenêtre perd le focus : jeu + audio en pause

        nkentseu::NkGraphicsApi mGraphicsApi = nkentseu::NkGraphicsApi::NK_GFX_API_AUTO;

        // === Input pointeur unifié ===
        struct FrameInput {
            nkentseu::math::NkVec2f pointerPos { 0.f, 0.f };
            bool pressed = false;
            bool pressedThisFrame = false;
            bool releasedThisFrame = false;
            void BeginFrame() { pressedThisFrame = false; releasedThisFrame = false; }
        } mInput;

        // === Timing / resize ===
        nkentseu::NkClock mClock;
        nkentseu::uint32 mLastWindowWidth = 0;
        nkentseu::uint32 mLastWindowHeight = 0;
    };

}  // namespace mou

#endif // MOU_PLATFORM_APP_H
